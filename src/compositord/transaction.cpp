#include "transaction.h"

#include "activation_requirement.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>
#include <utility>

#include <fcntl.h>

namespace HyprShelld::Compositor {
namespace {

using Hyprland::DesiredState;

constexpr char preSharedSpacingActionCatalogDigest[] =
    "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2";
constexpr char sharedSpacingActionCatalogDigest[] =
    "1438f04a169b4ecfc945078403d6286154bc89a0e32cb3a1a5073d209e0c358b";
constexpr char sharedSpacingConfigSchemaDigest[] =
    "75e299cc9f5d3a3289450df089cad4aa22efd26ba2f11fbbf27d46f78b898202";
constexpr char preDeviceQuarantineCatalogDigest[] =
    "0232f9b036849e2b9423d5960dd32f22001c79b5b6b6696330f481d1d0c657e0";
constexpr char deviceQuarantineCatalogDigest[] =
    "10d6c8bfd757407e93ebb379afb7f29df89dacd2a75936ab3d2cbe873d539388";
constexpr char bindingsQuarantineCatalogDigest[] =
    "402c8a8c570dd3760d4d7bea8c358c7f12021a7c51457e62a4771d69a581254b";

[[nodiscard]] bool acceptsPreSharedSpacingAuthority(
    const Hyprland::ActionCatalog &actions
) {
  return Hyprland::actionCatalogDigest(actions)
          == QLatin1String(sharedSpacingActionCatalogDigest)
      && actions.configSchemaDigest
          == QLatin1String(sharedSpacingConfigSchemaDigest);
}

[[nodiscard]] bool acceptsBindingsQuarantineAuthority(
    const Hyprland::Catalog &catalog
) {
  return Hyprland::catalogDigest(catalog)
      == QLatin1String(bindingsQuarantineCatalogDigest);
}

[[nodiscard]] int duplicateDescriptor(const int descriptor)
{
  int duplicate = -1;
  do {
    duplicate = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 3);
  } while (duplicate < 0 && errno == EINTR);
  return duplicate;
}

[[nodiscard]] QString hashBytes(const QByteArrayView bytes) {
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] bool validSha256(const QStringView value) {
  if (value.size() != 64)
    return false;
  for (const auto character : value) {
    if (!((character >= u'0' && character <= u'9') ||
          (character >= u'a' && character <= u'f')))
      return false;
  }
  return true;
}

[[nodiscard]] QJsonObject
compatibleHyprlandObject(const Hyprland::Catalog &catalog) {
  QJsonObject result{
      {QStringLiteral("major"), static_cast<qint64>(catalog.hyprland.major)},
      {QStringLiteral("minor"), static_cast<qint64>(catalog.hyprland.minor)},
      {QStringLiteral("reviewedVersion"),
       Hyprland::toString(catalog.hyprland.reviewedVersion)},
      {QStringLiteral("minimumPatch"),
       static_cast<qint64>(catalog.hyprland.minimumPatch)},
  };
  result.insert(
      QStringLiteral("maximumPatch"),
      catalog.hyprland.maximumPatch
          ? QJsonValue(static_cast<qint64>(*catalog.hyprland.maximumPatch))
          : QJsonValue::Null);
  return result;
}

[[nodiscard]] QString describeErrors(const Hyprland::ValidationErrors &errors) {
  QStringList result;
  for (const auto &error : errors) {
    result.append(QStringLiteral("%1: %2").arg(error.code, error.message));
    if (result.size() == 4)
      break;
  }
  return result.join(QStringLiteral("; "));
}

[[nodiscard]] bool parseRevision(const QString &text, quint64 &revision) {
  if (text.isEmpty() || (text.size() > 1 && text.front() == u'0'))
    return false;
  for (const auto character : text) {
    if (character < u'0' || character > u'9')
      return false;
  }
  bool converted = false;
  revision = text.toULongLong(&converted, 10);
  return converted;
}

[[nodiscard]] QSet<QString> keysOf(const QJsonObject &object) {
  QSet<QString> result;
  for (auto iterator = object.constBegin(); iterator != object.constEnd();
       ++iterator)
    result.insert(iterator.key());
  return result;
}

struct CompatibleDesiredState final {
  DesiredState state;
  bool preSharedSpacingAuthority = false;
  bool preDeviceQuarantineAuthority = false;
  bool preBindingsQuarantineAuthority = false;
};

[[nodiscard]] std::optional<CompatibleDesiredState>
parseCompatibleDesiredState(
    const QByteArrayView bytes,
    const Hyprland::Catalog &catalog,
    const Hyprland::ActionCatalog &actions
) {
  const auto current = Hyprland::parseDesiredState(bytes, catalog, actions);
  if (current
      && Hyprland::serializeDesiredState(*current.value) == bytes) {
    return CompatibleDesiredState{.state = *current.value};
  }

  const auto object = Hyprland::JsonSupport::parseStrictObject(
      bytes, Hyprland::maximumDesiredStateBytes, 64
  );
  if (!object) {
    return std::nullopt;
  }

  const auto storedCatalogDigest = object.value->value(
      QStringLiteral("catalogDigest")
  ).toString();
  const auto storedActionCatalogDigest = object.value->value(
      QStringLiteral("actionCatalogDigest")
  ).toString();
  const auto preDeviceQuarantineAuthority =
      acceptsBindingsQuarantineAuthority(catalog)
      && storedCatalogDigest
          == QLatin1String(preDeviceQuarantineCatalogDigest);
  const auto preBindingsQuarantineAuthority =
      acceptsBindingsQuarantineAuthority(catalog)
      && storedCatalogDigest
          == QLatin1String(deviceQuarantineCatalogDigest);
  const auto currentCatalogAuthority =
      storedCatalogDigest == Hyprland::catalogDigest(catalog);
  const auto preSharedSpacingAuthority =
      acceptsPreSharedSpacingAuthority(actions)
      && storedActionCatalogDigest
          == QLatin1String(preSharedSpacingActionCatalogDigest);
  const auto currentActionAuthority =
      storedActionCatalogDigest == Hyprland::actionCatalogDigest(actions);
  if ((!preDeviceQuarantineAuthority
       && !preBindingsQuarantineAuthority
       && !currentCatalogAuthority)
      || (!preSharedSpacingAuthority && !currentActionAuthority)
      || (!preDeviceQuarantineAuthority
          && !preBindingsQuarantineAuthority
          && !preSharedSpacingAuthority)) {
    return std::nullopt;
  }

  auto migratedObject = *object.value;
  migratedObject.insert(
      QStringLiteral("catalogDigest"), Hyprland::catalogDigest(catalog)
  );
  migratedObject.insert(
      QStringLiteral("actionCatalogDigest"),
      Hyprland::actionCatalogDigest(actions)
  );
  auto migratedBytes = Hyprland::JsonSupport::canonicalJson(migratedObject);
  migratedBytes.append('\n');
  const auto migrated = Hyprland::parseDesiredState(
      QByteArrayView(migratedBytes), catalog, actions
  );
  if (!migrated || migrated.value->readOnly) {
    return std::nullopt;
  }
  if (preSharedSpacingAuthority) {
    for (const auto &rule : migrated.value->workspaceRules) {
      if (rule.id == QLatin1String(Hyprland::sharedSpacingWorkspaceRuleId)
          || rule.selector == QLatin1String(
              Hyprland::sharedSpacingWorkspaceRuleSelector
          )) {
        return std::nullopt;
      }
    }
  }

  auto legacyState = *migrated.value;
  legacyState.catalogDigest = storedCatalogDigest;
  legacyState.actionCatalogDigest = storedActionCatalogDigest;
  if (Hyprland::serializeDesiredState(legacyState) != bytes) {
    return std::nullopt;
  }
  return CompatibleDesiredState{
      .state = std::move(legacyState),
      .preSharedSpacingAuthority = preSharedSpacingAuthority,
      .preDeviceQuarantineAuthority = preDeviceQuarantineAuthority,
      .preBindingsQuarantineAuthority = preBindingsQuarantineAuthority,
  };
}

[[nodiscard]] std::optional<DesiredState> currentAuthorityState(
    const CompatibleDesiredState &stored,
    const Hyprland::Catalog &catalog,
    const Hyprland::ActionCatalog &actions
) {
  if (!stored.preSharedSpacingAuthority
      && !stored.preDeviceQuarantineAuthority
      && !stored.preBindingsQuarantineAuthority) {
    return stored.state;
  }
  auto current = stored.state;
  current.catalogDigest = Hyprland::catalogDigest(catalog);
  current.actionCatalogDigest = Hyprland::actionCatalogDigest(actions);
  const auto bytes = Hyprland::serializeDesiredState(current);
  const auto verified = Hyprland::parseDesiredState(
      QByteArrayView(bytes), catalog, actions
  );
  if (!verified || *verified.value != current) {
    return std::nullopt;
  }
  return current;
}

[[nodiscard]] std::optional<ActivationRequirement>
parseRequirement(const QString &name) {
  if (name == QStringLiteral("reload"))
    return ActivationRequirement::Reload;
  if (name == QStringLiteral("restart"))
    return ActivationRequirement::Restart;
  if (name == QStringLiteral("session"))
    return ActivationRequirement::Session;
  return std::nullopt;
}

struct AppliedRecord final {
  quint64 revision = 0;
  QString snapshotDigest;
  QString generation;
  QString nonce;
  QString entrypoint;
  ActivationRequirement requirement = ActivationRequirement::Reload;

