#include "compositord/startup_reducer.h"

#include <QList>
#include <QtTest>

#include <array>
#include <limits>

using namespace HyprShelld::Compositor;

namespace {

const QString authorityId = QStringLiteral("11111111111111111111111111111111");
const QString otherAuthorityId =
    QStringLiteral("22222222222222222222222222222222");

[[nodiscard]] SettledV2StartupPrerequisites validPrerequisites() {
  return {
      .safeRootAndExclusiveLease = true,
      .protectedContractsExact = true,
      .authorityTransitionReconciledAndAbsent = true,
      .pendingClassifiedAsAbsentOrOrdinary = true,
      .referencedGenerationsVerified = true,
  };
}

[[nodiscard]] bool before(const MirrorRelation relation) {
  return relation == MirrorRelation::Before || relation == MirrorRelation::Both;
}

[[nodiscard]] bool after(const MirrorRelation relation) {
  return relation == MirrorRelation::After || relation == MirrorRelation::Both;
}

[[nodiscard]] SettledV2StartupDecision expectedPendingDecision(
    const OrdinaryPendingPhase phase, const MirrorRelation desired,
    const MirrorRelation lastGood, const MirrorRelation activation) {
  if (desired == MirrorRelation::Neither ||
      lastGood == MirrorRelation::Neither ||
      activation == MirrorRelation::Neither) {
    return SettledV2StartupDecision::RepairOnly;
  }
  if (phase == OrdinaryPendingPhase::Prepared) {
    return before(desired) && before(lastGood) && before(activation)
               ? SettledV2StartupDecision::RemovePrepared
               : SettledV2StartupDecision::RepairOnly;
  }
  const auto desiredAfter = after(desired);
  const auto lastGoodAfter = after(lastGood);
  const auto activationAfter = after(activation);
  if ((activationAfter && (!desiredAfter || !lastGoodAfter)) ||
      (lastGoodAfter && !desiredAfter)) {
    return SettledV2StartupDecision::RepairOnly;
  }
  return SettledV2StartupDecision::RollForwardCommitting;
}

} // namespace

class CompositorStartupReducerTest final : public QObject {
  Q_OBJECT

private slots:
  void classifiesTheCompleteAuthorityObservationTable();
  void rejectsObservationMetadataThatContradictsItsTag();
  void validatesOnlyTheFourPublicTupleShapes();
  void authorityKindNamesAreExactAndTotal();
  void prerequisiteOrderIsFatalThenDelegateThenRepair();
  void noPendingCoherenceTruthTableIsComplete();
  void pendingMirrorCartesianTruthTableIsComplete();
  void invalidPendingEnumsAlwaysRepair();
  void incoherentPendingAlwaysRepairs();
  void preparedAndCommittingNamedRowsMatchTheFreeze();
  void onlyPreparedToCommittingIsALegalPhaseEdge();
};

void CompositorStartupReducerTest::
    classifiesTheCompleteAuthorityObservationTable() {
  const std::array anchorKinds{
      AuthorityAnchorObservationKind::Missing,
      AuthorityAnchorObservationKind::PresentInvalid,
      AuthorityAnchorObservationKind::ExactV2,
  };
  const std::array desiredKinds{
      DesiredAuthorityObservationKind::Missing,
      DesiredAuthorityObservationKind::PresentInvalid,
      DesiredAuthorityObservationKind::ExactV1,
      DesiredAuthorityObservationKind::ExactV2,
  };

  for (const auto anchorKind : anchorKinds) {
    for (const auto desiredKind : desiredKinds) {
      AuthorityAnchorObservation anchor{
          .kind = anchorKind,
          .authorityId = anchorKind == AuthorityAnchorObservationKind::ExactV2
                             ? authorityId
                             : QString(),
      };
      DesiredAuthorityObservation desired{
          .kind = desiredKind,
          .authorityId = desiredKind == DesiredAuthorityObservationKind::ExactV2
                             ? authorityId
                             : QString(),
          .revision = desiredKind == DesiredAuthorityObservationKind::Missing
                          ? quint64{0}
                          : quint64{19},
      };
      const auto actual = classifyObservedAuthority(anchor, desired);

      if (anchorKind == AuthorityAnchorObservationKind::Missing &&
          desiredKind == DesiredAuthorityObservationKind::Missing) {
        const ObservedAuthorityTuple expected{
            .kind = ObservedAuthorityKind::Absent,
            .authorityId = {},
            .revision = 0,
        };
        QCOMPARE(actual, expected);
      } else if (anchorKind == AuthorityAnchorObservationKind::Missing &&
                 desiredKind == DesiredAuthorityObservationKind::ExactV1) {
        const ObservedAuthorityTuple expected{
            .kind = ObservedAuthorityKind::V1,
            .authorityId = {},
            .revision = 19,
        };
        QCOMPARE(actual, expected);
      } else if (anchorKind == AuthorityAnchorObservationKind::ExactV2 &&
                 desiredKind == DesiredAuthorityObservationKind::ExactV2) {
        const ObservedAuthorityTuple expected{
            .kind = ObservedAuthorityKind::V2,
            .authorityId = authorityId,
            .revision = 19,
        };
        QCOMPARE(actual, expected);
      } else {
        const ObservedAuthorityTuple expected{
            .kind = ObservedAuthorityKind::Unreadable,
            .authorityId = {},
            .revision = 0,
        };
        QCOMPARE(actual, expected);
      }
      QVERIFY(isValidObservedAuthorityTuple(actual));
    }
  }
}

