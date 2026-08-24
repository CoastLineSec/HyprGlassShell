#pragma once

#include "hyprland/desired_state.h"
#include "hyprland/input_device_inventory.h"

#include <QVariantList>

#include <optional>

namespace HyprShelld {

struct InputDeviceProjection final {
    QVariantList connectedDevices;
    QVariantList savedDevices;
    QVariantList otherSavedDevices;

    friend bool operator==(
        const InputDeviceProjection &,
        const InputDeviceProjection &
    ) = default;
};

[[nodiscard]] InputDeviceProjection projectInputDevices(
    const std::optional<QVector<Hyprland::DeviceConfiguration>> &savedDevices,
    const std::optional<Hyprland::ConnectedInputDeviceInventory> &inventory
);

} // namespace HyprShelld
