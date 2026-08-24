#include "compositord/settled_v2_generation_byte_graph.h"

#include "compositord/authority_records.h"
#include "compositord/ordinary_pending_record.h"
#include "compositord/renderer.h"

#include "hyprland/desired_state.h"
#include "hyprland/json_support.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

const QString authorityA = QStringLiteral("11111111111111111111111111111111");
const QString authorityB = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

[[nodiscard]] QByteArray readBytes(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

[[nodiscard]] QJsonObject readObject(const QString &path) {
  const auto document = QJsonDocument::fromJson(readBytes(path));
  return document.isObject() ? document.object() : QJsonObject{};
}

[[nodiscard]] QByteArray canonicalObject(const QJsonObject &object) {
  auto bytes = JsonSupport::canonicalJson(object);
  bytes.append('\n');
  return bytes;
}

[[nodiscard]] QString nonce(const QChar digit) { return QString(32, digit); }

[[nodiscard]] QDateTime fixedTime() {
  return QDateTime::fromString(QStringLiteral("2026-08-09T12:34:56.789Z"),
                               Qt::ISODateWithMs);
}

} // namespace

class CompositorSettledV2GenerationByteGraphTest final : public QObject {
  Q_OBJECT

private:
  enum class Content {
    Base,
    Alternate,
    Restart,
    Session,
  };

  enum class MirrorStage {
    Before,
    DesiredAfter,
    LastGoodAfter,
    ActivationAfter,
  };

  struct Product final {
    DesiredStateV2 state;
    QByteArray desiredBytes;
    AppliedRecordV2 applied;
    QByteArray appliedBytes;
    SettledV2GenerationEvidence evidence;
    ActivationRequirement isolatedRequirement = ActivationRequirement::Reload;
  };

  struct OrdinaryFixture final {
    OrdinaryPendingRecordV2 record;
    QByteArray pendingBytes;
    std::optional<Product> before;
    Product after;
  };

  Catalog catalogV2_;
  ActionCatalog actionCatalogV2_;
  QJsonObject template_;
  QByteArray authorityBytes_;

  [[nodiscard]] DesiredStateV2
  state(const quint64 revision, const Content content = Content::Base,
        const QString &authorityId = authorityA) const {
    auto object = template_;
    object.insert(QStringLiteral("authorityId"), authorityId);
    object.insert(QStringLiteral("revision"), QString::number(revision));
    if (content == Content::Alternate) {
      auto overrides = object.value(QStringLiteral("overrides")).toObject();
      overrides.insert(QStringLiteral("hyprland.animations.enabled"), false);
      object.insert(QStringLiteral("overrides"), overrides);
    } else if (content == Content::Restart) {
      auto overrides = object.value(QStringLiteral("overrides")).toObject();
      overrides.insert(QStringLiteral("hyprland.debug.full_cm_proto"), true);
      object.insert(QStringLiteral("overrides"), overrides);
    } else if (content == Content::Session) {
      object.insert(
          QStringLiteral("environment"),
          QJsonArray{QJsonObject{
              {QStringLiteral("id"), QStringLiteral("session-env")},
              {QStringLiteral("name"), QStringLiteral("HYPRSHELLD_SESSION")},
              {QStringLiteral("value"), QStringLiteral("qualified")},
              {QStringLiteral("scope"), QStringLiteral("hyprland")},
          }});
    }
    const auto parsed = parseDormantDesiredStateV2(
        canonicalObject(object), catalogV2_, actionCatalogV2_);
    if (!parsed) {
      qFatal("failed to build exact Desired v2 test state");
    }
    return *parsed.value;
  }

  [[nodiscard]] QByteArray desiredBytes(const DesiredStateV2 &value) const {
    const auto serialized = serializeDormantDesiredStateV2(value);
    if (!serialized) {
      qFatal("failed to serialize exact Desired v2 test state");
    }
    return *serialized.value;
  }

  [[nodiscard]] QByteArray appliedBytes(const AppliedRecordV2 &value) const {
    const auto serialized = serializeAppliedRecordV2(value);
    if (!serialized) {
      qFatal("failed to serialize exact Applied v2 test record");
    }
    return *serialized.value;
  }

