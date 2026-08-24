#include "startup_reducer.h"

#include "identity.h"

#include <type_traits>

namespace HyprShelld::Compositor {
namespace {

[[nodiscard]] bool matchesBefore(const MirrorRelation relation) {
  return relation == MirrorRelation::Before || relation == MirrorRelation::Both;
}

[[nodiscard]] bool matchesAfter(const MirrorRelation relation) {
  return relation == MirrorRelation::After || relation == MirrorRelation::Both;
}

[[nodiscard]] bool matchesEither(const MirrorRelation relation) {
  switch (relation) {
  case MirrorRelation::Before:
  case MirrorRelation::After:
  case MirrorRelation::Both:
    return true;
  case MirrorRelation::Neither:
    return false;
  }
  return false;
}

[[nodiscard]] SettledV2StartupDecision
reduceNoPending(const NoPendingCoherenceFacts &facts) {
  if (!facts.currentAuthorityCoherent || !facts.sameRevisionRuleSatisfied) {
    return SettledV2StartupDecision::RepairOnly;
  }
  if (!facts.lastGoodPresent && !facts.appliedPresent) {
    return SettledV2StartupDecision::Ready;
  }
  if (facts.lastGoodPresent != facts.appliedPresent ||
      !facts.presentPairCoherent || !facts.appliedRevisionAtMostDesired) {
    return SettledV2StartupDecision::RepairOnly;
  }
  return SettledV2StartupDecision::Ready;
}

[[nodiscard]] SettledV2StartupDecision
reduceOrdinaryPending(const OrdinaryPendingStartupFacts &facts) {
  if (!facts.currentAuthorityCoherent || !facts.recordCoherent ||
      !matchesEither(facts.desired) || !matchesEither(facts.lastGood) ||
      !matchesEither(facts.activation)) {
    return SettledV2StartupDecision::RepairOnly;
  }

  if (facts.phase == OrdinaryPendingPhase::Prepared) {
    return matchesBefore(facts.desired) && matchesBefore(facts.lastGood) &&
                   matchesBefore(facts.activation)
               ? SettledV2StartupDecision::RemovePrepared
               : SettledV2StartupDecision::RepairOnly;
  }
  if (facts.phase != OrdinaryPendingPhase::Committing) {
    return SettledV2StartupDecision::RepairOnly;
  }

  const auto desiredAfter = matchesAfter(facts.desired);
  const auto lastGoodAfter = matchesAfter(facts.lastGood);
  const auto activationAfter = matchesAfter(facts.activation);
  if ((activationAfter && (!desiredAfter || !lastGoodAfter)) ||
      (lastGoodAfter && !desiredAfter)) {
    return SettledV2StartupDecision::RepairOnly;
  }
  return SettledV2StartupDecision::RollForwardCommitting;
}

} // namespace

ObservedAuthorityTuple
classifyObservedAuthority(const AuthorityAnchorObservation &anchor,
                          const DesiredAuthorityObservation &desired) {
  if (anchor.kind == AuthorityAnchorObservationKind::Missing &&
      desired.kind == DesiredAuthorityObservationKind::Missing &&
      anchor.authorityId.isEmpty() && desired.authorityId.isEmpty() &&
      desired.revision == 0) {
    return {
        .kind = ObservedAuthorityKind::Absent,
        .authorityId = {},
        .revision = 0,
    };
  }

  if (anchor.kind == AuthorityAnchorObservationKind::Missing &&
      desired.kind == DesiredAuthorityObservationKind::ExactV1 &&
      anchor.authorityId.isEmpty() && desired.authorityId.isEmpty()) {
    return {
        .kind = ObservedAuthorityKind::V1,
        .authorityId = {},
        .revision = desired.revision,
    };
  }

  if (anchor.kind == AuthorityAnchorObservationKind::ExactV2 &&
      desired.kind == DesiredAuthorityObservationKind::ExactV2 &&
      isCanonicalIdentifier(anchor.authorityId) &&
      anchor.authorityId == desired.authorityId) {
    return {
        .kind = ObservedAuthorityKind::V2,
        .authorityId = anchor.authorityId,
        .revision = desired.revision,
    };
  }

  return {
      .kind = ObservedAuthorityKind::Unreadable,
      .authorityId = {},
      .revision = 0,
  };
}

bool isValidObservedAuthorityTuple(const ObservedAuthorityTuple &tuple) {
  switch (tuple.kind) {
  case ObservedAuthorityKind::V1:
    return tuple.authorityId.isEmpty();
  case ObservedAuthorityKind::V2:
    return isCanonicalIdentifier(tuple.authorityId);
  case ObservedAuthorityKind::Absent:
  case ObservedAuthorityKind::Unreadable:
    return tuple.authorityId.isEmpty() && tuple.revision == 0;
  }
  return false;
}

QString observedAuthorityKindName(const ObservedAuthorityKind kind) {
  switch (kind) {
  case ObservedAuthorityKind::V1:
    return QStringLiteral("v1");
  case ObservedAuthorityKind::V2:
    return QStringLiteral("v2");
  case ObservedAuthorityKind::Absent:
    return QStringLiteral("absent");
  case ObservedAuthorityKind::Unreadable:
    return QStringLiteral("unreadable");
  }
  return {};
}

bool isLegalOrdinaryPendingPhaseTransition(const OrdinaryPendingPhase from,
                                           const OrdinaryPendingPhase to) {
  return from == OrdinaryPendingPhase::Prepared &&
         to == OrdinaryPendingPhase::Committing;
}

SettledV2StartupDecision
reduceSettledV2Startup(const SettledV2StartupInput &input) {
  const auto &prerequisites = input.prerequisites;
  if (!prerequisites.safeRootAndExclusiveLease ||
      !prerequisites.protectedContractsExact) {
    return SettledV2StartupDecision::Fatal;
  }
  if (!prerequisites.authorityTransitionReconciledAndAbsent ||
      !prerequisites.pendingClassifiedAsAbsentOrOrdinary) {
    return SettledV2StartupDecision::DelegatePrerequisite;
  }
  if (!prerequisites.referencedGenerationsVerified) {
    return SettledV2StartupDecision::RepairOnly;
  }

  return std::visit(
      [](const auto &facts) -> SettledV2StartupDecision {
        using Facts = std::decay_t<decltype(facts)>;
        if constexpr (std::is_same_v<Facts, NoPendingCoherenceFacts>) {
          return reduceNoPending(facts);
        } else {
          return reduceOrdinaryPending(facts);
        }
      },
      input.pending);
}

} // namespace HyprShelld::Compositor
