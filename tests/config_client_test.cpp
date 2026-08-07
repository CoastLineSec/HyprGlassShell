#include "config_client.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>

namespace {

const QString busName = QStringLiteral("org.hyprshelld.Config1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Config1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Config1");

} // namespace

class ConfigClientTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        executable_ = qEnvironmentVariable("HYPRSHELLD_CONFIGD_EXECUTABLE");
        QVERIFY(!executable_.isEmpty());
        QVERIFY2(bus_.isConnected(), qPrintable(bus_.lastError().message()));
    }

    void cleanup()
    {
        stopService();
    }

    void tracksConfigurationAcrossServiceLifetime()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        HyprShelld::ConfigClient client(bus_, nullptr);
        QCOMPARE(client.available(), false);
        QCOMPARE(client.busy(), false);
        QCOMPARE(client.barHeight(), 48U);
        QCOMPARE(client.minimumBarHeight(), 32U);
        QCOMPARE(client.maximumBarHeight(), 96U);
        QCOMPARE(client.defaultBarHeight(), 48U);

        QVERIFY2(startService(directory.path()), qPrintable(processError_));
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.barHeight(), 48U);
        QCOMPARE(client.revision(), 0ULL);
        QCOMPARE(client.recoveryState(), QStringLiteral("normal"));

        client.setBarHeight(60);
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.barHeight(), 60U, 3000);
        QCOMPARE(client.revision(), 1ULL);
        QVERIFY(client.lastErrorName().isEmpty());

        client.setBarHeight(31);
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Config1.Error.InvalidBarHeight")
        );
        QVERIFY(!client.lastErrorMessage().isEmpty());
        QCOMPARE(client.barHeight(), 60U);
        QCOMPARE(client.revision(), 1ULL);

        client.clearError();
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorMessage().isEmpty());

        client.resetBarHeight();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.barHeight(), 48U, 3000);
        QCOMPARE(client.revision(), 2ULL);

        stopService();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.barHeight(), 48U);
        QCOMPARE(client.revision(), 2ULL);

        QVERIFY2(startService(directory.path()), qPrintable(processError_));
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.barHeight(), 48U);
        QCOMPARE(client.revision(), 2ULL);

        QDBusInterface external(busName, objectPath, interfaceName, bus_);
        QDBusPendingReply<qulonglong> changed = external.asyncCall(
            QStringLiteral("SetBarHeight"),
            72U
        );
        changed.waitForFinished();
        QVERIFY2(!changed.isError(), qPrintable(changed.error().message()));
        QCOMPARE(changed.value(), 3ULL);
        QTRY_COMPARE_WITH_TIMEOUT(client.barHeight(), 72U, 3000);
        QCOMPARE(client.revision(), 3ULL);
    }

private:
    bool startService(const QString &root)
    {
        stopService();
        processError_.clear();

        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(
            QStringLiteral("XDG_CONFIG_HOME"),
            root + QStringLiteral("/config")
        );
        environment.insert(
            QStringLiteral("XDG_STATE_HOME"),
            root + QStringLiteral("/state")
        );

        process_.setProcessEnvironment(environment);
        process_.setProgram(executable_);
        process_.setProcessChannelMode(QProcess::MergedChannels);
        process_.start();
        if (!process_.waitForStarted(3000)) {
            processError_ = process_.errorString();
            return false;
        }

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 5000) {
            if (process_.state() == QProcess::NotRunning) {
                processError_ = QString::fromUtf8(process_.readAll());
                return false;
            }

            const auto registered = bus_.interface()->isServiceRegistered(busName);
            if (registered.isValid() && registered.value()) {
                return true;
            }
            QTest::qWait(20);
        }

        processError_ = QStringLiteral("Timed out waiting for Config1 service");
        return false;
    }

    void stopService()
    {
        if (process_.state() != QProcess::NotRunning) {
            process_.terminate();
            if (!process_.waitForFinished(3000)) {
                process_.kill();
                process_.waitForFinished(3000);
            }
        }

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 1000) {
            const auto registered = bus_.interface()->isServiceRegistered(busName);
            if (!registered.isValid() || !registered.value()) {
                break;
            }
            QTest::qWait(10);
        }
    }

    QString executable_;
    QString processError_;
    QProcess process_;
    QDBusConnection bus_ = QDBusConnection::sessionBus();
};

QTEST_GUILESS_MAIN(ConfigClientTest)

#include "config_client_test.moc"
