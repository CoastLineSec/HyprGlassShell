#include "component/builtin_component_defaults.h"
#include "component/component_contract.h"
#include "component/surface_plan.h"

#include <QJsonDocument>
#include <QTest>

namespace {

constexpr auto instanceId =
    HyprShelld::Components::workspaceSwitcherDefaultInstanceId;

HyprShelld::Components::SurfacePlan defaultPlan()
{
    using namespace HyprShelld::Components;

    SurfacePlan plan;
    plan.catalogDigest = QString(64, QLatin1Char('a'));
    plan.configurationRevision = 18;
    plan.instances.insert(QString::fromLatin1(instanceId), {
        .componentId = QString::fromLatin1(workspaceSwitcherId),
        .componentType = QStringLiteral("bar-widget"),
        .packageDigest = QString(64, QLatin1Char('b')),
        .runtimeKind = QStringLiteral("builtin-v1"),
        .factory = QString::fromLatin1(workspaceSwitcherFactory),
        .settings = workspaceSwitcherDefaultSettings(),
    });
    plan.barLayouts.insert(QStringLiteral("main"), {
        .outputMode = QStringLiteral("all"),
        .start = {QString::fromLatin1(instanceId)},
    });
    return plan;
}

class SurfacePlanTest final : public QObject {
    Q_OBJECT

private slots:
    void roundTripsDeterministically()
    {
        const auto plan = defaultPlan();
        const auto first = HyprShelld::Components::makeSurfacePlanArtifact(plan);
        QVERIFY2(first, qPrintable(first.errors.value(0).message));
        QCOMPARE(first.value->plan, plan);
        QCOMPARE(first.value->digest.size(), 64);
        QVERIFY(first.value->revision != 0);
        QCOMPARE(
            HyprShelld::Components::surfacePlanDigest(first.value->bytes),
            first.value->digest
        );
        QCOMPARE(
            HyprShelld::Components::surfacePlanRevision(first.value->digest),
            first.value->revision
        );

        const auto second = HyprShelld::Components::makeSurfacePlanArtifact(plan);
        QVERIFY(second);
        QCOMPARE(second.value->bytes, first.value->bytes);
        QCOMPARE(second.value->digest, first.value->digest);

        auto changed = plan;
        auto instance = changed.instances.value(QString::fromLatin1(instanceId));
        instance.settings.insert(QStringLiteral("labelMode"), QStringLiteral("names"));
        changed.instances.insert(QString::fromLatin1(instanceId), instance);
        const auto third = HyprShelld::Components::makeSurfacePlanArtifact(changed);
        QVERIFY(third);
        QVERIFY(third.value->digest != first.value->digest);
        QVERIFY(third.value->revision != first.value->revision);
    }

    void acceptsAuthoritativeEmptyPlan()
    {
        auto plan = defaultPlan();
        plan.instances.clear();
        plan.barLayouts[QStringLiteral("main")].start.clear();

        const auto artifact = HyprShelld::Components::makeSurfacePlanArtifact(plan);
        QVERIFY(artifact);
        QVERIFY(artifact.value->revision != 0);
        QVERIFY(artifact.value->plan.instances.isEmpty());
        QCOMPARE(artifact.value->plan.barLayouts.size(), 1);
    }