  friend bool operator==(const AppliedRecord &,
                         const AppliedRecord &) = default;
};

[[nodiscard]] QJsonObject appliedObject(const AppliedRecord &record) {
  return {
      {QStringLiteral("formatVersion"), 1},
      {QStringLiteral("revision"), QString::number(record.revision)},
      {QStringLiteral("snapshotDigest"), record.snapshotDigest},
      {QStringLiteral("generation"), record.generation},
      {QStringLiteral("activationNonce"), record.nonce},
      {QStringLiteral("entrypoint"), record.entrypoint},
      {QStringLiteral("requiredActivation"),
       activationRequirementName(record.requirement)},
  };
}

[[nodiscard]] QByteArray appliedBytes(const AppliedRecord &record) {
  auto bytes = Hyprland::JsonSupport::canonicalJson(appliedObject(record));
  bytes.append('\n');
  return bytes;
}

[[nodiscard]] std::optional<AppliedRecord>
parseAppliedObject(const QJsonObject &object) {
  static const QSet<QString> expected{
      QStringLiteral("formatVersion"),      QStringLiteral("revision"),
      QStringLiteral("snapshotDigest"),     QStringLiteral("generation"),
      QStringLiteral("activationNonce"),    QStringLiteral("entrypoint"),
      QStringLiteral("requiredActivation"),
  };
  quint64 revision = 0;
  const auto requirement = parseRequirement(
      object.value(QStringLiteral("requiredActivation")).toString());
  const auto digest = object.value(QStringLiteral("snapshotDigest")).toString();
  const auto generation = object.value(QStringLiteral("generation")).toString();
  const auto nonce = object.value(QStringLiteral("activationNonce")).toString();
  const auto entrypoint = object.value(QStringLiteral("entrypoint")).toString();
  if (keysOf(object) != expected ||
      object.value(QStringLiteral("formatVersion")).toInt(-1) != 1 ||
      !parseRevision(object.value(QStringLiteral("revision")).toString(),
                     revision) ||
      digest.size() != 64 || generation.size() != 64 || nonce.size() < 32 ||
      nonce.size() > 128 || entrypoint.isEmpty() || !requirement)
    return std::nullopt;
  return AppliedRecord{
      .revision = revision,
      .snapshotDigest = digest,
      .generation = generation,
      .nonce = nonce,
      .entrypoint = entrypoint,
      .requirement = *requirement,
  };
}

[[nodiscard]] std::optional<AppliedRecord>
parseAppliedBytes(const QByteArrayView bytes) {
  const auto parsed =
      Hyprland::JsonSupport::parseStrictObject(bytes, 4 * 1024 * 1024, 16);
  if (!parsed)
    return std::nullopt;
  const auto record = parseAppliedObject(*parsed.value);
  if (!record || QByteArrayView(appliedBytes(*record)) != bytes) {
    return std::nullopt;
  }
  return record;
}

enum class PendingKind { Apply, Recovery, DisplayPreview };
enum class PendingPhase { Prepared, Committing };

struct PendingRecord final {
  PendingKind kind = PendingKind::Apply;
  PendingPhase phase = PendingPhase::Prepared;
  quint64 expectedRevision = 0;
  QString beforeDesiredDigest;
  DesiredState candidate;
  QByteArray candidateBytes;
  QString snapshotDigest;
  AppliedRecord after;
  std::optional<AppliedRecord> before;
};

[[nodiscard]] QJsonObject snapshotObject(const QByteArrayView bytes) {
  const auto parsed = Hyprland::JsonSupport::parseStrictObject(
      bytes, Hyprland::maximumDesiredStateBytes, 64);
  return parsed ? *parsed.value : QJsonObject{};
}

[[nodiscard]] QByteArray pendingBytes(const PendingRecord &pending) {
  const auto kindName = pending.kind == PendingKind::Apply
      ? QStringLiteral("apply")
      : pending.kind == PendingKind::Recovery
          ? QStringLiteral("recovery")
          : QStringLiteral("display-preview");
  QJsonObject object{
      {QStringLiteral("formatVersion"), 1},
      {QStringLiteral("kind"), kindName},
      {QStringLiteral("phase"), pending.phase == PendingPhase::Prepared
                                    ? QStringLiteral("prepared")
                                    : QStringLiteral("committing")},
      {QStringLiteral("expectedRevision"),
       QString::number(pending.expectedRevision)},
      {QStringLiteral("beforeDesiredDigest"), pending.beforeDesiredDigest},
      {QStringLiteral("candidateSnapshot"),
       snapshotObject(pending.candidateBytes)},
      {QStringLiteral("snapshotDigest"), pending.snapshotDigest},
      {QStringLiteral("afterActivation"), appliedObject(pending.after)},
      {QStringLiteral("beforeActivation"),
       pending.before ? QJsonValue(appliedObject(*pending.before))
                      : QJsonValue::Null},
  };
  auto bytes = Hyprland::JsonSupport::canonicalJson(object);
  bytes.append('\n');
  return bytes;
}

[[nodiscard]] std::optional<PendingRecord>
parsePendingBytes(const QByteArrayView bytes, const Hyprland::Catalog &catalog,
                  const Hyprland::ActionCatalog &actions) {
  const auto parsed =
      Hyprland::JsonSupport::parseStrictObject(bytes, 4 * 1024 * 1024, 64);
  if (!parsed)
    return std::nullopt;
  const auto object = *parsed.value;
  static const QSet<QString> expected{
      QStringLiteral("formatVersion"),
      QStringLiteral("kind"),
      QStringLiteral("phase"),
      QStringLiteral("expectedRevision"),
      QStringLiteral("beforeDesiredDigest"),
      QStringLiteral("candidateSnapshot"),
      QStringLiteral("snapshotDigest"),
      QStringLiteral("afterActivation"),
      QStringLiteral("beforeActivation"),
  };
  quint64 expectedRevision = 0;
  const auto beforeDesiredDigest =
      object.value(QStringLiteral("beforeDesiredDigest")).toString();
  if (keysOf(object) != expected ||
      object.value(QStringLiteral("formatVersion")).toInt(-1) != 1 ||
      !parseRevision(
          object.value(QStringLiteral("expectedRevision")).toString(),
          expectedRevision) ||
      !validSha256(beforeDesiredDigest) ||
      !object.value(QStringLiteral("candidateSnapshot")).isObject() ||
      !object.value(QStringLiteral("afterActivation")).isObject()) {
    return std::nullopt;
  }
  PendingKind kind;
  const auto kindName = object.value(QStringLiteral("kind")).toString();
  if (kindName == QStringLiteral("apply"))
    kind = PendingKind::Apply;
  else if (kindName == QStringLiteral("recovery"))
    kind = PendingKind::Recovery;
  else if (kindName == QStringLiteral("display-preview"))
    kind = PendingKind::DisplayPreview;
  else
    return std::nullopt;
  PendingPhase phase;
  const auto phaseName = object.value(QStringLiteral("phase")).toString();
  if (phaseName == QStringLiteral("prepared"))
    phase = PendingPhase::Prepared;
  else if (phaseName == QStringLiteral("committing"))
    phase = PendingPhase::Committing;
  else
    return std::nullopt;

  auto candidateBytes = Hyprland::JsonSupport::canonicalJson(
      object.value(QStringLiteral("candidateSnapshot")));
  candidateBytes.append('\n');
  const auto candidate = parseCompatibleDesiredState(
      QByteArrayView(candidateBytes), catalog, actions
  );
  const auto after = parseAppliedObject(
      object.value(QStringLiteral("afterActivation")).toObject());
  if (!candidate || !after || after->revision != candidate->state.revision ||
      after->snapshotDigest != hashBytes(candidateBytes) ||
      object.value(QStringLiteral("snapshotDigest")).toString() !=
          after->snapshotDigest)
    return std::nullopt;
  std::optional<AppliedRecord> before;
  if (!object.value(QStringLiteral("beforeActivation")).isNull()) {
    if (!object.value(QStringLiteral("beforeActivation")).isObject()) {
      return std::nullopt;
    }
    before = parseAppliedObject(
        object.value(QStringLiteral("beforeActivation")).toObject());
    if (!before)
      return std::nullopt;
  }
  if ((kind == PendingKind::Apply &&
       candidate->state.revision != expectedRevision) ||
      ((kind == PendingKind::Recovery ||
        kind == PendingKind::DisplayPreview) &&
       (expectedRevision == std::numeric_limits<quint64>::max() ||
        candidate->state.revision != expectedRevision + 1))) {
    return std::nullopt;
  }
  if (kind == PendingKind::Apply &&
      beforeDesiredDigest != after->snapshotDigest) {
    return std::nullopt;
  }
  PendingRecord result{
      .kind = kind,
      .phase = phase,
      .expectedRevision = expectedRevision,
      .beforeDesiredDigest = beforeDesiredDigest,
      .candidate = std::move(candidate->state),
      .candidateBytes = std::move(candidateBytes),
      .snapshotDigest = after->snapshotDigest,
      .after = *after,
      .before = before,
  };
  if (QByteArrayView(pendingBytes(result)) != bytes)
    return std::nullopt;
  return result;
}

[[nodiscard]] AuthorityResult
authorityFailure(const QString &code, const QString &message,
                 const AuthoritySnapshot &snapshot) {
  return {
      .success = false,
      .errorCode = code,
      .errorMessage = message,
      .snapshot = snapshot,
  };
}

} // namespace

