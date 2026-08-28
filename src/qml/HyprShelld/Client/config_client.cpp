#include "config_client.h"

#include "config/config_values.h"
#include "config1_interface.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QMetaType>
#include <QVariant>

#include <cmath>
#include <optional>
#include <utility>

namespace HyprShelld {
namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.Config1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Config1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Config1");
const QString propertiesInterface =
    QStringLiteral("org.freedesktop.DBus.Properties");

} // namespace

ConfigClient::ConfigClient(QObject *parent)
    : ConfigClient(QDBusConnection::sessionBus(), parent) {}

ConfigClient::ConfigClient(QDBusConnection connection, QObject *parent)
    : QObject(parent), connection_(std::move(connection)),
      barHeight_(ConfigValues::defaultBarHeight),
      shellBorderEnabled_(ConfigValues::defaultShellBorderEnabled),
      shellBorderWidth_(ConfigValues::defaultShellBorderWidth),
      shellBorderRadius_(ConfigValues::defaultShellBorderRadius),
      syncHyprlandWindowBorders_(
          ConfigValues::defaultSyncHyprlandWindowBorders),
      shellInnerSpacing_(ConfigValues::defaultShellInnerSpacing),
      shellOuterSpacing_(ConfigValues::defaultShellOuterSpacing),
      syncHyprlandWindowSpacing_(
          ConfigValues::defaultSyncHyprlandWindowSpacing),
      appearanceMode_(ConfigValues::defaultAppearanceMode),
      appearanceAutomationSource_(
          ConfigValues::defaultAppearanceAutomationSource),
      appearanceScheduleMode_(ConfigValues::defaultScheduleMode),
      appearanceDarkStartMinute_(
          ConfigValues::defaultAppearanceDarkStartMinute),
      appearanceLightStartMinute_(
          ConfigValues::defaultAppearanceLightStartMinute),
      appearanceLocationSource_(ConfigValues::defaultLocationSource),
      scheduledAppearanceMode_(QStringLiteral("system")),
      appearanceAutomationStatus_(QStringLiteral("desktop")),
      nightLightScheduleMode_(ConfigValues::defaultScheduleMode),
      nightLightDarkStartMinute_(
          ConfigValues::defaultNightLightDarkStartMinute),
      nightLightLightStartMinute_(
          ConfigValues::defaultNightLightLightStartMinute),
      nightLightLocationSource_(ConfigValues::defaultLocationSource),
      nightLightTemperature_(ConfigValues::defaultNightLightTemperature),
      nightLightDayTemperature_(ConfigValues::defaultNightLightDayTemperature),
      nightLightRuntimeState_(QStringLiteral("disabled")),
      nightLightStatus_(QStringLiteral("disabled")) {
  interface_ = new OrgHyprshelldConfig1Interface(serviceName, objectPath,
                                                 connection_, this);

  serviceWatcher_ = new QDBusServiceWatcher(
      serviceName, connection_, QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(serviceWatcher_, &QDBusServiceWatcher::serviceOwnerChanged, this,
          &ConfigClient::serviceOwnerChanged);

  connection_.connect(
      serviceName, objectPath, propertiesInterface,
      QStringLiteral("PropertiesChanged"), this,
      SLOT(propertiesChanged(QString, QVariantMap, QStringList)));

  refresh();
}

bool ConfigClient::available() const { return available_; }

bool ConfigClient::busy() const { return pendingOperations_ > 0; }

uint ConfigClient::barHeight() const { return barHeight_; }

bool ConfigClient::shellBorderEnabled() const { return shellBorderEnabled_; }

uint ConfigClient::shellBorderWidth() const { return shellBorderWidth_; }

uint ConfigClient::shellBorderRadius() const { return shellBorderRadius_; }

bool ConfigClient::syncHyprlandWindowBorders() const {
  return syncHyprlandWindowBorders_;
}

uint ConfigClient::shellInnerSpacing() const { return shellInnerSpacing_; }

uint ConfigClient::shellOuterSpacing() const { return shellOuterSpacing_; }

bool ConfigClient::syncHyprlandWindowSpacing() const {
  return syncHyprlandWindowSpacing_;
}

QString ConfigClient::appearanceMode() const { return appearanceMode_; }

QString ConfigClient::appearanceAutomationSource() const {
  return appearanceAutomationSource_;
}
QString ConfigClient::appearanceScheduleMode() const {
  return appearanceScheduleMode_;
}
uint ConfigClient::appearanceDarkStartMinute() const {
  return appearanceDarkStartMinute_;
}
uint ConfigClient::appearanceLightStartMinute() const {
  return appearanceLightStartMinute_;
}
QString ConfigClient::appearanceLocationSource() const {
  return appearanceLocationSource_;
}
bool ConfigClient::appearanceHasLocation() const {
  return appearanceHasLocation_;
}
double ConfigClient::appearanceLatitude() const { return appearanceLatitude_; }
double ConfigClient::appearanceLongitude() const {
  return appearanceLongitude_;
}
QString ConfigClient::scheduledAppearanceMode() const {
  return scheduledAppearanceMode_;
}
QString ConfigClient::appearanceNextTransition() const {
  return appearanceNextTransition_;
}
QString ConfigClient::appearanceSunrise() const { return appearanceSunrise_; }
QString ConfigClient::appearanceSunset() const { return appearanceSunset_; }
QString ConfigClient::appearanceAutomationStatus() const {
  return appearanceAutomationStatus_;
}
bool ConfigClient::nightLightEnabled() const { return nightLightEnabled_; }
bool ConfigClient::nightLightAutomatic() const { return nightLightAutomatic_; }
QString ConfigClient::nightLightScheduleMode() const {
  return nightLightScheduleMode_;
}
uint ConfigClient::nightLightDarkStartMinute() const {
  return nightLightDarkStartMinute_;
}
uint ConfigClient::nightLightLightStartMinute() const {
  return nightLightLightStartMinute_;
}
QString ConfigClient::nightLightLocationSource() const {
  return nightLightLocationSource_;
}
bool ConfigClient::nightLightHasLocation() const {
  return nightLightHasLocation_;
}
double ConfigClient::nightLightLatitude() const { return nightLightLatitude_; }
double ConfigClient::nightLightLongitude() const {
  return nightLightLongitude_;
}
uint ConfigClient::nightLightTemperature() const {
  return nightLightTemperature_;
}
uint ConfigClient::nightLightDayTemperature() const {
  return nightLightDayTemperature_;
}
bool ConfigClient::nightLightGradual() const { return nightLightGradual_; }
bool ConfigClient::hyprsunsetAvailable() const { return hyprsunsetAvailable_; }
QString ConfigClient::nightLightRuntimeState() const {
  return nightLightRuntimeState_;
}
uint ConfigClient::nightLightCurrentTemperature() const {
  return nightLightCurrentTemperature_;
}
QString ConfigClient::nightLightNextTransition() const {
  return nightLightNextTransition_;
}
QString ConfigClient::nightLightSunrise() const { return nightLightSunrise_; }
QString ConfigClient::nightLightSunset() const { return nightLightSunset_; }
QString ConfigClient::nightLightStatus() const { return nightLightStatus_; }

qulonglong ConfigClient::revision() const { return revision_; }

QString ConfigClient::revisionToken() const {
  return QString::number(revision_);
}

QString ConfigClient::recoveryState() const { return recoveryState_; }

uint ConfigClient::minimumBarHeight() const {
  return ConfigValues::minimumBarHeight;
}

uint ConfigClient::maximumBarHeight() const {
  return ConfigValues::maximumBarHeight;
}

uint ConfigClient::defaultBarHeight() const {
  return ConfigValues::defaultBarHeight;
}

bool ConfigClient::defaultShellBorderEnabled() const {
  return ConfigValues::defaultShellBorderEnabled;
}

uint ConfigClient::minimumShellBorderWidth() const {
  return ConfigValues::minimumShellBorderWidth;
}

uint ConfigClient::maximumShellBorderWidth() const {
  return ConfigValues::maximumShellBorderWidth;
}

uint ConfigClient::defaultShellBorderWidth() const {
  return ConfigValues::defaultShellBorderWidth;
}

uint ConfigClient::minimumShellBorderRadius() const {
  return ConfigValues::minimumShellBorderRadius;
}

uint ConfigClient::maximumShellBorderRadius() const {
  return ConfigValues::maximumShellBorderRadius;
}

uint ConfigClient::defaultShellBorderRadius() const {
  return ConfigValues::defaultShellBorderRadius;
}

bool ConfigClient::defaultSyncHyprlandWindowBorders() const {
  return ConfigValues::defaultSyncHyprlandWindowBorders;
}

uint ConfigClient::minimumShellSpacing() const {
  return ConfigValues::minimumShellSpacing;
}

uint ConfigClient::maximumShellSpacing() const {
  return ConfigValues::maximumShellSpacing;
}

uint ConfigClient::defaultShellInnerSpacing() const {
  return ConfigValues::defaultShellInnerSpacing;
}

uint ConfigClient::defaultShellOuterSpacing() const {
  return ConfigValues::defaultShellOuterSpacing;
}

bool ConfigClient::defaultSyncHyprlandWindowSpacing() const {
  return ConfigValues::defaultSyncHyprlandWindowSpacing;
}

QString ConfigClient::defaultAppearanceMode() const {
  return ConfigValues::defaultAppearanceMode;
}

QString ConfigClient::lastErrorName() const { return lastErrorName_; }

QString ConfigClient::lastErrorOperation() const { return lastErrorOperation_; }

QString ConfigClient::lastErrorMessage() const { return lastErrorMessage_; }

void ConfigClient::setBarHeight(uint height) {
  beginMutation(interface_->SetBarHeight(height), QStringLiteral("bar-height"));
}

void ConfigClient::resetBarHeight() {
  beginMutation(interface_->ResetBarHeight(), QStringLiteral("bar-height"));
}

void ConfigClient::setSharedBorder(const bool enabled, const uint width,
                                   const uint radius,
                                   const bool syncHyprlandWindowBorders) {
  beginMutation(interface_->SetSharedBorder(enabled, width, radius,
                                            syncHyprlandWindowBorders),
                QStringLiteral("shared-border"));
}

void ConfigClient::resetSharedBorder() {
  beginMutation(interface_->ResetSharedBorder(),
                QStringLiteral("shared-border"));
}

void ConfigClient::setSharedSpacing(const uint inner, const uint outer,
                                    const bool syncHyprlandWindowSpacing) {
  beginMutation(
      interface_->SetSharedSpacing(inner, outer, syncHyprlandWindowSpacing),
      QStringLiteral("shared-spacing"));
}

void ConfigClient::resetSharedSpacing() {
  beginMutation(interface_->ResetSharedSpacing(),
                QStringLiteral("shared-spacing"));
}

void ConfigClient::setAppearanceMode(const QString &mode) {
  beginMutation(interface_->SetAppearanceMode(mode),
                QStringLiteral("appearance-mode"));
}

void ConfigClient::resetAppearanceMode() {
  beginMutation(interface_->ResetAppearanceMode(),
                QStringLiteral("appearance-mode"));
}

void ConfigClient::setAppearanceAutomation(
    const QString &source, const QString &scheduleMode,
    const uint darkStartMinute, const uint lightStartMinute,
    const QString &locationSource, const bool hasLocation,
    const double latitude, const double longitude) {
  beginMutation(interface_->SetAppearanceAutomation(
                    source, scheduleMode, darkStartMinute, lightStartMinute,
                    locationSource, hasLocation, latitude, longitude),
                QStringLiteral("appearance-automation"));
}

void ConfigClient::resetAppearanceAutomation() {
  beginMutation(interface_->ResetAppearanceAutomation(),
                QStringLiteral("appearance-automation"));
}

void ConfigClient::setNightLightSettings(
    const bool enabled, const bool automatic, const QString &scheduleMode,
    const uint darkStartMinute, const uint lightStartMinute,
    const QString &locationSource, const bool hasLocation,
    const double latitude, const double longitude, const uint nightTemperature,
    const uint dayTemperature, const bool gradual) {
  beginMutation(interface_->SetNightLightSettings(
                    enabled, automatic, scheduleMode, darkStartMinute,
                    lightStartMinute, locationSource, hasLocation, latitude,
                    longitude, nightTemperature, dayTemperature, gradual),
                QStringLiteral("night-light"));
}

void ConfigClient::resetNightLightSettings() {
  beginMutation(interface_->ResetNightLightSettings(),
                QStringLiteral("night-light"));
}

void ConfigClient::clearError() {
  if (lastErrorName_.isEmpty() && lastErrorOperation_.isEmpty() &&
      lastErrorMessage_.isEmpty()) {
    return;
  }

  lastErrorName_.clear();
  lastErrorOperation_.clear();
  lastErrorMessage_.clear();
  emit lastErrorChanged();
}

void ConfigClient::propertiesChanged(const QString &changedInterface,
                                     const QVariantMap &changed,
                                     const QStringList &invalidated) {
  if (changedInterface != interfaceName) {
    return;
  }

  if (!invalidated.isEmpty()) {
    setAvailable(false);
    refresh();
    return;
  }

  if (!available_ || !applyProperties(changed, false)) {
    setAvailable(false);
    refresh();
    return;
  }

  setAvailable(true);
}

void ConfigClient::serviceOwnerChanged(const QString &name,
                                       const QString &oldOwner,
                                       const QString &newOwner) {
  Q_UNUSED(name)
  Q_UNUSED(oldOwner)

  ++ownerGeneration_;
  setAvailable(false);

  if (!newOwner.isEmpty()) {
    refresh();
  }
}

void ConfigClient::refresh() {
  auto message = QDBusMessage::createMethodCall(
      serviceName, objectPath, propertiesInterface, QStringLiteral("GetAll"));
  message.setArguments({interfaceName});

  const auto generation = ownerGeneration_;
  auto *watcher =
      new QDBusPendingCallWatcher(connection_.asyncCall(message), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation] {
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            watcher->deleteLater();

            if (generation != ownerGeneration_) {
              return;
            }

            if (reply.isError()) {
              setAvailable(false);
              return;
            }

            if (!applyProperties(reply.value(), true)) {
              setAvailable(false);
              return;
            }
            setAvailable(true);
          });
}

