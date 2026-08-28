#include "configd/hyprsunset_controller.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <unistd.h>

namespace {

bool writeScript(const QString &path, const QByteArray &contents) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      file.write(contents) != contents.size()) {
    return false;
  }
  file.close();
  return QFile::setPermissions(path, QFileDevice::ReadOwner |
                                         QFileDevice::WriteOwner |
                                         QFileDevice::ExeOwner);
}

QStringList readLines(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  const auto contents = QString::fromUtf8(file.readAll()).trimmed();
  return contents.isEmpty() ? QStringList()
                            : contents.split(u'\n', Qt::SkipEmptyParts);
}

} // namespace

class HyprsunsetControllerTest final : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();
  void ownsAppliesAndRestoresIdentity();
  void disablingDuringStartupStillRestoresIdentity();
  void respectsAnExternalDaemon();
  void takesOwnershipAfterExternalDaemonExits();
  void reenableDuringCancelledProbeRestartsProbe();
  void timedOutProbeStaysFailed();
  void retriesStartupOnlyToTheConfiguredBound();
  void terminalCommandFailureKeepsItsDiagnostic();
  void coalescesQueuedTemperatureRequests();
  void reportsUnavailableRuntimeTools();

private:
  [[nodiscard]] HyprShelld::HyprsunsetController::Options options() const;

  std::unique_ptr<QTemporaryDir> directory_;
  QString probePath_;
  QString probeLogPath_;
  QString daemonPath_;
  QString hyprctlPath_;
  QString daemonLogPath_;
  QString commandLogPath_;
};

