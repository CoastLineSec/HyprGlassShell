#include "config_service.h"

#include "appearance_schedule.h"
#include "config/config_values.h"
#include "geoclue_client.h"
#include "hyprsunset_controller.h"

#include <QDBusMessage>
#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace HyprShelld {
namespace {

const QString configInterface = QStringLiteral("org.hyprshelld.Config1");
const QString configPath = QStringLiteral("/org/hyprshelld/Config1");
const QString invalidBarHeightError =
    QStringLiteral("org.hyprshelld.Config1.Error.InvalidBarHeight");
const QString invalidSharedBorderError =
    QStringLiteral("org.hyprshelld.Config1.Error.InvalidSharedBorder");
const QString invalidSharedSpacingError =
    QStringLiteral("org.hyprshelld.Config1.Error.InvalidSharedSpacing");
const QString invalidAppearanceModeError =
    QStringLiteral("org.hyprshelld.Config1.Error.InvalidAppearanceMode");
const QString invalidAppearanceAutomationError =
    QStringLiteral("org.hyprshelld.Config1.Error.InvalidAppearanceAutomation");
const QString invalidNightLightError =
    QStringLiteral("org.hyprshelld.Config1.Error.InvalidNightLightSettings");
const QString persistenceError =
    QStringLiteral("org.hyprshelld.Config1.Error.PersistenceFailed");

QString recoveryStateName(ConfigRecoveryState state) {
  switch (state) {
  case ConfigRecoveryState::Normal:
    return QStringLiteral("normal");
  case ConfigRecoveryState::Recovered:
    return QStringLiteral("recovered");
  case ConfigRecoveryState::Defaulted:
    return QStringLiteral("defaulted");
  }

  Q_UNREACHABLE_RETURN(QString());
}

QString dateTimeText(const QDateTime &dateTime) {
  return dateTime.isValid() ? dateTime.toString(Qt::ISODateWithMs) : QString();
}

QString hyprsunsetStateName(const HyprsunsetController::State state) {
  using State = HyprsunsetController::State;
  switch (state) {
  case State::Disabled:
    return QStringLiteral("disabled");
  case State::Probing:
    return QStringLiteral("probing");
  case State::ExternalDaemon:
    return QStringLiteral("external-daemon");
  case State::Starting:
    return QStringLiteral("starting");
  case State::Applying:
    return QStringLiteral("applying");
  case State::Ready:
    return QStringLiteral("ready");
  case State::RetryWaiting:
    return QStringLiteral("retry-waiting");
  case State::Stopping:
    return QStringLiteral("stopping");
  case State::Failed:
    return QStringLiteral("failed");
  }
  Q_UNREACHABLE_RETURN(QStringLiteral("failed"));
}

} // namespace

ConfigService::ConfigService(ConfigStore store, const ConfigLoadResult &loaded,
                             QDBusConnection connection, QObject *parent)
    : QObject(parent), store_(std::move(store)), state_(loaded.state),
      recoveryState_(recoveryStateName(loaded.recoveryState)),
      connection_(std::move(connection)),
      legacyWorkspaceSettings_(loaded.legacyWorkspaceSettings),
      legacyWorkspaceRetirementPending_(
          loaded.legacyWorkspaceRetirementPending) {
  legacyWorkspaceRetirementTimer_.setSingleShot(true);
  legacyWorkspaceRetirementTimer_.setInterval(1000);
  connect(&legacyWorkspaceRetirementTimer_, &QTimer::timeout, this,
          &ConfigService::attemptLegacyWorkspaceRetirement);
  automationTimer_.setSingleShot(true);
  connect(&automationTimer_, &QTimer::timeout, this,
          &ConfigService::reconcileRuntime);
  reconcileRuntime();
}

uint ConfigService::barHeight() const { return state_.barHeight; }

bool ConfigService::shellBorderEnabled() const {
  return state_.shellBorderEnabled;
}

uint ConfigService::shellBorderWidth() const { return state_.shellBorderWidth; }

uint ConfigService::shellBorderRadius() const {
  return state_.shellBorderRadius;
}

bool ConfigService::syncHyprlandWindowBorders() const {
  return state_.syncHyprlandWindowBorders;
}

uint ConfigService::shellInnerSpacing() const {
  return state_.shellInnerSpacing;
}

uint ConfigService::shellOuterSpacing() const {
  return state_.shellOuterSpacing;
}

bool ConfigService::syncHyprlandWindowSpacing() const {
  return state_.syncHyprlandWindowSpacing;
}

QString ConfigService::appearanceMode() const { return state_.appearanceMode; }

QString ConfigService::appearanceAutomationSource() const {
  return state_.appearanceAutomation.source;
}

QString ConfigService::appearanceScheduleMode() const {
  return state_.appearanceAutomation.schedule.mode;
}

uint ConfigService::appearanceDarkStartMinute() const {
  return state_.appearanceAutomation.schedule.darkStartMinute;
}

uint ConfigService::appearanceLightStartMinute() const {
  return state_.appearanceAutomation.schedule.lightStartMinute;
}

