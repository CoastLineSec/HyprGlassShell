#pragma once

#include "activation_requirement.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Compositor {

inline constexpr quint32 legacyTransactionRecordV1FormatVersion = 1;
inline constexpr qsizetype maximumLegacyAppliedRecordV1Bytes =
    4 * 1024 * 1024;
inline constexpr int maximumLegacyAppliedRecordV1Depth = 16;
inline constexpr qsizetype maximumLegacyOrdinaryPendingRecordV1Bytes =
    4 * 1024 * 1024;
inline constexpr int maximumLegacyOrdinaryPendingRecordV1Depth = 64;

// This is the exact recovered v1 wire grammar, including its deliberately lax
// string syntax. In particular, the two 64-character fields are not required
// to be hexadecimal, activationNonce may contain any 32..128 QChars, and the
// entrypoint may be any non-empty string. A caller must not treat successful
// decoding as proof that these values qualify for v2 authority adoption.
struct LegacyAppliedRecordV1 final {
    quint64 revision = 0;
    QString snapshotDigest;
    QString generation;
    QString activationNonce;
    QString entrypoint;
    ActivationRequirement requiredActivation = ActivationRequirement::Reload;

    friend bool operator==(
        const LegacyAppliedRecordV1 &,
        const LegacyAppliedRecordV1 &
    ) = default;
};

enum class LegacyOrdinaryPendingKindV1 {
    Apply,
    Recovery,
    DisplayPreview,
};

enum class LegacyOrdinaryPendingPhaseV1 {
    Prepared,
    Committing,
};

struct LegacyOrdinaryPendingRecordV1 final {
    LegacyOrdinaryPendingKindV1 kind =
        LegacyOrdinaryPendingKindV1::Apply;
    LegacyOrdinaryPendingPhaseV1 phase =
        LegacyOrdinaryPendingPhaseV1::Prepared;
    quint64 expectedRevision = 0;
    QString beforeDesiredDigest;

    // The exact standalone Desired v1 serializer bytes, including one final
    // LF, are retained separately from the typed parser product. Legacy v1
    // snapshot relationships hash those complete bytes, including that LF.
    Hyprland::DesiredState candidateSnapshot;
    QByteArray candidateSnapshotBytes;

    QString snapshotDigest;
    LegacyAppliedRecordV1 afterActivation;
    std::optional<LegacyAppliedRecordV1> beforeActivation;

    friend bool operator==(
        const LegacyOrdinaryPendingRecordV1 &,
        const LegacyOrdinaryPendingRecordV1 &
    ) = default;
};

// Serializers are partial by design. They never manufacture durable bytes
// from a typed value that the exact recovered decoder would reject.
[[nodiscard]] std::optional<QByteArray> serializeLegacyAppliedRecordV1(
    const LegacyAppliedRecordV1 &record
);

[[nodiscard]] std::optional<LegacyAppliedRecordV1>
parseLegacyAppliedRecordV1(QByteArrayView bytes);

[[nodiscard]] std::optional<QByteArray>
serializeLegacyOrdinaryPendingRecordV1(
    const LegacyOrdinaryPendingRecordV1 &record,
    const Hyprland::Catalog &catalog,
    const Hyprland::ActionCatalog &actions
);

[[nodiscard]] std::optional<LegacyOrdinaryPendingRecordV1>
parseLegacyOrdinaryPendingRecordV1(
    QByteArrayView bytes,
    const Hyprland::Catalog &catalog,
    const Hyprland::ActionCatalog &actions
);

} // namespace HyprShelld::Compositor