void HyprsunsetControllerTest::init() {
  directory_ = std::make_unique<QTemporaryDir>();
  QVERIFY(directory_->isValid());

  probePath_ = directory_->filePath(QStringLiteral("fake-pgrep"));
  probeLogPath_ = directory_->filePath(QStringLiteral("probes.log"));
  daemonPath_ = directory_->filePath(QStringLiteral("hyprsunset"));
  hyprctlPath_ = directory_->filePath(QStringLiteral("hyprctl"));
  daemonLogPath_ = directory_->filePath(QStringLiteral("daemon.log"));
  commandLogPath_ = directory_->filePath(QStringLiteral("commands.log"));

  QVERIFY(writeScript(probePath_, R"(#!/bin/sh
printf '%s\n' "$*" >> "$HYPRSUNSET_TEST_PROBE_LOG"
delay="${HYPRSUNSET_TEST_PROBE_DELAY:-0}"
if [ "$delay" != "0" ]; then
    sleep "$delay"
fi
exit "${HYPRSUNSET_TEST_PROBE_EXIT:-1}"
)"));
  QVERIFY(writeScript(daemonPath_, R"(#!/bin/sh
printf '%s\n' "$*" >> "$HYPRSUNSET_TEST_DAEMON_LOG"
if [ "${HYPRSUNSET_TEST_DAEMON_MODE:-run}" = "fail" ]; then
    exit 17
fi
trap 'exit 0' TERM INT
while :; do
    sleep 1
done
)"));
  QVERIFY(writeScript(hyprctlPath_, R"(#!/bin/sh
printf '%s\n' "$*" >> "$HYPRSUNSET_TEST_COMMAND_LOG"
delay="${HYPRSUNSET_TEST_CTL_DELAY:-0}"
if [ "$delay" != "0" ]; then
    sleep "$delay"
fi
exit "${HYPRSUNSET_TEST_CTL_EXIT:-0}"
)"));

  qputenv("HYPRSUNSET_TEST_PROBE_LOG", probeLogPath_.toUtf8());
  qputenv("HYPRSUNSET_TEST_DAEMON_LOG", daemonLogPath_.toUtf8());
  qputenv("HYPRSUNSET_TEST_COMMAND_LOG", commandLogPath_.toUtf8());
  qputenv("HYPRSUNSET_TEST_PROBE_EXIT", "1");
  qputenv("HYPRSUNSET_TEST_PROBE_DELAY", "0");
  qputenv("HYPRSUNSET_TEST_DAEMON_MODE", "run");
  qputenv("HYPRSUNSET_TEST_CTL_DELAY", "0");
  qputenv("HYPRSUNSET_TEST_CTL_EXIT", "0");
}

void HyprsunsetControllerTest::cleanup() {
  for (const auto *name : {
           "HYPRSUNSET_TEST_PROBE_LOG",
           "HYPRSUNSET_TEST_DAEMON_LOG",
           "HYPRSUNSET_TEST_COMMAND_LOG",
           "HYPRSUNSET_TEST_PROBE_EXIT",
           "HYPRSUNSET_TEST_PROBE_DELAY",
           "HYPRSUNSET_TEST_DAEMON_MODE",
           "HYPRSUNSET_TEST_CTL_DELAY",
           "HYPRSUNSET_TEST_CTL_EXIT",
       }) {
    qunsetenv(name);
  }
  directory_.reset();
}

HyprShelld::HyprsunsetController::Options
HyprsunsetControllerTest::options() const {
  return {
      .hyprsunsetExecutable = daemonPath_,
      .hyprctlExecutable = hyprctlPath_,
      .processProbeExecutable = probePath_,
      .neutralTemperature = 6500,
      .maximumStartupAttempts = 3,
      .maximumCommandAttempts = 3,
      .retryBaseDelayMs = 20,
      .daemonSettleDelayMs = 20,
      .probeTimeoutMs = 500,
      .commandTimeoutMs = 1000,
      .stopTimeoutMs = 500,
      .externalProbeIntervalMs = 40,
  };
}

void HyprsunsetControllerTest::ownsAppliesAndRestoresIdentity() {
  HyprShelld::HyprsunsetController controller(options());
  QSignalSpy stateSpy(&controller,
                      &HyprShelld::HyprsunsetController::stateChanged);
  QSignalSpy temperatureSpy(
      &controller,
      &HyprShelld::HyprsunsetController::currentTemperatureChanged);

  controller.setTemperature(4321);
  controller.setEnabled(true);

  QTRY_COMPARE_WITH_TIMEOUT(
      controller.state(), HyprShelld::HyprsunsetController::State::Ready, 3000);
  QCOMPARE(controller.currentTemperature(), 4321);
  QVERIFY(controller.ownsDaemon());
  QVERIFY(controller.error().isEmpty());
  QCOMPARE(readLines(daemonLogPath_),
           QStringList{QStringLiteral("--temperature 6500")});
  QCOMPARE(readLines(commandLogPath_),
           QStringList{QStringLiteral("hyprsunset temperature 4321")});

  controller.setEnabled(false);
  QTRY_COMPARE_WITH_TIMEOUT(controller.state(),
                            HyprShelld::HyprsunsetController::State::Disabled,
                            3000);
  QCOMPARE(controller.currentTemperature(), 0);
  QVERIFY(!controller.ownsDaemon());
  const auto commands = readLines(commandLogPath_);
  QCOMPARE(commands.size(), 2);
  QCOMPARE(commands.last(), QStringLiteral("hyprsunset identity"));
  QVERIFY(stateSpy.count() > 0);
  QVERIFY(temperatureSpy.count() >= 2);
}

void HyprsunsetControllerTest::respectsAnExternalDaemon() {
  qputenv("HYPRSUNSET_TEST_PROBE_EXIT", "0");
  HyprShelld::HyprsunsetController controller(options());
  controller.setTemperature(3901);
  controller.setEnabled(true);

  QTRY_COMPARE_WITH_TIMEOUT(
      controller.state(),
      HyprShelld::HyprsunsetController::State::ExternalDaemon, 2000);
  QVERIFY(!controller.ownsDaemon());
  QCOMPARE(controller.currentTemperature(), 0);
  QVERIFY(readLines(daemonLogPath_).isEmpty());
  QVERIFY(readLines(commandLogPath_).isEmpty());

  controller.setEnabled(false);
  QCOMPARE(controller.state(),
           HyprShelld::HyprsunsetController::State::Disabled);
  QVERIFY(readLines(commandLogPath_).isEmpty());
}

void HyprsunsetControllerTest::takesOwnershipAfterExternalDaemonExits() {
  qputenv("HYPRSUNSET_TEST_PROBE_EXIT", "0");
  HyprShelld::HyprsunsetController controller(options());
  controller.setTemperature(3901);
  controller.setEnabled(true);

  QTRY_COMPARE_WITH_TIMEOUT(
      controller.state(),
      HyprShelld::HyprsunsetController::State::ExternalDaemon, 2000);
  qputenv("HYPRSUNSET_TEST_PROBE_EXIT", "1");

  QTRY_COMPARE_WITH_TIMEOUT(
      controller.state(), HyprShelld::HyprsunsetController::State::Ready, 3000);
  QVERIFY(controller.ownsDaemon());
  QCOMPARE(controller.currentTemperature(), 3901);

  controller.setEnabled(false);
  QTRY_COMPARE_WITH_TIMEOUT(controller.state(),
                            HyprShelld::HyprsunsetController::State::Disabled,
                            3000);
}

void HyprsunsetControllerTest::reenableDuringCancelledProbeRestartsProbe() {
  qputenv("HYPRSUNSET_TEST_PROBE_DELAY", "0.25");
  HyprShelld::HyprsunsetController controller(options());
  controller.setTemperature(4200);
  controller.setEnabled(true);
  QTRY_COMPARE_WITH_TIMEOUT(controller.state(),
                            HyprShelld::HyprsunsetController::State::Probing,
                            1000);

  controller.setEnabled(false);
  controller.setEnabled(true);

  QTRY_COMPARE_WITH_TIMEOUT(
      controller.state(), HyprShelld::HyprsunsetController::State::Ready, 3000);
  QCOMPARE(controller.currentTemperature(), 4200);
  QVERIFY(controller.ownsDaemon());

  controller.setEnabled(false);
  QTRY_COMPARE_WITH_TIMEOUT(controller.state(),
                            HyprShelld::HyprsunsetController::State::Disabled,
                            3000);
}

void HyprsunsetControllerTest::timedOutProbeStaysFailed() {
  qputenv("HYPRSUNSET_TEST_PROBE_DELAY", "0.25");
  auto controllerOptions = options();
  controllerOptions.probeTimeoutMs = 30;
  HyprShelld::HyprsunsetController controller(controllerOptions);

  controller.setEnabled(true);
  QTRY_COMPARE_WITH_TIMEOUT(controller.state(),
                            HyprShelld::HyprsunsetController::State::Failed,
                            1000);
  QCOMPARE(controller.error(),
           QStringLiteral("The hyprsunset process probe timed out"));
  const auto expectedProbe = QStringLiteral("-u %1 -x hyprsunset")
                                 .arg(static_cast<qulonglong>(::geteuid()));
  QCOMPARE(readLines(probeLogPath_), QStringList{expectedProbe});

  QTest::qWait(350);
  QCOMPARE(controller.state(), HyprShelld::HyprsunsetController::State::Failed);
  QCOMPARE(controller.error(),
           QStringLiteral("The hyprsunset process probe timed out"));
  QCOMPARE(readLines(probeLogPath_), QStringList{expectedProbe});
}

void HyprsunsetControllerTest::disablingDuringStartupStillRestoresIdentity() {
  auto controllerOptions = options();
  controllerOptions.daemonSettleDelayMs = 500;
  HyprShelld::HyprsunsetController controller(controllerOptions);
  controller.setEnabled(true);
  QTRY_COMPARE_WITH_TIMEOUT(readLines(daemonLogPath_).size(), 1, 2000);
  controller.setEnabled(false);

  QTRY_COMPARE_WITH_TIMEOUT(controller.state(),
                            HyprShelld::HyprsunsetController::State::Disabled,
                            3000);
  QVERIFY(!controller.ownsDaemon());
  const auto commands = readLines(commandLogPath_);
  QCOMPARE(commands, QStringList{QStringLiteral("hyprsunset identity")});
}

void HyprsunsetControllerTest::retriesStartupOnlyToTheConfiguredBound() {
  qputenv("HYPRSUNSET_TEST_DAEMON_MODE", "fail");
  auto controllerOptions = options();
  controllerOptions.maximumStartupAttempts = 3;
  controllerOptions.retryBaseDelayMs = 10;
  controllerOptions.daemonSettleDelayMs = 5;
  HyprShelld::HyprsunsetController controller(controllerOptions);

  controller.setEnabled(true);
  QTRY_COMPARE_WITH_TIMEOUT(controller.state(),
                            HyprShelld::HyprsunsetController::State::Failed,
                            3000);
  QCOMPARE(readLines(daemonLogPath_).size(), 3);
  QVERIFY(!controller.ownsDaemon());
  QCOMPARE(controller.currentTemperature(), 0);
  QVERIFY(!controller.error().isEmpty());

  QTest::qWait(100);
  QCOMPARE(readLines(daemonLogPath_).size(), 3);
  controller.setEnabled(false);
  QCOMPARE(controller.state(),
           HyprShelld::HyprsunsetController::State::Disabled);
}

void HyprsunsetControllerTest::terminalCommandFailureKeepsItsDiagnostic() {
  qputenv("HYPRSUNSET_TEST_CTL_EXIT", "17");
  auto controllerOptions = options();
  controllerOptions.maximumCommandAttempts = 2;
  HyprShelld::HyprsunsetController controller(controllerOptions);
  controller.setTemperature(4000);
  controller.setEnabled(true);

  QTRY_COMPARE_WITH_TIMEOUT(controller.state(),
                            HyprShelld::HyprsunsetController::State::Failed,
                            3000);
  const auto error = controller.error();
  QVERIFY(!error.isEmpty());
  QCOMPARE(readLines(commandLogPath_).size(), 2);

  controller.setTemperature(4000);
  QCOMPARE(controller.error(), error);
  QTest::qWait(100);
  QCOMPARE(readLines(commandLogPath_).size(), 2);

  controller.setEnabled(false);
  QTRY_COMPARE_WITH_TIMEOUT(controller.state(),
                            HyprShelld::HyprsunsetController::State::Disabled,
                            3000);
}

void HyprsunsetControllerTest::coalescesQueuedTemperatureRequests() {
  qputenv("HYPRSUNSET_TEST_CTL_DELAY", "0.25");
  HyprShelld::HyprsunsetController controller(options());
  controller.setTemperature(4000);
  controller.setEnabled(true);

  QTRY_COMPARE_WITH_TIMEOUT(readLines(commandLogPath_).size(), 1, 2000);
  controller.setTemperature(4100);
  controller.setTemperature(4200);
  controller.setTemperature(4300);

  QTRY_COMPARE_WITH_TIMEOUT(
      controller.state(), HyprShelld::HyprsunsetController::State::Ready, 3000);
  QCOMPARE(controller.currentTemperature(), 4300);
  const auto commands = readLines(commandLogPath_);
  QCOMPARE(commands.size(), 2);
  QCOMPARE(commands.at(0), QStringLiteral("hyprsunset temperature 4000"));
  QCOMPARE(commands.at(1), QStringLiteral("hyprsunset temperature 4300"));

  controller.setEnabled(false);
  QTRY_COMPARE_WITH_TIMEOUT(controller.state(),
                            HyprShelld::HyprsunsetController::State::Disabled,
                            3000);
}

void HyprsunsetControllerTest::reportsUnavailableRuntimeTools() {
  auto controllerOptions = options();
  controllerOptions.hyprsunsetExecutable =
      directory_->filePath(QStringLiteral("missing-hyprsunset"));
  HyprShelld::HyprsunsetController controller(controllerOptions);

  QVERIFY(!controller.available());
  QCOMPARE(controller.state(), HyprShelld::HyprsunsetController::State::Failed);
  QVERIFY(!controller.error().isEmpty());
}

QTEST_GUILESS_MAIN(HyprsunsetControllerTest)

#include "hyprsunset_controller_test.moc"
