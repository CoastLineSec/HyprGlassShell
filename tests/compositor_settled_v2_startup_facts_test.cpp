#include "compositord/settled_v2_startup_facts.h"

#include "compositord/authority_records.h"
#include "compositord/ordinary_pending_record.h"

#include "hyprland/desired_state.h"

#include <QCryptographicHash>
#include <QFile>
#include <QtTest>

#include <array>
#include <limits>
#include <optional>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

const QString authorityA = QStringLiteral("11111111111111111111111111111111");
const QString authorityB = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
const QString beforeNonce = QStringLiteral("22222222222222222222222222222222");
const QString afterNonce = QStringLiteral("33333333333333333333333333333333");
const QString beforeGeneration(64, QLatin1Char('b'));
const QString afterGeneration(64, QLatin1Char('c'));

[[nodiscard]] QByteArray readBytes(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

[[nodiscard]] QString digestWithoutFinalLf(const QByteArrayView bytes) {
  if (bytes.isEmpty() || bytes.back() != '\n') {
    return {};
  }
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes.first(bytes.size() - 1),
                               QCryptographicHash::Sha256)
          .toHex());
}

[[nodiscard]] QString digestIncludingFinalLf(const QByteArrayView bytes) {
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] bool allNeither(const OrdinaryPendingStartupFacts &facts) {
  return facts.desired == MirrorRelation::Neither &&
         facts.lastGood == MirrorRelation::Neither &&
         facts.activation == MirrorRelation::Neither;
}

} // namespace

class CompositorSettledV2StartupFactsTest final : public QObject {
  Q_OBJECT

private:
  Catalog catalogV2_;
  ActionCatalog actionCatalogV2_;

  struct PendingFixture final {
    OrdinaryPendingRecordV2 record;
    QByteArray bytes;
    DesiredStateV2 beforeState;
    QByteArray beforeBytes;
    DesiredStateV2 afterState;
    QByteArray afterBytes;
  };

  [[nodiscard]] DesiredStateV2
  state(quint64 revision, int contentVariant = 0,
        const QString &authorityId = authorityA) const {
    const auto initial =
        defaultDormantDesiredStateV2(catalogV2_, actionCatalogV2_, authorityId);
    Q_ASSERT(initial);
    auto value = *initial.value;
    value.semanticState.revision = revision;
    if (contentVariant != 0) {
      value.semanticState.overrides.insert(
          QStringLiteral("hyprland.input.sensitivity"),
          static_cast<double>(contentVariant) / 10.0);
    }
    return value;
  }

  [[nodiscard]] QByteArray stateBytes(const DesiredStateV2 &value) const {
    const auto encoded = serializeDormantDesiredStateV2(value);
    Q_ASSERT(encoded);
    return *encoded.value;
  }

  [[nodiscard]] QByteArray
  authorityBytes(const QString &authorityId = authorityA) const {
    const auto encoded = serializeAuthorityRecordV2({authorityId});
    Q_ASSERT(encoded);
    return *encoded.value;
  }

  [[nodiscard]] AppliedRecordV2
  applied(const quint64 revision, const QString &snapshotDigest,
          const bool before, const QString &authorityId = authorityA) const {
    return {
        .authorityId = authorityId,
        .revision = revision,
        .snapshotDigest = snapshotDigest,
        .generation = before ? beforeGeneration : afterGeneration,
        .activationNonce = before ? beforeNonce : afterNonce,
        .entrypoint = QStringLiteral("hyprland.lua"),
        .requiredActivation = ActivationRequirement::Reload,
    };
  }

  [[nodiscard]] QByteArray appliedBytes(const AppliedRecordV2 &value) const {
    const auto encoded = serializeAppliedRecordV2(value);
    Q_ASSERT(encoded);
    return *encoded.value;
  }

