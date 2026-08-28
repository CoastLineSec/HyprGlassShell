#include "config_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

using HyprShelld::ConfigPaths;
using HyprShelld::ConfigRecoveryState;
using HyprShelld::ConfigState;
using HyprShelld::ConfigStore;
using HyprShelld::LegacyWorkspaceSettings;

namespace {

ConfigPaths pathsFor(const QString &root) {
  return {
      .activeFile = root + QStringLiteral("/config/hyprshelld/settings.json"),
      .recoveryFile =
          root + QStringLiteral("/state/hyprshelld/settings.last-good.json"),
  };
}

QByteArray readFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}

bool writeFile(const QString &path, const QByteArray &data) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
         file.write(data) == data.size();
}

QByteArray snapshotWithWorkspace(const QByteArray &workspace,
                                 quint32 height = 40) {
  return QByteArrayLiteral(
             "{\"formatVersion\":1,\"revision\":\"7\",\"barHeight\":") +
         QByteArray::number(height) +
         QByteArrayLiteral(",\"workspaceSwitcher\":") + workspace +
         QByteArrayLiteral("}\n");
}

ConfigState migratedFormatOneState(const quint32 height,
                                   const quint64 revision = 7) {
  ConfigState state;
  state.barHeight = height;
  state.shellBorderEnabled = true;
  state.shellBorderWidth = 1;
  state.shellBorderRadius = std::min(16U, height * 3U / 8U);
  state.syncHyprlandWindowBorders = false;
  state.shellInnerSpacing = 8;
  state.shellOuterSpacing = 12;
  state.syncHyprlandWindowSpacing = false;
  state.revision = revision;
  return state;
}

} // namespace

class ConfigStoreTest final : public QObject {
  Q_OBJECT

private slots:
  void createsNormalCoreDefaultsOnFirstRun() {
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

    const auto object =
        QJsonDocument::fromJson(readFile(paths.activeFile)).object();
    QCOMPARE(object.size(), 13);
    QCOMPARE(object.value(QStringLiteral("formatVersion")).toInteger(), 5);
    QCOMPARE(object.value(QStringLiteral("revision")).toString(),
             QStringLiteral("0"));
    QCOMPARE(object.value(QStringLiteral("barHeight")).toInteger(), 40);
    QCOMPARE(object.value(QStringLiteral("shellBorderEnabled")).toBool(), true);
    QCOMPARE(object.value(QStringLiteral("shellBorderWidth")).toInteger(), 1);
    QCOMPARE(object.value(QStringLiteral("shellBorderRadius")).toInteger(), 15);
    QCOMPARE(object.value(QStringLiteral("syncHyprlandWindowBorders")).toBool(),
             true);
    QCOMPARE(object.value(QStringLiteral("shellInnerSpacing")).toInteger(), 8);
    QCOMPARE(object.value(QStringLiteral("shellOuterSpacing")).toInteger(), 12);
    QCOMPARE(object.value(QStringLiteral("syncHyprlandWindowSpacing")).toBool(),
             true);
    QCOMPARE(object.value(QStringLiteral("appearanceMode")).toString(),
             QStringLiteral("dark"));
    QCOMPARE(object.value(QStringLiteral("appearanceAutomation"))
                 .toObject()
                 .value(QStringLiteral("source"))
                 .toString(),
             QStringLiteral("desktop"));
    QCOMPARE(object.value(QStringLiteral("nightLight"))
                 .toObject()
                 .value(QStringLiteral("enabled"))
                 .toBool(),
             false);
    QVERIFY(!object.contains(QStringLiteral("workspaceSwitcher")));
  }

  void persistsAndReloadsOnlyCoreState() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    const ConfigStore store(paths);
    const auto loaded = store.load();
    QVERIFY2(loaded.success, qPrintable(loaded.error));

