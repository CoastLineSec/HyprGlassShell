#include "ordinary_pending_record.h"

#include "identity.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QJsonObject>
#include <QSet>

#include <limits>
#include <utility>

namespace HyprShelld::Compositor {
namespace {

struct DesiredMaterial final {
  Hyprland::DesiredStateV2 state;
  QByteArray bytes;
  QJsonObject object;
};

enum class DesiredMaterialRole {
  Candidate,
  BeforeActivation,
};

enum class DesiredMaterialFailure {
  Invalid,
  InvalidSize,
  BytesMismatch,
  Unvalidated,
  InvalidObject,
  SemanticMismatch,
};

[[nodiscard]] QString materialPath(const DesiredMaterialRole role) {
  return role == DesiredMaterialRole::Candidate
             ? QStringLiteral("$.candidateSnapshot")
             : QStringLiteral("$.beforeActivationDesired");
}

[[nodiscard]] QString materialName(const DesiredMaterialRole role) {
  return role == DesiredMaterialRole::Candidate
             ? QStringLiteral("candidate")
             : QStringLiteral("before-activation Desired snapshot");
}

[[nodiscard]] QString materialErrorCode(const DesiredMaterialRole role,
                                        const DesiredMaterialFailure failure) {
  if (role == DesiredMaterialRole::Candidate) {
    switch (failure) {
    case DesiredMaterialFailure::Invalid:
      return QStringLiteral("ordinary-pending.invalid-candidate");
    case DesiredMaterialFailure::InvalidSize:
      return QStringLiteral("ordinary-pending.invalid-candidate-size");
    case DesiredMaterialFailure::BytesMismatch:
      return QStringLiteral("ordinary-pending.candidate-bytes-mismatch");
    case DesiredMaterialFailure::Unvalidated:
      return QStringLiteral("ordinary-pending.unvalidated-candidate");
    case DesiredMaterialFailure::InvalidObject:
      return QStringLiteral("ordinary-pending.invalid-candidate-object");
    case DesiredMaterialFailure::SemanticMismatch:
      return QStringLiteral("ordinary-pending.candidate-semantic-mismatch");
    }
  }

  switch (failure) {
  case DesiredMaterialFailure::Invalid:
    return QStringLiteral("ordinary-pending.invalid-before-activation-desired");
  case DesiredMaterialFailure::InvalidSize:
    return QStringLiteral(
        "ordinary-pending.invalid-before-activation-desired-size");
  case DesiredMaterialFailure::BytesMismatch:
    return QStringLiteral(
        "ordinary-pending.before-activation-desired-bytes-mismatch");
  case DesiredMaterialFailure::Unvalidated:
    return QStringLiteral(
        "ordinary-pending.unvalidated-before-activation-desired");
  case DesiredMaterialFailure::InvalidObject:
    return QStringLiteral(
        "ordinary-pending.invalid-before-activation-desired-object");
  case DesiredMaterialFailure::SemanticMismatch:
    return QStringLiteral(
        "ordinary-pending.before-activation-desired-semantic-mismatch");
  }
  return {};
}

[[nodiscard]] CanonicalJson::Limits pendingLimits() {
  return {
      .maximumBytes = maximumOrdinaryPendingRecordV2Bytes,
      .maximumDepth = maximumOrdinaryPendingRecordV2Depth,
      .maximumValues = maximumOrdinaryPendingRecordV2Values,
  };
}

[[nodiscard]] CanonicalJson::Limits appliedLimits() {
  return {
      .maximumBytes = maximumAppliedRecordV2Bytes,
      .maximumDepth = maximumAppliedRecordV2Depth,
      .maximumValues = maximumAppliedRecordV2Values,
  };
}

[[nodiscard]] CanonicalJson::Error pendingError(QString code, QString path,
                                                QString message) {
  return {
      .code = std::move(code),
      .path = std::move(path),
      .message = std::move(message),
  };
}

template <typename T>
[[nodiscard]] CanonicalJson::Result<T> failure(CanonicalJson::Error error) {
  CanonicalJson::Result<T> result;
  result.errors.append(std::move(error));
  return result;
}

template <typename T, typename U>
[[nodiscard]] CanonicalJson::Result<T>
propagate(CanonicalJson::Result<U> source) {
  CanonicalJson::Result<T> result;
  result.errors = std::move(source.errors);
  return result;
}

[[nodiscard]] QSet<QString> keysOf(const QJsonObject &object) {
  QSet<QString> result;
  for (auto iterator = object.constBegin(); iterator != object.constEnd();
       ++iterator) {
    result.insert(iterator.key());
  }
  return result;
}

[[nodiscard]] QString kindName(const OrdinaryPendingKind kind) {
  switch (kind) {
  case OrdinaryPendingKind::Apply:
    return QStringLiteral("apply");
  case OrdinaryPendingKind::Recovery:
    return QStringLiteral("recovery");
  case OrdinaryPendingKind::DisplayPreview:
    return QStringLiteral("display-preview");
  }
  return {};
}

[[nodiscard]] std::optional<OrdinaryPendingKind>
kindFromName(const QStringView name) {
  if (name == QStringLiteral("apply")) {
    return OrdinaryPendingKind::Apply;
  }
  if (name == QStringLiteral("recovery")) {
    return OrdinaryPendingKind::Recovery;
  }
  if (name == QStringLiteral("display-preview")) {
    return OrdinaryPendingKind::DisplayPreview;
  }
  return std::nullopt;
}

[[nodiscard]] QString phaseName(const OrdinaryPendingPhase phase) {
  switch (phase) {
  case OrdinaryPendingPhase::Prepared:
    return QStringLiteral("prepared");
  case OrdinaryPendingPhase::Committing:
    return QStringLiteral("committing");
  }
  return {};
}

[[nodiscard]] std::optional<OrdinaryPendingPhase>
phaseFromName(const QStringView name) {
  if (name == QStringLiteral("prepared")) {
    return OrdinaryPendingPhase::Prepared;
  }
  if (name == QStringLiteral("committing")) {
    return OrdinaryPendingPhase::Committing;
  }
  return std::nullopt;
}

[[nodiscard]] bool validStandaloneDesiredBytes(const QByteArrayView bytes) {
  return bytes.size() >= 3 &&
         bytes.size() <= Hyprland::maximumDesiredStateBytes &&
         bytes.back() == '\n';
}

[[nodiscard]] QString snapshotDigest(const QByteArrayView bytes) {
  if (!validStandaloneDesiredBytes(bytes)) {
    return {};
  }
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes.first(bytes.size() - 1),
                               QCryptographicHash::Sha256)
          .toHex());
}

