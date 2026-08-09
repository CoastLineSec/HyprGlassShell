#include "config_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using HyprShelld::ConfigPaths;
using HyprShelld::ConfigRecoveryState;
using HyprShelld::ConfigState;
using HyprShelld::ConfigStore;
using HyprShelld::LegacyWorkspaceSettings;

namespace {

ConfigPaths pathsFor(const QString &root)
{
    return {
        .activeFile = root + QStringLiteral("/config/hyprshelld/settings.json"),
        .recoveryFile = root
            + QStringLiteral("/state/hyprshelld/settings.last-good.json"),
    };
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(data) == data.size();
}

QByteArray snapshotWithWorkspace(const QByteArray &workspace, quint32 height = 40)
{
    return QByteArrayLiteral(
        "{\"formatVersion\":1,\"revision\":\"7\",\"barHeight\":"
    ) + QByteArray::number(height)
        + QByteArrayLiteral(",\"workspaceSwitcher\":")
        + workspace + QByteArrayLiteral("}\n");
}

} // namespace

class ConfigStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void createsNormalCoreDefaultsOnFirstRun()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());

        const auto loaded = ConfigStore(paths).load();

        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state, ConfigState());
        QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Normal);
        QVERIFY(!loaded.legacyWorkspaceSettings.has_value());
        QVERIFY(!loaded.legacyWorkspaceRetirementPending);
        QCOMPARE(readFile(paths.activeFile), readFile(paths.recoveryFile));

        const auto object = QJsonDocument::fromJson(
            readFile(paths.activeFile)
        ).object();
        QCOMPARE(object.size(), 3);
        QCOMPARE(object.value(QStringLiteral("formatVersion")).toInteger(), 1);
        QCOMPARE(object.value(QStringLiteral("revision")).toString(), QStringLiteral("0"));
        QCOMPARE(object.value(QStringLiteral("barHeight")).toInteger(), 40);
        QVERIFY(!object.contains(QStringLiteral("workspaceSwitcher")));
    }

    void persistsAndReloadsOnlyCoreState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        const ConfigStore store(paths);
        const auto loaded = store.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));

        const ConfigState next {.barHeight = 64, .revision = 1};
        QString error;
        QVERIFY2(
            store.persist(
                loaded.state,
                next,
                loaded.legacyWorkspaceSettings,
                error
            ),
            qPrintable(error)
        );

        const auto reloaded = ConfigStore(paths).load();
        QVERIFY2(reloaded.success, qPrintable(reloaded.error));
        QCOMPARE(reloaded.state, next);
        QCOMPARE(reloaded.recoveryState, ConfigRecoveryState::Normal);
        QVERIFY(!reloaded.legacyWorkspaceSettings.has_value());
        QVERIFY(!reloaded.legacyWorkspaceRetirementPending);
        QCOMPARE(readFile(paths.activeFile), readFile(paths.recoveryFile));
        QVERIFY(!QJsonDocument::fromJson(readFile(paths.activeFile))
                     .object()
                     .contains(QStringLiteral("workspaceSwitcher")));
    }

    void loadsExistingFormatOneHeights_data()
    {
        QTest::addColumn<quint32>("height");

        QTest::newRow("previous-minimum") << quint32(32);
        QTest::newRow("previous-default") << quint32(48);
    }

    void loadsExistingFormatOneHeights()
    {
        QFETCH(quint32, height);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        const auto snapshot = QByteArrayLiteral(
            "{\"formatVersion\":1,\"revision\":\"7\",\"barHeight\":"
        ) + QByteArray::number(height) + QByteArrayLiteral("}\n");
        QVERIFY(writeFile(paths.activeFile, snapshot));

        const auto loaded = ConfigStore(paths).load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state.barHeight, height);
        QCOMPARE(loaded.state.revision, quint64(7));
        QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Normal);
        QVERIFY(!loaded.legacyWorkspaceSettings.has_value());
        QVERIFY(!loaded.legacyWorkspaceRetirementPending);
        QCOMPARE(readFile(paths.activeFile), snapshot);
    }

    void extractsExactLegacyWorkspaceSettingsAndIgnoresRetiredPadding()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        const auto snapshot = snapshotWithWorkspace(QByteArrayLiteral(
            "{\"labelMode\":\"names\",\"showApplications\":true,"
            "\"maximumApplications\":5,\"paddingEnabled\":true,"
            "\"occupiedOnly\":true,\"scrollMode\":\"reversed\"}"
        ));
        QVERIFY(writeFile(paths.activeFile, snapshot));

        const auto loaded = ConfigStore(paths).load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state, (ConfigState{.barHeight = 40, .revision = 7}));
        QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Normal);
        QVERIFY(loaded.legacyWorkspaceSettings.has_value());
        QVERIFY(loaded.legacyWorkspaceRetirementPending);
        QCOMPARE(
            *loaded.legacyWorkspaceSettings,
            (LegacyWorkspaceSettings{
                .labelMode = QStringLiteral("names"),
                .showApplications = true,
                .maximumApplications = 5,
                .occupiedOnly = true,
                .scrollMode = QStringLiteral("reversed"),
            })
        );
        QCOMPARE(readFile(paths.activeFile), snapshot);

        const auto repairedRecovery = QJsonDocument::fromJson(
            readFile(paths.recoveryFile)
        ).object();
        const auto repairedWorkspace = repairedRecovery.value(
            QStringLiteral("workspaceSwitcher")
        ).toObject();
        QCOMPARE(repairedWorkspace.size(), 5);
        QCOMPARE(
            repairedWorkspace.value(QStringLiteral("labelMode")).toString(),
            QStringLiteral("names")
        );
        QVERIFY(!repairedWorkspace.contains(QStringLiteral("paddingEnabled")));
    }

    void malformedLegacyWorkspaceDoesNotPoisonCoreSettings_data()
    {
        QTest::addColumn<QByteArray>("workspace");

        QTest::newRow("not-object") << QByteArrayLiteral("true");
        QTest::newRow("missing-field")
            << QByteArrayLiteral("{\"labelMode\":\"compact\"}");
        QTest::newRow("invalid-label")
            << QByteArrayLiteral(
                   "{\"labelMode\":\"icons\",\"showApplications\":false,"
                   "\"maximumApplications\":3,\"occupiedOnly\":false,"
                   "\"scrollMode\":\"disabled\"}"
               );
        QTest::newRow("wrong-boolean")
            << QByteArrayLiteral(
                   "{\"labelMode\":\"numbers\",\"showApplications\":0,"
                   "\"maximumApplications\":3,\"occupiedOnly\":false,"
                   "\"scrollMode\":\"disabled\"}"
               );
        QTest::newRow("fractional-maximum")
            << QByteArrayLiteral(
                   "{\"labelMode\":\"numbers\",\"showApplications\":false,"
                   "\"maximumApplications\":3.5,\"occupiedOnly\":false,"
                   "\"scrollMode\":\"disabled\"}"
               );
        QTest::newRow("maximum-too-small")
            << QByteArrayLiteral(
                   "{\"labelMode\":\"numbers\",\"showApplications\":false,"
                   "\"maximumApplications\":0,\"occupiedOnly\":false,"
                   "\"scrollMode\":\"disabled\"}"
               );
        QTest::newRow("maximum-too-large")
            << QByteArrayLiteral(
                   "{\"labelMode\":\"numbers\",\"showApplications\":false,"
                   "\"maximumApplications\":6,\"occupiedOnly\":false,"
                   "\"scrollMode\":\"disabled\"}"
               );
        QTest::newRow("invalid-scroll")
            << QByteArrayLiteral(
                   "{\"labelMode\":\"numbers\",\"showApplications\":false,"
                   "\"maximumApplications\":3,\"occupiedOnly\":false,"
                   "\"scrollMode\":\"natural\"}"
               );
    }

    void malformedLegacyWorkspaceDoesNotPoisonCoreSettings()
    {
        QFETCH(QByteArray, workspace);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        QVERIFY(writeFile(paths.activeFile, snapshotWithWorkspace(workspace, 56)));

        const auto loaded = ConfigStore(paths).load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state.barHeight, 56U);
        QCOMPARE(loaded.state.revision, quint64(7));
        QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Normal);
        QVERIFY(!loaded.legacyWorkspaceSettings.has_value());
        QVERIFY(loaded.legacyWorkspaceRetirementPending);
    }

    void corePersistencePreservesLegacyWorkspaceUntilRetirement()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        QVERIFY(writeFile(paths.activeFile, snapshotWithWorkspace(QByteArrayLiteral(
            "{\"labelMode\":\"compact\",\"showApplications\":true,"
            "\"maximumApplications\":4,\"occupiedOnly\":false,"
            "\"scrollMode\":\"normal\"}"
        ))));

        const ConfigStore store(paths);
        const auto loaded = store.load();
        QVERIFY(loaded.legacyWorkspaceSettings.has_value());
        const ConfigState next {.barHeight = 72, .revision = 8};
        QString error;
        QVERIFY2(
            store.persist(
                loaded.state,
                next,
                loaded.legacyWorkspaceSettings,
                error
            ),
            qPrintable(error)
        );

        for (const auto &path : {paths.activeFile, paths.recoveryFile}) {
            const auto object = QJsonDocument::fromJson(readFile(path)).object();
            QCOMPARE(object.value(QStringLiteral("barHeight")).toInteger(), 72);
            QCOMPARE(object.value(QStringLiteral("revision")).toString(), QStringLiteral("8"));
            QCOMPARE(
                object.value(QStringLiteral("workspaceSwitcher"))
                    .toObject()
                    .value(QStringLiteral("labelMode"))
                    .toString(),
                QStringLiteral("compact")
            );
        }

        const auto restarted = ConfigStore(paths).load();
        QVERIFY2(restarted.success, qPrintable(restarted.error));
        QCOMPARE(restarted.state, next);
        QVERIFY(restarted.legacyWorkspaceSettings.has_value());
        QVERIFY(restarted.legacyWorkspaceRetirementPending);
    }

    void recoversCoreAndLegacySettingsFromLastGood()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        QVERIFY(writeFile(paths.activeFile, QByteArrayLiteral("not json\n")));
        QVERIFY(writeFile(paths.recoveryFile, snapshotWithWorkspace(QByteArrayLiteral(
            "{\"labelMode\":\"compact\",\"showApplications\":true,"
            "\"maximumApplications\":4,\"occupiedOnly\":false,"
            "\"scrollMode\":\"normal\"}"
        ), 72)));

        const auto recovered = ConfigStore(paths).load();
        QVERIFY2(recovered.success, qPrintable(recovered.error));
        QCOMPARE(recovered.state, (ConfigState{.barHeight = 72, .revision = 7}));
        QCOMPARE(recovered.recoveryState, ConfigRecoveryState::Recovered);
        QVERIFY(recovered.legacyWorkspaceSettings.has_value());
        QVERIFY(recovered.legacyWorkspaceRetirementPending);
        QCOMPARE(
            recovered.legacyWorkspaceSettings->labelMode,
            QStringLiteral("compact")
        );
        QVERIFY(QJsonDocument::fromJson(readFile(paths.activeFile))
                    .object()
                    .contains(QStringLiteral("workspaceSwitcher")));

        const auto restarted = ConfigStore(paths).load();
        QVERIFY2(restarted.success, qPrintable(restarted.error));
        QCOMPARE(restarted.state, recovered.state);
        QVERIFY(restarted.legacyWorkspaceSettings.has_value());
        QCOMPARE(
            restarted.legacyWorkspaceSettings->labelMode,
            QStringLiteral("compact")
        );
    }

    void partialLegacyRetirementIsCrashSafeRetryableAndIdempotent()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        QVERIFY(writeFile(paths.activeFile, snapshotWithWorkspace(QByteArrayLiteral(
            "{\"labelMode\":\"names\",\"showApplications\":true,"
            "\"maximumApplications\":5,\"occupiedOnly\":true,"
            "\"scrollMode\":\"reversed\"}"
        ))));

        const ConfigStore store(paths);
        const auto loaded = store.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QVERIFY(loaded.legacyWorkspaceRetirementPending);

        const auto configDirectory = QFileInfo(paths.activeFile).absolutePath();
        const auto heldDirectory = configDirectory + QStringLiteral(".held");
        QVERIFY(QDir().rename(configDirectory, heldDirectory));
        QFile blocker(configDirectory);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        QString error;
        QVERIFY(!store.retireLegacyWorkspaceSettings(loaded.state, error));
        QVERIFY(!error.isEmpty());

        QVERIFY(blocker.remove());
        QVERIFY(QDir().rename(heldDirectory, configDirectory));
        QVERIFY(QJsonDocument::fromJson(readFile(paths.activeFile))
                    .object()
                    .contains(QStringLiteral("workspaceSwitcher")));
        QVERIFY(!QJsonDocument::fromJson(readFile(paths.recoveryFile))
                     .object()
                     .contains(QStringLiteral("workspaceSwitcher")));

        const auto restarted = ConfigStore(paths).load();
        QVERIFY2(restarted.success, qPrintable(restarted.error));
        QVERIFY(restarted.legacyWorkspaceSettings.has_value());
        QVERIFY(restarted.legacyWorkspaceRetirementPending);
        for (const auto &path : {paths.activeFile, paths.recoveryFile}) {
            QVERIFY(QJsonDocument::fromJson(readFile(path))
                        .object()
                        .contains(QStringLiteral("workspaceSwitcher")));
        }

        error.clear();
        QVERIFY2(
            store.retireLegacyWorkspaceSettings(restarted.state, error),
            qPrintable(error)
        );
        const auto retiredActive = readFile(paths.activeFile);
        const auto retiredRecovery = readFile(paths.recoveryFile);
        for (const auto &bytes : {retiredActive, retiredRecovery}) {
            QVERIFY(!QJsonDocument::fromJson(bytes)
                         .object()
                         .contains(QStringLiteral("workspaceSwitcher")));
        }

        error.clear();
        QVERIFY2(
            store.retireLegacyWorkspaceSettings(restarted.state, error),
            qPrintable(error)
        );
        QCOMPARE(readFile(paths.activeFile), retiredActive);
        QCOMPARE(readFile(paths.recoveryFile), retiredRecovery);
    }

    void replacesDamagedCoreStateWithDefaults()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        QVERIFY(writeFile(paths.activeFile, QByteArrayLiteral("not json\n")));
        QVERIFY(writeFile(paths.recoveryFile, QByteArrayLiteral("also not json\n")));

        const auto defaulted = ConfigStore(paths).load();
        QVERIFY2(defaulted.success, qPrintable(defaulted.error));
        QCOMPARE(defaulted.state, ConfigState());
        QCOMPARE(defaulted.recoveryState, ConfigRecoveryState::Defaulted);
        QVERIFY(!defaulted.legacyWorkspaceSettings.has_value());
        QVERIFY(!defaulted.legacyWorkspaceRetirementPending);
        QCOMPARE(readFile(paths.activeFile), readFile(paths.recoveryFile));

        const auto reloaded = ConfigStore(paths).load();
        QCOMPARE(reloaded.recoveryState, ConfigRecoveryState::Normal);
    }

    void preservesUnsupportedFutureState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        QVERIFY(ConfigStore(paths).load().success);

        const QByteArray future(
            "{\"formatVersion\":2,\"revision\":\"9\",\"barHeight\":80}\n"
        );
        QVERIFY(writeFile(paths.activeFile, future));

        const auto loaded = ConfigStore(paths).load();
        QVERIFY(!loaded.success);
        QVERIFY(loaded.error.contains(QStringLiteral("Unsupported format version")));
        QCOMPARE(readFile(paths.activeFile), future);
    }

    void preservesUnsupportedFutureRecoveryState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        QVERIFY(ConfigStore(paths).load().success);

        const auto active = readFile(paths.activeFile);
        const QByteArray future(
            "{\"formatVersion\":2,\"revision\":\"9\",\"barHeight\":80}\n"
        );
        QVERIFY(writeFile(paths.recoveryFile, future));

        const auto loaded = ConfigStore(paths).load();
        QVERIFY(!loaded.success);
        QVERIFY(loaded.error.contains(QStringLiteral("Unsupported format version")));
        QCOMPARE(readFile(paths.activeFile), active);
        QCOMPARE(readFile(paths.recoveryFile), future);
    }

    void defaultsSemanticallyDamagedCoreState_data()
    {
        QTest::addColumn<QByteArray>("data");

        QTest::newRow("missing-height")
            << QByteArrayLiteral("{\"formatVersion\":1,\"revision\":\"0\"}\n");
        QTest::newRow("fractional-version")
            << QByteArrayLiteral(
                   "{\"formatVersion\":1.5,\"revision\":\"0\",\"barHeight\":48}\n"
               );
        QTest::newRow("fractional-height")
            << QByteArrayLiteral(
                   "{\"formatVersion\":1,\"revision\":\"0\",\"barHeight\":48.5}\n"
               );
        QTest::newRow("leading-zero-revision")
            << QByteArrayLiteral(
                   "{\"formatVersion\":1,\"revision\":\"01\",\"barHeight\":48}\n"
               );
        QTest::newRow("overflowing-revision")
            << QByteArrayLiteral(
                   "{\"formatVersion\":1,\"revision\":\"18446744073709551616\",\"barHeight\":48}\n"
               );
        QTest::newRow("height-out-of-range")
            << QByteArrayLiteral(
                   "{\"formatVersion\":1,\"revision\":\"0\",\"barHeight\":23}\n"
               );
    }

    void defaultsSemanticallyDamagedCoreState()
    {
        QFETCH(QByteArray, data);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        QVERIFY(writeFile(paths.activeFile, data));

        const auto loaded = ConfigStore(paths).load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state, ConfigState());
        QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Defaulted);
    }

    void boundsSnapshotBytesBeforeParsing()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        QVERIFY(writeFile(paths.activeFile, QByteArray(64 * 1024 + 1, 'x')));

        const auto loaded = ConfigStore(paths).load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Defaulted);
        QCOMPARE(loaded.state, ConfigState());
    }

    void failedWritePreservesActiveState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        const ConfigStore store(paths);
        const auto loaded = store.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));

        const auto original = readFile(paths.activeFile);
        const auto configDirectory = QFileInfo(paths.activeFile).absolutePath();
        const auto heldDirectory = configDirectory + QStringLiteral(".held");
        QVERIFY(QDir().rename(configDirectory, heldDirectory));
        QFile blocker(configDirectory);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        QString error;
        const ConfigState next {.barHeight = 56, .revision = 1};
        const auto persisted = store.persist(
            loaded.state,
            next,
            loaded.legacyWorkspaceSettings,
            error
        );

        QVERIFY(blocker.remove());
        QVERIFY(QDir().rename(heldDirectory, configDirectory));
        QVERIFY(!persisted);
        QVERIFY(!error.isEmpty());
        QCOMPARE(readFile(paths.activeFile), original);
    }
};

QTEST_APPLESS_MAIN(ConfigStoreTest)

#include "config_store_test.moc"
