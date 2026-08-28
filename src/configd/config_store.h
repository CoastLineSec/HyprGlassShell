#pragma once

#include "config/config_values.h"
#include "legacy_workspace_settings.h"

#include <QString>
#include <QtTypes>

#include <optional>

namespace HyprShelld {

struct ScheduleConfig final {
  QString mode = ConfigValues::defaultScheduleMode;
  quint32 darkStartMinute = ConfigValues::defaultAppearanceDarkStartMinute;
  quint32 lightStartMinute = ConfigValues::defaultAppearanceLightStartMinute;
  QString locationSource = ConfigValues::defaultLocationSource;
  bool hasLocation = false;
  double latitude = 0.0;
  double longitude = 0.0;

  friend bool operator==(const ScheduleConfig &,
                         const ScheduleConfig &) = default;
};

struct AppearanceAutomationConfig final {
  QString source = ConfigValues::defaultAppearanceAutomationSource;
  ScheduleConfig schedule;

  friend bool operator==(const AppearanceAutomationConfig &,
                         const AppearanceAutomationConfig &) = default;
};

struct NightLightConfig final {
  bool enabled = false;
  bool automatic = true;
  ScheduleConfig schedule{
      .mode = ConfigValues::defaultScheduleMode,
      .darkStartMinute = ConfigValues::defaultNightLightDarkStartMinute,
      .lightStartMinute = ConfigValues::defaultNightLightLightStartMinute,
      .locationSource = ConfigValues::defaultLocationSource,
      .hasLocation = false,
      .latitude = 0.0,
      .longitude = 0.0,
  };
  quint32 nightTemperature = ConfigValues::defaultNightLightTemperature;
  quint32 dayTemperature = ConfigValues::defaultNightLightDayTemperature;
  bool gradual = true;

  friend bool operator==(const NightLightConfig &,
                         const NightLightConfig &) = default;
};

struct ConfigState final {
  quint32 barHeight = ConfigValues::defaultBarHeight;
  bool shellBorderEnabled = ConfigValues::defaultShellBorderEnabled;
  quint32 shellBorderWidth = ConfigValues::defaultShellBorderWidth;
  quint32 shellBorderRadius = ConfigValues::defaultShellBorderRadius;
  bool syncHyprlandWindowBorders =
      ConfigValues::defaultSyncHyprlandWindowBorders;
  quint32 shellInnerSpacing = ConfigValues::defaultShellInnerSpacing;
  quint32 shellOuterSpacing = ConfigValues::defaultShellOuterSpacing;
  bool syncHyprlandWindowSpacing =
      ConfigValues::defaultSyncHyprlandWindowSpacing;
  QString appearanceMode = ConfigValues::defaultAppearanceMode;
  AppearanceAutomationConfig appearanceAutomation;
  NightLightConfig nightLight;
  quint64 revision = 0;

  friend bool operator==(const ConfigState &, const ConfigState &) = default;
};

enum class ConfigRecoveryState {
  Normal,
  Recovered,
  Defaulted,
};

struct ConfigPaths final {
  QString activeFile;
  QString recoveryFile;

  [[nodiscard]] static ConfigPaths standard();
};

struct ConfigLoadResult final {
  bool success = false;
  ConfigState state;
  ConfigRecoveryState recoveryState = ConfigRecoveryState::Normal;
  // Carried until component configuration is durably authoritative.
  std::optional<LegacyWorkspaceSettings> legacyWorkspaceSettings;
  bool legacyWorkspaceRetirementPending = false;
  QString error;
};

class ConfigStore final {
public:
  explicit ConfigStore(ConfigPaths paths);

  [[nodiscard]] ConfigLoadResult load() const;
  [[nodiscard]] bool
  persist(const ConfigState &current, const ConfigState &next,
          const std::optional<LegacyWorkspaceSettings> &legacyWorkspaceSettings,
          QString &error) const;
  [[nodiscard]] bool retireLegacyWorkspaceSettings(const ConfigState &state,
                                                   QString &error) const;

private:
  ConfigPaths paths_;
};

} // namespace HyprShelld