QString ConfigService::appearanceLocationSource() const {
  return state_.appearanceAutomation.schedule.locationSource;
}

bool ConfigService::appearanceHasLocation() const {
  return state_.appearanceAutomation.schedule.hasLocation;
}

double ConfigService::appearanceLatitude() const {
  return state_.appearanceAutomation.schedule.latitude;
}

double ConfigService::appearanceLongitude() const {
  return state_.appearanceAutomation.schedule.longitude;
}

QString ConfigService::scheduledAppearanceMode() const {
  return scheduledAppearanceMode_;
}

QString ConfigService::appearanceNextTransition() const {
  return appearanceNextTransition_;
}

QString ConfigService::appearanceSunrise() const { return appearanceSunrise_; }

QString ConfigService::appearanceSunset() const { return appearanceSunset_; }

QString ConfigService::appearanceAutomationStatus() const {
  return appearanceAutomationStatus_;
}

bool ConfigService::nightLightEnabled() const {
  return state_.nightLight.enabled;
}

bool ConfigService::nightLightAutomatic() const {
  return state_.nightLight.automatic;
}

QString ConfigService::nightLightScheduleMode() const {
  return state_.nightLight.schedule.mode;
}

uint ConfigService::nightLightDarkStartMinute() const {
  return state_.nightLight.schedule.darkStartMinute;
}

uint ConfigService::nightLightLightStartMinute() const {
  return state_.nightLight.schedule.lightStartMinute;
}

QString ConfigService::nightLightLocationSource() const {
  return state_.nightLight.schedule.locationSource;
}

bool ConfigService::nightLightHasLocation() const {
  return state_.nightLight.schedule.hasLocation;
}

double ConfigService::nightLightLatitude() const {
  return state_.nightLight.schedule.latitude;
}

double ConfigService::nightLightLongitude() const {
  return state_.nightLight.schedule.longitude;
}

uint ConfigService::nightLightTemperature() const {
  return state_.nightLight.nightTemperature;
}

uint ConfigService::nightLightDayTemperature() const {
  return state_.nightLight.dayTemperature;
}

bool ConfigService::nightLightGradual() const {
  return state_.nightLight.gradual;
}

bool ConfigService::hyprsunsetAvailable() const { return hyprsunsetAvailable_; }

QString ConfigService::nightLightRuntimeState() const {
  return nightLightRuntimeState_;
}

uint ConfigService::nightLightCurrentTemperature() const {
  return nightLightCurrentTemperature_;
}

QString ConfigService::nightLightNextTransition() const {
  return nightLightNextTransition_;
}

QString ConfigService::nightLightSunrise() const { return nightLightSunrise_; }

QString ConfigService::nightLightSunset() const { return nightLightSunset_; }

QString ConfigService::nightLightStatus() const { return nightLightStatus_; }

qulonglong ConfigService::revision() const { return state_.revision; }

QString ConfigService::recoveryState() const { return recoveryState_; }

void ConfigService::authorizeLegacyWorkspaceRetirement() {
  legacyWorkspaceRetirementAuthorized_ = true;
  attemptLegacyWorkspaceRetirement();
}

