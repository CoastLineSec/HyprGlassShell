#pragma once

#include <QObject>
#include <QString>
#include <QtTypes>

class QProcess;
class QTimer;

namespace HyprShelld {

class HyprsunsetController final : public QObject {
  Q_OBJECT

public:
  enum class State {
    Disabled,
    Probing,
    ExternalDaemon,
    Starting,
    Applying,
    Ready,
    RetryWaiting,
    Stopping,
    Failed,
  };
  Q_ENUM(State)

  struct Options final {
    QString hyprsunsetExecutable;
    QString hyprctlExecutable;
    QString processProbeExecutable;
    int neutralTemperature = 6500;
    int maximumStartupAttempts = 3;
    int maximumCommandAttempts = 3;
    int retryBaseDelayMs = 150;
    int daemonSettleDelayMs = 300;
    int probeTimeoutMs = 3000;
    int commandTimeoutMs = 5000;
    int stopTimeoutMs = 1000;
    int externalProbeIntervalMs = 5000;
  };

  Q_PROPERTY(bool available READ available CONSTANT)
  Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
  Q_PROPERTY(State state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(int currentTemperature READ currentTemperature NOTIFY
                 currentTemperatureChanged)
  Q_PROPERTY(bool ownsDaemon READ ownsDaemon NOTIFY ownsDaemonChanged)

  explicit HyprsunsetController(Options options, QObject *parent = nullptr);
  ~HyprsunsetController() override;

  [[nodiscard]] bool available() const;
  [[nodiscard]] bool enabled() const;
  [[nodiscard]] State state() const;
  [[nodiscard]] QString error() const;
  [[nodiscard]] int currentTemperature() const;
  [[nodiscard]] int desiredTemperature() const;
  [[nodiscard]] bool ownsDaemon() const;

public slots:
  void setEnabled(bool enabled);
  void setTemperature(int temperature);
  void retry();

signals:
  void enabledChanged();
  void stateChanged();
  void errorChanged();
  void currentTemperatureChanged();
  void ownsDaemonChanged();

private:
  enum class CommandKind {
    None,
    Temperature,
    Identity,
  };

  [[nodiscard]] bool optionsAreValid() const;
  [[nodiscard]] QString daemonProcessName() const;
  void beginProbe();
  void handleProbeFinished(int exitCode, bool normalExit);
  void handleProbeFailure(const QString &message);
  void startOwnedDaemon();
  void handleDaemonStarted();
  void handleDaemonFinished(int exitCode, bool normalExit);
  void scheduleStartupRetry(const QString &message);
  void dispatchDesiredCommand();
  void handleCommandFinished(int exitCode, bool normalExit);
  void handleCommandFailure(const QString &message);
  void scheduleCommandRetry(const QString &message);
  void stopOwnedDaemon();
  void finishStoppedDaemon();

  void setState(State state);
  void setError(const QString &error);
  void setCurrentTemperature(int temperature);
  void setOwnsDaemon(bool ownsDaemon);

  Options options_;
  QProcess *probeProcess_ = nullptr;
  QProcess *daemonProcess_ = nullptr;
  QProcess *commandProcess_ = nullptr;
  QTimer *startupRetryTimer_ = nullptr;
  QTimer *commandRetryTimer_ = nullptr;
  QTimer *daemonSettleTimer_ = nullptr;
  QTimer *probeTimeoutTimer_ = nullptr;
  QTimer *commandTimeoutTimer_ = nullptr;
  QTimer *stopTimeoutTimer_ = nullptr;
  QTimer *externalProbeTimer_ = nullptr;

  State state_ = State::Disabled;
  QString error_;
  int currentTemperature_ = 0;
  int desiredTemperature_ = 6500;
  int startupAttempts_ = 0;
  int commandAttempts_ = 0;
  quint64 desiredGeneration_ = 0;
  quint64 commandGeneration_ = 0;
  int commandTemperature_ = 0;
  CommandKind commandKind_ = CommandKind::None;
  bool enabled_ = false;
  bool ownsDaemon_ = false;
  bool stopRequested_ = false;
  bool shuttingDown_ = false;
  bool probeCompletionHandled_ = false;
  bool probeRestartRequested_ = false;
  bool daemonFailureHandled_ = false;
  bool commandCompletionHandled_ = false;
  bool commandTimedOut_ = false;
};

} // namespace HyprShelld
