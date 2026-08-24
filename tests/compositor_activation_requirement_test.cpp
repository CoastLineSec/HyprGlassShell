#include "compositord/activation_requirement.h"

#include <QJsonObject>
#include <QObject>
#include <QtTest>

#include <utility>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

[[nodiscard]] OptionDefinition option(
    QString id,
    const ApplyMode applyMode
)
{
    OptionDefinition result;
    result.id = std::move(id);
    result.applyMode = applyMode;
    return result;
}

[[nodiscard]] ComplexSurfaceDefinition surface(
    QString id,
    const ApplyMode applyMode
)
{
    ComplexSurfaceDefinition result;
    result.id = std::move(id);
    result.applyMode = applyMode;
    return result;
}

[[nodiscard]] Catalog classifierCatalog()
{
    Catalog catalog;
    catalog.options = {
        option(QStringLiteral("reload-option"), ApplyMode::Reload),
        option(QStringLiteral("restart-option"), ApplyMode::Restart),
        option(QStringLiteral("session-option"), ApplyMode::Session),
    };
    catalog.complexSurfaces = {
        surface(QStringLiteral("monitors"), ApplyMode::Reload),
        surface(QStringLiteral("devices"), ApplyMode::Restart),
        surface(QStringLiteral("curves"), ApplyMode::Reload),
        surface(QStringLiteral("animations"), ApplyMode::Session),
        surface(QStringLiteral("gestures"), ApplyMode::Restart),
        surface(QStringLiteral("workspaceRules"), ApplyMode::Session),
        surface(QStringLiteral("windowRules"), ApplyMode::Restart),
        surface(QStringLiteral("layerRules"), ApplyMode::Session),
        surface(QStringLiteral("submaps"), ApplyMode::Restart),
        surface(QStringLiteral("bindings"), ApplyMode::Session),
        surface(QStringLiteral("permissions"), ApplyMode::Restart),
        surface(QStringLiteral("environment"), ApplyMode::Session),
    };
    return catalog;
}

[[nodiscard]] AnimationCurve bezierCurve(
    QString id,
    QString name,
    const double offset = 0.0
)
{
    AnimationCurve curve;
    curve.id = std::move(id);
    curve.name = std::move(name);
    BezierCurveParameters parameters;
    parameters.points = {{
        {0.1 + offset, 0.2 + offset},
        {0.8 - offset, 0.9 - offset},
    }};
    curve.parameters = parameters;
    return curve;
}

[[nodiscard]] AnimationCurve springCurve(
    QString id,
    QString name,
    const double stiffness = 250.0
)
{
    AnimationCurve curve;
    curve.id = std::move(id);
    curve.name = std::move(name);
    curve.parameters = SpringCurveParameters{
        .stiffness = stiffness,
        .dampening = 25.0,
        .mass = 1.0,
    };
    return curve;
}

template <typename Record>
void appendSurfaceMatrixFailures(
    QStringList &failures,
    const QString &surfaceId,
    QVector<Record> DesiredState::*member,
    const Record &first,
    const Record &second,
    const Record &mutated,
    const ActivationRequirement addRemoveRequirement,
    const ActivationRequirement mutationRequirement,
    const ActivationRequirement reorderRequirement,
    const Catalog &catalog
)
{
    const auto check = [&](
        const QString &operation,
        const DesiredState &before,
        const DesiredState &after,
        const ActivationRequirement expected
    ) {
        const auto actual = activationRequirementForDelta(
            &before, after, catalog
        );
        if (actual != expected) {
            failures.append(QStringLiteral("%1 %2: expected %3, got %4")
                                .arg(
                                    surfaceId,
                                    operation,
                                    activationRequirementName(expected),
                                    activationRequirementName(actual)
                                ));
        }
    };
    const auto checkBothDirections = [&](
        const QString &operation,
        const DesiredState &left,
        const DesiredState &right,
        const ActivationRequirement expected
    ) {
        check(operation + QStringLiteral(" forward"), left, right, expected);
        check(operation + QStringLiteral(" reverse"), right, left, expected);
    };

    DesiredState empty;
    DesiredState one;
    one.*member = {first};
    checkBothDirections(
        QStringLiteral("add/remove"), empty, one, addRemoveRequirement
    );

    DesiredState changed;
    changed.*member = {mutated};
    checkBothDirections(
        QStringLiteral("mutate"), one, changed, mutationRequirement
    );

    DesiredState ordered;
    ordered.*member = {first, second};
    DesiredState reordered;
    reordered.*member = {second, first};
    checkBothDirections(
        QStringLiteral("reorder"), ordered, reordered, reorderRequirement
    );
}

} // namespace

