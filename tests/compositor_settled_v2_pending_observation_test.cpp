#include "compositord/settled_v2_pending_observation.h"

#include "compositord/authority_records.h"
#include "compositord/ordinary_pending_record.h"

#include "hyprland/desired_state.h"

#include <QCryptographicHash>
#include <QFile>
#include <QtTest>

#include <array>
#include <limits>
#include <optional>
#include <variant>

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

void compareFacts(const NoPendingCoherenceFacts &actual,
                  const NoPendingCoherenceFacts &expected) {
  QCOMPARE(actual.currentAuthorityCoherent, expected.currentAuthorityCoherent);
  QCOMPARE(actual.lastGoodPresent, expected.lastGoodPresent);
  QCOMPARE(actual.appliedPresent, expected.appliedPresent);
  QCOMPARE(actual.presentPairCoherent, expected.presentPairCoherent);
  QCOMPARE(actual.appliedRevisionAtMostDesired,
           expected.appliedRevisionAtMostDesired);
  QCOMPARE(actual.sameRevisionRuleSatisfied,
           expected.sameRevisionRuleSatisfied);
}

void compareFacts(const OrdinaryPendingStartupFacts &actual,
                  const OrdinaryPendingStartupFacts &expected) {
  QCOMPARE(actual.currentAuthorityCoherent, expected.currentAuthorityCoherent);
  QCOMPARE(actual.recordCoherent, expected.recordCoherent);
  QCOMPARE(actual.phase, expected.phase);
  QCOMPARE(actual.desired, expected.desired);
  QCOMPARE(actual.lastGood, expected.lastGood);
  QCOMPARE(actual.activation, expected.activation);
}

} // namespace

class CompositorSettledV2PendingObservationTest final : public QObject {
  Q_OBJECT

private:
  Catalog catalogV2_;
  ActionCatalog actionCatalogV2_;

  struct PendingFixture final {
    OrdinaryPendingRecordV2 record;
    QByteArray bytes;
    QByteArray beforeBytes;
    QByteArray afterBytes;
    QByteArray beforeAppliedBytes;
  };

