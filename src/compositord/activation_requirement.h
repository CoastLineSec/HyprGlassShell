#pragma once

#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"

#include <QString>

namespace HyprShelld::Compositor {

enum class ActivationRequirement {
    None,
    Reload,
    Restart,
    Session,
};

[[nodiscard]] QString activationRequirementName(ActivationRequirement value);

[[nodiscard]] ActivationRequirement activationRequirementForDesiredState(
    const Hyprland::DesiredState &state,
    const Hyprland::Catalog &catalog
);

// A null baseline is reserved for positively proven first adoption. Missing,
// corrupt, or otherwise unprovable applied state must not use that shortcut.
[[nodiscard]] ActivationRequirement activationRequirementForDelta(
    const Hyprland::DesiredState *before,
    const Hyprland::DesiredState &after,
    const Hyprland::Catalog &catalog
);

} // namespace HyprShelld::Compositor
