#include "component_plan_builder.h"

#include "component/builtin_component_defaults.h"
#include "component/component_contract.h"

#include <QRegularExpression>
#include <QSet>

#include <array>

namespace HyprShelld::Components {
namespace {

const QRegularExpression &digestPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    return pattern;
}

void appendError(
    ValidationErrors &errors,
    QString path,
    QString code,
    QString message
)
{
    errors.append({
        .path = std::move(path),
        .code = std::move(code),
        .message = std::move(message),
    });
}

bool isExactWorkspaceCatalogEntry(const RuntimeCatalogEntry &entry)
{
    auto capabilities = entry.capabilityIds;
    capabilities.sort();
    return entry.componentId == QLatin1StringView(workspaceSwitcherId)
        && entry.componentType == QStringLiteral("bar-widget")
        && digestPattern().match(entry.packageDigest).hasMatch()
        && entry.origin == QStringLiteral("system")
        && !entry.removable
        && entry.componentApiVersion
            == QLatin1StringView(currentComponentApiVersion)
        && entry.runtimeKind == QStringLiteral("builtin-v1")
        && entry.factory == QLatin1StringView(workspaceSwitcherFactory)
        && entry.runtimeEntryPoint.isEmpty()
        && entry.runtimeArguments.isEmpty()
        && entry.dependencyIds.isEmpty()
        && capabilities
            == QStringList{
                QString::fromLatin1(workspacesActivateCapability),
                QString::fromLatin1(workspacesReadCapability),
            };
}

QStringList *regionByName(SurfaceBarLayout &layout, const QString &name)
{
    if (name == QStringLiteral("start")) {
        return &layout.start;
    }
    if (name == QStringLiteral("center")) {
        return &layout.center;
    }
    return &layout.end;
}

const QStringList *desiredRegionByName(
    const RuntimeDesiredBarLayout &layout,
    const QString &name
)
{
    if (name == QStringLiteral("start")) {
        return &layout.start;
    }
    if (name == QStringLiteral("center")) {
        return &layout.center;
    }
    return &layout.end;
}

} // namespace

