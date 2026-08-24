#pragma once

#include "validation.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QVector>

#include <optional>

namespace HyprShelld::Hyprland {

inline constexpr quint32 currentInputDeviceInventoryFormatVersion = 1;
inline constexpr qsizetype maximumInputDeviceInventoryBytes = 512 * 1024;
inline constexpr qsizetype maximumConnectedInputDevices = 256;

enum class ConnectedInputDeviceKind {
    Keyboard,
    Pointer,
    Touch,
    Tablet,
};

struct ConnectedInputDevice final {
    QString sessionSelector;
    ConnectedInputDeviceKind observedKind =
        ConnectedInputDeviceKind::Keyboard;
    std::optional<QString> activeKeymap;

    friend bool operator==(
        const ConnectedInputDevice &,
        const ConnectedInputDevice &
    ) = default;
};

struct UnaddressableInputDeviceCounts final {
    quint32 switches = 0;
    quint32 tabletPads = 0;
    quint32 tabletTools = 0;

    friend bool operator==(
        const UnaddressableInputDeviceCounts &,
        const UnaddressableInputDeviceCounts &
    ) = default;
};

struct ConnectedInputDeviceInventory final {
    QVector<ConnectedInputDevice> records;
    UnaddressableInputDeviceCounts unaddressable;
    QString inventoryDigest;
    QByteArray document;

    friend bool operator==(
        const ConnectedInputDeviceInventory &,
        const ConnectedInputDeviceInventory &
    ) = default;
};

[[nodiscard]] QString connectedInputDeviceKindName(
    ConnectedInputDeviceKind kind
);

// Parses the filtered canonical v1 document returned by Compositor1. This is
// the shared trusted parser for D-Bus clients; it has no access to the private
// runtime fingerprint inputs used to construct the digest.
[[nodiscard]] ValidationResult<ConnectedInputDeviceInventory>
parseConnectedInputDeviceInventoryDocument(QByteArrayView document);

// Converts the pinned Hyprland 0.56.x `j/devices` reply into the bounded
// public inventory. The authenticated runtime identity, raw addresses, and
// service-owned epoch are used only for the opaque private fingerprint.
[[nodiscard]] ValidationResult<ConnectedInputDeviceInventory>
parseConnectedInputDeviceInventory(
    QByteArrayView hyprlandReply,
    QStringView authenticatedRuntimeIdentity,
    QByteArrayView serviceEpoch
);

} // namespace HyprShelld::Hyprland
