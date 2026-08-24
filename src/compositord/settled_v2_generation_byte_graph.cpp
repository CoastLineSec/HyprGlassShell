#include "settled_v2_generation_byte_graph.h"

#include "authority_records.h"
#include "dormant_generation_verifier.h"
#include "ordinary_pending_record.h"
#include "settled_v2_pending_observation.h"
#include "startup_reducer.h"

#include "hyprland/desired_state.h"

#include <QCryptographicHash>

#include <array>
#include <cstddef>
#include <optional>

namespace HyprShelld::Compositor {
namespace {

inline constexpr qsizetype expectedDormantGenerationFileCount = 17;
inline constexpr qsizetype maximumEvidencePathCodeUnits = 4096;
inline constexpr qsizetype maximumEvidenceFilePathCodeUnits = 255;

[[nodiscard]] bool validView(const QByteArrayView bytes,
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
  if (!validView(bytes, maximumAuthorityRecordV2Bytes)) {
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
  if (!validView(bytes, Hyprland::maximumDesiredStateBytes)) {
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

[[nodiscard]] std::optional<AppliedRecordV2>
exactApplied(const QByteArrayView bytes) {
  if (!validView(bytes, maximumAppliedRecordV2Bytes)) {
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
  return *parsed.value;
}

[[nodiscard]] std::optional<OrdinaryPendingRecordV2>
exactPending(const QByteArrayView bytes, const Hyprland::Catalog &catalogV2,
             const Hyprland::ActionCatalog &actionCatalogV2) {
  if (!validView(bytes, maximumOrdinaryPendingRecordV2Bytes)) {
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
  return *parsed.value;
}

[[nodiscard]] QString desiredDigest(const QByteArrayView bytes) {
  if (bytes.isEmpty() || bytes.back() != '\n') {
    return {};
  }
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes.first(bytes.size() - 1),
                               QCryptographicHash::Sha256)
          .toHex());
}

[[nodiscard]] bool
boundedEvidenceScalars(const SettledV2GenerationEvidence &evidence) {
  if (evidence.activationNonce.size() != Hyprland::authorityIdHexLength ||
      evidence.generationRoot.size() > maximumEvidencePathCodeUnits ||
      evidence.userCustomPath.size() > maximumEvidencePathCodeUnits ||
      evidence.files.size() != expectedDormantGenerationFileCount) {
    return false;
  }
  for (auto iterator = evidence.files.constBegin();
       iterator != evidence.files.constEnd(); ++iterator) {
    if (iterator.key().size() > maximumEvidenceFilePathCodeUnits) {
      return false;
    }
  }
  return true;
}

struct ReferenceSet final {
  std::array<AppliedRecordV2, maximumSettledV2GenerationEvidence> values{};
  std::size_t count = 0;
};

[[nodiscard]] bool addReference(ReferenceSet &references,
                                const AppliedRecordV2 &candidate) {
  for (std::size_t index = 0; index < references.count; ++index) {
    const auto &existing = references.values.at(index);
    const auto sameGeneration = existing.generation == candidate.generation;
    const auto sameNonce =
        existing.activationNonce == candidate.activationNonce;
    if (sameGeneration != sameNonce) {
      return false;
    }
    if (sameGeneration && sameNonce) {
      // One generation identity cannot carry two historical Applied records.
      // This deliberately includes entrypoint and requiredActivation, even
      // though the latter is not a renderer-v2 content expectation.
      return existing == candidate;
    }
  }
  if (references.count >= references.values.size()) {
    return false;
  }
  references.values.at(references.count++) = candidate;
  return true;
}

struct DocumentClaim final {
  QString authorityId;
  quint64 revision = 0;
  QByteArrayView bytes;
};

struct DocumentSet final {
  std::array<DocumentClaim, 7> values{};
  std::size_t count = 0;
};

[[nodiscard]] bool addDocument(DocumentSet &documents,
                               const Hyprland::DesiredStateV2 &state,
                               const QByteArrayView bytes) {
  if (documents.count >= documents.values.size()) {
    return false;
  }
  documents.values.at(documents.count++) = {
      .authorityId = state.authorityId,
      .revision = state.semanticState.revision,
      .bytes = bytes,
  };
  return true;
}

[[nodiscard]] bool
sameRevisionDocumentsAreIdentical(const DocumentSet &documents) {
  for (std::size_t left = 0; left < documents.count; ++left) {
    for (std::size_t right = left + 1; right < documents.count; ++right) {
      const auto &a = documents.values.at(left);
      const auto &b = documents.values.at(right);
      if (a.authorityId == b.authorityId && a.revision == b.revision &&
          a.bytes != b.bytes) {
        return false;
      }
    }
  }
  return true;
}

[[nodiscard]] bool
acceptedCounterfactualDecision(const SettledV2PendingFacts &pending) {
  // COUNTERFACTUAL: all gates are true only to reuse the startup reducer's
  // exact no-Pending and ordinary mirror/phase legality. The resulting
  // decision is discarded. This function neither proves nor returns any
  // prerequisite; especially, it cannot set referencedGenerationsVerified.
  const SettledV2StartupInput counterfactual{
      .prerequisites =
          {
              .safeRootAndExclusiveLease = true,
              .protectedContractsExact = true,
              .authorityTransitionReconciledAndAbsent = true,
              .pendingClassifiedAsAbsentOrOrdinary = true,
              .referencedGenerationsVerified = true,
          },
      .pending = pending,
  };
  switch (reduceSettledV2Startup(counterfactual)) {
  case SettledV2StartupDecision::Ready:
  case SettledV2StartupDecision::RemovePrepared:
  case SettledV2StartupDecision::RollForwardCommitting:
    return true;
  case SettledV2StartupDecision::RepairOnly:
  case SettledV2StartupDecision::Fatal:
  case SettledV2StartupDecision::DelegatePrerequisite:
    return false;
  }
  return false;
}

} // namespace

SettledV2GenerationByteGraphResult classifySettledV2GenerationContentByteGraph(
    const SettledV2CurrentRecordBytes &current,
    const std::optional<QByteArrayView> pendingObservation,
    const QVector<SettledV2GenerationEvidence> &evidence,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2) {
  // Cardinality is O(1); reject before walking or hashing any caller evidence.
  if (evidence.size() > maximumSettledV2GenerationEvidence) {
    return SettledV2GenerationByteGraphResult::Incoherent;
  }

  const auto pendingFacts = tryBuildSettledV2PendingFactsV2(
      current, pendingObservation, catalogV2, actionCatalogV2);
  if (!pendingFacts) {
    return SettledV2GenerationByteGraphResult::DelegatePendingOwner;
  }
  if (!acceptedCounterfactualDecision(*pendingFacts)) {
    return SettledV2GenerationByteGraphResult::Incoherent;
  }

  const auto authority = exactAuthority(current.authority);
  const auto desired =
      exactDesired(current.desired, catalogV2, actionCatalogV2);
  if (!authority || !desired ||
      authority->value.authorityId != desired->value.authorityId) {
    return SettledV2GenerationByteGraphResult::Incoherent;
  }

  std::optional<ExactValue<Hyprland::DesiredStateV2>> lastGood;
  if (current.lastGood) {
    lastGood = exactDesired(*current.lastGood, catalogV2, actionCatalogV2);
    if (!lastGood) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }
  }
  std::optional<AppliedRecordV2> applied;
  if (current.applied) {
    applied = exactApplied(*current.applied);
    if (!applied) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }
  }

  ReferenceSet references;
  DocumentSet documents;
  if (!addDocument(documents, desired->value, desired->bytes) ||
      (lastGood && !addDocument(documents, lastGood->value, lastGood->bytes))) {
    return SettledV2GenerationByteGraphResult::Incoherent;
  }

  QByteArray recoveryProjectionBytes;
  std::optional<OrdinaryPendingRecordV2> pending;
  if (!pendingObservation) {
    if (lastGood.has_value() != applied.has_value()) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }
    if (applied && !addReference(references, *applied)) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }
  } else {
    pending = exactPending(*pendingObservation, catalogV2, actionCatalogV2);
    if (!pending ||
        pending->beforeActivation.has_value() !=
            pending->beforeActivationDesired.has_value() ||
        !addReference(references, pending->afterActivation) ||
        (pending->beforeActivation &&
         !addReference(references, *pending->beforeActivation))) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }

    if (applied) {
      const auto matchesBefore =
          pending->beforeActivation && *applied == *pending->beforeActivation;
      const auto matchesAfter = *applied == pending->afterActivation;
      if (!matchesBefore && !matchesAfter) {
        return SettledV2GenerationByteGraphResult::Incoherent;
      }
    }

    const auto candidate =
        exactDesired(QByteArrayView(pending->candidateSnapshotBytes), catalogV2,
                     actionCatalogV2);
    if (!candidate ||
        !addDocument(documents, candidate->value, candidate->bytes)) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }

    if (pending->beforeActivationDesired &&
        !addDocument(documents, pending->beforeActivationDesired->state,
                     QByteArrayView(pending->beforeActivationDesired->bytes))) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }

    if (pending->kind == OrdinaryPendingKind::Recovery) {
      if (!pending->beforeActivation || !pending->beforeActivationDesired) {
        return SettledV2GenerationByteGraphResult::Incoherent;
      }
      auto projection = pending->candidateSnapshot;
      projection.semanticState.revision = pending->beforeActivation->revision;
      const auto serialized =
          Hyprland::serializeDormantDesiredStateV2(projection);
      if (!serialized) {
        return SettledV2GenerationByteGraphResult::Incoherent;
      }
      recoveryProjectionBytes = *serialized.value;
      const auto exactProjection = exactDesired(
          QByteArrayView(recoveryProjectionBytes), catalogV2, actionCatalogV2);
      if (!exactProjection || exactProjection->value != projection ||
          pending->beforeActivationDesired->state != projection ||
          pending->beforeActivationDesired->bytes != recoveryProjectionBytes ||
          !addDocument(documents, exactProjection->value,
                       exactProjection->bytes)) {
        return SettledV2GenerationByteGraphResult::Incoherent;
      }
    }
  }

  if (references.count != static_cast<std::size_t>(evidence.size())) {
    return SettledV2GenerationByteGraphResult::Incoherent;
  }

  std::array<const SettledV2GenerationEvidence *,
             maximumSettledV2GenerationEvidence>
      evidenceByReference{};
  std::array<ExactValue<Hyprland::DesiredStateV2>,
             maximumSettledV2GenerationEvidence>
      evidenceDesired{};
  for (const auto &item : evidence) {
    // Bound all caller-owned QString work before nonce comparison, path
    // normalization, regex matching, or QSet hashing inside the verifier.
    if (!boundedEvidenceScalars(item)) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }
    std::optional<std::size_t> matchingIndex;
    for (std::size_t index = 0; index < references.count; ++index) {
      if (references.values.at(index).activationNonce == item.activationNonce) {
        matchingIndex = index;
        break;
      }
    }
    if (!matchingIndex || evidenceByReference.at(*matchingIndex) != nullptr) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }
    const auto exact = exactDesired(QByteArrayView(item.desiredBytes),
                                    catalogV2, actionCatalogV2);
    if (!exact) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }
    const auto &reference = references.values.at(*matchingIndex);
    if (exact->value.authorityId != reference.authorityId ||
        exact->value.semanticState.revision != reference.revision ||
        desiredDigest(exact->bytes) != reference.snapshotDigest) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }
    evidenceByReference.at(*matchingIndex) = &item;
    evidenceDesired.at(*matchingIndex) = *exact;
    if (!addDocument(documents, exact->value, exact->bytes)) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }
  }

  for (std::size_t index = 0; index < references.count; ++index) {
    if (evidenceByReference.at(index) == nullptr) {
      return SettledV2GenerationByteGraphResult::Incoherent;
    }
  }
  if (!sameRevisionDocumentsAreIdentical(documents)) {
    return SettledV2GenerationByteGraphResult::Incoherent;
  }

  bool everyGenerationVerified = true;
  for (std::size_t index = 0; index < references.count; ++index) {
    const auto &reference = references.values.at(index);
    const auto &item = *evidenceByReference.at(index);
    const DormantGenerationV2Expectation expected{
        .authorityId = reference.authorityId,
        .revision = reference.revision,
        .snapshotDigest = reference.snapshotDigest,
        .generation = reference.generation,
        .activationNonce = reference.activationNonce,
        .generationRoot = item.generationRoot,
        .userCustomPath = item.userCustomPath,
    };
    const auto verified = verifyDormantGenerationV2(
        QByteArrayView(item.manifestBytes), item.files, expected,
        evidenceDesired.at(index).value, catalogV2, actionCatalogV2);
    if (!verified ||
        reference.entrypoint != verified.value->rendered.entrypoint) {
      everyGenerationVerified = false;
    }
    // Do not compare requiredActivation with the isolated rerender. Applied
    // is a historical activation delta, while renderer v2 reports the target
    // in isolation and its manifest/digest deliberately excludes that delta.
  }

  return everyGenerationVerified
             ? SettledV2GenerationByteGraphResult::GenerationContentByteCoherent
             : SettledV2GenerationByteGraphResult::Incoherent;
}

} // namespace HyprShelld::Compositor
