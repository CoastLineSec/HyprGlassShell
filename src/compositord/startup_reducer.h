#pragma once

#include <QString>
#include <QtTypes>

#include <variant>

namespace HyprShelld::Compositor {

enum class AuthorityAnchorObservationKind {
  Missing,
  PresentInvalid,
  ExactV2,
};

struct AuthorityAnchorObservation final {
  AuthorityAnchorObservationKind kind = AuthorityAnchorObservationKind::Missing;
  QString authorityId;
};

enum class DesiredAuthorityObservationKind {
  Missing,
  PresentInvalid,
  ExactV1,
  ExactV2,
};

struct DesiredAuthorityObservation final {
  DesiredAuthorityObservationKind kind =
      DesiredAuthorityObservationKind::Missing;
  QString authorityId;
  quint64 revision = 0;
};

enum class ObservedAuthorityKind {
  V1,
  V2,
  Absent,
  Unreadable,
};

struct ObservedAuthorityTuple final {
  ObservedAuthorityKind kind = ObservedAuthorityKind::Unreadable;
  QString authorityId;
  quint64 revision = 0;

  friend bool operator==(const ObservedAuthorityTuple &,
                         const ObservedAuthorityTuple &) = default;
};

// These inputs are already classified observations. This pure function never
// reads bytes, parses legacy state, or mints an identifier.
[[nodiscard]] ObservedAuthorityTuple
classifyObservedAuthority(const AuthorityAnchorObservation &anchor,
                          const DesiredAuthorityObservation &desired);

[[nodiscard]] bool
isValidObservedAuthorityTuple(const ObservedAuthorityTuple &tuple);

[[nodiscard]] QString observedAuthorityKindName(ObservedAuthorityKind kind);

struct SettledV2StartupPrerequisites final {
  bool safeRootAndExclusiveLease = false;
  bool protectedContractsExact = false;
  bool authorityTransitionReconciledAndAbsent = false;
  // True only after pending.json was completely classified as either absent
  // or an ordinary Apply/Recovery/display record. False delegates a Restart
  // tag or an incomplete classification to its owning coordinator.
  bool pendingClassifiedAsAbsentOrOrdinary = false;
  bool referencedGenerationsVerified = false;
};

struct NoPendingCoherenceFacts final {
  // True only when the retained authority record and canonical Desired v2
  // bytes form one exact current epoch. This is byte-graph coherence, not a
  // filesystem-freshness or lease claim.
  bool currentAuthorityCoherent = false;
  bool lastGoodPresent = false;
  bool appliedPresent = false;
  // When both mirrors are present, they must be exact records in the current
  // epoch and Applied must bind LastGood's revision and LF-excluded digest.
  bool presentPairCoherent = false;
  bool appliedRevisionAtMostDesired = false;
  // Vacuously true when the pair is absent or its revision differs from
  // Desired. At an equal revision, canonical Desired and LastGood bytes must
  // be byte-identical. Invalid or incomplete evidence is false.
  bool sameRevisionRuleSatisfied = false;
};

enum class OrdinaryPendingPhase {
  Prepared,
  Committing,
};

enum class MirrorRelation {
  Before,
  After,
  Both,
  Neither,
};

struct OrdinaryPendingStartupFacts final {
  // The retained authority record and canonical Desired v2 bytes must first
  // form one exact current epoch. This does not assert freshness or a lease.
  bool currentAuthorityCoherent = false;
  // True only for an exact ordinary Pending record and exact optional live
  // mirrors in that same epoch, including same-revision uniqueness.
  bool recordCoherent = false;
  OrdinaryPendingPhase phase = OrdinaryPendingPhase::Prepared;
  MirrorRelation desired = MirrorRelation::Neither;
  MirrorRelation lastGood = MirrorRelation::Neither;
  MirrorRelation activation = MirrorRelation::Neither;
};

using SettledV2PendingFacts =
    std::variant<NoPendingCoherenceFacts, OrdinaryPendingStartupFacts>;

struct SettledV2StartupInput final {
  SettledV2StartupPrerequisites prerequisites;
  SettledV2PendingFacts pending = NoPendingCoherenceFacts{};
};

enum class SettledV2StartupDecision {
  Ready,
  RemovePrepared,
  RollForwardCommitting,
  RepairOnly,
  Fatal,
  DelegatePrerequisite,
};

// The only phase edge is Prepared -> Committing. Removal after Prepared abort
// or after Committing roll-forward is a store operation, not another phase.
[[nodiscard]] bool
isLegalOrdinaryPendingPhaseTransition(OrdinaryPendingPhase from,
                                      OrdinaryPendingPhase to);

// Pure classification only. It neither reconciles mirrors nor touches Store.
[[nodiscard]] SettledV2StartupDecision
reduceSettledV2Startup(const SettledV2StartupInput &input);

} // namespace HyprShelld::Compositor