ValidationResult<SurfacePlan> buildSurfacePlan(
    const RuntimeCatalogSnapshot &catalog,
    const RuntimeConfigurationSnapshot &configuration
)
{
    ValidationResult<SurfacePlan> result;
    if (!digestPattern().match(catalog.catalogDigest).hasMatch()) {
        appendError(
            result.errors,
            QStringLiteral("$.catalogDigest"),
            QStringLiteral("component-runtime.invalid-catalog-digest"),
            QStringLiteral("The catalog digest is invalid.")
        );
        return result;
    }
    if (configuration.catalogDigest != catalog.catalogDigest) {
        appendError(
            result.errors,
            QStringLiteral("$.catalogDigest"),
            QStringLiteral("component-runtime.catalog-mismatch"),
            QStringLiteral("The component configuration was validated against another catalog.")
        );
        return result;
    }

    QSet<QString> listedIds;
    for (qsizetype index = 0; index < catalog.listedComponentIds.size(); ++index) {
        const auto &componentId = catalog.listedComponentIds.at(index);
        if (!isValidComponentId(componentId)
            || listedIds.contains(componentId)) {
            appendError(
                result.errors,
                QStringLiteral("$.catalog.componentIds[%1]").arg(index),
                QStringLiteral("component-runtime.invalid-catalog-id-set"),
                QStringLiteral("The catalog ID set contains an invalid or duplicate ID.")
            );
            return result;
        }
        listedIds.insert(componentId);
    }
    if (listedIds.size() != catalog.entries.size()) {
        appendError(
            result.errors,
            QStringLiteral("$.catalog"),
            QStringLiteral("component-runtime.incomplete-catalog"),
            QStringLiteral("The hydrated catalog entries do not match the listed ID set.")
        );
        return result;
    }
    for (auto iterator = catalog.entries.cbegin(); iterator != catalog.entries.cend(); ++iterator) {
        if (!listedIds.contains(iterator.key())
            || iterator->componentId != iterator.key()) {
            appendError(
                result.errors,
                QStringLiteral("$.catalog.%1").arg(iterator.key()),
                QStringLiteral("component-runtime.incomplete-catalog"),
                QStringLiteral("A hydrated catalog entry does not match its listed ID.")
            );
            return result;
        }
    }

    const auto workspaceId = QString::fromLatin1(workspaceSwitcherId);
    const auto workspaceCatalog = catalog.entries.constFind(workspaceId);
    if (workspaceCatalog == catalog.entries.cend()
        || !isExactWorkspaceCatalogEntry(*workspaceCatalog)) {
        appendError(
            result.errors,
            QStringLiteral("$.catalog.%1").arg(workspaceId),
            QStringLiteral("component-runtime.invalid-builtin-factory"),
            QStringLiteral("The protected workspace factory declaration is missing or invalid.")
        );
        return result;
    }

    if (configuration.instances.size() > maximumSurfacePlanInstances
        || configuration.barLayouts.size() > maximumSurfacePlanBarLayouts) {
        appendError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("component-runtime.plan-limit"),
            QStringLiteral("The desired visual configuration exceeds the runtime limits.")
        );
        return result;
    }

    SurfacePlan plan;
    plan.catalogDigest = catalog.catalogDigest;
    plan.configurationRevision = configuration.revision;

    QSet<QString> placed;
    auto layoutIds = configuration.barLayouts.keys();
    layoutIds.sort();
    for (const auto &layoutId : layoutIds) {
        const auto &desiredLayout = configuration.barLayouts.value(layoutId);
        if (desiredLayout.outputMode != QStringLiteral("all")) {
            appendError(
                result.errors,
                QStringLiteral("$.layouts.bars.%1.outputs.mode").arg(layoutId),
                QStringLiteral("component-runtime.unsupported-output-selector"),
                QStringLiteral("Only the all-output selector is supported.")
            );
            return result;
        }

        SurfaceBarLayout effectiveLayout;
        effectiveLayout.outputMode = desiredLayout.outputMode;
        const std::array<QString, 3> regionNames {
            QStringLiteral("start"),
            QStringLiteral("center"),
            QStringLiteral("end"),
        };

        for (const auto &regionName : regionNames) {
            const auto *desiredIds = desiredRegionByName(
                desiredLayout,
                regionName
            );
            auto *effectiveIds = regionByName(effectiveLayout, regionName);
            for (qsizetype index = 0; index < desiredIds->size(); ++index) {
                const auto &instanceId = desiredIds->at(index);
                const auto placementPath = QStringLiteral(
                    "$.layouts.bars.%1.regions.%2[%3]"
                ).arg(layoutId, regionName).arg(index);
                if (placed.contains(instanceId)) {
                    appendError(
                        result.errors,
                        placementPath,
                        QStringLiteral("component-runtime.duplicate-placement"),
                        QStringLiteral("A visual instance may be placed only once.")
                    );
                    return result;
                }
                placed.insert(instanceId);

                const auto desiredInstance = configuration.instances.constFind(
                    instanceId
                );
                if (desiredInstance == configuration.instances.cend()) {
                    appendError(
                        result.errors,
                        placementPath,
                        QStringLiteral("component-runtime.dangling-instance"),
                        QStringLiteral("The placement references an unknown instance.")
                    );
                    return result;
                }
                if (!desiredInstance->enabled) {
                    continue;
                }

                const auto desiredComponent = configuration.components.constFind(
                    desiredInstance->componentId
                );
                const auto catalogEntry = catalog.entries.constFind(
                    desiredInstance->componentId
                );
                if (desiredComponent == configuration.components.cend()
                    || catalogEntry == catalog.entries.cend()
                    || !desiredComponent->enabled
                    || desiredComponent->packageDigest
                        != catalogEntry->packageDigest) {
                    // Missing definitions and digest-mismatched desired records
                    // are deliberately inert rather than discarded.
                    continue;
                }

                if (desiredInstance->componentId != workspaceId
                    || !isExactWorkspaceCatalogEntry(*catalogEntry)
                    || !desiredComponent->grantedCapabilities.isEmpty()
                    || !isValidWorkspaceSwitcherSettings(
                        desiredInstance->settings
                    )) {
                    appendError(
                        result.errors,
                        QStringLiteral("$.instances.%1").arg(instanceId),
                        QStringLiteral("component-runtime.invalid-effective-instance"),
                        QStringLiteral("The desired built-in instance is invalid.")
                    );
                    return result;
                }

                plan.instances.insert(instanceId, {
                    .componentId = catalogEntry->componentId,
                    .componentType = catalogEntry->componentType,
                    .packageDigest = catalogEntry->packageDigest,
                    .runtimeKind = catalogEntry->runtimeKind,
                    .factory = catalogEntry->factory,
                    .settings = desiredInstance->settings,
                });
                effectiveIds->append(instanceId);
            }
        }

        plan.barLayouts.insert(layoutId, std::move(effectiveLayout));
    }

    result.value = std::move(plan);
    return result;
}

} // namespace HyprShelld::Components
