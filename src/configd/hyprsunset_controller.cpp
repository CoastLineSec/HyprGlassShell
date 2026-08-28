#include "hyprsunset_controller.h"

#include <QFileInfo>
#include <QProcess>
#include <QTimer>

#include <algorithm>

#include <csignal>
#include <sys/prctl.h>
#include <unistd.h>
#include <utility>

namespace HyprShelld {
namespace {

constexpr int minimumTemperature = 1;
constexpr int maximumRetryDelayMs = 2000;

int retryDelay(const int baseDelayMs, const int completedAttempts) {
  const auto factor = std::max(1, completedAttempts);
  const auto boundedBase = std::min(baseDelayMs, maximumRetryDelayMs);
  if (boundedBase > maximumRetryDelayMs / factor) {
    return maximumRetryDelayMs;
  }
  return std::min(maximumRetryDelayMs, boundedBase * factor);
}

QString processFailure(const QString &operation, QProcess *process,
                       const int exitCode, const bool normalExit) {
  auto detail = QString::fromUtf8(process->readAllStandardError()).trimmed();
  if (detail.size() > 512) {
    detail.truncate(512);
  }
  if (!normalExit) {
    return detail.isEmpty()
               ? QStringLiteral("%1 crashed").arg(operation)
               : QStringLiteral("%1 crashed: %2").arg(operation, detail);
  }
  return detail.isEmpty() ? QStringLiteral("%1 exited with status %2")
                                .arg(operation)
                                .arg(exitCode)
                          : QStringLiteral("%1 exited with status %2: %3")
                                .arg(operation)
                                .arg(exitCode)
                                .arg(detail);
}

} // namespace

HyprsunsetController::HyprsunsetController(Options options, QObject *parent)
    : QObject(parent), options_(std::move(options)),
      probeProcess_(new QProcess(this)), daemonProcess_(new QProcess(this)),
      commandProcess_(new QProcess(this)), startupRetryTimer_(new QTimer(this)),
      commandRetryTimer_(new QTimer(this)),
      daemonSettleTimer_(new QTimer(this)),
      probeTimeoutTimer_(new QTimer(this)),
      commandTimeoutTimer_(new QTimer(this)),
      stopTimeoutTimer_(new QTimer(this)),
      externalProbeTimer_(new QTimer(this)),
      desiredTemperature_(options_.neutralTemperature) {
  daemonProcess_->setChildProcessModifier([] {
    // hyprsunset is an owned companion, not an independent user service.
    // Ensure a configd crash cannot strand a warm-filter process that the
    // restarted service must then conservatively treat as external.
    if (::prctl(PR_SET_PDEATHSIG, SIGTERM) == 0 && ::getppid() == 1) {
      ::raise(SIGTERM);
    }
  });

  for (auto *timer : {
           startupRetryTimer_,
           commandRetryTimer_,
           daemonSettleTimer_,
           probeTimeoutTimer_,
           commandTimeoutTimer_,
           stopTimeoutTimer_,
           externalProbeTimer_,
       }) {
    timer->setSingleShot(true);
  }

  connect(probeProcess_, &QProcess::finished, this,
          [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
            handleProbeFinished(exitCode, exitStatus == QProcess::NormalExit);
          });
  connect(
      probeProcess_, &QProcess::errorOccurred, this,
      [this](const QProcess::ProcessError processError) {
        if (processError == QProcess::FailedToStart) {
          handleProbeFailure(
              QStringLiteral("Cannot start the hyprsunset process probe: %1")
                  .arg(probeProcess_->errorString()));
        }
      });
  connect(probeTimeoutTimer_, &QTimer::timeout, this, [this] {
    if (probeProcess_->state() == QProcess::NotRunning) {
      return;
    }
    probeCompletionHandled_ = true;
    probeRestartRequested_ = false;
    probeProcess_->kill();
    handleProbeFailure(
        QStringLiteral("The hyprsunset process probe timed out"));
  });

  connect(daemonProcess_, &QProcess::started, this,
          &HyprsunsetController::handleDaemonStarted);
  connect(daemonProcess_, &QProcess::finished, this,
          [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
            handleDaemonFinished(exitCode, exitStatus == QProcess::NormalExit);
          });
  connect(daemonProcess_, &QProcess::errorOccurred, this,
          [this](const QProcess::ProcessError processError) {
            if (processError != QProcess::FailedToStart ||
                daemonFailureHandled_) {
              return;
            }
            daemonFailureHandled_ = true;
            setOwnsDaemon(false);
            scheduleStartupRetry(QStringLiteral("Cannot start hyprsunset: %1")
                                     .arg(daemonProcess_->errorString()));
          });

  connect(commandProcess_, &QProcess::finished, this,
          [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
            handleCommandFinished(exitCode, exitStatus == QProcess::NormalExit);
          });
  connect(commandProcess_, &QProcess::errorOccurred, this,
          [this](const QProcess::ProcessError processError) {
            if (processError == QProcess::FailedToStart) {
              handleCommandFailure(QStringLiteral("Cannot start hyprctl: %1")
                                       .arg(commandProcess_->errorString()));
            }
          });
  connect(commandTimeoutTimer_, &QTimer::timeout, this, [this] {
    if (commandProcess_->state() == QProcess::NotRunning) {
      return;
    }
    commandTimedOut_ = true;
    commandProcess_->kill();
  });

  connect(startupRetryTimer_, &QTimer::timeout, this, [this] { beginProbe(); });
  connect(commandRetryTimer_, &QTimer::timeout, this,
          [this] { dispatchDesiredCommand(); });
  connect(daemonSettleTimer_, &QTimer::timeout, this,
          [this] { dispatchDesiredCommand(); });
  connect(stopTimeoutTimer_, &QTimer::timeout, this, [this] {
    if (daemonProcess_->state() != QProcess::NotRunning) {
      daemonProcess_->kill();
    }
  });
  connect(externalProbeTimer_, &QTimer::timeout, this,
          &HyprsunsetController::beginProbe);

  if (!optionsAreValid()) {
    setError(QStringLiteral("Invalid hyprsunset controller options"));
    setState(State::Failed);
  }
}

HyprsunsetController::~HyprsunsetController() {
  shuttingDown_ = true;
  for (auto *timer : {
           startupRetryTimer_,
           commandRetryTimer_,
           daemonSettleTimer_,
           probeTimeoutTimer_,
           commandTimeoutTimer_,
           stopTimeoutTimer_,
           externalProbeTimer_,
       }) {
    timer->stop();
  }
  if (probeProcess_->state() != QProcess::NotRunning) {
    probeProcess_->kill();
  }
  if (commandProcess_->state() != QProcess::NotRunning) {
    commandProcess_->kill();
  }
  if (daemonProcess_->state() != QProcess::NotRunning) {
    daemonProcess_->kill();
  }
}

bool HyprsunsetController::available() const { return optionsAreValid(); }

bool HyprsunsetController::enabled() const { return enabled_; }

HyprsunsetController::State HyprsunsetController::state() const {
  return state_;
}

QString HyprsunsetController::error() const { return error_; }

int HyprsunsetController::currentTemperature() const {
  return currentTemperature_;
}

int HyprsunsetController::desiredTemperature() const {
  return desiredTemperature_;
}

bool HyprsunsetController::ownsDaemon() const { return ownsDaemon_; }

void HyprsunsetController::setEnabled(const bool enabled) {
  if (enabled == enabled_) {
    return;
  }

  enabled_ = enabled;
  emit enabledChanged();
  ++desiredGeneration_;
  commandAttempts_ = 0;
  startupRetryTimer_->stop();
  commandRetryTimer_->stop();
  daemonSettleTimer_->stop();
  setError(QString());

  if (enabled_) {
    if (!optionsAreValid()) {
      setError(QStringLiteral("Invalid hyprsunset controller options"));
      setState(State::Failed);
      return;
    }
    startupAttempts_ = 0;
    if (ownsDaemon_ && daemonProcess_->state() != QProcess::NotRunning) {
      if (!stopRequested_) {
        dispatchDesiredCommand();
      }
      return;
    }
    beginProbe();
    return;
  }

  probeTimeoutTimer_->stop();
  auto stoppingProbe = false;
  if (probeProcess_->state() != QProcess::NotRunning) {
    probeCompletionHandled_ = true;
    probeRestartRequested_ = false;
    probeProcess_->kill();
    stoppingProbe = true;
  }

  if (!ownsDaemon_) {
    setCurrentTemperature(0);
    setState(stoppingProbe ? State::Stopping : State::Disabled);
    return;
  }

  if (daemonProcess_->state() == QProcess::Running) {
    setState(State::Stopping);
    dispatchDesiredCommand();
    return;
  }
  if (daemonProcess_->state() == QProcess::Starting) {
    // Let QProcess finish the asynchronous exec. handleDaemonStarted() will
    // send identity before stopping the child, while FailedToStart follows
    // the normal disabled cleanup path.
    setState(State::Stopping);
    return;
  }

  stopOwnedDaemon();
}

void HyprsunsetController::setTemperature(const int temperature) {
  if (temperature < minimumTemperature) {
    setError(QStringLiteral("Temperature must be a positive integer"));
    return;
  }

  const auto changed = temperature != desiredTemperature_;
  if (changed) {
    desiredTemperature_ = temperature;
    ++desiredGeneration_;
    commandAttempts_ = 0;
    commandRetryTimer_->stop();
  }
  if (changed) {
    setError(QString());
  }

  if (enabled_ && ownsDaemon_ && daemonProcess_->state() == QProcess::Running &&
      !stopRequested_ &&
      (changed || currentTemperature_ != desiredTemperature_ ||
       state_ != State::Ready) &&
      state_ != State::Failed && !commandRetryTimer_->isActive()) {
    dispatchDesiredCommand();
  }
}

void HyprsunsetController::retry() {
  if (!enabled_ || shuttingDown_) {
    return;
  }
  setError(QString());
  startupRetryTimer_->stop();
  commandRetryTimer_->stop();
  if (ownsDaemon_ && daemonProcess_->state() == QProcess::Running &&
      !stopRequested_) {
    commandAttempts_ = 0;
    dispatchDesiredCommand();
    return;
  }
  startupAttempts_ = 0;
  beginProbe();
}

bool HyprsunsetController::optionsAreValid() const {
  const auto isExecutable = [](const QString &path) {
    const QFileInfo info(path);
    return info.isFile() && info.isExecutable();
  };
  return isExecutable(options_.hyprsunsetExecutable) &&
         isExecutable(options_.hyprctlExecutable) &&
         isExecutable(options_.processProbeExecutable) &&
         options_.neutralTemperature >= minimumTemperature &&
         options_.maximumStartupAttempts >= 1 &&
         options_.maximumCommandAttempts >= 1 &&
         options_.retryBaseDelayMs >= 0 && options_.daemonSettleDelayMs >= 0 &&
         options_.probeTimeoutMs >= 1 && options_.commandTimeoutMs >= 1 &&
         options_.stopTimeoutMs >= 1 && options_.externalProbeIntervalMs >= 1;
}

QString HyprsunsetController::daemonProcessName() const {
  return QFileInfo(options_.hyprsunsetExecutable).fileName();
}

void HyprsunsetController::beginProbe() {
  if (!enabled_ || shuttingDown_ || ownsDaemon_) {
    return;
  }
  if (probeProcess_->state() != QProcess::NotRunning) {
    if (probeCompletionHandled_) {
      probeRestartRequested_ = true;
    }
    return;
  }

  probeCompletionHandled_ = false;
  probeRestartRequested_ = false;
  if (state_ != State::ExternalDaemon) {
    setState(State::Probing);
  }
  probeProcess_->setProgram(options_.processProbeExecutable);
  probeProcess_->setArguments({
      QStringLiteral("-u"),
      QString::number(static_cast<qulonglong>(::geteuid())),
      QStringLiteral("-x"),
      daemonProcessName(),
  });
  probeProcess_->start();
  probeTimeoutTimer_->start(options_.probeTimeoutMs);
}

void HyprsunsetController::handleProbeFinished(const int exitCode,
                                               const bool normalExit) {
  if (probeCompletionHandled_) {
    const auto restartRequested = std::exchange(probeRestartRequested_, false);
    if (!enabled_ && state_ == State::Stopping) {
      setState(State::Disabled);
    } else if (restartRequested && enabled_ && !shuttingDown_ && !ownsDaemon_) {
      QTimer::singleShot(0, this, &HyprsunsetController::beginProbe);
    }
    return;
  }
  probeCompletionHandled_ = true;
  probeTimeoutTimer_->stop();
  if (!enabled_ || shuttingDown_) {
    return;
  }
  if (!normalExit) {
    handleProbeFailure(QStringLiteral("The hyprsunset process probe crashed"));
    return;
  }
  if (exitCode == 0) {
    startupAttempts_ = 0;
    setOwnsDaemon(false);
    setCurrentTemperature(0);
    setError(QString());
    setState(State::ExternalDaemon);
    return;
  }
  if (exitCode == 1) {
    startOwnedDaemon();
    return;
  }
  handleProbeFailure(
      processFailure(QStringLiteral("The hyprsunset process probe"),
                     probeProcess_, exitCode, true));
}

void HyprsunsetController::handleProbeFailure(const QString &message) {
  probeCompletionHandled_ = true;
  probeTimeoutTimer_->stop();
  if (!enabled_ || shuttingDown_) {
    return;
  }
  setError(message);
  setState(State::Failed);
}

void HyprsunsetController::startOwnedDaemon() {
  if (!enabled_ || shuttingDown_) {
    return;
  }
  if (startupAttempts_ >= options_.maximumStartupAttempts) {
    setState(State::Failed);
    return;
  }

  ++startupAttempts_;
  stopRequested_ = false;
  daemonFailureHandled_ = false;
  setOwnsDaemon(true);
  setState(State::Starting);
  daemonProcess_->setProgram(options_.hyprsunsetExecutable);
  daemonProcess_->setArguments({
      QStringLiteral("--temperature"),
      QString::number(options_.neutralTemperature),
  });
  daemonProcess_->start();
}

void HyprsunsetController::handleDaemonStarted() {
  if (shuttingDown_) {
    daemonProcess_->kill();
    return;
  }
  if (!enabled_) {
    setState(State::Stopping);
    dispatchDesiredCommand();
    return;
  }
  daemonSettleTimer_->start(options_.daemonSettleDelayMs);
}

void HyprsunsetController::handleDaemonFinished(const int exitCode,
                                                const bool normalExit) {
  if (daemonFailureHandled_) {
    return;
  }
  daemonSettleTimer_->stop();
  stopTimeoutTimer_->stop();
  const auto expectedStop = stopRequested_ || !enabled_ || shuttingDown_;
  stopRequested_ = false;
  daemonFailureHandled_ = true;
  setOwnsDaemon(false);
  setCurrentTemperature(0);

  if (commandProcess_->state() != QProcess::NotRunning) {
    ++desiredGeneration_;
    commandAttempts_ = 0;
    commandProcess_->kill();
  }

  if (shuttingDown_) {
    return;
  }
  if (expectedStop) {
    if (enabled_) {
      QTimer::singleShot(0, this, &HyprsunsetController::beginProbe);
    } else {
      setState(State::Disabled);
    }
    return;
  }

  scheduleStartupRetry(processFailure(QStringLiteral("hyprsunset"),
                                      daemonProcess_, exitCode, normalExit));
}

void HyprsunsetController::scheduleStartupRetry(const QString &message) {
  daemonSettleTimer_->stop();
  setOwnsDaemon(false);
  setCurrentTemperature(0);
  if (!enabled_ || shuttingDown_) {
    setState(State::Disabled);
    return;
  }
  setError(message);
  if (startupAttempts_ >= options_.maximumStartupAttempts) {
    setState(State::Failed);
    return;
  }
  setState(State::RetryWaiting);
  startupRetryTimer_->start(
      retryDelay(options_.retryBaseDelayMs, startupAttempts_));
}

void HyprsunsetController::dispatchDesiredCommand() {
  if (shuttingDown_ || !ownsDaemon_ || stopRequested_ ||
      daemonProcess_->state() != QProcess::Running ||
      commandProcess_->state() != QProcess::NotRunning) {
    return;
  }

  commandKind_ = enabled_ ? CommandKind::Temperature : CommandKind::Identity;
  commandGeneration_ = desiredGeneration_;
  commandTemperature_ = enabled_ ? desiredTemperature_ : 0;
  commandCompletionHandled_ = false;
  commandTimedOut_ = false;
  ++commandAttempts_;

  QStringList arguments{QStringLiteral("hyprsunset")};
  if (commandKind_ == CommandKind::Temperature) {
    arguments.append(QStringLiteral("temperature"));
    arguments.append(QString::number(commandTemperature_));
    setState(State::Applying);
  } else {
    arguments.append(QStringLiteral("identity"));
    setState(State::Stopping);
  }

  commandProcess_->setProgram(options_.hyprctlExecutable);
  commandProcess_->setArguments(arguments);
  commandProcess_->start();
  commandTimeoutTimer_->start(options_.commandTimeoutMs);
}

void HyprsunsetController::handleCommandFinished(const int exitCode,
                                                 const bool normalExit) {
  if (commandCompletionHandled_) {
    return;
  }
  commandCompletionHandled_ = true;
  commandTimeoutTimer_->stop();

  const auto completedKind = commandKind_;
  const auto completedGeneration = commandGeneration_;
  const auto completedTemperature = commandTemperature_;
  commandKind_ = CommandKind::None;

  if (shuttingDown_) {
    return;
  }
  if (completedGeneration != desiredGeneration_) {
    QTimer::singleShot(0, this, &HyprsunsetController::dispatchDesiredCommand);
    return;
  }

  if (normalExit && exitCode == 0 && !commandTimedOut_) {
    commandAttempts_ = 0;
    startupAttempts_ = 0;
    setError(QString());
    if (completedKind == CommandKind::Temperature && enabled_) {
      setCurrentTemperature(completedTemperature);
      setState(State::Ready);
    } else if (completedKind == CommandKind::Identity && !enabled_) {
      setCurrentTemperature(0);
      stopOwnedDaemon();
    }
    return;
  }

  const auto message =
      commandTimedOut_ ? QStringLiteral("hyprctl timed out")
                       : processFailure(QStringLiteral("hyprctl"),
                                        commandProcess_, exitCode, normalExit);
  scheduleCommandRetry(message);
}

void HyprsunsetController::handleCommandFailure(const QString &message) {
  if (commandCompletionHandled_) {
    return;
  }
  commandCompletionHandled_ = true;
  commandTimeoutTimer_->stop();
  commandKind_ = CommandKind::None;
  if (shuttingDown_) {
    return;
  }
  if (commandGeneration_ != desiredGeneration_) {
    QTimer::singleShot(0, this, &HyprsunsetController::dispatchDesiredCommand);
    return;
  }
  scheduleCommandRetry(message);
}

void HyprsunsetController::scheduleCommandRetry(const QString &message) {
  if (!ownsDaemon_ || daemonProcess_->state() != QProcess::Running) {
    return;
  }
  if (commandAttempts_ < options_.maximumCommandAttempts) {
    commandRetryTimer_->start(
        retryDelay(options_.retryBaseDelayMs, commandAttempts_));
    setState(enabled_ ? State::Applying : State::Stopping);
    setError(message);
    return;
  }
  if (!enabled_) {
    stopOwnedDaemon();
  } else {
    setState(State::Failed);
  }
  setError(message);
}

void HyprsunsetController::stopOwnedDaemon() {
  daemonSettleTimer_->stop();
  commandRetryTimer_->stop();
  if (daemonProcess_->state() == QProcess::NotRunning) {
    finishStoppedDaemon();
    return;
  }
  stopRequested_ = true;
  setState(State::Stopping);
  daemonProcess_->terminate();
  stopTimeoutTimer_->start(options_.stopTimeoutMs);
}

void HyprsunsetController::finishStoppedDaemon() {
  stopRequested_ = false;
  setOwnsDaemon(false);
  setCurrentTemperature(0);
  if (enabled_) {
    beginProbe();
  } else {
    setState(State::Disabled);
  }
}

void HyprsunsetController::setState(const State state) {
  if (state == state_) {
    if (state == State::ExternalDaemon && enabled_ &&
        !externalProbeTimer_->isActive()) {
      externalProbeTimer_->start(options_.externalProbeIntervalMs);
    }
    return;
  }
  externalProbeTimer_->stop();
  state_ = state;
  emit stateChanged();
  if (state_ == State::ExternalDaemon && enabled_) {
    externalProbeTimer_->start(options_.externalProbeIntervalMs);
  }
}

void HyprsunsetController::setError(const QString &error) {
  if (error == error_) {
    return;
  }
  error_ = error;
  emit errorChanged();
}

void HyprsunsetController::setCurrentTemperature(const int temperature) {
  if (temperature == currentTemperature_) {
    return;
  }
  currentTemperature_ = temperature;
  emit currentTemperatureChanged();
}

void HyprsunsetController::setOwnsDaemon(const bool ownsDaemon) {
  if (ownsDaemon == ownsDaemon_) {
    return;
  }
  ownsDaemon_ = ownsDaemon;
  emit ownsDaemonChanged();
}

} // namespace HyprShelld