  [[nodiscard]] Product
  product(const DesiredStateV2 &value, const QString &activationNonce,
          const std::optional<ActivationRequirement> historicalRequirement =
              std::nullopt) const {
    Product result;
    result.state = value;
    result.desiredBytes = desiredBytes(value);
    result.evidence.activationNonce = activationNonce;
    result.evidence.desiredBytes = result.desiredBytes;
    result.evidence.generationRoot =
        QStringLiteral("/tmp/hyprshelld-v2-byte-graph/") + activationNonce;
    result.evidence.userCustomPath =
        QStringLiteral("/tmp/hyprshelld-v2-byte-graph-user.lua");
    const auto rendered = renderDormantGenerationV2(
        value, catalogV2_, actionCatalogV2_, result.evidence.generationRoot,
        result.evidence.userCustomPath, activationNonce, fixedTime());
    if (!rendered) {
      qFatal("failed to render exact dormant v2 test generation");
    }
    result.evidence.manifestBytes = rendered.value->manifestBytes;
    for (auto iterator = rendered.value->files.constBegin();
         iterator != rendered.value->files.constEnd(); ++iterator) {
      result.evidence.files.insert(iterator.key(), iterator->contents);
    }
    result.isolatedRequirement = rendered.value->activationRequirement;
    result.applied = {
        .authorityId = value.authorityId,
        .revision = value.semanticState.revision,
        .snapshotDigest = rendered.value->snapshotDigest,
        .generation = rendered.value->generation,
        .activationNonce = activationNonce,
        .entrypoint = rendered.value->entrypoint,
        .requiredActivation = historicalRequirement.value_or(
            rendered.value->activationRequirement),
    };
    result.appliedBytes = appliedBytes(result.applied);
    return result;
  }

  [[nodiscard]] QByteArray
  pendingBytes(const OrdinaryPendingRecordV2 &record) const {
    const auto serialized =
        serializeOrdinaryPendingRecordV2(record, catalogV2_, actionCatalogV2_);
    if (!serialized) {
      qFatal("failed to serialize exact ordinary Pending v2 test record");
    }
    return *serialized.value;
  }

  [[nodiscard]] OrdinaryFixture ordinary(const OrdinaryPendingKind kind,
                                         const OrdinaryPendingPhase phase,
                                         std::optional<Product> before,
                                         Product after) const {
    const auto expectedRevision = kind == OrdinaryPendingKind::Apply
                                      ? after.state.semanticState.revision
                                      : after.state.semanticState.revision - 1;
    OrdinaryFixture result{
        .record = {},
        .pendingBytes = {},
        .before = std::move(before),
        .after = std::move(after),
    };
    result.record = {
        .authorityId = authorityA,
        .kind = kind,
        .phase = phase,
        .expectedRevision = expectedRevision,
        .beforeDesiredDigest = kind == OrdinaryPendingKind::Apply
                                   ? result.after.applied.snapshotDigest
                                   : result.before->applied.snapshotDigest,
        .beforeActivationDesired =
            result.before
                ? std::optional<
                      OrdinaryPendingDesiredMaterialV2>{OrdinaryPendingDesiredMaterialV2{
                      .state = result.before->state,
                      .bytes = result.before->desiredBytes,
                  }}
                : std::nullopt,
        .candidateSnapshot = result.after.state,
        .candidateSnapshotBytes = result.after.desiredBytes,
        .snapshotDigest = result.after.applied.snapshotDigest,
        .afterActivation = result.after.applied,
        .beforeActivation =
            result.before
                ? std::optional<AppliedRecordV2>{result.before->applied}
                : std::nullopt,
    };
    result.pendingBytes = pendingBytes(result.record);
    return result;
  }

  [[nodiscard]] OrdinaryFixture
  ordinaryFixture(const OrdinaryPendingKind kind,
                  const OrdinaryPendingPhase phase) const {
    if (kind == OrdinaryPendingKind::Apply) {
      return ordinary(
          kind, phase, product(state(6), nonce(QLatin1Char('2'))),
          product(state(7, Content::Alternate), nonce(QLatin1Char('3'))));
    }
    if (kind == OrdinaryPendingKind::Recovery) {
      return ordinary(kind, phase, product(state(7), nonce(QLatin1Char('4'))),
                      product(state(8), nonce(QLatin1Char('5'))));
    }
    return ordinary(
        kind, phase, product(state(7), nonce(QLatin1Char('6'))),
        product(state(8, Content::Alternate), nonce(QLatin1Char('7'))));
  }

  [[nodiscard]] QVector<SettledV2GenerationEvidence>
  evidenceFor(const OrdinaryFixture &fixture) const {
    QVector<SettledV2GenerationEvidence> result{fixture.after.evidence};
    if (fixture.before) {
      result.append(fixture.before->evidence);
    }
    return result;
  }