struct ConfigurationTransaction::Impl final {
  StorePaths paths;
  Hyprland::Catalog catalog;
  Hyprland::ActionCatalog actions;
  PersistentStore store;
  GenerationStore generations;
  bool initialized = false;
  bool failed = false;
  QString loadState = QStringLiteral("unavailable");
  DesiredState desired;
  QByteArray desiredBytes;
  QString desiredDigest;
  std::optional<DesiredState> appliedState;
  QByteArray lastGoodBytes;
  std::optional<AppliedRecord> applied;
  std::optional<PendingRecord> pending;
  bool desiredSemanticallyMatchesApplied = false;

  Impl(StorePaths storePaths, Hyprland::Catalog scalarCatalog,
       Hyprland::ActionCatalog actionCatalog)
      : paths(std::move(storePaths)), catalog(std::move(scalarCatalog)),
        actions(std::move(actionCatalog)), store(paths), generations(store) {}

  [[nodiscard]] AuthoritySnapshot snapshot() const {
    AuthoritySnapshot result;
    if (!initialized || failed || desiredBytes.isEmpty()) {
      result.loadState = failed ? QStringLiteral("unavailable") : loadState;
      result.applyState =
          failed ? QStringLiteral("failed") : QStringLiteral("unavailable");
      // Losing desired-state authority does not erase the last durable
      // applied tuple. Preserve it so GenerationDigest remains empty
      // exactly when no managed generation has ever been activated.
      if (applied) {
        result.appliedRevision = applied->revision;
        result.generationDigest = applied->generation;
      }
      return result;
    }
    result.available = true;
    result.writable = !desired.readOnly && !pending.has_value();
    result.desiredState = desiredBytes;
    if (appliedState) {
      result.appliedDesiredState = Hyprland::serializeDesiredState(*appliedState);
    }
    result.revision = desired.revision;
    result.catalogDigest = desired.catalogDigest;
    result.actionCatalogDigest = desired.actionCatalogDigest;
    result.loadState = loadState;
    if (applied) {
      result.appliedRevision = applied->revision;
      result.generationDigest = applied->generation;
    }
    if (desired.readOnly) {
      result.applyState = QStringLiteral("retained");
      return result;
    }
    if (applied
        && (applied->snapshotDigest == desiredDigest
            || desiredSemanticallyMatchesApplied)
        && !pending) {
      result.applyState = QStringLiteral("current");
      return result;
    }
    result.applyState =
        applied ? QStringLiteral("retained") : QStringLiteral("inactive");
    if (pending) {
      result.requiredActivation = pending->after.requirement;
    } else {
      result.requiredActivation = activationRequirementForDelta(
          appliedState ? &*appliedState : nullptr, desired, catalog
      );
    }
    return result;
  }