bool ConfigClient::applyProperties(const QVariantMap &properties,
                                   const bool requireComplete) {
  const QStringList required{
      QStringLiteral("BarHeight"),
      QStringLiteral("ShellBorderEnabled"),
      QStringLiteral("ShellBorderWidth"),
      QStringLiteral("ShellBorderRadius"),
      QStringLiteral("SyncHyprlandWindowBorders"),
      QStringLiteral("ShellInnerSpacing"),
      QStringLiteral("ShellOuterSpacing"),
      QStringLiteral("SyncHyprlandWindowSpacing"),
      QStringLiteral("AppearanceMode"),
      QStringLiteral("AppearanceAutomationSource"),
      QStringLiteral("AppearanceScheduleMode"),
      QStringLiteral("AppearanceDarkStartMinute"),
      QStringLiteral("AppearanceLightStartMinute"),
      QStringLiteral("AppearanceLocationSource"),
      QStringLiteral("AppearanceHasLocation"),
      QStringLiteral("AppearanceLatitude"),
      QStringLiteral("AppearanceLongitude"),
      QStringLiteral("ScheduledAppearanceMode"),
      QStringLiteral("AppearanceNextTransition"),
      QStringLiteral("AppearanceSunrise"),
      QStringLiteral("AppearanceSunset"),
      QStringLiteral("AppearanceAutomationStatus"),
      QStringLiteral("NightLightEnabled"),
      QStringLiteral("NightLightAutomatic"),
      QStringLiteral("NightLightScheduleMode"),
      QStringLiteral("NightLightDarkStartMinute"),
      QStringLiteral("NightLightLightStartMinute"),
      QStringLiteral("NightLightLocationSource"),
      QStringLiteral("NightLightHasLocation"),
      QStringLiteral("NightLightLatitude"),
      QStringLiteral("NightLightLongitude"),
      QStringLiteral("NightLightTemperature"),
      QStringLiteral("NightLightDayTemperature"),
      QStringLiteral("NightLightGradual"),
      QStringLiteral("HyprsunsetAvailable"),
      QStringLiteral("NightLightRuntimeState"),
      QStringLiteral("NightLightCurrentTemperature"),
      QStringLiteral("NightLightNextTransition"),
      QStringLiteral("NightLightSunrise"),
      QStringLiteral("NightLightSunset"),
      QStringLiteral("NightLightStatus"),
      QStringLiteral("Revision"),
      QStringLiteral("RecoveryState"),
  };
  if (requireComplete) {
    for (const auto &name : required) {
      if (!properties.contains(name)) {
        return false;
      }
    }
  }

  const QStringList sharedBorderProperties{
      QStringLiteral("ShellBorderEnabled"),
      QStringLiteral("ShellBorderWidth"),
      QStringLiteral("ShellBorderRadius"),
      QStringLiteral("SyncHyprlandWindowBorders"),
  };
  const QStringList sharedSpacingProperties{
      QStringLiteral("ShellInnerSpacing"),
      QStringLiteral("ShellOuterSpacing"),
      QStringLiteral("SyncHyprlandWindowSpacing"),
  };
  const QStringList appearanceAutomationProperties{
      QStringLiteral("AppearanceAutomationSource"),
      QStringLiteral("AppearanceScheduleMode"),
      QStringLiteral("AppearanceDarkStartMinute"),
      QStringLiteral("AppearanceLightStartMinute"),
      QStringLiteral("AppearanceLocationSource"),
      QStringLiteral("AppearanceHasLocation"),
      QStringLiteral("AppearanceLatitude"),
      QStringLiteral("AppearanceLongitude"),
  };
  const QStringList nightLightProperties{
      QStringLiteral("NightLightEnabled"),
      QStringLiteral("NightLightAutomatic"),
      QStringLiteral("NightLightScheduleMode"),
      QStringLiteral("NightLightDarkStartMinute"),
      QStringLiteral("NightLightLightStartMinute"),
      QStringLiteral("NightLightLocationSource"),
      QStringLiteral("NightLightHasLocation"),
      QStringLiteral("NightLightLatitude"),
      QStringLiteral("NightLightLongitude"),
      QStringLiteral("NightLightTemperature"),
      QStringLiteral("NightLightDayTemperature"),
      QStringLiteral("NightLightGradual"),
  };
  auto sharedBorderSupplied = false;
  for (const auto &name : sharedBorderProperties) {
    sharedBorderSupplied = sharedBorderSupplied || properties.contains(name);
  }
  if (sharedBorderSupplied) {
    for (const auto &name : sharedBorderProperties) {
      if (!properties.contains(name)) {
        return false;
      }
    }
  }
  auto sharedSpacingSupplied = false;
  for (const auto &name : sharedSpacingProperties) {
    sharedSpacingSupplied = sharedSpacingSupplied || properties.contains(name);
  }
  if (sharedSpacingSupplied) {
    for (const auto &name : sharedSpacingProperties) {
      if (!properties.contains(name)) {
        return false;
      }
    }
  }
  const auto completeTupleSupplied =
      [&properties](const QStringList &names) -> std::optional<bool> {
    auto supplied = false;
    for (const auto &name : names) {
      supplied = supplied || properties.contains(name);
    }
    if (!supplied) {
      return false;
    }
    for (const auto &name : names) {
      if (!properties.contains(name)) {
        return std::nullopt;
      }
    }
    return true;
  };
  const auto appearanceAutomationSupplied =
      completeTupleSupplied(appearanceAutomationProperties);
  const auto nightLightSupplied = completeTupleSupplied(nightLightProperties);
  if (!appearanceAutomationSupplied.has_value() ||
      !nightLightSupplied.has_value()) {
    return false;
  }
  if ((properties.contains(QStringLiteral("BarHeight")) ||
       sharedBorderSupplied || sharedSpacingSupplied ||
       properties.contains(QStringLiteral("AppearanceMode")) ||
       *appearanceAutomationSupplied || *nightLightSupplied) &&
      !properties.contains(QStringLiteral("Revision"))) {
    return false;
  }

  auto nextBarHeight = barHeight_;
  auto nextShellBorderEnabled = shellBorderEnabled_;
  auto nextShellBorderWidth = shellBorderWidth_;
  auto nextShellBorderRadius = shellBorderRadius_;
  auto nextSyncHyprlandWindowBorders = syncHyprlandWindowBorders_;
  auto nextShellInnerSpacing = shellInnerSpacing_;
  auto nextShellOuterSpacing = shellOuterSpacing_;
  auto nextSyncHyprlandWindowSpacing = syncHyprlandWindowSpacing_;
  auto nextAppearanceMode = appearanceMode_;
  auto nextAppearanceAutomationSource = appearanceAutomationSource_;
  auto nextAppearanceScheduleMode = appearanceScheduleMode_;
  auto nextAppearanceDarkStartMinute = appearanceDarkStartMinute_;
  auto nextAppearanceLightStartMinute = appearanceLightStartMinute_;
  auto nextAppearanceLocationSource = appearanceLocationSource_;
  auto nextAppearanceHasLocation = appearanceHasLocation_;
  auto nextAppearanceLatitude = appearanceLatitude_;
  auto nextAppearanceLongitude = appearanceLongitude_;
  auto nextScheduledAppearanceMode = scheduledAppearanceMode_;
  auto nextAppearanceNextTransition = appearanceNextTransition_;
  auto nextAppearanceSunrise = appearanceSunrise_;
  auto nextAppearanceSunset = appearanceSunset_;
  auto nextAppearanceAutomationStatus = appearanceAutomationStatus_;
  auto nextNightLightEnabled = nightLightEnabled_;
  auto nextNightLightAutomatic = nightLightAutomatic_;
  auto nextNightLightScheduleMode = nightLightScheduleMode_;
  auto nextNightLightDarkStartMinute = nightLightDarkStartMinute_;
  auto nextNightLightLightStartMinute = nightLightLightStartMinute_;
  auto nextNightLightLocationSource = nightLightLocationSource_;
  auto nextNightLightHasLocation = nightLightHasLocation_;
  auto nextNightLightLatitude = nightLightLatitude_;
  auto nextNightLightLongitude = nightLightLongitude_;
  auto nextNightLightTemperature = nightLightTemperature_;
  auto nextNightLightDayTemperature = nightLightDayTemperature_;
  auto nextNightLightGradual = nightLightGradual_;
  auto nextHyprsunsetAvailable = hyprsunsetAvailable_;
  auto nextNightLightRuntimeState = nightLightRuntimeState_;
  auto nextNightLightCurrentTemperature = nightLightCurrentTemperature_;
  auto nextNightLightNextTransition = nightLightNextTransition_;
  auto nextNightLightSunrise = nightLightSunrise_;
  auto nextNightLightSunset = nightLightSunset_;
  auto nextNightLightStatus = nightLightStatus_;
  auto nextRevision = revision_;
  auto nextRecoveryState = recoveryState_;

  const auto barHeight = properties.constFind(QStringLiteral("BarHeight"));
  if (barHeight != properties.cend()) {
    if (barHeight->metaType().id() != QMetaType::UInt) {
      return false;
    }
    nextBarHeight = barHeight->toUInt();
    if (nextBarHeight < ConfigValues::minimumBarHeight ||
        nextBarHeight > ConfigValues::maximumBarHeight) {
      return false;
    }
  }

  const auto shellBorderEnabled =
      properties.constFind(QStringLiteral("ShellBorderEnabled"));
  if (shellBorderEnabled != properties.cend()) {
    if (shellBorderEnabled->metaType().id() != QMetaType::Bool) {
      return false;
    }
    nextShellBorderEnabled = shellBorderEnabled->toBool();
  }

  const auto shellBorderWidth =
      properties.constFind(QStringLiteral("ShellBorderWidth"));
  if (shellBorderWidth != properties.cend()) {
    if (shellBorderWidth->metaType().id() != QMetaType::UInt) {
      return false;
    }
    nextShellBorderWidth = shellBorderWidth->toUInt();
    if (nextShellBorderWidth < ConfigValues::minimumShellBorderWidth ||
        nextShellBorderWidth > ConfigValues::maximumShellBorderWidth) {
      return false;
    }
  }

  const auto shellBorderRadius =
      properties.constFind(QStringLiteral("ShellBorderRadius"));
  if (shellBorderRadius != properties.cend()) {
    if (shellBorderRadius->metaType().id() != QMetaType::UInt) {
      return false;
    }
    nextShellBorderRadius = shellBorderRadius->toUInt();
    if (nextShellBorderRadius < ConfigValues::minimumShellBorderRadius ||
        nextShellBorderRadius > ConfigValues::maximumShellBorderRadius) {
      return false;
    }
  }

  const auto syncHyprlandWindowBorders =
      properties.constFind(QStringLiteral("SyncHyprlandWindowBorders"));
  if (syncHyprlandWindowBorders != properties.cend()) {
    if (syncHyprlandWindowBorders->metaType().id() != QMetaType::Bool) {
      return false;
    }
    nextSyncHyprlandWindowBorders = syncHyprlandWindowBorders->toBool();
  }

  const auto shellInnerSpacing =
      properties.constFind(QStringLiteral("ShellInnerSpacing"));
  if (shellInnerSpacing != properties.cend()) {
    if (shellInnerSpacing->metaType().id() != QMetaType::UInt) {
      return false;
    }
    nextShellInnerSpacing = shellInnerSpacing->toUInt();
    if (nextShellInnerSpacing < ConfigValues::minimumShellSpacing ||
        nextShellInnerSpacing > ConfigValues::maximumShellSpacing) {
      return false;
    }
  }

  const auto shellOuterSpacing =
      properties.constFind(QStringLiteral("ShellOuterSpacing"));
  if (shellOuterSpacing != properties.cend()) {
    if (shellOuterSpacing->metaType().id() != QMetaType::UInt) {
      return false;
    }
    nextShellOuterSpacing = shellOuterSpacing->toUInt();
    if (nextShellOuterSpacing < ConfigValues::minimumShellSpacing ||
        nextShellOuterSpacing > ConfigValues::maximumShellSpacing) {
      return false;
    }
  }

  const auto syncHyprlandWindowSpacing =
      properties.constFind(QStringLiteral("SyncHyprlandWindowSpacing"));
  if (syncHyprlandWindowSpacing != properties.cend()) {
    if (syncHyprlandWindowSpacing->metaType().id() != QMetaType::Bool) {
      return false;
    }
    nextSyncHyprlandWindowSpacing = syncHyprlandWindowSpacing->toBool();
  }

  const auto revision = properties.constFind(QStringLiteral("Revision"));

  const auto appearanceMode =
      properties.constFind(QStringLiteral("AppearanceMode"));
  if (appearanceMode != properties.cend()) {
    if (appearanceMode->metaType().id() != QMetaType::QString) {
      return false;
    }
    nextAppearanceMode = appearanceMode->toString();
    if (!ConfigValues::isValidAppearanceMode(nextAppearanceMode)) {
      return false;
    }
  }

  const auto readString = [&properties](const QString &name, QString &target) {
    const auto value = properties.constFind(name);
    if (value == properties.cend()) {
      return true;
    }
    if (value->metaType().id() != QMetaType::QString) {
      return false;
    }
    target = value->toString();
    return target.size() <= 2048;
  };
  const auto readUInt = [&properties](const QString &name, uint &target) {
    const auto value = properties.constFind(name);
    if (value == properties.cend()) {
      return true;
    }
    if (value->metaType().id() != QMetaType::UInt) {
      return false;
    }
    target = value->toUInt();
    return true;
  };
  const auto readBool = [&properties](const QString &name, bool &target) {
    const auto value = properties.constFind(name);
    if (value == properties.cend()) {
      return true;
    }
    if (value->metaType().id() != QMetaType::Bool) {
      return false;
    }
    target = value->toBool();
    return true;
  };
  const auto readDouble = [&properties](const QString &name, double &target) {
    const auto value = properties.constFind(name);
    if (value == properties.cend()) {
      return true;
    }
    if (value->metaType().id() != QMetaType::Double) {
      return false;
    }
    target = value->toDouble();
    return std::isfinite(target);
  };

  if (!readString(QStringLiteral("AppearanceAutomationSource"),
                  nextAppearanceAutomationSource) ||
      !readString(QStringLiteral("AppearanceScheduleMode"),
                  nextAppearanceScheduleMode) ||
      !readUInt(QStringLiteral("AppearanceDarkStartMinute"),
                nextAppearanceDarkStartMinute) ||
      !readUInt(QStringLiteral("AppearanceLightStartMinute"),
                nextAppearanceLightStartMinute) ||
      !readString(QStringLiteral("AppearanceLocationSource"),
                  nextAppearanceLocationSource) ||
      !readBool(QStringLiteral("AppearanceHasLocation"),
                nextAppearanceHasLocation) ||
      !readDouble(QStringLiteral("AppearanceLatitude"),
                  nextAppearanceLatitude) ||
      !readDouble(QStringLiteral("AppearanceLongitude"),
                  nextAppearanceLongitude) ||
      !ConfigValues::isValidAppearanceAutomationSource(
          nextAppearanceAutomationSource) ||
      !ConfigValues::isValidScheduleMode(nextAppearanceScheduleMode) ||
      nextAppearanceDarkStartMinute > ConfigValues::maximumScheduleMinute ||
      nextAppearanceLightStartMinute > ConfigValues::maximumScheduleMinute ||
      nextAppearanceDarkStartMinute == nextAppearanceLightStartMinute ||
      !ConfigValues::isValidLocationSource(nextAppearanceLocationSource) ||
      nextAppearanceLatitude < -90.0 || nextAppearanceLatitude > 90.0 ||
      nextAppearanceLongitude < -180.0 || nextAppearanceLongitude > 180.0) {
    return false;
  }

  if (!readString(QStringLiteral("ScheduledAppearanceMode"),
                  nextScheduledAppearanceMode) ||
      !readString(QStringLiteral("AppearanceNextTransition"),
                  nextAppearanceNextTransition) ||
      !readString(QStringLiteral("AppearanceSunrise"), nextAppearanceSunrise) ||
      !readString(QStringLiteral("AppearanceSunset"), nextAppearanceSunset) ||
      !readString(QStringLiteral("AppearanceAutomationStatus"),
                  nextAppearanceAutomationStatus) ||
      (nextScheduledAppearanceMode != QStringLiteral("system") &&
       nextScheduledAppearanceMode != QStringLiteral("light") &&
       nextScheduledAppearanceMode != QStringLiteral("dark") &&
       nextScheduledAppearanceMode != QStringLiteral("unavailable"))) {
    return false;
  }

  if (!readBool(QStringLiteral("NightLightEnabled"), nextNightLightEnabled) ||
      !readBool(QStringLiteral("NightLightAutomatic"),
                nextNightLightAutomatic) ||
      !readString(QStringLiteral("NightLightScheduleMode"),
                  nextNightLightScheduleMode) ||
      !readUInt(QStringLiteral("NightLightDarkStartMinute"),
                nextNightLightDarkStartMinute) ||
      !readUInt(QStringLiteral("NightLightLightStartMinute"),
                nextNightLightLightStartMinute) ||
      !readString(QStringLiteral("NightLightLocationSource"),
                  nextNightLightLocationSource) ||
      !readBool(QStringLiteral("NightLightHasLocation"),
                nextNightLightHasLocation) ||
      !readDouble(QStringLiteral("NightLightLatitude"),
                  nextNightLightLatitude) ||
      !readDouble(QStringLiteral("NightLightLongitude"),
                  nextNightLightLongitude) ||
      !readUInt(QStringLiteral("NightLightTemperature"),
                nextNightLightTemperature) ||
      !readUInt(QStringLiteral("NightLightDayTemperature"),
                nextNightLightDayTemperature) ||
      !readBool(QStringLiteral("NightLightGradual"), nextNightLightGradual) ||
      !ConfigValues::isValidScheduleMode(nextNightLightScheduleMode) ||
      nextNightLightDarkStartMinute > ConfigValues::maximumScheduleMinute ||
      nextNightLightLightStartMinute > ConfigValues::maximumScheduleMinute ||
      nextNightLightDarkStartMinute == nextNightLightLightStartMinute ||
      !ConfigValues::isValidLocationSource(nextNightLightLocationSource) ||
      nextNightLightLatitude < -90.0 || nextNightLightLatitude > 90.0 ||
      nextNightLightLongitude < -180.0 || nextNightLightLongitude > 180.0 ||
      nextNightLightTemperature < ConfigValues::minimumNightLightTemperature ||
      nextNightLightTemperature > ConfigValues::maximumNightTemperature ||
      nextNightLightDayTemperature < nextNightLightTemperature ||
      nextNightLightDayTemperature >
          ConfigValues::maximumNightLightTemperature) {
    return false;
  }

  if (!readBool(QStringLiteral("HyprsunsetAvailable"),
                nextHyprsunsetAvailable) ||
      !readString(QStringLiteral("NightLightRuntimeState"),
                  nextNightLightRuntimeState) ||
      !readUInt(QStringLiteral("NightLightCurrentTemperature"),
                nextNightLightCurrentTemperature) ||
      !readString(QStringLiteral("NightLightNextTransition"),
                  nextNightLightNextTransition) ||
      !readString(QStringLiteral("NightLightSunrise"), nextNightLightSunrise) ||
      !readString(QStringLiteral("NightLightSunset"), nextNightLightSunset) ||
      !readString(QStringLiteral("NightLightStatus"), nextNightLightStatus) ||
      nextNightLightCurrentTemperature >
          ConfigValues::maximumNightLightTemperature) {
    return false;
  }

  if (revision != properties.cend()) {
    if (revision->metaType().id() != QMetaType::ULongLong) {
      return false;
    }
    nextRevision = revision->toULongLong();
  }

  const auto recoveryState =
      properties.constFind(QStringLiteral("RecoveryState"));
  if (recoveryState != properties.cend()) {
    if (recoveryState->metaType().id() != QMetaType::QString) {
      return false;
    }
    nextRecoveryState = recoveryState->toString();
  }

  const auto barHeightChanged = nextBarHeight != barHeight_;
  const auto sharedBorderChanged =
      nextShellBorderEnabled != shellBorderEnabled_ ||
      nextShellBorderWidth != shellBorderWidth_ ||
      nextShellBorderRadius != shellBorderRadius_ ||
      nextSyncHyprlandWindowBorders != syncHyprlandWindowBorders_;
  const auto sharedSpacingChanged =
      nextShellInnerSpacing != shellInnerSpacing_ ||
      nextShellOuterSpacing != shellOuterSpacing_ ||
      nextSyncHyprlandWindowSpacing != syncHyprlandWindowSpacing_;
  const auto appearanceModeChanged = nextAppearanceMode != appearanceMode_;
  const auto appearanceAutomationChanged =
      nextAppearanceAutomationSource != appearanceAutomationSource_ ||
      nextAppearanceScheduleMode != appearanceScheduleMode_ ||
      nextAppearanceDarkStartMinute != appearanceDarkStartMinute_ ||
      nextAppearanceLightStartMinute != appearanceLightStartMinute_ ||
      nextAppearanceLocationSource != appearanceLocationSource_ ||
      nextAppearanceHasLocation != appearanceHasLocation_ ||
      nextAppearanceLatitude != appearanceLatitude_ ||
      nextAppearanceLongitude != appearanceLongitude_;
  const auto appearanceRuntimeChanged =
      nextScheduledAppearanceMode != scheduledAppearanceMode_ ||
      nextAppearanceNextTransition != appearanceNextTransition_ ||
      nextAppearanceSunrise != appearanceSunrise_ ||
      nextAppearanceSunset != appearanceSunset_ ||
      nextAppearanceAutomationStatus != appearanceAutomationStatus_;
  const auto nightLightChanged =
      nextNightLightEnabled != nightLightEnabled_ ||
      nextNightLightAutomatic != nightLightAutomatic_ ||
      nextNightLightScheduleMode != nightLightScheduleMode_ ||
      nextNightLightDarkStartMinute != nightLightDarkStartMinute_ ||
      nextNightLightLightStartMinute != nightLightLightStartMinute_ ||
      nextNightLightLocationSource != nightLightLocationSource_ ||
      nextNightLightHasLocation != nightLightHasLocation_ ||
      nextNightLightLatitude != nightLightLatitude_ ||
      nextNightLightLongitude != nightLightLongitude_ ||
      nextNightLightTemperature != nightLightTemperature_ ||
      nextNightLightDayTemperature != nightLightDayTemperature_ ||
      nextNightLightGradual != nightLightGradual_;
  const auto nightLightRuntimeChanged =
      nextHyprsunsetAvailable != hyprsunsetAvailable_ ||
      nextNightLightRuntimeState != nightLightRuntimeState_ ||
      nextNightLightCurrentTemperature != nightLightCurrentTemperature_ ||
      nextNightLightNextTransition != nightLightNextTransition_ ||
      nextNightLightSunrise != nightLightSunrise_ ||
      nextNightLightSunset != nightLightSunset_ ||
      nextNightLightStatus != nightLightStatus_;
  const auto revisionChanged = nextRevision != revision_;
  const auto recoveryStateChanged = nextRecoveryState != recoveryState_;

  if (projectionEstablished_ && revision != properties.cend() &&
      (nextRevision < revision_ ||
       (nextRevision == revision_ &&
        (barHeightChanged || sharedBorderChanged || sharedSpacingChanged ||
         appearanceModeChanged || appearanceAutomationChanged ||
         nightLightChanged)))) {
    return false;
  }

  barHeight_ = nextBarHeight;
  shellBorderEnabled_ = nextShellBorderEnabled;
  shellBorderWidth_ = nextShellBorderWidth;
  shellBorderRadius_ = nextShellBorderRadius;
  syncHyprlandWindowBorders_ = nextSyncHyprlandWindowBorders;
  shellInnerSpacing_ = nextShellInnerSpacing;
  shellOuterSpacing_ = nextShellOuterSpacing;
  syncHyprlandWindowSpacing_ = nextSyncHyprlandWindowSpacing;
  appearanceMode_ = nextAppearanceMode;
  appearanceAutomationSource_ = nextAppearanceAutomationSource;
  appearanceScheduleMode_ = nextAppearanceScheduleMode;
  appearanceDarkStartMinute_ = nextAppearanceDarkStartMinute;
  appearanceLightStartMinute_ = nextAppearanceLightStartMinute;
  appearanceLocationSource_ = nextAppearanceLocationSource;
  appearanceHasLocation_ = nextAppearanceHasLocation;
  appearanceLatitude_ = nextAppearanceLatitude;
  appearanceLongitude_ = nextAppearanceLongitude;
  scheduledAppearanceMode_ = nextScheduledAppearanceMode;
  appearanceNextTransition_ = nextAppearanceNextTransition;
  appearanceSunrise_ = nextAppearanceSunrise;
  appearanceSunset_ = nextAppearanceSunset;
  appearanceAutomationStatus_ = nextAppearanceAutomationStatus;
  nightLightEnabled_ = nextNightLightEnabled;
  nightLightAutomatic_ = nextNightLightAutomatic;
  nightLightScheduleMode_ = nextNightLightScheduleMode;
  nightLightDarkStartMinute_ = nextNightLightDarkStartMinute;
  nightLightLightStartMinute_ = nextNightLightLightStartMinute;
  nightLightLocationSource_ = nextNightLightLocationSource;
  nightLightHasLocation_ = nextNightLightHasLocation;
  nightLightLatitude_ = nextNightLightLatitude;
  nightLightLongitude_ = nextNightLightLongitude;
  nightLightTemperature_ = nextNightLightTemperature;
  nightLightDayTemperature_ = nextNightLightDayTemperature;
  nightLightGradual_ = nextNightLightGradual;
  hyprsunsetAvailable_ = nextHyprsunsetAvailable;
  nightLightRuntimeState_ = nextNightLightRuntimeState;
  nightLightCurrentTemperature_ = nextNightLightCurrentTemperature;
  nightLightNextTransition_ = nextNightLightNextTransition;
  nightLightSunrise_ = nextNightLightSunrise;
  nightLightSunset_ = nextNightLightSunset;
  nightLightStatus_ = nextNightLightStatus;
  revision_ = nextRevision;
  recoveryState_ = nextRecoveryState;
  projectionEstablished_ = true;

  if (barHeightChanged) {
    emit this->barHeightChanged();
  }
  if (sharedBorderChanged) {
    emit this->sharedBorderChanged();
  }
  if (sharedSpacingChanged) {
    emit this->sharedSpacingChanged();
  }
  if (appearanceModeChanged) {
    emit this->appearanceModeChanged();
  }
  if (appearanceAutomationChanged) {
    emit this->appearanceAutomationChanged();
  }
  if (appearanceRuntimeChanged) {
    emit this->appearanceRuntimeChanged();
  }
  if (nightLightChanged) {
    emit this->nightLightChanged();
  }
  if (nightLightRuntimeChanged) {
    emit this->nightLightRuntimeChanged();
  }
  if (revisionChanged) {
    emit this->revisionChanged();
  }
  if (recoveryStateChanged) {
    emit this->recoveryStateChanged();
  }

  return true;
}