  [[nodiscard]] PendingFixture
  pendingFixture(const OrdinaryPendingKind kind,
                 const OrdinaryPendingPhase phase,
                 const quint64 expectedRevision = 7,
                 const bool applyBeforePresent = true) const {
    const auto advancing = kind != OrdinaryPendingKind::Apply;
    auto afterState =
        state(advancing ? expectedRevision + 1 : expectedRevision);
    auto afterBytes = stateBytes(afterState);
    const auto afterDigest = digestWithoutFinalLf(afterBytes);

    auto beforeState = afterState;
    beforeState.semanticState.revision = expectedRevision;
    auto beforeBytes = stateBytes(beforeState);
    const auto beforeDigest = digestWithoutFinalLf(beforeBytes);

    OrdinaryPendingRecordV2 record{
        .authorityId = authorityA,
        .kind = kind,
        .phase = phase,
        .expectedRevision = expectedRevision,
        .beforeDesiredDigest =
            kind == OrdinaryPendingKind::Apply ? afterDigest : beforeDigest,
        .beforeActivationDesired = std::nullopt,
        .candidateSnapshot = afterState,
        .candidateSnapshotBytes = afterBytes,
        .snapshotDigest = afterDigest,
        .afterActivation =
            applied(afterState.semanticState.revision, afterDigest, false),
        .beforeActivation = std::nullopt,
    };
    if (kind != OrdinaryPendingKind::Apply || applyBeforePresent) {
      record.beforeActivation = applied(expectedRevision, beforeDigest, true);
      record.beforeActivationDesired = OrdinaryPendingDesiredMaterialV2{
          .state = beforeState,
          .bytes = beforeBytes,
      };
    }

    const auto encoded =
        serializeOrdinaryPendingRecordV2(record, catalogV2_, actionCatalogV2_);
    Q_ASSERT(encoded);
    return {
        .record = record,
        .bytes = *encoded.value,
        .beforeState = beforeState,
        .beforeBytes = beforeBytes,
        .afterState = afterState,
        .afterBytes = afterBytes,
    };
  }

  [[nodiscard]] NoPendingCoherenceFacts noPending(
      const QByteArrayView authority, const QByteArrayView desired,
      const std::optional<QByteArrayView> lastGood = std::nullopt,
      const std::optional<QByteArrayView> activation = std::nullopt) const {
    return buildNoPendingCoherenceFactsV2(
        {
            .authority = authority,
            .desired = desired,
            .lastGood = lastGood,
            .applied = activation,
        },
        catalogV2_, actionCatalogV2_);
  }

  [[nodiscard]] OrdinaryPendingStartupFacts
  ordinary(const QByteArrayView authority, const QByteArrayView desired,
           const std::optional<QByteArrayView> lastGood,
           const std::optional<QByteArrayView> activation,
           const std::optional<QByteArrayView> pending) const {
    return buildOrdinaryPendingStartupFactsV2(
        {
            .authority = authority,
            .desired = desired,
            .lastGood = lastGood,
            .applied = activation,
        },
        pending, catalogV2_, actionCatalogV2_);
  }

private slots:
  void initTestCase();
  void noPendingAbsencePairAndRevisionRulesAreExact();
  void noPendingPresentInvalidNeverAliasesMissing();
  void ordinaryAllKindsAndPhasesComposeWithReducer();
  void mirrorRelationsCoverNeitherBeforeAfterAndBoth();
  void absentPriorSemanticsAreExact();
  void invalidPendingAndCurrentEvidenceNormalizeWithoutAuthorization();
  void sameRevisionUniquenessRejectsConflictingClaims();
  void snapshotDomainExcludesExactlyOneProvenLf();
  void revisionEndpointsAndOversizedViewsFailClosed();
  void malformedViewMetadataFailsBeforeParsing();
};

void CompositorSettledV2StartupFactsTest::initTestCase() {
  const auto catalog = parseDormantCatalogV2(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE)));
  QVERIFY(catalog);
  catalogV2_ = *catalog.value;

  const auto actions = parseDormantActionCatalogV2(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE)),
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE)));
  QVERIFY(actions);
  actionCatalogV2_ = *actions.value;
}

