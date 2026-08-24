#include "settled_v2_startup_facts.h"

#include "authority_records.h"
#include "ordinary_pending_record.h"

#include "hyprland/desired_state.h"

#include <QCryptographicHash>
#include <QString>

#include <array>
#include <cstddef>
#include <optional>

namespace HyprShelld::Compositor {
namespace {

[[nodiscard]] bool validObservationView(const QByteArrayView bytes,
                                        const qsizetype maximumBytes) {
  return bytes.size() >= 0 && bytes.size() <= maximumBytes &&
         (bytes.size() == 0 || bytes.data() != nullptr);
}

template <typename T> struct ExactValue final {
  T value;
  QByteArrayView bytes;
};

[[nodiscard]] std::optional<ExactValue<AuthorityRecordV2>>
exactAuthority(const QByteArrayView bytes) {
  if (!validObservationView(bytes, maximumAuthorityRecordV2Bytes)) {
    return std::nullopt;
  }
  const auto parsed = parseAuthorityRecordV2(bytes);
  if (!parsed) {
    return std::nullopt;
  }
  const auto serialized = serializeAuthorityRecordV2(*parsed.value);
  if (!serialized || QByteArrayView(*serialized.value) != bytes) {
    return std::nullopt;
  }
  return ExactValue<AuthorityRecordV2>{*parsed.value, bytes};
}

[[nodiscard]] std::optional<ExactValue<Hyprland::DesiredStateV2>>
exactDesired(const QByteArrayView bytes, const Hyprland::Catalog &catalogV2,
             const Hyprland::ActionCatalog &actionCatalogV2) {
  if (!validObservationView(bytes, Hyprland::maximumDesiredStateBytes)) {
    return std::nullopt;
  }
  const auto parsed =
      Hyprland::parseDormantDesiredStateV2(bytes, catalogV2, actionCatalogV2);
  if (!parsed) {
    return std::nullopt;
  }
  const auto serialized =
      Hyprland::serializeDormantDesiredStateV2(*parsed.value);
  if (!serialized || QByteArrayView(*serialized.value) != bytes) {
    return std::nullopt;
  }
  return ExactValue<Hyprland::DesiredStateV2>{*parsed.value, bytes};
}

[[nodiscard]] std::optional<ExactValue<AppliedRecordV2>>
exactApplied(const QByteArrayView bytes) {
  if (!validObservationView(bytes, maximumAppliedRecordV2Bytes)) {
    return std::nullopt;
  }
  const auto parsed = parseAppliedRecordV2(bytes);
  if (!parsed) {
    return std::nullopt;
  }
  const auto serialized = serializeAppliedRecordV2(*parsed.value);
  if (!serialized || QByteArrayView(*serialized.value) != bytes) {
    return std::nullopt;
  }
  return ExactValue<AppliedRecordV2>{*parsed.value, bytes};
}

[[nodiscard]] std::optional<ExactValue<OrdinaryPendingRecordV2>>
exactOrdinaryPending(const QByteArrayView bytes,
                     const Hyprland::Catalog &catalogV2,
                     const Hyprland::ActionCatalog &actionCatalogV2) {
  if (!validObservationView(bytes, maximumOrdinaryPendingRecordV2Bytes)) {
    return std::nullopt;
  }
  const auto parsed =
      parseOrdinaryPendingRecordV2(bytes, catalogV2, actionCatalogV2);
  if (!parsed) {
    return std::nullopt;
  }
  const auto serialized = serializeOrdinaryPendingRecordV2(
      *parsed.value, catalogV2, actionCatalogV2);
  if (!serialized || QByteArrayView(*serialized.value) != bytes) {
    return std::nullopt;
  }
  return ExactValue<OrdinaryPendingRecordV2>{*parsed.value, bytes};
}

// Callers reach this helper only after exact Desired serialization proved the
// sole final LF. The v2 snapshot domain excludes exactly that one byte.
[[nodiscard]] QString desiredSnapshotDigest(const QByteArrayView exactBytes) {
  if (exactBytes.isEmpty() || exactBytes.back() != '\n') {
    return {};
  }
  return QString::fromLatin1(
      QCryptographicHash::hash(exactBytes.first(exactBytes.size() - 1),
                               QCryptographicHash::Sha256)
          .toHex());
}

[[nodiscard]] MirrorRelation mirrorRelation(const bool before,
                                            const bool after) {
  if (before && after) {
    return MirrorRelation::Both;
  }
  if (before) {
    return MirrorRelation::Before;
  }
  if (after) {
    return MirrorRelation::After;
  }
  return MirrorRelation::Neither;
}

struct SnapshotClaim final {
  quint64 revision = 0;
  QString digest;
};

struct DocumentClaim final {
  quint64 revision = 0;
  QByteArrayView bytes;
  QString digest;
};

template <std::size_t Capacity>
[[nodiscard]] bool addSnapshotClaim(std::array<SnapshotClaim, Capacity> &claims,
                                    std::size_t &count, const quint64 revision,
                                    const QString &digest) {
  if (count >= claims.size() || digest.size() != 64) {
    return false;
  }
  claims.at(count++) = {
      .revision = revision,
      .digest = digest,
  };
  return true;
}

template <std::size_t Capacity>
[[nodiscard]] bool
addDocumentClaim(std::array<DocumentClaim, Capacity> &documents,
                 std::size_t &count, const quint64 revision,
                 const QByteArrayView bytes, const QString &digest) {
  if (count >= documents.size() || digest.size() != 64) {
    return false;
  }
  documents.at(count++) = {
      .revision = revision,
      .bytes = bytes,
      .digest = digest,
  };
  return true;
}

template <std::size_t Capacity>
[[nodiscard]] bool
sameRevisionDigestsUnique(const std::array<SnapshotClaim, Capacity> &claims,
                          const std::size_t count) {
  for (std::size_t left = 0; left < count; ++left) {
    for (std::size_t right = left + 1; right < count; ++right) {
      if (claims.at(left).revision == claims.at(right).revision &&
          claims.at(left).digest != claims.at(right).digest) {
        return false;
      }
    }
  }
  return true;
}

template <std::size_t Capacity>
[[nodiscard]] bool sameRevisionDocumentsUnique(
    const std::array<DocumentClaim, Capacity> &documents,
    const std::size_t count) {
  for (std::size_t left = 0; left < count; ++left) {
    for (std::size_t right = left + 1; right < count; ++right) {
      if (documents.at(left).revision == documents.at(right).revision &&
          documents.at(left).bytes != documents.at(right).bytes) {
        return false;
      }
    }
  }
  return true;
}

} // namespace

NoPendingCoherenceFacts
buildNoPendingCoherenceFactsV2(const SettledV2CurrentRecordBytes &current,
                               const Hyprland::Catalog &catalogV2,
                               const Hyprland::ActionCatalog &actionCatalogV2) {
  NoPendingCoherenceFacts facts{
      .lastGoodPresent = current.lastGood.has_value(),
      .appliedPresent = current.applied.has_value(),
      .sameRevisionRuleSatisfied = !current.lastGood && !current.applied,
  };

  const auto authority = exactAuthority(current.authority);
  const auto desired =
      exactDesired(current.desired, catalogV2, actionCatalogV2);
  facts.currentAuthorityCoherent =
      authority && desired &&
      authority->value.authorityId == desired->value.authorityId;

  std::optional<ExactValue<Hyprland::DesiredStateV2>> lastGood;
  if (current.lastGood) {
    lastGood = exactDesired(*current.lastGood, catalogV2, actionCatalogV2);
  }
  std::optional<ExactValue<AppliedRecordV2>> applied;
  if (current.applied) {
    applied = exactApplied(*current.applied);
  }

  if (!current.lastGood || !current.applied) {
    return facts;
  }
  if (!lastGood || !applied || !desired || !facts.currentAuthorityCoherent) {
    facts.sameRevisionRuleSatisfied = false;
    return facts;
  }

  const auto lastGoodDigest = desiredSnapshotDigest(lastGood->bytes);
  const auto epochExact =
      lastGood->value.authorityId == authority->value.authorityId &&
      applied->value.authorityId == authority->value.authorityId;
  facts.presentPairCoherent =
      epochExact &&
      lastGood->value.semanticState.revision == applied->value.revision &&
      !lastGoodDigest.isEmpty() &&
      lastGoodDigest == applied->value.snapshotDigest;
  if (facts.presentPairCoherent) {
    facts.appliedRevisionAtMostDesired =
        applied->value.revision <= desired->value.semanticState.revision;
    facts.sameRevisionRuleSatisfied =
        lastGood->value.semanticState.revision !=
            desired->value.semanticState.revision ||
        lastGood->bytes == desired->bytes;
  }
  return facts;
}

OrdinaryPendingStartupFacts buildOrdinaryPendingStartupFactsV2(
    const SettledV2CurrentRecordBytes &current,
    const std::optional<QByteArrayView> ordinaryPending,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2) {
  OrdinaryPendingStartupFacts facts;

  const auto authority = exactAuthority(current.authority);
  const auto desired =
      exactDesired(current.desired, catalogV2, actionCatalogV2);
  facts.currentAuthorityCoherent =
      authority && desired &&
      authority->value.authorityId == desired->value.authorityId;

  std::optional<ExactValue<Hyprland::DesiredStateV2>> lastGood;
  if (current.lastGood) {
    lastGood = exactDesired(*current.lastGood, catalogV2, actionCatalogV2);
  }
  std::optional<ExactValue<AppliedRecordV2>> applied;
  if (current.applied) {
    applied = exactApplied(*current.applied);
  }
  std::optional<ExactValue<OrdinaryPendingRecordV2>> pending;
  if (ordinaryPending) {
    pending =
        exactOrdinaryPending(*ordinaryPending, catalogV2, actionCatalogV2);
  }

  const auto optionalRecordsExact =
      (!current.lastGood || lastGood.has_value()) &&
      (!current.applied || applied.has_value());
  if (!facts.currentAuthorityCoherent || !optionalRecordsExact ||
      !ordinaryPending || !pending) {
    return facts;
  }

  const auto &authorityId = authority->value.authorityId;
  const auto epochExact =
      pending->value.authorityId == authorityId &&
      (!lastGood || lastGood->value.authorityId == authorityId) &&
      (!applied || applied->value.authorityId == authorityId);
  if (!epochExact) {
    return facts;
  }

  const auto desiredDigest = desiredSnapshotDigest(desired->bytes);
  const auto candidateDigest = desiredSnapshotDigest(
      QByteArrayView(pending->value.candidateSnapshotBytes));
  if (desiredDigest.isEmpty() || candidateDigest.isEmpty() ||
      candidateDigest != pending->value.snapshotDigest) {
    return facts;
  }

  std::optional<QString> lastGoodDigest;
  if (lastGood) {
    lastGoodDigest = desiredSnapshotDigest(lastGood->bytes);
    if (lastGoodDigest->isEmpty()) {
      return facts;
    }
  }

  if (pending->value.beforeActivation.has_value() !=
      pending->value.beforeActivationDesired.has_value()) {
    return facts;
  }

  QByteArray recoveryPriorBytes;
  QString recoveryPriorDigest;
  if (pending->value.kind == OrdinaryPendingKind::Recovery) {
    if (!pending->value.beforeActivation ||
        !pending->value.beforeActivationDesired) {
      return facts;
    }
    auto prior = pending->value.candidateSnapshot;
    prior.semanticState.revision = pending->value.beforeActivation->revision;
    const auto serializedPrior =
        Hyprland::serializeDormantDesiredStateV2(prior);
    if (!serializedPrior) {
      return facts;
    }
    const auto reparsedPrior = Hyprland::parseDormantDesiredStateV2(
        QByteArrayView(*serializedPrior.value), catalogV2, actionCatalogV2);
    if (!reparsedPrior || *reparsedPrior.value != prior) {
      return facts;
    }
    recoveryPriorBytes = *serializedPrior.value;
    recoveryPriorDigest =
        desiredSnapshotDigest(QByteArrayView(recoveryPriorBytes));
    if (recoveryPriorDigest.isEmpty() ||
        pending->value.beforeActivationDesired->state != prior ||
        pending->value.beforeActivationDesired->bytes != recoveryPriorBytes) {
      return facts;
    }
  }

  std::array<DocumentClaim, 5> documents{};
  std::size_t documentCount = 0;
  if (!addDocumentClaim(documents, documentCount,
                        desired->value.semanticState.revision, desired->bytes,
                        desiredDigest)) {
    return facts;
  }
  if (lastGood && !addDocumentClaim(documents, documentCount,
                                    lastGood->value.semanticState.revision,
                                    lastGood->bytes, *lastGoodDigest)) {
    return facts;
  }
  if (!addDocumentClaim(documents, documentCount,
                        pending->value.candidateSnapshot.semanticState.revision,
                        QByteArrayView(pending->value.candidateSnapshotBytes),
                        candidateDigest)) {
    return facts;
  }
  if (pending->value.beforeActivationDesired &&
      !addDocumentClaim(
          documents, documentCount,
          pending->value.beforeActivationDesired->state.semanticState.revision,
          QByteArrayView(pending->value.beforeActivationDesired->bytes),
          pending->value.beforeActivation->snapshotDigest)) {
    return facts;
  }
  if (!recoveryPriorBytes.isEmpty() &&
      !addDocumentClaim(
          documents, documentCount, pending->value.beforeActivation->revision,
          QByteArrayView(recoveryPriorBytes), recoveryPriorDigest)) {
    return facts;
  }
  if (!sameRevisionDocumentsUnique(documents, documentCount)) {
    return facts;
  }

  std::array<SnapshotClaim, 10> claims{};
  std::size_t claimCount = 0;
  for (std::size_t index = 0; index < documentCount; ++index) {
    if (!addSnapshotClaim(claims, claimCount, documents.at(index).revision,
                          documents.at(index).digest)) {
      return facts;
    }
  }
  if (!addSnapshotClaim(claims, claimCount, pending->value.expectedRevision,
                        pending->value.beforeDesiredDigest) ||
      !addSnapshotClaim(claims, claimCount,
                        pending->value.afterActivation.revision,
                        pending->value.afterActivation.snapshotDigest)) {
    return facts;
  }
  if (pending->value.beforeActivation &&
      !addSnapshotClaim(claims, claimCount,
                        pending->value.beforeActivation->revision,
                        pending->value.beforeActivation->snapshotDigest)) {
    return facts;
  }
  if (applied && !addSnapshotClaim(claims, claimCount, applied->value.revision,
                                   applied->value.snapshotDigest)) {
    return facts;
  }
  if (!sameRevisionDigestsUnique(claims, claimCount)) {
    return facts;
  }

  facts.recordCoherent = true;
  facts.phase = pending->value.phase;

  const auto desiredBefore =
      desired->value.semanticState.revision ==
          pending->value.expectedRevision &&
      desiredDigest == pending->value.beforeDesiredDigest;
  const auto desiredAfter =
      desired->value.semanticState.revision ==
          pending->value.candidateSnapshot.semanticState.revision &&
      desiredDigest == pending->value.snapshotDigest;
  facts.desired = mirrorRelation(desiredBefore, desiredAfter);

  const auto lastGoodBefore =
      !lastGood ? !pending->value.beforeActivation.has_value()
                : pending->value.beforeActivation &&
                      lastGood->value.semanticState.revision ==
                          pending->value.beforeActivation->revision &&
                      *lastGoodDigest ==
                          pending->value.beforeActivation->snapshotDigest;
  const auto lastGoodAfter =
      lastGood &&
      lastGood->value.semanticState.revision ==
          pending->value.candidateSnapshot.semanticState.revision &&
      *lastGoodDigest == pending->value.snapshotDigest;
  facts.lastGood = mirrorRelation(lastGoodBefore, lastGoodAfter);

  const auto activationBefore =
      !applied ? !pending->value.beforeActivation.has_value()
               : pending->value.beforeActivation &&
                     applied->value == *pending->value.beforeActivation;
  const auto activationAfter =
      applied && applied->value == pending->value.afterActivation;
  facts.activation = mirrorRelation(activationBefore, activationAfter);
  return facts;
}

} // namespace HyprShelld::Compositor