class CompositorActivationRequirementTest final : public QObject
{
    Q_OBJECT

private slots:
    void namesAndInvalidFallbackRemainStable()
    {
        QCOMPARE(
            activationRequirementName(ActivationRequirement::None),
            QStringLiteral("none")
        );
        QCOMPARE(
            activationRequirementName(ActivationRequirement::Reload),
            QStringLiteral("reload")
        );
        QCOMPARE(
            activationRequirementName(ActivationRequirement::Restart),
            QStringLiteral("restart")
        );
        QCOMPARE(
            activationRequirementName(ActivationRequirement::Session),
            QStringLiteral("session")
        );
        QCOMPARE(
            activationRequirementName(
                static_cast<ActivationRequirement>(-1)
            ),
            QStringLiteral("session")
        );
    }

    void fullStateUsesReloadFloorAndStrongestOption()
    {
        const auto catalog = classifierCatalog();
        DesiredState state;
        QCOMPARE(
            activationRequirementForDesiredState(state, catalog),
            ActivationRequirement::Reload
        );

        state.overrides.insert(QStringLiteral("unknown-option"), true);
        QCOMPARE(
            activationRequirementForDesiredState(state, catalog),
            ActivationRequirement::Reload
        );
        state.overrides.insert(QStringLiteral("restart-option"), false);
        QCOMPARE(
            activationRequirementForDesiredState(state, catalog),
            ActivationRequirement::Restart
        );
        state.overrides.insert(QStringLiteral("session-option"), 0);
        QCOMPARE(
            activationRequirementForDesiredState(state, catalog),
            ActivationRequirement::Session
        );
    }

    void fullStateChecksAllTwelveSurfaceVectors()
    {
        const auto catalog = classifierCatalog();
        const auto classify = [&catalog](const DesiredState &state) {
            return activationRequirementForDesiredState(state, catalog);
        };

        DesiredState state;
        state.monitors.append(MonitorConfiguration{});
        QCOMPARE(classify(state), ActivationRequirement::Reload);
        state = {};
        state.devices.append(DeviceConfiguration{});
        QCOMPARE(classify(state), ActivationRequirement::Restart);
        state = {};
        state.curves.append(AnimationCurve{});
        QCOMPARE(classify(state), ActivationRequirement::Reload);
        state = {};
        state.animations.append(AnimationConfiguration{});
        QCOMPARE(classify(state), ActivationRequirement::Session);
        state = {};
        state.gestures.append(GestureConfiguration{});
        QCOMPARE(classify(state), ActivationRequirement::Restart);
        state = {};
        state.workspaceRules.append(WorkspaceRule{});
        QCOMPARE(classify(state), ActivationRequirement::Session);
        state = {};
        state.windowRules.append(WindowRule{});
        QCOMPARE(classify(state), ActivationRequirement::Restart);
        state = {};
        state.layerRules.append(LayerRule{});
        QCOMPARE(classify(state), ActivationRequirement::Session);
        state = {};
        state.submaps.append(SubmapConfiguration{});
        QCOMPARE(classify(state), ActivationRequirement::Restart);
        state = {};
        state.bindings.append(BindingConfiguration{});
        QCOMPARE(classify(state), ActivationRequirement::Session);
        state = {};
        state.permissions.append(PermissionConfiguration{});
        QCOMPARE(classify(state), ActivationRequirement::Restart);
        state = {};
        state.environment.append(EnvironmentConfiguration{});
        QCOMPARE(classify(state), ActivationRequirement::Session);
    }

