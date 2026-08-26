#pragma once

#include "desired_state.h"

#include <QStringView>
#include <QVector>

namespace HyprShelld::Hyprland {

// The managed baseline is deliberately limited to reviewed native Hyprland
// dispatchers. Legacy HGS process commands and layout-specific messages stay
// out until their typed HyprShelld actions have a working broker.
inline constexpr qsizetype shippedDefaultKeybindingCount = 64;

[[nodiscard]] const QVector<BindingConfiguration> &
shippedDefaultKeybindings();

[[nodiscard]] const BindingConfiguration *shippedDefaultKeybindingById(
    QStringView id
);

// Exact-chord fallback keeps pre-layering snapshots compatible. New writes use
// the stable default ID, so changing a default chord remains resettable.
[[nodiscard]] const BindingConfiguration *matchedShippedDefaultKeybinding(
    const BindingConfiguration &binding
);

} // namespace HyprShelld::Hyprland
