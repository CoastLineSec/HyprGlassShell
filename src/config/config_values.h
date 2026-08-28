#pragma once

#include <QString>

namespace HyprShelld::ConfigValues {

inline constexpr unsigned int minimumBarHeight = 24U;
inline constexpr unsigned int maximumBarHeight = 96U;
inline constexpr unsigned int defaultBarHeight = 40U;

inline constexpr bool defaultShellBorderEnabled = true;
inline constexpr unsigned int minimumShellBorderWidth = 0U;
inline constexpr unsigned int maximumShellBorderWidth = 20U;
inline constexpr unsigned int defaultShellBorderWidth = 1U;
inline constexpr unsigned int minimumShellBorderRadius = 0U;
inline constexpr unsigned int maximumShellBorderRadius = 20U;
inline constexpr unsigned int defaultShellBorderRadius = 15U;
inline constexpr bool defaultSyncHyprlandWindowBorders = true;

inline constexpr unsigned int minimumShellSpacing = 0U;
inline constexpr unsigned int maximumShellSpacing = 32U;
inline constexpr unsigned int defaultShellInnerSpacing = 8U;
inline constexpr unsigned int defaultShellOuterSpacing = 12U;
inline constexpr bool defaultSyncHyprlandWindowSpacing = true;

inline const QString automaticAppearanceMode = QStringLiteral("automatic");
inline const QString lightAppearanceMode = QStringLiteral("light");
inline const QString darkAppearanceMode = QStringLiteral("dark");
// Preserve the established dark presentation when upgrading snapshots that
// predate the appearance-mode setting.
inline const QString defaultAppearanceMode = darkAppearanceMode;

inline const QString desktopAppearanceAutomationSource =
    QStringLiteral("desktop");
inline const QString scheduleAppearanceAutomationSource =
    QStringLiteral("schedule");
inline const QString nightLightAppearanceAutomationSource =
    QStringLiteral("night-light");
inline const QString defaultAppearanceAutomationSource =
    desktopAppearanceAutomationSource;

inline const QString timeScheduleMode = QStringLiteral("time");
inline const QString locationScheduleMode = QStringLiteral("location");
inline const QString defaultScheduleMode = timeScheduleMode;

inline const QString manualLocationSource = QStringLiteral("manual");
inline const QString geoclueLocationSource = QStringLiteral("geoclue");
inline const QString defaultLocationSource = manualLocationSource;

inline constexpr unsigned int minutesPerDay = 24U * 60U;
inline constexpr unsigned int maximumScheduleMinute = minutesPerDay - 1U;
inline constexpr unsigned int defaultAppearanceDarkStartMinute = 18U * 60U;
inline constexpr unsigned int defaultAppearanceLightStartMinute = 6U * 60U;
inline constexpr unsigned int defaultNightLightDarkStartMinute = 20U * 60U;
inline constexpr unsigned int defaultNightLightLightStartMinute = 6U * 60U;

inline constexpr unsigned int minimumNightLightTemperature = 2500U;
inline constexpr unsigned int maximumNightTemperature = 6000U;
inline constexpr unsigned int maximumNightLightTemperature = 10000U;
inline constexpr unsigned int defaultNightLightTemperature = 4000U;
inline constexpr unsigned int defaultNightLightDayTemperature = 6500U;

[[nodiscard]] inline bool isValidAppearanceMode(const QString &mode) {
  return mode == automaticAppearanceMode || mode == lightAppearanceMode ||
         mode == darkAppearanceMode;
}

[[nodiscard]] inline bool
isValidAppearanceAutomationSource(const QString &source) {
  return source == desktopAppearanceAutomationSource ||
         source == scheduleAppearanceAutomationSource ||
         source == nightLightAppearanceAutomationSource;
}

[[nodiscard]] inline bool isValidScheduleMode(const QString &mode) {
  return mode == timeScheduleMode || mode == locationScheduleMode;
}

[[nodiscard]] inline bool isValidLocationSource(const QString &source) {
  return source == manualLocationSource || source == geoclueLocationSource;
}

} // namespace HyprShelld::ConfigValues