  [[nodiscard]] AuthorityResult fail(QString code, QString message) const {
    return authorityFailure(code, message, snapshot());
  }

  [[nodiscard]] bool readCanonicalSnapshot(const StoreFile file,
                                           QByteArray &bytes,
                                           DesiredState &state,
                                           QString &error) const {
    const auto stored = store.read(file);
    if (!stored.present()) {
      error = stored.status == StoreReadStatus::Missing
                  ? QStringLiteral("A required compositor snapshot is missing")
                  : stored.errorMessage;
      return false;
    }
    const auto parsed = parseCompatibleDesiredState(
        QByteArrayView(stored.bytes), catalog, actions
    );
    if (!parsed) {
      error = QStringLiteral(
          "A persistent compositor snapshot is invalid or non-canonical");
      return false;
    }
    bytes = stored.bytes;
    state = parsed->state;
    return true;
  }

  [[nodiscard]] bool persistentStateMatchesBefore(const PendingRecord &record,
                                                  QString &error) const {
    QByteArray currentDesiredBytes;
    DesiredState currentDesired;
    if (!readCanonicalSnapshot(StoreFile::Desired, currentDesiredBytes,
                               currentDesired, error) ||
        currentDesired.revision != record.expectedRevision ||
        hashBytes(currentDesiredBytes) != record.beforeDesiredDigest) {
      if (error.isEmpty()) {
        error = QStringLiteral(
            "The desired snapshot changed after the transaction was prepared");
      }
      return false;
    }

    const auto activationRead = store.read(StoreFile::Activation);
    if (record.before) {
      if (!activationRead.present() ||
          activationRead.bytes != appliedBytes(*record.before)) {
        error = QStringLiteral(
            "The activation tuple changed after the transaction was prepared");
        return false;
      }
    } else if (activationRead.status != StoreReadStatus::Missing) {
      error = activationRead.present()
                  ? QStringLiteral("An activation tuple appeared after the "
                                   "transaction was prepared")
                  : activationRead.errorMessage;
      return false;
    }

    const auto lastGoodRead = store.read(StoreFile::LastGood);
    if (record.before) {
      if (!lastGoodRead.present()) {
        error = lastGoodRead.status == StoreReadStatus::Missing
                    ? QStringLiteral("The prior last-good snapshot disappeared")
                    : lastGoodRead.errorMessage;
        return false;
      }
      const auto parsed = parseCompatibleDesiredState(
          QByteArrayView(lastGoodRead.bytes), catalog, actions
      );
      if (!parsed || parsed->state.readOnly
          || parsed->state.revision != record.before->revision ||
          hashBytes(lastGoodRead.bytes) != record.before->snapshotDigest) {
        error = QStringLiteral(
            "The prior last-good snapshot changed after prepare");
        return false;
      }
    } else if (lastGoodRead.status != StoreReadStatus::Missing) {
      error = lastGoodRead.present()
                  ? QStringLiteral("A last-good snapshot appeared after the "
                                   "transaction was prepared")
                  : lastGoodRead.errorMessage;
      return false;
    }
    return true;
  }

  [[nodiscard]] bool crossCheckGeneration(const PendingRecord &record,
                                          QString &error) const {
    const auto verified = generations.verify(record.after.nonce);
    if (!verified.success || !verified.generation) {
      error = verified.errorMessage;
      return false;
    }
    const auto &generation = *verified.generation;
    const auto candidateCatalogDigest = record.candidate.catalogDigest;
    const auto catalogAuthorityCompatible =
        candidateCatalogDigest == Hyprland::catalogDigest(catalog)
        || (acceptsBindingsQuarantineAuthority(catalog)
            && (candidateCatalogDigest == QLatin1String(
                    preDeviceQuarantineCatalogDigest
                )
                || candidateCatalogDigest == QLatin1String(
                    deviceQuarantineCatalogDigest
                )));
    const auto candidateActionDigest = record.candidate.actionCatalogDigest;
    const auto actionAuthorityCompatible =
        candidateActionDigest == Hyprland::actionCatalogDigest(actions)
        || (acceptsPreSharedSpacingAuthority(actions)
            && candidateActionDigest == QLatin1String(
                preSharedSpacingActionCatalogDigest
            ));
    if (generation.id != record.after.generation ||
        generation.entrypoint != record.after.entrypoint ||
        generation.snapshotDigest != record.snapshotDigest ||
        generation.revision != record.candidate.revision) {
      error = QStringLiteral(
          "The immutable generation does not match the transaction journal");
      return false;
    }
    const auto manifest = Hyprland::JsonSupport::parseStrictObject(
        QByteArrayView(generation.manifest), 4 * 1024 * 1024, 32);
    if (!manifest ||
        manifest.value->value(QStringLiteral("catalogDigest")).toString() !=
            candidateCatalogDigest ||
        manifest.value->value(QStringLiteral("actionCatalogDigest"))
                .toString() != candidateActionDigest ||
        manifest.value->value(QStringLiteral("targetHyprland")).toString() !=
            record.candidate.targetHyprland ||
        !manifest.value->value(QStringLiteral("compatibleHyprland"))
             .isObject() ||
        manifest.value->value(QStringLiteral("compatibleHyprland"))
                .toObject() != compatibleHyprlandObject(catalog) ||
        !catalogAuthorityCompatible ||
        !actionAuthorityCompatible) {
      error = QStringLiteral(
          "The generation is outside the active catalog authority");
      return false;
    }
    return true;
  }

