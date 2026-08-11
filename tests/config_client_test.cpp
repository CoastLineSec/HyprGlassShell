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
        QCOMPARE(client.barHeight(), 40U);
        QCOMPARE(client.minimumBarHeight(), 24U);
        QCOMPARE(client.maximumBarHeight(), 96U);
        QCOMPARE(client.defaultBarHeight(), 40U);
        QCOMPARE(client.shellBorderEnabled(), true);
        QCOMPARE(client.shellBorderWidth(), 1U);
        QCOMPARE(client.shellBorderRadius(), 15U);
        QCOMPARE(client.syncHyprlandWindowBorders(), true);
        QCOMPARE(client.defaultShellBorderEnabled(), true);
        QCOMPARE(client.minimumShellBorderWidth(), 0U);
        QCOMPARE(client.maximumShellBorderWidth(), 20U);
        QCOMPARE(client.defaultShellBorderWidth(), 1U);
        QCOMPARE(client.minimumShellBorderRadius(), 0U);
        QCOMPARE(client.maximumShellBorderRadius(), 20U);
        QCOMPARE(client.defaultShellBorderRadius(), 15U);
        QCOMPARE(client.defaultSyncHyprlandWindowBorders(), true);

        QVERIFY2(startService(directory.path()), qPrintable(processError_));
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.barHeight(), 40U);
        QCOMPARE(client.revision(), 0ULL);
        QCOMPARE(client.recoveryState(), QStringLiteral("normal"));

        client.setBarHeight(60);
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.barHeight(), 60U, 3000);
        QCOMPARE(client.revision(), 1ULL);
        QVERIFY(client.lastErrorName().isEmpty());

        client.setBarHeight(23);
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
        QTRY_COMPARE_WITH_TIMEOUT(client.barHeight(), 40U, 3000);
        QCOMPARE(client.revision(), 2ULL);

        QSignalSpy sharedBorderChanged(
            &client,
            &HyprShelld::ConfigClient::sharedBorderChanged
        );
        client.setSharedBorder(false, 7U, 12U, false);
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(sharedBorderChanged.count(), 1, 3000);
        QCOMPARE(client.shellBorderEnabled(), false);
        QCOMPARE(client.shellBorderWidth(), 7U);
        QCOMPARE(client.shellBorderRadius(), 12U);
        QCOMPARE(client.syncHyprlandWindowBorders(), false);
        QCOMPARE(client.revision(), 3ULL);
        QVERIFY(client.lastErrorName().isEmpty());

        client.setSharedBorder(false, 21U, 12U, false);
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Config1.Error.InvalidSharedBorder")
        );
        QVERIFY(!client.lastErrorMessage().isEmpty());
        QCOMPARE(client.shellBorderWidth(), 7U);
        QCOMPARE(client.revision(), 3ULL);

        stopService();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.barHeight(), 40U);
        QCOMPARE(client.shellBorderEnabled(), false);
        QCOMPARE(client.shellBorderWidth(), 7U);
        QCOMPARE(client.shellBorderRadius(), 12U);
        QCOMPARE(client.syncHyprlandWindowBorders(), false);
        QCOMPARE(client.revision(), 3ULL);

        client.setBarHeight(64);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QVERIFY(!client.lastErrorName().isEmpty());

        QVERIFY2(startService(directory.path()), qPrintable(processError_));
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.barHeight(), 40U);
        QCOMPARE(client.shellBorderEnabled(), false);
        QCOMPARE(client.shellBorderWidth(), 7U);
        QCOMPARE(client.shellBorderRadius(), 12U);
        QCOMPARE(client.syncHyprlandWindowBorders(), false);
        QCOMPARE(client.revision(), 3ULL);
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorMessage().isEmpty());

        QDBusInterface external(busName, objectPath, interfaceName, bus_);
        QDBusPendingReply<qulonglong> changed = external.asyncCall(
            QStringLiteral("SetBarHeight"),
            72U
        );
        changed.waitForFinished();
        QVERIFY2(!changed.isError(), qPrintable(changed.error().message()));
        QCOMPARE(changed.value(), 4ULL);
        QTRY_COMPARE_WITH_TIMEOUT(client.barHeight(), 72U, 3000);
        QCOMPARE(client.revision(), 4ULL);

        client.resetSharedBorder();
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(sharedBorderChanged.count(), 2, 3000);
        QCOMPARE(client.shellBorderEnabled(), true);
        QCOMPARE(client.shellBorderWidth(), 1U);
        QCOMPARE(client.shellBorderRadius(), 15U);
        QCOMPARE(client.syncHyprlandWindowBorders(), true);
        QCOMPARE(client.revision(), 5ULL);

        QSignalSpy barHeightChanged(
            &client,
            &HyprShelld::ConfigClient::barHeightChanged
        );
        QSignalSpy revisionChanged(
            &client,
            &HyprShelld::ConfigClient::revisionChanged
        );
        const QStringList noInvalidatedProperties;
        const QVariantMap borderWithoutRevision{
            {QStringLiteral("ShellBorderEnabled"), false},
            {QStringLiteral("ShellBorderWidth"), 7U},
            {QStringLiteral("ShellBorderRadius"), 12U},
            {QStringLiteral("SyncHyprlandWindowBorders"), false},
        };
        QVERIFY(QMetaObject::invokeMethod(
            &client,
            "propertiesChanged",
            Qt::DirectConnection,
            Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, borderWithoutRevision),
            Q_ARG(QStringList, noInvalidatedProperties)
        ));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.shellBorderEnabled(), true);
        QCOMPARE(client.shellBorderWidth(), 1U);
        QCOMPARE(client.shellBorderRadius(), 15U);
        QCOMPARE(client.syncHyprlandWindowBorders(), true);
        QCOMPARE(client.revision(), 5ULL);
        QCOMPARE(sharedBorderChanged.count(), 2);
        QCOMPARE(revisionChanged.count(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        const QVariantMap barHeightWithoutRevision{
            {QStringLiteral("BarHeight"), 80U},
        };
        QVERIFY(QMetaObject::invokeMethod(
            &client,
            "propertiesChanged",
            Qt::DirectConnection,
            Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, barHeightWithoutRevision),
            Q_ARG(QStringList, noInvalidatedProperties)
        ));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.barHeight(), 72U);
        QCOMPARE(client.revision(), 5ULL);
        QCOMPARE(barHeightChanged.count(), 0);
        QCOMPARE(revisionChanged.count(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        const QVariantMap malformedBorder{
            {QStringLiteral("ShellBorderEnabled"), false},
            {QStringLiteral("ShellBorderWidth"), QStringLiteral("7")},
            {QStringLiteral("ShellBorderRadius"), 12U},
            {QStringLiteral("SyncHyprlandWindowBorders"), false},
            {QStringLiteral("Revision"), qulonglong(6)},
        };
        QVERIFY(QMetaObject::invokeMethod(
            &client,
            "propertiesChanged",
            Qt::DirectConnection,
            Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, malformedBorder),
            Q_ARG(QStringList, noInvalidatedProperties)
        ));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.shellBorderEnabled(), true);
        QCOMPARE(client.shellBorderWidth(), 1U);
        QCOMPARE(client.shellBorderRadius(), 15U);
        QCOMPARE(client.syncHyprlandWindowBorders(), true);
        QCOMPARE(client.revision(), 5ULL);
        QCOMPARE(sharedBorderChanged.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        auto outOfRangeBorder = malformedBorder;
        outOfRangeBorder.insert(QStringLiteral("ShellBorderWidth"), 21U);
        QVERIFY(QMetaObject::invokeMethod(
            &client,
            "propertiesChanged",
            Qt::DirectConnection,
            Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, outOfRangeBorder),
            Q_ARG(QStringList, noInvalidatedProperties)
        ));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.shellBorderWidth(), 1U);
        QCOMPARE(client.revision(), 5ULL);
        QCOMPARE(sharedBorderChanged.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
    }

    void exposesAnExactRevisionTokenBeyondQmlIntegerPrecision()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY2(startService(directory.path()), qPrintable(processError_));

        HyprShelld::ConfigClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        constexpr qulonglong exactRevision = 9'007'199'254'740'993ULL;
        const QVariantMap revisionChange{
            {
                QStringLiteral("Revision"),
                QVariant::fromValue<qulonglong>(exactRevision)
            },
        };
        const QStringList noInvalidatedProperties;
        QVERIFY(QMetaObject::invokeMethod(
            &client,
            "propertiesChanged",
            Qt::DirectConnection,
            Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, revisionChange),
            Q_ARG(QStringList, noInvalidatedProperties)
        ));

        QCOMPARE(client.revision(), exactRevision);
        QCOMPARE(client.revisionToken(), QStringLiteral("9007199254740993"));
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