[[nodiscard]] CanonicalJson::Result<DesiredMaterial>
desiredMaterialForSerialization(
    const Hyprland::DesiredStateV2 &state, const QByteArray &bytes,
    const DesiredMaterialRole role, const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2) {
  const auto serialized = Hyprland::serializeDormantDesiredStateV2(state);
  if (!serialized) {
    return failure<DesiredMaterial>(pendingError(
        materialErrorCode(role, DesiredMaterialFailure::Invalid),
        materialPath(role),
        QStringLiteral(
            "The typed %1 cannot be serialized as dormant Desired v2.")
            .arg(materialName(role))));
  }
  if (!validStandaloneDesiredBytes(*serialized.value)) {
    return failure<DesiredMaterial>(pendingError(
        materialErrorCode(role, DesiredMaterialFailure::InvalidSize),
        materialPath(role),
        QStringLiteral(
            "The standalone %1 exceeds its exact Desired v2 bound or framing.")
            .arg(materialName(role))));
  }
  if (*serialized.value != bytes) {
    return failure<DesiredMaterial>(pendingError(
        materialErrorCode(role, DesiredMaterialFailure::BytesMismatch),
        materialPath(role),
        QStringLiteral("Retained %1 bytes must equal the authoritative Desired "
                       "v2 serializer output.")
            .arg(materialName(role))));
  }

  const auto reparsed = Hyprland::parseDormantDesiredStateV2(
      QByteArrayView(bytes), catalogV2, actionCatalogV2);
  if (!reparsed || *reparsed.value != state) {
    return failure<DesiredMaterial>(pendingError(
        materialErrorCode(role, DesiredMaterialFailure::Unvalidated),
        materialPath(role),
        QStringLiteral("The %1 must be the exact typed parser product under "
                       "the supplied v2 authorities.")
            .arg(materialName(role))));
  }

  const auto parsedObject = Hyprland::JsonSupport::parseStrictObject(
      QByteArrayView(bytes), Hyprland::maximumDesiredStateBytes, 64);
  if (!parsedObject) {
    return failure<DesiredMaterial>(pendingError(
        materialErrorCode(role, DesiredMaterialFailure::InvalidObject),
        materialPath(role),
        QStringLiteral("The standalone %1 is not a JSON object.")
            .arg(materialName(role))));
  }

  return {
      .value =
          DesiredMaterial{
              .state = state,
              .bytes = bytes,
              .object = *parsedObject.value,
          },
      .errors = {},
  };
}