  [[nodiscard]] SettledV2CurrentRecordBytes
  currentFor(const OrdinaryFixture &fixture, const MirrorStage stage) const {
    const auto desiredAfter = stage != MirrorStage::Before;
    const Product *desiredProduct = &fixture.after;
    if (fixture.record.kind != OrdinaryPendingKind::Apply && !desiredAfter) {
      desiredProduct = &*fixture.before;
    }

    std::optional<QByteArrayView> lastGood;
    if (stage == MirrorStage::LastGoodAfter ||
        stage == MirrorStage::ActivationAfter) {
      lastGood = QByteArrayView(fixture.after.desiredBytes);
    } else if (fixture.before) {
      lastGood = QByteArrayView(fixture.before->desiredBytes);
    }

    std::optional<QByteArrayView> applied;
    if (stage == MirrorStage::ActivationAfter) {
      applied = QByteArrayView(fixture.after.appliedBytes);
    } else if (fixture.before) {
      applied = QByteArrayView(fixture.before->appliedBytes);
    }

    return {
        .authority = authorityBytes_,
        .desired = desiredProduct->desiredBytes,
        .lastGood = lastGood,
        .applied = applied,
    };
  }

  [[nodiscard]] SettledV2CurrentRecordBytes noPendingCurrent(
      const Product &desired,
      const std::optional<std::reference_wrapper<const Product>> retained =
          std::nullopt) const {
    return {
        .authority = authorityBytes_,
        .desired = desired.desiredBytes,
        .lastGood =
            retained
                ? std::optional<QByteArrayView>{retained->get().desiredBytes}
                : std::nullopt,
        .applied =
            retained
                ? std::optional<QByteArrayView>{retained->get().appliedBytes}
                : std::nullopt,
    };
  }

  [[nodiscard]] SettledV2GenerationByteGraphResult
  classify(const SettledV2CurrentRecordBytes &current,
           const std::optional<QByteArrayView> pending,
           const QVector<SettledV2GenerationEvidence> &evidence) const {
    return classifySettledV2GenerationContentByteGraph(
        current, pending, evidence, catalogV2_, actionCatalogV2_);
  }

  [[nodiscard]] SettledV2GenerationByteGraphResult classifyNoPending(
      const Product &desired,
      const std::optional<std::reference_wrapper<const Product>> retained,
      const QVector<SettledV2GenerationEvidence> &evidence) const {
    return classify(noPendingCurrent(desired, retained), std::nullopt,
                    evidence);
  }

  static constexpr auto coherent =
      SettledV2GenerationByteGraphResult::GenerationContentByteCoherent;
  static constexpr auto delegate =
      SettledV2GenerationByteGraphResult::DelegatePendingOwner;
  static constexpr auto incoherent =
      SettledV2GenerationByteGraphResult::Incoherent;

private slots:
  void initTestCase();
  void coherentNoPendingZeroAndOneReference();
  void ordinaryKindsPhasesAndLegalProgressionRows();
  void nullPriorApplyAndEmbeddedDisplayPriorRemainExact();
  void sameRevisionApplyRequiresExactDesiredIdentity();
  void revisionEndpointsRemainCoherentWhereLegal();
  void nonordinaryPendingDelegatesWithinBound();
  void illegalRecordGraphsAreIncoherent();
  void evidenceCardinalityKeysAndAliasesFailClosed();
  void desiredEvidenceMustBeExactAndBoundToApplied();
  void retainedDocumentsMustRemainByteIdentical();
  void verifierRejectsRepresentativeProductInputMutations();
  void historicalActivationDeltaIsNotAnIsolatedRendererClaim();
  void borrowedViewMetadataFailsClosed();
};

void CompositorSettledV2GenerationByteGraphTest::initTestCase() {
  const auto catalog = parseDormantCatalogV2(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE)));
  QVERIFY(catalog);
  catalogV2_ = *catalog.value;

  const auto actions = parseDormantActionCatalogV2(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE)),
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE)));
  QVERIFY(actions);
  actionCatalogV2_ = *actions.value;

  template_ = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_TEMPLATE_FILE));
  QVERIFY(!template_.isEmpty());
  const auto authority = serializeAuthorityRecordV2({authorityA});
  QVERIFY(authority);
  authorityBytes_ = *authority.value;
  QVERIFY(fixedTime().isValid());
}

void CompositorSettledV2GenerationByteGraphTest::
    coherentNoPendingZeroAndOneReference() {
  const auto empty = product(state(9), nonce(QLatin1Char('2')));
  QCOMPARE(classifyNoPending(empty, std::nullopt, {}), coherent);

  const auto retained = product(state(7), nonce(QLatin1Char('3')));
  const auto current = product(state(9), nonce(QLatin1Char('4')));
  QCOMPARE(classifyNoPending(current, std::cref(retained), {retained.evidence}),
           coherent);
  QCOMPARE(retained.applied.entrypoint, QStringLiteral("hyprland.lua"));
}