void CompositorStartupReducerTest::
    rejectsObservationMetadataThatContradictsItsTag() {
  QCOMPARE(classifyObservedAuthority(
               {
                   .kind = AuthorityAnchorObservationKind::Missing,
                   .authorityId = authorityId,
               },
               {.kind = DesiredAuthorityObservationKind::Missing})
               .kind,
           ObservedAuthorityKind::Unreadable);
  QCOMPARE(classifyObservedAuthority(
               {.kind = AuthorityAnchorObservationKind::Missing},
               {
                   .kind = DesiredAuthorityObservationKind::ExactV1,
                   .authorityId = authorityId,
                   .revision = 1,
               })
               .kind,
           ObservedAuthorityKind::Unreadable);
  QCOMPARE(classifyObservedAuthority(
               {
                   .kind = AuthorityAnchorObservationKind::ExactV2,
                   .authorityId = authorityId,
               },
               {
                   .kind = DesiredAuthorityObservationKind::ExactV2,
                   .authorityId = otherAuthorityId,
                   .revision = 1,
               })
               .kind,
           ObservedAuthorityKind::Unreadable);
  QCOMPARE(classifyObservedAuthority(
               {
                   .kind = AuthorityAnchorObservationKind::ExactV2,
                   .authorityId = QString(32, QLatin1Char('0')),
               },
               {
                   .kind = DesiredAuthorityObservationKind::ExactV2,
                   .authorityId = QString(32, QLatin1Char('0')),
                   .revision = 1,
               })
               .kind,
           ObservedAuthorityKind::Unreadable);
  QCOMPARE(classifyObservedAuthority(
               {.kind = AuthorityAnchorObservationKind::Missing},
               {
                   .kind = DesiredAuthorityObservationKind::Missing,
                   .revision = 1,
               })
               .kind,
           ObservedAuthorityKind::Unreadable);
  QCOMPARE(classifyObservedAuthority(
               {
                   .kind = static_cast<AuthorityAnchorObservationKind>(999),
                   .authorityId = {},
               },
               {
                   .kind = DesiredAuthorityObservationKind::Missing,
                   .authorityId = {},
                   .revision = 0,
               })
               .kind,
           ObservedAuthorityKind::Unreadable);
  QCOMPARE(classifyObservedAuthority(
               {
                   .kind = AuthorityAnchorObservationKind::Missing,
                   .authorityId = {},
               },
               {
                   .kind = static_cast<DesiredAuthorityObservationKind>(999),
                   .authorityId = {},
                   .revision = 0,
               })
               .kind,
           ObservedAuthorityKind::Unreadable);
}

void CompositorStartupReducerTest::validatesOnlyTheFourPublicTupleShapes() {
  QVERIFY(isValidObservedAuthorityTuple({
      .kind = ObservedAuthorityKind::V1,
      .revision = 0,
  }));
  QVERIFY(isValidObservedAuthorityTuple({
      .kind = ObservedAuthorityKind::V1,
      .revision = std::numeric_limits<quint64>::max(),
  }));
  QVERIFY(isValidObservedAuthorityTuple({
      .kind = ObservedAuthorityKind::V2,
      .authorityId = authorityId,
      .revision = std::numeric_limits<quint64>::max(),
  }));
  QVERIFY(isValidObservedAuthorityTuple({
      .kind = ObservedAuthorityKind::Absent,
  }));
  QVERIFY(isValidObservedAuthorityTuple({
      .kind = ObservedAuthorityKind::Unreadable,
  }));

  QVERIFY(!isValidObservedAuthorityTuple({
      .kind = ObservedAuthorityKind::V1,
      .authorityId = authorityId,
  }));
  QVERIFY(!isValidObservedAuthorityTuple({
      .kind = ObservedAuthorityKind::V2,
      .revision = 1,
  }));
  QVERIFY(!isValidObservedAuthorityTuple({
      .kind = ObservedAuthorityKind::Absent,
      .authorityId = authorityId,
  }));
  QVERIFY(!isValidObservedAuthorityTuple({
      .kind = ObservedAuthorityKind::Absent,
      .revision = 1,
  }));
  QVERIFY(!isValidObservedAuthorityTuple({
      .kind = ObservedAuthorityKind::Unreadable,
      .revision = 1,
  }));
  QVERIFY(!isValidObservedAuthorityTuple({
      .kind = static_cast<ObservedAuthorityKind>(999),
      .authorityId = {},
      .revision = 0,
  }));
}