void CompositorSettledV2StartupFactsTest::
    noPendingAbsencePairAndRevisionRulesAreExact() {
  const auto authority = authorityBytes();
  const auto desiredState = state(7);
  const auto desired = stateBytes(desiredState);

  auto facts = noPending(authority, desired);
  QVERIFY(facts.currentAuthorityCoherent);
  QVERIFY(!facts.lastGoodPresent);
  QVERIFY(!facts.appliedPresent);
  QVERIFY(facts.sameRevisionRuleSatisfied);
  QCOMPARE(reduceSettledV2Startup({
               .prerequisites =
                   {
                       true,
                       true,
                       true,
                       true,
                       true,
                   },
               .pending = facts,
           }),
           SettledV2StartupDecision::Ready);

  const auto lastGoodState = state(5);
  const auto lastGood = stateBytes(lastGoodState);
  const auto activation =
      appliedBytes(applied(5, digestWithoutFinalLf(lastGood), true));
  facts = noPending(authority, desired, QByteArrayView(lastGood),
                    QByteArrayView(activation));
  QVERIFY(facts.presentPairCoherent);
  QVERIFY(facts.appliedRevisionAtMostDesired);
  QVERIFY(facts.sameRevisionRuleSatisfied);

  const auto equalDesiredState = state(5);
  const auto equalDesired = stateBytes(equalDesiredState);
  facts = noPending(authority, equalDesired, QByteArrayView(lastGood),
                    QByteArrayView(activation));
  QVERIFY(facts.sameRevisionRuleSatisfied);

  const auto changedDesired = stateBytes(state(5, 1));
  facts = noPending(authority, changedDesired, QByteArrayView(lastGood),
                    QByteArrayView(activation));
  QVERIFY(facts.presentPairCoherent);
  QVERIFY(facts.appliedRevisionAtMostDesired);
  QVERIFY(!facts.sameRevisionRuleSatisfied);
  QCOMPARE(reduceSettledV2Startup({
               .prerequisites = {true, true, true, true, true},
               .pending = facts,
           }),
           SettledV2StartupDecision::RepairOnly);

  const auto regressedDesired = stateBytes(state(4));
  facts = noPending(authority, regressedDesired, QByteArrayView(lastGood),
                    QByteArrayView(activation));
  QVERIFY(!facts.appliedRevisionAtMostDesired);
  QVERIFY(facts.sameRevisionRuleSatisfied);
}

void CompositorSettledV2StartupFactsTest::
    noPendingPresentInvalidNeverAliasesMissing() {
  const auto authority = authorityBytes();
  const auto desired = stateBytes(state(7));
  const QByteArray empty;

  auto facts =
      noPending(authority, desired, QByteArrayView(empty), std::nullopt);
  QVERIFY(facts.lastGoodPresent);
  QVERIFY(!facts.appliedPresent);
  QVERIFY(!facts.presentPairCoherent);
  QVERIFY(!facts.sameRevisionRuleSatisfied);

  facts = noPending(authority, desired, QByteArrayView(empty),
                    QByteArrayView(empty));
  QVERIFY(facts.lastGoodPresent);
  QVERIFY(facts.appliedPresent);
  QVERIFY(!facts.presentPairCoherent);
  QVERIFY(!facts.sameRevisionRuleSatisfied);

  auto malformedAuthority = authority.chopped(1);
  facts = noPending(malformedAuthority, desired);
  QVERIFY(!facts.currentAuthorityCoherent);
  QVERIFY(facts.sameRevisionRuleSatisfied);

  auto noncanonicalDesired = desired;
  noncanonicalDesired.append('\n');
  facts = noPending(authority, noncanonicalDesired);
  QVERIFY(!facts.currentAuthorityCoherent);

  const auto otherAuthority = authorityBytes(authorityB);
  facts = noPending(otherAuthority, desired);
  QVERIFY(!facts.currentAuthorityCoherent);
}