void CompositorSettledV2GenerationByteGraphTest::
    ordinaryKindsPhasesAndLegalProgressionRows() {
  const std::array kinds{
      OrdinaryPendingKind::Apply,
      OrdinaryPendingKind::Recovery,
      OrdinaryPendingKind::DisplayPreview,
  };
  for (const auto kind : kinds) {
    const auto prepared = ordinaryFixture(kind, OrdinaryPendingPhase::Prepared);
    QCOMPARE(prepared.record.beforeActivationDesired.has_value(),
             prepared.record.beforeActivation.has_value());
    QVERIFY(prepared.record.beforeActivationDesired);
    QCOMPARE(prepared.record.beforeActivationDesired->state,
             prepared.before->state);
    QCOMPARE(prepared.record.beforeActivationDesired->bytes,
             prepared.before->desiredBytes);
    QCOMPARE(classify(currentFor(prepared, MirrorStage::Before),
                      QByteArrayView(prepared.pendingBytes),
                      evidenceFor(prepared)),
             coherent);

    const auto committing =
        ordinaryFixture(kind, OrdinaryPendingPhase::Committing);
    const std::array stages{
        MirrorStage::Before,
        MirrorStage::DesiredAfter,
        MirrorStage::LastGoodAfter,
        MirrorStage::ActivationAfter,
    };
    for (const auto stage : stages) {
      QCOMPARE(classify(currentFor(committing, stage),
                        QByteArrayView(committing.pendingBytes),
                        evidenceFor(committing)),
               coherent);
    }
  }
}

void CompositorSettledV2GenerationByteGraphTest::
    nullPriorApplyAndEmbeddedDisplayPriorRemainExact() {
  const auto after =
      product(state(7, Content::Alternate), nonce(QLatin1Char('8')));
  const auto prepared =
      ordinary(OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared,
               std::nullopt, after);
  QVERIFY(!prepared.record.beforeActivationDesired);
  QCOMPARE(classify(currentFor(prepared, MirrorStage::Before),
                    QByteArrayView(prepared.pendingBytes),
                    evidenceFor(prepared)),
           coherent);

  const auto committing =
      ordinary(OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Committing,
               std::nullopt, after);
  for (const auto stage : {MirrorStage::Before, MirrorStage::LastGoodAfter,
                           MirrorStage::ActivationAfter}) {
    QCOMPARE(classify(currentFor(committing, stage),
                      QByteArrayView(committing.pendingBytes),
                      evidenceFor(committing)),
             coherent);
  }

  const auto display = ordinaryFixture(OrdinaryPendingKind::DisplayPreview,
                                       OrdinaryPendingPhase::Committing);
  QVERIFY(display.record.beforeActivationDesired);
  QCOMPARE(display.record.beforeActivationDesired->state,
           display.before->state);
  QCOMPARE(display.record.beforeActivationDesired->bytes,
           display.before->desiredBytes);
  const auto current = currentFor(display, MirrorStage::ActivationAfter);
  QCOMPARE(current.desired, QByteArrayView(display.after.desiredBytes));
  QCOMPARE(*current.lastGood, QByteArrayView(display.after.desiredBytes));
  QCOMPARE(*current.applied, QByteArrayView(display.after.appliedBytes));
  QCOMPARE(classify(current, QByteArrayView(display.pendingBytes),
                    evidenceFor(display)),
           coherent);

  auto priorCorrupt = evidenceFor(display);
  priorCorrupt[1].files[QStringLiteral("hyprland.lua")].append("--prior\n");
  QCOMPARE(
      classify(current, QByteArrayView(display.pendingBytes), priorCorrupt),
      incoherent);
  auto afterCorrupt = evidenceFor(display);
  afterCorrupt[0].files[QStringLiteral("hyprland.lua")].append("--after\n");
  QCOMPARE(
      classify(current, QByteArrayView(display.pendingBytes), afterCorrupt),
      incoherent);
}