void ConfigClient::beginMutation(const QDBusPendingCall &call,
                                 const QString &operation) {
  clearError();

  const auto wasBusy = busy();
  ++pendingOperations_;
  if (!wasBusy) {
    emit busyChanged();
  }

  auto *watcher = new QDBusPendingCallWatcher(call, this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, operation] {
            const QDBusPendingReply<qulonglong> reply = *watcher;
            watcher->deleteLater();

            if (reply.isError()) {
              const auto error = reply.error();
              setError(error.name(), error.message(), operation);
              if (error.type() == QDBusError::ServiceUnknown) {
                setAvailable(false);
              }
            } else if (!available_) {
              refresh();
            }

            --pendingOperations_;
            if (!busy()) {
              emit busyChanged();
            }
          });
}

void ConfigClient::setAvailable(bool available) {
  if (available == available_) {
    return;
  }

  available_ = available;
  emit availableChanged();

  if (available_) {
    clearError();
  }
}

void ConfigClient::setError(const QString &name, const QString &message,
                            const QString &operation) {
  if (name != lastErrorName_ || operation != lastErrorOperation_ ||
      message != lastErrorMessage_) {
    lastErrorName_ = name;
    lastErrorOperation_ = operation;
    lastErrorMessage_ = message;
    emit lastErrorChanged();
  }

  emit operationFailed(name, message);
}

} // namespace HyprShelld