void CompositorSettledV2StartupFactsTest::
    ordinaryAllKindsAndPhasesComposeWithReducer() {
  const auto authority = authorityBytes();
  const std::array kinds{
      OrdinaryPendingKind::Apply,
      OrdinaryPendingKind::Recovery,
      OrdinaryPendingKind::DisplayPreview,
  };
  const std::array phases{
      OrdinaryPendingPhase::Prepared,
      OrdinaryPendingPhase::Committing,
  };
  for (const auto kind : kinds) {
    for (const auto phase : phases) {
      const auto fixture = pendingFixture(kind, phase);
      QCOMPARE(fixture.record.beforeActivationDesired.has_value(),
               fixture.record.beforeActivation.has_value());
      if (fixture.record.beforeActivationDesired) {
        QCOMPARE(fixture.record.beforeActivationDesired->state,
                 fixture.beforeState);
        QCOMPARE(fixture.record.beforeActivationDesired->bytes,
                 fixture.beforeBytes);
      }
      const auto beforeActivation =
          fixture.record.beforeActivation
              ? std::optional<QByteArray>{appliedBytes(
                    *fixture.record.beforeActivation)}
              : std::nullopt;
      const auto facts = ordinary(
          authority, fixture.beforeBytes,
          fixture.record.beforeActivation
              ? std::optional<QByteArrayView>(fixture.beforeBytes)
              : std::nullopt,
          beforeActivation ? std::optional<QByteArrayView>(*beforeActivation)
                           : std::nullopt,
          QByteArrayView(fixture.bytes));
      QVERIFY(facts.currentAuthorityCoherent);
      QVERIFY(facts.recordCoherent);
      QCOMPARE(facts.phase, phase);
      QVERIFY(facts.desired == MirrorRelation::Before ||
              facts.desired == MirrorRelation::Both);
      QVERIFY(facts.lastGood == MirrorRelation::Before ||
              facts.lastGood == MirrorRelation::Both);
      QCOMPARE(facts.activation, MirrorRelation::Before);

      const auto decision = reduceSettledV2Startup({
          .prerequisites = {true, true, true, true, true},
          .pending = facts,
      });
      QCOMPARE(decision, phase == OrdinaryPendingPhase::Prepared
                             ? SettledV2StartupDecision::RemovePrepared
                             : SettledV2StartupDecision::RollForwardCommitting);
    }
  }
}

void CompositorSettledV2StartupFactsTest::
    mirrorRelationsCoverNeitherBeforeAfterAndBoth() {
  const auto authority = authorityBytes();
  const auto applyFixture = pendingFixture(OrdinaryPendingKind::Apply,
                                           OrdinaryPendingPhase::Prepared);
  const auto beforeActivation =
      appliedBytes(*applyFixture.record.beforeActivation);
  auto facts = ordinary(authority, applyFixture.beforeBytes,
                        QByteArrayView(applyFixture.beforeBytes),
                        QByteArrayView(beforeActivation),
                        QByteArrayView(applyFixture.bytes));
  QCOMPARE(facts.desired, MirrorRelation::Both);
  QCOMPARE(facts.lastGood, MirrorRelation::Both);
  QCOMPARE(facts.activation, MirrorRelation::Before);

  const auto advancing = pendingFixture(OrdinaryPendingKind::DisplayPreview,
                                        OrdinaryPendingPhase::Committing);
  const auto advancingBefore = appliedBytes(*advancing.record.beforeActivation);
  const auto advancingAfter = appliedBytes(advancing.record.afterActivation);
  facts = ordinary(
      authority, advancing.beforeBytes, QByteArrayView(advancing.beforeBytes),
      QByteArrayView(advancingBefore), QByteArrayView(advancing.bytes));
  QCOMPARE(facts.desired, MirrorRelation::Before);
  QCOMPARE(facts.lastGood, MirrorRelation::Before);
  QCOMPARE(facts.activation, MirrorRelation::Before);

  facts = ordinary(
      authority, advancing.afterBytes, QByteArrayView(advancing.afterBytes),
      QByteArrayView(advancingAfter), QByteArrayView(advancing.bytes));
  QCOMPARE(facts.desired, MirrorRelation::After);
  QCOMPARE(facts.lastGood, MirrorRelation::After);
  QCOMPARE(facts.activation, MirrorRelation::After);

  const auto unrelated = stateBytes(state(99));
  const auto unrelatedApplied =
      appliedBytes(applied(99, digestWithoutFinalLf(unrelated), true));
  facts = ordinary(authority, unrelated, QByteArrayView(unrelated),
                   QByteArrayView(unrelatedApplied),
                   QByteArrayView(advancing.bytes));
  QVERIFY(facts.recordCoherent);
  QVERIFY(allNeither(facts));
}