    void presentBaselineHasReloadFloorAndIgnoresMetadata()
    {
        const auto catalog = classifierCatalog();
        DesiredState before;
        auto after = before;
        QCOMPARE(
            activationRequirementForDelta(&before, after, catalog),
            ActivationRequirement::Reload
        );
        QCOMPARE(
            activationRequirementForDelta(&after, before, catalog),
            ActivationRequirement::Reload
        );

        after.formatVersion = 99;
        after.revision = 42;
        after.targetHyprland = QStringLiteral("different");
        after.catalogDigest = QStringLiteral("different-catalog");
        after.actionCatalogDigest = QStringLiteral("different-actions");
        after.compatibility = CompatibilityDecision::UnsupportedFuture;
        after.readOnly = true;
        after.opaqueFutureDocument = QJsonObject{
            {QStringLiteral("opaque"), true},
        };
        QCOMPARE(
            activationRequirementForDelta(&before, after, catalog),
            ActivationRequirement::Reload
        );
    }

    void optionDeltaKeepsLegacyCanonicalUndefinedEquality()
    {
        const auto catalog = classifierCatalog();
        DesiredState before;
        DesiredState after;

        // The legacy canonical encoder spells both a missing map value
        // (QJsonValue::Undefined) and explicit null as JSON null.
        after.overrides.insert(
            QStringLiteral("session-option"), QJsonValue::Null
        );
        QCOMPARE(
            activationRequirementForDelta(&before, after, catalog),
            ActivationRequirement::Reload
        );

        after.overrides.insert(QStringLiteral("session-option"), true);
        QCOMPARE(
            activationRequirementForDelta(&before, after, catalog),
            ActivationRequirement::Session
        );

        before.overrides.insert(QStringLiteral("restart-option"), true);
        after = {};
        QCOMPARE(
            activationRequirementForDelta(&before, after, catalog),
            ActivationRequirement::Restart
        );
        QCOMPARE(
            activationRequirementForDelta(&after, before, catalog),
            ActivationRequirement::Restart
        );

        before = {};
        before.overrides.insert(
            QStringLiteral("restart-option"),
            QJsonObject{
                {QStringLiteral("b"), 2},
                {QStringLiteral("a"), 1},
            }
        );
        after = {};
        after.overrides.insert(
            QStringLiteral("restart-option"),
            QJsonObject{
                {QStringLiteral("a"), 1},
                {QStringLiteral("b"), 2},
            }
        );
        QCOMPARE(
            activationRequirementForDelta(&before, after, catalog),
            ActivationRequirement::Reload
        );
        QCOMPARE(
            activationRequirementForDelta(&after, before, catalog),
            ActivationRequirement::Reload
        );
    }

    void deltaChecksAllTwelveSurfaceVectorsExactly()
    {
        const auto catalog = classifierCatalog();
        QStringList failures;

        MonitorConfiguration monitorA;
        monitorA.id = QStringLiteral("monitor-a");
        monitorA.selector = QStringLiteral("DP-1");
        auto monitorB = monitorA;
        monitorB.id = QStringLiteral("monitor-b");
        monitorB.selector = QStringLiteral("HDMI-A-1");
        auto monitorChanged = monitorA;
        monitorChanged.mode = QStringLiteral("1920x1080@60");
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("monitors"), &DesiredState::monitors,
            monitorA, monitorB, monitorChanged,
            ActivationRequirement::Reload, ActivationRequirement::Reload,
            ActivationRequirement::Reload, catalog
        );