void CompositorSettledV2GenerationByteGraphTest::
    sameRevisionApplyRequiresExactDesiredIdentity() {
  const auto same =
      ordinary(OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared,
               product(state(7), nonce(QLatin1Char('2'))),
               product(state(7), nonce(QLatin1Char('3'))));
  auto evidence = evidenceFor(same);
  QCOMPARE(classify(currentFor(same, MirrorStage::Before),
                    QByteArrayView(same.pendingBytes), evidence),
           coherent);
  std::reverse(evidence.begin(), evidence.end());
  QCOMPARE(classify(currentFor(same, MirrorStage::Before),
                    QByteArrayView(same.pendingBytes), evidence),
           coherent);

  auto conflicting = same.record;
  const auto conflictingPrior =
      product(state(7, Content::Alternate), nonce(QLatin1Char('4')));
  conflicting.beforeActivation = conflictingPrior.applied;
  conflicting.beforeActivationDesired = OrdinaryPendingDesiredMaterialV2{
      .state = conflictingPrior.state,
      .bytes = conflictingPrior.desiredBytes,
  };
  const auto rejected = serializeOrdinaryPendingRecordV2(
      conflicting, catalogV2_, actionCatalogV2_);
  QVERIFY(!rejected);
}

void CompositorSettledV2GenerationByteGraphTest::
    revisionEndpointsRemainCoherentWhereLegal() {
  for (const auto revision :
       {quint64{0}, std::numeric_limits<quint64>::max()}) {
    const auto retained = product(state(revision), nonce(QLatin1Char('9')));
    QCOMPARE(
        classifyNoPending(retained, std::cref(retained), {retained.evidence}),
        coherent);
  }

  const auto maxApplyProduct =
      product(state(std::numeric_limits<quint64>::max(), Content::Alternate),
              nonce(QLatin1Char('a')));
  const auto maxApply =
      ordinary(OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared,
               std::nullopt, maxApplyProduct);
  QCOMPARE(classify(currentFor(maxApply, MirrorStage::Before),
                    QByteArrayView(maxApply.pendingBytes),
                    evidenceFor(maxApply)),
           coherent);

  for (const auto kind :
       {OrdinaryPendingKind::Recovery, OrdinaryPendingKind::DisplayPreview}) {
    const auto contentAfter = kind == OrdinaryPendingKind::Recovery
                                  ? Content::Base
                                  : Content::Alternate;
    const auto edge =
        ordinary(kind, OrdinaryPendingPhase::Prepared,
                 product(state(0), nonce(QLatin1Char('b'))),
                 product(state(1, contentAfter), nonce(QLatin1Char('c'))));
    QCOMPARE(classify(currentFor(edge, MirrorStage::Before),
                      QByteArrayView(edge.pendingBytes), evidenceFor(edge)),
             coherent);
  }
}

void CompositorSettledV2GenerationByteGraphTest::
    nonordinaryPendingDelegatesWithinBound() {
  const auto currentProduct = product(state(7), nonce(QLatin1Char('2')));
  const auto current = noPendingCurrent(currentProduct);
  const auto ordinaryPending = ordinaryFixture(OrdinaryPendingKind::Apply,
                                               OrdinaryPendingPhase::Prepared)
                                   .pendingBytes;
  auto noncanonical = ordinaryPending;
  noncanonical.prepend(' ');
  const QByteArray restartShaped =
      QByteArrayLiteral("{\"formatVersion\":2,\"kind\":\"Restart\"}\n");
  const QByteArray unknown =
      QByteArrayLiteral("{\"formatVersion\":2,\"kind\":\"Unknown\"}\n");
  const QByteArray malformed = QByteArrayLiteral("{");
  const QByteArray oversized(maximumOrdinaryPendingRecordV2Bytes + 1, 'x');
  for (const auto &bytes :
       {restartShaped, unknown, malformed, noncanonical, oversized}) {
    QCOMPARE(classify(current, QByteArrayView(bytes), {}), delegate);
  }

  auto sameNonce = ordinaryFixture(OrdinaryPendingKind::Apply,
                                   OrdinaryPendingPhase::Prepared);
  const auto afterNonce = sameNonce.record.afterActivation.activationNonce;
  const auto beforeNonce = sameNonce.record.beforeActivation->activationNonce;
  QCOMPARE(sameNonce.pendingBytes.count(afterNonce.toLatin1()), 1);
  sameNonce.pendingBytes.replace(afterNonce.toLatin1(), beforeNonce.toLatin1());
  QCOMPARE(classify(currentFor(sameNonce, MirrorStage::Before),
                    QByteArrayView(sameNonce.pendingBytes),
                    evidenceFor(sameNonce)),
           delegate);
}