void ConfigService::attachAppearanceRuntime(GeoClueClient *geoClue,
                                            HyprsunsetController *hyprsunset) {
  if (geoClue_ || hyprsunset_) {
    return;
  }
  geoClue_ = geoClue;
  hyprsunset_ = hyprsunset;
  if (geoClue_) {
    connect(geoClue_, &GeoClueClient::changed, this,
            &ConfigService::reconcileRuntime);
  }
  if (hyprsunset_) {
    connect(hyprsunset_, &HyprsunsetController::stateChanged, this,
            &ConfigService::reconcileRuntime);
    connect(hyprsunset_, &HyprsunsetController::errorChanged, this,
            &ConfigService::reconcileRuntime);
    connect(hyprsunset_, &HyprsunsetController::currentTemperatureChanged, this,
            &ConfigService::reconcileRuntime);
  }

  const auto runtimeAvailable = hyprsunset_ && hyprsunset_->available();
  const auto availabilityChanged = hyprsunsetAvailable_ != runtimeAvailable;
  hyprsunsetAvailable_ = runtimeAvailable;
  if (availabilityChanged) {
    auto signal = QDBusMessage::createSignal(
        configPath, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    signal.setArguments({
        configInterface,
        QVariantMap{
            {QStringLiteral("HyprsunsetAvailable"), hyprsunsetAvailable_}},
        QStringList(),
    });
    connection_.send(signal);
  }
  reconcileRuntime();
}

qulonglong ConfigService::SetBarHeight(uint height) {
  return setBarHeight(height);
}

qulonglong ConfigService::ResetBarHeight() {
  return setBarHeight(ConfigValues::defaultBarHeight);
}

qulonglong
ConfigService::SetSharedBorder(const bool enabled, const uint width,
                               const uint radius,
                               const bool syncHyprlandWindowBorders) {
  return setSharedBorder(enabled, width, radius, syncHyprlandWindowBorders);
}

qulonglong ConfigService::ResetSharedBorder() {
  return setSharedBorder(ConfigValues::defaultShellBorderEnabled,
                         ConfigValues::defaultShellBorderWidth,
                         ConfigValues::defaultShellBorderRadius,
                         ConfigValues::defaultSyncHyprlandWindowBorders);
}

qulonglong
ConfigService::SetSharedSpacing(const uint inner, const uint outer,
                                const bool syncHyprlandWindowSpacing) {
  return setSharedSpacing(inner, outer, syncHyprlandWindowSpacing);
}

qulonglong ConfigService::ResetSharedSpacing() {
  return setSharedSpacing(ConfigValues::defaultShellInnerSpacing,
                          ConfigValues::defaultShellOuterSpacing,
                          ConfigValues::defaultSyncHyprlandWindowSpacing);
}

qulonglong ConfigService::SetAppearanceMode(const QString &mode) {
  return setAppearanceMode(mode);
}

qulonglong ConfigService::ResetAppearanceMode() {
  return setAppearanceMode(ConfigValues::defaultAppearanceMode);
}

qulonglong ConfigService::SetAppearanceAutomation(
    const QString &source, const QString &scheduleMode,
    const uint darkStartMinute, const uint lightStartMinute,
    const QString &locationSource, const bool hasLocation,
    const double latitude, const double longitude) {
  return setAppearanceAutomation(AppearanceAutomationConfig{
      .source = source,
      .schedule =
          {
              .mode = scheduleMode,
              .darkStartMinute = darkStartMinute,
              .lightStartMinute = lightStartMinute,
              .locationSource = locationSource,
              .hasLocation = hasLocation,
              .latitude = latitude,
              .longitude = longitude,
          },
  });
}

qulonglong ConfigService::ResetAppearanceAutomation() {
  return setAppearanceAutomation(AppearanceAutomationConfig());
}

qulonglong ConfigService::SetNightLightSettings(
    const bool enabled, const bool automatic, const QString &scheduleMode,
    const uint darkStartMinute, const uint lightStartMinute,
    const QString &locationSource, const bool hasLocation,
    const double latitude, const double longitude, const uint nightTemperature,
    const uint dayTemperature, const bool gradual) {
  return setNightLightSettings(NightLightConfig{
      .enabled = enabled,
      .automatic = automatic,
      .schedule =
          {
              .mode = scheduleMode,
              .darkStartMinute = darkStartMinute,
              .lightStartMinute = lightStartMinute,
              .locationSource = locationSource,
              .hasLocation = hasLocation,
              .latitude = latitude,
              .longitude = longitude,
          },
      .nightTemperature = nightTemperature,
      .dayTemperature = dayTemperature,
      .gradual = gradual,
  });
}

qulonglong ConfigService::ResetNightLightSettings() {
  return setNightLightSettings(NightLightConfig());
}

qulonglong ConfigService::setBarHeight(uint height) {
  if (height < ConfigValues::minimumBarHeight ||
      height > ConfigValues::maximumBarHeight) {
    reportError(
        invalidBarHeightError,
        QStringLiteral("Bar height must be between %1 and %2 logical pixels")
            .arg(ConfigValues::minimumBarHeight)
            .arg(ConfigValues::maximumBarHeight));
    return state_.revision;
  }

  if (height == state_.barHeight) {
    return state_.revision;
  }

  if (state_.revision == std::numeric_limits<quint64>::max()) {
    reportError(persistenceError,
                QStringLiteral("Configuration revision is exhausted"));
    return state_.revision;
  }

  auto next = state_;
  next.barHeight = height;
  next.revision = state_.revision + 1;

  QString error;
  if (!store_.persist(state_, next, legacyWorkspaceSettings_, error)) {
    reportError(persistenceError, error);
    return state_.revision;
  }

  const auto previous = state_;
  state_ = next;
  publishChange(previous);
  return state_.revision;
}

qulonglong
ConfigService::setSharedBorder(const bool enabled, const uint width,
                               const uint radius,
                               const bool syncHyprlandWindowBorders) {
  if (width < ConfigValues::minimumShellBorderWidth ||
      width > ConfigValues::maximumShellBorderWidth ||
      radius < ConfigValues::minimumShellBorderRadius ||
      radius > ConfigValues::maximumShellBorderRadius) {
    reportError(invalidSharedBorderError,
                QStringLiteral("Shared border width and radius must each be "
                               "between %1 and %2 logical pixels")
                    .arg(ConfigValues::minimumShellBorderWidth)
                    .arg(ConfigValues::maximumShellBorderWidth));
    return state_.revision;
  }

  if (enabled == state_.shellBorderEnabled &&
      width == state_.shellBorderWidth && radius == state_.shellBorderRadius &&
      syncHyprlandWindowBorders == state_.syncHyprlandWindowBorders) {
    return state_.revision;
  }

  if (state_.revision == std::numeric_limits<quint64>::max()) {
    reportError(persistenceError,
                QStringLiteral("Configuration revision is exhausted"));
    return state_.revision;
  }

  auto next = state_;
  next.shellBorderEnabled = enabled;
  next.shellBorderWidth = width;
  next.shellBorderRadius = radius;
  next.syncHyprlandWindowBorders = syncHyprlandWindowBorders;
  next.revision = state_.revision + 1;

  QString error;
  if (!store_.persist(state_, next, legacyWorkspaceSettings_, error)) {
    reportError(persistenceError, error);
    return state_.revision;
  }

  const auto previous = state_;
  state_ = next;
  publishChange(previous);
  return state_.revision;
}

qulonglong
ConfigService::setSharedSpacing(const uint inner, const uint outer,
                                const bool syncHyprlandWindowSpacing) {
  if (inner < ConfigValues::minimumShellSpacing ||
      inner > ConfigValues::maximumShellSpacing ||
      outer < ConfigValues::minimumShellSpacing ||
      outer > ConfigValues::maximumShellSpacing) {
    reportError(invalidSharedSpacingError,
                QStringLiteral("Shared inner and outer spacing must each be "
                               "between %1 and %2 logical pixels")
                    .arg(ConfigValues::minimumShellSpacing)
                    .arg(ConfigValues::maximumShellSpacing));
    return state_.revision;
  }

  if (inner == state_.shellInnerSpacing && outer == state_.shellOuterSpacing &&
      syncHyprlandWindowSpacing == state_.syncHyprlandWindowSpacing) {
    return state_.revision;
  }

  if (state_.revision == std::numeric_limits<quint64>::max()) {
    reportError(persistenceError,
                QStringLiteral("Configuration revision is exhausted"));
    return state_.revision;
  }

  auto next = state_;
  next.shellInnerSpacing = inner;
  next.shellOuterSpacing = outer;
  next.syncHyprlandWindowSpacing = syncHyprlandWindowSpacing;
  next.revision = state_.revision + 1;

  QString error;
  if (!store_.persist(state_, next, legacyWorkspaceSettings_, error)) {
    reportError(persistenceError, error);
    return state_.revision;
  }

  const auto previous = state_;
  state_ = next;
  publishChange(previous);
  return state_.revision;
}

qulonglong ConfigService::setAppearanceMode(const QString &mode) {
  if (!ConfigValues::isValidAppearanceMode(mode)) {
    reportError(
        invalidAppearanceModeError,
        QStringLiteral(
            "Appearance mode must be one of automatic, light, or dark"));
    return state_.revision;
  }

  if (mode == state_.appearanceMode) {
    return state_.revision;
  }

  if (state_.revision == std::numeric_limits<quint64>::max()) {
    reportError(persistenceError,
                QStringLiteral("Configuration revision is exhausted"));
    return state_.revision;
  }

  auto next = state_;
  next.appearanceMode = mode;
  next.revision = state_.revision + 1;

  QString error;
  if (!store_.persist(state_, next, legacyWorkspaceSettings_, error)) {
    reportError(persistenceError, error);
    return state_.revision;
  }

  const auto previous = state_;
  state_ = next;
  publishChange(previous);
  reconcileRuntime();
  return state_.revision;
}

bool ConfigService::validateSchedule(const ScheduleConfig &schedule,
                                     QString &message) const {
  if (!ConfigValues::isValidScheduleMode(schedule.mode)) {
    message = QStringLiteral("Schedule mode must be time or location");
    return false;
  }
  if (schedule.darkStartMinute > ConfigValues::maximumScheduleMinute ||
      schedule.lightStartMinute > ConfigValues::maximumScheduleMinute) {
    message = QStringLiteral("Schedule times must be within a 24-hour day");
    return false;
  }
  if (schedule.darkStartMinute == schedule.lightStartMinute) {
    message = QStringLiteral("Dark and light start times must be different");
    return false;
  }
  if (!ConfigValues::isValidLocationSource(schedule.locationSource)) {
    message = QStringLiteral("Location source must be manual or geoclue");
    return false;
  }
  if (!std::isfinite(schedule.latitude) || schedule.latitude < -90.0 ||
      schedule.latitude > 90.0 || !std::isfinite(schedule.longitude) ||
      schedule.longitude < -180.0 || schedule.longitude > 180.0) {
    message = QStringLiteral(
        "Latitude must be -90 through 90 and longitude -180 through 180");
    return false;
  }
  return true;
}

qulonglong ConfigService::setAppearanceAutomation(
    const AppearanceAutomationConfig &automation) {
  QString message;
  if (!ConfigValues::isValidAppearanceAutomationSource(automation.source) ||
      !validateSchedule(automation.schedule, message)) {
    if (message.isEmpty()) {
      message = QStringLiteral(
          "Automatic source must be desktop, schedule, or night-light");
    }
    reportError(invalidAppearanceAutomationError, message);
    return state_.revision;
  }
  if (automation == state_.appearanceAutomation) {
    return state_.revision;
  }
  if (state_.revision == std::numeric_limits<quint64>::max()) {
    reportError(persistenceError,
                QStringLiteral("Configuration revision is exhausted"));
    return state_.revision;
  }

  auto next = state_;
  next.appearanceAutomation = automation;
  next.revision = state_.revision + 1;
  QString error;
  if (!store_.persist(state_, next, legacyWorkspaceSettings_, error)) {
    reportError(persistenceError, error);
    return state_.revision;
  }

  const auto previous = state_;
  state_ = next;
  publishChange(previous);
  reconcileRuntime();
  return state_.revision;
}

qulonglong
ConfigService::setNightLightSettings(const NightLightConfig &nightLight) {
  QString message;
  if (!validateSchedule(nightLight.schedule, message) ||
      nightLight.nightTemperature <
          ConfigValues::minimumNightLightTemperature ||
      nightLight.nightTemperature > ConfigValues::maximumNightTemperature ||
      nightLight.dayTemperature < nightLight.nightTemperature ||
      nightLight.dayTemperature > ConfigValues::maximumNightLightTemperature) {
    if (message.isEmpty()) {
      message = QStringLiteral("Night temperature must be 2500–6000 K and no "
                               "cooler than the day temperature");
    }
    reportError(invalidNightLightError, message);
    return state_.revision;
  }
  if (nightLight == state_.nightLight) {
    return state_.revision;
  }
  if (state_.revision == std::numeric_limits<quint64>::max()) {
    reportError(persistenceError,
                QStringLiteral("Configuration revision is exhausted"));
    return state_.revision;
  }

  auto next = state_;
  next.nightLight = nightLight;
  next.revision = state_.revision + 1;
  QString error;
  if (!store_.persist(state_, next, legacyWorkspaceSettings_, error)) {
    reportError(persistenceError, error);
    return state_.revision;
  }

  const auto previous = state_;
  state_ = next;
  publishChange(previous);
  reconcileRuntime();
  return state_.revision;
}

void ConfigService::reconcileRuntime() {
  if (runtimeReconcileInProgress_) {
    runtimeReconcilePending_ = true;
    return;
  }
  runtimeReconcileInProgress_ = true;
  runtimeReconcilePending_ = false;

  const auto previousScheduledAppearanceMode = scheduledAppearanceMode_;
  const auto previousAppearanceNextTransition = appearanceNextTransition_;
  const auto previousAppearanceSunrise = appearanceSunrise_;
  const auto previousAppearanceSunset = appearanceSunset_;
  const auto previousAppearanceStatus = appearanceAutomationStatus_;
  const auto previousNightLightRuntimeState = nightLightRuntimeState_;
  const auto previousNightLightCurrentTemperature =
      nightLightCurrentTemperature_;
  const auto previousNightLightNextTransition = nightLightNextTransition_;
  const auto previousNightLightSunrise = nightLightSunrise_;
  const auto previousNightLightSunset = nightLightSunset_;
  const auto previousNightLightStatus = nightLightStatus_;

  const auto now = QDateTime::currentDateTime();
  const auto automaticAppearanceActive =
      state_.appearanceMode == ConfigValues::automaticAppearanceMode;
  const auto appearanceOwnScheduleActive =
      automaticAppearanceActive &&
      state_.appearanceAutomation.source ==
          ConfigValues::scheduleAppearanceAutomationSource;
  // Match the Legacy HGS dry-schedule behavior: an automatic Night Light
  // schedule continues publishing day/night and transition state while the
  // display filter itself is off. Theme automation can then follow it
  // immediately without first enabling the filter.
  const auto nightLightScheduleActive = state_.nightLight.automatic;

  const auto scheduleNeedsGeoClue = [](const ScheduleConfig &schedule) {
    return schedule.mode == ConfigValues::locationScheduleMode &&
           schedule.locationSource == ConfigValues::geoclueLocationSource;
  };
  const auto geoClueNeeded =
      (appearanceOwnScheduleActive &&
       scheduleNeedsGeoClue(state_.appearanceAutomation.schedule)) ||
      (nightLightScheduleActive &&
       scheduleNeedsGeoClue(state_.nightLight.schedule));
  if (geoClue_) {
    if (geoClueNeeded) {
      geoClue_->start();
    } else if (geoClue_->active() ||
               geoClue_->status() != QStringLiteral("idle")) {
      geoClue_->stop();
    }
  }

  struct ScheduleProjection final {
    AppearanceSchedule::Evaluation evaluation;
    QString status = QStringLiteral("unavailable");
  };
  const auto evaluateSchedule =
      [this, &now](const ScheduleConfig &schedule) -> ScheduleProjection {
    if (schedule.mode == ConfigValues::timeScheduleMode) {
      const auto evaluation = AppearanceSchedule::evaluateTime(
          now, schedule.darkStartMinute, schedule.lightStartMinute);
      return {
          .evaluation = evaluation,
          .status = AppearanceSchedule::statusName(evaluation.status),
      };
    }

    auto hasLocation = false;
    auto latitude = 0.0;
    auto longitude = 0.0;
    if (schedule.locationSource == ConfigValues::manualLocationSource) {
      hasLocation = schedule.hasLocation;
      latitude = schedule.latitude;
      longitude = schedule.longitude;
    } else if (geoClue_ && geoClue_->available()) {
      hasLocation = true;
      latitude = geoClue_->latitude();
      longitude = geoClue_->longitude();
    }
    if (!hasLocation) {
      return {.status = QStringLiteral("waiting-location")};
    }

    const auto evaluation =
        AppearanceSchedule::evaluateLocation(now, latitude, longitude);
    return {
        .evaluation = evaluation,
        .status = AppearanceSchedule::statusName(evaluation.status),
    };
  };

  ScheduleProjection appearanceSchedule;
  if (state_.appearanceAutomation.source ==
      ConfigValues::scheduleAppearanceAutomationSource) {
    appearanceSchedule = evaluateSchedule(state_.appearanceAutomation.schedule);
  }
  ScheduleProjection nightLightSchedule;
  if (state_.nightLight.automatic) {
    nightLightSchedule = evaluateSchedule(state_.nightLight.schedule);
  }

  scheduledAppearanceMode_ = QStringLiteral("unavailable");
  appearanceNextTransition_.clear();
  appearanceSunrise_.clear();
  appearanceSunset_.clear();
  appearanceAutomationStatus_ = QStringLiteral("unavailable");

  const auto applyAppearanceProjection =
      [this](const ScheduleProjection &projection) {
        if (!projection.evaluation.valid) {
          appearanceAutomationStatus_ = projection.status;
          return;
        }
        scheduledAppearanceMode_ = projection.evaluation.isDark
                                       ? ConfigValues::darkAppearanceMode
                                       : ConfigValues::lightAppearanceMode;
        appearanceNextTransition_ =
            dateTimeText(projection.evaluation.nextTransition);
        appearanceSunrise_ = dateTimeText(projection.evaluation.sunrise);
        appearanceSunset_ = dateTimeText(projection.evaluation.sunset);
        appearanceAutomationStatus_ = projection.status;
      };

  if (state_.appearanceAutomation.source ==
      ConfigValues::desktopAppearanceAutomationSource) {
    scheduledAppearanceMode_ = QStringLiteral("system");
    appearanceAutomationStatus_ = QStringLiteral("desktop");
  } else if (state_.appearanceAutomation.source ==
             ConfigValues::scheduleAppearanceAutomationSource) {
    applyAppearanceProjection(appearanceSchedule);
  } else if (state_.nightLight.automatic) {
    applyAppearanceProjection(nightLightSchedule);
  }

  nightLightNextTransition_ =
      nightLightSchedule.evaluation.valid
          ? dateTimeText(nightLightSchedule.evaluation.nextTransition)
          : QString();
  nightLightSunrise_ = nightLightSchedule.evaluation.valid
                           ? dateTimeText(nightLightSchedule.evaluation.sunrise)
                           : QString();
  nightLightSunset_ = nightLightSchedule.evaluation.valid
                          ? dateTimeText(nightLightSchedule.evaluation.sunset)
                          : QString();

  auto targetTemperature = state_.nightLight.nightTemperature;
  auto filterCanRun = state_.nightLight.enabled;
  if (state_.nightLight.automatic) {
    filterCanRun = filterCanRun && nightLightSchedule.evaluation.valid;
    targetTemperature = AppearanceSchedule::targetTemperature(
        nightLightSchedule.evaluation, state_.nightLight.nightTemperature,
        state_.nightLight.dayTemperature,
        state_.nightLight.gradual && state_.nightLight.schedule.mode ==
                                         ConfigValues::locationScheduleMode);
  }

  if (hyprsunsetAvailable_) {
    if (filterCanRun) {
      const auto gradualSolar =
          state_.nightLight.automatic && state_.nightLight.gradual &&
          state_.nightLight.schedule.mode == ConfigValues::locationScheduleMode;
      const auto currentTemperature = hyprsunset_->currentTemperature();
      if (gradualSolar && currentTemperature > 0 &&
          std::abs(static_cast<int>(targetTemperature) - currentTemperature) <
              25) {
        targetTemperature = static_cast<uint>(currentTemperature);
      }
      hyprsunset_->setTemperature(static_cast<int>(targetTemperature));
    }
    hyprsunset_->setEnabled(filterCanRun);
    nightLightRuntimeState_ = hyprsunsetStateName(hyprsunset_->state());
    nightLightCurrentTemperature_ =
        static_cast<uint>(std::max(0, hyprsunset_->currentTemperature()));
  } else {
    if (hyprsunset_) {
      hyprsunset_->setEnabled(false);
    }
    nightLightRuntimeState_ = state_.nightLight.enabled
                                  ? QStringLiteral("unavailable")
                                  : QStringLiteral("disabled");
    nightLightCurrentTemperature_ = 0;
  }

  if (!state_.nightLight.enabled) {
    nightLightStatus_ = QStringLiteral("disabled");
  } else if (!hyprsunsetAvailable_) {
    nightLightStatus_ = QStringLiteral("unavailable");
  } else if (state_.nightLight.automatic &&
             !nightLightSchedule.evaluation.valid) {
    nightLightStatus_ = nightLightSchedule.status;
  } else {
    using State = HyprsunsetController::State;
    switch (hyprsunset_->state()) {
    case State::ExternalDaemon:
      nightLightStatus_ = QStringLiteral("external-daemon");
      break;
    case State::Failed:
      nightLightStatus_ = hyprsunset_->error().isEmpty()
                              ? QStringLiteral("failed")
                              : hyprsunset_->error();
      break;
    case State::Ready:
      nightLightStatus_ =
          !state_.nightLight.automatic || nightLightSchedule.evaluation.isDark
              ? QStringLiteral("night")
              : QStringLiteral("day");
      break;
    case State::Disabled:
    case State::Probing:
    case State::Starting:
    case State::Applying:
    case State::RetryWaiting:
    case State::Stopping:
      nightLightStatus_ = QStringLiteral("applying");
      break;
    }
  }

  automationTimer_.stop();
  const auto scheduleWatchdogNeeded =
      appearanceOwnScheduleActive || nightLightScheduleActive || geoClueNeeded;
  if (scheduleWatchdogNeeded) {
    auto interval = qint64(60000);
    const auto considerRefresh = [&now, &interval](const QDateTime &refresh) {
      if (!refresh.isValid()) {
        return;
      }
      interval =
          std::min(interval, std::max<qint64>(1000, now.msecsTo(refresh)));
    };
    if (appearanceOwnScheduleActive) {
      considerRefresh(appearanceSchedule.evaluation.nextRefresh);
    }
    if (nightLightScheduleActive) {
      considerRefresh(nightLightSchedule.evaluation.nextRefresh);
    }
    automationTimer_.start(static_cast<int>(interval));
  }

  publishRuntimeChange(
      previousScheduledAppearanceMode, previousAppearanceNextTransition,
      previousAppearanceSunrise, previousAppearanceSunset,
      previousAppearanceStatus, previousNightLightRuntimeState,
      previousNightLightCurrentTemperature, previousNightLightNextTransition,
      previousNightLightSunrise, previousNightLightSunset,
      previousNightLightStatus);

  runtimeReconcileInProgress_ = false;
  if (runtimeReconcilePending_) {
    QMetaObject::invokeMethod(this, &ConfigService::reconcileRuntime,
                              Qt::QueuedConnection);
  }
}

void ConfigService::publishRuntimeChange(
    const QString &previousScheduledAppearanceMode,
    const QString &previousAppearanceNextTransition,
    const QString &previousAppearanceSunrise,
    const QString &previousAppearanceSunset,
    const QString &previousAppearanceStatus,
    const QString &previousNightLightRuntimeState,
    const uint previousNightLightCurrentTemperature,
    const QString &previousNightLightNextTransition,
    const QString &previousNightLightSunrise,
    const QString &previousNightLightSunset,
    const QString &previousNightLightStatus) const {
  QVariantMap changed;
  const auto addString = [&changed](const QString &name, const QString &before,
                                    const QString &after) {
    if (before != after) {
      changed.insert(name, after);
    }
  };
  addString(QStringLiteral("ScheduledAppearanceMode"),
            previousScheduledAppearanceMode, scheduledAppearanceMode_);
  addString(QStringLiteral("AppearanceNextTransition"),
            previousAppearanceNextTransition, appearanceNextTransition_);
  addString(QStringLiteral("AppearanceSunrise"), previousAppearanceSunrise,
            appearanceSunrise_);
  addString(QStringLiteral("AppearanceSunset"), previousAppearanceSunset,
            appearanceSunset_);
  addString(QStringLiteral("AppearanceAutomationStatus"),
            previousAppearanceStatus, appearanceAutomationStatus_);
  addString(QStringLiteral("NightLightRuntimeState"),
            previousNightLightRuntimeState, nightLightRuntimeState_);
  if (previousNightLightCurrentTemperature != nightLightCurrentTemperature_) {
    changed.insert(QStringLiteral("NightLightCurrentTemperature"),
                   nightLightCurrentTemperature_);
  }
  addString(QStringLiteral("NightLightNextTransition"),
            previousNightLightNextTransition, nightLightNextTransition_);
  addString(QStringLiteral("NightLightSunrise"), previousNightLightSunrise,
            nightLightSunrise_);
  addString(QStringLiteral("NightLightSunset"), previousNightLightSunset,
            nightLightSunset_);
  addString(QStringLiteral("NightLightStatus"), previousNightLightStatus,
            nightLightStatus_);
  if (changed.isEmpty()) {
    return;
  }

  auto signal = QDBusMessage::createSignal(
      configPath, QStringLiteral("org.freedesktop.DBus.Properties"),
      QStringLiteral("PropertiesChanged"));
  signal.setArguments({configInterface, changed, QStringList()});
  if (!connection_.send(signal)) {
    qWarning() << "Failed to publish appearance runtime change";
  }
}

void ConfigService::attemptLegacyWorkspaceRetirement() {
  if (!legacyWorkspaceRetirementAuthorized_ ||
      !legacyWorkspaceRetirementPending_) {
    return;
  }

  QString error;
  if (store_.retireLegacyWorkspaceSettings(state_, error)) {
    legacyWorkspaceRetirementPending_ = false;
    legacyWorkspaceSettings_.reset();
    legacyWorkspaceRetirementTimer_.stop();
    return;
  }

  qWarning().noquote()
      << QStringLiteral("Failed to retire migrated workspace settings: %1")
             .arg(error);
  if (!legacyWorkspaceRetirementTimer_.isActive()) {
    legacyWorkspaceRetirementTimer_.start();
  }
}

void ConfigService::reportError(const QString &name,
                                const QString &message) const {
  if (calledFromDBus()) {
    sendErrorReply(name, message);
  }
}

void ConfigService::publishChange(const ConfigState &previous) const {
  QVariantMap changed;
  if (state_.barHeight != previous.barHeight) {
    changed.insert(QStringLiteral("BarHeight"), state_.barHeight);
  }
  if (state_.shellBorderEnabled != previous.shellBorderEnabled ||
      state_.shellBorderWidth != previous.shellBorderWidth ||
      state_.shellBorderRadius != previous.shellBorderRadius ||
      state_.syncHyprlandWindowBorders != previous.syncHyprlandWindowBorders) {
    changed.insert(QStringLiteral("ShellBorderEnabled"),
                   state_.shellBorderEnabled);
    changed.insert(QStringLiteral("ShellBorderWidth"), state_.shellBorderWidth);
    changed.insert(QStringLiteral("ShellBorderRadius"),
                   state_.shellBorderRadius);
    changed.insert(QStringLiteral("SyncHyprlandWindowBorders"),
                   state_.syncHyprlandWindowBorders);
  }
  if (state_.shellInnerSpacing != previous.shellInnerSpacing ||
      state_.shellOuterSpacing != previous.shellOuterSpacing ||
      state_.syncHyprlandWindowSpacing != previous.syncHyprlandWindowSpacing) {
    changed.insert(QStringLiteral("ShellInnerSpacing"),
                   state_.shellInnerSpacing);
    changed.insert(QStringLiteral("ShellOuterSpacing"),
                   state_.shellOuterSpacing);
    changed.insert(QStringLiteral("SyncHyprlandWindowSpacing"),
                   state_.syncHyprlandWindowSpacing);
  }
  if (state_.appearanceMode != previous.appearanceMode) {
    changed.insert(QStringLiteral("AppearanceMode"), state_.appearanceMode);
  }
  if (state_.appearanceAutomation != previous.appearanceAutomation) {
    const auto &automation = state_.appearanceAutomation;
    changed.insert(QStringLiteral("AppearanceAutomationSource"),
                   automation.source);
    changed.insert(QStringLiteral("AppearanceScheduleMode"),
                   automation.schedule.mode);
    changed.insert(QStringLiteral("AppearanceDarkStartMinute"),
                   automation.schedule.darkStartMinute);
    changed.insert(QStringLiteral("AppearanceLightStartMinute"),
                   automation.schedule.lightStartMinute);
    changed.insert(QStringLiteral("AppearanceLocationSource"),
                   automation.schedule.locationSource);
    changed.insert(QStringLiteral("AppearanceHasLocation"),
                   automation.schedule.hasLocation);
    changed.insert(QStringLiteral("AppearanceLatitude"),
                   automation.schedule.latitude);
    changed.insert(QStringLiteral("AppearanceLongitude"),
                   automation.schedule.longitude);
  }
  if (state_.nightLight != previous.nightLight) {
    const auto &nightLight = state_.nightLight;
    changed.insert(QStringLiteral("NightLightEnabled"), nightLight.enabled);
    changed.insert(QStringLiteral("NightLightAutomatic"), nightLight.automatic);
    changed.insert(QStringLiteral("NightLightScheduleMode"),
                   nightLight.schedule.mode);
    changed.insert(QStringLiteral("NightLightDarkStartMinute"),
                   nightLight.schedule.darkStartMinute);
    changed.insert(QStringLiteral("NightLightLightStartMinute"),
                   nightLight.schedule.lightStartMinute);
    changed.insert(QStringLiteral("NightLightLocationSource"),
                   nightLight.schedule.locationSource);
    changed.insert(QStringLiteral("NightLightHasLocation"),
                   nightLight.schedule.hasLocation);
    changed.insert(QStringLiteral("NightLightLatitude"),
                   nightLight.schedule.latitude);
    changed.insert(QStringLiteral("NightLightLongitude"),
                   nightLight.schedule.longitude);
    changed.insert(QStringLiteral("NightLightTemperature"),
                   nightLight.nightTemperature);
    changed.insert(QStringLiteral("NightLightDayTemperature"),
                   nightLight.dayTemperature);
    changed.insert(QStringLiteral("NightLightGradual"), nightLight.gradual);
  }
  changed.insert(QStringLiteral("Revision"),
                 QVariant::fromValue<qulonglong>(state_.revision));

  auto signal = QDBusMessage::createSignal(
      configPath, QStringLiteral("org.freedesktop.DBus.Properties"),
      QStringLiteral("PropertiesChanged"));
  signal.setArguments({configInterface, changed, QStringList()});

  if (!connection_.send(signal)) {
    qWarning() << "Failed to publish configuration change";
  }
}

} // namespace HyprShelld