void CompositorStartupReducerTest::authorityKindNamesAreExactAndTotal() {
  QCOMPARE(observedAuthorityKindName(ObservedAuthorityKind::V1),
           QStringLiteral("v1"));
  QCOMPARE(observedAuthorityKindName(ObservedAuthorityKind::V2),
           QStringLiteral("v2"));
  QCOMPARE(observedAuthorityKindName(ObservedAuthorityKind::Absent),
           QStringLiteral("absent"));
  QCOMPARE(observedAuthorityKindName(ObservedAuthorityKind::Unreadable),
           QStringLiteral("unreadable"));
  QVERIFY(observedAuthorityKindName(static_cast<ObservedAuthorityKind>(999))
              .isEmpty());
}

void CompositorStartupReducerTest::
    prerequisiteOrderIsFatalThenDelegateThenRepair() {
  SettledV2StartupInput input{
      .prerequisites = validPrerequisites(),
      .pending =
          NoPendingCoherenceFacts{
              .currentAuthorityCoherent = true,
              .sameRevisionRuleSatisfied = true,
          },
  };
  QCOMPARE(reduceSettledV2Startup(input), SettledV2StartupDecision::Ready);

  input.prerequisites.safeRootAndExclusiveLease = false;
  QCOMPARE(reduceSettledV2Startup(input), SettledV2StartupDecision::Fatal);
  input.prerequisites.protectedContractsExact = false;
  QCOMPARE(reduceSettledV2Startup(input), SettledV2StartupDecision::Fatal);

  input.prerequisites = validPrerequisites();
  input.prerequisites.protectedContractsExact = false;
  QCOMPARE(reduceSettledV2Startup(input), SettledV2StartupDecision::Fatal);

  input.prerequisites = validPrerequisites();
  input.prerequisites.authorityTransitionReconciledAndAbsent = false;
  QCOMPARE(reduceSettledV2Startup(input),
           SettledV2StartupDecision::DelegatePrerequisite);

  input.prerequisites = validPrerequisites();
  input.prerequisites.pendingClassifiedAsAbsentOrOrdinary = false;
  QCOMPARE(reduceSettledV2Startup(input),
           SettledV2StartupDecision::DelegatePrerequisite);

  input.prerequisites = validPrerequisites();
  input.prerequisites.referencedGenerationsVerified = false;
  QCOMPARE(reduceSettledV2Startup(input), SettledV2StartupDecision::RepairOnly);

  input.prerequisites = validPrerequisites();
  input.prerequisites.authorityTransitionReconciledAndAbsent = false;
  input.prerequisites.referencedGenerationsVerified = false;
  QCOMPARE(reduceSettledV2Startup(input),
           SettledV2StartupDecision::DelegatePrerequisite);
}

void CompositorStartupReducerTest::noPendingCoherenceTruthTableIsComplete() {
  for (int mask = 0; mask < 64; ++mask) {
    const NoPendingCoherenceFacts facts{
        .currentAuthorityCoherent = (mask & 1) != 0,
        .lastGoodPresent = (mask & 2) != 0,
        .appliedPresent = (mask & 4) != 0,
        .presentPairCoherent = (mask & 8) != 0,
        .appliedRevisionAtMostDesired = (mask & 16) != 0,
        .sameRevisionRuleSatisfied = (mask & 32) != 0,
    };
    const auto expected =
        facts.currentAuthorityCoherent && facts.sameRevisionRuleSatisfied &&
                ((!facts.lastGoodPresent && !facts.appliedPresent) ||
                 (facts.lastGoodPresent && facts.appliedPresent &&
                  facts.presentPairCoherent &&
                  facts.appliedRevisionAtMostDesired))
            ? SettledV2StartupDecision::Ready
            : SettledV2StartupDecision::RepairOnly;
    const SettledV2StartupInput input{
        .prerequisites = validPrerequisites(),
        .pending = facts,
    };
    QCOMPARE(reduceSettledV2Startup(input), expected);
  }
}