    auto next = loaded.state;
    next.barHeight = 64;
    next.shellBorderEnabled = false;
    next.shellBorderWidth = 7;
    next.shellBorderRadius = 3;
    next.syncHyprlandWindowBorders = false;
    next.shellInnerSpacing = 0;
    next.shellOuterSpacing = 32;
    next.syncHyprlandWindowSpacing = false;
    next.appearanceMode = QStringLiteral("light");
    next.appearanceAutomation.source = QStringLiteral("schedule");
    next.appearanceAutomation.schedule.mode = QStringLiteral("location");
    next.appearanceAutomation.schedule.darkStartMinute = 1190;
    next.appearanceAutomation.schedule.lightStartMinute = 415;
    next.appearanceAutomation.schedule.hasLocation = true;
    next.appearanceAutomation.schedule.latitude = 40.7128;
    next.appearanceAutomation.schedule.longitude = -74.0060;
    next.nightLight.enabled = true;
    next.nightLight.automatic = true;
    next.nightLight.schedule.mode = QStringLiteral("time");
    next.nightLight.schedule.darkStartMinute = 1230;
    next.nightLight.schedule.lightStartMinute = 390;
    next.nightLight.nightTemperature = 3600;
    next.nightLight.dayTemperature = 6800;
    next.nightLight.gradual = false;
    next.revision = 1;
    QString error;
    QVERIFY2(store.persist(loaded.state, next, loaded.legacyWorkspaceSettings,
                           error),
             qPrintable(error));

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

  void loadsExistingFormatOneHeights_data() {
    QTest::addColumn<quint32>("height");

    QTest::newRow("previous-minimum") << quint32(32);
    QTest::newRow("previous-default") << quint32(48);
    QTest::newRow("current-minimum") << quint32(24);
    QTest::newRow("current-default") << quint32(40);
    QTest::newRow("maximum") << quint32(96);
  }

