#pragma once

#include <QJsonObject>
#include <QString>

namespace HyprShelld::Components {

inline constexpr auto workspaceSwitcherDefaultInstanceId =
    "7b4e2329-4320-4e15-894d-218fa690d782";
inline constexpr auto defaultBarLayoutId = "main";

[[nodiscard]] QJsonObject workspaceSwitcherDefaultSettings();
[[nodiscard]] bool isValidWorkspaceSwitcherSettings(
    const QJsonObject &settings
);

} // namespace HyprShelld::Components