void CompositorStartupReducerTest::
    pendingMirrorCartesianTruthTableIsComplete() {
  const std::array relations{
      MirrorRelation::Before,
      MirrorRelation::After,
      MirrorRelation::Both,
      MirrorRelation::Neither,
  };
  const std::array phases{
      OrdinaryPendingPhase::Prepared,
      OrdinaryPendingPhase::Committing,
  };
  for (const auto phase : phases) {
    for (const auto desired : relations) {
      for (const auto lastGood : relations) {
        for (const auto activation : relations) {
          const OrdinaryPendingStartupFacts facts{
              .currentAuthorityCoherent = true,
              .recordCoherent = true,
              .phase = phase,
              .desired = desired,
              .lastGood = lastGood,
              .activation = activation,
          };
          const SettledV2StartupInput input{
              .prerequisites = validPrerequisites(),
              .pending = facts,
          };
          QCOMPARE(
              reduceSettledV2Startup(input),
              expectedPendingDecision(phase, desired, lastGood, activation));
        }
      }
    }
  }
}

void CompositorStartupReducerTest::incoherentPendingAlwaysRepairs() {
  const std::array phases{
      OrdinaryPendingPhase::Prepared,
      OrdinaryPendingPhase::Committing,
  };
  for (const auto phase : phases) {
    const SettledV2StartupInput input{
        .prerequisites = validPrerequisites(),
        .pending =
            OrdinaryPendingStartupFacts{
                .currentAuthorityCoherent = true,
                .recordCoherent = false,
                .phase = phase,
                .desired = MirrorRelation::Both,
                .lastGood = MirrorRelation::Both,
                .activation = MirrorRelation::Both,
            },
    };
    QCOMPARE(reduceSettledV2Startup(input),
             SettledV2StartupDecision::RepairOnly);

    const SettledV2StartupInput staleCurrent{
        .prerequisites = validPrerequisites(),
        .pending =
            OrdinaryPendingStartupFacts{
                .currentAuthorityCoherent = false,
                .recordCoherent = true,
                .phase = phase,
                .desired = MirrorRelation::Both,
                .lastGood = MirrorRelation::Both,
                .activation = MirrorRelation::Both,
            },
    };
    QCOMPARE(reduceSettledV2Startup(staleCurrent),
             SettledV2StartupDecision::RepairOnly);
  }
}

void CompositorStartupReducerTest::invalidPendingEnumsAlwaysRepair() {
  const auto invalidRelation = static_cast<MirrorRelation>(999);
  const auto invalidPhase = static_cast<OrdinaryPendingPhase>(999);
  const QList<OrdinaryPendingStartupFacts> invalid{
      {
          .currentAuthorityCoherent = true,
          .recordCoherent = true,
          .phase = OrdinaryPendingPhase::Prepared,
          .desired = invalidRelation,
          .lastGood = MirrorRelation::Before,
          .activation = MirrorRelation::Before,
      },
      {
          .currentAuthorityCoherent = true,
          .recordCoherent = true,
          .phase = OrdinaryPendingPhase::Prepared,
          .desired = MirrorRelation::Before,
          .lastGood = invalidRelation,
          .activation = MirrorRelation::Before,
      },
      {
          .currentAuthorityCoherent = true,
          .recordCoherent = true,
          .phase = OrdinaryPendingPhase::Prepared,
          .desired = MirrorRelation::Before,
          .lastGood = MirrorRelation::Before,
          .activation = invalidRelation,
      },
      {
          .currentAuthorityCoherent = true,
          .recordCoherent = true,
          .phase = invalidPhase,
          .desired = MirrorRelation::Before,
          .lastGood = MirrorRelation::Before,
          .activation = MirrorRelation::Before,
      },
  };
  for (const auto &facts : invalid) {
    const SettledV2StartupInput input{
        .prerequisites = validPrerequisites(),
        .pending = facts,
    };
    QCOMPARE(reduceSettledV2Startup(input),
             SettledV2StartupDecision::RepairOnly);
  }
}

