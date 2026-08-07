#include "coordinator_policy.h"

#include <QHash>
#include <QtTest>

using HyprShelld::CoordinatorPolicy;

namespace {

const QString configUnit = QStringLiteral("hyprshelld-configd.service");
const QString surfaceUnit = QStringLiteral("hyprshelld-surfaced.service");

} // namespace

class CoordinatorPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesExactAllowlist()
    {
        QCOMPARE(
            CoordinatorPolicy::allowedUnits(),
            QStringList({configUnit, surfaceUnit})
        );
        QVERIFY(CoordinatorPolicy::isKnown(configUnit));
        QVERIFY(CoordinatorPolicy::isKnown(surfaceUnit));
        QVERIFY(!CoordinatorPolicy::isKnown(QStringLiteral("hyprshelld.service")));
        QVERIFY(!CoordinatorPolicy::isKnown(configUnit + QLatin1Char(' ')));
    }

    void ignoresNonfailureStatesBeforeFailure()
    {
        CoordinatorPolicy policy;

        for (const auto &state : {
                 QStringLiteral("inactive"),
                 QStringLiteral("activating"),
                 QStringLiteral("deactivating"),
                 QStringLiteral("reloading"),
                 QStringLiteral("unknown"),
             }) {
            QVERIFY(!policy.applySnapshot({{configUnit, state}}));
            QVERIFY(policy.healthy());
            QVERIFY(policy.failedUnits().isEmpty());
            QVERIFY(policy.failureSummary().isEmpty());
        }
    }

    void latchesFailuresUntilActiveRecovery()
    {
        CoordinatorPolicy policy;

        QVERIFY(policy.applySnapshot({
            {configUnit, QStringLiteral("failed")},
            {surfaceUnit, QStringLiteral("active")},
        }));
        QVERIFY(!policy.healthy());
        QVERIFY(policy.isFailed(configUnit));
        QCOMPARE(policy.failedUnits(), QStringList({configUnit}));
        QCOMPARE(
            policy.failureSummary(),
            QStringLiteral("A HyprShelld component needs attention.")
        );

        for (const auto &state : {
                 QStringLiteral("inactive"),
                 QStringLiteral("activating"),
                 QStringLiteral("deactivating"),
                 QStringLiteral("failed"),
             }) {
            QVERIFY(!policy.applySnapshot({{configUnit, state}}));
            QVERIFY(policy.isFailed(configUnit));
        }

        QVERIFY(policy.applySnapshot({{surfaceUnit, QStringLiteral("failed")}}));
        QCOMPARE(policy.failedUnits(), QStringList({configUnit, surfaceUnit}));
        QCOMPARE(
            policy.failureSummary(),
            QStringLiteral("2 HyprShelld components need attention.")
        );

        QVERIFY(policy.applySnapshot({{configUnit, QStringLiteral("active")}}));
        QCOMPARE(policy.failedUnits(), QStringList({surfaceUnit}));
        QVERIFY(policy.applySnapshot({{surfaceUnit, QStringLiteral("active")}}));
        QVERIFY(policy.healthy());
        QVERIFY(policy.failedUnits().isEmpty());
        QVERIFY(policy.failureSummary().isEmpty());
    }

    void ignoresMissingAndUnknownSnapshotEntries()
    {
        CoordinatorPolicy policy;
        QVERIFY(policy.applySnapshot({{configUnit, QStringLiteral("failed")}}));

        QVERIFY(!policy.applySnapshot({
            {QStringLiteral("unrelated.service"), QStringLiteral("active")},
        }));
        QVERIFY(policy.isFailed(configUnit));
    }
};

QTEST_APPLESS_MAIN(CoordinatorPolicyTest)

#include "coordinator_policy_test.moc"
