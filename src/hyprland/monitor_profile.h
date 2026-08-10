#pragma once

#include "action_catalog.h"
#include "catalog.h"
#include "desired_state.h"
#include "validation.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace HyprShelld::Hyprland {

inline constexpr quint32 currentDisplayProfileFormatVersion = 1;
inline constexpr qsizetype maximumDisplayProfileBytes = 512 * 1024;

struct DisplayProfile final {
    QString topologyDigest;
    QJsonArray outputs;

    friend bool operator==(const DisplayProfile &, const DisplayProfile &)
        = default;
};

struct ConnectedDisplayMode final {
    qint32 width = 0;
    qint32 height = 0;
    double refreshRate = 0.0;
    QString managedMode;

    friend bool operator==(
        const ConnectedDisplayMode &,
        const ConnectedDisplayMode &
    ) = default;
};

struct ConnectedDisplay final {
    qint64 upstreamId = -1;
    QString selector;
    QString description;
    QString make;
    QString model;
    QString serial;
    bool enabled = false;
    qint32 width = 0;
    qint32 height = 0;
    qint32 physicalWidthMm = 0;
    qint32 physicalHeightMm = 0;
    double refreshRate = 0.0;
    qint32 x = 0;
    qint32 y = 0;
    // Canonical CSS order: top, right, bottom, left. HyprCtl's pinned wire
    // order is left, top, right, bottom and is remapped by the parser.
    QVector<qint32> reserved;
    double scale = 1.0;
    qint32 transform = 0;
    bool focused = false;
    bool dpms = false;
    bool vrrActive = false;
    QString mirrorOf;
    QVector<ConnectedDisplayMode> modes;
    QString colorManagement;
    QString currentFormat;
    double sdrBrightness = 1.0;
    double sdrSaturation = 1.0;
    double sdrMinLuminance = 0.2;
    qint64 sdrMaxLuminance = 80;

    friend bool operator==(
        const ConnectedDisplay &,
        const ConnectedDisplay &
    ) = default;
};

struct ConnectedDisplayTopology final {
    QVector<ConnectedDisplay> outputs;
    QString topologyDigest;
    QByteArray document;

    friend bool operator==(
        const ConnectedDisplayTopology &,
        const ConnectedDisplayTopology &
    ) = default;
};

struct DisplayCandidate final {
    DesiredState state;
    QByteArray bytes;
};

// The transport is compact canonical JSON terminated by one newline:
// {"formatVersion":1,"outputs":[...],"topologyDigest":"..."}\n.
[[nodiscard]] ValidationResult<DisplayProfile> parseDisplayProfile(
    QByteArrayView bytes
);
[[nodiscard]] QByteArray serializeDisplayProfile(const DisplayProfile &profile);

// Converts the pinned Hyprland 0.56.x `j/monitors all` reply into the bounded
// compositor-owned topology contract. Dynamic geometry is exported for the
// UI, while topologyDigest covers only connection identity and advertised
// modes so a display preview does not invalidate its own observation.
[[nodiscard]] ValidationResult<ConnectedDisplayTopology>
parseConnectedDisplayTopology(QByteArrayView hyprlandReply);

// Merges exact connected-output records after untouched offline/description
// rules. This preserves offline profiles and ensures connector rules win the
// tagged Hyprland reverse rule lookup. The returned state is canonical N+1.
[[nodiscard]] ValidationResult<DisplayCandidate> buildDisplayCandidate(
    const DesiredState &baseline,
    const DisplayProfile &profile,
    const ConnectedDisplayTopology &topology,
    const Catalog &catalog,
    const ActionCatalog &actionCatalog
);

// Checks the safety relationship that cannot be expressed by desired-state
// JSON alone: a fresh topology fingerprint, one record per connected output,
// and at least one connected output left enabled.
[[nodiscard]] ValidationErrors validateDisplayProfileTopology(
    const DisplayProfile &profile,
    const ConnectedDisplayTopology &topology
);

// Proves the safety-relevant realized output state after activation and again
// immediately before confirmation. Automatic placement/mode policy and
// dynamic layer reservations are intentionally not compared.
[[nodiscard]] ValidationErrors validateDisplayRealization(
    const DesiredState &candidate,
    const ConnectedDisplayTopology &topology
);
[[nodiscard]] ValidationErrors validateDisplayRealization(
    const DisplayProfile &profile,
    const ConnectedDisplayTopology &topology
);

} // namespace HyprShelld::Hyprland