[[nodiscard]] CanonicalJson::Result<DesiredMaterial>
desiredMaterialFromObject(const QJsonObject &object,
                          const DesiredMaterialRole role,
                          const Hyprland::Catalog &catalogV2,
                          const Hyprland::ActionCatalog &actionCatalogV2) {
  // This compact form is only an input bridge to the typed parser. It is
  // neither retained nor hashed. The typed serializer below reconstructs
  // the sole authoritative standalone Desired bytes.
  auto parserInput = Hyprland::JsonSupport::canonicalJson(object);
  parserInput.append('\n');

  const auto parsed = Hyprland::parseDormantDesiredStateV2(
      QByteArrayView(parserInput), catalogV2, actionCatalogV2);
  if (!parsed) {
    return failure<DesiredMaterial>(pendingError(
        materialErrorCode(role, DesiredMaterialFailure::Invalid),
        materialPath(role),
        QStringLiteral(
            "The embedded %1 is not valid under the supplied v2 authorities.")
            .arg(materialName(role))));
  }

  const auto authoritative =
      Hyprland::serializeDormantDesiredStateV2(*parsed.value);
  if (!authoritative || !validStandaloneDesiredBytes(*authoritative.value)) {
    return failure<DesiredMaterial>(pendingError(
        materialErrorCode(role, DesiredMaterialFailure::InvalidSize),
        materialPath(role),
        QStringLiteral(
            "The reconstructed standalone %1 is invalid or oversized.")
            .arg(materialName(role))));
  }
  const auto reparsed = Hyprland::parseDormantDesiredStateV2(
      QByteArrayView(*authoritative.value), catalogV2, actionCatalogV2);
  if (!reparsed || *reparsed.value != *parsed.value) {
    return failure<DesiredMaterial>(pendingError(
        materialErrorCode(role, DesiredMaterialFailure::Unvalidated),
        materialPath(role),
        QStringLiteral(
            "The reconstructed %1 is not the exact typed parser product.")
            .arg(materialName(role))));
  }

  const auto authoritativeObject = Hyprland::JsonSupport::parseStrictObject(
      QByteArrayView(*authoritative.value), Hyprland::maximumDesiredStateBytes,
      64);
  if (!authoritativeObject || *authoritativeObject.value != object) {
    return failure<DesiredMaterial>(pendingError(
        materialErrorCode(role, DesiredMaterialFailure::SemanticMismatch),
        materialPath(role),
        QStringLiteral("The embedded %1 must equal its typed Desired v2 "
                       "reconstruction semantically.")
            .arg(materialName(role))));
  }

  return {
      .value =
          DesiredMaterial{
              .state = *parsed.value,
              .bytes = *authoritative.value,
              .object = object,
          },
      .errors = {},
  };
}