  [[nodiscard]] bool reconcilePending(QString &error) {
    const auto stored = store.read(StoreFile::Pending);
    if (stored.status == StoreReadStatus::Missing)
      return true;
    if (!stored.present()) {
      error = stored.errorMessage;
      return false;
    }
    auto parsed =
        parsePendingBytes(QByteArrayView(stored.bytes), catalog, actions);
    if (!parsed) {
      error = QStringLiteral("The pending compositor transaction is invalid");
      return false;
    }
    pending = std::move(*parsed);
    if (!crossCheckGeneration(*pending, error))
      return false;

    QByteArray currentDesiredBytes;
    DesiredState currentDesired;
    if (!readCanonicalSnapshot(StoreFile::Desired, currentDesiredBytes,
                               currentDesired, error)) {
      return false;
    }
    const auto currentDesiredDigest = hashBytes(currentDesiredBytes);
    const auto desiredIsBefore =
        currentDesired.revision == pending->expectedRevision &&
        currentDesiredDigest == pending->beforeDesiredDigest;
    const auto desiredIsAfter =
        currentDesired.revision == pending->candidate.revision &&
        currentDesiredDigest == pending->snapshotDigest;
    if (!desiredIsBefore && !desiredIsAfter) {
      error = QStringLiteral(
          "Pending recovery found an unrelated desired snapshot");
      return false;
    }

    const auto activationRead = store.read(StoreFile::Activation);
    std::optional<AppliedRecord> currentActivation;
    if (activationRead.present()) {
      currentActivation = parseAppliedBytes(activationRead.bytes);
      if (!currentActivation) {
        error =
            QStringLiteral("The activation record is invalid during recovery");
        return false;
      }
    } else if (activationRead.status != StoreReadStatus::Missing) {
      error = activationRead.errorMessage;
      return false;
    }
    const auto activationIsBefore = currentActivation == pending->before;
    const auto activationIsAfter =
        currentActivation && *currentActivation == pending->after;
    if (!activationIsBefore && !activationIsAfter) {
      error = QStringLiteral(
          "Pending recovery found an unrelated activation tuple");
      return false;
    }

    const auto lastGoodRead = store.read(StoreFile::LastGood);
    bool lastGoodIsBefore = false;
    bool lastGoodIsAfter = false;
    if (lastGoodRead.status == StoreReadStatus::Missing) {
      lastGoodIsBefore = !pending->before;
    } else if (!lastGoodRead.present()) {
      error = lastGoodRead.errorMessage;
      return false;
    } else {
      const auto parsedLast = parseCompatibleDesiredState(
          QByteArrayView(lastGoodRead.bytes), catalog, actions
      );
      if (!parsedLast || parsedLast->state.readOnly) {
        error = QStringLiteral(
            "Pending recovery found an invalid last-good snapshot");
        return false;
      }
      const auto lastGoodDigest = hashBytes(lastGoodRead.bytes);
      lastGoodIsAfter =
          parsedLast->state.revision == pending->candidate.revision &&
          lastGoodDigest == pending->snapshotDigest;
      lastGoodIsBefore =
          pending->before &&
          parsedLast->state.revision == pending->before->revision &&
          lastGoodDigest == pending->before->snapshotDigest;
    }
    if (!lastGoodIsBefore && !lastGoodIsAfter) {
      error = QStringLiteral(
          "Pending recovery found an unrelated last-good snapshot");
      return false;
    }

    if (pending->phase == PendingPhase::Prepared) {
      if (!desiredIsBefore || !activationIsBefore || !lastGoodIsBefore) {
        error = QStringLiteral(
            "Prepared transaction recovery found modified authority state");
        return false;
      }
      const auto removed = store.remove(StoreFile::Pending);
      if (!removed.success) {
        error = removed.errorMessage;
        return false;
      }
      pending.reset();
      loadState = QStringLiteral("recovered");
      return true;
    }

    // "committing" is the durable success decision written only after the
    // executor proved the exact nonce. Roll forward every mirror.
    if ((activationIsAfter && (!desiredIsAfter || !lastGoodIsAfter)) ||
        (lastGoodIsAfter && !desiredIsAfter)) {
      error = QStringLiteral(
          "Confirmed transaction mirrors violate durable write order");
      return false;
    }
    if (!store.write(StoreFile::Desired, pending->candidateBytes).success ||
        !store.write(StoreFile::LastGood, pending->candidateBytes).success ||
        !store.write(StoreFile::Activation, appliedBytes(pending->after))
             .success) {
      error = QStringLiteral(
          "Cannot roll forward a confirmed compositor transaction");
      return false;
    }
    const auto removed = store.remove(StoreFile::Pending);
    if (!removed.success) {
      error = removed.errorMessage;
      return false;
    }
    pending.reset();
    loadState = QStringLiteral("recovered");
    return true;
  }