        DeviceConfiguration deviceA;
        deviceA.id = QStringLiteral("device-a");
        deviceA.selector = QStringLiteral("primary-keyboard");
        auto deviceB = deviceA;
        deviceB.id = QStringLiteral("device-b");
        deviceB.selector = QStringLiteral("secondary-keyboard");
        auto deviceChanged = deviceA;
        deviceChanged.enabled = false;
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("devices"), &DesiredState::devices,
            deviceA, deviceB, deviceChanged,
            ActivationRequirement::Restart, ActivationRequirement::Restart,
            ActivationRequirement::Restart, catalog
        );

        const auto curveA = bezierCurve(
            QStringLiteral("curve-a"), QStringLiteral("ease")
        );
        const auto curveB = springCurve(
            QStringLiteral("curve-b"), QStringLiteral("bounce")
        );
        const auto curveChanged = bezierCurve(
            QStringLiteral("curve-a-renamed-id"),
            QStringLiteral("ease"),
            0.05
        );
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("curves"), &DesiredState::curves,
            curveA, curveB, curveChanged,
            ActivationRequirement::Restart, ActivationRequirement::Reload,
            ActivationRequirement::Reload, catalog
        );

        AnimationConfiguration animationA;
        animationA.id = QStringLiteral("animation-a");
        animationA.name = QStringLiteral("windows");
        auto animationB = animationA;
        animationB.id = QStringLiteral("animation-b");
        animationB.name = QStringLiteral("layers");
        auto animationChanged = animationA;
        animationChanged.speed = 2.0;
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("animations"), &DesiredState::animations,
            animationA, animationB, animationChanged,
            ActivationRequirement::Session, ActivationRequirement::Session,
            ActivationRequirement::Session, catalog
        );

        GestureConfiguration gestureA;
        gestureA.id = QStringLiteral("gesture-a");
        gestureA.direction = QStringLiteral("left");
        auto gestureB = gestureA;
        gestureB.id = QStringLiteral("gesture-b");
        gestureB.direction = QStringLiteral("right");
        auto gestureChanged = gestureA;
        gestureChanged.fingers = 4;
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("gestures"), &DesiredState::gestures,
            gestureA, gestureB, gestureChanged,
            ActivationRequirement::Restart, ActivationRequirement::Restart,
            ActivationRequirement::Restart, catalog
        );

        WorkspaceRule workspaceA;
        workspaceA.id = QStringLiteral("workspace-a");
        workspaceA.selector = QStringLiteral("1");
        auto workspaceB = workspaceA;
        workspaceB.id = QStringLiteral("workspace-b");
        workspaceB.selector = QStringLiteral("2");
        auto workspaceChanged = workspaceA;
        workspaceChanged.persistent = true;
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("workspaceRules"),
            &DesiredState::workspaceRules, workspaceA, workspaceB,
            workspaceChanged, ActivationRequirement::Session,
            ActivationRequirement::Session, ActivationRequirement::Session,
            catalog
        );

        WindowRule windowA;
        windowA.id = QStringLiteral("window-a");
        windowA.name = QStringLiteral("Window A");
        auto windowB = windowA;
        windowB.id = QStringLiteral("window-b");
        windowB.name = QStringLiteral("Window B");
        auto windowChanged = windowA;
        windowChanged.enabled = false;
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("windowRules"),
            &DesiredState::windowRules, windowA, windowB, windowChanged,
            ActivationRequirement::Restart, ActivationRequirement::Restart,
            ActivationRequirement::Restart, catalog
        );

        LayerRule layerA;
        layerA.id = QStringLiteral("layer-a");
        layerA.name = QStringLiteral("Layer A");
        auto layerB = layerA;
        layerB.id = QStringLiteral("layer-b");
        layerB.name = QStringLiteral("Layer B");
        auto layerChanged = layerA;
        layerChanged.enabled = false;
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("layerRules"),
            &DesiredState::layerRules, layerA, layerB, layerChanged,
            ActivationRequirement::Session, ActivationRequirement::Session,
            ActivationRequirement::Session, catalog
        );

        SubmapConfiguration submapA;
        submapA.id = QStringLiteral("submap-a");
        submapA.name = QStringLiteral("resize");
        auto submapB = submapA;
        submapB.id = QStringLiteral("submap-b");
        submapB.name = QStringLiteral("media");
        auto submapChanged = submapA;
        submapChanged.enabled = false;
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("submaps"), &DesiredState::submaps,
            submapA, submapB, submapChanged,
            ActivationRequirement::Restart, ActivationRequirement::Restart,
            ActivationRequirement::Restart, catalog
        );

        BindingConfiguration bindingA;
        bindingA.id = QStringLiteral("binding-a");
        bindingA.key = QStringLiteral("F7");
        auto bindingB = bindingA;
        bindingB.id = QStringLiteral("binding-b");
        bindingB.key = QStringLiteral("F8");
        auto bindingChanged = bindingA;
        bindingChanged.enabled = false;
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("bindings"), &DesiredState::bindings,
            bindingA, bindingB, bindingChanged,
            ActivationRequirement::Session, ActivationRequirement::Session,
            ActivationRequirement::Session, catalog
        );

        PermissionConfiguration permissionA;
        permissionA.id = QStringLiteral("permission-a");
        permissionA.binary = QStringLiteral("portal-a");
        auto permissionB = permissionA;
        permissionB.id = QStringLiteral("permission-b");
        permissionB.binary = QStringLiteral("portal-b");
        auto permissionChanged = permissionA;
        permissionChanged.mode = QStringLiteral("allow");
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("permissions"),
            &DesiredState::permissions, permissionA, permissionB,
            permissionChanged, ActivationRequirement::Restart,
            ActivationRequirement::Restart, ActivationRequirement::Restart,
            catalog
        );

        EnvironmentConfiguration environmentA;
        environmentA.id = QStringLiteral("environment-a");
        environmentA.name = QStringLiteral("VARIABLE_A");
        auto environmentB = environmentA;
        environmentB.id = QStringLiteral("environment-b");
        environmentB.name = QStringLiteral("VARIABLE_B");
        auto environmentChanged = environmentA;
        environmentChanged.value = QStringLiteral("changed");
        appendSurfaceMatrixFailures(
            failures, QStringLiteral("environment"),
            &DesiredState::environment, environmentA, environmentB,
            environmentChanged, ActivationRequirement::Session,
            ActivationRequirement::Session, ActivationRequirement::Session,
            catalog
        );

        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));

        Catalog missingRows;
        DesiredState before;
        DesiredState after;
        after.environment.append(EnvironmentConfiguration{});
        QCOMPARE(
            activationRequirementForDelta(&before, after, missingRows),
            ActivationRequirement::Reload
        );
        QCOMPARE(
            activationRequirementForDelta(&after, before, missingRows),
            ActivationRequirement::Reload
        );
    }

    void unchangedHighModesStayReloadAndDominanceIsSymmetric()
    {
        const auto catalog = classifierCatalog();
        DesiredState before;
        before.overrides.insert(QStringLiteral("session-option"), true);
        before.devices.append(DeviceConfiguration{
            .id = QStringLiteral("device-a"),
        });
        before.environment.append(EnvironmentConfiguration{
            .id = QStringLiteral("environment-a"),
        });

        auto reloadOnly = before;
        reloadOnly.overrides.insert(QStringLiteral("reload-option"), true);
        QCOMPARE(
            activationRequirementForDelta(&before, reloadOnly, catalog),
            ActivationRequirement::Reload
        );
        QCOMPARE(
            activationRequirementForDelta(&reloadOnly, before, catalog),
            ActivationRequirement::Reload
        );

        auto deletedSessionContent = before;
        deletedSessionContent.environment.clear();
        QCOMPARE(
            activationRequirementForDelta(
                &before, deletedSessionContent, catalog
            ),
            ActivationRequirement::Session
        );
        QCOMPARE(
            activationRequirementForDelta(
                &deletedSessionContent, before, catalog
            ),
            ActivationRequirement::Session
        );

        DesiredState restartSide;
        restartSide.devices.append(DeviceConfiguration{
            .id = QStringLiteral("device-a"),
        });
        DesiredState sessionSide;
        sessionSide.environment.append(EnvironmentConfiguration{
            .id = QStringLiteral("environment-a"),
        });
        QCOMPARE(
            activationRequirementForDelta(
                &restartSide, sessionSide, catalog
            ),
            ActivationRequirement::Session
        );
        QCOMPARE(
            activationRequirementForDelta(
                &sessionSide, restartSide, catalog
            ),
            ActivationRequirement::Session
        );

        restartSide = {};
        restartSide.overrides.insert(QStringLiteral("restart-option"), true);
        sessionSide = {};
        sessionSide.overrides.insert(QStringLiteral("session-option"), true);
        QCOMPARE(
            activationRequirementForDelta(
                &restartSide, sessionSide, catalog
            ),
            ActivationRequirement::Session
        );
        QCOMPARE(
            activationRequirementForDelta(
                &sessionSide, restartSide, catalog
            ),
            ActivationRequirement::Session
        );
    }

    void nullBaselineIsOnlyTheFirstAdoptionClassifier()
    {
        const auto catalog = classifierCatalog();
        DesiredState after;
        QCOMPARE(
            activationRequirementForDelta(nullptr, after, catalog),
            ActivationRequirement::Reload
        );

        after.overrides.insert(QStringLiteral("restart-option"), true);
        QCOMPARE(
            activationRequirementForDelta(nullptr, after, catalog),
            ActivationRequirement::Restart
        );

        after.curves = {bezierCurve(
            QStringLiteral("curve-a"), QStringLiteral("ease")
        )};
        QCOMPARE(
            activationRequirementForDelta(nullptr, after, catalog),
            ActivationRequirement::Restart
        );
        QCOMPARE(
            activationRequirementForDelta(&after, after, catalog),
            ActivationRequirement::Reload
        );

        after.overrides.insert(QStringLiteral("session-option"), true);
        QCOMPARE(
            activationRequirementForDelta(nullptr, after, catalog),
            ActivationRequirement::Session
        );
    }

    void curveMapUsesNameTypeAndLastDuplicateWins()
    {
        const auto catalog = classifierCatalog();
        DesiredState before;
        before.curves = {
            springCurve(
                QStringLiteral("old-spring"), QStringLiteral("duplicate")
            ),
            bezierCurve(
                QStringLiteral("old-bezier"), QStringLiteral("duplicate")
            ),
        };

        DesiredState after;
        after.curves = {bezierCurve(
            QStringLiteral("new-bezier"), QStringLiteral("duplicate"), 0.05
        )};
        QCOMPARE(
            activationRequirementForDelta(&before, after, catalog),
            ActivationRequirement::Reload
        );
        QCOMPARE(
            activationRequirementForDelta(&after, before, catalog),
            ActivationRequirement::Reload
        );

        after.curves = {
            bezierCurve(
                QStringLiteral("new-bezier"), QStringLiteral("duplicate")
            ),
            springCurve(
                QStringLiteral("new-spring"), QStringLiteral("duplicate")
            ),
        };
        QCOMPARE(
            activationRequirementForDelta(&before, after, catalog),
            ActivationRequirement::Restart
        );
        QCOMPARE(
            activationRequirementForDelta(&after, before, catalog),
            ActivationRequirement::Restart
        );

        before.curves = {
            bezierCurve(QStringLiteral("a"), QStringLiteral("ease")),
            springCurve(QStringLiteral("b"), QStringLiteral("bounce")),
        };
        after.curves = {
            springCurve(
                QStringLiteral("new-b"), QStringLiteral("bounce"), 275.5
            ),
            bezierCurve(
                QStringLiteral("new-a"), QStringLiteral("ease"), 0.05
            ),
        };
        QCOMPARE(
            activationRequirementForDelta(&before, after, catalog),
            ActivationRequirement::Reload
        );
        QCOMPARE(
            activationRequirementForDelta(&after, before, catalog),
            ActivationRequirement::Reload
        );
    }
};

QTEST_GUILESS_MAIN(CompositorActivationRequirementTest)

#include "compositor_activation_requirement_test.moc"
