#pragma once

#include "config_store.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

namespace HyprShelld {

class GeoClueClient;
class HyprsunsetController;

class ConfigService final : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_PROPERTY(uint BarHeight READ barHeight)
  Q_PROPERTY(bool ShellBorderEnabled READ shellBorderEnabled)
  Q_PROPERTY(uint ShellBorderWidth READ shellBorderWidth)
  Q_PROPERTY(uint ShellBorderRadius READ shellBorderRadius)
  Q_PROPERTY(bool SyncHyprlandWindowBorders READ syncHyprlandWindowBorders)
  Q_PROPERTY(uint ShellInnerSpacing READ shellInnerSpacing)
  Q_PROPERTY(uint ShellOuterSpacing READ shellOuterSpacing)
  Q_PROPERTY(bool SyncHyprlandWindowSpacing READ syncHyprlandWindowSpacing)
  Q_PROPERTY(QString AppearanceMode READ appearanceMode)
  Q_PROPERTY(QString AppearanceAutomationSource READ appearanceAutomationSource)
  Q_PROPERTY(QString AppearanceScheduleMode READ appearanceScheduleMode)
  Q_PROPERTY(uint AppearanceDarkStartMinute READ appearanceDarkStartMinute)
  Q_PROPERTY(uint AppearanceLightStartMinute READ appearanceLightStartMinute)
  Q_PROPERTY(QString AppearanceLocationSource READ appearanceLocationSource)
  Q_PROPERTY(bool AppearanceHasLocation READ appearanceHasLocation)
  Q_PROPERTY(double AppearanceLatitude READ appearanceLatitude)
  Q_PROPERTY(double AppearanceLongitude READ appearanceLongitude)
  Q_PROPERTY(QString ScheduledAppearanceMode READ scheduledAppearanceMode)
  Q_PROPERTY(QString AppearanceNextTransition READ appearanceNextTransition)
  Q_PROPERTY(QString AppearanceSunrise READ appearanceSunrise)
  Q_PROPERTY(QString AppearanceSunset READ appearanceSunset)
  Q_PROPERTY(QString AppearanceAutomationStatus READ appearanceAutomationStatus)
  Q_PROPERTY(bool NightLightEnabled READ nightLightEnabled)
  Q_PROPERTY(bool NightLightAutomatic READ nightLightAutomatic)
  Q_PROPERTY(QString NightLightScheduleMode READ nightLightScheduleMode)
  Q_PROPERTY(uint NightLightDarkStartMinute READ nightLightDarkStartMinute)
  Q_PROPERTY(uint NightLightLightStartMinute READ nightLightLightStartMinute)
  Q_PROPERTY(QString NightLightLocationSource READ nightLightLocationSource)
  Q_PROPERTY(bool NightLightHasLocation READ nightLightHasLocation)
  Q_PROPERTY(double NightLightLatitude READ nightLightLatitude)
  Q_PROPERTY(double NightLightLongitude READ nightLightLongitude)
  Q_PROPERTY(uint NightLightTemperature READ nightLightTemperature)
  Q_PROPERTY(uint NightLightDayTemperature READ nightLightDayTemperature)
  Q_PROPERTY(bool NightLightGradual READ nightLightGradual)
  Q_PROPERTY(bool HyprsunsetAvailable READ hyprsunsetAvailable)
  Q_PROPERTY(QString NightLightRuntimeState READ nightLightRuntimeState)
  Q_PROPERTY(
      uint NightLightCurrentTemperature READ nightLightCurrentTemperature)
  Q_PROPERTY(QString NightLightNextTransition READ nightLightNextTransition)
  Q_PROPERTY(QString NightLightSunrise READ nightLightSunrise)
  Q_PROPERTY(QString NightLightSunset READ nightLightSunset)
  Q_PROPERTY(QString NightLightStatus READ nightLightStatus)
  Q_PROPERTY(qulonglong Revision READ revision)
  Q_PROPERTY(QString RecoveryState READ recoveryState)

public:
  ConfigService(ConfigStore store, const ConfigLoadResult &loaded,
                QDBusConnection connection, QObject *parent = nullptr);