[[nodiscard]] CanonicalJson::Result<QJsonObject>
appliedObject(const AppliedRecordV2 &record) {
  auto encoded = serializeAppliedRecordV2(record);
  if (!encoded) {
    return propagate<QJsonObject>(std::move(encoded));
  }
  return CanonicalJson::parseCanonicalObject(
      QByteArrayView(*encoded.value),
      CanonicalJson::Framing::OneTrailingLineFeed,
      CanonicalJson::TextPolicy::Rfc8785, appliedLimits());
}

[[nodiscard]] CanonicalJson::Result<AppliedRecordV2>
appliedFromObject(const QJsonObject &object) {
  auto encoded = CanonicalJson::serialize(
      object, CanonicalJson::Framing::OneTrailingLineFeed,
      CanonicalJson::TextPolicy::Rfc8785, appliedLimits());
  if (!encoded) {
    return propagate<AppliedRecordV2>(std::move(encoded));
  }
  return parseAppliedRecordV2(QByteArrayView(*encoded.value));
}

[[nodiscard]] std::optional<CanonicalJson::Error>
validateRecordCoherence(const OrdinaryPendingRecordV2 &record,
                        const Hyprland::Catalog &catalogV2,
                        const Hyprland::ActionCatalog &actionCatalogV2) {
  if (record.beforeActivation.has_value() !=
      record.beforeActivationDesired.has_value()) {
    return pendingError(
        QStringLiteral("ordinary-pending.before-activation-desired-presence"),
        QStringLiteral("$.beforeActivationDesired"),
        QStringLiteral("Before-activation Desired material must be present "
                       "exactly when before activation is present."));
  }
  if (!isCanonicalIdentifier(record.authorityId)) {
    return pendingError(QStringLiteral("ordinary-pending.invalid-authority-id"),
                        QStringLiteral("$.authorityId"),
                        QStringLiteral("Pending authority ID is invalid."));
  }
  if (kindName(record.kind).isEmpty()) {
    return pendingError(
        QStringLiteral("ordinary-pending.invalid-kind"),
        QStringLiteral("$.kind"),
        QStringLiteral("Pending kind is outside the closed ordinary grammar."));
  }
  if (phaseName(record.phase).isEmpty()) {
    return pendingError(
        QStringLiteral("ordinary-pending.invalid-phase"),
        QStringLiteral("$.phase"),
        QStringLiteral("Pending phase is outside the closed phase grammar."));
  }
  if (record.candidateSnapshot.authorityId != record.authorityId ||
      record.afterActivation.authorityId != record.authorityId ||
      (record.beforeActivation &&
       record.beforeActivation->authorityId != record.authorityId) ||
      (record.beforeActivationDesired &&
       record.beforeActivationDesired->state.authorityId !=
           record.authorityId)) {
    return pendingError(
        QStringLiteral("ordinary-pending.authority-mismatch"),
        QStringLiteral("$.authorityId"),
        QStringLiteral("Outer, retained Desired material, and activation "
                       "records must share one authority ID."));
  }
  if (!isCanonicalSha256Digest(record.beforeDesiredDigest)) {
    return pendingError(
        QStringLiteral("ordinary-pending.invalid-before-digest"),
        QStringLiteral("$.beforeDesiredDigest"),
        QStringLiteral("Before-Desired digest must be lowercase SHA-256."));
  }
  if (!isCanonicalSha256Digest(record.snapshotDigest)) {
    return pendingError(
        QStringLiteral("ordinary-pending.invalid-snapshot-digest"),
        QStringLiteral("$.snapshotDigest"),
        QStringLiteral("Candidate snapshot digest must be lowercase SHA-256."));
  }
  const auto computedDigest =
      snapshotDigest(QByteArrayView(record.candidateSnapshotBytes));
  if (computedDigest.isEmpty() || record.snapshotDigest != computedDigest ||
      record.afterActivation.snapshotDigest != computedDigest) {
    return pendingError(
        QStringLiteral("ordinary-pending.snapshot-mismatch"),
        QStringLiteral("$.snapshotDigest"),
        QStringLiteral("Candidate and after-activation digests must bind "
                       "reconstructed Desired bytes without the final LF."));
  }

  const auto candidateRevision =
      record.candidateSnapshot.semanticState.revision;
  if (record.afterActivation.revision != candidateRevision) {
    return pendingError(
        QStringLiteral("ordinary-pending.after-revision-mismatch"),
        QStringLiteral("$.afterActivation.revision"),
        QStringLiteral("After-activation revision must match the candidate."));
  }
  if (record.beforeActivation) {
    if (record.beforeActivation->revision > record.expectedRevision) {
      return pendingError(
          QStringLiteral("ordinary-pending.before-revision-ahead"),
          QStringLiteral("$.beforeActivation.revision"),
          QStringLiteral("Before-activation revision cannot exceed the "
                         "expected Desired revision."));
    }
    if (record.beforeActivation->activationNonce ==
        record.afterActivation.activationNonce) {
      return pendingError(
          QStringLiteral("ordinary-pending.activation-nonce-reuse"),
          QStringLiteral("$.afterActivation.activationNonce"),
          QStringLiteral(
              "A pending generation cannot reuse the prior activation nonce."));
    }
    const auto beforeMaterialDigest =
        snapshotDigest(QByteArrayView(record.beforeActivationDesired->bytes));
    if (record.beforeActivationDesired->state.semanticState.revision !=
            record.beforeActivation->revision ||
        beforeMaterialDigest.isEmpty() ||
        beforeMaterialDigest != record.beforeActivation->snapshotDigest) {
      return pendingError(
          QStringLiteral("ordinary-pending.before-activation-desired-mismatch"),
          QStringLiteral("$.beforeActivationDesired"),
          QStringLiteral("Before-activation Desired material must bind the "
                         "exact prior activation revision and digest."));
    }
    if (record.beforeActivation->revision == candidateRevision &&
        record.beforeActivationDesired->bytes !=
            record.candidateSnapshotBytes) {
      return pendingError(
          QStringLiteral("ordinary-pending.same-revision-desired-mismatch"),
          QStringLiteral("$.beforeActivationDesired"),
          QStringLiteral("One authority revision cannot name two different "
                         "Desired documents."));
    }
  }

  if (record.kind == OrdinaryPendingKind::Apply) {
    if (candidateRevision != record.expectedRevision ||
        record.beforeDesiredDigest != record.snapshotDigest) {
      return pendingError(
          QStringLiteral("ordinary-pending.invalid-apply-relation"),
          QStringLiteral("$.candidateSnapshot"),
          QStringLiteral(
              "Apply must journal the exact expected Desired snapshot."));
    }
    return std::nullopt;
  }

  if (!record.beforeActivation ||
      record.expectedRevision == std::numeric_limits<quint64>::max() ||
      candidateRevision != record.expectedRevision + 1) {
    return pendingError(
        QStringLiteral("ordinary-pending.invalid-advancing-relation"),
        QStringLiteral("$.candidateSnapshot"),
        QStringLiteral("Recovery and display preview require a prior "
                       "activation and exactly expectedRevision + 1."));
  }

  if (record.kind == OrdinaryPendingKind::Recovery) {
    auto priorProjection = record.candidateSnapshot;
    priorProjection.semanticState.revision = record.beforeActivation->revision;
    const auto priorBytes =
        Hyprland::serializeDormantDesiredStateV2(priorProjection);
    if (!priorBytes || !validStandaloneDesiredBytes(*priorBytes.value)) {
      return pendingError(
          QStringLiteral("ordinary-pending.invalid-recovery-base"),
          QStringLiteral("$.candidateSnapshot"),
          QStringLiteral("Recovery prior projection is invalid."));
    }
    const auto reparsedPrior = Hyprland::parseDormantDesiredStateV2(
        QByteArrayView(*priorBytes.value), catalogV2, actionCatalogV2);
    if (!reparsedPrior || *reparsedPrior.value != priorProjection ||
        snapshotDigest(*priorBytes.value) !=
            record.beforeActivation->snapshotDigest ||
        record.beforeActivationDesired->state != priorProjection ||
        record.beforeActivationDesired->bytes != *priorBytes.value) {
      return pendingError(
          QStringLiteral("ordinary-pending.invalid-recovery-base"),
          QStringLiteral("$.beforeActivationDesired"),
          QStringLiteral("Recovery candidate content and retained Desired "
                         "material must reconstruct the exact prior applied "
                         "snapshot at its prior revision."));
    }
    return std::nullopt;
  }

  if (record.kind == OrdinaryPendingKind::DisplayPreview) {
    if (record.beforeActivation->revision != record.expectedRevision ||
        record.beforeActivation->snapshotDigest != record.beforeDesiredDigest) {
      return pendingError(
          QStringLiteral("ordinary-pending.invalid-display-base"),
          QStringLiteral("$.beforeActivation"),
          QStringLiteral("Display preview requires the exact current applied "
                         "Desired baseline."));
    }
    return std::nullopt;
  }

  return pendingError(
      QStringLiteral("ordinary-pending.invalid-kind"), QStringLiteral("$.kind"),
      QStringLiteral("Pending kind is outside the closed ordinary grammar."));
}

} // namespace

