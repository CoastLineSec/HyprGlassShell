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

[[nodiscard]] inline bool isValidAppearanceMode(const QString &mode)
{
    return mode == automaticAppearanceMode
        || mode == lightAppearanceMode
        || mode == darkAppearanceMode;
}

} // namespace HyprShelld::ConfigValues