  void loadsExistingFormatOneHeights() {
    QFETCH(quint32, height);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    const auto snapshot =
        QByteArrayLiteral(
            "{\"formatVersion\":1,\"revision\":\"7\",\"barHeight\":") +
        QByteArray::number(height) + QByteArrayLiteral("}\n");
    QVERIFY(writeFile(paths.activeFile, snapshot));

    const auto loaded = ConfigStore(paths).load();
    QVERIFY2(loaded.success, qPrintable(loaded.error));
    QCOMPARE(loaded.state, migratedFormatOneState(height));
    QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Normal);
    QVERIFY(!loaded.legacyWorkspaceSettings.has_value());
    QVERIFY(!loaded.legacyWorkspaceRetirementPending);
    QCOMPARE(readFile(paths.activeFile), readFile(paths.recoveryFile));
    const auto migrated =
        QJsonDocument::fromJson(readFile(paths.activeFile)).object();
    QCOMPARE(migrated.value(QStringLiteral("formatVersion")).toInteger(), 5);
    QCOMPARE(migrated.value(QStringLiteral("revision")).toString(),
             QStringLiteral("7"));
    QCOMPARE(migrated.value(QStringLiteral("barHeight")).toInteger(), height);
    QCOMPARE(migrated.value(QStringLiteral("shellBorderEnabled")).toBool(),
             true);
    QCOMPARE(migrated.value(QStringLiteral("shellBorderWidth")).toInteger(), 1);
    QCOMPARE(migrated.value(QStringLiteral("shellBorderRadius")).toInteger(),
             std::min(16U, height * 3U / 8U));
    QCOMPARE(
        migrated.value(QStringLiteral("syncHyprlandWindowBorders")).toBool(),
        false);
    QCOMPARE(migrated.value(QStringLiteral("shellInnerSpacing")).toInteger(),
             8);
    QCOMPARE(migrated.value(QStringLiteral("shellOuterSpacing")).toInteger(),
             12);
    QCOMPARE(
        migrated.value(QStringLiteral("syncHyprlandWindowSpacing")).toBool(),
        false);
    QCOMPARE(migrated.value(QStringLiteral("appearanceMode")).toString(),
             QStringLiteral("dark"));
  }

  void migratesFormatTwoSpacingWithoutAdvancingRevisionAndIsIdempotent() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    const auto snapshot = QByteArrayLiteral(
        "{\"formatVersion\":2,\"revision\":\"9\",\"barHeight\":56,"
        "\"shellBorderEnabled\":false,\"shellBorderWidth\":7,"
        "\"shellBorderRadius\":3,\"syncHyprlandWindowBorders\":true}\n");
    QVERIFY(writeFile(paths.activeFile, snapshot));

    const auto loaded = ConfigStore(paths).load();
    QVERIFY2(loaded.success, qPrintable(loaded.error));
    QCOMPARE(loaded.state.barHeight, 56U);
    QCOMPARE(loaded.state.shellBorderEnabled, false);
    QCOMPARE(loaded.state.shellBorderWidth, 7U);
    QCOMPARE(loaded.state.shellBorderRadius, 3U);
    QCOMPARE(loaded.state.syncHyprlandWindowBorders, true);
    QCOMPARE(loaded.state.shellInnerSpacing, 8U);
    QCOMPARE(loaded.state.shellOuterSpacing, 12U);
    QCOMPARE(loaded.state.syncHyprlandWindowSpacing, false);
    QCOMPARE(loaded.state.revision, quint64(9));
    QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Normal);
    QCOMPARE(readFile(paths.activeFile), readFile(paths.recoveryFile));

    const auto migratedBytes = readFile(paths.activeFile);
    const auto migrated = QJsonDocument::fromJson(migratedBytes).object();
    QCOMPARE(migrated.value(QStringLiteral("formatVersion")).toInteger(), 5);
    QCOMPARE(migrated.value(QStringLiteral("revision")).toString(),
             QStringLiteral("9"));
    QCOMPARE(migrated.value(QStringLiteral("shellInnerSpacing")).toInteger(),
             8);
    QCOMPARE(migrated.value(QStringLiteral("shellOuterSpacing")).toInteger(),
             12);
    QCOMPARE(
        migrated.value(QStringLiteral("syncHyprlandWindowSpacing")).toBool(),
        false);
    QCOMPARE(migrated.value(QStringLiteral("appearanceMode")).toString(),
             QStringLiteral("dark"));

    const auto restarted = ConfigStore(paths).load();
    QVERIFY2(restarted.success, qPrintable(restarted.error));
    QCOMPARE(restarted.state, loaded.state);
    QCOMPARE(restarted.recoveryState, ConfigRecoveryState::Normal);
    QCOMPARE(readFile(paths.activeFile), migratedBytes);
    QCOMPARE(readFile(paths.recoveryFile), migratedBytes);
  }

  void migratesFormatFourAutomationDefaultsWithoutAdvancingRevision() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    const auto snapshot = QByteArrayLiteral(
        "{\"formatVersion\":4,\"revision\":\"23\",\"barHeight\":56,"
        "\"shellBorderEnabled\":false,\"shellBorderWidth\":7,"
        "\"shellBorderRadius\":3,\"syncHyprlandWindowBorders\":true,"
        "\"shellInnerSpacing\":4,\"shellOuterSpacing\":22,"
        "\"syncHyprlandWindowSpacing\":false,"
        "\"appearanceMode\":\"light\"}\n");
    QVERIFY(writeFile(paths.activeFile, snapshot));

    const auto loaded = ConfigStore(paths).load();
    QVERIFY2(loaded.success, qPrintable(loaded.error));
    ConfigState expected;
    expected.barHeight = 56;
    expected.shellBorderEnabled = false;
    expected.shellBorderWidth = 7;
    expected.shellBorderRadius = 3;
    expected.syncHyprlandWindowBorders = true;
    expected.shellInnerSpacing = 4;
    expected.shellOuterSpacing = 22;
    expected.syncHyprlandWindowSpacing = false;
    expected.appearanceMode = QStringLiteral("light");
    expected.revision = 23;
    QCOMPARE(loaded.state, expected);
    QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Normal);

    const QJsonObject defaultAppearanceSchedule{
        {QStringLiteral("mode"), QStringLiteral("time")},
        {QStringLiteral("darkStartMinute"), 1080},
        {QStringLiteral("lightStartMinute"), 360},
        {QStringLiteral("locationSource"), QStringLiteral("manual")},
        {QStringLiteral("hasLocation"), false},
        {QStringLiteral("latitude"), 0.0},
        {QStringLiteral("longitude"), 0.0},
    };
    const QJsonObject defaultNightLightSchedule{
        {QStringLiteral("mode"), QStringLiteral("time")},
        {QStringLiteral("darkStartMinute"), 1200},
        {QStringLiteral("lightStartMinute"), 360},
        {QStringLiteral("locationSource"), QStringLiteral("manual")},
        {QStringLiteral("hasLocation"), false},
        {QStringLiteral("latitude"), 0.0},
        {QStringLiteral("longitude"), 0.0},
    };
    const QJsonObject defaultAutomation{
        {QStringLiteral("source"), QStringLiteral("desktop")},
        {QStringLiteral("schedule"), defaultAppearanceSchedule},
    };
    const QJsonObject defaultNightLight{
        {QStringLiteral("enabled"), false},
        {QStringLiteral("automatic"), true},
        {QStringLiteral("schedule"), defaultNightLightSchedule},
        {QStringLiteral("nightTemperature"), 4000},
        {QStringLiteral("dayTemperature"), 6500},
        {QStringLiteral("gradual"), true},
    };

    const auto activeBytes = readFile(paths.activeFile);
    QCOMPARE(readFile(paths.recoveryFile), activeBytes);
    for (const auto &path : {paths.activeFile, paths.recoveryFile}) {
      const auto migrated = QJsonDocument::fromJson(readFile(path)).object();
      QCOMPARE(migrated.value(QStringLiteral("formatVersion")).toInteger(), 5);
      QCOMPARE(migrated.value(QStringLiteral("revision")).toString(),
               QStringLiteral("23"));
      QCOMPARE(migrated.value(QStringLiteral("appearanceMode")).toString(),
               QStringLiteral("light"));
      QCOMPARE(migrated.value(QStringLiteral("appearanceAutomation"))
                   .toObject(),
               defaultAutomation);
      QCOMPARE(migrated.value(QStringLiteral("nightLight")).toObject(),
               defaultNightLight);
    }
  }

  void recoversAndMigratesFormatTwoSpacingWithoutAdvancingRevision() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    QVERIFY(writeFile(paths.activeFile, QByteArrayLiteral("not json\n")));
    QVERIFY(writeFile(
        paths.recoveryFile,
        QByteArrayLiteral(
            "{\"formatVersion\":2,\"revision\":\"11\",\"barHeight\":64,"
            "\"shellBorderEnabled\":true,\"shellBorderWidth\":2,"
            "\"shellBorderRadius\":6,\"syncHyprlandWindowBorders\":false}\n")));

    const auto recovered = ConfigStore(paths).load();
    QVERIFY2(recovered.success, qPrintable(recovered.error));
    QCOMPARE(recovered.recoveryState, ConfigRecoveryState::Recovered);
    QCOMPARE(recovered.state.revision, quint64(11));
    QCOMPARE(recovered.state.shellInnerSpacing, 8U);
    QCOMPARE(recovered.state.shellOuterSpacing, 12U);
    QCOMPARE(recovered.state.syncHyprlandWindowSpacing, false);
    QCOMPARE(readFile(paths.activeFile), readFile(paths.recoveryFile));
    QCOMPARE(QJsonDocument::fromJson(readFile(paths.activeFile))
                 .object()
                 .value(QStringLiteral("formatVersion"))
                 .toInteger(),
             5);
  }

  void extractsExactLegacyWorkspaceSettingsAndIgnoresRetiredPadding() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    const auto snapshot = snapshotWithWorkspace(QByteArrayLiteral(
        "{\"labelMode\":\"names\",\"showApplications\":true,"
        "\"maximumApplications\":5,\"paddingEnabled\":true,"
        "\"occupiedOnly\":true,\"scrollMode\":\"reversed\"}"));
    QVERIFY(writeFile(paths.activeFile, snapshot));

    const auto loaded = ConfigStore(paths).load();
    QVERIFY2(loaded.success, qPrintable(loaded.error));
    QCOMPARE(loaded.state, migratedFormatOneState(40));
    QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Normal);
    QVERIFY(loaded.legacyWorkspaceSettings.has_value());
    QVERIFY(loaded.legacyWorkspaceRetirementPending);
    QCOMPARE(*loaded.legacyWorkspaceSettings,
             (LegacyWorkspaceSettings{
                 .labelMode = QStringLiteral("names"),
                 .showApplications = true,
                 .maximumApplications = 5,
                 .occupiedOnly = true,
                 .scrollMode = QStringLiteral("reversed"),
             }));
    QCOMPARE(readFile(paths.activeFile), readFile(paths.recoveryFile));
    QCOMPARE(QJsonDocument::fromJson(readFile(paths.activeFile))
                 .object()
                 .value(QStringLiteral("formatVersion"))
                 .toInteger(),
             5);

    const auto repairedRecovery =
        QJsonDocument::fromJson(readFile(paths.recoveryFile)).object();
    const auto repairedWorkspace =
        repairedRecovery.value(QStringLiteral("workspaceSwitcher")).toObject();
    QCOMPARE(repairedWorkspace.size(), 5);
    QCOMPARE(repairedWorkspace.value(QStringLiteral("labelMode")).toString(),
             QStringLiteral("names"));
    QVERIFY(!repairedWorkspace.contains(QStringLiteral("paddingEnabled")));
  }

  void malformedLegacyWorkspaceDoesNotPoisonCoreSettings_data() {
    QTest::addColumn<QByteArray>("workspace");

    QTest::newRow("not-object") << QByteArrayLiteral("true");
    QTest::newRow("missing-field")
        << QByteArrayLiteral("{\"labelMode\":\"compact\"}");
    QTest::newRow("invalid-label") << QByteArrayLiteral(
        "{\"labelMode\":\"icons\",\"showApplications\":false,"
        "\"maximumApplications\":3,\"occupiedOnly\":false,"
        "\"scrollMode\":\"disabled\"}");
    QTest::newRow("wrong-boolean") << QByteArrayLiteral(
        "{\"labelMode\":\"numbers\",\"showApplications\":0,"
        "\"maximumApplications\":3,\"occupiedOnly\":false,"
        "\"scrollMode\":\"disabled\"}");
    QTest::newRow("fractional-maximum") << QByteArrayLiteral(
        "{\"labelMode\":\"numbers\",\"showApplications\":false,"
        "\"maximumApplications\":3.5,\"occupiedOnly\":false,"
        "\"scrollMode\":\"disabled\"}");
    QTest::newRow("maximum-too-small") << QByteArrayLiteral(
        "{\"labelMode\":\"numbers\",\"showApplications\":false,"
        "\"maximumApplications\":0,\"occupiedOnly\":false,"
        "\"scrollMode\":\"disabled\"}");
    QTest::newRow("maximum-too-large") << QByteArrayLiteral(
        "{\"labelMode\":\"numbers\",\"showApplications\":false,"
        "\"maximumApplications\":6,\"occupiedOnly\":false,"
        "\"scrollMode\":\"disabled\"}");
    QTest::newRow("invalid-scroll") << QByteArrayLiteral(
        "{\"labelMode\":\"numbers\",\"showApplications\":false,"
        "\"maximumApplications\":3,\"occupiedOnly\":false,"
        "\"scrollMode\":\"natural\"}");
  }

  void malformedLegacyWorkspaceDoesNotPoisonCoreSettings() {
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

  void corePersistencePreservesLegacyWorkspaceUntilRetirement() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    QVERIFY(
        writeFile(paths.activeFile,
                  snapshotWithWorkspace(QByteArrayLiteral(
                      "{\"labelMode\":\"compact\",\"showApplications\":true,"
                      "\"maximumApplications\":4,\"occupiedOnly\":false,"
                      "\"scrollMode\":\"normal\"}"))));

    const ConfigStore store(paths);
    const auto loaded = store.load();
    QVERIFY(loaded.legacyWorkspaceSettings.has_value());
    auto next = loaded.state;
    next.barHeight = 72;
    next.revision = 8;
    QString error;
    QVERIFY2(store.persist(loaded.state, next, loaded.legacyWorkspaceSettings,
                           error),
             qPrintable(error));

    for (const auto &path : {paths.activeFile, paths.recoveryFile}) {
      const auto object = QJsonDocument::fromJson(readFile(path)).object();
      QCOMPARE(object.value(QStringLiteral("barHeight")).toInteger(), 72);
      QCOMPARE(object.value(QStringLiteral("revision")).toString(),
               QStringLiteral("8"));
      QCOMPARE(object.value(QStringLiteral("workspaceSwitcher"))
                   .toObject()
                   .value(QStringLiteral("labelMode"))
                   .toString(),
               QStringLiteral("compact"));
    }

    const auto restarted = ConfigStore(paths).load();
    QVERIFY2(restarted.success, qPrintable(restarted.error));
    QCOMPARE(restarted.state, next);
    QVERIFY(restarted.legacyWorkspaceSettings.has_value());
    QVERIFY(restarted.legacyWorkspaceRetirementPending);
  }

  void recoversCoreAndLegacySettingsFromLastGood() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    QVERIFY(writeFile(paths.activeFile, QByteArrayLiteral("not json\n")));
    QVERIFY(writeFile(
        paths.recoveryFile,
        snapshotWithWorkspace(
            QByteArrayLiteral(
                "{\"labelMode\":\"compact\",\"showApplications\":true,"
                "\"maximumApplications\":4,\"occupiedOnly\":false,"
                "\"scrollMode\":\"normal\"}"),
            72)));

    const auto recovered = ConfigStore(paths).load();
    QVERIFY2(recovered.success, qPrintable(recovered.error));
    QCOMPARE(recovered.state, migratedFormatOneState(72));
    QCOMPARE(recovered.recoveryState, ConfigRecoveryState::Recovered);
    QVERIFY(recovered.legacyWorkspaceSettings.has_value());
    QVERIFY(recovered.legacyWorkspaceRetirementPending);
    QCOMPARE(recovered.legacyWorkspaceSettings->labelMode,
             QStringLiteral("compact"));
    QVERIFY(QJsonDocument::fromJson(readFile(paths.activeFile))
                .object()
                .contains(QStringLiteral("workspaceSwitcher")));

    const auto restarted = ConfigStore(paths).load();
    QVERIFY2(restarted.success, qPrintable(restarted.error));
    QCOMPARE(restarted.state, recovered.state);
    QVERIFY(restarted.legacyWorkspaceSettings.has_value());
    QCOMPARE(restarted.legacyWorkspaceSettings->labelMode,
             QStringLiteral("compact"));
  }

  void partialLegacyRetirementIsCrashSafeRetryableAndIdempotent() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    QVERIFY(writeFile(paths.activeFile,
                      snapshotWithWorkspace(QByteArrayLiteral(
                          "{\"labelMode\":\"names\",\"showApplications\":true,"
                          "\"maximumApplications\":5,\"occupiedOnly\":true,"
                          "\"scrollMode\":\"reversed\"}"))));

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
    QVERIFY2(store.retireLegacyWorkspaceSettings(restarted.state, error),
             qPrintable(error));
    const auto retiredActive = readFile(paths.activeFile);
    const auto retiredRecovery = readFile(paths.recoveryFile);
    for (const auto &bytes : {retiredActive, retiredRecovery}) {
      QVERIFY(!QJsonDocument::fromJson(bytes).object().contains(
          QStringLiteral("workspaceSwitcher")));
    }

    error.clear();
    QVERIFY2(store.retireLegacyWorkspaceSettings(restarted.state, error),
             qPrintable(error));
    QCOMPARE(readFile(paths.activeFile), retiredActive);
    QCOMPARE(readFile(paths.recoveryFile), retiredRecovery);
  }

  void replacesDamagedCoreStateWithDefaults() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    QVERIFY(writeFile(paths.activeFile, QByteArrayLiteral("not json\n")));
    QVERIFY(
        writeFile(paths.recoveryFile, QByteArrayLiteral("also not json\n")));

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

  void preservesUnsupportedFutureState() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    QVERIFY(ConfigStore(paths).load().success);

    const QByteArray future(
        "{\"formatVersion\":6,\"revision\":\"9\",\"barHeight\":80}\n");
    QVERIFY(writeFile(paths.activeFile, future));

    const auto loaded = ConfigStore(paths).load();
    QVERIFY(!loaded.success);
    QVERIFY(
        loaded.error.contains(QStringLiteral("Unsupported format version")));
    QCOMPARE(readFile(paths.activeFile), future);
  }

  void preservesUnsupportedFutureRecoveryState() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    QVERIFY(ConfigStore(paths).load().success);

    const auto active = readFile(paths.activeFile);
    const QByteArray future(
        "{\"formatVersion\":6,\"revision\":\"9\",\"barHeight\":80}\n");
    QVERIFY(writeFile(paths.recoveryFile, future));

    const auto loaded = ConfigStore(paths).load();
    QVERIFY(!loaded.success);
    QVERIFY(
        loaded.error.contains(QStringLiteral("Unsupported format version")));
    QCOMPARE(readFile(paths.activeFile), active);
    QCOMPARE(readFile(paths.recoveryFile), future);
  }

  void acceptsAndPreservesSharedSpacingBounds() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    const auto snapshot = QByteArrayLiteral(
        "{\"formatVersion\":3,\"revision\":\"17\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":true,"
        "\"shellInnerSpacing\":0,\"shellOuterSpacing\":32,"
        "\"syncHyprlandWindowSpacing\":false}\n");
    QVERIFY(writeFile(paths.activeFile, snapshot));

    const auto loaded = ConfigStore(paths).load();
    QVERIFY2(loaded.success, qPrintable(loaded.error));
    QCOMPARE(loaded.state.shellInnerSpacing, 0U);
    QCOMPARE(loaded.state.shellOuterSpacing, 32U);
    QCOMPARE(loaded.state.syncHyprlandWindowSpacing, false);
    QCOMPARE(loaded.state.appearanceMode, QStringLiteral("dark"));
    QCOMPARE(loaded.state.revision, quint64(17));
    QCOMPARE(QJsonDocument::fromJson(readFile(paths.activeFile)).object(),
             QJsonDocument::fromJson(readFile(paths.recoveryFile)).object());
    QCOMPARE(QJsonDocument::fromJson(readFile(paths.activeFile))
                 .object()
                 .value(QStringLiteral("formatVersion"))
                 .toInteger(),
             5);
  }

  void defaultsSemanticallyDamagedCoreState_data() {
    QTest::addColumn<QByteArray>("data");

    QTest::newRow("missing-height")
        << QByteArrayLiteral("{\"formatVersion\":1,\"revision\":\"0\"}\n");
    QTest::newRow("fractional-version") << QByteArrayLiteral(
        "{\"formatVersion\":1.5,\"revision\":\"0\",\"barHeight\":48}\n");
    QTest::newRow("fractional-height") << QByteArrayLiteral(
        "{\"formatVersion\":1,\"revision\":\"0\",\"barHeight\":48.5}\n");
    QTest::newRow("leading-zero-revision") << QByteArrayLiteral(
        "{\"formatVersion\":1,\"revision\":\"01\",\"barHeight\":48}\n");
    QTest::newRow("overflowing-revision")
        << QByteArrayLiteral("{\"formatVersion\":1,\"revision\":"
                             "\"18446744073709551616\",\"barHeight\":48}\n");
    QTest::newRow("height-out-of-range") << QByteArrayLiteral(
        "{\"formatVersion\":1,\"revision\":\"0\",\"barHeight\":23}\n");
    QTest::newRow("version-two-missing-border") << QByteArrayLiteral(
        "{\"formatVersion\":2,\"revision\":\"0\",\"barHeight\":40}\n");
    QTest::newRow("border-enabled-not-boolean") << QByteArrayLiteral(
        "{\"formatVersion\":2,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":1,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":true}\n");
    QTest::newRow("border-width-out-of-range") << QByteArrayLiteral(
        "{\"formatVersion\":2,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":21,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":true}\n");
    QTest::newRow("border-radius-fractional") << QByteArrayLiteral(
        "{\"formatVersion\":2,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":2.5,\"syncHyprlandWindowBorders\":true}\n");
    QTest::newRow("border-sync-not-boolean") << QByteArrayLiteral(
        "{\"formatVersion\":2,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":\"yes\"}\n");
    QTest::newRow("version-three-missing-spacing") << QByteArrayLiteral(
        "{\"formatVersion\":3,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":true}\n");
    QTest::newRow("inner-spacing-fractional") << QByteArrayLiteral(
        "{\"formatVersion\":3,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":true,"
        "\"shellInnerSpacing\":8.5,\"shellOuterSpacing\":12,"
        "\"syncHyprlandWindowSpacing\":true}\n");
    QTest::newRow("inner-spacing-negative") << QByteArrayLiteral(
        "{\"formatVersion\":3,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":true,"
        "\"shellInnerSpacing\":-1,\"shellOuterSpacing\":12,"
        "\"syncHyprlandWindowSpacing\":true}\n");
    QTest::newRow("outer-spacing-too-large") << QByteArrayLiteral(
        "{\"formatVersion\":3,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":true,"
        "\"shellInnerSpacing\":8,\"shellOuterSpacing\":33,"
        "\"syncHyprlandWindowSpacing\":true}\n");
    QTest::newRow("spacing-sync-not-boolean") << QByteArrayLiteral(
        "{\"formatVersion\":3,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":true,"
        "\"shellInnerSpacing\":8,\"shellOuterSpacing\":12,"
        "\"syncHyprlandWindowSpacing\":\"yes\"}\n");
    QTest::newRow("version-four-missing-appearance-mode") << QByteArrayLiteral(
        "{\"formatVersion\":4,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":true,"
        "\"shellInnerSpacing\":8,\"shellOuterSpacing\":12,"
        "\"syncHyprlandWindowSpacing\":true}\n");
    QTest::newRow("appearance-mode-not-string") << QByteArrayLiteral(
        "{\"formatVersion\":4,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":true,"
        "\"shellInnerSpacing\":8,\"shellOuterSpacing\":12,"
        "\"syncHyprlandWindowSpacing\":true,\"appearanceMode\":1}\n");
    QTest::newRow("appearance-mode-unknown") << QByteArrayLiteral(
        "{\"formatVersion\":4,\"revision\":\"0\",\"barHeight\":40,"
        "\"shellBorderEnabled\":true,\"shellBorderWidth\":1,"
        "\"shellBorderRadius\":15,\"syncHyprlandWindowBorders\":true,"
        "\"shellInnerSpacing\":8,\"shellOuterSpacing\":12,"
        "\"syncHyprlandWindowSpacing\":true,\"appearanceMode\":\"system\"}\n");
  }

  void defaultsSemanticallyDamagedCoreState() {
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

  void boundsSnapshotBytesBeforeParsing() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = pathsFor(directory.path());
    QVERIFY(writeFile(paths.activeFile, QByteArray(64 * 1024 + 1, 'x')));

    const auto loaded = ConfigStore(paths).load();
    QVERIFY2(loaded.success, qPrintable(loaded.error));
    QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Defaulted);
    QCOMPARE(loaded.state, ConfigState());
  }

  void failedWritePreservesActiveState() {
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
    auto next = loaded.state;
    next.barHeight = 56;
    next.shellInnerSpacing = 0;
    next.shellOuterSpacing = 32;
    next.syncHyprlandWindowSpacing = false;
    next.revision = 1;
    const auto persisted = store.persist(loaded.state, next,
                                         loaded.legacyWorkspaceSettings, error);

    QVERIFY(blocker.remove());
    QVERIFY(QDir().rename(heldDirectory, configDirectory));
    QVERIFY(!persisted);
    QVERIFY(!error.isEmpty());
    QCOMPARE(readFile(paths.activeFile), original);
  }
};

QTEST_APPLESS_MAIN(ConfigStoreTest)

#include "config_store_test.moc"
