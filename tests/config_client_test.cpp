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
        QCOMPARE(client.shellInnerSpacing(), 8U);
        QCOMPARE(client.shellOuterSpacing(), 12U);
        QCOMPARE(client.syncHyprlandWindowSpacing(), true);
        QCOMPARE(client.minimumShellSpacing(), 0U);
        QCOMPARE(client.maximumShellSpacing(), 32U);
        QCOMPARE(client.defaultShellInnerSpacing(), 8U);
        QCOMPARE(client.defaultShellOuterSpacing(), 12U);
        QCOMPARE(client.defaultSyncHyprlandWindowSpacing(), true);
        QCOMPARE(client.appearanceMode(), QStringLiteral("dark"));
        QCOMPARE(client.defaultAppearanceMode(), QStringLiteral("dark"));

        QVERIFY2(startService(directory.path()), qPrintable(processError_));
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.barHeight(), 40U);
        QCOMPARE(client.revision(), 0ULL);
        QCOMPARE(client.recoveryState(), QStringLiteral("normal"));
        QCOMPARE(client.appearanceMode(), QStringLiteral("dark"));

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

        QSignalSpy sharedSpacingChanged(
            &client,
            &HyprShelld::ConfigClient::sharedSpacingChanged
        );
        client.setSharedSpacing(0U, 32U, false);
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(sharedSpacingChanged.count(), 1, 3000);
        QCOMPARE(client.shellInnerSpacing(), 0U);
        QCOMPARE(client.shellOuterSpacing(), 32U);
        QCOMPARE(client.syncHyprlandWindowSpacing(), false);
        QCOMPARE(client.revision(), 4ULL);
        QVERIFY(client.lastErrorName().isEmpty());

        client.setSharedSpacing(0U, 33U, false);
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Config1.Error.InvalidSharedSpacing")
        );
        QVERIFY(!client.lastErrorMessage().isEmpty());
        QCOMPARE(client.shellInnerSpacing(), 0U);
        QCOMPARE(client.shellOuterSpacing(), 32U);
        QCOMPARE(client.syncHyprlandWindowSpacing(), false);
        QCOMPARE(client.revision(), 4ULL);

        stopService();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.barHeight(), 40U);
        QCOMPARE(client.shellBorderEnabled(), false);
        QCOMPARE(client.shellBorderWidth(), 7U);
        QCOMPARE(client.shellBorderRadius(), 12U);
        QCOMPARE(client.syncHyprlandWindowBorders(), false);
        QCOMPARE(client.shellInnerSpacing(), 0U);
        QCOMPARE(client.shellOuterSpacing(), 32U);
        QCOMPARE(client.syncHyprlandWindowSpacing(), false);
        QCOMPARE(client.revision(), 4ULL);

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
        QCOMPARE(client.shellInnerSpacing(), 0U);
        QCOMPARE(client.shellOuterSpacing(), 32U);
        QCOMPARE(client.syncHyprlandWindowSpacing(), false);
        QCOMPARE(client.revision(), 4ULL);
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorMessage().isEmpty());

        QDBusInterface external(busName, objectPath, interfaceName, bus_);
        QDBusPendingReply<qulonglong> changed = external.asyncCall(
            QStringLiteral("SetBarHeight"),
            72U
        );
        changed.waitForFinished();
        QVERIFY2(!changed.isError(), qPrintable(changed.error().message()));
        QCOMPARE(changed.value(), 5ULL);
        QTRY_COMPARE_WITH_TIMEOUT(client.barHeight(), 72U, 3000);
        QCOMPARE(client.revision(), 5ULL);

        client.resetSharedBorder();
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(sharedBorderChanged.count(), 2, 3000);
        QCOMPARE(client.shellBorderEnabled(), true);
        QCOMPARE(client.shellBorderWidth(), 1U);
        QCOMPARE(client.shellBorderRadius(), 15U);
        QCOMPARE(client.syncHyprlandWindowBorders(), true);
        QCOMPARE(client.revision(), 6ULL);

        client.resetSharedSpacing();
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(sharedSpacingChanged.count(), 2, 3000);
        QCOMPARE(client.shellInnerSpacing(), 8U);
        QCOMPARE(client.shellOuterSpacing(), 12U);
        QCOMPARE(client.syncHyprlandWindowSpacing(), true);
        QCOMPARE(client.revision(), 7ULL);

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
        QCOMPARE(client.revision(), 7ULL);
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
        QCOMPARE(client.revision(), 7ULL);
        QCOMPARE(barHeightChanged.count(), 0);
        QCOMPARE(revisionChanged.count(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        const QVariantMap malformedBorder{
            {QStringLiteral("ShellBorderEnabled"), false},
            {QStringLiteral("ShellBorderWidth"), QStringLiteral("7")},
            {QStringLiteral("ShellBorderRadius"), 12U},
            {QStringLiteral("SyncHyprlandWindowBorders"), false},
            {QStringLiteral("Revision"), qulonglong(8)},
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
        QCOMPARE(client.revision(), 7ULL);
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
        QCOMPARE(client.revision(), 7ULL);
        QCOMPARE(sharedBorderChanged.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        const QVariantMap spacingWithoutRevision{
            {QStringLiteral("ShellInnerSpacing"), 0U},
            {QStringLiteral("ShellOuterSpacing"), 32U},
            {QStringLiteral("SyncHyprlandWindowSpacing"), false},
        };
        QVERIFY(QMetaObject::invokeMethod(
            &client,
            "propertiesChanged",
            Qt::DirectConnection,
            Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, spacingWithoutRevision),
            Q_ARG(QStringList, noInvalidatedProperties)
        ));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.shellInnerSpacing(), 8U);
        QCOMPARE(client.shellOuterSpacing(), 12U);
        QCOMPARE(client.syncHyprlandWindowSpacing(), true);
        QCOMPARE(client.revision(), 7ULL);
        QCOMPARE(sharedSpacingChanged.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        const QVariantMap partialSpacing{
            {QStringLiteral("ShellInnerSpacing"), 0U},
            {QStringLiteral("Revision"), qulonglong(8)},
        };
        QVERIFY(QMetaObject::invokeMethod(
            &client,
            "propertiesChanged",
            Qt::DirectConnection,
            Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, partialSpacing),
            Q_ARG(QStringList, noInvalidatedProperties)
        ));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.shellInnerSpacing(), 8U);
        QCOMPARE(client.revision(), 7ULL);
        QCOMPARE(sharedSpacingChanged.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        const QVariantMap malformedSpacing{
            {QStringLiteral("ShellInnerSpacing"), 0U},
            {QStringLiteral("ShellOuterSpacing"), QStringLiteral("32")},
            {QStringLiteral("SyncHyprlandWindowSpacing"), false},
            {QStringLiteral("Revision"), qulonglong(8)},
        };
        QVERIFY(QMetaObject::invokeMethod(
            &client,
            "propertiesChanged",
            Qt::DirectConnection,
            Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, malformedSpacing),
            Q_ARG(QStringList, noInvalidatedProperties)
        ));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.shellOuterSpacing(), 12U);
        QCOMPARE(client.revision(), 7ULL);
        QCOMPARE(sharedSpacingChanged.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        auto outOfRangeSpacing = malformedSpacing;
        outOfRangeSpacing.insert(QStringLiteral("ShellOuterSpacing"), 33U);
        QVERIFY(QMetaObject::invokeMethod(
            &client,
            "propertiesChanged",
            Qt::DirectConnection,
            Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, outOfRangeSpacing),
            Q_ARG(QStringList, noInvalidatedProperties)
        ));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.shellOuterSpacing(), 12U);
        QCOMPARE(client.revision(), 7ULL);
        QCOMPARE(sharedSpacingChanged.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        const auto invokeSpacingRevision = [
            &client,
            &noInvalidatedProperties
        ](const qulonglong revision) {
            return QMetaObject::invokeMethod(
                &client,
                "propertiesChanged",
                Qt::DirectConnection,
                Q_ARG(QString, interfaceName),
                Q_ARG(QVariantMap, (QVariantMap{
                    {QStringLiteral("ShellInnerSpacing"), 0U},
                    {QStringLiteral("ShellOuterSpacing"), 32U},
                    {QStringLiteral("SyncHyprlandWindowSpacing"), false},
                    {QStringLiteral("Revision"), revision},
                })),
                Q_ARG(QStringList, noInvalidatedProperties)
            );
        };
        QVERIFY(invokeSpacingRevision(6));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.shellInnerSpacing(), 8U);
        QCOMPARE(client.revision(), 7ULL);
        QCOMPARE(sharedSpacingChanged.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        QVERIFY(invokeSpacingRevision(7));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.shellInnerSpacing(), 8U);
        QCOMPARE(client.revision(), 7ULL);
        QCOMPARE(sharedSpacingChanged.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        const QStringList invalidatedSpacing{
            QStringLiteral("ShellOuterSpacing"),
        };
        QVERIFY(QMetaObject::invokeMethod(
            &client,
            "propertiesChanged",
            Qt::DirectConnection,
            Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, QVariantMap()),
            Q_ARG(QStringList, invalidatedSpacing)
        ));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.shellOuterSpacing(), 12U);
        QCOMPARE(client.revision(), 7ULL);
        QCOMPARE(sharedSpacingChanged.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
    }

    void ownerReplacementGetAllCannotRollBackEstablishedRevision()
    {
        QTemporaryDir establishedRoot;
        QTemporaryDir lowerRoot;
        QVERIFY(establishedRoot.isValid());
        QVERIFY(lowerRoot.isValid());

        HyprShelld::ConfigClient client(bus_, nullptr);
        QVERIFY2(
            startService(establishedRoot.path()),
            qPrintable(processError_)
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        client.setBarHeight(60U);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 1ULL, 3000);
        client.setSharedBorder(false, 7U, 12U, false);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 2ULL, 3000);
        client.setSharedSpacing(0U, 32U, false);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 3ULL, 3000);
        QCOMPARE(client.barHeight(), 60U);
        QCOMPARE(client.shellBorderEnabled(), false);
        QCOMPARE(client.shellBorderWidth(), 7U);
        QCOMPARE(client.shellBorderRadius(), 12U);
        QCOMPARE(client.syncHyprlandWindowBorders(), false);
        QCOMPARE(client.shellInnerSpacing(), 0U);
        QCOMPARE(client.shellOuterSpacing(), 32U);
        QCOMPARE(client.syncHyprlandWindowSpacing(), false);

        stopService();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QVERIFY2(startService(lowerRoot.path()), qPrintable(processError_));
        // The fresh owner publishes a complete, internally valid revision-0
        // GetAll. It must not rewind the retained revision-3 ConfigState.
        QTest::qWait(250);
        QCOMPARE(client.available(), false);
        QCOMPARE(client.revision(), 3ULL);
        QCOMPARE(client.barHeight(), 60U);
        QCOMPARE(client.shellBorderEnabled(), false);
        QCOMPARE(client.shellBorderWidth(), 7U);
        QCOMPARE(client.shellBorderRadius(), 12U);
        QCOMPARE(client.syncHyprlandWindowBorders(), false);
        QCOMPARE(client.shellInnerSpacing(), 0U);
        QCOMPARE(client.shellOuterSpacing(), 32U);
        QCOMPARE(client.syncHyprlandWindowSpacing(), false);

        stopService();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QVERIFY2(
            startService(establishedRoot.path()),
            qPrintable(processError_)
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.revision(), 3ULL);
        QCOMPARE(client.barHeight(), 60U);
        QCOMPARE(client.shellBorderEnabled(), false);
        QCOMPARE(client.shellBorderWidth(), 7U);
        QCOMPARE(client.shellBorderRadius(), 12U);
        QCOMPARE(client.syncHyprlandWindowBorders(), false);
        QCOMPARE(client.shellInnerSpacing(), 0U);
        QCOMPARE(client.shellOuterSpacing(), 32U);
        QCOMPARE(client.syncHyprlandWindowSpacing(), false);
    }

    void tracksAppearanceModeAndRejectsMalformedProjections()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY2(startService(directory.path()), qPrintable(processError_));

        HyprShelld::ConfigClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.appearanceMode(), QStringLiteral("dark"));
        QCOMPARE(client.defaultAppearanceMode(), QStringLiteral("dark"));
        QCOMPARE(client.revision(), 0ULL);

        QSignalSpy appearanceModeChanged(
            &client,
            &HyprShelld::ConfigClient::appearanceModeChanged
        );
        QSignalSpy revisionChanged(
            &client,
            &HyprShelld::ConfigClient::revisionChanged
        );

        client.setAppearanceMode(QStringLiteral("light"));
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.appearanceMode(),
            QStringLiteral("light"),
            3000
        );
        QCOMPARE(client.revision(), 1ULL);
        QCOMPARE(appearanceModeChanged.count(), 1);
        QCOMPARE(revisionChanged.count(), 1);

        client.setAppearanceMode(QStringLiteral("system"));
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.Config1.Error.InvalidAppearanceMode"
            )
        );
        QCOMPARE(client.appearanceMode(), QStringLiteral("light"));
        QCOMPARE(client.revision(), 1ULL);
        QCOMPARE(appearanceModeChanged.count(), 1);

        client.setAppearanceMode(QStringLiteral("automatic"));
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.appearanceMode(),
            QStringLiteral("automatic"),
            3000
        );
        QCOMPARE(client.revision(), 2ULL);
        QCOMPARE(appearanceModeChanged.count(), 2);

        client.resetAppearanceMode();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.appearanceMode(),
            QStringLiteral("dark"),
            3000
        );
        QCOMPARE(client.revision(), 3ULL);
        QCOMPARE(appearanceModeChanged.count(), 3);

        const QStringList noInvalidatedProperties;
        const auto inject = [
            &client,
            &noInvalidatedProperties
        ](const QVariantMap &properties) {
            return QMetaObject::invokeMethod(
                &client,
                "propertiesChanged",
                Qt::DirectConnection,
                Q_ARG(QString, interfaceName),
                Q_ARG(QVariantMap, properties),
                Q_ARG(QStringList, noInvalidatedProperties)
            );
        };

        QVERIFY(inject({
            {QStringLiteral("AppearanceMode"), QStringLiteral("light")},
        }));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.appearanceMode(), QStringLiteral("dark"));
        QCOMPARE(client.revision(), 3ULL);
        QCOMPARE(appearanceModeChanged.count(), 3);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        QVERIFY(inject({
            {QStringLiteral("AppearanceMode"), 1U},
            {QStringLiteral("Revision"), qulonglong(4)},
        }));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.appearanceMode(), QStringLiteral("dark"));
        QCOMPARE(client.revision(), 3ULL);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        QVERIFY(inject({
            {QStringLiteral("AppearanceMode"), QStringLiteral("system")},
            {QStringLiteral("Revision"), qulonglong(4)},
        }));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.appearanceMode(), QStringLiteral("dark"));
        QCOMPARE(client.revision(), 3ULL);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        QVERIFY(inject({
            {QStringLiteral("AppearanceMode"), QStringLiteral("light")},
            {QStringLiteral("Revision"), qulonglong(3)},
        }));
        QCOMPARE(client.available(), false);
        QCOMPARE(client.appearanceMode(), QStringLiteral("dark"));
        QCOMPARE(client.revision(), 3ULL);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        QDBusInterface external(busName, objectPath, interfaceName, bus_);
        QDBusPendingReply<qulonglong> changed = external.asyncCall(
            QStringLiteral("SetAppearanceMode"),
            QStringLiteral("light")
        );
        changed.waitForFinished();
        QVERIFY2(!changed.isError(), qPrintable(changed.error().message()));
        QCOMPARE(changed.value(), 4ULL);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.appearanceMode(),
            QStringLiteral("light"),
            3000
        );
        QCOMPARE(client.revision(), 4ULL);
        QCOMPARE(appearanceModeChanged.count(), 4);

        const QStringList invalidatedAppearance{
            QStringLiteral("AppearanceMode"),
        };
        QVERIFY(QMetaObject::invokeMethod(
            &client,
            "propertiesChanged",
            Qt::DirectConnection,
            Q_ARG(QString, interfaceName),
            Q_ARG(QVariantMap, QVariantMap()),
            Q_ARG(QStringList, invalidatedAppearance)
        ));
        QCOMPARE(client.available(), false);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.appearanceMode(), QStringLiteral("light"));
        QCOMPARE(client.revision(), 4ULL);
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
