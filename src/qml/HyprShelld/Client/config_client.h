#pragma once

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class OrgHyprshelldConfig1Interface;
class QDBusServiceWatcher;

namespace HyprShelld {

class ConfigClient final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool available READ available NOTIFY availableChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(uint barHeight READ barHeight NOTIFY barHeightChanged)
  Q_PROPERTY(bool shellBorderEnabled READ shellBorderEnabled NOTIFY
                 sharedBorderChanged)
  Q_PROPERTY(
      uint shellBorderWidth READ shellBorderWidth NOTIFY sharedBorderChanged)
  Q_PROPERTY(
      uint shellBorderRadius READ shellBorderRadius NOTIFY sharedBorderChanged)
  Q_PROPERTY(bool syncHyprlandWindowBorders READ syncHyprlandWindowBorders
                 NOTIFY sharedBorderChanged)
  Q_PROPERTY(
      uint shellInnerSpacing READ shellInnerSpacing NOTIFY sharedSpacingChanged)
  Q_PROPERTY(
      uint shellOuterSpacing READ shellOuterSpacing NOTIFY sharedSpacingChanged)
  Q_PROPERTY(bool syncHyprlandWindowSpacing READ syncHyprlandWindowSpacing
                 NOTIFY sharedSpacingChanged)
  Q_PROPERTY(
      QString appearanceMode READ appearanceMode NOTIFY appearanceModeChanged)
  Q_PROPERTY(QString appearanceAutomationSource READ appearanceAutomationSource
                 NOTIFY appearanceAutomationChanged)
  Q_PROPERTY(QString appearanceScheduleMode READ appearanceScheduleMode NOTIFY
                 appearanceAutomationChanged)
  Q_PROPERTY(uint appearanceDarkStartMinute READ appearanceDarkStartMinute
                 NOTIFY appearanceAutomationChanged)
  Q_PROPERTY(uint appearanceLightStartMinute READ appearanceLightStartMinute
                 NOTIFY appearanceAutomationChanged)
  Q_PROPERTY(QString appearanceLocationSource READ appearanceLocationSource
                 NOTIFY appearanceAutomationChanged)
  Q_PROPERTY(bool appearanceHasLocation READ appearanceHasLocation NOTIFY
                 appearanceAutomationChanged)
  Q_PROPERTY(double appearanceLatitude READ appearanceLatitude NOTIFY
                 appearanceAutomationChanged)
  Q_PROPERTY(double appearanceLongitude READ appearanceLongitude NOTIFY
                 appearanceAutomationChanged)
  Q_PROPERTY(QString scheduledAppearanceMode READ scheduledAppearanceMode NOTIFY
                 appearanceRuntimeChanged)
  Q_PROPERTY(QString appearanceNextTransition READ appearanceNextTransition
                 NOTIFY appearanceRuntimeChanged)
  Q_PROPERTY(QString appearanceSunrise READ appearanceSunrise NOTIFY
                 appearanceRuntimeChanged)
  Q_PROPERTY(QString appearanceSunset READ appearanceSunset NOTIFY
                 appearanceRuntimeChanged)
  Q_PROPERTY(QString appearanceAutomationStatus READ appearanceAutomationStatus
                 NOTIFY appearanceRuntimeChanged)
  Q_PROPERTY(
      bool nightLightEnabled READ nightLightEnabled NOTIFY nightLightChanged)
  Q_PROPERTY(bool nightLightAutomatic READ nightLightAutomatic NOTIFY
                 nightLightChanged)
  Q_PROPERTY(QString nightLightScheduleMode READ nightLightScheduleMode NOTIFY
                 nightLightChanged)
  Q_PROPERTY(uint nightLightDarkStartMinute READ nightLightDarkStartMinute
                 NOTIFY nightLightChanged)
  Q_PROPERTY(uint nightLightLightStartMinute READ nightLightLightStartMinute
                 NOTIFY nightLightChanged)
  Q_PROPERTY(QString nightLightLocationSource READ nightLightLocationSource
                 NOTIFY nightLightChanged)
  Q_PROPERTY(bool nightLightHasLocation READ nightLightHasLocation NOTIFY
                 nightLightChanged)
  Q_PROPERTY(double nightLightLatitude READ nightLightLatitude NOTIFY
                 nightLightChanged)
  Q_PROPERTY(double nightLightLongitude READ nightLightLongitude NOTIFY
                 nightLightChanged)
  Q_PROPERTY(uint nightLightTemperature READ nightLightTemperature NOTIFY
                 nightLightChanged)
  Q_PROPERTY(uint nightLightDayTemperature READ nightLightDayTemperature NOTIFY
                 nightLightChanged)
  Q_PROPERTY(
      bool nightLightGradual READ nightLightGradual NOTIFY nightLightChanged)
  Q_PROPERTY(bool hyprsunsetAvailable READ hyprsunsetAvailable NOTIFY
                 nightLightRuntimeChanged)
  Q_PROPERTY(QString nightLightRuntimeState READ nightLightRuntimeState NOTIFY
                 nightLightRuntimeChanged)
  Q_PROPERTY(uint nightLightCurrentTemperature READ nightLightCurrentTemperature
                 NOTIFY nightLightRuntimeChanged)
  Q_PROPERTY(QString nightLightNextTransition READ nightLightNextTransition
                 NOTIFY nightLightRuntimeChanged)
  Q_PROPERTY(QString nightLightSunrise READ nightLightSunrise NOTIFY
                 nightLightRuntimeChanged)
  Q_PROPERTY(QString nightLightSunset READ nightLightSunset NOTIFY
                 nightLightRuntimeChanged)
  Q_PROPERTY(QString nightLightStatus READ nightLightStatus NOTIFY
                 nightLightRuntimeChanged)
  Q_PROPERTY(qulonglong revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(QString revisionToken READ revisionToken NOTIFY revisionChanged)
  Q_PROPERTY(
      QString recoveryState READ recoveryState NOTIFY recoveryStateChanged)
  Q_PROPERTY(uint minimumBarHeight READ minimumBarHeight CONSTANT)
  Q_PROPERTY(uint maximumBarHeight READ maximumBarHeight CONSTANT)
  Q_PROPERTY(uint defaultBarHeight READ defaultBarHeight CONSTANT)
  Q_PROPERTY(
      bool defaultShellBorderEnabled READ defaultShellBorderEnabled CONSTANT)
  Q_PROPERTY(uint minimumShellBorderWidth READ minimumShellBorderWidth CONSTANT)
  Q_PROPERTY(uint maximumShellBorderWidth READ maximumShellBorderWidth CONSTANT)
  Q_PROPERTY(uint defaultShellBorderWidth READ defaultShellBorderWidth CONSTANT)
  Q_PROPERTY(
      uint minimumShellBorderRadius READ minimumShellBorderRadius CONSTANT)
  Q_PROPERTY(
      uint maximumShellBorderRadius READ maximumShellBorderRadius CONSTANT)
  Q_PROPERTY(
      uint defaultShellBorderRadius READ defaultShellBorderRadius CONSTANT)
  Q_PROPERTY(bool defaultSyncHyprlandWindowBorders READ
                 defaultSyncHyprlandWindowBorders CONSTANT)
  Q_PROPERTY(uint minimumShellSpacing READ minimumShellSpacing CONSTANT)
  Q_PROPERTY(uint maximumShellSpacing READ maximumShellSpacing CONSTANT)
  Q_PROPERTY(
      uint defaultShellInnerSpacing READ defaultShellInnerSpacing CONSTANT)
  Q_PROPERTY(
      uint defaultShellOuterSpacing READ defaultShellOuterSpacing CONSTANT)
  Q_PROPERTY(bool defaultSyncHyprlandWindowSpacing READ
                 defaultSyncHyprlandWindowSpacing CONSTANT)
  Q_PROPERTY(QString defaultAppearanceMode READ defaultAppearanceMode CONSTANT)
  Q_PROPERTY(QString lastErrorName READ lastErrorName NOTIFY lastErrorChanged)
  Q_PROPERTY(QString lastErrorOperation READ lastErrorOperation NOTIFY
                 lastErrorChanged)
  Q_PROPERTY(
      QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorChanged)

public:
  explicit ConfigClient(QObject *parent = nullptr);
  ConfigClient(QDBusConnection connection, QObject *parent);

  [[nodiscard]] bool available() const;
  [[nodiscard]] bool busy() const;
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
  [[nodiscard]] QString revisionToken() const;
  [[nodiscard]] QString recoveryState() const;
  [[nodiscard]] uint minimumBarHeight() const;
  [[nodiscard]] uint maximumBarHeight() const;
  [[nodiscard]] uint defaultBarHeight() const;
  [[nodiscard]] bool defaultShellBorderEnabled() const;
  [[nodiscard]] uint minimumShellBorderWidth() const;
  [[nodiscard]] uint maximumShellBorderWidth() const;
  [[nodiscard]] uint defaultShellBorderWidth() const;
  [[nodiscard]] uint minimumShellBorderRadius() const;
  [[nodiscard]] uint maximumShellBorderRadius() const;
  [[nodiscard]] uint defaultShellBorderRadius() const;
  [[nodiscard]] bool defaultSyncHyprlandWindowBorders() const;
  [[nodiscard]] uint minimumShellSpacing() const;
  [[nodiscard]] uint maximumShellSpacing() const;
  [[nodiscard]] uint defaultShellInnerSpacing() const;
  [[nodiscard]] uint defaultShellOuterSpacing() const;
  [[nodiscard]] bool defaultSyncHyprlandWindowSpacing() const;
  [[nodiscard]] QString defaultAppearanceMode() const;
  [[nodiscard]] QString lastErrorName() const;
  [[nodiscard]] QString lastErrorOperation() const;
  [[nodiscard]] QString lastErrorMessage() const;

  Q_INVOKABLE void setBarHeight(uint height);
  Q_INVOKABLE void resetBarHeight();
  Q_INVOKABLE void setSharedBorder(bool enabled, uint width, uint radius,
                                   bool syncHyprlandWindowBorders);
  Q_INVOKABLE void resetSharedBorder();
  Q_INVOKABLE void setSharedSpacing(uint inner, uint outer,
                                    bool syncHyprlandWindowSpacing);
  Q_INVOKABLE void resetSharedSpacing();
  Q_INVOKABLE void setAppearanceMode(const QString &mode);
  Q_INVOKABLE void resetAppearanceMode();
  Q_INVOKABLE void
  setAppearanceAutomation(const QString &source, const QString &scheduleMode,
                          uint darkStartMinute, uint lightStartMinute,
                          const QString &locationSource, bool hasLocation,
                          double latitude, double longitude);
  Q_INVOKABLE void resetAppearanceAutomation();
  Q_INVOKABLE void
  setNightLightSettings(bool enabled, bool automatic,
                        const QString &scheduleMode, uint darkStartMinute,
                        uint lightStartMinute, const QString &locationSource,
                        bool hasLocation, double latitude, double longitude,
                        uint nightTemperature, uint dayTemperature,
                        bool gradual);
  Q_INVOKABLE void resetNightLightSettings();
  Q_INVOKABLE void clearError();

signals:
  void availableChanged();
  void busyChanged();
  void barHeightChanged();
  void sharedBorderChanged();
  void sharedSpacingChanged();
  void appearanceModeChanged();
  void appearanceAutomationChanged();
  void appearanceRuntimeChanged();
  void nightLightChanged();
  void nightLightRuntimeChanged();
  void revisionChanged();
  void recoveryStateChanged();
  void lastErrorChanged();
  void operationFailed(const QString &name, const QString &message);

private slots:
  void propertiesChanged(const QString &interfaceName,
                         const QVariantMap &changed,
                         const QStringList &invalidated);
  void serviceOwnerChanged(const QString &name, const QString &oldOwner,
                           const QString &newOwner);

private:
  void refresh();
  [[nodiscard]] bool applyProperties(const QVariantMap &properties,
                                     bool requireComplete);
  void beginMutation(const QDBusPendingCall &call, const QString &operation);
  void setAvailable(bool available);
  void setError(const QString &name, const QString &message,
                const QString &operation);

  QDBusConnection connection_;
  OrgHyprshelldConfig1Interface *interface_ = nullptr;
  QDBusServiceWatcher *serviceWatcher_ = nullptr;
  quint64 ownerGeneration_ = 0;
  int pendingOperations_ = 0;
  bool available_ = false;
  bool projectionEstablished_ = false;
  uint barHeight_ = 0;
  bool shellBorderEnabled_ = true;
  uint shellBorderWidth_ = 0;
  uint shellBorderRadius_ = 0;
  bool syncHyprlandWindowBorders_ = true;
  uint shellInnerSpacing_ = 0;
  uint shellOuterSpacing_ = 0;
  bool syncHyprlandWindowSpacing_ = true;
  QString appearanceMode_;
  QString appearanceAutomationSource_;
  QString appearanceScheduleMode_;
  uint appearanceDarkStartMinute_ = 0;
  uint appearanceLightStartMinute_ = 0;
  QString appearanceLocationSource_;
  bool appearanceHasLocation_ = false;
  double appearanceLatitude_ = 0.0;
  double appearanceLongitude_ = 0.0;
  QString scheduledAppearanceMode_;
  QString appearanceNextTransition_;
  QString appearanceSunrise_;
  QString appearanceSunset_;
  QString appearanceAutomationStatus_;
  bool nightLightEnabled_ = false;
  bool nightLightAutomatic_ = true;
  QString nightLightScheduleMode_;
  uint nightLightDarkStartMinute_ = 0;
  uint nightLightLightStartMinute_ = 0;
  QString nightLightLocationSource_;
  bool nightLightHasLocation_ = false;
  double nightLightLatitude_ = 0.0;
  double nightLightLongitude_ = 0.0;
  uint nightLightTemperature_ = 0;
  uint nightLightDayTemperature_ = 0;
  bool nightLightGradual_ = true;
  bool hyprsunsetAvailable_ = false;
  QString nightLightRuntimeState_;
  uint nightLightCurrentTemperature_ = 0;
  QString nightLightNextTransition_;
  QString nightLightSunrise_;
  QString nightLightSunset_;
  QString nightLightStatus_;
  qulonglong revision_ = 0;
  QString recoveryState_;
  QString lastErrorName_;
  QString lastErrorOperation_;
  QString lastErrorMessage_;
};

} // namespace HyprShelld