void CompositorSettledV2GenerationByteGraphTest::
    illegalRecordGraphsAreIncoherent() {
  const auto retained = product(state(7), nonce(QLatin1Char('2')));
  const auto desired = product(state(9), nonce(QLatin1Char('3')));
  auto partial = noPendingCurrent(desired);
  partial.lastGood = QByteArrayView(retained.desiredBytes);
  QCOMPARE(classify(partial, std::nullopt, {retained.evidence}), incoherent);

  auto invalidCurrent = noPendingCurrent(desired);
  const QByteArray malformedAuthority = QByteArrayLiteral("{");
  invalidCurrent.authority = malformedAuthority;
  QCOMPARE(classify(invalidCurrent, std::nullopt, {}), incoherent);

  auto prepared = ordinaryFixture(OrdinaryPendingKind::DisplayPreview,
                                  OrdinaryPendingPhase::Prepared);
  QCOMPARE(classify(currentFor(prepared, MirrorStage::DesiredAfter),
                    QByteArrayView(prepared.pendingBytes),
                    evidenceFor(prepared)),
           incoherent);

  auto committing = ordinaryFixture(OrdinaryPendingKind::DisplayPreview,
                                    OrdinaryPendingPhase::Committing);
  auto illegal = currentFor(committing, MirrorStage::Before);
  illegal.lastGood = QByteArrayView(committing.after.desiredBytes);
  QCOMPARE(classify(illegal, QByteArrayView(committing.pendingBytes),
                    evidenceFor(committing)),
           incoherent);
}

void CompositorSettledV2GenerationByteGraphTest::
    evidenceCardinalityKeysAndAliasesFailClosed() {
  const auto fixture = ordinaryFixture(OrdinaryPendingKind::DisplayPreview,
                                       OrdinaryPendingPhase::Prepared);
  const auto current = currentFor(fixture, MirrorStage::Before);
  const auto pending = QByteArrayView(fixture.pendingBytes);
  const auto exactEvidence = evidenceFor(fixture);

  auto missing = exactEvidence;
  missing.removeLast();
  QCOMPARE(classify(current, pending, missing), incoherent);

  auto duplicate = exactEvidence;
  duplicate[1].activationNonce = duplicate[0].activationNonce;
  QCOMPARE(classify(current, pending, duplicate), incoherent);

  auto wrong = exactEvidence;
  wrong[0].activationNonce = nonce(QLatin1Char('f'));
  QCOMPARE(classify(current, pending, wrong), incoherent);

  auto nullPrior =
      ordinary(OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared,
               std::nullopt, product(state(7), nonce(QLatin1Char('8'))));
  auto extra = evidenceFor(nullPrior);
  extra.append(fixture.before->evidence);
  QCOMPARE(classify(currentFor(nullPrior, MirrorStage::Before),
                    QByteArrayView(nullPrior.pendingBytes), extra),
           incoherent);

  auto tooMany = exactEvidence;
  tooMany.append(fixture.after.evidence);
  QCOMPARE(
      classify(current,
               QByteArrayView(QByteArrayLiteral("{\"kind\":\"Restart\"}\n")),
               tooMany),
      incoherent);

  auto generationAlias = fixture;
  generationAlias.record.afterActivation.generation =
      generationAlias.record.beforeActivation->generation;
  generationAlias.pendingBytes = pendingBytes(generationAlias.record);
  QCOMPARE(classify(currentFor(generationAlias, MirrorStage::Before),
                    QByteArrayView(generationAlias.pendingBytes),
                    evidenceFor(generationAlias)),
           incoherent);

  auto nonceAliasCurrent = current;
  auto nonceAliasApplied = fixture.before->applied;
  nonceAliasApplied.activationNonce = fixture.after.applied.activationNonce;
  const auto nonceAliasBytes = appliedBytes(nonceAliasApplied);
  nonceAliasCurrent.applied = QByteArrayView(nonceAliasBytes);
  QCOMPARE(classify(nonceAliasCurrent, pending, exactEvidence), incoherent);

  auto unequalIdentityCurrent = current;
  auto unequalIdentityApplied = fixture.before->applied;
  unequalIdentityApplied.requiredActivation = ActivationRequirement::Session;
  QVERIFY(unequalIdentityApplied.requiredActivation !=
          fixture.before->applied.requiredActivation);
  const auto unequalIdentityBytes = appliedBytes(unequalIdentityApplied);
  unequalIdentityCurrent.applied = QByteArrayView(unequalIdentityBytes);
  QCOMPARE(classify(unequalIdentityCurrent, pending, exactEvidence),
           incoherent);
}

