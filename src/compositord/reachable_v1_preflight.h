#pragma once

#include "desired_migration_reducer.h"
#include "dormant_generation_v1_verifier.h"
#include "legacy_entrypoint_records.h"
#include "legacy_transaction_records.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QMap>
#include <QString>
#include <QVector>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Compositor {

// A caller may populate ExactRegular only after its descriptor-safe reader has
// captured one bounded, stable regular file. This reducer deliberately has no
// path, stat, descriptor, Store, or filesystem API of its own.
enum class LegacyReadKindV1 {
    Missing,
    ExactRegular,
    Unsafe,
};

struct LegacyReadV1 final {
    LegacyReadKindV1 kind = LegacyReadKindV1::Missing;
    QByteArray bytes;

    friend bool operator==(const LegacyReadV1 &, const LegacyReadV1 &) =
        default;
};

struct LegacyGenerationEvidenceV1 final {
    // The caller has already captured one bounded regular immutable tree.
    // Unsafe/special/path-ambiguous capture must stop before this API.
    QByteArray desiredBytes;
    QByteArray manifestBytes;
    QMap<QString, QByteArray> files;
    DormantGenerationV1Expectation expected;
};

struct ReachableV1PreflightInput final {
    LegacyReadV1 desired;
    LegacyReadV1 lastGood;
    LegacyReadV1 applied;
    LegacyReadV1 pending;
    LegacyReadV1 ownership;
    LegacyReadV1 bridge;
    QVector<LegacyGenerationEvidenceV1> referencedGenerations;
};

enum class ReachableV1PreflightDisposition {
    Absent,
    ByteCoherentNeedsEntrypointQualification,
    UnsupportedTarget,
    RepairOnly,
};

// Closed, deterministic first-failure vocabulary. Callers must not turn a
// free-form parser error into migration authority or a public repair promise.
enum class ReachableV1PreflightReason {
    None,
    UnsafeRead,
    AbsentResidue,
    MissingDesired,
    InvalidDesired,
    InvalidLastGood,
    InvalidApplied,
    InvalidPending,
    InvalidOwnership,
    InvalidBridge,
    DuplicateGeneration,
    MissingGeneration,
    ExtraGeneration,
    InvalidGeneration,
    PendingKindRelation,
    MirrorUnrelated,
    PreparedModified,
    CommittingWriteOrder,
    LastGoodAppliedMismatch,
    DesiredRevisionRegression,
    SameRevisionMismatch,
    MixedTarget,
    MixedCatalogLineage,
    MixedActionLineage,
    PendingBridgeMismatch,
    OwnershipMismatch,
    BridgeMismatch,
    BridgeAmbiguous,
    UnsupportedTarget,
    EntrypointQualificationRequired,
};

enum class PendingResolutionV1 {
    None,
    RemovePrepared,
    RollForwardCommitting,
};

enum class EntrypointSideV1 {
    Unmanaged,
    Managed,
    BridgePrior,
    BridgeTarget,
};

enum class LegacyMirrorRelationV1 {
    NotApplicable,
    Before,
    After,
    Both,
    Unrelated,
};

struct MirrorRelationsV1 final {
    LegacyMirrorRelationV1 desired =
        LegacyMirrorRelationV1::NotApplicable;
    LegacyMirrorRelationV1 lastGood =
        LegacyMirrorRelationV1::NotApplicable;
    LegacyMirrorRelationV1 activation =
        LegacyMirrorRelationV1::NotApplicable;

    friend bool operator==(const MirrorRelationsV1 &,
                           const MirrorRelationsV1 &) = default;
};

// Predicate-only oracle for the recovered active v1 mirror semantics. This
// classifies no bytes, conveys no migration eligibility, and never authorizes
// a Store action; it exists so the complete overlapping Before/After truth
// table can exercise the exact logic used by inspectReachableV1Preflight.
enum class LegacyPendingMirrorClassificationV1 {
    InvalidOrUnrelated,
    PreparedModified,
    CommittingWriteOrder,
    CoherentPrepared,
    CoherentCommitting,
};

[[nodiscard]] LegacyPendingMirrorClassificationV1
classifyLegacyPendingMirrorPredicatesV1(
    LegacyOrdinaryPendingPhaseV1 phase,
    const MirrorRelationsV1 &mirrors
);

struct ResolvedReachableV1 final {
    QByteArray desiredBytes;
    std::optional<QByteArray> lastGoodBytes;
    std::optional<LegacyAppliedRecordV1> applied;
    std::optional<QByteArray> appliedBytes;
};

// These are byte-graph expectations only. In particular, device/inode/path,
// mode, swap, backup, and stable-live-entrypoint identity intentionally remain
// absent. A later descriptor-relative qualifier must consume the original raw
// read tuple together with this result; this result alone is never a receipt.
struct EntrypointExpectationV1 final {
    // `side` is the authority-selected reconciliation destination, never an
    // observation of the current stable path. A retained Ready bridge may
    // still expose either receipt-bound side until the later qualifier proves
    // and reconciles it.
    EntrypointSideV1 side = EntrypointSideV1::Unmanaged;
    // Unmanaged/no-bridge state has no stable-file observation in this leaf.
    std::optional<LegacyEntrypointFileKindV1> kind;
    QString generation;
    QString activationNonce;
    QString digest;
    quint64 size = 0;
    QString generationEntrypoint;

    friend bool operator==(const EntrypointExpectationV1 &,
                           const EntrypointExpectationV1 &) = default;
};

struct ReachableV1PreflightResult final {
    ReachableV1PreflightDisposition disposition =
        ReachableV1PreflightDisposition::RepairOnly;
    ReachableV1PreflightReason reason =
        ReachableV1PreflightReason::InvalidDesired;
    PendingResolutionV1 pendingResolution = PendingResolutionV1::None;
    MirrorRelationsV1 mirrors;
    EntrypointExpectationV1 entrypoint;
    std::optional<ResolvedReachableV1> resolved;
    quint32 sourcePatch = 0;
};

// pendingResolution, resolved, and entrypoint are plan-bearing fields and are
// populated only for ByteCoherentNeedsEntrypointQualification. RepairOnly and
// UnsupportedTarget retain no such plan; sourcePatch and mirror predicates may
// remain as diagnostics.

// Pure, bounded, source-only qualification of the complete reachable v1 byte
// graph. It performs no filesystem access, publication, ID/nonce minting,
// journal construction, v2 materialization, activation, D-Bus, or runtime
// operation. Even a byte-coherent result still requires entrypoint/filesystem
// qualification and therefore never reports Eligible.
[[nodiscard]] ReachableV1PreflightResult inspectReachableV1Preflight(
    const ReachableV1PreflightInput &input,
    QByteArrayView migrationManifestBytes,
    QByteArrayView sourceManifestV2Bytes,
    const Hyprland::Catalog &catalogV1,
    const Hyprland::ActionCatalog &actionCatalogV1,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2
);

} // namespace HyprShelld::Compositor