CanonicalJson::Result<QByteArray> serializeOrdinaryPendingRecordV2(
    const OrdinaryPendingRecordV2 &record, const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2) {
  if (record.beforeActivation.has_value() !=
      record.beforeActivationDesired.has_value()) {
    return failure<QByteArray>(pendingError(
        QStringLiteral("ordinary-pending.before-activation-desired-presence"),
        QStringLiteral("$.beforeActivationDesired"),
        QStringLiteral("Before-activation Desired material must be present "
                       "exactly when before activation is present.")));
  }

  auto candidate = desiredMaterialForSerialization(
      record.candidateSnapshot, record.candidateSnapshotBytes,
      DesiredMaterialRole::Candidate, catalogV2, actionCatalogV2);
  if (!candidate) {
    return propagate<QByteArray>(std::move(candidate));
  }

  std::optional<DesiredMaterial> beforeDesired;
  if (record.beforeActivationDesired) {
    auto encodedBeforeDesired = desiredMaterialForSerialization(
        record.beforeActivationDesired->state,
        record.beforeActivationDesired->bytes,
        DesiredMaterialRole::BeforeActivation, catalogV2, actionCatalogV2);
    if (!encodedBeforeDesired) {
      return propagate<QByteArray>(std::move(encodedBeforeDesired));
    }
    beforeDesired = std::move(*encodedBeforeDesired.value);
  }

  auto after = appliedObject(record.afterActivation);
  if (!after) {
    return propagate<QByteArray>(std::move(after));
  }
  std::optional<QJsonObject> before;
  if (record.beforeActivation) {
    auto encodedBefore = appliedObject(*record.beforeActivation);
    if (!encodedBefore) {
      return propagate<QByteArray>(std::move(encodedBefore));
    }
    before = *encodedBefore.value;
  }

  if (const auto invalid =
          validateRecordCoherence(record, catalogV2, actionCatalogV2)) {
    return failure<QByteArray>(*invalid);
  }

  const QJsonObject object{
      {
          QStringLiteral("formatVersion"),
          static_cast<qint64>(ordinaryPendingRecordV2FormatVersion),
      },
      {QStringLiteral("authorityId"), record.authorityId},
      {QStringLiteral("kind"), kindName(record.kind)},
      {QStringLiteral("phase"), phaseName(record.phase)},
      {
          QStringLiteral("expectedRevision"),
          QString::number(record.expectedRevision),
      },
      {
          QStringLiteral("beforeDesiredDigest"),
          record.beforeDesiredDigest,
      },
      {QStringLiteral("candidateSnapshot"), candidate.value->object},
      {QStringLiteral("snapshotDigest"), record.snapshotDigest},
      {QStringLiteral("afterActivation"), *after.value},
      {
          QStringLiteral("beforeActivation"),
          before ? QJsonValue(*before) : QJsonValue::Null,
      },
      {
          QStringLiteral("beforeActivationDesired"),
          beforeDesired ? QJsonValue(beforeDesired->object) : QJsonValue::Null,
      },
  };
  return CanonicalJson::serialize(
      object, CanonicalJson::Framing::OneTrailingLineFeed,
      CanonicalJson::TextPolicy::Rfc8785, pendingLimits());
}