void CompositorSettledV2GenerationByteGraphTest::
    desiredEvidenceMustBeExactAndBoundToApplied() {
  const auto retained = product(state(7), nonce(QLatin1Char('2')));
  const auto current = noPendingCurrent(retained, std::cref(retained));
  const QVector exact{retained.evidence};

  auto malformed = exact;
  malformed[0].desiredBytes = QByteArrayLiteral("{");
  QCOMPARE(classify(current, std::nullopt, malformed), incoherent);

  auto noncanonical = exact;
  noncanonical[0].desiredBytes.chop(1);
  QCOMPARE(classify(current, std::nullopt, noncanonical), incoherent);

  auto oversized = exact;
  oversized[0].desiredBytes = QByteArray(maximumDesiredStateBytes + 1, 'x');
  QCOMPARE(classify(current, std::nullopt, oversized), incoherent);

  auto authorityMismatch = exact;
  authorityMismatch[0].desiredBytes =
      desiredBytes(state(7, Content::Base, authorityB));
  QCOMPARE(classify(current, std::nullopt, authorityMismatch), incoherent);

  auto revisionMismatch = exact;
  revisionMismatch[0].desiredBytes = desiredBytes(state(8));
  QCOMPARE(classify(current, std::nullopt, revisionMismatch), incoherent);

  auto digestMismatch = exact;
  digestMismatch[0].desiredBytes = desiredBytes(state(7, Content::Alternate));
  QCOMPARE(classify(current, std::nullopt, digestMismatch), incoherent);
}

void CompositorSettledV2GenerationByteGraphTest::
    retainedDocumentsMustRemainByteIdentical() {
  const auto fixture = ordinaryFixture(OrdinaryPendingKind::DisplayPreview,
                                       OrdinaryPendingPhase::Committing);
  auto current = currentFor(fixture, MirrorStage::ActivationAfter);
  const auto conflicting = desiredBytes(
      state(fixture.after.state.semanticState.revision, Content::Restart));
  current.lastGood = QByteArrayView(conflicting);
  QCOMPARE(classify(current, QByteArrayView(fixture.pendingBytes),
                    evidenceFor(fixture)),
           incoherent);

  auto evidence = evidenceFor(fixture);
  evidence[0].desiredBytes = conflicting;
  QCOMPARE(classify(currentFor(fixture, MirrorStage::ActivationAfter),
                    QByteArrayView(fixture.pendingBytes), evidence),
           incoherent);
}

void CompositorSettledV2GenerationByteGraphTest::
    verifierRejectsRepresentativeProductInputMutations() {
  const auto retained = product(state(7), nonce(QLatin1Char('2')));
  const auto current = noPendingCurrent(retained, std::cref(retained));
  const QVector exact{retained.evidence};

  auto manifest = exact;
  manifest[0].manifestBytes.append(' ');
  QCOMPARE(classify(current, std::nullopt, manifest), incoherent);

  auto oversizedManifest = exact;
  oversizedManifest[0].manifestBytes = QByteArray(4 * 1024 * 1024 + 1, 'x');
  QCOMPARE(classify(current, std::nullopt, oversizedManifest), incoherent);

  auto file = exact;
  file[0].files[QStringLiteral("hyprland.lua")].append("--mutation\n");
  QCOMPARE(classify(current, std::nullopt, file), incoherent);

  auto oversizedFile = exact;
  oversizedFile[0].files[QStringLiteral("hyprland.lua")] =
      QByteArray(16 * 1024 * 1024 + 1, 'x');
  QCOMPARE(classify(current, std::nullopt, oversizedFile), incoherent);

  auto generationPath = exact;
  generationPath[0].generationRoot =
      QStringLiteral("/tmp/hyprshelld-v2-byte-graph/wrong-nonce");
  QCOMPARE(classify(current, std::nullopt, generationPath), incoherent);

  auto customPath = exact;
  customPath[0].userCustomPath = QStringLiteral("/tmp/other-user.lua");
  QCOMPARE(classify(current, std::nullopt, customPath), incoherent);

  auto oversizedGenerationPath = exact;
  oversizedGenerationPath[0].generationRoot = QString(4097, QLatin1Char('x'));
  QCOMPARE(classify(current, std::nullopt, oversizedGenerationPath),
           incoherent);

  auto oversizedNonceKey = exact;
  oversizedNonceKey[0].activationNonce = QString(4097, QLatin1Char('x'));
  QCOMPARE(classify(current, std::nullopt, oversizedNonceKey), incoherent);

  auto oversizedCustomPath = exact;
  oversizedCustomPath[0].userCustomPath = QString(4097, QLatin1Char('x'));
  QCOMPARE(classify(current, std::nullopt, oversizedCustomPath), incoherent);

  auto oversizedFileKey = exact;
  oversizedFileKey[0].files.remove(QStringLiteral("hyprland.lua"));
  oversizedFileKey[0].files.insert(QString(256, QLatin1Char('x')),
                                   QByteArrayLiteral("bounded"));
  QCOMPARE(classify(current, std::nullopt, oversizedFileKey), incoherent);

  auto createdAt = exact;
  QCOMPARE(createdAt[0].manifestBytes.count("2026-08-09T12:34:56.789Z"), 1);
  createdAt[0].manifestBytes.replace("2026-08-09T12:34:56.789Z",
                                     "2026-08-09T12:34:57.789Z");
  QCOMPARE(classify(current, std::nullopt, createdAt), incoherent);

  auto recordGeneration = retained.applied;
  recordGeneration.generation = QString(64, QLatin1Char('f'));
  auto generationCurrent = current;
  const auto generationAppliedBytes = appliedBytes(recordGeneration);
  generationCurrent.applied = QByteArrayView(generationAppliedBytes);
  QCOMPARE(classify(generationCurrent, std::nullopt, exact), incoherent);
}

