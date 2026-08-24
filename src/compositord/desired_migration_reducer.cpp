#include "desired_migration_reducer.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

#include <limits>
#include <utility>

namespace HyprShelld::Compositor {
namespace {

constexpr char migrationManifestRawSha256[] =
    "9cedd126fcd41f450d6625aacdab84bf6b97d8576e87172678e34634ad4cab35";
constexpr qsizetype migrationManifestExactBytes = 37215;
constexpr char sourceManifestV2RawSha256[] =
    "09965e7626da69910c1d16e856baba3859cf06d9f8a14896a9b8a6e06cfe4619";
constexpr qsizetype sourceManifestV2ExactBytes = 73262;
constexpr qsizetype maximumSourceManifestV2Bytes = 1024 * 1024;
constexpr int maximumSourceManifestV2Depth = 32;

constexpr const char *predecessorCatalogDigests[] = {
    "0232f9b036849e2b9423d5960dd32f22001c79b5b6b6696330f481d1d0c657e0",
    "10d6c8bfd757407e93ebb379afb7f29df89dacd2a75936ab3d2cbe873d539388",
    Hyprland::reviewedCatalogDigest,
};
constexpr const char *predecessorActionCatalogDigests[] = {
    "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2",
    Hyprland::reviewedActionCatalogDigest,
};

struct SourceAuthorities final {
  Hyprland::Catalog catalog;
  Hyprland::ActionCatalog actions;
};

struct DestinationAuthorities final {
  Hyprland::Catalog catalog;
  Hyprland::ActionCatalog actions;
};

enum class ExactDesiredV1Status {
  InvalidSource,
  UnqualifiedAuthorities,
  Exact,
};

struct ExactDesiredV1SemanticInspection final {
  ExactDesiredV1Status status = ExactDesiredV1Status::InvalidSource;
  QByteArray sourceBytes;
  QString sourceBytesSha256;
  quint32 sourcePatch = 0;
  std::optional<Hyprland::DesiredState> normalizedState;
};

[[nodiscard]] QString sha256(const QByteArrayView bytes) {
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] bool isCanonicalSha256(const QString &value) {
  static const QRegularExpression expression(QStringLiteral("^[0-9a-f]{64}$"));
  return expression.match(value).hasMatch();
}

template <std::size_t Size>
[[nodiscard]] bool containsDigest(const char *const (&digests)[Size],
                                  const QString &candidate) {
  for (const auto &digest : digests) {
    if (candidate == QLatin1String(digest)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::optional<SourceAuthorities>
reparseSourceAuthorities(const Hyprland::Catalog &catalog,
                         const Hyprland::ActionCatalog &actions) {
  const auto catalogBytes = Hyprland::canonicalCatalogJson(catalog);
  const auto parsedCatalog = Hyprland::parseCatalog(catalogBytes);
  if (!parsedCatalog) {
    return std::nullopt;
  }

  const auto actionBytes = Hyprland::canonicalActionCatalogJson(actions);
  const auto parsedActions =
      Hyprland::parseActionCatalog(actionBytes, actions.configSchemaDocument);
  if (!parsedActions) {
    return std::nullopt;
  }
  return SourceAuthorities{
      .catalog = *parsedCatalog.value,
      .actions = *parsedActions.value,
  };
}

[[nodiscard]] std::optional<DestinationAuthorities>
reparseDestinationAuthorities(const Hyprland::Catalog &catalog,
                              const Hyprland::ActionCatalog &actions,
                              const QByteArrayView sourceManifestBytes) {
  if (sourceManifestBytes.size() != sourceManifestV2ExactBytes ||
      sha256(sourceManifestBytes) != QLatin1String(sourceManifestV2RawSha256)) {
    return std::nullopt;
  }
  const auto manifest = Hyprland::JsonSupport::parseStrictObject(
      sourceManifestBytes, maximumSourceManifestV2Bytes,
      maximumSourceManifestV2Depth);
  if (!manifest) {
    return std::nullopt;
  }
  const auto canonicalSourceDigest =
      sha256(Hyprland::JsonSupport::canonicalJson(*manifest.value));
  if (canonicalSourceDigest !=
      QLatin1String(Hyprland::dormantReviewedSourceManifestDigest)) {
    return std::nullopt;
  }

  const auto catalogBytes = Hyprland::canonicalCatalogJson(catalog);
  const auto parsedCatalog = Hyprland::parseDormantCatalogV2(catalogBytes);
  if (!parsedCatalog) {
    return std::nullopt;
  }
  const auto actionBytes = Hyprland::canonicalActionCatalogJson(actions);
  const auto parsedActions = Hyprland::parseDormantActionCatalogV2(
      actionBytes, actions.configSchemaDocument);
  if (!parsedActions) {
    return std::nullopt;
  }
  if (parsedCatalog.value->sourceManifestDigest != canonicalSourceDigest ||
      parsedActions.value->sourceManifestDigest != canonicalSourceDigest ||
      parsedCatalog.value->sourceManifestDigest !=
          parsedActions.value->sourceManifestDigest) {
    return std::nullopt;
  }
  return DestinationAuthorities{
      .catalog = *parsedCatalog.value,
      .actions = *parsedActions.value,
  };
}

[[nodiscard]] std::optional<quint32>
exactMigrationPatch(const QString &target) {
  static const QRegularExpression expression(
      QStringLiteral("^0\\.56\\.(0|[1-9][0-9]{0,9})$"));
  const auto match = expression.match(target);
  if (!match.hasMatch()) {
    return std::nullopt;
  }
  bool ok = false;
  const auto patch = match.captured(1).toUInt(&ok, 10);
  return ok ? std::optional<quint32>{patch} : std::nullopt;
}

[[nodiscard]] bool
containsPreSharedProtectedSelector(const Hyprland::DesiredState &state) {
  for (const auto &rule : state.workspaceRules) {
    if (rule.id == QLatin1String(Hyprland::sharedSpacingWorkspaceRuleId) ||
        rule.selector ==
            QLatin1String(Hyprland::sharedSpacingWorkspaceRuleSelector)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] DesiredMigrationRoute routeForPatch(const quint32 patch) {
  switch (patch) {
  case 0:
    return DesiredMigrationRoute::M01ThenM12;
  case 1:
    return DesiredMigrationRoute::M12;
  case 2:
    return DesiredMigrationRoute::M22;
  default:
    return DesiredMigrationRoute::None;
  }
}

[[nodiscard]] ExactDesiredV1SemanticInspection
inspectExactDesiredV1Semantic(const QByteArrayView sourceBytes,
                              const Hyprland::Catalog &catalogV1,
                              const Hyprland::ActionCatalog &actionCatalogV1) {
  ExactDesiredV1SemanticInspection result;
  if (sourceBytes.size() < 0 ||
      sourceBytes.size() > Hyprland::maximumDesiredStateBytes ||
      (sourceBytes.size() > 0 && sourceBytes.data() == nullptr)) {
    return result;
  }

  result.sourceBytes = QByteArray(sourceBytes.data(), sourceBytes.size());
  result.sourceBytesSha256 = sha256(result.sourceBytes);

  const auto sourceAuthorities =
      reparseSourceAuthorities(catalogV1, actionCatalogV1);
  if (!sourceAuthorities) {
    result.status = ExactDesiredV1Status::UnqualifiedAuthorities;
    return result;
  }

  const auto raw = Hyprland::JsonSupport::parseStrictObject(
      result.sourceBytes, Hyprland::maximumDesiredStateBytes, 64);
  if (!raw) {
    return result;
  }
  auto normalizedRoot = *raw.value;
  const auto catalogValue =
      normalizedRoot.value(QStringLiteral("catalogDigest"));
  const auto actionValue =
      normalizedRoot.value(QStringLiteral("actionCatalogDigest"));
  if (!catalogValue.isString() || !actionValue.isString() ||
      !isCanonicalSha256(catalogValue.toString()) ||
      !isCanonicalSha256(actionValue.toString())) {
    return result;
  }
  const auto predecessorCatalogDigest = catalogValue.toString();
  const auto predecessorActionDigest = actionValue.toString();

  normalizedRoot.insert(QStringLiteral("catalogDigest"),
                        QLatin1String(Hyprland::reviewedCatalogDigest));
  normalizedRoot.insert(QStringLiteral("actionCatalogDigest"),
                        QLatin1String(Hyprland::reviewedActionCatalogDigest));
  auto normalizedBytes = Hyprland::JsonSupport::canonicalJson(normalizedRoot);
  normalizedBytes.append('\n');
  const auto parsed = Hyprland::parseDesiredState(
      normalizedBytes, sourceAuthorities->catalog, sourceAuthorities->actions);
  if (!parsed) {
    return result;
  }

  auto restoredState = *parsed.value;
  restoredState.catalogDigest = predecessorCatalogDigest;
  restoredState.actionCatalogDigest = predecessorActionDigest;
  if (Hyprland::serializeDesiredState(restoredState) != result.sourceBytes) {
    return result;
  }
  const auto patch = exactMigrationPatch(restoredState.targetHyprland);
  if (!patch) {
    return result;
  }
  result.sourcePatch = *patch;

  if (!containsDigest(predecessorCatalogDigests, predecessorCatalogDigest) ||
      !containsDigest(predecessorActionCatalogDigests,
                      predecessorActionDigest)) {
    return result;
  }
  if (predecessorActionDigest ==
          QLatin1String(predecessorActionCatalogDigests[0]) &&
      containsPreSharedProtectedSelector(restoredState)) {
    return result;
  }

  result.normalizedState = *parsed.value;
  result.status = ExactDesiredV1Status::Exact;
  return result;
}

} // namespace

DesiredMigrationDisposition DesiredMigrationPlan::disposition() const {
  return disposition_;
}

DesiredMigrationRoute DesiredMigrationPlan::route() const { return route_; }

quint32 DesiredMigrationPlan::sourcePatch() const { return sourcePatch_; }

const QByteArray &DesiredMigrationPlan::sourceBytes() const {
  return sourceBytes_;
}

const QString &DesiredMigrationPlan::sourceBytesSha256() const {
  return sourceBytesSha256_;
}

std::optional<ExactDesiredV1Observation> inspectExactDesiredV1Observation(
    const QByteArrayView sourceBytes, const Hyprland::Catalog &catalogV1,
    const Hyprland::ActionCatalog &actionCatalogV1) {
  const auto inspected =
      inspectExactDesiredV1Semantic(sourceBytes, catalogV1, actionCatalogV1);
  if (inspected.status != ExactDesiredV1Status::Exact ||
      !inspected.normalizedState) {
    return std::nullopt;
  }
  return ExactDesiredV1Observation{
      .revision = inspected.normalizedState->revision,
      .sourcePatch = inspected.sourcePatch,
  };
}

DesiredMigrationPlan
inspectDesiredV1ToV2Migration(const QByteArrayView sourceBytes,
                              const QByteArrayView migrationManifestBytes,
                              const QByteArrayView sourceManifestV2Bytes,
                              const Hyprland::Catalog &catalogV1,
                              const Hyprland::ActionCatalog &actionCatalogV1,
                              const Hyprland::Catalog &catalogV2,
                              const Hyprland::ActionCatalog &actionCatalogV2) {
  DesiredMigrationPlan plan;
  const auto source =
      inspectExactDesiredV1Semantic(sourceBytes, catalogV1, actionCatalogV1);
  plan.sourceBytes_ = source.sourceBytes;
  plan.sourceBytesSha256_ = source.sourceBytesSha256;
  plan.sourcePatch_ = source.sourcePatch;
  if (source.status == ExactDesiredV1Status::UnqualifiedAuthorities) {
    plan.disposition_ = DesiredMigrationDisposition::UnqualifiedEvidence;
    return plan;
  }
  if (source.status != ExactDesiredV1Status::Exact || !source.normalizedState) {
    return plan;
  }

  if (source.sourcePatch >= 3) {
    plan.disposition_ = DesiredMigrationDisposition::UnsupportedNewerPatch;
    return plan;
  }

  if (migrationManifestBytes.size() != migrationManifestExactBytes ||
      sha256(migrationManifestBytes) !=
          QLatin1String(migrationManifestRawSha256)) {
    plan.disposition_ = DesiredMigrationDisposition::UnqualifiedEvidence;
    return plan;
  }
  const auto destinationAuthorities = reparseDestinationAuthorities(
      catalogV2, actionCatalogV2, sourceManifestV2Bytes);
  if (!destinationAuthorities) {
    plan.disposition_ = DesiredMigrationDisposition::UnqualifiedEvidence;
    return plan;
  }

  plan.route_ = routeForPatch(source.sourcePatch);
  plan.normalizedState_ = *source.normalizedState;
  plan.disposition_ = DesiredMigrationDisposition::Eligible;
  return plan;
}

std::optional<DesiredMigrationMaterialization>
materializeDesiredV1ToV2Migration(
    const DesiredMigrationPlan &plan, QString authorityId,
    const QByteArrayView migrationManifestBytes,
    const QByteArrayView sourceManifestV2Bytes,
    const Hyprland::Catalog &catalogV1,
    const Hyprland::ActionCatalog &actionCatalogV1,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2) {
  if (plan.disposition_ != DesiredMigrationDisposition::Eligible ||
      !plan.normalizedState_ ||
      !Hyprland::isCanonicalAuthorityId(authorityId)) {
    return std::nullopt;
  }

  const auto checked = inspectDesiredV1ToV2Migration(
      plan.sourceBytes_, migrationManifestBytes, sourceManifestV2Bytes,
      catalogV1, actionCatalogV1, catalogV2, actionCatalogV2);
  if (checked.disposition_ != DesiredMigrationDisposition::Eligible ||
      !checked.normalizedState_ || checked.route_ != plan.route_ ||
      checked.sourcePatch_ != plan.sourcePatch_ ||
      checked.sourceBytes_ != plan.sourceBytes_ ||
      checked.sourceBytesSha256_ != plan.sourceBytesSha256_) {
    return std::nullopt;
  }

  auto finalSemantic = *checked.normalizedState_;
  finalSemantic.targetHyprland = QStringLiteral("0.56.2");
  finalSemantic.catalogDigest =
      QLatin1String(Hyprland::dormantReviewedCatalogV2Digest);
  finalSemantic.actionCatalogDigest =
      QLatin1String(Hyprland::dormantReviewedActionCatalogV2Digest);
  finalSemantic.compatibility = Hyprland::CompatibilityDecision::Exact;
  finalSemantic.readOnly = false;
  finalSemantic.opaqueFutureDocument.reset();
  Hyprland::DesiredStateV2 finalState{
      .authorityId = std::move(authorityId),
      .semanticState = std::move(finalSemantic),
  };
  const auto encoded = Hyprland::serializeDormantDesiredStateV2(finalState);
  if (!encoded) {
    return std::nullopt;
  }

  const auto destinationAuthorities = reparseDestinationAuthorities(
      catalogV2, actionCatalogV2, sourceManifestV2Bytes);
  if (!destinationAuthorities) {
    return std::nullopt;
  }
  const auto reparsed = Hyprland::parseDormantDesiredStateV2(
      *encoded.value, destinationAuthorities->catalog,
      destinationAuthorities->actions);
  if (!reparsed || *reparsed.value != finalState) {
    return std::nullopt;
  }
  if (encoded.value->size() < 2 || !encoded.value->endsWith('\n') ||
      encoded.value->at(encoded.value->size() - 2) == '\n') {
    return std::nullopt;
  }
  const auto destinationCanonical =
      QByteArrayView(*encoded.value).first(encoded.value->size() - 1);

  return DesiredMigrationMaterialization{
      .route = checked.route_,
      .state = std::move(finalState),
      .bytes = *encoded.value,
      .sourceV1StoredBytesSha256 = checked.sourceBytesSha256_,
      .destinationV2StoredBytesSha256 = sha256(*encoded.value),
      .destinationV2SnapshotDigest = sha256(destinationCanonical),
  };
}

} // namespace HyprShelld::Compositor