void CompositorSettledV2StartupFactsTest::absentPriorSemanticsAreExact() {
  const auto authority = authorityBytes();
  const auto fixture = pendingFixture(OrdinaryPendingKind::Apply,
                                      OrdinaryPendingPhase::Prepared, 7, false);
  QVERIFY(!fixture.record.beforeActivationDesired);
  auto facts = ordinary(authority, fixture.beforeBytes, std::nullopt,
                        std::nullopt, QByteArrayView(fixture.bytes));
  QVERIFY(facts.recordCoherent);
  QCOMPARE(facts.lastGood, MirrorRelation::Before);
  QCOMPARE(facts.activation, MirrorRelation::Before);

  const QByteArray malformed;
  facts = ordinary(authority, fixture.beforeBytes, QByteArrayView(malformed),
                   QByteArrayView(malformed), QByteArrayView(fixture.bytes));
  QVERIFY(!facts.recordCoherent);
  QVERIFY(allNeither(facts));
}

void CompositorSettledV2StartupFactsTest::
    invalidPendingAndCurrentEvidenceNormalizeWithoutAuthorization() {
  const auto authority = authorityBytes();
  const auto fixture = pendingFixture(OrdinaryPendingKind::Apply,
                                      OrdinaryPendingPhase::Prepared);
  const auto activation = appliedBytes(*fixture.record.beforeActivation);

  for (const auto &invalidPending : {
           QByteArray{},
           fixture.bytes.chopped(1),
           fixture.bytes + '\n',
       }) {
    const auto facts = ordinary(
        authority, fixture.beforeBytes, QByteArrayView(fixture.beforeBytes),
        QByteArrayView(activation), QByteArrayView(invalidPending));
    QVERIFY(facts.currentAuthorityCoherent);
    QVERIFY(!facts.recordCoherent);
    QCOMPARE(facts.phase, OrdinaryPendingPhase::Prepared);
    QVERIFY(allNeither(facts));
  }

  auto invalidPhase = fixture.bytes;
  QVERIFY(invalidPhase.contains(QByteArrayLiteral("\"phase\":\"prepared\"")));
  invalidPhase.replace(QByteArrayLiteral("\"phase\":\"prepared\""),
                       QByteArrayLiteral("\"phase\":\"invalid\""));
  auto facts = ordinary(
      authority, fixture.beforeBytes, QByteArrayView(fixture.beforeBytes),
      QByteArrayView(activation), QByteArrayView(invalidPhase));
  QVERIFY(!facts.recordCoherent);
  QCOMPARE(facts.phase, OrdinaryPendingPhase::Prepared);
  QVERIFY(allNeither(facts));

  auto committing = fixture.bytes;
  QVERIFY(committing.contains(QByteArrayLiteral("\"phase\":\"prepared\"")));
  committing.replace(QByteArrayLiteral("\"phase\":\"prepared\""),
                     QByteArrayLiteral("\"phase\":\"committing\""));
  facts = ordinary(authority, fixture.beforeBytes,
                   QByteArrayView(fixture.beforeBytes),
                   QByteArrayView(activation), QByteArrayView(committing));
  QVERIFY(facts.recordCoherent);
  QCOMPARE(facts.phase, OrdinaryPendingPhase::Committing);

  auto wrongKind = fixture.bytes;
  QVERIFY(wrongKind.contains(QByteArrayLiteral("\"kind\":\"apply\"")));
  wrongKind.replace(QByteArrayLiteral("\"kind\":\"apply\""),
                    QByteArrayLiteral("\"kind\":\"recovery\""));
  facts = ordinary(authority, fixture.beforeBytes,
                   QByteArrayView(fixture.beforeBytes),
                   QByteArrayView(activation), QByteArrayView(wrongKind));
  QVERIFY(!facts.recordCoherent);
  QVERIFY(allNeither(facts));

  auto badDesired = fixture.beforeBytes;
  badDesired.append('\n');
  facts = ordinary(authority, badDesired, QByteArrayView(fixture.beforeBytes),
                   QByteArrayView(activation), QByteArrayView(fixture.bytes));
  QVERIFY(!facts.currentAuthorityCoherent);
  QVERIFY(!facts.recordCoherent);
  QVERIFY(allNeither(facts));

  facts = ordinary(authority, fixture.beforeBytes,
                   QByteArrayView(fixture.beforeBytes),
                   QByteArrayView(activation), std::nullopt);
  QVERIFY(!facts.recordCoherent);
  QVERIFY(allNeither(facts));
}