  [[nodiscard]] DesiredStateV2
  state(const quint64 revision, const QString &authorityId = authorityA) const {
    const auto initial =
        defaultDormantDesiredStateV2(catalogV2_, actionCatalogV2_, authorityId);
    Q_ASSERT(initial);
    auto value = *initial.value;
    value.semanticState.revision = revision;
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

  [[nodiscard]] AppliedRecordV2 applied(const quint64 revision,
                                        const QString &snapshotDigest,
                                        const bool before) const {
    return {
        .authorityId = authorityA,
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
                 const bool beforeActivationPresent = true) const {
    const auto advancing = kind != OrdinaryPendingKind::Apply;
    const auto afterState =
        state(advancing ? expectedRevision + 1 : expectedRevision);
    const auto afterBytes = stateBytes(afterState);
    const auto afterDigest = digestWithoutFinalLf(afterBytes);

    auto beforeState = afterState;
    beforeState.semanticState.revision = expectedRevision;
    const auto beforeBytes = stateBytes(beforeState);
    const auto beforeDigest = digestWithoutFinalLf(beforeBytes);

    OrdinaryPendingRecordV2 record{
        .authorityId = authorityA,
        .kind = kind,
        .phase = phase,
        .expectedRevision = expectedRevision,
        .beforeDesiredDigest =
            kind == OrdinaryPendingKind::Apply ? afterDigest : beforeDigest,
        .beforeActivationDesired =
            beforeActivationPresent
                ? std::optional{OrdinaryPendingDesiredMaterialV2{
                      .state = beforeState,
                      .bytes = beforeBytes,
                  }}
                : std::nullopt,
        .candidateSnapshot = afterState,
        .candidateSnapshotBytes = afterBytes,
        .snapshotDigest = afterDigest,
        .afterActivation =
            applied(afterState.semanticState.revision, afterDigest, false),
        .beforeActivation = beforeActivationPresent
                                ? std::optional<AppliedRecordV2>{applied(
                                      expectedRevision, beforeDigest, true)}
                                : std::nullopt,
    };
    const auto encoded =
        serializeOrdinaryPendingRecordV2(record, catalogV2_, actionCatalogV2_);
    Q_ASSERT(encoded);
    return {
        .record = record,
        .bytes = *encoded.value,
        .beforeBytes = beforeBytes,
        .afterBytes = afterBytes,
        .beforeAppliedBytes = record.beforeActivation
                                  ? appliedBytes(*record.beforeActivation)
                                  : QByteArray{},
    };
  }

  [[nodiscard]] SettledV2CurrentRecordBytes
  coherentCurrent(const PendingFixture &fixture,
                  const QByteArrayView authority) const {
    return {
        .authority = authority,
        .desired = fixture.beforeBytes,
        .lastGood = QByteArrayView(fixture.beforeBytes),
        .applied = QByteArrayView(fixture.beforeAppliedBytes),
    };
  }

  [[nodiscard]] std::optional<SettledV2PendingFacts>
  classify(const SettledV2CurrentRecordBytes &current,
           const std::optional<QByteArrayView> pending) const {
    return tryBuildSettledV2PendingFactsV2(current, pending, catalogV2_,
                                           actionCatalogV2_);
  }

private slots:
  void initTestCase();
  void missingObservationMatchesNoPendingBuilder();
  void everyOrdinaryKindAndPhaseMatchesDirectBuilder();
  void applyWithoutBeforeActivationMatchesDirectBuilder();
  void exactOrdinaryClassificationIsIndependentOfCurrentCoherence();
  void everyOtherPresentObservationFailsClosed();
  void revisionEndpointsRemainExactOrdinary();
  void malformedViewMetadataFailsBeforeParsing();
  void borrowedNonterminatedAndAliasedViewsRemainExact();
};

void CompositorSettledV2PendingObservationTest::initTestCase() {
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

void CompositorSettledV2PendingObservationTest::
    missingObservationMatchesNoPendingBuilder() {
  const auto authority = authorityBytes();
  const auto desired = stateBytes(state(7));
  const auto lastGood = stateBytes(state(5));
  const auto activation =
      appliedBytes(applied(5, digestWithoutFinalLf(lastGood), true));
  const SettledV2CurrentRecordBytes current{
      .authority = authority,
      .desired = desired,
      .lastGood = QByteArrayView(lastGood),
      .applied = QByteArrayView(activation),
  };

  const auto classified = classify(current, std::nullopt);
  QVERIFY(classified);
  QVERIFY(std::holds_alternative<NoPendingCoherenceFacts>(*classified));
  compareFacts(
      std::get<NoPendingCoherenceFacts>(*classified),
      buildNoPendingCoherenceFactsV2(current, catalogV2_, actionCatalogV2_));
}

void CompositorSettledV2PendingObservationTest::
    everyOrdinaryKindAndPhaseMatchesDirectBuilder() {
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
      QVERIFY(fixture.record.beforeActivationDesired);
      QCOMPARE(fixture.record.beforeActivationDesired->bytes,
               fixture.beforeBytes);
      const auto current = coherentCurrent(fixture, authority);
      const auto pending = std::optional<QByteArrayView>{fixture.bytes};
      const auto classified = classify(current, pending);
      QVERIFY(classified);
      QVERIFY(std::holds_alternative<OrdinaryPendingStartupFacts>(*classified));
      compareFacts(std::get<OrdinaryPendingStartupFacts>(*classified),
                   buildOrdinaryPendingStartupFactsV2(
                       current, pending, catalogV2_, actionCatalogV2_));
    }
  }
}

void CompositorSettledV2PendingObservationTest::
    applyWithoutBeforeActivationMatchesDirectBuilder() {
  const auto authority = authorityBytes();
  const auto fixture = pendingFixture(OrdinaryPendingKind::Apply,
                                      OrdinaryPendingPhase::Prepared, 7, false);
  QVERIFY(!fixture.record.beforeActivation);
  QVERIFY(!fixture.record.beforeActivationDesired);
  const SettledV2CurrentRecordBytes current{
      .authority = authority,
      .desired = fixture.beforeBytes,
      .lastGood = std::nullopt,
      .applied = std::nullopt,
  };
  const auto pending = std::optional<QByteArrayView>{fixture.bytes};

  const auto classified = classify(current, pending);
  QVERIFY(classified);
  QVERIFY(std::holds_alternative<OrdinaryPendingStartupFacts>(*classified));
  compareFacts(std::get<OrdinaryPendingStartupFacts>(*classified),
               buildOrdinaryPendingStartupFactsV2(current, pending, catalogV2_,
                                                  actionCatalogV2_));
  const auto &facts = std::get<OrdinaryPendingStartupFacts>(*classified);
  QVERIFY(facts.recordCoherent);
  QCOMPARE(facts.lastGood, MirrorRelation::Before);
  QCOMPARE(facts.activation, MirrorRelation::Before);
}

void CompositorSettledV2PendingObservationTest::
    exactOrdinaryClassificationIsIndependentOfCurrentCoherence() {
  const auto fixture = pendingFixture(OrdinaryPendingKind::Recovery,
                                      OrdinaryPendingPhase::Committing);
  const auto wrongAuthority = authorityBytes(authorityB);
  const auto current = coherentCurrent(fixture, wrongAuthority);
  const auto classified = classify(current, QByteArrayView(fixture.bytes));
  QVERIFY(classified);
  QVERIFY(std::holds_alternative<OrdinaryPendingStartupFacts>(*classified));
  const auto &facts = std::get<OrdinaryPendingStartupFacts>(*classified);
  QVERIFY(!facts.currentAuthorityCoherent);
  QVERIFY(!facts.recordCoherent);
  QCOMPARE(facts.phase, OrdinaryPendingPhase::Prepared);
  QCOMPARE(facts.desired, MirrorRelation::Neither);
  QCOMPARE(facts.lastGood, MirrorRelation::Neither);
  QCOMPARE(facts.activation, MirrorRelation::Neither);
}

void CompositorSettledV2PendingObservationTest::
    everyOtherPresentObservationFailsClosed() {
  const auto authority = authorityBytes();
  const auto fixture = pendingFixture(OrdinaryPendingKind::Apply,
                                      OrdinaryPendingPhase::Prepared);
  const auto current = coherentCurrent(fixture, authority);

  auto unknownKind = fixture.bytes;
  QVERIFY(unknownKind.contains(QByteArrayLiteral("\"kind\":\"apply\"")));
  unknownKind.replace(QByteArrayLiteral("\"kind\":\"apply\""),
                      QByteArrayLiteral("\"kind\":\"restart\""));
  const QByteArray opaqueRestartShape =
      QByteArrayLiteral("{\"formatVersion\":2,\"kind\":\"restart\"}\n");
  const std::array invalid{
      QByteArray{},         fixture.bytes.chopped(1),
      fixture.bytes + '\n', QByteArrayLiteral(" ") + fixture.bytes,
      unknownKind,          opaqueRestartShape,
  };
  for (const auto &bytes : invalid) {
    QVERIFY(!classify(current, QByteArrayView(bytes)));
  }

  const QByteArray oversized(maximumOrdinaryPendingRecordV2Bytes + 1, 'x');
  QVERIFY(!classify(current, QByteArrayView(oversized)));

  // A present-invalid record never aliases the disengaged missing branch.
  const auto missing = classify(current, std::nullopt);
  QVERIFY(missing);
  QVERIFY(std::holds_alternative<NoPendingCoherenceFacts>(*missing));
}

void CompositorSettledV2PendingObservationTest::
    revisionEndpointsRemainExactOrdinary() {
  const auto authority = authorityBytes();
  for (const auto revision : {
           quint64{0},
           std::numeric_limits<quint64>::max(),
       }) {
    const auto fixture = pendingFixture(
        OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared, revision);
    const auto classified = classify(coherentCurrent(fixture, authority),
                                     QByteArrayView(fixture.bytes));
    QVERIFY(classified);
    QVERIFY(std::holds_alternative<OrdinaryPendingStartupFacts>(*classified));
    QVERIFY(std::get<OrdinaryPendingStartupFacts>(*classified).recordCoherent);
  }
}

void CompositorSettledV2PendingObservationTest::
    malformedViewMetadataFailsBeforeParsing() {
  const auto authority = authorityBytes();
  const auto fixture = pendingFixture(OrdinaryPendingKind::Apply,
                                      OrdinaryPendingPhase::Prepared);
  const auto current = coherentCurrent(fixture, authority);

  // QT_NO_DEBUG is scoped to this test executable. The dormant library keeps
  // its normal flags and rejects both shapes before invoking a parser.
  const std::array invalidViews{
      QByteArrayView(fixture.bytes.constData(), qsizetype{-1}),
      QByteArrayView(static_cast<const char *>(nullptr), qsizetype{1}),
  };
  for (const auto invalid : invalidViews) {
    QVERIFY(!classify(current, invalid));
  }
}

void CompositorSettledV2PendingObservationTest::
    borrowedNonterminatedAndAliasedViewsRemainExact() {
  const auto authority = authorityBytes();
  const auto fixture = pendingFixture(OrdinaryPendingKind::DisplayPreview,
                                      OrdinaryPendingPhase::Committing);

  auto framed = [](const QByteArray &bytes) {
    QByteArray backing = QByteArrayLiteral("!");
    backing.append(bytes);
    backing.append('?');
    return backing;
  };
  auto authorityBacking = framed(authority);
  auto desiredBacking = framed(fixture.beforeBytes);
  auto appliedBacking = framed(fixture.beforeAppliedBytes);
  auto pendingBacking = framed(fixture.bytes);
  const auto authorityView =
      QByteArrayView(authorityBacking.constData() + 1, authority.size());
  const auto desiredView = QByteArrayView(desiredBacking.constData() + 1,
                                          fixture.beforeBytes.size());
  const auto appliedView = QByteArrayView(appliedBacking.constData() + 1,
                                          fixture.beforeAppliedBytes.size());
  const auto pendingView =
      QByteArrayView(pendingBacking.constData() + 1, fixture.bytes.size());
  const SettledV2CurrentRecordBytes current{
      .authority = authorityView,
      .desired = desiredView,
      .lastGood = desiredView,
      .applied = appliedView,
  };

  const auto classified = classify(current, pendingView);
  QVERIFY(classified);
  QVERIFY(std::holds_alternative<OrdinaryPendingStartupFacts>(*classified));
  authorityBacking.fill('x');
  desiredBacking.fill('x');
  appliedBacking.fill('x');
  pendingBacking.fill('x');
  const auto &facts = std::get<OrdinaryPendingStartupFacts>(*classified);
  QVERIFY(facts.currentAuthorityCoherent);
  QVERIFY(facts.recordCoherent);
  QCOMPARE(facts.phase, OrdinaryPendingPhase::Committing);
}

QTEST_APPLESS_MAIN(CompositorSettledV2PendingObservationTest)

#include "compositor_settled_v2_pending_observation_test.moc"
