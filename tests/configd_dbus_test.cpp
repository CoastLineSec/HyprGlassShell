#include "config1_interface.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QXmlStreamReader>
#include <QtTest>

#include <limits>
#include <utility>

namespace {

const QString busName = QStringLiteral("org.hyprshelld.Config1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Config1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Config1");
const QString propertiesInterface =
    QStringLiteral("org.freedesktop.DBus.Properties");

QString attribute(const QXmlStreamReader &xml, const char16_t *name) {
  return xml.attributes().value(QStringView(name)).toString();
}

QStringList describeInterface(QXmlStreamReader &xml, QString &error) {
  QStringList description;
  QString memberKind;
  QString memberName;
  bool insideTarget = false;

  while (!xml.atEnd()) {
    xml.readNext();

    if (xml.isStartElement()) {
      const auto element = xml.name();
      if (element == u"interface") {
        insideTarget = attribute(xml, u"name") == interfaceName;
        if (insideTarget) {
          description.append(QStringLiteral("interface=") + interfaceName);
        }
      } else if (insideTarget && element == u"property") {
        memberKind = QStringLiteral("property");
        memberName = attribute(xml, u"name");
        description.append(QStringLiteral("property=%1:%2:%3")
                               .arg(memberName, attribute(xml, u"type"),
                                    attribute(xml, u"access")));
      } else if (insideTarget &&
                 (element == u"method" || element == u"signal")) {
        memberKind = element.toString();
        memberName = attribute(xml, u"name");
        description.append(QStringLiteral("%1=%2").arg(memberKind, memberName));
      } else if (insideTarget && element == u"arg") {
        description.append(
            QStringLiteral("arg=%1:%2:%3:%4:%5")
                .arg(memberKind, memberName, attribute(xml, u"name"),
                     attribute(xml, u"type"), attribute(xml, u"direction")));
      } else if (insideTarget && element == u"annotation") {
        description.append(QStringLiteral("annotation=%1:%2:%3:%4")
                               .arg(memberKind, memberName,
                                    attribute(xml, u"name"),
                                    attribute(xml, u"value")));
      }
    } else if (xml.isEndElement()) {
      const auto element = xml.name();
      if (insideTarget && (element == u"property" || element == u"method" ||
                           element == u"signal")) {
        memberKind.clear();
        memberName.clear();
      } else if (element == u"interface" && insideTarget) {
        insideTarget = false;
      }
    }
  }

  if (xml.hasError()) {
    error = xml.errorString();
    return {};
  }

  description.sort();
  return description;
}

QStringList describeInterface(const QString &xmlText, QString &error) {
  QXmlStreamReader xml(xmlText);
  return describeInterface(xml, error);
}

QStringList describeInterfaceFile(const QString &path, QString &error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    error = file.errorString();
    return {};
  }

  QXmlStreamReader xml(&file);
  return describeInterface(xml, error);
}

QByteArray readFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}

bool writeFile(const QString &path, const QByteArray &data) {
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
    return false;
  }

  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
         file.write(data) == data.size();
}

bool blockDirectory(const QString &path) {
  if (!QDir().rename(path, path + QStringLiteral(".held"))) {
    return false;
  }

  QFile blocker(path);
  return blocker.open(QIODevice::WriteOnly);
}

bool restoreDirectory(const QString &path) {
  return QFile::remove(path) &&
         QDir().rename(path + QStringLiteral(".held"), path);
}

} // namespace

class ConfigdDbusTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    executable_ = qEnvironmentVariable("HYPRSHELLD_CONFIGD_EXECUTABLE");
    contractPath_ = qEnvironmentVariable("HYPRSHELLD_CONFIG1_XML");
    QVERIFY(!executable_.isEmpty());
    QVERIFY(!contractPath_.isEmpty());
    QVERIFY2(bus_.isConnected(), qPrintable(bus_.lastError().message()));
  }

  void cleanup() {
    stopService();
    resetSignalCapture();
  }

  void exportsContractAndMutatesConfiguration() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY2(startService(directory.path()), qPrintable(processError_));

    OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
    QVERIFY(proxy.isValid());
    QCOMPARE(proxy.barHeight(), 40U);
    QCOMPARE(proxy.shellBorderEnabled(), true);
    QCOMPARE(proxy.shellBorderWidth(), 1U);
    QCOMPARE(proxy.shellBorderRadius(), 15U);
    QCOMPARE(proxy.syncHyprlandWindowBorders(), true);
    QCOMPARE(proxy.shellInnerSpacing(), 8U);
    QCOMPARE(proxy.shellOuterSpacing(), 12U);
    QCOMPARE(proxy.syncHyprlandWindowSpacing(), true);
    QCOMPARE(proxy.appearanceMode(), QStringLiteral("dark"));
    QCOMPARE(proxy.appearanceAutomationSource(), QStringLiteral("desktop"));
    QCOMPARE(proxy.appearanceScheduleMode(), QStringLiteral("time"));
    QCOMPARE(proxy.appearanceDarkStartMinute(), 18U * 60U);
    QCOMPARE(proxy.appearanceLightStartMinute(), 6U * 60U);
    QCOMPARE(proxy.appearanceLocationSource(), QStringLiteral("manual"));
    QCOMPARE(proxy.appearanceHasLocation(), false);
    QCOMPARE(proxy.appearanceLatitude(), 0.0);
    QCOMPARE(proxy.appearanceLongitude(), 0.0);
    QCOMPARE(proxy.scheduledAppearanceMode(), QStringLiteral("system"));
    QCOMPARE(proxy.appearanceNextTransition(), QString());
    QCOMPARE(proxy.appearanceSunrise(), QString());
    QCOMPARE(proxy.appearanceSunset(), QString());
    QCOMPARE(proxy.appearanceAutomationStatus(), QStringLiteral("desktop"));
    QCOMPARE(proxy.nightLightEnabled(), false);
    QCOMPARE(proxy.nightLightAutomatic(), true);
    QCOMPARE(proxy.nightLightScheduleMode(), QStringLiteral("time"));
    QCOMPARE(proxy.nightLightDarkStartMinute(), 20U * 60U);
    QCOMPARE(proxy.nightLightLightStartMinute(), 6U * 60U);
    QCOMPARE(proxy.nightLightLocationSource(), QStringLiteral("manual"));
    QCOMPARE(proxy.nightLightHasLocation(), false);
    QCOMPARE(proxy.nightLightLatitude(), 0.0);
    QCOMPARE(proxy.nightLightLongitude(), 0.0);
    QCOMPARE(proxy.nightLightTemperature(), 4000U);
    QCOMPARE(proxy.nightLightDayTemperature(), 6500U);
    QCOMPARE(proxy.nightLightGradual(), true);
    QCOMPARE(proxy.hyprsunsetAvailable(), true);
    QCOMPARE(proxy.nightLightRuntimeState(), QStringLiteral("disabled"));
    QCOMPARE(proxy.nightLightCurrentTemperature(), 0U);
    const auto nightLightNextTransition = QDateTime::fromString(
        proxy.nightLightNextTransition(), Qt::ISODateWithMs);
    QVERIFY(nightLightNextTransition.isValid());
    QVERIFY(nightLightNextTransition > QDateTime::currentDateTime());
    QVERIFY(QDateTime::fromString(proxy.nightLightSunrise(), Qt::ISODateWithMs)
                .isValid());
    QVERIFY(QDateTime::fromString(proxy.nightLightSunset(), Qt::ISODateWithMs)
                .isValid());
    QCOMPARE(proxy.nightLightStatus(), QStringLiteral("disabled"));
    QCOMPARE(proxy.revision(), 0ULL);
    QCOMPARE(proxy.recoveryState(), QStringLiteral("normal"));

    QDBusInterface introspection(
        busName, objectPath,
        QStringLiteral("org.freedesktop.DBus.Introspectable"), bus_);
    const QDBusReply<QString> liveXml =
        introspection.call(QStringLiteral("Introspect"));
    QVERIFY2(liveXml.isValid(), qPrintable(liveXml.error().message()));

    QString sourceError;
    QString liveError;
    const auto sourceDescription =
        describeInterfaceFile(contractPath_, sourceError);
    const auto liveDescription = describeInterface(liveXml.value(), liveError);
    QVERIFY2(sourceError.isEmpty(), qPrintable(sourceError));
    QVERIFY2(liveError.isEmpty(), qPrintable(liveError));
    QCOMPARE(liveDescription, sourceDescription);

    QVERIFY(connectPropertiesSignal());
    const auto originalBytes = readFile(activeFile_);
    QVERIFY(!originalBytes.isEmpty());

    for (const auto invalidHeight : {23U, 97U}) {
      auto invalid = proxy.SetBarHeight(invalidHeight);
      invalid.waitForFinished();
      QVERIFY(invalid.isError());
      QCOMPARE(invalid.error().name(),
               QStringLiteral("org.hyprshelld.Config1.Error.InvalidBarHeight"));
    }
    QTest::qWait(50);
    QCOMPARE(signalCount_, 0);
    QCOMPARE(proxy.barHeight(), 40U);
    QCOMPARE(proxy.revision(), 0ULL);
    QCOMPARE(readFile(activeFile_), originalBytes);

    auto changed = proxy.SetBarHeight(60);
    changed.waitForFinished();
    QVERIFY2(!changed.isError(), qPrintable(changed.error().message()));
    QCOMPARE(changed.value(), 1ULL);
    QTRY_COMPARE_WITH_TIMEOUT(signalCount_, 1, 1000);
    QCOMPARE(lastChanged_.value(QStringLiteral("BarHeight")).toUInt(), 60U);
    QCOMPARE(lastChanged_.value(QStringLiteral("Revision")).toULongLong(),
             1ULL);
    QVERIFY(lastInvalidated_.isEmpty());
    QVERIFY(!bytesSeenAtSignal_.isEmpty());

    const auto persisted = QJsonDocument::fromJson(bytesSeenAtSignal_).object();
    QCOMPARE(persisted.size(), 13);
    QCOMPARE(persisted.value(QStringLiteral("formatVersion")).toInteger(), 5);
    QCOMPARE(persisted.value(QStringLiteral("barHeight")).toInteger(), 60);
    QCOMPARE(persisted.value(QStringLiteral("shellBorderEnabled")).toBool(),
             true);
    QCOMPARE(persisted.value(QStringLiteral("shellBorderWidth")).toInteger(),
             1);
    QCOMPARE(persisted.value(QStringLiteral("shellBorderRadius")).toInteger(),
             15);
    QCOMPARE(
        persisted.value(QStringLiteral("syncHyprlandWindowBorders")).toBool(),
        true);
    QCOMPARE(persisted.value(QStringLiteral("shellInnerSpacing")).toInteger(),
             8);
    QCOMPARE(persisted.value(QStringLiteral("shellOuterSpacing")).toInteger(),
             12);
    QCOMPARE(
        persisted.value(QStringLiteral("syncHyprlandWindowSpacing")).toBool(),
        true);
    QCOMPARE(persisted.value(QStringLiteral("appearanceMode")).toString(),
             QStringLiteral("dark"));
    QCOMPARE(persisted.value(QStringLiteral("revision")).toString(),
             QStringLiteral("1"));
    QVERIFY(!persisted.contains(QStringLiteral("workspaceSwitcher")));
    QCOMPARE(persisted.value(QStringLiteral("appearanceAutomation"))
                 .toObject()
                 .value(QStringLiteral("source"))
                 .toString(),
             QStringLiteral("desktop"));
    QCOMPARE(persisted.value(QStringLiteral("nightLight"))
                 .toObject()
                 .value(QStringLiteral("nightTemperature"))
                 .toInteger(),
             4000);
    QCOMPARE(proxy.barHeight(), 60U);
    QCOMPARE(proxy.revision(), 1ULL);

    const auto changedBytes = readFile(activeFile_);
    const auto changedRecoveryBytes = readFile(recoveryFile_);
    const auto configDirectory = QFileInfo(activeFile_).absolutePath();
    const auto stateDirectory = QFileInfo(recoveryFile_).absolutePath();
    QVERIFY(blockDirectory(configDirectory));
    QVERIFY(blockDirectory(stateDirectory));
    auto idempotent = proxy.SetBarHeight(60);
    idempotent.waitForFinished();
    QVERIFY(restoreDirectory(stateDirectory));
    QVERIFY(restoreDirectory(configDirectory));
    QVERIFY2(!idempotent.isError(), qPrintable(idempotent.error().message()));
    QCOMPARE(idempotent.value(), 1ULL);
    QTest::qWait(50);
    QCOMPARE(signalCount_, 1);
    QCOMPARE(readFile(activeFile_), changedBytes);
    QCOMPARE(readFile(recoveryFile_), changedRecoveryBytes);

    auto reset = proxy.ResetBarHeight();
    reset.waitForFinished();
    QVERIFY2(!reset.isError(), qPrintable(reset.error().message()));
    QCOMPARE(reset.value(), 2ULL);
    QTRY_COMPARE_WITH_TIMEOUT(signalCount_, 2, 1000);
    QCOMPARE(proxy.barHeight(), 40U);
    QCOMPARE(proxy.revision(), 2ULL);

    const auto resetBytes = readFile(activeFile_);
    const auto resetRecoveryBytes = readFile(recoveryFile_);
    QVERIFY(blockDirectory(configDirectory));
    QVERIFY(blockDirectory(stateDirectory));
    auto repeatedReset = proxy.ResetBarHeight();
    repeatedReset.waitForFinished();
    QVERIFY(restoreDirectory(stateDirectory));
    QVERIFY(restoreDirectory(configDirectory));
    QVERIFY2(!repeatedReset.isError(),
             qPrintable(repeatedReset.error().message()));
    QCOMPARE(repeatedReset.value(), 2ULL);
    QTest::qWait(50);
    QCOMPARE(signalCount_, 2);
    QCOMPARE(readFile(activeFile_), resetBytes);
    QCOMPARE(readFile(recoveryFile_), resetRecoveryBytes);
  }

  void mutatesSharedBorderAsOnePersistentTuple() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY2(startService(directory.path()), qPrintable(processError_));
    QVERIFY(connectPropertiesSignal());

    OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
    const auto originalActive = readFile(activeFile_);
    const auto originalRecovery = readFile(recoveryFile_);

    for (const auto invalid : {
             std::pair{21U, 15U},
             std::pair{1U, 21U},
         }) {
      auto reply =
          proxy.SetSharedBorder(true, invalid.first, invalid.second, true);
      reply.waitForFinished();
      QVERIFY(reply.isError());
      QCOMPARE(
          reply.error().name(),
          QStringLiteral("org.hyprshelld.Config1.Error.InvalidSharedBorder"));
    }
    QTest::qWait(50);
    QCOMPARE(signalCount_, 0);
    QCOMPARE(proxy.revision(), 0ULL);
    QCOMPARE(readFile(activeFile_), originalActive);
    QCOMPARE(readFile(recoveryFile_), originalRecovery);

    auto changed = proxy.SetSharedBorder(false, 7U, 12U, false);
    changed.waitForFinished();
    QVERIFY2(!changed.isError(), qPrintable(changed.error().message()));
    QCOMPARE(changed.value(), 1ULL);
    QTRY_COMPARE_WITH_TIMEOUT(signalCount_, 1, 1000);
    QCOMPARE(lastChanged_.size(), 5);
    QCOMPARE(lastChanged_.value(QStringLiteral("ShellBorderEnabled")).toBool(),
             false);
    QCOMPARE(lastChanged_.value(QStringLiteral("ShellBorderWidth")).toUInt(),
             7U);
    QCOMPARE(lastChanged_.value(QStringLiteral("ShellBorderRadius")).toUInt(),
             12U);
    QCOMPARE(lastChanged_.value(QStringLiteral("SyncHyprlandWindowBorders"))
                 .toBool(),
             false);
    QCOMPARE(lastChanged_.value(QStringLiteral("Revision")).toULongLong(),
             1ULL);
    QVERIFY(!lastChanged_.contains(QStringLiteral("BarHeight")));
    QVERIFY(lastInvalidated_.isEmpty());

    const auto persisted = QJsonDocument::fromJson(bytesSeenAtSignal_).object();
    QCOMPARE(persisted.value(QStringLiteral("formatVersion")).toInteger(), 5);
    QCOMPARE(persisted.value(QStringLiteral("shellBorderEnabled")).toBool(),
             false);
    QCOMPARE(persisted.value(QStringLiteral("shellBorderWidth")).toInteger(),
             7);
    QCOMPARE(persisted.value(QStringLiteral("shellBorderRadius")).toInteger(),
             12);
    QCOMPARE(
        persisted.value(QStringLiteral("syncHyprlandWindowBorders")).toBool(),
        false);
    QCOMPARE(persisted.value(QStringLiteral("revision")).toString(),
             QStringLiteral("1"));
    QCOMPARE(proxy.shellBorderEnabled(), false);
    QCOMPARE(proxy.shellBorderWidth(), 7U);
    QCOMPARE(proxy.shellBorderRadius(), 12U);
    QCOMPARE(proxy.syncHyprlandWindowBorders(), false);

    const auto changedActive = readFile(activeFile_);
    const auto changedRecovery = readFile(recoveryFile_);
    const auto configDirectory = QFileInfo(activeFile_).absolutePath();
    const auto stateDirectory = QFileInfo(recoveryFile_).absolutePath();
    QVERIFY(blockDirectory(configDirectory));
    QVERIFY(blockDirectory(stateDirectory));
    auto idempotent = proxy.SetSharedBorder(false, 7U, 12U, false);
    idempotent.waitForFinished();
    QVERIFY(restoreDirectory(stateDirectory));
    QVERIFY(restoreDirectory(configDirectory));
    QVERIFY2(!idempotent.isError(), qPrintable(idempotent.error().message()));
    QCOMPARE(idempotent.value(), 1ULL);
    QTest::qWait(50);
    QCOMPARE(signalCount_, 1);
    QCOMPARE(readFile(activeFile_), changedActive);
    QCOMPARE(readFile(recoveryFile_), changedRecovery);

    auto reset = proxy.ResetSharedBorder();
    reset.waitForFinished();
    QVERIFY2(!reset.isError(), qPrintable(reset.error().message()));
    QCOMPARE(reset.value(), 2ULL);
    QTRY_COMPARE_WITH_TIMEOUT(signalCount_, 2, 1000);
    QCOMPARE(proxy.shellBorderEnabled(), true);
    QCOMPARE(proxy.shellBorderWidth(), 1U);
    QCOMPARE(proxy.shellBorderRadius(), 15U);
    QCOMPARE(proxy.syncHyprlandWindowBorders(), true);
    QCOMPARE(proxy.revision(), 2ULL);

    const auto resetActive = readFile(activeFile_);
    const auto resetRecovery = readFile(recoveryFile_);
    QVERIFY(blockDirectory(configDirectory));
    QVERIFY(blockDirectory(stateDirectory));
    auto repeatedReset = proxy.ResetSharedBorder();
    repeatedReset.waitForFinished();
    QVERIFY(restoreDirectory(stateDirectory));
    QVERIFY(restoreDirectory(configDirectory));
    QVERIFY2(!repeatedReset.isError(),
             qPrintable(repeatedReset.error().message()));
    QCOMPARE(repeatedReset.value(), 2ULL);
    QTest::qWait(50);
    QCOMPARE(signalCount_, 2);
    QCOMPARE(readFile(activeFile_), resetActive);
    QCOMPARE(readFile(recoveryFile_), resetRecovery);
  }

  void mutatesSharedSpacingAsOnePersistentTuple() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY2(startService(directory.path()), qPrintable(processError_));
    QVERIFY(connectPropertiesSignal());

    OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
    QCOMPARE(proxy.shellInnerSpacing(), 8U);
    QCOMPARE(proxy.shellOuterSpacing(), 12U);
    QCOMPARE(proxy.syncHyprlandWindowSpacing(), true);
    const auto originalActive = readFile(activeFile_);
    const auto originalRecovery = readFile(recoveryFile_);

    for (const auto invalid : {
             std::pair{33U, 12U},
             std::pair{8U, 33U},
         }) {
      auto reply = proxy.SetSharedSpacing(invalid.first, invalid.second, false);
      reply.waitForFinished();
      QVERIFY(reply.isError());
      QCOMPARE(
          reply.error().name(),
          QStringLiteral("org.hyprshelld.Config1.Error.InvalidSharedSpacing"));
    }
    QTest::qWait(50);
    QCOMPARE(signalCount_, 0);
    QCOMPARE(proxy.revision(), 0ULL);
    QCOMPARE(readFile(activeFile_), originalActive);
    QCOMPARE(readFile(recoveryFile_), originalRecovery);

    auto changed = proxy.SetSharedSpacing(0U, 32U, false);
    changed.waitForFinished();
    QVERIFY2(!changed.isError(), qPrintable(changed.error().message()));
    QCOMPARE(changed.value(), 1ULL);
    QTRY_COMPARE_WITH_TIMEOUT(signalCount_, 1, 1000);
    QCOMPARE(lastChanged_.size(), 4);
    QCOMPARE(lastChanged_.value(QStringLiteral("ShellInnerSpacing")).toUInt(),
             0U);
    QCOMPARE(lastChanged_.value(QStringLiteral("ShellOuterSpacing")).toUInt(),
             32U);
    QCOMPARE(lastChanged_.value(QStringLiteral("SyncHyprlandWindowSpacing"))
                 .toBool(),
             false);
    QCOMPARE(lastChanged_.value(QStringLiteral("Revision")).toULongLong(),
             1ULL);
    QVERIFY(!lastChanged_.contains(QStringLiteral("BarHeight")));
    QVERIFY(!lastChanged_.contains(QStringLiteral("ShellBorderWidth")));
    QVERIFY(lastInvalidated_.isEmpty());

    const auto persisted = QJsonDocument::fromJson(bytesSeenAtSignal_).object();
    QCOMPARE(persisted.value(QStringLiteral("formatVersion")).toInteger(), 5);
    QCOMPARE(persisted.value(QStringLiteral("shellInnerSpacing")).toInteger(),
             0);
    QCOMPARE(persisted.value(QStringLiteral("shellOuterSpacing")).toInteger(),
             32);
    QCOMPARE(
        persisted.value(QStringLiteral("syncHyprlandWindowSpacing")).toBool(),
        false);
    QCOMPARE(persisted.value(QStringLiteral("revision")).toString(),
             QStringLiteral("1"));
    QCOMPARE(proxy.shellInnerSpacing(), 0U);
    QCOMPARE(proxy.shellOuterSpacing(), 32U);
    QCOMPARE(proxy.syncHyprlandWindowSpacing(), false);

    const auto changedActive = readFile(activeFile_);
    const auto changedRecovery = readFile(recoveryFile_);
    const auto configDirectory = QFileInfo(activeFile_).absolutePath();
    const auto stateDirectory = QFileInfo(recoveryFile_).absolutePath();
    QVERIFY(blockDirectory(configDirectory));
    QVERIFY(blockDirectory(stateDirectory));
    auto idempotent = proxy.SetSharedSpacing(0U, 32U, false);
    idempotent.waitForFinished();
    QVERIFY(restoreDirectory(stateDirectory));
    QVERIFY(restoreDirectory(configDirectory));
    QVERIFY2(!idempotent.isError(), qPrintable(idempotent.error().message()));
    QCOMPARE(idempotent.value(), 1ULL);
    QTest::qWait(50);
    QCOMPARE(signalCount_, 1);
    QCOMPARE(readFile(activeFile_), changedActive);
    QCOMPARE(readFile(recoveryFile_), changedRecovery);

    auto reset = proxy.ResetSharedSpacing();
    reset.waitForFinished();
    QVERIFY2(!reset.isError(), qPrintable(reset.error().message()));
    QCOMPARE(reset.value(), 2ULL);
    QTRY_COMPARE_WITH_TIMEOUT(signalCount_, 2, 1000);
    QCOMPARE(proxy.shellInnerSpacing(), 8U);
    QCOMPARE(proxy.shellOuterSpacing(), 12U);
    QCOMPARE(proxy.syncHyprlandWindowSpacing(), true);
    QCOMPARE(proxy.revision(), 2ULL);

    const auto resetActive = readFile(activeFile_);
    const auto resetRecovery = readFile(recoveryFile_);
    QVERIFY(blockDirectory(configDirectory));
    QVERIFY(blockDirectory(stateDirectory));
    auto repeatedReset = proxy.ResetSharedSpacing();
    repeatedReset.waitForFinished();
    QVERIFY(restoreDirectory(stateDirectory));
    QVERIFY(restoreDirectory(configDirectory));
    QVERIFY2(!repeatedReset.isError(),
             qPrintable(repeatedReset.error().message()));
    QCOMPARE(repeatedReset.value(), 2ULL);
    QTest::qWait(50);
    QCOMPARE(signalCount_, 2);
    QCOMPARE(readFile(activeFile_), resetActive);
    QCOMPARE(readFile(recoveryFile_), resetRecovery);
  }

  void reportsPersistenceFailureWithoutChangingState() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY2(startService(directory.path()), qPrintable(processError_));
    QVERIFY(connectPropertiesSignal());

    OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
    const auto originalBytes = readFile(activeFile_);
    const auto configDirectory = QFileInfo(activeFile_).absolutePath();
    QVERIFY(blockDirectory(configDirectory));

    auto failed = proxy.SetBarHeight(64);
    failed.waitForFinished();

    auto failedBorder = proxy.SetSharedBorder(false, 7U, 12U, false);
    failedBorder.waitForFinished();

    auto failedSpacing = proxy.SetSharedSpacing(0U, 32U, false);
    failedSpacing.waitForFinished();

    auto failedAppearance = proxy.SetAppearanceMode(QStringLiteral("light"));
    failedAppearance.waitForFinished();

    QVERIFY(restoreDirectory(configDirectory));
    QVERIFY(failed.isError());
    QCOMPARE(failed.error().name(),
             QStringLiteral("org.hyprshelld.Config1.Error.PersistenceFailed"));
    QVERIFY(failedBorder.isError());
    QCOMPARE(failedBorder.error().name(),
             QStringLiteral("org.hyprshelld.Config1.Error.PersistenceFailed"));
    QVERIFY(failedSpacing.isError());
    QCOMPARE(failedSpacing.error().name(),
             QStringLiteral("org.hyprshelld.Config1.Error.PersistenceFailed"));
    QVERIFY(failedAppearance.isError());
    QCOMPARE(failedAppearance.error().name(),
             QStringLiteral("org.hyprshelld.Config1.Error.PersistenceFailed"));
    QTest::qWait(50);
    QCOMPARE(signalCount_, 0);
    QCOMPARE(proxy.barHeight(), 40U);
    QCOMPARE(proxy.shellBorderEnabled(), true);
    QCOMPARE(proxy.shellBorderWidth(), 1U);
    QCOMPARE(proxy.shellBorderRadius(), 15U);
    QCOMPARE(proxy.syncHyprlandWindowBorders(), true);
    QCOMPARE(proxy.shellInnerSpacing(), 8U);
    QCOMPARE(proxy.shellOuterSpacing(), 12U);
    QCOMPARE(proxy.syncHyprlandWindowSpacing(), true);
    QCOMPARE(proxy.appearanceMode(), QStringLiteral("dark"));
    QCOMPARE(proxy.revision(), 0ULL);
    QCOMPARE(readFile(activeFile_), originalBytes);
  }

  void acceptsInclusiveRangeBoundaries() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY2(startService(directory.path()), qPrintable(processError_));

    OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
    auto minimum = proxy.SetBarHeight(24);
    minimum.waitForFinished();
    QVERIFY2(!minimum.isError(), qPrintable(minimum.error().message()));
    QCOMPARE(minimum.value(), 1ULL);
    QCOMPARE(proxy.barHeight(), 24U);

    auto maximum = proxy.SetBarHeight(96);
    maximum.waitForFinished();
    QVERIFY2(!maximum.isError(), qPrintable(maximum.error().message()));
    QCOMPARE(maximum.value(), 2ULL);
    QCOMPARE(proxy.barHeight(), 96U);

    auto borderMinimumMaximum = proxy.SetSharedBorder(false, 0U, 20U, false);
    borderMinimumMaximum.waitForFinished();
    QVERIFY2(!borderMinimumMaximum.isError(),
             qPrintable(borderMinimumMaximum.error().message()));
    QCOMPARE(borderMinimumMaximum.value(), 3ULL);
    QCOMPARE(proxy.shellBorderWidth(), 0U);
    QCOMPARE(proxy.shellBorderRadius(), 20U);

    auto borderMaximumMinimum = proxy.SetSharedBorder(true, 20U, 0U, true);
    borderMaximumMinimum.waitForFinished();
    QVERIFY2(!borderMaximumMinimum.isError(),
             qPrintable(borderMaximumMinimum.error().message()));
    QCOMPARE(borderMaximumMinimum.value(), 4ULL);
    QCOMPARE(proxy.shellBorderWidth(), 20U);
    QCOMPARE(proxy.shellBorderRadius(), 0U);

    auto spacingMinimumMaximum = proxy.SetSharedSpacing(0U, 32U, false);
    spacingMinimumMaximum.waitForFinished();
    QVERIFY2(!spacingMinimumMaximum.isError(),
             qPrintable(spacingMinimumMaximum.error().message()));
    QCOMPARE(spacingMinimumMaximum.value(), 5ULL);
    QCOMPARE(proxy.shellInnerSpacing(), 0U);
    QCOMPARE(proxy.shellOuterSpacing(), 32U);

    auto spacingMaximumMinimum = proxy.SetSharedSpacing(32U, 0U, true);
    spacingMaximumMinimum.waitForFinished();
    QVERIFY2(!spacingMaximumMinimum.isError(),
             qPrintable(spacingMaximumMinimum.error().message()));
    QCOMPARE(spacingMaximumMinimum.value(), 6ULL);
    QCOMPARE(proxy.shellInnerSpacing(), 32U);
    QCOMPARE(proxy.shellOuterSpacing(), 0U);
    QCOMPARE(proxy.syncHyprlandWindowSpacing(), true);
  }

  void mutatesAppearanceModeWithStrictEnumValidation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY2(startService(directory.path()), qPrintable(processError_));
    QVERIFY(connectPropertiesSignal());

    OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
    QCOMPARE(proxy.appearanceMode(), QStringLiteral("dark"));
    QCOMPARE(proxy.revision(), 0ULL);
    const auto originalActive = readFile(activeFile_);
    const auto originalRecovery = readFile(recoveryFile_);

    for (const auto &invalid : {
             QString(),
             QStringLiteral("system"),
             QStringLiteral("Dark"),
         }) {
      auto reply = proxy.SetAppearanceMode(invalid);
      reply.waitForFinished();
      QVERIFY(reply.isError());
      QCOMPARE(
          reply.error().name(),
          QStringLiteral("org.hyprshelld.Config1.Error.InvalidAppearanceMode"));
    }
    QTest::qWait(50);
    QCOMPARE(signalCount_, 0);
    QCOMPARE(proxy.appearanceMode(), QStringLiteral("dark"));
    QCOMPARE(proxy.revision(), 0ULL);
    QCOMPARE(readFile(activeFile_), originalActive);
    QCOMPARE(readFile(recoveryFile_), originalRecovery);

    auto light = proxy.SetAppearanceMode(QStringLiteral("light"));
    light.waitForFinished();
    QVERIFY2(!light.isError(), qPrintable(light.error().message()));
    QCOMPARE(light.value(), 1ULL);
    QTRY_COMPARE_WITH_TIMEOUT(signalCount_, 1, 1000);
    QCOMPARE(lastChanged_.size(), 2);
    QCOMPARE(lastChanged_.value(QStringLiteral("AppearanceMode")).toString(),
             QStringLiteral("light"));
    QCOMPARE(lastChanged_.value(QStringLiteral("Revision")).toULongLong(),
             1ULL);
    QVERIFY(lastInvalidated_.isEmpty());
    QCOMPARE(proxy.appearanceMode(), QStringLiteral("light"));

    const auto persisted = QJsonDocument::fromJson(bytesSeenAtSignal_).object();
    QCOMPARE(persisted.value(QStringLiteral("formatVersion")).toInteger(), 5);
    QCOMPARE(persisted.value(QStringLiteral("appearanceMode")).toString(),
             QStringLiteral("light"));

    const auto lightActive = readFile(activeFile_);
    const auto lightRecovery = readFile(recoveryFile_);
    const auto configDirectory = QFileInfo(activeFile_).absolutePath();
    const auto stateDirectory = QFileInfo(recoveryFile_).absolutePath();
    QVERIFY(blockDirectory(configDirectory));
    QVERIFY(blockDirectory(stateDirectory));
    auto idempotent = proxy.SetAppearanceMode(QStringLiteral("light"));
    idempotent.waitForFinished();
    QVERIFY(restoreDirectory(stateDirectory));
    QVERIFY(restoreDirectory(configDirectory));
    QVERIFY2(!idempotent.isError(), qPrintable(idempotent.error().message()));
    QCOMPARE(idempotent.value(), 1ULL);
    QTest::qWait(50);
    QCOMPARE(signalCount_, 1);
    QCOMPARE(readFile(activeFile_), lightActive);
    QCOMPARE(readFile(recoveryFile_), lightRecovery);

    auto automatic = proxy.SetAppearanceMode(QStringLiteral("automatic"));
    automatic.waitForFinished();
    QVERIFY2(!automatic.isError(), qPrintable(automatic.error().message()));
    QCOMPARE(automatic.value(), 2ULL);
    QTRY_COMPARE_WITH_TIMEOUT(signalCount_, 2, 1000);
    QCOMPARE(proxy.appearanceMode(), QStringLiteral("automatic"));

    auto reset = proxy.ResetAppearanceMode();
    reset.waitForFinished();
    QVERIFY2(!reset.isError(), qPrintable(reset.error().message()));
    QCOMPARE(reset.value(), 3ULL);
    QTRY_COMPARE_WITH_TIMEOUT(signalCount_, 3, 1000);
    QCOMPARE(proxy.appearanceMode(), QStringLiteral("dark"));

    const auto resetActive = readFile(activeFile_);
    const auto resetRecovery = readFile(recoveryFile_);
    QVERIFY(blockDirectory(configDirectory));
    QVERIFY(blockDirectory(stateDirectory));
    auto repeatedReset = proxy.ResetAppearanceMode();
    repeatedReset.waitForFinished();
    QVERIFY(restoreDirectory(stateDirectory));
    QVERIFY(restoreDirectory(configDirectory));
    QVERIFY2(!repeatedReset.isError(),
             qPrintable(repeatedReset.error().message()));
    QCOMPARE(repeatedReset.value(), 3ULL);
    QTest::qWait(50);
    QCOMPARE(signalCount_, 3);
    QCOMPARE(readFile(activeFile_), resetActive);
    QCOMPARE(readFile(recoveryFile_), resetRecovery);
  }

  void mutatesAppearanceAutomationAndNightLightAsAtomicTuples() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY2(startService(directory.path()), qPrintable(processError_));
    QVERIFY(connectPropertiesSignal());

    OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
    QCOMPARE(proxy.revision(), 0ULL);
    const auto originalActive = readFile(activeFile_);
    const auto originalRecovery = readFile(recoveryFile_);

    const auto expectInvalidAppearance =
        [&proxy](const QString &source, const QString &mode,
                 const uint darkStart, const uint lightStart,
                 const QString &locationSource, const double latitude,
                 const double longitude) {
          auto reply = proxy.SetAppearanceAutomation(source, mode, darkStart,
                                                     lightStart, locationSource,
                                                     true, latitude, longitude);
          reply.waitForFinished();
          QVERIFY(reply.isError());
          QCOMPARE(reply.error().name(),
                   QStringLiteral("org.hyprshelld.Config1.Error."
                                  "InvalidAppearanceAutomation"));
        };
    expectInvalidAppearance(QStringLiteral("system"), QStringLiteral("time"),
                            18U * 60U, 6U * 60U, QStringLiteral("manual"), 0.0,
                            0.0);
    expectInvalidAppearance(QStringLiteral("schedule"), QStringLiteral("solar"),
                            18U * 60U, 6U * 60U, QStringLiteral("manual"), 0.0,
                            0.0);
    expectInvalidAppearance(QStringLiteral("schedule"), QStringLiteral("time"),
                            1440U, 6U * 60U, QStringLiteral("manual"), 0.0,
                            0.0);
    expectInvalidAppearance(QStringLiteral("schedule"), QStringLiteral("time"),
                            6U * 60U, 6U * 60U, QStringLiteral("manual"), 0.0,
                            0.0);
    expectInvalidAppearance(QStringLiteral("schedule"),
                            QStringLiteral("location"), 18U * 60U, 6U * 60U,
                            QStringLiteral("network"), 0.0, 0.0);
    expectInvalidAppearance(QStringLiteral("schedule"),
                            QStringLiteral("location"), 18U * 60U, 6U * 60U,
                            QStringLiteral("manual"), 91.0, 0.0);
    QTest::qWait(50);
    QCOMPARE(signalCount_, 0);
    QCOMPARE(proxy.revision(), 0ULL);
    QCOMPARE(readFile(activeFile_), originalActive);
    QCOMPARE(readFile(recoveryFile_), originalRecovery);

    auto appearance = proxy.SetAppearanceAutomation(
        QStringLiteral("schedule"), QStringLiteral("location"), 19U * 60U,
        6U * 60U + 30U, QStringLiteral("manual"), true, 51.5074, -0.1278);
    appearance.waitForFinished();
    QVERIFY2(!appearance.isError(), qPrintable(appearance.error().message()));
    QCOMPARE(appearance.value(), 1ULL);
    QTRY_VERIFY_WITH_TIMEOUT(signalCount_ >= 1, 1000);
    QCOMPARE(proxy.appearanceAutomationSource(), QStringLiteral("schedule"));
    QCOMPARE(proxy.appearanceScheduleMode(), QStringLiteral("location"));
    QCOMPARE(proxy.appearanceDarkStartMinute(), 19U * 60U);
    QCOMPARE(proxy.appearanceLightStartMinute(), 6U * 60U + 30U);
    QCOMPARE(proxy.appearanceLocationSource(), QStringLiteral("manual"));
    QCOMPARE(proxy.appearanceHasLocation(), true);
    QCOMPARE(proxy.appearanceLatitude(), 51.5074);
    QCOMPARE(proxy.appearanceLongitude(), -0.1278);
    QCOMPARE(proxy.revision(), 1ULL);

    auto appearanceChangeFound = false;
    auto runtimeWithoutRevisionFound = false;
    for (const auto &change : std::as_const(changes_)) {
      if (change.value(QStringLiteral("Revision")).toULongLong() == 1ULL &&
          change.contains(QStringLiteral("AppearanceAutomationSource"))) {
        QCOMPARE(change.size(), 9);
        QCOMPARE(change.value(QStringLiteral("AppearanceAutomationSource"))
                     .toString(),
                 QStringLiteral("schedule"));
        QCOMPARE(change.value(QStringLiteral("AppearanceLatitude")).toDouble(),
                 51.5074);
        appearanceChangeFound = true;
      }
      if (change.contains(QStringLiteral("ScheduledAppearanceMode")) &&
          !change.contains(QStringLiteral("Revision"))) {
        runtimeWithoutRevisionFound = true;
      }
    }
    QVERIFY(appearanceChangeFound);
    QTRY_VERIFY_WITH_TIMEOUT(runtimeWithoutRevisionFound || signalCount_ >= 2,
                             1000);
    if (!runtimeWithoutRevisionFound) {
      for (const auto &change : std::as_const(changes_)) {
        runtimeWithoutRevisionFound =
            runtimeWithoutRevisionFound ||
            (change.contains(QStringLiteral("ScheduledAppearanceMode")) &&
             !change.contains(QStringLiteral("Revision")));
      }
    }
    QVERIFY(runtimeWithoutRevisionFound);

    const auto appearanceObject =
        QJsonDocument::fromJson(readFile(activeFile_))
            .object()
            .value(QStringLiteral("appearanceAutomation"))
            .toObject();
    QCOMPARE(appearanceObject.value(QStringLiteral("source")).toString(),
             QStringLiteral("schedule"));
    const auto appearanceSchedule =
        appearanceObject.value(QStringLiteral("schedule")).toObject();
    QCOMPARE(appearanceSchedule.value(QStringLiteral("mode")).toString(),
             QStringLiteral("location"));
    QCOMPARE(
        appearanceSchedule.value(QStringLiteral("darkStartMinute")).toInteger(),
        19 * 60);
    QCOMPARE(appearanceSchedule.value(QStringLiteral("hasLocation")).toBool(),
             true);

    const auto beforeInvalidNightLight = readFile(activeFile_);
    const auto expectInvalidNightLight = [&proxy](const QString &mode,
                                                  const uint darkStart,
                                                  const uint lightStart,
                                                  const QString &locationSource,
                                                  const double latitude,
                                                  const uint nightTemperature,
                                                  const uint dayTemperature) {
      auto reply = proxy.SetNightLightSettings(
          false, true, mode, darkStart, lightStart, locationSource, true,
          latitude, 0.0, nightTemperature, dayTemperature, true);
      reply.waitForFinished();
      QVERIFY(reply.isError());
      QCOMPARE(reply.error().name(),
               QStringLiteral(
                   "org.hyprshelld.Config1.Error.InvalidNightLightSettings"));
    };
    expectInvalidNightLight(QStringLiteral("solar"), 20U * 60U, 6U * 60U,
                            QStringLiteral("manual"), 0.0, 4000U, 6500U);
    expectInvalidNightLight(QStringLiteral("time"), 20U * 60U, 20U * 60U,
                            QStringLiteral("manual"), 0.0, 4000U, 6500U);
    expectInvalidNightLight(QStringLiteral("location"), 20U * 60U, 6U * 60U,
                            QStringLiteral("network"), 0.0, 4000U, 6500U);
    expectInvalidNightLight(QStringLiteral("location"), 20U * 60U, 6U * 60U,
                            QStringLiteral("manual"), -91.0, 4000U, 6500U);
    expectInvalidNightLight(QStringLiteral("time"), 20U * 60U, 6U * 60U,
                            QStringLiteral("manual"), 0.0, 2499U, 6500U);
    expectInvalidNightLight(QStringLiteral("time"), 20U * 60U, 6U * 60U,
                            QStringLiteral("manual"), 0.0, 6001U, 6500U);
    QTest::qWait(50);
    QCOMPARE(proxy.revision(), 1ULL);
    QCOMPARE(readFile(activeFile_), beforeInvalidNightLight);

    resetSignalCapture();
    auto nightLight = proxy.SetNightLightSettings(
        false, false, QStringLiteral("location"), 21U * 60U, 7U * 60U,
        QStringLiteral("manual"), true, 35.6762, 139.6503, 3300U, 6800U, false);
    nightLight.waitForFinished();
    QVERIFY2(!nightLight.isError(), qPrintable(nightLight.error().message()));
    QCOMPARE(nightLight.value(), 2ULL);
    QTRY_VERIFY_WITH_TIMEOUT(signalCount_ >= 1, 1000);
    QCOMPARE(proxy.nightLightEnabled(), false);
    QCOMPARE(proxy.nightLightAutomatic(), false);
    QCOMPARE(proxy.nightLightScheduleMode(), QStringLiteral("location"));
    QCOMPARE(proxy.nightLightDarkStartMinute(), 21U * 60U);
    QCOMPARE(proxy.nightLightLightStartMinute(), 7U * 60U);
    QCOMPARE(proxy.nightLightLocationSource(), QStringLiteral("manual"));
    QCOMPARE(proxy.nightLightHasLocation(), true);
    QCOMPARE(proxy.nightLightLatitude(), 35.6762);
    QCOMPARE(proxy.nightLightLongitude(), 139.6503);
    QCOMPARE(proxy.nightLightTemperature(), 3300U);
    QCOMPARE(proxy.nightLightDayTemperature(), 6800U);
    QCOMPARE(proxy.nightLightGradual(), false);
    QCOMPARE(proxy.revision(), 2ULL);

    auto nightLightChangeFound = false;
    for (const auto &change : std::as_const(changes_)) {
      if (change.value(QStringLiteral("Revision")).toULongLong() == 2ULL &&
          change.contains(QStringLiteral("NightLightEnabled"))) {
        QCOMPARE(change.size(), 13);
        QCOMPARE(change.value(QStringLiteral("NightLightTemperature")).toUInt(),
                 3300U);
        QCOMPARE(change.value(QStringLiteral("NightLightLongitude")).toDouble(),
                 139.6503);
        nightLightChangeFound = true;
      }
    }
    QVERIFY(nightLightChangeFound);

    const auto nightLightObject = QJsonDocument::fromJson(readFile(activeFile_))
                                      .object()
                                      .value(QStringLiteral("nightLight"))
                                      .toObject();
    QCOMPARE(nightLightObject.value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(nightLightObject.value(QStringLiteral("automatic")).toBool(),
             false);
    QCOMPARE(
        nightLightObject.value(QStringLiteral("nightTemperature")).toInteger(),
        3300);
    QCOMPARE(
        nightLightObject.value(QStringLiteral("dayTemperature")).toInteger(),
        6800);

    stopService();
    QVERIFY2(startService(directory.path()), qPrintable(processError_));
    OrgHyprshelldConfig1Interface restarted(busName, objectPath, bus_);
    QCOMPARE(restarted.revision(), 2ULL);
    QCOMPARE(restarted.appearanceAutomationSource(),
             QStringLiteral("schedule"));
    QCOMPARE(restarted.appearanceLatitude(), 51.5074);
    QCOMPARE(restarted.nightLightAutomatic(), false);
    QCOMPARE(restarted.nightLightTemperature(), 3300U);
    QCOMPARE(restarted.nightLightLongitude(), 139.6503);

    auto resetAppearance = restarted.ResetAppearanceAutomation();
    resetAppearance.waitForFinished();
    QVERIFY2(!resetAppearance.isError(),
             qPrintable(resetAppearance.error().message()));
    QCOMPARE(resetAppearance.value(), 3ULL);
    QCOMPARE(restarted.appearanceAutomationSource(), QStringLiteral("desktop"));
    QCOMPARE(restarted.appearanceScheduleMode(), QStringLiteral("time"));
    QCOMPARE(restarted.appearanceDarkStartMinute(), 18U * 60U);
    QCOMPARE(restarted.appearanceLightStartMinute(), 6U * 60U);
    QCOMPARE(restarted.appearanceHasLocation(), false);

    auto resetNightLight = restarted.ResetNightLightSettings();
    resetNightLight.waitForFinished();
    QVERIFY2(!resetNightLight.isError(),
             qPrintable(resetNightLight.error().message()));
    QCOMPARE(resetNightLight.value(), 4ULL);
    QCOMPARE(restarted.nightLightEnabled(), false);
    QCOMPARE(restarted.nightLightAutomatic(), true);
    QCOMPARE(restarted.nightLightScheduleMode(), QStringLiteral("time"));
    QCOMPARE(restarted.nightLightDarkStartMinute(), 20U * 60U);
    QCOMPARE(restarted.nightLightLightStartMinute(), 6U * 60U);
    QCOMPARE(restarted.nightLightHasLocation(), false);
    QCOMPARE(restarted.nightLightTemperature(), 4000U);
    QCOMPARE(restarted.nightLightDayTemperature(), 6500U);
    QCOMPARE(restarted.nightLightGradual(), true);
    QCOMPARE(restarted.revision(), 4ULL);
  }

  void reportsRestartRecoveryStates() {
    QTemporaryDir recoveredDirectory;
    QVERIFY(recoveredDirectory.isValid());
    QVERIFY2(startService(recoveredDirectory.path()),
             qPrintable(processError_));

    {
      OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
      auto changed = proxy.SetBarHeight(72);
      changed.waitForFinished();
      QVERIFY2(!changed.isError(), qPrintable(changed.error().message()));
      QCOMPARE(changed.value(), 1ULL);
    }

    stopService();
    QVERIFY2(startService(recoveredDirectory.path()),
             qPrintable(processError_));
    {
      OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
      QCOMPARE(proxy.recoveryState(), QStringLiteral("normal"));
      QCOMPARE(proxy.barHeight(), 72U);
      QCOMPARE(proxy.revision(), 1ULL);
    }

    stopService();
    QVERIFY(writeFile(activeFile_, "not json\n"));
    QVERIFY2(startService(recoveredDirectory.path()),
             qPrintable(processError_));
    {
      OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
      QCOMPARE(proxy.recoveryState(), QStringLiteral("recovered"));
      QCOMPARE(proxy.barHeight(), 72U);
      QCOMPARE(proxy.revision(), 1ULL);
    }

    stopService();

    QTemporaryDir defaultedDirectory;
    QVERIFY(defaultedDirectory.isValid());
    const auto defaultedActive =
        defaultedDirectory.path() +
        QStringLiteral("/config/hyprshelld/settings.json");
    QVERIFY(writeFile(defaultedActive, "not json\n"));
    QVERIFY2(startService(defaultedDirectory.path()),
             qPrintable(processError_));

    OrgHyprshelldConfig1Interface defaulted(busName, objectPath, bus_);
    QCOMPARE(defaulted.recoveryState(), QStringLiteral("defaulted"));
    QCOMPARE(defaulted.barHeight(), 40U);
    QCOMPARE(defaulted.revision(), 0ULL);
  }

  void publishesMigratedFormatOneVisualsWithoutAdvancingRevision() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto active =
        directory.path() + QStringLiteral("/config/hyprshelld/settings.json");
    QVERIFY(writeFile(
        active,
        QByteArrayLiteral(
            "{\"formatVersion\":1,\"revision\":\"9\",\"barHeight\":32}\n")));

    QVERIFY2(startService(directory.path()), qPrintable(processError_));
    OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
    QCOMPARE(proxy.barHeight(), 32U);
    QCOMPARE(proxy.shellBorderEnabled(), true);
    QCOMPARE(proxy.shellBorderWidth(), 1U);
    QCOMPARE(proxy.shellBorderRadius(), 12U);
    QCOMPARE(proxy.syncHyprlandWindowBorders(), false);
    QCOMPARE(proxy.shellInnerSpacing(), 8U);
    QCOMPARE(proxy.shellOuterSpacing(), 12U);
    QCOMPARE(proxy.syncHyprlandWindowSpacing(), false);
    QCOMPARE(proxy.appearanceMode(), QStringLiteral("dark"));
    QCOMPARE(proxy.revision(), 9ULL);
    QCOMPARE(proxy.recoveryState(), QStringLiteral("normal"));

    QCOMPARE(readFile(activeFile_), readFile(recoveryFile_));
    const auto migrated =
        QJsonDocument::fromJson(readFile(activeFile_)).object();
    QCOMPARE(migrated.value(QStringLiteral("formatVersion")).toInteger(), 5);
    QCOMPARE(migrated.value(QStringLiteral("revision")).toString(),
             QStringLiteral("9"));
    QCOMPARE(migrated.value(QStringLiteral("shellBorderRadius")).toInteger(),
             12);
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
  }

  void rejectsMutationsAtMaximumRevisionWithoutChangingState() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto active =
        directory.path() + QStringLiteral("/config/hyprshelld/settings.json");
    QVERIFY(writeFile(
        active,
        QByteArrayLiteral(
            "{\"formatVersion\":3,\"revision\":\"18446744073709551615\","
            "\"barHeight\":40,\"shellBorderEnabled\":true,"
            "\"shellBorderWidth\":1,\"shellBorderRadius\":15,"
            "\"syncHyprlandWindowBorders\":true,\"shellInnerSpacing\":8,"
            "\"shellOuterSpacing\":12,\"syncHyprlandWindowSpacing\":true}\n")));

    QVERIFY2(startService(directory.path()), qPrintable(processError_));
    QVERIFY(connectPropertiesSignal());
    OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
    QCOMPARE(proxy.revision(), std::numeric_limits<qulonglong>::max());
    const auto original = readFile(activeFile_);

    auto idempotent = proxy.SetSharedSpacing(8U, 12U, true);
    idempotent.waitForFinished();
    QVERIFY2(!idempotent.isError(), qPrintable(idempotent.error().message()));
    QCOMPARE(idempotent.value(), std::numeric_limits<qulonglong>::max());

    auto idempotentAppearance = proxy.SetAppearanceMode(QStringLiteral("dark"));
    idempotentAppearance.waitForFinished();
    QVERIFY2(!idempotentAppearance.isError(),
             qPrintable(idempotentAppearance.error().message()));
    QCOMPARE(idempotentAppearance.value(),
             std::numeric_limits<qulonglong>::max());

    auto failed = proxy.SetSharedSpacing(0U, 32U, false);
    failed.waitForFinished();
    QVERIFY(failed.isError());
    QCOMPARE(failed.error().name(),
             QStringLiteral("org.hyprshelld.Config1.Error.PersistenceFailed"));
    QVERIFY(failed.error().message().contains(QStringLiteral("exhausted")));

    auto failedAppearance = proxy.SetAppearanceMode(QStringLiteral("light"));
    failedAppearance.waitForFinished();
    QVERIFY(failedAppearance.isError());
    QCOMPARE(failedAppearance.error().name(),
             QStringLiteral("org.hyprshelld.Config1.Error.PersistenceFailed"));
    QVERIFY(failedAppearance.error().message().contains(
        QStringLiteral("exhausted")));
    QTest::qWait(50);
    QCOMPARE(signalCount_, 0);
    QCOMPARE(proxy.shellInnerSpacing(), 8U);
    QCOMPARE(proxy.shellOuterSpacing(), 12U);
    QCOMPARE(proxy.syncHyprlandWindowSpacing(), true);
    QCOMPARE(proxy.appearanceMode(), QStringLiteral("dark"));
    QCOMPARE(proxy.revision(), std::numeric_limits<qulonglong>::max());
    QCOMPARE(readFile(activeFile_), original);
  }

  void propertiesChanged(const QString &changedInterface,
                         const QVariantMap &changed,
                         const QStringList &invalidated) {
    if (changedInterface != interfaceName) {
      return;
    }

    ++signalCount_;
    lastChanged_ = changed;
    changes_.append(changed);
    lastInvalidated_ = invalidated;
    bytesSeenAtSignal_ = readFile(activeFile_);
  }