void CompositorSettledV2StartupFactsTest::
    sameRevisionUniquenessRejectsConflictingClaims() {
  const auto authority = authorityBytes();

  auto applyFixture = pendingFixture(OrdinaryPendingKind::Apply,
                                     OrdinaryPendingPhase::Prepared);
  applyFixture.record.beforeActivation->snapshotDigest =
      QString(64, QLatin1Char('d'));
  const auto encodedApply = serializeOrdinaryPendingRecordV2(
      applyFixture.record, catalogV2_, actionCatalogV2_);
  QVERIFY(!encodedApply);

  auto recoveryFixture = pendingFixture(OrdinaryPendingKind::Recovery,
                                        OrdinaryPendingPhase::Prepared);
  const auto differentBefore = stateBytes(state(7, 2));
  recoveryFixture.record.beforeDesiredDigest =
      digestWithoutFinalLf(differentBefore);
  const auto encodedRecovery = serializeOrdinaryPendingRecordV2(
      recoveryFixture.record, catalogV2_, actionCatalogV2_);
  QVERIFY(encodedRecovery);
  const auto recoveryActivation =
      appliedBytes(*recoveryFixture.record.beforeActivation);
  auto facts = ordinary(authority, differentBefore,
                        QByteArrayView(recoveryFixture.beforeBytes),
                        QByteArrayView(recoveryActivation),
                        QByteArrayView(*encodedRecovery.value));
  QVERIFY(!facts.recordCoherent);
  QVERIFY(allNeither(facts));

  const auto currentDifferent = stateBytes(state(7, 3));
  facts = ordinary(authority, currentDifferent,
                   QByteArrayView(recoveryFixture.beforeBytes),
                   QByteArrayView(recoveryActivation),
                   QByteArrayView(recoveryFixture.bytes));
  QVERIFY(!facts.recordCoherent);
  QVERIFY(allNeither(facts));
}

void CompositorSettledV2StartupFactsTest::
    snapshotDomainExcludesExactlyOneProvenLf() {
  const auto authority = authorityBytes();
  const auto lastGood = stateBytes(state(5));
  const auto correct =
      appliedBytes(applied(5, digestWithoutFinalLf(lastGood), true));
  auto facts = noPending(authority, stateBytes(state(7)),
                         QByteArrayView(lastGood), QByteArrayView(correct));
  QVERIFY(facts.presentPairCoherent);

  const auto wrong =
      appliedBytes(applied(5, digestIncludingFinalLf(lastGood), true));
  facts = noPending(authority, stateBytes(state(7)), QByteArrayView(lastGood),
                    QByteArrayView(wrong));
  QVERIFY(!facts.presentPairCoherent);

  auto noLf = lastGood.chopped(1);
  facts = noPending(authority, stateBytes(state(7)), QByteArrayView(noLf),
                    QByteArrayView(correct));
  QVERIFY(!facts.presentPairCoherent);

  auto twoLf = lastGood;
  twoLf.append('\n');
  facts = noPending(authority, stateBytes(state(7)), QByteArrayView(twoLf),
                    QByteArrayView(correct));
  QVERIFY(!facts.presentPairCoherent);
}