  [[nodiscard]] bool loadSnapshots(QString &error) {
    desiredSemanticallyMatchesApplied = false;
    auto desiredRead = store.read(StoreFile::Desired);
    auto lastGoodRead = store.read(StoreFile::LastGood);
    const auto activationRead = store.read(StoreFile::Activation);
    if ((desiredRead.status != StoreReadStatus::Missing &&
         !desiredRead.present()) ||
        (lastGoodRead.status != StoreReadStatus::Missing &&
         !lastGoodRead.present()) ||
        (activationRead.status != StoreReadStatus::Missing &&
         !activationRead.present())) {
      error = !desiredRead.errorMessage.isEmpty() ? desiredRead.errorMessage
              : !lastGoodRead.errorMessage.isEmpty()
                  ? lastGoodRead.errorMessage
                  : activationRead.errorMessage;
      return false;
    }

    if (desiredRead.status == StoreReadStatus::Missing &&
        lastGoodRead.status == StoreReadStatus::Missing) {
      if (activationRead.present()) {
        error = QStringLiteral(
            "Activation metadata exists without either desired snapshot");
        return false;
      }
      desired = Hyprland::defaultDesiredState(catalog, actions);
      desiredBytes = Hyprland::serializeDesiredState(desired);
      const auto written = store.write(StoreFile::Desired, desiredBytes);
      if (!written.success) {
        error = written.errorMessage;
        return false;
      }
      desiredRead = {.status = StoreReadStatus::Present, .bytes = desiredBytes};
      loadState = QStringLiteral("defaulted");
    }

    if (desiredRead.status == StoreReadStatus::Missing) {
      // last-good is intentionally not a revision watermark. If a newer
      // desired snapshot was lost, deriving last-good+1 could decrement
      // or reuse an already-issued CAS token (ABA).
      error =
          QStringLiteral("The desired snapshot is missing while last-good "
                         "exists; monotonic revision recovery is unavailable");
      return false;
    }

    const auto parsedDesired = parseCompatibleDesiredState(
        QByteArrayView(desiredRead.bytes), catalog, actions
    );
    if (!parsedDesired) {
      error = QStringLiteral(
          "The desired snapshot is invalid or outside compatible authority"
      );
      return false;
    }
    const auto currentDesired = currentAuthorityState(
        *parsedDesired, catalog, actions
    );
    if (!currentDesired) {
      error = QStringLiteral(
          "The desired snapshot cannot be migrated to current authority"
      );
      return false;
    }
    desired = *currentDesired;
    desiredBytes = Hyprland::serializeDesiredState(desired);
    const auto migrateDesiredAuthority =
        parsedDesired->preSharedSpacingAuthority
        || parsedDesired->preDeviceQuarantineAuthority
        || parsedDesired->preBindingsQuarantineAuthority;
    if (!migrateDesiredAuthority && desiredBytes != desiredRead.bytes) {
      error = QStringLiteral("The desired snapshot is not canonical");
      return false;
    }
    desiredDigest = hashBytes(desiredBytes);
    if (desired.readOnly)
      loadState = QStringLiteral("unsupported");

    if (lastGoodRead.status == StoreReadStatus::Missing) {
      if (activationRead.present()) {
        error = QStringLiteral(
            "Activation metadata exists without last-good state");
        return false;
      }
      applied.reset();
      appliedState.reset();
      lastGoodBytes.clear();
      if (migrateDesiredAuthority) {
        const auto migrated = store.write(StoreFile::Desired, desiredBytes);
        if (!migrated.success) {
          error = migrated.errorMessage;
          return false;
        }
      }
      return true;
    }
    if (!activationRead.present()) {
      error =
          QStringLiteral("Last-good state exists without activation metadata");
      return false;
    }
    const auto parsedLast = parseCompatibleDesiredState(
        QByteArrayView(lastGoodRead.bytes), catalog, actions
    );
    const auto parsedActivation = parseAppliedBytes(activationRead.bytes);
    if (!parsedLast || parsedLast->state.readOnly || !parsedActivation ||
        parsedActivation->revision != parsedLast->state.revision ||
        parsedActivation->snapshotDigest != hashBytes(lastGoodRead.bytes)) {
      error = QStringLiteral("Last-good and activation metadata disagree");
      return false;
    }
    PendingRecord proof{
        .candidate = parsedLast->state,
        .candidateBytes = lastGoodRead.bytes,
        .snapshotDigest = parsedActivation->snapshotDigest,
        .after = *parsedActivation,
    };
    if (!crossCheckGeneration(proof, error))
      return false;
    const auto currentApplied = currentAuthorityState(
        *parsedLast, catalog, actions
    );
    if (!currentApplied) {
      error = QStringLiteral(
          "The last-good snapshot cannot be migrated to current authority"
      );
      return false;
    }
    applied = *parsedActivation;
    appliedState = *currentApplied;
    lastGoodBytes = lastGoodRead.bytes;
    desiredSemanticallyMatchesApplied =
        applied->revision == desired.revision && *appliedState == desired;
    if (migrateDesiredAuthority) {
      const auto migrated = store.write(StoreFile::Desired, desiredBytes);
      if (!migrated.success) {
        error = migrated.errorMessage;
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] AuthorityResult stageCandidate(
      const quint64 expectedRevision,
      const QString &nonce,
      const QDateTime &createdAt,
      const PendingKind kind,
      DesiredState candidate,
      QByteArray candidateBytes
  ) {
    const auto safetyErrors = Hyprland::validateManagedActivationSafety(
        candidate, catalog
    );
    if (!safetyErrors.isEmpty()) {
      return fail(
          QStringLiteral("VerificationFailed"), describeErrors(safetyErrors)
      );
    }
    const auto candidateDigest = hashBytes(candidateBytes);
    auto requirement = activationRequirementForDelta(
        appliedState ? &*appliedState : nullptr, candidate, catalog
    );
    const auto directory = generations.directoryForNonce(nonce);
    auto rendered = renderGeneration(candidate, catalog, actions, directory,
                                     paths.userCustomPath(), nonce, createdAt);
    if (!rendered) {
      const auto deferred =
          std::ranges::any_of(rendered.errors, [](const auto &item) {
            return item.code == QStringLiteral("renderer.broker-unavailable") ||
                   item.code == QStringLiteral("renderer.uwsm-unavailable");
          });
      return fail(deferred ? QStringLiteral("ActivationRequired")
                           : QStringLiteral("VerificationFailed"),
                  describeErrors(rendered.errors));
    }
    // The renderer reports the strongest mode in the target in isolation.
    // A transaction has a live applied baseline, so unchanged restart or
    // session settings must not upgrade an otherwise reload-only delta.
    rendered.value->activationRequirement = requirement;
    const auto published = generations.publish(*rendered.value);
    if (!published.success || !published.generation) {
      return fail(QStringLiteral("VerificationFailed"), published.errorMessage);
    }
    PendingRecord transaction{
        .kind = kind,
        .phase = PendingPhase::Prepared,
        .expectedRevision = expectedRevision,
        .beforeDesiredDigest = desiredDigest,
        .candidate = std::move(candidate),
        .candidateBytes = std::move(candidateBytes),
        .snapshotDigest = candidateDigest,
        .after =
            AppliedRecord{
                .revision = published.generation->revision,
                .snapshotDigest = candidateDigest,
                .generation = published.generation->id,
                .nonce = nonce,
                .entrypoint = published.generation->entrypoint,
                .requirement = requirement,
            },
        .before = applied,
    };
    QString verificationError;
    if (!crossCheckGeneration(transaction, verificationError)) {
      return fail(QStringLiteral("VerificationFailed"), verificationError);
    }
    const auto persisted =
        store.write(StoreFile::Pending, pendingBytes(transaction));
    if (!persisted.success) {
      if (persisted.committedButNotDurable)
        failed = true;
      return fail(QStringLiteral("PersistenceFailed"), persisted.errorMessage);
    }
    pending = std::move(transaction);
    return {
        .success = true,
        .snapshot = snapshot(),
        .prepared =
            ActivationGeneration{
                .id = published.generation->id,
                .nonce = nonce,
                .snapshotDigest = published.generation->snapshotDigest,
                .revision = published.generation->revision,
                .directory = published.generation->directory,
                .entrypoint = published.generation->entrypoint,
                .manifest = published.generation->manifest,
                .requirement = requirement,
            },
    };
  }

  [[nodiscard]] AuthorityResult prepare(const quint64 expectedRevision,
                                        const QString &nonce,
                                        const QDateTime &createdAt,
                                        const bool recovery) {
    if (!initialized || failed)
      return fail(QStringLiteral("Unavailable"),
                  QStringLiteral("The compositor authority is unavailable"));
    if (desired.readOnly)
      return fail(
          QStringLiteral("ReadOnly"),
          QStringLiteral(
              "The desired snapshot is compatibility-preserved read-only"));
    if (pending)
      return fail(
          QStringLiteral("ApplyFailed"),
          QStringLiteral("A compositor transaction is already pending"));
    if (expectedRevision != desired.revision)
      return fail(QStringLiteral("StaleRevision"),
                  QStringLiteral("The desired revision changed"));
    if (recovery && !appliedState)
      return fail(
          QStringLiteral("RecoveryUnavailable"),
          QStringLiteral("No successfully applied snapshot is available"));
    if (recovery && expectedRevision == std::numeric_limits<quint64>::max())
      return fail(QStringLiteral("RevisionExhausted"),
                  QStringLiteral("The desired revision cannot advance"));

    DesiredState candidate = recovery ? *appliedState : desired;
    if (recovery)
      candidate.revision = expectedRevision + 1;
    auto candidateBytes = Hyprland::serializeDesiredState(candidate);
    return stageCandidate(
        expectedRevision, nonce, createdAt,
        recovery ? PendingKind::Recovery : PendingKind::Apply,
        std::move(candidate), std::move(candidateBytes)
    );
  }

  [[nodiscard]] AuthorityResult prepareDisplay(
      const quint64 expectedRevision,
      const Hyprland::DisplayProfile &profile,
      const Hyprland::ConnectedDisplayTopology &topology,
      const QString &nonce,
      const QDateTime &createdAt
  ) {
    if (!initialized || failed)
      return fail(QStringLiteral("Unavailable"),
                  QStringLiteral("The compositor authority is unavailable"));
    if (desired.readOnly)
      return fail(QStringLiteral("ReadOnly"),
                  QStringLiteral("The desired snapshot is read-only"));
    if (pending)
      return fail(QStringLiteral("ConfirmationPending"),
                  QStringLiteral("A compositor transaction is already pending"));
    if (expectedRevision != desired.revision)
      return fail(QStringLiteral("StaleRevision"),
                  QStringLiteral("The desired revision changed"));
    const auto exactAppliedBaseline = applied && appliedState
        && applied->revision == desired.revision
        && (applied->snapshotDigest == desiredDigest
            || desiredSemanticallyMatchesApplied)
        && *appliedState == desired;
    if (!exactAppliedBaseline) {
      return fail(
          QStringLiteral("DisplayScopeConflict"),
          QStringLiteral(
              "A display preview requires the exact current applied baseline"
          )
      );
    }
    const auto candidate = Hyprland::buildDisplayCandidate(
        desired, profile, topology, catalog, actions
    );
    if (!candidate) {
      return fail(QStringLiteral("InvalidDisplayProfile"),
                  describeErrors(candidate.errors));
    }
    return stageCandidate(
        expectedRevision, nonce, createdAt, PendingKind::DisplayPreview,
        candidate.value->state, candidate.value->bytes
    );
  }
};

ConfigurationTransaction::ConfigurationTransaction(
    StorePaths paths, Hyprland::Catalog catalog,
    Hyprland::ActionCatalog actionCatalog)
    : impl_(std::make_unique<Impl>(std::move(paths), std::move(catalog),
                                   std::move(actionCatalog))) {}

ConfigurationTransaction::~ConfigurationTransaction() = default;

AuthorityResult ConfigurationTransaction::initialize() {
  if (impl_->initialized) {
    return impl_->fail(
        QStringLiteral("Unavailable"),
        QStringLiteral("The compositor authority was already initialized"));
  }
  const auto stored = impl_->store.initialize();
  if (!stored.success) {
    impl_->failed = true;
    impl_->loadState = QStringLiteral("unavailable");
    return impl_->fail(QStringLiteral("Unavailable"), stored.errorMessage);
  }
  const auto generated = impl_->generations.initialize();
  if (!generated.success) {
    impl_->failed = true;
    impl_->generations.shutdown();
    impl_->store.shutdown();
    return impl_->fail(QStringLiteral("Unavailable"), generated.errorMessage);
  }
  impl_->initialized = true;
  impl_->loadState = QStringLiteral("normal");
  QString error;
  if (!impl_->reconcilePending(error) || !impl_->loadSnapshots(error)) {
    impl_->failed = true;
    impl_->loadState = QStringLiteral("unavailable");
    impl_->generations.shutdown();
    impl_->store.shutdown();
    impl_->initialized = false;
    return impl_->fail(QStringLiteral("Unavailable"), error);
  }
  return {.success = true, .snapshot = impl_->snapshot()};
}

FilesystemContextResult
ConfigurationTransaction::duplicateActivationFilesystemContext() const {
  if (!impl_->initialized || impl_->failed || !impl_->store.rootsStillNamed() ||
      !impl_->generations.directoryStillNamed()) {
    return {
        .success = false,
        .errorCode = QStringLiteral("PersistenceFailed"),
        .errorMessage = QStringLiteral(
            "The compositor authority filesystem roots are unavailable"),
    };
  }

  ActivationFilesystemContext context;
  context.stateRoot = impl_->paths.stateRoot;
  context.configRoot = impl_->paths.configRoot;
  context.managedConfigRoot = impl_->paths.managedConfigRoot;
  context.stableEntrypoint = impl_->paths.stableEntrypointPath();
  context.stateDirectoryFd = duplicateDescriptor(
      impl_->store.stateDirectoryFd());
  context.configDirectoryFd = duplicateDescriptor(
      impl_->store.configDirectoryFd());
  context.managedDirectoryFd = duplicateDescriptor(
      impl_->store.managedDirectoryFd());
  context.generationsDirectoryFd = duplicateDescriptor(
      impl_->generations.directoryFd());
  if (!context.complete() || !impl_->store.rootsStillNamed() ||
      !impl_->generations.directoryStillNamed()) {
    return {
        .success = false,
        .errorCode = QStringLiteral("PersistenceFailed"),
        .errorMessage = QStringLiteral(
            "The compositor authority filesystem identity changed while "
            "being shared with the activation backend"),
    };
  }

  FilesystemContextResult result{.success = true};
  result.context.emplace(std::move(context));
  return result;
}

AuthoritySnapshot ConfigurationTransaction::snapshot() const {
  return impl_->snapshot();
}

QByteArray ConfigurationTransaction::optionCatalog() const {
  return Hyprland::canonicalCatalogJson(impl_->catalog);
}

QByteArray ConfigurationTransaction::actionCatalog() const {
  return Hyprland::canonicalActionCatalogJson(impl_->actions);
}

QByteArray ConfigurationTransaction::configSchema() const {
  return impl_->actions.configSchemaDocument;
}

Hyprland::ValidationErrors
ConfigurationTransaction::currentActivationSafetyErrors() const {
  return Hyprland::validateManagedActivationSafety(
      impl_->desired, impl_->catalog
  );
}

AuthorityResult
ConfigurationTransaction::replaceSnapshot(const quint64 expectedRevision,
                                          const QByteArray &candidateBytes) {
  if (!impl_->initialized || impl_->failed)
    return impl_->fail(
        QStringLiteral("Unavailable"),
        QStringLiteral("The compositor authority is unavailable"));
  if (impl_->desired.readOnly)
    return impl_->fail(QStringLiteral("ReadOnly"),
                       QStringLiteral("The desired snapshot is read-only"));
  const auto currentToken = expectedRevision == impl_->desired.revision;
  const auto possibleRetry =
      expectedRevision != std::numeric_limits<quint64>::max() &&
      expectedRevision + 1 == impl_->desired.revision;
  if (!currentToken && !possibleRetry) {
    return impl_->fail(QStringLiteral("StaleRevision"),
                       QStringLiteral("The desired revision changed"));
  }
  const auto parsed = Hyprland::parseDesiredState(
      QByteArrayView(candidateBytes), impl_->catalog, impl_->actions);
  if (!parsed || parsed.value->readOnly ||
      parsed.value->revision != expectedRevision) {
    return impl_->fail(
        QStringLiteral("InvalidSnapshot"),
        parsed ? QStringLiteral(
                     "The candidate must embed the current expected revision")
               : describeErrors(parsed.errors));
  }
  auto canonical = Hyprland::serializeDesiredState(*parsed.value);
  if (possibleRetry) {
    auto retried = *parsed.value;
    retried.revision = impl_->desired.revision;
    if (Hyprland::serializeDesiredState(retried) == impl_->desiredBytes) {
      return {.success = true, .snapshot = impl_->snapshot()};
    }
    return impl_->fail(QStringLiteral("StaleRevision"),
                       QStringLiteral("The desired revision changed"));
  }
  if (impl_->pending)
    return impl_->fail(
        QStringLiteral("ConfirmationPending"),
        QStringLiteral("A compositor transaction is already pending")
    );
  if (canonical == impl_->desiredBytes) {
    return {.success = true, .snapshot = impl_->snapshot()};
  }
  if (expectedRevision == std::numeric_limits<quint64>::max()) {
    return impl_->fail(QStringLiteral("RevisionExhausted"),
                       QStringLiteral("The desired revision cannot advance"));
  }
  auto next = *parsed.value;
  next.revision = expectedRevision + 1;
  canonical = Hyprland::serializeDesiredState(next);
  const auto revalidated = Hyprland::parseDesiredState(
      QByteArrayView(canonical), impl_->catalog, impl_->actions);
  if (!revalidated || *revalidated.value != next) {
    return impl_->fail(
        QStringLiteral("InvalidSnapshot"),
        QStringLiteral(
            "The incremented candidate failed canonical validation"));
  }
  const auto safetyErrors = Hyprland::validateManagedActivationSafety(
      next, impl_->catalog
  );
  if (!safetyErrors.isEmpty()) {
    return impl_->fail(
        QStringLiteral("InvalidSnapshot"), describeErrors(safetyErrors)
    );
  }
  const auto persisted = impl_->store.write(StoreFile::Desired, canonical);
  if (!persisted.success) {
    if (persisted.committedButNotDurable)
      impl_->failed = true;
    return impl_->fail(QStringLiteral("PersistenceFailed"),
                       persisted.errorMessage);
  }
  impl_->desired = std::move(next);
  impl_->desiredBytes = std::move(canonical);
  impl_->desiredDigest = hashBytes(impl_->desiredBytes);
  impl_->desiredSemanticallyMatchesApplied = false;
  impl_->loadState = QStringLiteral("normal");
  return {.success = true, .snapshot = impl_->snapshot()};
}

AuthorityResult
ConfigurationTransaction::prepareApply(const quint64 expectedRevision,
                                       const QString &nonce,
                                       const QDateTime &createdAt) {
  return impl_->prepare(expectedRevision, nonce, createdAt, false);
}

AuthorityResult
ConfigurationTransaction::prepareRecovery(const quint64 expectedRevision,
                                          const QString &nonce,
                                          const QDateTime &createdAt) {
  return impl_->prepare(expectedRevision, nonce, createdAt, true);
}

AuthorityResult ConfigurationTransaction::prepareDisplayApply(
    const quint64 expectedRevision,
    const Hyprland::DisplayProfile &profile,
    const Hyprland::ConnectedDisplayTopology &topology,
    const QString &nonce,
    const QDateTime &createdAt
) {
  return impl_->prepareDisplay(
      expectedRevision, profile, topology, nonce, createdAt
  );
}

AuthorityResult
ConfigurationTransaction::commitApply(const QString &generation) {
  if (!impl_->initialized || impl_->failed || !impl_->pending)
    return impl_->fail(
        QStringLiteral("ApplyFailed"),
        QStringLiteral("No prepared compositor transaction exists"));
  if (impl_->pending->after.generation != generation)
    return impl_->fail(
        QStringLiteral("VerificationFailed"),
        QStringLiteral(
            "The generation does not match the pending transaction"));
  if (impl_->pending->phase != PendingPhase::Prepared) {
    return impl_->fail(QStringLiteral("ApplyFailed"),
                       QStringLiteral("The compositor transaction already has "
                                      "a durable commit decision"));
  }
  QString verificationError;
  if (!impl_->crossCheckGeneration(*impl_->pending, verificationError))
    return impl_->fail(QStringLiteral("VerificationFailed"), verificationError);
  auto committing = *impl_->pending;
  committing.phase = PendingPhase::Committing;
  auto persisted =
      impl_->store.write(StoreFile::Pending, pendingBytes(committing));
  if (!persisted.success) {
    auto result = impl_->fail(QStringLiteral("PersistenceFailed"),
                              persisted.errorMessage);
    if (persisted.committedButNotDurable) {
      impl_->failed = true;
      result = impl_->fail(QStringLiteral("PersistenceFailed"),
                           persisted.errorMessage);
      result.commitDecisionMayExist = true;
    }
    return result;
  }
  impl_->pending = std::move(committing);
  const auto durableFailure = [this](QString code, QString message) {
    auto result = impl_->fail(std::move(code), std::move(message));
    result.commitDecisionDurable = true;
    result.commitDecisionMayExist = true;
    return result;
  };
  // commitApply is invoked only after the executor positively proved this
  // generation. Persist that irreversible success decision before checking
  // or changing any mirrors; a damaged mirror must leave a one-way recovery
  // journal, never turn live activation into an abortable prepared state.
  if (!impl_->persistentStateMatchesBefore(*impl_->pending,
                                           verificationError)) {
    impl_->failed = true;
    return durableFailure(QStringLiteral("PersistenceFailed"),
                          verificationError);
  }
  if (impl_->pending->kind == PendingKind::Recovery ||
      impl_->pending->kind == PendingKind::DisplayPreview) {
    persisted =
        impl_->store.write(StoreFile::Desired, impl_->pending->candidateBytes);
    if (!persisted.success) {
      impl_->failed = true;
      return durableFailure(QStringLiteral("PersistenceFailed"),
                            persisted.errorMessage);
    }
  }
  persisted =
      impl_->store.write(StoreFile::LastGood, impl_->pending->candidateBytes);
  if (!persisted.success) {
    impl_->failed = true;
    return durableFailure(QStringLiteral("PersistenceFailed"),
                          persisted.errorMessage);
  }
  persisted = impl_->store.write(StoreFile::Activation,
                                 appliedBytes(impl_->pending->after));
  if (!persisted.success) {
    impl_->failed = true;
    return durableFailure(QStringLiteral("PersistenceFailed"),
                          persisted.errorMessage);
  }
  const auto publishesDesired =
      impl_->pending->kind == PendingKind::Recovery ||
      impl_->pending->kind == PendingKind::DisplayPreview;
  const auto recovery = impl_->pending->kind == PendingKind::Recovery;
  const auto committed = *impl_->pending;
  persisted = impl_->store.remove(StoreFile::Pending);
  if (!persisted.success) {
    impl_->failed = true;
    return durableFailure(QStringLiteral("PersistenceFailed"),
                          persisted.errorMessage);
  }
  impl_->pending.reset();
  if (publishesDesired) {
    impl_->desired = committed.candidate;
    impl_->desiredBytes = committed.candidateBytes;
    impl_->desiredDigest = committed.snapshotDigest;
    impl_->loadState = recovery ? QStringLiteral("recovered")
                                : QStringLiteral("normal");
  } else {
    impl_->loadState = QStringLiteral("normal");
  }
  impl_->applied = committed.after;
  impl_->appliedState = committed.candidate;
  impl_->lastGoodBytes = committed.candidateBytes;
  impl_->desiredSemanticallyMatchesApplied = false;
  return {
      .success = true,
      .commitDecisionDurable = true,
      .commitDecisionMayExist = true,
      .snapshot = impl_->snapshot(),
  };
}

AuthorityResult
ConfigurationTransaction::abortApply(const QString &generation) {
  if (!impl_->initialized || impl_->failed || !impl_->pending)
    return impl_->fail(
        QStringLiteral("ApplyFailed"),
        QStringLiteral("No prepared compositor transaction exists"));
  if (impl_->pending->after.generation != generation)
    return impl_->fail(
        QStringLiteral("VerificationFailed"),
        QStringLiteral(
            "The generation does not match the pending transaction"));
  if (impl_->pending->phase != PendingPhase::Prepared) {
    impl_->failed = true;
    return impl_->fail(
        QStringLiteral("ApplyFailed"),
        QStringLiteral("A confirmed commit decision cannot be aborted"));
  }
  QString verificationError;
  if (!impl_->persistentStateMatchesBefore(*impl_->pending,
                                           verificationError)) {
    impl_->failed = true;
    return impl_->fail(QStringLiteral("ApplyFailed"), verificationError);
  }
  const auto removed = impl_->store.remove(StoreFile::Pending);
  if (!removed.success) {
    impl_->failed = true;
    return impl_->fail(QStringLiteral("PersistenceFailed"),
                       removed.errorMessage);
  }
  impl_->pending.reset();
  return {.success = true, .snapshot = impl_->snapshot()};
}

GenerationResult
ConfigurationTransaction::verifyGeneration(const QString &nonce) const {
  return impl_->generations.verify(nonce);
}

} // namespace HyprShelld::Compositor