CanonicalJson::Result<OrdinaryPendingRecordV2>
parseOrdinaryPendingRecordV2(const QByteArrayView bytes,
                             const Hyprland::Catalog &catalogV2,
                             const Hyprland::ActionCatalog &actionCatalogV2) {
  auto parsed = CanonicalJson::parseCanonicalObject(
      bytes, CanonicalJson::Framing::OneTrailingLineFeed,
      CanonicalJson::TextPolicy::Rfc8785, pendingLimits());
  if (!parsed) {
    return propagate<OrdinaryPendingRecordV2>(std::move(parsed));
  }

  static const QSet<QString> exactKeys{
      QStringLiteral("formatVersion"),
      QStringLiteral("authorityId"),
      QStringLiteral("kind"),
      QStringLiteral("phase"),
      QStringLiteral("expectedRevision"),
      QStringLiteral("beforeDesiredDigest"),
      QStringLiteral("candidateSnapshot"),
      QStringLiteral("snapshotDigest"),
      QStringLiteral("afterActivation"),
      QStringLiteral("beforeActivation"),
      QStringLiteral("beforeActivationDesired"),
  };
  const auto &object = *parsed.value;
  if (keysOf(object) != exactKeys) {
    return failure<OrdinaryPendingRecordV2>(pendingError(
        QStringLiteral("ordinary-pending.invalid-fields"), QStringLiteral("$"),
        QStringLiteral("Ordinary pending v2 fields must be exact.")));
  }
  const auto version = object.value(QStringLiteral("formatVersion"));
  if (!version.isDouble() ||
      version.toDouble(-1.0) !=
          static_cast<double>(ordinaryPendingRecordV2FormatVersion)) {
    return failure<OrdinaryPendingRecordV2>(pendingError(
        QStringLiteral("ordinary-pending.invalid-version"),
        QStringLiteral("$.formatVersion"),
        QStringLiteral("Ordinary pending format must be exactly 2.")));
  }

  static const QSet<QString> stringKeys{
      QStringLiteral("authorityId"),
      QStringLiteral("kind"),
      QStringLiteral("phase"),
      QStringLiteral("expectedRevision"),
      QStringLiteral("beforeDesiredDigest"),
      QStringLiteral("snapshotDigest"),
  };
  for (const auto &key : stringKeys) {
    if (!object.value(key).isString()) {
      return failure<OrdinaryPendingRecordV2>(pendingError(
          QStringLiteral("ordinary-pending.string-required"),
          QStringLiteral("$.") + key,
          QStringLiteral("Ordinary pending scalar fields must be strings.")));
    }
  }
  if (!object.value(QStringLiteral("candidateSnapshot")).isObject() ||
      !object.value(QStringLiteral("afterActivation")).isObject() ||
      (!object.value(QStringLiteral("beforeActivation")).isNull() &&
       !object.value(QStringLiteral("beforeActivation")).isObject()) ||
      (!object.value(QStringLiteral("beforeActivationDesired")).isNull() &&
       !object.value(QStringLiteral("beforeActivationDesired")).isObject())) {
    return failure<OrdinaryPendingRecordV2>(pendingError(
        QStringLiteral("ordinary-pending.invalid-object-fields"),
        QStringLiteral("$"),
        QStringLiteral(
            "Candidate and after activation must be objects; before activation "
            "and its Desired material must be objects or null.")));
  }

  const auto kind =
      kindFromName(object.value(QStringLiteral("kind")).toString());
  const auto phase =
      phaseFromName(object.value(QStringLiteral("phase")).toString());
  const auto expectedRevision = parseCanonicalUint64(
      object.value(QStringLiteral("expectedRevision")).toString());
  if (!kind || !phase || !expectedRevision) {
    return failure<OrdinaryPendingRecordV2>(
        pendingError(QStringLiteral("ordinary-pending.invalid-scalar-grammar"),
                     QStringLiteral("$"),
                     QStringLiteral("Kind, phase, and expected revision must "
                                    "use their exact closed grammar.")));
  }

  auto after = appliedFromObject(
      object.value(QStringLiteral("afterActivation")).toObject());
  if (!after) {
    return propagate<OrdinaryPendingRecordV2>(std::move(after));
  }
  std::optional<AppliedRecordV2> before;
  if (object.value(QStringLiteral("beforeActivation")).isObject()) {
    auto decodedBefore = appliedFromObject(
        object.value(QStringLiteral("beforeActivation")).toObject());
    if (!decodedBefore) {
      return propagate<OrdinaryPendingRecordV2>(std::move(decodedBefore));
    }
    before = *decodedBefore.value;
  }
  const auto beforeDesiredValue =
      object.value(QStringLiteral("beforeActivationDesired"));
  if (before.has_value() != beforeDesiredValue.isObject()) {
    return failure<OrdinaryPendingRecordV2>(pendingError(
        QStringLiteral("ordinary-pending.before-activation-desired-presence"),
        QStringLiteral("$.beforeActivationDesired"),
        QStringLiteral("Before-activation Desired material must be present "
                       "exactly when before activation is present.")));
  }

  auto candidate = desiredMaterialFromObject(
      object.value(QStringLiteral("candidateSnapshot")).toObject(),
      DesiredMaterialRole::Candidate, catalogV2, actionCatalogV2);
  if (!candidate) {
    return propagate<OrdinaryPendingRecordV2>(std::move(candidate));
  }
  std::optional<OrdinaryPendingDesiredMaterialV2> beforeDesired;
  if (beforeDesiredValue.isObject()) {
    auto decodedBeforeDesired = desiredMaterialFromObject(
        beforeDesiredValue.toObject(), DesiredMaterialRole::BeforeActivation,
        catalogV2, actionCatalogV2);
    if (!decodedBeforeDesired) {
      return propagate<OrdinaryPendingRecordV2>(
          std::move(decodedBeforeDesired));
    }
    beforeDesired = OrdinaryPendingDesiredMaterialV2{
        .state = std::move(decodedBeforeDesired.value->state),
        .bytes = std::move(decodedBeforeDesired.value->bytes),
    };
  }

  OrdinaryPendingRecordV2 record{
      .authorityId = object.value(QStringLiteral("authorityId")).toString(),
      .kind = *kind,
      .phase = *phase,
      .expectedRevision = *expectedRevision,
      .beforeDesiredDigest =
          object.value(QStringLiteral("beforeDesiredDigest")).toString(),
      .beforeActivationDesired = std::move(beforeDesired),
      .candidateSnapshot = candidate.value->state,
      .candidateSnapshotBytes = candidate.value->bytes,
      .snapshotDigest =
          object.value(QStringLiteral("snapshotDigest")).toString(),
      .afterActivation = *after.value,
      .beforeActivation = std::move(before),
  };
  if (const auto invalid =
          validateRecordCoherence(record, catalogV2, actionCatalogV2)) {
    return failure<OrdinaryPendingRecordV2>(*invalid);
  }

  const auto reencoded =
      serializeOrdinaryPendingRecordV2(record, catalogV2, actionCatalogV2);
  if (!reencoded || QByteArrayView(*reencoded.value) != bytes) {
    return failure<OrdinaryPendingRecordV2>(pendingError(
        QStringLiteral("ordinary-pending.noncanonical-reconstruction"),
        QStringLiteral("$"),
        QStringLiteral("Ordinary pending bytes do not equal their typed "
                       "reconstruction.")));
  }

  return {
      .value = std::move(record),
      .errors = {},
  };
}

} // namespace HyprShelld::Compositor