void CompositorSettledV2StartupFactsTest::
    revisionEndpointsAndOversizedViewsFailClosed() {
  const auto authority = authorityBytes();
  for (const auto revision : {
           quint64{0},
           std::numeric_limits<quint64>::max(),
       }) {
    const auto fixture = pendingFixture(
        OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared, revision);
    const auto activation = appliedBytes(*fixture.record.beforeActivation);
    const auto facts = ordinary(
        authority, fixture.beforeBytes, QByteArrayView(fixture.beforeBytes),
        QByteArrayView(activation), QByteArrayView(fixture.bytes));
    QVERIFY(facts.recordCoherent);
    QCOMPARE(facts.desired, MirrorRelation::Both);
  }

  const auto desired = stateBytes(state(0));
  const QByteArray oversizedDesired(maximumDesiredStateBytes + 1, 'x');
  auto facts = ordinary(authority, oversizedDesired, std::nullopt, std::nullopt,
                        QByteArrayView(QByteArrayLiteral("{}\n")));
  QVERIFY(!facts.currentAuthorityCoherent);
  QVERIFY(!facts.recordCoherent);
  QVERIFY(allNeither(facts));

  const QByteArray oversizedAuthority(maximumAuthorityRecordV2Bytes + 1, 'x');
  auto noPendingFacts = noPending(oversizedAuthority, desired);
  QVERIFY(!noPendingFacts.currentAuthorityCoherent);
  QVERIFY(noPendingFacts.sameRevisionRuleSatisfied);

  const QByteArray oversizedLastGood(maximumDesiredStateBytes + 1, 'x');
  const QByteArray oversizedApplied(maximumAppliedRecordV2Bytes + 1, 'x');
  noPendingFacts =
      noPending(authority, desired, QByteArrayView(oversizedLastGood),
                QByteArrayView(oversizedApplied));
  QVERIFY(noPendingFacts.lastGoodPresent);
  QVERIFY(noPendingFacts.appliedPresent);
  QVERIFY(!noPendingFacts.presentPairCoherent);
  QVERIFY(!noPendingFacts.appliedRevisionAtMostDesired);
  QVERIFY(!noPendingFacts.sameRevisionRuleSatisfied);

  const auto validPending = pendingFixture(
      OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared, 0, false);
  facts = ordinary(authority, desired, std::nullopt,
                   QByteArrayView(oversizedApplied),
                   QByteArrayView(validPending.bytes));
  QVERIFY(!facts.recordCoherent);
  QVERIFY(allNeither(facts));

  const QByteArray oversizedPending(maximumOrdinaryPendingRecordV2Bytes + 1,
                                    'x');
  facts = ordinary(authority, desired, std::nullopt, std::nullopt,
                   QByteArrayView(oversizedPending));
  QVERIFY(facts.currentAuthorityCoherent);
  QVERIFY(!facts.recordCoherent);
  QVERIFY(allNeither(facts));
}

void CompositorSettledV2StartupFactsTest::
    malformedViewMetadataFailsBeforeParsing() {
  const auto authority = authorityBytes();
  const auto desired = stateBytes(state(7));
  const auto lastGood = stateBytes(state(5));
  const auto activation =
      appliedBytes(applied(5, digestWithoutFinalLf(lastGood), true));

  // QT_NO_DEBUG is scoped to this test executable so the public Qt view
  // constructor can express metadata an ordinary Debug caller cannot create.
  // The library itself retains its normal build flags and must reject both
  // shapes before calling any parser or view slicing primitive.
  const std::array invalidViews{
      QByteArrayView(authority.constData(), qsizetype{-1}),
      QByteArrayView(static_cast<const char *>(nullptr), qsizetype{1}),
  };
  for (const auto invalid : invalidViews) {
    auto noPendingFacts = noPending(invalid, desired);
    QVERIFY(!noPendingFacts.currentAuthorityCoherent);

    noPendingFacts = noPending(authority, invalid);
    QVERIFY(!noPendingFacts.currentAuthorityCoherent);

    noPendingFacts =
        noPending(authority, desired, invalid, QByteArrayView(activation));
    QVERIFY(noPendingFacts.lastGoodPresent);
    QVERIFY(!noPendingFacts.presentPairCoherent);
    QVERIFY(!noPendingFacts.sameRevisionRuleSatisfied);

    noPendingFacts =
        noPending(authority, desired, QByteArrayView(lastGood), invalid);
    QVERIFY(noPendingFacts.appliedPresent);
    QVERIFY(!noPendingFacts.presentPairCoherent);
    QVERIFY(!noPendingFacts.sameRevisionRuleSatisfied);

    const auto ordinaryFacts =
        ordinary(authority, desired, std::nullopt, std::nullopt, invalid);
    QVERIFY(ordinaryFacts.currentAuthorityCoherent);
    QVERIFY(!ordinaryFacts.recordCoherent);
    QVERIFY(allNeither(ordinaryFacts));
  }
}

QTEST_APPLESS_MAIN(CompositorSettledV2StartupFactsTest)

#include "compositor_settled_v2_startup_facts_test.moc"