void CompositorStartupReducerTest::
    preparedAndCommittingNamedRowsMatchTheFreeze() {
  struct Row final {
    OrdinaryPendingPhase phase;
    MirrorRelation desired;
    MirrorRelation lastGood;
    MirrorRelation activation;
    SettledV2StartupDecision expected;
  };
  const QList<Row> rows{
      {OrdinaryPendingPhase::Prepared, MirrorRelation::Before,
       MirrorRelation::Before, MirrorRelation::Before,
       SettledV2StartupDecision::RemovePrepared},
      {OrdinaryPendingPhase::Prepared, MirrorRelation::After,
       MirrorRelation::Before, MirrorRelation::Before,
       SettledV2StartupDecision::RepairOnly},
      {OrdinaryPendingPhase::Committing, MirrorRelation::Before,
       MirrorRelation::Before, MirrorRelation::Before,
       SettledV2StartupDecision::RollForwardCommitting},
      {OrdinaryPendingPhase::Committing, MirrorRelation::After,
       MirrorRelation::Before, MirrorRelation::Before,
       SettledV2StartupDecision::RollForwardCommitting},
      {OrdinaryPendingPhase::Committing, MirrorRelation::After,
       MirrorRelation::After, MirrorRelation::Before,
       SettledV2StartupDecision::RollForwardCommitting},
      {OrdinaryPendingPhase::Committing, MirrorRelation::After,
       MirrorRelation::After, MirrorRelation::After,
       SettledV2StartupDecision::RollForwardCommitting},
      {OrdinaryPendingPhase::Committing, MirrorRelation::Before,
       MirrorRelation::Before, MirrorRelation::After,
       SettledV2StartupDecision::RepairOnly},
      {OrdinaryPendingPhase::Committing, MirrorRelation::Before,
       MirrorRelation::After, MirrorRelation::Before,
       SettledV2StartupDecision::RepairOnly},
      {OrdinaryPendingPhase::Committing, MirrorRelation::Before,
       MirrorRelation::After, MirrorRelation::After,
       SettledV2StartupDecision::RepairOnly},
      {OrdinaryPendingPhase::Committing, MirrorRelation::After,
       MirrorRelation::Before, MirrorRelation::After,
       SettledV2StartupDecision::RepairOnly},
  };
  for (const auto &row : rows) {
    const SettledV2StartupInput input{
        .prerequisites = validPrerequisites(),
        .pending =
            OrdinaryPendingStartupFacts{
                .currentAuthorityCoherent = true,
                .recordCoherent = true,
                .phase = row.phase,
                .desired = row.desired,
                .lastGood = row.lastGood,
                .activation = row.activation,
            },
    };
    QCOMPARE(reduceSettledV2Startup(input), row.expected);
  }

  const SettledV2StartupInput applyPrepared{
      .prerequisites = validPrerequisites(),
      .pending =
          OrdinaryPendingStartupFacts{
              .currentAuthorityCoherent = true,
              .recordCoherent = true,
              .phase = OrdinaryPendingPhase::Prepared,
              .desired = MirrorRelation::Both,
              .lastGood = MirrorRelation::Before,
              .activation = MirrorRelation::Before,
          },
  };
  QCOMPARE(reduceSettledV2Startup(applyPrepared),
           SettledV2StartupDecision::RemovePrepared);

  auto applyCommitting = applyPrepared;
  std::get<OrdinaryPendingStartupFacts>(applyCommitting.pending).phase =
      OrdinaryPendingPhase::Committing;
  QCOMPARE(reduceSettledV2Startup(applyCommitting),
           SettledV2StartupDecision::RollForwardCommitting);
}

void CompositorStartupReducerTest::onlyPreparedToCommittingIsALegalPhaseEdge() {
  QVERIFY(isLegalOrdinaryPendingPhaseTransition(
      OrdinaryPendingPhase::Prepared, OrdinaryPendingPhase::Committing));
  QVERIFY(!isLegalOrdinaryPendingPhaseTransition(
      OrdinaryPendingPhase::Prepared, OrdinaryPendingPhase::Prepared));
  QVERIFY(!isLegalOrdinaryPendingPhaseTransition(
      OrdinaryPendingPhase::Committing, OrdinaryPendingPhase::Prepared));
  QVERIFY(!isLegalOrdinaryPendingPhaseTransition(
      OrdinaryPendingPhase::Committing, OrdinaryPendingPhase::Committing));
  const auto invalid = static_cast<OrdinaryPendingPhase>(999);
  QVERIFY(!isLegalOrdinaryPendingPhaseTransition(
      invalid, OrdinaryPendingPhase::Committing));
  QVERIFY(!isLegalOrdinaryPendingPhaseTransition(OrdinaryPendingPhase::Prepared,
                                                 invalid));
  QVERIFY(!isLegalOrdinaryPendingPhaseTransition(invalid, invalid));
}

QTEST_APPLESS_MAIN(CompositorStartupReducerTest)

#include "compositor_startup_reducer_test.moc"