void CompositorSettledV2GenerationByteGraphTest::
    historicalActivationDeltaIsNotAnIsolatedRendererClaim() {
  for (const auto requirement :
       {ActivationRequirement::Reload, ActivationRequirement::Restart,
        ActivationRequirement::Session}) {
    const auto retained =
        product(state(7), nonce(QLatin1Char('2')), requirement);
    QCOMPARE(
        classifyNoPending(retained, std::cref(retained), {retained.evidence}),
        coherent);
  }

  const auto restart =
      product(state(8, Content::Restart), nonce(QLatin1Char('3')),
              ActivationRequirement::Reload);
  QCOMPARE(restart.isolatedRequirement, ActivationRequirement::Restart);
  QCOMPARE(restart.applied.requiredActivation, ActivationRequirement::Reload);
  QCOMPARE(classifyNoPending(restart, std::cref(restart), {restart.evidence}),
           coherent);

  const auto session =
      product(state(9, Content::Session), nonce(QLatin1Char('4')),
              ActivationRequirement::Reload);
  QCOMPARE(session.isolatedRequirement, ActivationRequirement::Session);
  QCOMPARE(session.applied.requiredActivation, ActivationRequirement::Reload);
  QCOMPARE(classifyNoPending(session, std::cref(session), {session.evidence}),
           coherent);
}

void CompositorSettledV2GenerationByteGraphTest::
    borrowedViewMetadataFailsClosed() {
  const auto retained = product(state(7), nonce(QLatin1Char('2')));
  const auto current = noPendingCurrent(retained, std::cref(retained));
  const QVector exact{retained.evidence};

  auto negativeCurrent = current;
  negativeCurrent.authority =
      QByteArrayView(static_cast<const char *>(nullptr), -1);
  QCOMPARE(classify(negativeCurrent, std::nullopt, exact), incoherent);

  auto nullPositiveCurrent = current;
  nullPositiveCurrent.desired =
      QByteArrayView(static_cast<const char *>(nullptr), 1);
  QCOMPARE(classify(nullPositiveCurrent, std::nullopt, exact), incoherent);

  auto negativeLastGood = current;
  negativeLastGood.lastGood =
      QByteArrayView(static_cast<const char *>(nullptr), -1);
  QCOMPARE(classify(negativeLastGood, std::nullopt, exact), incoherent);

  auto nullPositiveLastGood = current;
  nullPositiveLastGood.lastGood =
      QByteArrayView(static_cast<const char *>(nullptr), 1);
  QCOMPARE(classify(nullPositiveLastGood, std::nullopt, exact), incoherent);

  auto negativeApplied = current;
  negativeApplied.applied =
      QByteArrayView(static_cast<const char *>(nullptr), -1);
  QCOMPARE(classify(negativeApplied, std::nullopt, exact), incoherent);

  auto nullPositiveApplied = current;
  nullPositiveApplied.applied =
      QByteArrayView(static_cast<const char *>(nullptr), 1);
  QCOMPARE(classify(nullPositiveApplied, std::nullopt, exact), incoherent);

  QCOMPARE(classify(current,
                    QByteArrayView(static_cast<const char *>(nullptr), -1),
                    exact),
           delegate);
  QCOMPARE(classify(current,
                    QByteArrayView(static_cast<const char *>(nullptr), 1),
                    exact),
           delegate);

  auto aliased = retained.desiredBytes;
  const auto exactLength = QByteArrayView(aliased.constData(), aliased.size());
  auto aliasedCurrent = current;
  aliasedCurrent.desired = exactLength;
  aliasedCurrent.lastGood = exactLength;
  QCOMPARE(classify(aliasedCurrent, std::nullopt, exact), coherent);
}

QTEST_GUILESS_MAIN(CompositorSettledV2GenerationByteGraphTest)

#include "compositor_settled_v2_generation_byte_graph_test.moc"