    void rejectsDuplicateJsonKeys()
    {
        const QByteArray bytes = R"JSON({
          "formatVersion":1,
          "formatVersion":1,
          "catalogDigest":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "configurationRevision":"0",
          "instances":{},
          "layouts":{"bars":{}}
        })JSON";
        const auto parsed = HyprShelld::Components::parseSurfacePlan(bytes);
        QVERIFY(!parsed);
        QCOMPARE(parsed.errors.constFirst().code, QStringLiteral("json.duplicate-key"));
    }

    void rejectsUnsupportedOrUnclosedValues()
    {
        auto document = QJsonDocument::fromJson(
            HyprShelld::Components::makeSurfacePlanArtifact(defaultPlan())
                .value->bytes
        );
        auto root = document.object();
        root.insert(QStringLiteral("unknown"), true);
        auto parsed = HyprShelld::Components::parseSurfacePlan(
            QJsonDocument(root).toJson(QJsonDocument::Compact)
        );
        QVERIFY(!parsed);
        QCOMPARE(
            parsed.errors.constFirst().code,
            QStringLiteral("surface-plan.closed-object")
        );

        root.remove(QStringLiteral("unknown"));
        auto layouts = root.value(QStringLiteral("layouts")).toObject();
        auto bars = layouts.value(QStringLiteral("bars")).toObject();
        auto main = bars.value(QStringLiteral("main")).toObject();
        auto outputs = main.value(QStringLiteral("outputs")).toObject();
        outputs.insert(QStringLiteral("mode"), QStringLiteral("named"));
        main.insert(QStringLiteral("outputs"), outputs);
        bars.insert(QStringLiteral("main"), main);
        layouts.insert(QStringLiteral("bars"), bars);
        root.insert(QStringLiteral("layouts"), layouts);
        parsed = HyprShelld::Components::parseSurfacePlan(
            QJsonDocument(root).toJson(QJsonDocument::Compact)
        );
        QVERIFY(!parsed);
        QCOMPARE(
            parsed.errors.constFirst().code,
            QStringLiteral("surface-plan.unsupported-output-selector")
        );
    }

    void rejectsDanglingDuplicateAndUnplacedInstances()
    {
        auto plan = defaultPlan();
        plan.barLayouts[QStringLiteral("main")].start = {
            QStringLiteral("11111111-1111-4111-8111-111111111111")
        };
        auto artifact = HyprShelld::Components::makeSurfacePlanArtifact(plan);
        QVERIFY(!artifact);
        QCOMPARE(
            artifact.errors.constFirst().code,
            QStringLiteral("surface-plan.dangling-instance")
        );

        plan = defaultPlan();
        plan.barLayouts[QStringLiteral("main")].center = {
            QString::fromLatin1(instanceId)
        };
        artifact = HyprShelld::Components::makeSurfacePlanArtifact(plan);
        QVERIFY(!artifact);
        QCOMPARE(
            artifact.errors.constFirst().code,
            QStringLiteral("surface-plan.duplicate-placement")
        );

        plan = defaultPlan();
        plan.barLayouts[QStringLiteral("main")].start.clear();
        artifact = HyprShelld::Components::makeSurfacePlanArtifact(plan);
        QVERIFY(!artifact);
        QCOMPARE(
            artifact.errors.constFirst().code,
            QStringLiteral("surface-plan.unplaced-instance")
        );
    }

    void rejectsWrongFactoryAndSettings()
    {
        auto plan = defaultPlan();
        auto instance = plan.instances.value(QString::fromLatin1(instanceId));
        instance.factory = QStringLiteral("other");
        plan.instances.insert(QString::fromLatin1(instanceId), instance);
        auto artifact = HyprShelld::Components::makeSurfacePlanArtifact(plan);
        QVERIFY(!artifact);
        QCOMPARE(
            artifact.errors.constFirst().code,
            QStringLiteral("surface-plan.unsupported-factory-instance")
        );

        plan = defaultPlan();
        instance = plan.instances.value(QString::fromLatin1(instanceId));
        instance.settings.insert(QStringLiteral("maximumApplications"), 6);
        plan.instances.insert(QString::fromLatin1(instanceId), instance);
        artifact = HyprShelld::Components::makeSurfacePlanArtifact(plan);
        QVERIFY(!artifact);
        QCOMPARE(
            artifact.errors.constFirst().code,
            QStringLiteral("surface-plan.unsupported-factory-instance")
        );
    }
};

} // namespace

QTEST_GUILESS_MAIN(SurfacePlanTest)

#include "surface_plan_test.moc"