private:
  bool startService(const QString &root) {
    stopService();
    resetSignalCapture();
    processError_.clear();

    const auto configHome = root + QStringLiteral("/config");
    const auto stateHome = root + QStringLiteral("/state");
    activeFile_ = configHome + QStringLiteral("/hyprshelld/settings.json");
    recoveryFile_ =
        stateHome + QStringLiteral("/hyprshelld/settings.last-good.json");

    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), configHome);
    environment.insert(QStringLiteral("XDG_STATE_HOME"), stateHome);

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

  void stopService() {
    bus_.disconnect(busName, objectPath, propertiesInterface,
                    QStringLiteral("PropertiesChanged"), this,
                    SLOT(propertiesChanged(QString, QVariantMap, QStringList)));

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

  bool connectPropertiesSignal() {
    return bus_.connect(
        busName, objectPath, propertiesInterface,
        QStringLiteral("PropertiesChanged"), this,
        SLOT(propertiesChanged(QString, QVariantMap, QStringList)));
  }

  void resetSignalCapture() {
    signalCount_ = 0;
    lastChanged_.clear();
    changes_.clear();
    lastInvalidated_.clear();
    bytesSeenAtSignal_.clear();
  }

  QString executable_;
  QString contractPath_;
  QString activeFile_;
  QString recoveryFile_;
  QString processError_;
  QProcess process_;
  QDBusConnection bus_ = QDBusConnection::sessionBus();
  int signalCount_ = 0;
  QVariantMap lastChanged_;
  QList<QVariantMap> changes_;
  QStringList lastInvalidated_;
  QByteArray bytesSeenAtSignal_;
};

QTEST_GUILESS_MAIN(ConfigdDbusTest)

#include "configd_dbus_test.moc"
