#include "component/builtin_component_defaults.h"
#include "component/component_contract.h"
#include "coordinator/components/component_plan_builder.h"
#include "coordinator/components/component_plan_controller.h"

#include <QSignalSpy>
#include <QTest>

namespace {

using namespace HyprShelld::Components;

const QString digestA(64, QLatin1Char('a'));
const QString digestB(64, QLatin1Char('b'));
const QString workspaceId = QString::fromLatin1(workspaceSwitcherId);
const QString instanceId = QString::fromLatin1(
    workspaceSwitcherDefaultInstanceId
);

RuntimeCatalogSnapshot catalogSnapshot()
{
    RuntimeCatalogSnapshot snapshot;
    snapshot.catalogDigest = digestA;
    snapshot.listedComponentIds = {workspaceId};
    snapshot.entries.insert(workspaceId, {
        .componentId = workspaceId,
        .componentType = QStringLiteral("bar-widget"),
        .packageDigest = digestB,
        .origin = QStringLiteral("system"),
        .removable = false,
        .componentApiVersion = QStringLiteral("1.0"),
        .runtimeKind = QStringLiteral("builtin-v1"),
        .factory = QString::fromLatin1(workspaceSwitcherFactory),
        .capabilityIds = {
            QString::fromLatin1(workspacesReadCapability),
            QString::fromLatin1(workspacesActivateCapability),
        },
    });
    return snapshot;
}

RuntimeConfigurationSnapshot configurationSnapshot()
{
    RuntimeConfigurationSnapshot snapshot;
    snapshot.catalogDigest = digestA;
    snapshot.revision = 4;
    snapshot.components.insert(workspaceId, {
        .enabled = true,
        .packageDigest = digestB,
    });
    snapshot.instances.insert(instanceId, {
        .componentId = workspaceId,
        .enabled = true,
        .settings = workspaceSwitcherDefaultSettings(),
    });
    snapshot.barLayouts.insert(QStringLiteral("main"), {
        .outputMode = QStringLiteral("all"),
        .start = {instanceId},
    });
    return snapshot;
}

class ComponentPlanTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsDefaultEffectivePlan()
    {
        const auto plan = buildSurfacePlan(
            catalogSnapshot(),
            configurationSnapshot()
        );
        QVERIFY(plan);
        QCOMPARE(plan.value->catalogDigest, digestA);
        QCOMPARE(plan.value->configurationRevision, 4ULL);
        QCOMPARE(plan.value->instances.size(), 1);
        QCOMPARE(
            plan.value->barLayouts.value(QStringLiteral("main")).start,
            QStringList{instanceId}
        );
    }

    void disabledOrInertRecordsProduceAuthoritativeEmpty()
    {
        auto configuration = configurationSnapshot();
        configuration.components[workspaceId].enabled = false;
        auto plan = buildSurfacePlan(catalogSnapshot(), configuration);
        QVERIFY(plan);
        QVERIFY(plan.value->instances.isEmpty());
        QVERIFY(plan.value->barLayouts.value(QStringLiteral("main")).start.isEmpty());

        configuration = configurationSnapshot();
        configuration.instances[instanceId].enabled = false;
        plan = buildSurfacePlan(catalogSnapshot(), configuration);
        QVERIFY(plan);
        QVERIFY(plan.value->instances.isEmpty());

        configuration = configurationSnapshot();
        configuration.components[workspaceId].packageDigest = QString(64, u'c');
        plan = buildSurfacePlan(catalogSnapshot(), configuration);
        QVERIFY(plan);
        QVERIFY(plan.value->instances.isEmpty());
    }

    void rejectsMismatchedCatalogOrBuiltInAuthority()
    {
        auto configuration = configurationSnapshot();
        configuration.catalogDigest = QString(64, u'c');
        auto plan = buildSurfacePlan(catalogSnapshot(), configuration);
        QVERIFY(!plan);
        QCOMPARE(
            plan.errors.constFirst().code,
            QStringLiteral("component-runtime.catalog-mismatch")
        );

        auto catalog = catalogSnapshot();
        catalog.entries[workspaceId].origin = QStringLiteral("user");
        plan = buildSurfacePlan(catalog, configurationSnapshot());
        QVERIFY(!plan);
        QCOMPARE(
            plan.errors.constFirst().code,
            QStringLiteral("component-runtime.invalid-builtin-factory")
        );

        configuration = configurationSnapshot();
        configuration.components[workspaceId].grantedCapabilities = {
            QString::fromLatin1(workspacesReadCapability)
        };
        plan = buildSurfacePlan(catalogSnapshot(), configuration);
        QVERIFY(!plan);
        QCOMPARE(
            plan.errors.constFirst().code,
            QStringLiteral("component-runtime.invalid-effective-instance")
        );
    }

    void rejectsInvalidPlacementAndSettings()
    {
        auto configuration = configurationSnapshot();
        configuration.barLayouts[QStringLiteral("main")].center = {instanceId};
        auto plan = buildSurfacePlan(catalogSnapshot(), configuration);
        QVERIFY(!plan);
        QCOMPARE(
            plan.errors.constFirst().code,
            QStringLiteral("component-runtime.duplicate-placement")
        );

        configuration = configurationSnapshot();
        configuration.instances[instanceId].settings.insert(
            QStringLiteral("scrollMode"),
            QStringLiteral("wrap")
        );
        plan = buildSurfacePlan(catalogSnapshot(), configuration);
        QVERIFY(!plan);
        QCOMPARE(
            plan.errors.constFirst().code,
            QStringLiteral("component-runtime.invalid-effective-instance")
        );
    }

    void controllerRetainsLastKnownGood()
    {
        HyprShelld::ComponentPlanController controller;
        QSignalSpy changed(&controller, &HyprShelld::ComponentPlanController::runtimeChanged);
        QCOMPARE(controller.stateName(), QStringLiteral("hydrating"));
        QCOMPARE(controller.revision(), 0ULL);

        QVERIFY(controller.acceptSnapshots(
            catalogSnapshot(),
            configurationSnapshot()
        ));
        QCOMPARE(controller.stateName(), QStringLiteral("authoritative"));
        QVERIFY(controller.revision() != 0);
        const auto revision = controller.revision();
        const auto digest = controller.digest();

        controller.beginHydration();
        QCOMPARE(controller.stateName(), QStringLiteral("retained"));
        QCOMPARE(controller.revision(), revision);
        controller.hydrationFailed(QStringLiteral("new snapshot is invalid"));
        QCOMPARE(controller.stateName(), QStringLiteral("retained"));
        QCOMPARE(controller.digest(), digest);

        auto configuration = configurationSnapshot();
        configuration.components[workspaceId].enabled = false;
        configuration.revision += 1;
        QVERIFY(controller.acceptSnapshots(catalogSnapshot(), configuration));
        QCOMPARE(controller.stateName(), QStringLiteral("authoritative"));
        QVERIFY(controller.revision() != revision);
        QVERIFY(controller.artifact()->plan.instances.isEmpty());
        QVERIFY(changed.count() >= 4);
    }

    void coldFailureNeverInventsAPlan()
    {
        HyprShelld::ComponentPlanController controller;
        controller.sourceUnavailable(QStringLiteral("component truth unavailable"));
        QCOMPARE(controller.stateName(), QStringLiteral("unavailable"));
        QCOMPARE(controller.revision(), 0ULL);
        QVERIFY(controller.digest().isEmpty());
        QVERIFY(controller.artifact() == nullptr);
    }
};

} // namespace

QTEST_GUILESS_MAIN(ComponentPlanTest)

#include "component_plan_test.moc"