  [[nodiscard]] uint barHeight() const;
  [[nodiscard]] bool shellBorderEnabled() const;
  [[nodiscard]] uint shellBorderWidth() const;
  [[nodiscard]] uint shellBorderRadius() const;
  [[nodiscard]] bool syncHyprlandWindowBorders() const;
  [[nodiscard]] uint shellInnerSpacing() const;
  [[nodiscard]] uint shellOuterSpacing() const;
  [[nodiscard]] bool syncHyprlandWindowSpacing() const;
  [[nodiscard]] QString appearanceMode() const;
  [[nodiscard]] QString appearanceAutomationSource() const;
  [[nodiscard]] QString appearanceScheduleMode() const;
  [[nodiscard]] uint appearanceDarkStartMinute() const;
  [[nodiscard]] uint appearanceLightStartMinute() const;
  [[nodiscard]] QString appearanceLocationSource() const;
  [[nodiscard]] bool appearanceHasLocation() const;
  [[nodiscard]] double appearanceLatitude() const;
  [[nodiscard]] double appearanceLongitude() const;
  [[nodiscard]] QString scheduledAppearanceMode() const;
  [[nodiscard]] QString appearanceNextTransition() const;
  [[nodiscard]] QString appearanceSunrise() const;
  [[nodiscard]] QString appearanceSunset() const;
  [[nodiscard]] QString appearanceAutomationStatus() const;
  [[nodiscard]] bool nightLightEnabled() const;
  [[nodiscard]] bool nightLightAutomatic() const;
  [[nodiscard]] QString nightLightScheduleMode() const;
  [[nodiscard]] uint nightLightDarkStartMinute() const;
  [[nodiscard]] uint nightLightLightStartMinute() const;
  [[nodiscard]] QString nightLightLocationSource() const;
  [[nodiscard]] bool nightLightHasLocation() const;
  [[nodiscard]] double nightLightLatitude() const;
  [[nodiscard]] double nightLightLongitude() const;
  [[nodiscard]] uint nightLightTemperature() const;
  [[nodiscard]] uint nightLightDayTemperature() const;
  [[nodiscard]] bool nightLightGradual() const;
  [[nodiscard]] bool hyprsunsetAvailable() const;
  [[nodiscard]] QString nightLightRuntimeState() const;
  [[nodiscard]] uint nightLightCurrentTemperature() const;
  [[nodiscard]] QString nightLightNextTransition() const;
  [[nodiscard]] QString nightLightSunrise() const;
  [[nodiscard]] QString nightLightSunset() const;
  [[nodiscard]] QString nightLightStatus() const;
  [[nodiscard]] qulonglong revision() const;
  [[nodiscard]] QString recoveryState() const;

  void authorizeLegacyWorkspaceRetirement();
  void attachAppearanceRuntime(GeoClueClient *geoClue,
                               HyprsunsetController *hyprsunset);

public slots:
  qulonglong SetBarHeight(uint height);
  qulonglong ResetBarHeight();
  qulonglong SetSharedBorder(bool enabled, uint width, uint radius,
                             bool syncHyprlandWindowBorders);
  qulonglong ResetSharedBorder();
  qulonglong SetSharedSpacing(uint inner, uint outer,
                              bool syncHyprlandWindowSpacing);
  qulonglong ResetSharedSpacing();
  qulonglong SetAppearanceMode(const QString &mode);
  qulonglong ResetAppearanceMode();
  qulonglong
  SetAppearanceAutomation(const QString &source, const QString &scheduleMode,
                          uint darkStartMinute, uint lightStartMinute,
                          const QString &locationSource, bool hasLocation,
                          double latitude, double longitude);
  qulonglong ResetAppearanceAutomation();
  qulonglong SetNightLightSettings(bool enabled, bool automatic,
                                   const QString &scheduleMode,
                                   uint darkStartMinute, uint lightStartMinute,
                                   const QString &locationSource,
                                   bool hasLocation, double latitude,
                                   double longitude, uint nightTemperature,
                                   uint dayTemperature, bool gradual);
  qulonglong ResetNightLightSettings();

private:
  qulonglong setBarHeight(uint height);
  qulonglong setSharedBorder(bool enabled, uint width, uint radius,
                             bool syncHyprlandWindowBorders);
  qulonglong setSharedSpacing(uint inner, uint outer,
                              bool syncHyprlandWindowSpacing);
  qulonglong setAppearanceMode(const QString &mode);
  qulonglong
  setAppearanceAutomation(const AppearanceAutomationConfig &automation);
  qulonglong setNightLightSettings(const NightLightConfig &nightLight);
  [[nodiscard]] bool validateSchedule(const ScheduleConfig &schedule,
                                      QString &message) const;
  void reconcileRuntime();
  void publishRuntimeChange(const QString &previousScheduledAppearanceMode,
                            const QString &previousAppearanceNextTransition,
                            const QString &previousAppearanceSunrise,
                            const QString &previousAppearanceSunset,
                            const QString &previousAppearanceStatus,
                            const QString &previousNightLightRuntimeState,
                            uint previousNightLightCurrentTemperature,
                            const QString &previousNightLightNextTransition,
                            const QString &previousNightLightSunrise,
                            const QString &previousNightLightSunset,
                            const QString &previousNightLightStatus) const;
  void attemptLegacyWorkspaceRetirement();
  void reportError(const QString &name, const QString &message) const;
  void publishChange(const ConfigState &previous) const;

  ConfigStore store_;
  ConfigState state_;
  QString recoveryState_;
  QDBusConnection connection_;
  std::optional<LegacyWorkspaceSettings> legacyWorkspaceSettings_;
  bool legacyWorkspaceRetirementPending_ = false;
  bool legacyWorkspaceRetirementAuthorized_ = false;
  QTimer legacyWorkspaceRetirementTimer_;
  QTimer automationTimer_;
  QPointer<GeoClueClient> geoClue_;
  QPointer<HyprsunsetController> hyprsunset_;
  QString scheduledAppearanceMode_ = QStringLiteral("system");
  QString appearanceNextTransition_;
  QString appearanceSunrise_;
  QString appearanceSunset_;
  QString appearanceAutomationStatus_ = QStringLiteral("desktop");
  bool hyprsunsetAvailable_ = false;
  QString nightLightRuntimeState_ = QStringLiteral("disabled");
  uint nightLightCurrentTemperature_ = 0;
  QString nightLightNextTransition_;
  QString nightLightSunrise_;
  QString nightLightSunset_;
  QString nightLightStatus_ = QStringLiteral("disabled");
  bool runtimeReconcileInProgress_ = false;
  bool runtimeReconcilePending_ = false;
};

} // namespace HyprShelld
