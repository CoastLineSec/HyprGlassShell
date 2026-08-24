#pragma once

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

} // namespace HyprShelld::ConfigValues
