#include "activation_requirement.h"

#include "hyprland/json_support.h"

#include <QJsonValue>
#include <QMap>
#include <QSet>

#include <algorithm>

namespace HyprShelld::Compositor {
namespace {

[[nodiscard]] int rank(const ActivationRequirement requirement)
{
    switch (requirement) {
    case ActivationRequirement::None: return 0;
    case ActivationRequirement::Reload: return 1;
    case ActivationRequirement::Restart: return 2;
    case ActivationRequirement::Session: return 3;
    }
    return 3;
}

[[nodiscard]] ActivationRequirement strongest(
    const ActivationRequirement left,
    const ActivationRequirement right
)
{
    return rank(left) >= rank(right) ? left : right;
}

[[nodiscard]] ActivationRequirement fromApplyMode(
    const Hyprland::ApplyMode mode
)
{
    if (mode == Hyprland::ApplyMode::Session) {
        return ActivationRequirement::Session;
    }
    if (mode == Hyprland::ApplyMode::Restart) {
        return ActivationRequirement::Restart;
    }
    return ActivationRequirement::Reload;
}

void includeRequirement(
    ActivationRequirement &current,
    const Hyprland::ApplyMode applyMode
)
{
    current = strongest(current, fromApplyMode(applyMode));
}

[[nodiscard]] const Hyprland::ComplexSurfaceDefinition *surface(
    const Hyprland::Catalog &catalog,
    const QString &id
)
{
    const auto found = std::ranges::find_if(
        catalog.complexSurfaces,
        [&id](const auto &candidate) { return candidate.id == id; }
    );
    return found == catalog.complexSurfaces.end() ? nullptr : &*found;
}

void includeSurfaceRequirement(
    ActivationRequirement &current,
    const Hyprland::Catalog &catalog,
    const QString &surfaceId
)
{
    if (const auto *definition = surface(catalog, surfaceId)) {
        includeRequirement(current, definition->applyMode);
    }
}

template <typename Values>
void includeSurfaceDelta(
    ActivationRequirement &result,
    const Values &before,
    const Values &after,
    const Hyprland::Catalog &catalog,
    const QString &id
)
{
    if (before == after) return;
    if (const auto *definition = surface(catalog, id)) {
        result = strongest(result, fromApplyMode(definition->applyMode));
    }
}

[[nodiscard]] bool curveStructureRequiresRestart(
    const QVector<Hyprland::AnimationCurve> &before,
    const QVector<Hyprland::AnimationCurve> &after
)
{
    QMap<QString, int> beforeTypes;
    for (const auto &curve : before) {
        beforeTypes.insert(
            curve.name, static_cast<int>(curve.parameters.index())
        );
    }
    QMap<QString, int> afterTypes;
    for (const auto &curve : after) {
        afterTypes.insert(
            curve.name, static_cast<int>(curve.parameters.index())
        );
    }
    return beforeTypes != afterTypes;
}

} // namespace

QString activationRequirementName(const ActivationRequirement value)
{
    switch (value) {
    case ActivationRequirement::None: return QStringLiteral("none");
    case ActivationRequirement::Reload: return QStringLiteral("reload");
    case ActivationRequirement::Restart: return QStringLiteral("restart");
    case ActivationRequirement::Session: return QStringLiteral("session");
    }
    return QStringLiteral("session");
}

ActivationRequirement activationRequirementForDesiredState(
    const Hyprland::DesiredState &state,
    const Hyprland::Catalog &catalog
)
{
    ActivationRequirement required = ActivationRequirement::Reload;
    for (auto iterator = state.overrides.constBegin();
         iterator != state.overrides.constEnd(); ++iterator) {
        if (const auto *option = Hyprland::findOption(
                catalog, iterator.key()
            )) {
            includeRequirement(required, option->applyMode);
        }
    }
    const auto include = [&](const bool nonempty, const QString &surfaceId) {
        if (nonempty) {
            includeSurfaceRequirement(required, catalog, surfaceId);
        }
    };
    include(!state.monitors.isEmpty(), QStringLiteral("monitors"));
    include(!state.devices.isEmpty(), QStringLiteral("devices"));
    include(!state.curves.isEmpty(), QStringLiteral("curves"));
    include(!state.animations.isEmpty(), QStringLiteral("animations"));
    include(!state.gestures.isEmpty(), QStringLiteral("gestures"));
    include(!state.workspaceRules.isEmpty(), QStringLiteral("workspaceRules"));
    include(!state.windowRules.isEmpty(), QStringLiteral("windowRules"));
    include(!state.layerRules.isEmpty(), QStringLiteral("layerRules"));
    include(!state.submaps.isEmpty(), QStringLiteral("submaps"));
    include(!state.bindings.isEmpty(), QStringLiteral("bindings"));
    include(!state.permissions.isEmpty(), QStringLiteral("permissions"));
    include(!state.environment.isEmpty(), QStringLiteral("environment"));
    return required;
}

ActivationRequirement activationRequirementForDelta(
    const Hyprland::DesiredState *before,
    const Hyprland::DesiredState &after,
    const Hyprland::Catalog &catalog
)
{
    if (!before) {
        auto result = activationRequirementForDesiredState(after, catalog);
        if (!after.curves.isEmpty()) {
            result = strongest(result, ActivationRequirement::Restart);
        }
        return result;
    }
    ActivationRequirement result = ActivationRequirement::Reload;
    QSet<QString> optionIds;
    for (auto iterator = before->overrides.constBegin();
         iterator != before->overrides.constEnd(); ++iterator) {
        optionIds.insert(iterator.key());
    }
    for (auto iterator = after.overrides.constBegin();
         iterator != after.overrides.constEnd(); ++iterator) {
        optionIds.insert(iterator.key());
    }
    for (const auto &id : optionIds) {
        const auto oldValue = before->overrides.value(
            id, QJsonValue::Undefined
        );
        const auto newValue = after.overrides.value(
            id, QJsonValue::Undefined
        );
        if (Hyprland::JsonSupport::canonicalJson(oldValue)
            == Hyprland::JsonSupport::canonicalJson(newValue)) {
            continue;
        }
        if (const auto *option = Hyprland::findOption(catalog, id)) {
            result = strongest(result, fromApplyMode(option->applyMode));
        }
    }
    includeSurfaceDelta(
        result, before->monitors, after.monitors, catalog,
        QStringLiteral("monitors")
    );
    includeSurfaceDelta(
        result, before->devices, after.devices, catalog,
        QStringLiteral("devices")
    );
    includeSurfaceDelta(
        result, before->curves, after.curves, catalog,
        QStringLiteral("curves")
    );
    if (curveStructureRequiresRestart(before->curves, after.curves)) {
        result = strongest(result, ActivationRequirement::Restart);
    }
    includeSurfaceDelta(
        result, before->animations, after.animations, catalog,
        QStringLiteral("animations")
    );
    includeSurfaceDelta(
        result, before->gestures, after.gestures, catalog,
        QStringLiteral("gestures")
    );
    includeSurfaceDelta(
        result, before->workspaceRules, after.workspaceRules, catalog,
        QStringLiteral("workspaceRules")
    );
    includeSurfaceDelta(
        result, before->windowRules, after.windowRules, catalog,
        QStringLiteral("windowRules")
    );
    includeSurfaceDelta(
        result, before->layerRules, after.layerRules, catalog,
        QStringLiteral("layerRules")
    );
    includeSurfaceDelta(
        result, before->submaps, after.submaps, catalog,
        QStringLiteral("submaps")
    );
    includeSurfaceDelta(
        result, before->bindings, after.bindings, catalog,
        QStringLiteral("bindings")
    );
    includeSurfaceDelta(
        result, before->permissions, after.permissions, catalog,
        QStringLiteral("permissions")
    );
    includeSurfaceDelta(
        result, before->environment, after.environment, catalog,
        QStringLiteral("environment")
    );
    return result;
}

} // namespace HyprShelld::Compositor
