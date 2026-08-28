pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Page {
    id: root

    property bool serviceAvailable: false
    property string shellAppearanceMode: "dark"
    property string shellEffectiveAppearanceMode: "dark"
    property bool shellAppearanceServiceAvailable: false
    property bool shellAppearanceBusy: false
    property string shellAppearanceError: ""
    property string shellAppearanceAutomationError: ""
    property string nightLightSettingsError: ""
    property string shellAppearanceAutomationSource: "desktop"
    property string shellAppearanceScheduleMode: "time"
    property int shellAppearanceDarkStartMinute: 18 * 60
    property int shellAppearanceLightStartMinute: 6 * 60
    property string shellAppearanceLocationSource: "manual"
    property bool shellAppearanceHasLocation: false
    property real shellAppearanceLatitude: 0
    property real shellAppearanceLongitude: 0
    property string shellAppearanceNextTransition: ""
    property string shellAppearanceSunrise: ""
    property string shellAppearanceSunset: ""
    property string shellAppearanceAutomationStatus: "desktop"
    property bool nightLightEnabled: false
    property bool nightLightAutomatic: true
    property string nightLightScheduleMode: "time"
    property int nightLightDarkStartMinute: 20 * 60
    property int nightLightLightStartMinute: 6 * 60
    property string nightLightLocationSource: "manual"
    property bool nightLightHasLocation: false
    property real nightLightLatitude: 0
    property real nightLightLongitude: 0
    property int nightLightTemperature: 4000
    property int nightLightDayTemperature: 6500
    property bool nightLightGradual: true
    property bool hyprsunsetAvailable: false
    property int nightLightCurrentTemperature: 0
    property string nightLightNextTransition: ""
    property string nightLightSunrise: ""
    property string nightLightSunset: ""
    property string nightLightStatus: "disabled"
    property bool writable: false
    property bool catalogAvailable: false
    property bool appearanceAvailable: false
    property bool appearanceProjectionAvailable: false
    property bool appearanceAnimationProjectionAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property var appearanceOptions: []
    property var appearanceValues: ({})
    property var appearanceCurves: []
    property var appearanceAnimations: []
    property bool sharedBorderAvailable: false
    property bool sharedBorderBusy: false
    property bool windowBorderSynced: true
    property string sharedBorderSyncState: "unavailable"
    property string sharedBorderSyncError: ""
    property string sharedBorderClientError: ""
    property string sharedBorderConfigRevisionToken: ""
    property string sharedBorderVerifiedRevisionToken: ""
    property bool sharedSpacingAvailable: false
    property bool sharedSpacingBusy: false
    property bool windowSpacingSynced: true
    property string sharedSpacingSyncState: "unavailable"
    property string sharedSpacingSyncError: ""
    property string sharedSpacingClientError: ""
    property string sharedSpacingConfigRevisionToken: ""
    property string sharedSpacingVerifiedRevisionToken: ""
    property string revisionToken: "0"
    property double appliedRevision: 0
    property string loadState: "unavailable"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property string appearanceErrorName: ""
    property string appearanceErrorMessage: ""
    property string errorName: ""
    property string errorMessage: ""
    property bool retryApplyAvailable: false
    property bool recoveryAvailable: false
    property real contentTopMargin: 28

    property var draftValues: ({})
    property var synchronizedValues: ({})
    property var submittedValues: ({})
    property var draftCurves: []
    property var synchronizedCurves: []
    property var submittedCurves: []
    property var draftAnimations: []
    property var synchronizedAnimations: []
    property var submittedAnimations: []
    property string synchronizedRevisionToken: ""
    property string submittedRevisionToken: ""
    property bool projectionInitialized: false
    property bool externalChangeWhileEditing: false
    property bool saveSubmitted: false
    property int appearanceTabIndex: 0
    property string editingCurveId: ""
    property string editingAnimationId: ""
    property bool synchronizedWindowBorderSynced: windowBorderSynced
    property bool sharedBorderProjectionPending: false
    property string sharedBorderSourceActionError: ""
    property bool sharedBorderSourceRequestPending: false
    property bool sharedBorderSourceRequestSawBusy: false
    property bool sharedBorderSourceRequestErrorCleared: false
    property bool sharedBorderSourceExpectedSync: windowBorderSynced
    property bool synchronizedWindowSpacingSynced: windowSpacingSynced
    property bool sharedSpacingProjectionPending: false
    property string sharedSpacingSourceActionError: ""
    property bool sharedSpacingSourceRequestPending: false
    property bool sharedSpacingSourceRequestSawBusy: false
    property bool sharedSpacingSourceRequestErrorCleared: false
    property bool sharedSpacingSourceExpectedSync: windowSpacingSynced

    signal refreshRequested()
    signal shellAppearanceModeRequested(string mode)
    signal shellAppearanceAutomationRequested(
        string source,
        string scheduleMode,
        int darkStartMinute,
        int lightStartMinute,
        string locationSource,
        bool hasLocation,
        real latitude,
        real longitude
    )
    signal nightLightSettingsRequested(
        bool nightLightEnabled,
        bool automatic,
        string scheduleMode,
        int darkStartMinute,
        int lightStartMinute,
        string locationSource,
        bool hasLocation,
        real latitude,
        real longitude,
        int nightTemperature,
        int dayTemperature,
        bool gradual
    )
    signal openDisplaysRequested()
    signal saveRequested(var values, var curves, var animations)
    signal retryApplyRequested()
    signal recoveryRequested()
    signal windowBorderSyncRequested(bool sync)
    signal retrySharedBorderSyncRequested()
    signal windowSpacingSyncRequested(bool sync)
    signal retrySharedSpacingSyncRequested()

    readonly property string borderSizeId: "hyprland.general.border_size"
    readonly property string roundingId: "hyprland.decoration.rounding"
    readonly property string roundingPowerId:
        "hyprland.decoration.rounding_power"
    readonly property string blurId: "hyprland.decoration.blur.enabled"
    readonly property string shadowId: "hyprland.decoration.shadow.enabled"
    readonly property string shadowRangeId:
        "hyprland.decoration.shadow.range"
    readonly property string shadowRenderPowerId:
        "hyprland.decoration.shadow.render_power"
    readonly property string shadowSharpId:
        "hyprland.decoration.shadow.sharp"
    readonly property string shadowOffsetId:
        "hyprland.decoration.shadow.offset"
    readonly property string shadowScaleId:
        "hyprland.decoration.shadow.scale"
    readonly property string glowEnabledId:
        "hyprland.decoration.glow.enabled"
    readonly property string glowRangeId:
        "hyprland.decoration.glow.range"
    readonly property string glowRenderPowerId:
        "hyprland.decoration.glow.render_power"
    readonly property string borderPartOfWindowId:
        "hyprland.decoration.border_part_of_window"
    readonly property string animationsId: "hyprland.animations.enabled"
    readonly property string dimInactiveId:
        "hyprland.decoration.dim_inactive"
    readonly property string dimStrengthId:
        "hyprland.decoration.dim_strength"
    readonly property string activeOpacityId:
        "hyprland.decoration.active_opacity"
    readonly property string inactiveOpacityId:
        "hyprland.decoration.inactive_opacity"
    readonly property string fullscreenOpacityId:
        "hyprland.decoration.fullscreen_opacity"
    readonly property string dimModalId:
        "hyprland.decoration.dim_modal"
    readonly property string dimSpecialId:
        "hyprland.decoration.dim_special"
    readonly property string dimAroundId:
        "hyprland.decoration.dim_around"
    readonly property string blurSizeId:
        "hyprland.decoration.blur.size"
    readonly property string blurPassesId:
        "hyprland.decoration.blur.passes"
    readonly property string blurIgnoreOpacityId:
        "hyprland.decoration.blur.ignore_opacity"
    readonly property string blurOptimizationsId:
        "hyprland.decoration.blur.new_optimizations"
    readonly property string blurXrayId:
        "hyprland.decoration.blur.xray"
    readonly property string blurSpecialId:
        "hyprland.decoration.blur.special"
    readonly property string blurPopupsId:
        "hyprland.decoration.blur.popups"
    readonly property string blurPopupsIgnoreAlphaId:
        "hyprland.decoration.blur.popups_ignorealpha"
    readonly property string blurInputMethodsId:
        "hyprland.decoration.blur.input_methods"
    readonly property string blurInputMethodsIgnoreAlphaId:
        "hyprland.decoration.blur.input_methods_ignorealpha"
    readonly property string blurBrightnessId:
        "hyprland.decoration.blur.brightness"
    readonly property string blurContrastId:
        "hyprland.decoration.blur.contrast"
    readonly property string blurNoiseId:
        "hyprland.decoration.blur.noise"
    readonly property string blurVibrancyId:
        "hyprland.decoration.blur.vibrancy"
    readonly property string blurVibrancyDarknessId:
        "hyprland.decoration.blur.vibrancy_darkness"
    readonly property string gapsInId: "hyprland.general.gaps_in"
    readonly property string gapsOutId: "hyprland.general.gaps_out"
    readonly property real minimumTargetSize: 44
    readonly property int maximumSharedSourceErrorLength: 1024
    readonly property int maximumSharedBorderSourceErrorLength:
        maximumSharedSourceErrorLength
    readonly property bool compactPreview:
        root.width < 560 || root.height < 640
    readonly property var expectedOptionIds: [
        root.borderSizeId,
        root.roundingId,
        root.gapsInId,
        root.gapsOutId,
        root.blurId,
        root.shadowId,
        root.animationsId,
        root.dimInactiveId,
        root.dimStrengthId,
        root.activeOpacityId,
        root.inactiveOpacityId,
        root.fullscreenOpacityId,
        root.dimModalId,
        root.dimSpecialId,
        root.dimAroundId,
        root.blurSizeId,
        root.blurPassesId,
        root.blurIgnoreOpacityId,
        root.blurOptimizationsId,
        root.blurXrayId,
        root.blurSpecialId,
        root.blurPopupsId,
        root.blurPopupsIgnoreAlphaId,
        root.blurInputMethodsId,
        root.blurInputMethodsIgnoreAlphaId,
        root.blurBrightnessId,
        root.blurContrastId,
        root.blurNoiseId,
        root.blurVibrancyId,
        root.blurVibrancyDarknessId,
        root.borderPartOfWindowId,
        root.roundingPowerId,
        root.shadowRangeId,
        root.shadowRenderPowerId,
        root.shadowSharpId,
        root.shadowOffsetId,
        root.shadowScaleId,
        root.glowEnabledId,
        root.glowRangeId,
        root.glowRenderPowerId
    ]
    readonly property var exactDecimalOptionIds: [
        root.blurBrightnessId,
        root.blurContrastId,
        root.blurNoiseId,
        root.blurVibrancyId,
        root.blurVibrancyDarknessId,
        root.roundingPowerId,
        root.shadowScaleId
    ]
    readonly property var exactVectorOptionIds: [
        root.shadowOffsetId
    ]
    readonly property var animationLeaves: [
        "global", "windows", "layers", "fade", "border", "borderangle",
        "shadowangle", "glowangle", "workspaces", "zoomFactor",
        "monitorAdded", "layersIn", "layersOut", "windowsIn",
        "windowsOut", "windowsMove", "fadeIn", "fadeOut", "fadeSwitch",
        "fadeShadow", "fadeGlow", "fadeDim", "fadeLayers",
        "fadeLayersIn", "fadeLayersOut", "fadePopups", "fadePopupsIn",
        "fadePopupsOut", "fadeDpms", "workspacesIn", "workspacesOut",
        "specialWorkspace", "specialWorkspaceIn", "specialWorkspaceOut"
    ]
    readonly property bool trustedDefinitionsValid: root.validateOptions()
    readonly property bool trustedValuesValid:
        root.appearanceProjectionAvailable
        && root.trustedDefinitionsValid
        && root.validateValues(root.appearanceValues)
    readonly property bool authoritativeGlowSafe:
        root.trustedValuesValid
        && root.glowCombinationSafe(root.appearanceValues)
    readonly property bool trustedAnimationProjectionValid:
        root.appearanceAnimationProjectionAvailable
        && root.validateCurveCollection(root.appearanceCurves, false)
        && root.validateAnimationCollection(
            root.appearanceAnimations, root.appearanceCurves
        )
    readonly property bool revisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.revisionToken)
    readonly property bool synchronizedRevisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.synchronizedRevisionToken)
    readonly property bool draftValuesValid:
        root.trustedDefinitionsValid && root.validateValues(root.draftValues)
    readonly property bool draftAnimationCollectionsValid:
        root.validateCurveCollection(root.draftCurves, false)
        && root.validateAnimationCollection(
            root.draftAnimations, root.draftCurves
        )
    readonly property bool glowDraftSafe:
        root.glowCombinationSafe(root.draftValues)
    readonly property bool glowSafetyViolation:
        root.draftValuesValid && !root.glowDraftSafe
    readonly property bool draftValid:
        root.draftValuesValid && root.glowDraftSafe
        && root.draftAnimationCollectionsValid
    readonly property bool draftDirty:
        root.projectionInitialized
        && (!root.valuesEqual(root.draftValues, root.synchronizedValues)
            || !root.valueEqual(root.draftCurves, root.synchronizedCurves)
            || !root.valueEqual(
                root.draftAnimations, root.synchronizedAnimations
            ))
    readonly property bool displayTestActive:
        root.confirmationState !== "idle"
        || root.managementState === "preview"
    readonly property bool sharedBorderTransitionBusy:
        root.sharedBorderBusy
        || root.sharedBorderSyncState === "pending"
        || root.sharedBorderProjectionPending
        || root.sharedBorderSourceRequestPending
    readonly property bool sharedBorderRevisionVerified:
        /^(0|[1-9][0-9]*)$/.test(root.sharedBorderConfigRevisionToken)
        && /^(0|[1-9][0-9]*)$/.test(
            root.sharedBorderVerifiedRevisionToken
        )
        && root.sharedBorderConfigRevisionToken
            === root.sharedBorderVerifiedRevisionToken
    readonly property bool sharedBorderProjectionVerified:
        root.sharedBorderRevisionVerified
        && ((!root.windowBorderSynced
                && root.sharedBorderSyncState === "override")
            || (root.windowBorderSynced
                && (root.sharedBorderSyncState === "saved"
                    || root.sharedBorderSyncState === "current"
                    || root.sharedBorderSyncState === "failed"
                    || (root.sharedBorderSyncState === "unavailable"
                        && root.sharedBorderConfigRevisionToken !== "0"))))
    readonly property bool sharedBorderApplyStateSettled:
        (!root.windowBorderSynced
            && root.sharedBorderSyncState === "override")
        || (root.windowBorderSynced
            && (root.sharedBorderSyncState === "saved"
                || root.sharedBorderSyncState === "current"))
    readonly property bool sharedBorderApplyVerified:
        root.sharedBorderRevisionVerified
        && root.sharedBorderApplyStateSettled
    readonly property bool sharedBorderApplySafe:
        root.sharedBorderAvailable
        && !root.sharedBorderTransitionBusy
        && root.sharedBorderApplyVerified
    readonly property bool sharedSpacingTransitionBusy:
        root.sharedSpacingBusy
        || root.sharedSpacingSyncState === "pending"
        || root.sharedSpacingProjectionPending
        || root.sharedSpacingSourceRequestPending
    readonly property bool sharedSpacingRevisionVerified:
        /^(0|[1-9][0-9]*)$/.test(root.sharedSpacingConfigRevisionToken)
        && /^(0|[1-9][0-9]*)$/.test(
            root.sharedSpacingVerifiedRevisionToken
        )
        && root.sharedSpacingConfigRevisionToken
            === root.sharedSpacingVerifiedRevisionToken
    readonly property bool sharedSpacingProjectionVerified:
        root.sharedSpacingRevisionVerified
        && (root.sharedSpacingSyncState === "saved"
            || (!root.windowSpacingSynced
                && root.sharedSpacingSyncState === "override")
            || (root.windowSpacingSynced
                && (root.sharedSpacingSyncState === "current"
                    || root.sharedSpacingSyncState === "failed"
                    || (root.sharedSpacingSyncState === "unavailable"
                        && root.sharedSpacingConfigRevisionToken !== "0"))))
    readonly property bool sharedSpacingApplyStateSettled:
        root.sharedSpacingSyncState === "saved"
        || (!root.windowSpacingSynced
            && root.sharedSpacingSyncState === "override")
        || (root.windowSpacingSynced
            && root.sharedSpacingSyncState === "current")
    readonly property bool sharedSpacingApplyVerified:
        root.sharedSpacingRevisionVerified
        && root.sharedSpacingApplyStateSettled
    readonly property bool sharedSpacingApplySafe:
        root.sharedSpacingAvailable
        && !root.sharedSpacingTransitionBusy
        && root.sharedSpacingApplyVerified
    readonly property bool sharedVisualTransitionBusy:
        root.sharedBorderTransitionBusy
        || root.sharedSpacingTransitionBusy
    readonly property bool sharedVisualApplySafe:
        root.serviceAvailable
        && root.sharedBorderApplySafe
        && root.sharedSpacingApplySafe
    readonly property bool controlsEnabled:
        root.serviceAvailable
        && root.writable
        && root.catalogAvailable
        && root.appearanceAvailable
        && root.revisionTokenValid
        && root.trustedDefinitionsValid
        && root.trustedValuesValid
        && root.trustedAnimationProjectionValid
        && !root.busy
        && !root.saveSubmitted
        && !root.sharedVisualTransitionBusy
        && !root.externalChangeWhileEditing
        && !root.displayTestActive
    readonly property bool saveEnabled:
        root.controlsEnabled && root.draftDirty && root.draftValid
        && !root.saveSubmitted && root.sharedVisualApplySafe
    readonly property bool animationControlsEnabled:
        root.controlsEnabled && root.draftValue(root.animationsId) === true
    readonly property bool shadowOffsetControlsEnabled:
        root.controlsEnabled && root.draftValue(root.shadowId) === true
    readonly property bool shadowScaleControlEnabled:
        root.controlsEnabled && root.draftValue(root.shadowId) === true
    readonly property bool glowEnabledControlEnabled:
        root.controlsEnabled
        && (root.draftValue(root.glowEnabledId) === true
            || Number(root.draftValue(root.glowRangeId)) >= 10)
    readonly property bool glowFalloffControlEnabled:
        root.controlsEnabled
        && root.draftValue(root.glowEnabledId) === true
    readonly property bool blurDetailsEnabled:
        root.controlsEnabled && root.draftValue(root.blurId) === true
    readonly property bool blurXrayEnabled:
        root.blurDetailsEnabled
        && root.draftValue(root.blurOptimizationsId) === true
    readonly property bool blurPopupThresholdEnabled:
        root.blurDetailsEnabled
        && root.draftValue(root.blurPopupsId) === true
    readonly property bool blurInputMethodThresholdEnabled:
        root.blurDetailsEnabled
        && root.draftValue(root.blurInputMethodsId) === true
    readonly property var editingCurve: root.curveById(root.editingCurveId)
    readonly property var editingAnimation:
        root.animationById(root.editingAnimationId)
    readonly property bool animationDetailActive:
        root.editingCurve !== null || root.editingAnimation !== null
    readonly property bool resetTargetDiffers: {
        const target = root.resetTargetValues();
        return target !== null
            && (!root.valuesEqual(root.draftValues, target)
                || !root.valueEqual(
                    root.draftCurves, root.synchronizedCurves
                )
                || root.draftAnimations.length > 0);
    }
    readonly property bool sharedBorderSourceActionEnabled:
        root.sharedBorderAvailable && !root.sharedBorderBusy
        && root.sharedBorderSyncState !== "pending"
        && !root.sharedBorderSourceRequestPending
        && !root.busy && !root.displayTestActive
        && !root.draftDirty && !root.externalChangeWhileEditing
        && !root.saveSubmitted
        && !root.sharedSpacingTransitionBusy
    readonly property bool sharedBorderRetryAvailable:
        root.windowBorderSynced
        && root.sharedBorderAvailable
        && root.serviceAvailable
        && (root.sharedBorderSyncState === "failed"
            || root.sharedBorderSyncState === "unavailable")
    readonly property bool sharedSpacingSourceActionEnabled:
        root.sharedSpacingAvailable && !root.sharedSpacingBusy
        && root.sharedSpacingSyncState !== "pending"
        && !root.sharedSpacingSourceRequestPending
        && !root.busy && !root.displayTestActive
        && !root.draftDirty && !root.externalChangeWhileEditing
        && !root.saveSubmitted
        && !root.sharedBorderTransitionBusy
    readonly property bool sharedSpacingRetryAvailable:
        root.windowSpacingSynced
        && root.sharedSpacingAvailable
        && root.serviceAvailable
        && (root.sharedSpacingSyncState === "failed"
            || root.sharedSpacingSyncState === "unavailable")
    readonly property string windowBorderAuthorityMessage: {
        if (!root.windowBorderSynced)
            return qsTr("Window borders use an explicit Hyprland override. Sync them to make HyprShelld's shared border shape authoritative again.");
        if (!root.sharedBorderAvailable)
            return qsTr("Controlled by HyprShelld. The shared border service is unavailable, so these resolved values remain read-only.");
        if (root.sharedBorderSyncState === "failed")
            return qsTr("Controlled by HyprShelld, but the current shared border could not be applied to Hyprland. %1").arg(root.sharedBorderSyncError);
        if (root.sharedBorderSyncState === "pending")
            return qsTr("Controlled by HyprShelld. The shared border is waiting for the current compositor operation to finish.");
        if (root.sharedBorderSyncState === "saved")
            return qsTr("Controlled by HyprShelld. The matching window border is saved and will become active after explicit compositor takeover or apply recovery.");
        if (root.sharedBorderSyncState === "unavailable")
            return qsTr("Controlled by HyprShelld. Synchronization is temporarily unavailable, so the last applied window border is preserved.");
        return qsTr("Controlled by HyprShelld. Window border thickness and corner radius follow the shared border configured on the Bar page.");
    }
    readonly property string windowSpacingAuthorityMessage: {
        if (!root.windowSpacingSynced
                && root.sharedSpacingSyncState === "saved") {
            return qsTr("Window gaps use an explicit Hyprland override. The protected maximize rule is saved but not active yet; apply the exact pending compositor revision before relying on gapless maximized windows.");
        }
        if (!root.windowSpacingSynced)
            return qsTr("Window gaps use an explicit Hyprland override. Sync them to make HyprShelld's shared spacing authoritative again.");
        if (!root.sharedSpacingAvailable)
            return qsTr("Controlled by HyprShelld. The shared spacing service is unavailable, so these resolved values remain read-only.");
        if (root.sharedSpacingSyncState === "failed")
            return qsTr("Controlled by HyprShelld, but the current shared spacing could not be applied to Hyprland. %1").arg(root.sharedSpacingSyncError);
        if (root.sharedSpacingSyncState === "pending")
            return qsTr("Controlled by HyprShelld. Shared spacing is waiting for the current compositor operation to finish.");
        if (root.sharedSpacingSyncState === "saved")
            return qsTr("Controlled by HyprShelld. Matching window gaps are saved and will become active after explicit compositor takeover or apply recovery.");
        if (root.sharedSpacingSyncState === "unavailable")
            return qsTr("Controlled by HyprShelld. Synchronization is temporarily unavailable, so the last applied window gaps are preserved.");
        return qsTr("Controlled by HyprShelld. Normal window gaps follow the inner and outer spacing configured on the Bar page.");
    }
    readonly property bool statusVisible:
        !root.serviceAvailable
        || !root.writable
        || !root.catalogAvailable
        || !root.appearanceAvailable
        || !root.revisionTokenValid
        || !root.trustedDefinitionsValid
        || !root.trustedValuesValid
        || !root.appearanceAnimationProjectionAvailable
        || !root.trustedAnimationProjectionValid
        || root.loadState === "recovered"
        || root.loadState === "defaulted"
        || root.loadState === "unsupported"
        || root.managementState !== "managed"
        || root.applyState !== "current"
        || root.requiredActivation !== "none"
        || root.displayTestActive
        || root.externalChangeWhileEditing
        || root.appearanceErrorMessage.length > 0
        || root.errorMessage.length > 0
        || root.busy
    readonly property bool statusIsDanger:
        root.managementState === "conflict"
        || root.applyState === "failed"
        || root.loadState === "unsupported"
        || (root.catalogAvailable && !root.trustedDefinitionsValid)
        || (root.appearanceProjectionAvailable
            && !root.appearanceAvailable
            && root.appearanceErrorMessage.length > 0)
        || (root.appearanceAnimationProjectionAvailable
            && !root.trustedAnimationProjectionValid)
    readonly property string statusMessage: {
        const detail = root.errorMessage.length > 0
            ? " " + root.errorMessage : "";
        const appearanceDetail = root.appearanceErrorMessage.length > 0
            ? " " + root.appearanceErrorMessage : "";
        if (!root.serviceAvailable)
            return qsTr("Window visuals and animations are unavailable. The compositor settings service may be restarting.%1").arg(detail);
        if (root.displayTestActive)
            return qsTr("A display test is active. Window visual and animation changes stay locked until that test is kept or reverted.");
        if (root.managementState === "unmanaged")
            return qsTr("HyprShelld is not managing the Hyprland entrypoint yet. Review takeover from Displays before changing window visuals or animations.");
        if (root.managementState === "conflict")
            return qsTr("The managed compositor entrypoint or its ownership state changed unexpectedly. Window visual and animation changes are locked to preserve it.%1").arg(detail);
        if (!root.writable)
            return qsTr("This compositor configuration is read-only and has been preserved.");
        if (!root.catalogAvailable)
            return qsTr("The trusted Hyprland option catalog is unavailable or does not match the compositor authority. Window visual and animation changes are disabled.%1").arg(appearanceDetail);
        if (!root.trustedDefinitionsValid || !root.trustedValuesValid)
            return qsTr("The trusted compositor appearance contract does not match this Settings build. No compositor values will be written.%1").arg(appearanceDetail);
        if (!root.appearanceAnimationProjectionAvailable)
            return qsTr("Curves and animations are waiting for a current, authenticated full compositor projection. Existing visual values remain readable, but the combined compositor appearance draft cannot be changed yet.%1").arg(appearanceDetail);
        if (!root.trustedAnimationProjectionValid)
            return qsTr("The current curves and animations do not match the strict managed compositor appearance contract. No compositor values will be written.%1").arg(appearanceDetail);
        if (!root.revisionTokenValid)
            return qsTr("The exact compositor revision token is unavailable. Window visual and animation changes are disabled to prevent overwriting another revision.");
        if (root.externalChangeWhileEditing)
            return qsTr("Compositor settings changed outside this draft. Your draft is preserved, but it cannot be saved over the newer revision. Load the current settings to continue.");
        if (root.busy) {
            if (root.busyOperation === "appearance-save")
                return qsTr("Saving the validated compositor appearance draft…");
            if (root.busyOperation === "compositor-apply"
                    || root.busyOperation === "appearance-apply")
                return qsTr("Applying and verifying the saved compositor revision…");
            if (root.busyOperation === "recover")
                return qsTr("Restoring and verifying the last working compositor configuration…");
            return qsTr("Another compositor operation is in progress. Window visual and animation changes are temporarily locked.");
        }
        if (root.applyState === "retained"
                || root.applyState === "failed"
                || root.applyState === "inactive") {
            if (root.requiredActivation === "reload") {
                return root.retryApplyAvailable
                    ? qsTr("The desired compositor settings were saved, but they are not active. Retry the exact saved revision or restore the last working compositor configuration.%1").arg(detail)
                    : qsTr("The desired compositor settings are saved but not active. Wait for the compositor service to make retry or recovery available.%1").arg(detail);
            }
            if (root.requiredActivation === "restart")
                return qsTr("The saved desired state requires a verified compositor-restart workflow that HyprShelld does not have yet, so this revision cannot be activated from Settings. Restore the last working compositor configuration to continue.%1").arg(detail);
            if (root.requiredActivation === "session")
                return qsTr("The saved desired state requires a verified new-session workflow that HyprShelld does not have yet, so this revision cannot be activated from Settings. Restore the last working compositor configuration to continue.%1").arg(detail);
            return qsTr("The desired compositor state is not the active state. Review recovery options before making another change.%1").arg(detail);
        }
        if (root.loadState === "recovered")
            return qsTr("Compositor settings were restored from the last known good desired-state copy. Review the values before changing them.");
        if (root.loadState === "defaulted")
            return qsTr("Compositor settings could not be recovered, so safe desired-state defaults are in use. Review them before continuing.");
        if (root.loadState === "unsupported")
            return qsTr("The compositor settings use a newer format. They were preserved and remain read-only.");
        if (root.appearanceProjectionAvailable
                && !root.appearanceAvailable
                && root.appearanceErrorMessage.length > 0) {
            return qsTr("Compositor appearance authority verification failed. Current visual values remain readable, but changes are disabled until the managed action, schema, and full-state contract is authenticated.%1").arg(appearanceDetail);
        }
        if (root.appearanceErrorMessage.length > 0)
            return qsTr("The compositor appearance operation failed.%1").arg(appearanceDetail);
        if (root.errorMessage.length > 0)
            return qsTr("The compositor operation failed.%1").arg(detail);
        if (!root.appearanceAvailable)
            return qsTr("Window visuals and animations are waiting for a current, verified compositor baseline.");
        return "";
    }

    function clone(value) {
        try {
            return JSON.parse(JSON.stringify(value));
        } catch (error) {
            return null;
        }
    }

    function valuesEqual(left, right) {
        if (!left || !right || typeof left !== "object"
                || typeof right !== "object"
                || Array.isArray(left) || Array.isArray(right)) {
            return false;
        }
        for (const id of root.expectedOptionIds) {
            if (!root.valueEqual(left[id], right[id]))
                return false;
        }
        return Object.keys(left).length === root.expectedOptionIds.length
            && Object.keys(right).length === root.expectedOptionIds.length;
    }

    function valueEqual(left, right) {
        if (Array.isArray(left) || Array.isArray(right)) {
            if (!Array.isArray(left) || !Array.isArray(right)
                    || left.length !== right.length) {
                return false;
            }
            for (let index = 0; index < left.length; ++index) {
                if (!root.valueEqual(left[index], right[index]))
                    return false;
            }
            return true;
        }
        if ((left && typeof left === "object")
                || (right && typeof right === "object")) {
            if (!left || !right || typeof left !== "object"
                    || typeof right !== "object"
                    || Array.isArray(left) || Array.isArray(right)) {
                return false;
            }
            const leftKeys = Object.keys(left).sort();
            const rightKeys = Object.keys(right).sort();
            if (!root.valueEqual(leftKeys, rightKeys))
                return false;
            for (const key of leftKeys) {
                if (!root.valueEqual(left[key], right[key]))
                    return false;
            }
            return true;
        }
        return left === right;
    }

    function isUnicodeFormatCharacter(codePoint) {
        return codePoint === 0x00AD
            || (codePoint >= 0x0600 && codePoint <= 0x0605)
            || codePoint === 0x061C || codePoint === 0x06DD
            || codePoint === 0x070F
            || (codePoint >= 0x0890 && codePoint <= 0x0891)
            || codePoint === 0x08E2 || codePoint === 0x180E
            || (codePoint >= 0x200B && codePoint <= 0x200F)
            || (codePoint >= 0x202A && codePoint <= 0x202E)
            || (codePoint >= 0x2060 && codePoint <= 0x2064)
            || (codePoint >= 0x2066 && codePoint <= 0x206F)
            || codePoint === 0xFEFF
            || (codePoint >= 0xFFF9 && codePoint <= 0xFFFB)
            || codePoint === 0x110BD || codePoint === 0x110CD
            || (codePoint >= 0x13430 && codePoint <= 0x1343F)
            || (codePoint >= 0x1BCA0 && codePoint <= 0x1BCA3)
            || (codePoint >= 0x1D173 && codePoint <= 0x1D17A)
            || codePoint === 0xE0001
            || (codePoint >= 0xE0020 && codePoint <= 0xE007F);
    }

    function hasDisallowedCharacter(value) {
        if (typeof value !== "string")
            return true;
        for (let index = 0; index < value.length;) {
            const codePoint = value.codePointAt(index);
            index += codePoint > 0xFFFF ? 2 : 1;
            if (codePoint <= 0x1F
                    || (codePoint >= 0x7F && codePoint <= 0x9F)
                    || root.isUnicodeFormatCharacter(codePoint)) {
                return true;
            }
        }
        return false;
    }

    function isSchemaString(value, maximumLength, allowEmpty) {
        return typeof value === "string"
            && value.length <= maximumLength
            && (allowEmpty || value.length > 0)
            && value === value.normalize("NFC")
            && !root.hasDisallowedCharacter(value);
    }

    function isStableRecordId(value) {
        return typeof value === "string" && value.length >= 1
            && value.length <= 128
            && /^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/.test(value);
    }

    function isFiniteRange(value, minimum, maximum) {
        return typeof value === "number" && Number.isFinite(value)
            && value >= minimum && value <= maximum;
    }

    function isValidSpringValue(value) {
        return typeof value === "number" && Number.isFinite(value)
            && value > 0.5 && value <= 1000000;
    }

    function validateCurveRecord(record, allowIncomplete) {
        if (!record || typeof record !== "object" || Array.isArray(record)
                || !root.isStableRecordId(record.id)
                || !root.isSchemaString(record.name, 256, allowIncomplete)
                || !["bezier", "spring"].includes(record.type)) {
            return false;
        }
        if (record.type === "bezier") {
            if (!root.valueEqual(
                    Object.keys(record).sort(),
                    ["id", "name", "points", "type"].sort()
                ) || !Array.isArray(record.points)
                    || record.points.length !== 2) {
                return false;
            }
            for (const point of record.points) {
                if (!Array.isArray(point) || point.length !== 2)
                    return false;
                for (const coordinate of point) {
                    if (!root.isFiniteRange(coordinate, -1, 2)
                            && !(allowIncomplete
                                && typeof coordinate === "string")) {
                        return false;
                    }
                }
            }
            return true;
        }
        if (!root.valueEqual(
                Object.keys(record).sort(),
                ["dampening", "id", "mass", "name", "stiffness", "type"]
                    .sort()
            )) {
            return false;
        }
        for (const key of ["stiffness", "dampening", "mass"]) {
            if (!root.isValidSpringValue(record[key])
                    && !(allowIncomplete && typeof record[key] === "string")) {
                return false;
            }
        }
        return true;
    }

    function validateCurveCollection(curves, allowIncomplete) {
        if (!Array.isArray(curves) || curves.length > 256)
            return false;
        const ids = new Set();
        const names = new Set();
        for (const curve of curves) {
            if (!root.validateCurveRecord(curve, allowIncomplete)
                    || ids.has(curve.id)) {
                return false;
            }
            if (!allowIncomplete || root.isSchemaString(curve.name, 256, false)) {
                if (names.has(curve.name))
                    return false;
                names.add(curve.name);
            }
            ids.add(curve.id);
        }
        return true;
    }

    function animationStyleValid(leaf, style) {
        if (typeof style !== "string" || style.length > 128)
            return false;
        if (style === "")
            return true;
        if (["windows", "windowsIn", "windowsOut", "windowsMove"]
                .includes(leaf)) {
            return /^(?:slide(?: (?:top|bottom|left|right))?|gnome|gnomed|popin(?: (?:0|[1-9][0-9]?|100)%)?)$/.test(style);
        }
        if (["workspaces", "workspacesIn", "workspacesOut",
                "specialWorkspace", "specialWorkspaceIn",
                "specialWorkspaceOut"].includes(leaf)) {
            return /^(?:fade|(?:slide|slidevert|slidefade|slidefadevert)(?: (?:top|bottom|left|right))?(?: (?:0|[1-9][0-9]?|100)%)?)$/.test(style);
        }
        if (["borderangle", "shadowangle", "glowangle"].includes(leaf))
            return style === "loop" || style === "once";
        if (["layers", "layersIn", "layersOut"].includes(leaf)) {
            return /^(?:fade|slide(?: (?:top|bottom|left|right))?|popin(?: (?:0|[1-9][0-9]?|100)%)?)$/.test(style);
        }
        return false;
    }

    function validateAnimationRecord(record, curves, allowIncomplete) {
        if (!record || typeof record !== "object" || Array.isArray(record)
                || !root.valueEqual(
                    Object.keys(record).sort(),
                    ["curve", "enabled", "id", "name", "speed", "style"]
                        .sort()
                ) || !root.isStableRecordId(record.id)
                || (!root.animationLeaves.includes(record.name)
                    && !(allowIncomplete && record.name === ""))
                || typeof record.enabled !== "boolean"
                || (!root.isFiniteRange(record.speed, Number.MIN_VALUE, 100)
                    && !(allowIncomplete && typeof record.speed === "string"))
                || !root.isSchemaString(record.curve, 256, allowIncomplete)
                || !root.animationStyleValid(record.name, record.style)) {
            return false;
        }
        if (allowIncomplete && record.curve === "")
            return true;
        const available = new Set(["default", "linear"]);
        for (const curve of curves) {
            if (curve && typeof curve.name === "string"
                    && curve.name.length > 0) {
                available.add(curve.name);
            }
        }
        return available.has(record.curve);
    }

    function validateAnimationCollection(animations, curves, allowIncomplete) {
        const incomplete = allowIncomplete === true;
        if (!Array.isArray(animations) || animations.length > 256
                || !Array.isArray(curves)) {
            return false;
        }
        const ids = new Set();
        const leaves = new Set();
        for (const animation of animations) {
            if (!root.validateAnimationRecord(animation, curves, incomplete)
                    || ids.has(animation.id)) {
                return false;
            }
            if (!incomplete || root.animationLeaves.includes(animation.name)) {
                if (leaves.has(animation.name))
                    return false;
                leaves.add(animation.name);
            }
            ids.add(animation.id);
        }
        return true;
    }

    function curveIndex(id) {
        for (let index = 0; index < root.draftCurves.length; ++index) {
            if (root.draftCurves[index]
                    && root.draftCurves[index].id === id) {
                return index;
            }
        }
        return -1;
    }

    function curveById(id) {
        const index = root.curveIndex(id);
        return index >= 0 ? root.draftCurves[index] : null;
    }

    function animationIndex(id) {
        for (let index = 0; index < root.draftAnimations.length; ++index) {
            if (root.draftAnimations[index]
                    && root.draftAnimations[index].id === id) {
                return index;
            }
        }
        return -1;
    }

    function animationById(id) {
        const index = root.animationIndex(id);
        return index >= 0 ? root.draftAnimations[index] : null;
    }

    function curveReferenceCount(name) {
        if (typeof name !== "string" || name.length === 0)
            return 0;
        let count = 0;
        for (const animation of root.draftAnimations) {
            if (animation && animation.curve === name)
                ++count;
        }
        return count;
    }

    function nextRecordIdentity(prefix, records) {
        const ids = new Set(records.map(record => record.id));
        let suffix = 1;
        while (ids.has(prefix + suffix))
            ++suffix;
        return prefix + suffix;
    }

    function replaceCurve(id, record) {
        if (!root.animationControlsEnabled)
            return;
        const curves = root.clone(root.draftCurves);
        const index = root.curveIndex(id);
        if (!curves || index < 0 || !record)
            return;
        curves[index] = record;
        root.draftCurves = curves;
    }

    function setCurveProperty(id, propertyName, value) {
        if (!root.animationControlsEnabled)
            return;
        if (!["stiffness", "dampening", "mass"].includes(propertyName))
            return;
        const record = root.clone(root.curveById(id));
        if (!record)
            return;
        record[propertyName] = value;
        root.replaceCurve(id, record);
    }

    function setCurvePoint(id, pointIndex, coordinateIndex, value) {
        if (!root.animationControlsEnabled)
            return;
        const record = root.clone(root.curveById(id));
        if (!record || record.type !== "bezier"
                || !Array.isArray(record.points)
                || pointIndex < 0 || pointIndex > 1
                || coordinateIndex < 0 || coordinateIndex > 1) {
            return;
        }
        record.points[pointIndex][coordinateIndex] = value;
        root.replaceCurve(id, record);
    }

    function moveCurve(id, offset) {
        if (!root.animationControlsEnabled || (offset !== -1 && offset !== 1))
            return;
        const curves = root.clone(root.draftCurves);
        const index = root.curveIndex(id);
        const target = index + offset;
        if (!curves || index < 0 || target < 0 || target >= curves.length)
            return;
        const record = curves[index];
        curves[index] = curves[target];
        curves[target] = record;
        root.draftCurves = curves;
    }

    function openCurve(id) {
        if (root.curveIndex(id) < 0)
            return;
        root.appearanceTabIndex = 1;
        root.editingAnimationId = "";
        root.editingCurveId = id;
    }

    function replaceAnimation(id, record) {
        if (!root.animationControlsEnabled)
            return;
        const animations = root.clone(root.draftAnimations);
        const index = root.animationIndex(id);
        if (!animations || index < 0 || !record)
            return;
        animations[index] = record;
        root.draftAnimations = animations;
    }

    function setAnimationProperty(id, propertyName, value) {
        if (!root.animationControlsEnabled)
            return;
        const record = root.clone(root.animationById(id));
        if (!record)
            return;
        record[propertyName] = value;
        if (propertyName === "name"
                && !root.animationStyleValid(record.name, record.style)) {
            record.style = "";
        }
        root.replaceAnimation(id, record);
    }

    function firstUnusedAnimationLeaf() {
        const names = new Set(root.draftAnimations.map(item => item.name));
        for (const leaf of root.animationLeaves) {
            if (!names.has(leaf))
                return leaf;
        }
        return "";
    }

    function addAnimation() {
        if (!root.animationControlsEnabled
                || root.draftAnimations.length >= 256) {
            return;
        }
        const leaf = root.firstUnusedAnimationLeaf();
        if (leaf.length === 0)
            return;
        const animations = root.clone(root.draftAnimations);
        const id = root.nextRecordIdentity("animation-", animations);
        animations.push({
            id,
            name: leaf,
            enabled: true,
            speed: 8,
            curve: "default",
            style: ""
        });
        root.draftAnimations = animations;
        root.editingCurveId = "";
        root.editingAnimationId = id;
    }

    function removeAnimation(id) {
        if (!root.animationControlsEnabled)
            return;
        const animations = root.clone(root.draftAnimations);
        const index = root.animationIndex(id);
        if (!animations || index < 0)
            return;
        animations.splice(index, 1);
        root.draftAnimations = animations;
        if (root.editingAnimationId === id)
            root.editingAnimationId = "";
    }

    function moveAnimation(id, offset) {
        if (!root.animationControlsEnabled || (offset !== -1 && offset !== 1))
            return;
        const animations = root.clone(root.draftAnimations);
        const index = root.animationIndex(id);
        const target = index + offset;
        if (!animations || index < 0 || target < 0
                || target >= animations.length) {
            return;
        }
        const record = animations[index];
        animations[index] = animations[target];
        animations[target] = record;
        root.draftAnimations = animations;
    }

    function openAnimation(id) {
        if (root.animationIndex(id) < 0)
            return;
        root.appearanceTabIndex = 1;
        root.editingCurveId = "";
        root.editingAnimationId = id;
    }

    function closeAnimationDetail() {
        root.editingCurveId = "";
        root.editingAnimationId = "";
    }

    function curveChoices() {
        const custom = new Map();
        for (const curve of root.draftCurves) {
            if (curve && typeof curve.name === "string"
                    && curve.name.length > 0 && !custom.has(curve.name)) {
                custom.set(curve.name, curve);
            }
        }
        const result = [];
        for (const builtin of ["default", "linear"]) {
            result.push({
                value: builtin,
                label: custom.has(builtin)
                    ? qsTr("%1 — custom override").arg(builtin)
                    : qsTr("%1 — built in").arg(builtin)
            });
            custom.delete(builtin);
        }
        for (const curve of root.draftCurves) {
            if (curve && custom.has(curve.name)) {
                result.push({
                    value: curve.name,
                    label: qsTr("%1 — custom").arg(curve.name)
                });
                custom.delete(curve.name);
            }
        }
        return result;
    }

    function animationLeafChoices(id) {
        const used = new Set();
        for (const animation of root.draftAnimations) {
            if (animation && animation.id !== id)
                used.add(animation.name);
        }
        return root.animationLeaves.filter(leaf => !used.has(leaf));
    }

    function curveIssue(record) {
        if (!record)
            return "";
        if (!root.isSchemaString(record.name, 256, false))
            return qsTr("Enter a curve name of 1–256 canonical characters without control or format characters.");
        if (root.draftCurves.some(curve => curve.id !== record.id
                && curve.name === record.name)) {
            return qsTr("Each custom curve name must be unique.");
        }
        if (!root.validateCurveRecord(record, false))
            return record.type === "bezier"
                ? qsTr("Finish all four Bezier coordinates from −1 through 2.")
                : qsTr("Finish all three spring values above 0.5 and no greater than 1,000,000.");
        return "";
    }

    function animationIssue(record) {
        if (!record)
            return "";
        if (!root.animationLeaves.includes(record.name))
            return qsTr("Choose one supported animation leaf.");
        if (root.draftAnimations.some(animation => animation.id !== record.id
                && animation.name === record.name)) {
            return qsTr("Each animation leaf can appear only once.");
        }
        if (!root.isFiniteRange(record.speed, Number.MIN_VALUE, 100))
            return qsTr("Enter a speed greater than 0 and no greater than 100.");
        if (!root.validateAnimationRecord(record, root.draftCurves, false))
            return qsTr("Choose an available curve and a style supported by this animation leaf.");
        return "";
    }

    function isWindowBorderOption(id) {
        return id === root.borderSizeId || id === root.roundingId;
    }

    function valuesEqualExceptWindowBorder(left, right) {
        if (!left || !right || typeof left !== "object"
                || typeof right !== "object"
                || Array.isArray(left) || Array.isArray(right)) {
            return false;
        }
        for (const id of root.expectedOptionIds) {
            if (!root.isWindowBorderOption(id)
                    && !root.valueEqual(left[id], right[id]))
                return false;
        }
        return true;
    }

    function isWindowSpacingOption(id) {
        return id === root.gapsInId || id === root.gapsOutId;
    }

    function valuesEqualExceptSharedVisual(left, right) {
        if (!left || !right || typeof left !== "object"
                || typeof right !== "object"
                || Array.isArray(left) || Array.isArray(right)) {
            return false;
        }
        for (const id of root.expectedOptionIds) {
            if (!root.isWindowBorderOption(id)
                    && !root.isWindowSpacingOption(id)
                    && !root.valueEqual(left[id], right[id])) {
                return false;
            }
        }
        return true;
    }

    function optionById(id) {
        if (!Array.isArray(root.appearanceOptions))
            return null;
        for (const option of root.appearanceOptions) {
            if (option && typeof option === "object" && option.id === id)
                return option;
        }
        return null;
    }

    function choiceValues(option) {
        if (!option || !Array.isArray(option.choices))
            return [];
        const values = [];
        for (const choice of option.choices) {
            if (typeof choice === "string")
                values.push(choice);
            else if (choice && typeof choice === "object"
                    && typeof choice.value === "string")
                values.push(choice.value);
            else
                return [];
        }
        return values;
    }

    function validateBooleanOption(option, id, defaultValue) {
        return option && option.id === id
            && option.type === "boolean"
            && option.control === "toggle"
            && option.defaultValue === defaultValue
            && option.min === undefined
            && option.max === undefined
            && (option.choices === undefined
                || (Array.isArray(option.choices)
                    && option.choices.length === 0));
    }

    function validateIntegerOption(
        option, id, defaultValue, minimum, maximum
    ) {
        return option && option.id === id
            && option.type === "integer"
            && option.control === "spinBox"
            && option.defaultValue === defaultValue
            && option.min === minimum
            && option.max === maximum
            && option.step === undefined
            && (option.choices === undefined
                || (Array.isArray(option.choices)
                    && option.choices.length === 0));
    }

    function validateNumberOption(
        option, id, defaultValue, minimum, maximum
    ) {
        return option && option.id === id
            && option.type === "number"
            && option.control === "slider"
            && option.defaultValue === defaultValue
            && option.min === minimum
            && option.max === maximum
            && option.step === undefined
            && (option.choices === undefined
                || (Array.isArray(option.choices)
                    && option.choices.length === 0));
    }

    function validateVector2Option(
        option, id, defaultValue, minimum, maximum
    ) {
        return option && option.id === id
            && option.type === "vector2"
            && option.control === "vector2"
            && root.valueEqual(option.defaultValue, defaultValue)
            && root.valueEqual(option.min, minimum)
            && root.valueEqual(option.max, maximum)
            && option.step === undefined
            && (option.choices === undefined
                || (Array.isArray(option.choices)
                    && option.choices.length === 0));
    }

    function validateCssGapOption(option, id, defaultValue) {
        return option && option.id === id
            && option.type === "cssGap"
            && option.control === "text"
            && root.valueEqual(option.defaultValue, defaultValue)
            && option.min === undefined
            && option.max === undefined
            && option.step === undefined
            && (option.choices === undefined
                || (Array.isArray(option.choices)
                    && option.choices.length === 0));
    }

    function validateOptions() {
        if (!Array.isArray(root.appearanceOptions)
                || root.appearanceOptions.length
                    !== root.expectedOptionIds.length) {
            return false;
        }
        const seen = Object.create(null);
        for (let index = 0;
                index < root.appearanceOptions.length; ++index) {
            const option = root.appearanceOptions[index];
            if (!option || typeof option !== "object"
                    || typeof option.id !== "string" || seen[option.id]
                    || option.id !== root.expectedOptionIds[index]) {
                return false;
            }
            seen[option.id] = true;
        }
        return root.validateIntegerOption(
                root.optionById(root.borderSizeId), root.borderSizeId,
                1, 0, 20)
            && root.validateIntegerOption(
                root.optionById(root.roundingId), root.roundingId,
                0, 0, 20)
            && root.validateCssGapOption(
                root.optionById(root.gapsInId), root.gapsInId,
                [5, 5, 5, 5])
            && root.validateCssGapOption(
                root.optionById(root.gapsOutId), root.gapsOutId,
                [20, 20, 20, 20])
            && root.validateBooleanOption(
                root.optionById(root.blurId), root.blurId, true)
            && root.validateBooleanOption(
                root.optionById(root.shadowId), root.shadowId, true)
            && root.validateBooleanOption(
                root.optionById(root.animationsId), root.animationsId, true)
            && root.validateBooleanOption(
                root.optionById(root.dimInactiveId),
                root.dimInactiveId, false)
            && root.validateNumberOption(
                root.optionById(root.dimStrengthId),
                root.dimStrengthId, 0.5, 0, 1)
            && root.validateNumberOption(
                root.optionById(root.activeOpacityId),
                root.activeOpacityId, 1, 0, 1)
            && root.validateNumberOption(
                root.optionById(root.inactiveOpacityId),
                root.inactiveOpacityId, 1, 0, 1)
            && root.validateNumberOption(
                root.optionById(root.fullscreenOpacityId),
                root.fullscreenOpacityId, 1, 0, 1)
            && root.validateBooleanOption(
                root.optionById(root.dimModalId), root.dimModalId, true)
            && root.validateNumberOption(
                root.optionById(root.dimSpecialId),
                root.dimSpecialId, 0.2, 0, 1)
            && root.validateNumberOption(
                root.optionById(root.dimAroundId),
                root.dimAroundId, 0.4, 0, 1)
            && root.validateIntegerOption(
                root.optionById(root.blurSizeId), root.blurSizeId,
                8, 0, 100)
            && root.validateIntegerOption(
                root.optionById(root.blurPassesId), root.blurPassesId,
                1, 0, 10)
            && root.validateBooleanOption(
                root.optionById(root.blurIgnoreOpacityId),
                root.blurIgnoreOpacityId, true)
            && root.validateBooleanOption(
                root.optionById(root.blurOptimizationsId),
                root.blurOptimizationsId, true)
            && root.validateBooleanOption(
                root.optionById(root.blurXrayId), root.blurXrayId, false)
            && root.validateBooleanOption(
                root.optionById(root.blurSpecialId),
                root.blurSpecialId, false)
            && root.validateBooleanOption(
                root.optionById(root.blurPopupsId),
                root.blurPopupsId, false)
            && root.validateNumberOption(
                root.optionById(root.blurPopupsIgnoreAlphaId),
                root.blurPopupsIgnoreAlphaId, 0.2, 0, 1)
            && root.validateBooleanOption(
                root.optionById(root.blurInputMethodsId),
                root.blurInputMethodsId, false)
            && root.validateNumberOption(
                root.optionById(root.blurInputMethodsIgnoreAlphaId),
                root.blurInputMethodsIgnoreAlphaId, 0.2, 0, 1)
            && root.validateNumberOption(
                root.optionById(root.blurBrightnessId),
                root.blurBrightnessId, 1, 0, 2)
            && root.validateNumberOption(
                root.optionById(root.blurContrastId),
                root.blurContrastId, 0.8916, 0, 2)
            && root.validateNumberOption(
                root.optionById(root.blurNoiseId),
                root.blurNoiseId, 0.0117, 0, 1)
            && root.validateNumberOption(
                root.optionById(root.blurVibrancyId),
                root.blurVibrancyId, 0.1696, 0, 1)
            && root.validateNumberOption(
                root.optionById(root.blurVibrancyDarknessId),
                root.blurVibrancyDarknessId, 0, 0, 1)
            && root.validateBooleanOption(
                root.optionById(root.borderPartOfWindowId),
                root.borderPartOfWindowId, true)
            && root.validateNumberOption(
                root.optionById(root.roundingPowerId),
                root.roundingPowerId, 2, 2, 10)
            && root.validateIntegerOption(
                root.optionById(root.shadowRangeId),
                root.shadowRangeId, 4, 0, 100)
            && root.validateIntegerOption(
                root.optionById(root.shadowRenderPowerId),
                root.shadowRenderPowerId, 3, 1, 4)
            && root.validateBooleanOption(
                root.optionById(root.shadowSharpId),
                root.shadowSharpId, false)
            && root.validateVector2Option(
                root.optionById(root.shadowOffsetId),
                root.shadowOffsetId, [0, 0], [-250, -250], [250, 250])
            && root.validateNumberOption(
                root.optionById(root.shadowScaleId),
                root.shadowScaleId, 1, 0, 1)
            && root.validateBooleanOption(
                root.optionById(root.glowEnabledId),
                root.glowEnabledId, false)
            && root.validateIntegerOption(
                root.optionById(root.glowRangeId),
                root.glowRangeId, 10, 0, 100)
            && root.validateIntegerOption(
                root.optionById(root.glowRenderPowerId),
                root.glowRenderPowerId, 3, 1, 4);
    }

    function validateValues(values) {
        if (!values || typeof values !== "object" || Array.isArray(values)
                || Object.keys(values).length
                    !== root.expectedOptionIds.length) {
            return false;
        }
        for (const id of root.expectedOptionIds) {
            if (!Object.prototype.hasOwnProperty.call(values, id))
                return false;
            const option = root.optionById(id);
            const value = values[id];
            if (!option)
                return false;
            if (option.type === "boolean") {
                if (typeof value !== "boolean")
                    return false;
            } else if (option.type === "integer") {
                if (!Number.isInteger(value)
                        || value < option.min || value > option.max) {
                    return false;
                }
            } else if (option.type === "number") {
                if (typeof value !== "number" || !Number.isFinite(value)
                        || value < option.min || value > option.max) {
                    return false;
                }
            } else if (option.type === "vector2") {
                if (!Array.isArray(value) || value.length !== 2
                        || !Array.isArray(option.min)
                        || option.min.length !== 2
                        || !Array.isArray(option.max)
                        || option.max.length !== 2) {
                    return false;
                }
                for (let index = 0; index < 2; ++index) {
                    if (typeof value[index] !== "number"
                            || !Number.isFinite(value[index])
                            || value[index] < option.min[index]
                            || value[index] > option.max[index]) {
                        return false;
                    }
                }
            } else if (option.type === "cssGap") {
                if (!Array.isArray(value) || value.length !== 4
                        || !value.every(part =>
                            typeof part === "number"
                            && Number.isSafeInteger(part))) {
                    return false;
                }
            } else if (option.type === "enum") {
                if (typeof value !== "string"
                        || !root.choiceValues(option).includes(value)) {
                    return false;
                }
            } else {
                return false;
            }
        }
        return true;
    }

    function glowCombinationSafe(values) {
        if (!values || typeof values !== "object" || Array.isArray(values)
                || !Object.prototype.hasOwnProperty.call(
                    values, root.glowEnabledId
                )
                || !Object.prototype.hasOwnProperty.call(
                    values, root.glowRangeId
                )) {
            return false;
        }
        return values[root.glowEnabledId] !== true
            || (Number.isInteger(values[root.glowRangeId])
                && values[root.glowRangeId] >= 10);
    }

    function optionMinimum(id) {
        const option = root.optionById(id);
        return option && typeof option.min === "number"
                && Number.isFinite(option.min)
            ? option.min : 0;
    }

    function optionMaximum(id) {
        const option = root.optionById(id);
        return option && typeof option.max === "number"
                && Number.isFinite(option.max)
            ? option.max : 0;
    }

    function optionComponentMinimum(id, index) {
        const option = root.optionById(id);
        return option && Array.isArray(option.min)
                && index >= 0 && index < option.min.length
                && typeof option.min[index] === "number"
                && Number.isFinite(option.min[index])
            ? option.min[index] : 0;
    }

    function optionComponentMaximum(id, index) {
        const option = root.optionById(id);
        return option && Array.isArray(option.max)
                && index >= 0 && index < option.max.length
                && typeof option.max[index] === "number"
                && Number.isFinite(option.max[index])
            ? option.max[index] : 0;
    }

    function optionDefault(id) {
        const option = root.optionById(id);
        return option ? option.defaultValue : undefined;
    }

    function draftValue(id) {
        if (root.draftValues
                && Object.prototype.hasOwnProperty.call(root.draftValues, id)) {
            return root.draftValues[id];
        }
        if (root.trustedValuesValid && root.appearanceValues
                && Object.prototype.hasOwnProperty.call(
                    root.appearanceValues, id
                )) {
            return root.appearanceValues[id];
        }
        return root.optionDefault(id);
    }

    function gapComponent(id, index) {
        const value = root.draftValue(id);
        return Array.isArray(value) && index >= 0 && index < value.length
            ? value[index] : 0;
    }

    function vectorDraftComponent(id, index) {
        const value = root.draftValue(id);
        return Array.isArray(value) && value.length === 2
                && index >= 0 && index < 2
            ? value[index] : 0;
    }

    function setGapComponent(id, index, text) {
        if (!root.controlsEnabled || root.windowSpacingSynced
                || !root.isWindowSpacingOption(id)
                || index < 0 || index > 3
                || !/^(0|-?[1-9][0-9]*)$/.test(String(text))) {
            return false;
        }
        const value = Number(text);
        if (!Number.isSafeInteger(value))
            return false;
        const next = root.clone(root.draftValue(id));
        if (!Array.isArray(next) || next.length !== 4)
            return false;
        next[index] = value;
        root.setDraftValue(id, next);
        return root.valueEqual(root.draftValue(id), next);
    }

    function resetTargetValues() {
        if (!root.trustedDefinitionsValid)
            return null;
        const defaults = {};
        for (const id of root.expectedOptionIds) {
            const sharedBorderValue = root.windowBorderSynced
                && root.isWindowBorderOption(id);
            const sharedSpacingValue = root.windowSpacingSynced
                && root.isWindowSpacingOption(id);
            defaults[id] = sharedBorderValue || sharedSpacingValue
                ? root.clone(root.draftValue(id))
                : root.clone(root.optionDefault(id));
        }
        return root.validateValues(defaults) ? defaults : null;
    }

    function finishSharedBorderSourceRequest() {
        root.sharedBorderSourceRequestPending = false;
        root.sharedBorderSourceRequestSawBusy = false;
        root.sharedBorderSourceRequestErrorCleared = false;
    }

    function reviewSharedBorderSourceRequest() {
        if (!root.sharedBorderSourceRequestPending)
            return;
        if (root.windowBorderSynced === root.sharedBorderSourceExpectedSync) {
            root.sharedBorderSourceActionError = "";
            root.finishSharedBorderSourceRequest();
            return;
        }
        if (root.sharedBorderBusy) {
            root.sharedBorderSourceRequestSawBusy = true;
            return;
        }
        if (!root.sharedBorderSourceRequestSawBusy)
            return;
        if (root.sharedBorderSourceRequestErrorCleared
                && root.sharedBorderClientError.length > 0) {
            root.sharedBorderSourceActionError = root.sharedBorderClientError
                .slice(0, root.maximumSharedSourceErrorLength);
            root.finishSharedBorderSourceRequest();
        }
    }

    function scheduleSharedBorderSourceRequestReview() {
        Qt.callLater(root.reviewSharedBorderSourceRequest);
    }

    function requestWindowBorderSync(sync) {
        if (!root.sharedBorderSourceActionEnabled
                || typeof sync !== "boolean"
                || sync === root.windowBorderSynced) {
            return;
        }
        root.sharedBorderSourceActionError = "";
        root.sharedBorderSourceRequestPending = true;
        root.sharedBorderSourceRequestSawBusy = root.sharedBorderBusy;
        root.sharedBorderSourceRequestErrorCleared =
            root.sharedBorderClientError.length === 0;
        root.sharedBorderSourceExpectedSync = sync;
        root.windowBorderSyncRequested(sync);
        root.scheduleSharedBorderSourceRequestReview();
    }

    function finishSharedSpacingSourceRequest() {
        root.sharedSpacingSourceRequestPending = false;
        root.sharedSpacingSourceRequestSawBusy = false;
        root.sharedSpacingSourceRequestErrorCleared = false;
    }

    function reviewSharedSpacingSourceRequest() {
        if (!root.sharedSpacingSourceRequestPending)
            return;
        if (root.windowSpacingSynced
                === root.sharedSpacingSourceExpectedSync) {
            root.sharedSpacingSourceActionError = "";
            root.finishSharedSpacingSourceRequest();
            return;
        }
        if (root.sharedSpacingBusy) {
            root.sharedSpacingSourceRequestSawBusy = true;
            return;
        }
        if (!root.sharedSpacingSourceRequestSawBusy)
            return;
        if (root.sharedSpacingSourceRequestErrorCleared
                && root.sharedSpacingClientError.length > 0) {
            root.sharedSpacingSourceActionError = root.sharedSpacingClientError
                .slice(0, root.maximumSharedSourceErrorLength);
            root.finishSharedSpacingSourceRequest();
        }
    }

    function scheduleSharedSpacingSourceRequestReview() {
        Qt.callLater(root.reviewSharedSpacingSourceRequest);
    }

    function requestWindowSpacingSync(sync) {
        if (!root.sharedSpacingSourceActionEnabled
                || typeof sync !== "boolean"
                || sync === root.windowSpacingSynced) {
            return;
        }
        root.sharedSpacingSourceActionError = "";
        root.sharedSpacingSourceRequestPending = true;
        root.sharedSpacingSourceRequestSawBusy = root.sharedSpacingBusy;
        root.sharedSpacingSourceRequestErrorCleared =
            root.sharedSpacingClientError.length === 0;
        root.sharedSpacingSourceExpectedSync = sync;
        root.windowSpacingSyncRequested(sync);
        root.scheduleSharedSpacingSourceRequestReview();
    }

    function reconcileWindowBorderProjection() {
        if (!root.serviceAvailable
                || !root.projectionInitialized || !root.trustedValuesValid
                || root.sharedBorderBusy
                || root.sharedBorderSyncState === "pending") {
            return false;
        }
        const modeChanged = root.windowBorderSynced
            !== root.synchronizedWindowBorderSynced;
        const pairChanged = root.appearanceValues[root.borderSizeId]
                !== root.synchronizedValues[root.borderSizeId]
            || root.appearanceValues[root.roundingId]
                !== root.synchronizedValues[root.roundingId];
        if (!modeChanged && (!root.windowBorderSynced || !pairChanged))
            return false;

        const nextDraft = root.clone(root.draftValues);
        const nextSynchronized = root.clone(root.synchronizedValues);
        if (!nextDraft || !nextSynchronized)
            return false;
        for (const id of [root.borderSizeId, root.roundingId]) {
            nextDraft[id] = root.appearanceValues[id];
            nextSynchronized[id] = root.appearanceValues[id];
        }
        if (!root.validateValues(nextDraft)
                || !root.validateValues(nextSynchronized)) {
            return false;
        }
        root.draftValues = nextDraft;
        root.synchronizedValues = nextSynchronized;
        root.synchronizedWindowBorderSynced = root.windowBorderSynced;
        root.sharedBorderProjectionPending = true;
        return true;
    }

    function settleSharedBorderProjection() {
        if (!root.serviceAvailable
                || !root.sharedBorderProjectionPending
                || root.sharedBorderBusy
                || !root.sharedBorderProjectionVerified) {
            return;
        }
        root.sharedBorderProjectionPending = false;
    }

    function reconcileWindowSpacingProjection() {
        if (!root.serviceAvailable
                || !root.projectionInitialized || !root.trustedValuesValid
                || root.sharedSpacingBusy
                || root.sharedSpacingSyncState === "pending") {
            return false;
        }
        const modeChanged = root.windowSpacingSynced
            !== root.synchronizedWindowSpacingSynced;
        const pairChanged = !root.valueEqual(
                root.appearanceValues[root.gapsInId],
                root.synchronizedValues[root.gapsInId])
            || !root.valueEqual(
                root.appearanceValues[root.gapsOutId],
                root.synchronizedValues[root.gapsOutId]);
        if (!modeChanged && (!root.windowSpacingSynced || !pairChanged))
            return false;

        const nextDraft = root.clone(root.draftValues);
        const nextSynchronized = root.clone(root.synchronizedValues);
        if (!nextDraft || !nextSynchronized)
            return false;
        for (const id of [root.gapsInId, root.gapsOutId]) {
            nextDraft[id] = root.clone(root.appearanceValues[id]);
            nextSynchronized[id] = root.clone(root.appearanceValues[id]);
        }
        if (!root.validateValues(nextDraft)
                || !root.validateValues(nextSynchronized)) {
            return false;
        }
        root.draftValues = nextDraft;
        root.synchronizedValues = nextSynchronized;
        root.synchronizedWindowSpacingSynced = root.windowSpacingSynced;
        root.sharedSpacingProjectionPending = true;
        return true;
    }

    function settleSharedSpacingProjection() {
        if (!root.serviceAvailable
                || !root.sharedSpacingProjectionPending
                || root.sharedSpacingBusy
                || !root.sharedSpacingProjectionVerified) {
            return;
        }
        root.sharedSpacingProjectionPending = false;
    }

    function setDraftValue(id, value) {
        if (!root.controlsEnabled || !root.trustedDefinitionsValid)
            return;
        if (root.windowBorderSynced
                && (id === root.borderSizeId || id === root.roundingId)) {
            return;
        }
        if (root.windowSpacingSynced && root.isWindowSpacingOption(id))
            return;
        const next = root.clone(root.draftValues);
        if (!next)
            return;
        next[id] = root.clone(value);
        if (next[id] === null && value !== null)
            return;
        if (!root.validateValues(next))
            return;
        root.draftValues = next;
    }

    function setExactDecimalDraftValue(id, value) {
        if (!root.controlsEnabled || !root.trustedDefinitionsValid
                || !root.exactDecimalOptionIds.includes(id)
                || (id === root.shadowScaleId
                    && !root.shadowScaleControlEnabled)
                || (typeof value !== "string"
                    && (typeof value !== "number"
                        || !Number.isFinite(value)))) {
            return;
        }
        const option = root.optionById(id);
        if (!option)
            return;
        if (typeof value === "number"
                && (value < option.min || value > option.max)) {
            return;
        }
        const next = root.clone(root.draftValues);
        if (!next)
            return;
        next[id] = typeof value === "number" && Object.is(value, -0)
            ? 0 : value;
        root.draftValues = next;
    }

    function setExactVectorComponentDraftValue(id, index, value) {
        if (!root.shadowOffsetControlsEnabled
                || !root.trustedDefinitionsValid
                || !root.exactVectorOptionIds.includes(id)
                || !Number.isInteger(index) || index < 0 || index > 1
                || (typeof value !== "string"
                    && (typeof value !== "number"
                        || !Number.isFinite(value)))) {
            return;
        }
        const option = root.optionById(id);
        if (!option || !Array.isArray(option.min)
                || !Array.isArray(option.max)) {
            return;
        }
        if (typeof value === "number"
                && (value < option.min[index]
                    || value > option.max[index])) {
            return;
        }
        const next = root.clone(root.draftValues);
        if (!next || !Array.isArray(next[id]) || next[id].length !== 2)
            return;
        next[id][index] = typeof value === "number" && Object.is(value, -0)
            ? 0 : value;
        root.draftValues = next;
    }

    function exactPreviewValue(id) {
        const value = root.draftValue(id);
        return typeof value === "number" && Number.isFinite(value)
            ? value : Number.NaN;
    }

    function exactVectorPreviewComponent(id, index) {
        const value = root.vectorDraftComponent(id, index);
        return typeof value === "number" && Number.isFinite(value)
            ? value : Number.NaN;
    }

    function setUnitSliderValue(id, value) {
        if (![root.dimStrengthId, root.activeOpacityId,
                root.inactiveOpacityId, root.fullscreenOpacityId,
                root.dimSpecialId, root.dimAroundId,
                root.blurPopupsIgnoreAlphaId,
                root.blurInputMethodsIgnoreAlphaId].includes(id)
                || typeof value !== "number" || !Number.isFinite(value)) {
            return;
        }
        let canonical = Number(
            (Math.round(value / 0.05) * 0.05).toFixed(2)
        );
        if (Object.is(canonical, -0))
            canonical = 0;
        root.setDraftValue(id, canonical);
    }

    function setDimStrength(value) {
        root.setUnitSliderValue(root.dimStrengthId, value);
    }

    function synchronizeDraft() {
        if (!root.serviceAvailable || !root.appearanceProjectionAvailable
                || !root.appearanceAnimationProjectionAvailable
                || !root.revisionTokenValid
                || !root.trustedValuesValid
                || !root.trustedAnimationProjectionValid
                || root.sharedBorderBusy
                || root.sharedBorderSyncState === "pending"
                || root.sharedBorderProjectionPending
                || root.sharedSpacingBusy
                || root.sharedSpacingSyncState === "pending"
                || root.sharedSpacingProjectionPending) {
            return;
        }
        const next = root.clone(root.appearanceValues);
        const curves = root.clone(root.appearanceCurves);
        const animations = root.clone(root.appearanceAnimations);
        if (!next || !curves || !animations)
            return;
        root.synchronizedValues = root.clone(next);
        root.draftValues = next;
        root.synchronizedCurves = root.clone(curves);
        root.draftCurves = curves;
        root.synchronizedAnimations = root.clone(animations);
        root.draftAnimations = animations;
        root.synchronizedRevisionToken = root.revisionToken;
        root.synchronizedWindowBorderSynced = root.windowBorderSynced;
        root.synchronizedWindowSpacingSynced = root.windowSpacingSynced;
        root.projectionInitialized = true;
        root.externalChangeWhileEditing = false;
        root.saveSubmitted = false;
        root.submittedValues = ({});
        root.submittedCurves = [];
        root.submittedAnimations = [];
        root.submittedRevisionToken = "";
        root.sharedBorderProjectionPending = false;
        root.sharedSpacingProjectionPending = false;
    }

    function resetDraftToDefaults() {
        if (!root.controlsEnabled || !root.trustedDefinitionsValid)
            return;
        const target = root.resetTargetValues();
        const curves = root.clone(root.synchronizedCurves);
        if (!target || !curves)
            return;
        root.draftValues = target;
        root.draftCurves = curves;
        root.draftAnimations = [];
    }

    function submitDraft() {
        if (!root.saveEnabled)
            return;
        const candidate = root.clone(root.draftValues);
        const curves = root.clone(root.draftCurves);
        const animations = root.clone(root.draftAnimations);
        if (!candidate || !curves || !animations
                || !root.validateValues(candidate)
                || !root.glowCombinationSafe(candidate)
                || !root.validateCurveCollection(curves, false)
                || !root.validateAnimationCollection(animations, curves)) {
            return;
        }
        root.saveSubmitted = true;
        root.submittedValues = root.clone(candidate);
        root.submittedCurves = root.clone(curves);
        root.submittedAnimations = root.clone(animations);
        root.submittedRevisionToken = root.revisionToken;
        root.saveRequested(candidate, curves, animations);
        // Main forwards this signal synchronously. If the client rejects the
        // request at the authorization boundary without entering busy state,
        // release the submission guard while preserving the draft.
        root.scheduleProjectionReview();
    }

    function reviewProjection() {
        if (!root.serviceAvailable || !root.trustedValuesValid
                || !root.trustedAnimationProjectionValid) {
            if (root.serviceAvailable && !root.saveSubmitted
                    && root.projectionInitialized && root.draftDirty
                    && root.revisionTokenValid
                    && root.synchronizedRevisionTokenValid
                    && root.revisionToken
                        !== root.synchronizedRevisionToken) {
                root.externalChangeWhileEditing = true;
            }
            return;
        }
        if (root.sharedBorderBusy
                || root.sharedBorderSyncState === "pending"
                || root.sharedSpacingBusy
                || root.sharedSpacingSyncState === "pending") {
            return;
        }
        if (!root.projectionInitialized) {
            root.synchronizeDraft();
            return;
        }
        root.reconcileWindowBorderProjection();
        root.reconcileWindowSpacingProjection();
        if ((root.sharedBorderProjectionPending
                || root.sharedSpacingProjectionPending)
                && root.valuesEqualExceptSharedVisual(
                    root.appearanceValues, root.synchronizedValues
                )) {
            root.synchronizedRevisionToken = root.revisionToken;
        }
        root.settleSharedBorderProjection();
        root.settleSharedSpacingProjection();
        if (root.saveSubmitted) {
            if (root.busy || root.sharedVisualTransitionBusy)
                return;
            if (root.valuesEqual(
                    root.appearanceValues, root.submittedValues)
                    && root.valueEqual(
                        root.appearanceCurves, root.submittedCurves)
                    && root.valueEqual(
                        root.appearanceAnimations,
                        root.submittedAnimations)) {
                root.synchronizeDraft();
                return;
            }
            if (root.revisionToken === root.submittedRevisionToken) {
                // Replace did not commit. Preserve the draft for retry.
                root.saveSubmitted = false;
                root.submittedValues = ({});
                root.submittedCurves = [];
                root.submittedAnimations = [];
                root.submittedRevisionToken = "";
                return;
            }
            root.saveSubmitted = false;
            root.externalChangeWhileEditing = true;
            return;
        }
        const projectionChanged = root.revisionToken
                !== root.synchronizedRevisionToken
            || !root.valuesEqual(
                root.appearanceValues, root.synchronizedValues
            ) || !root.valueEqual(
                root.appearanceCurves, root.synchronizedCurves
            ) || !root.valueEqual(
                root.appearanceAnimations, root.synchronizedAnimations
            );
        if (!projectionChanged)
            return;
        if (root.draftDirty || root.externalChangeWhileEditing) {
            root.externalChangeWhileEditing = true;
            return;
        }
        root.synchronizeDraft();
    }

    function scheduleProjectionReview() {
        Qt.callLater(root.reviewProjection);
    }

    onAppearanceOptionsChanged: root.scheduleProjectionReview()
    onAppearanceValuesChanged: root.scheduleProjectionReview()
    onAppearanceCurvesChanged: root.scheduleProjectionReview()
    onAppearanceAnimationsChanged: root.scheduleProjectionReview()
    onAppearanceAnimationProjectionAvailableChanged:
        root.scheduleProjectionReview()
    onRevisionTokenChanged: root.scheduleProjectionReview()
    onServiceAvailableChanged: root.scheduleProjectionReview()
    onSharedBorderConfigRevisionTokenChanged:
        root.scheduleProjectionReview()
    onSharedBorderVerifiedRevisionTokenChanged:
        root.scheduleProjectionReview()
    onWindowBorderSyncedChanged: {
        if (root.projectionInitialized)
            root.sharedBorderProjectionPending = true;
        root.scheduleProjectionReview();
        root.scheduleSharedBorderSourceRequestReview();
    }
    onSharedBorderAvailableChanged: {
        if (root.sharedBorderAvailable)
            root.sharedBorderSourceActionError = "";
        root.scheduleSharedBorderSourceRequestReview();
    }
    onSharedBorderBusyChanged: {
        if (root.sharedBorderBusy) {
            if (root.sharedBorderSourceRequestPending)
                root.sharedBorderSourceRequestSawBusy = true;
            else
                root.sharedBorderSourceActionError = "";
        }
        root.scheduleProjectionReview();
        root.scheduleSharedBorderSourceRequestReview();
    }
    onSharedBorderClientErrorChanged: {
        if (root.sharedBorderClientError.length === 0) {
            if (root.sharedBorderSourceRequestPending) {
                root.sharedBorderSourceRequestErrorCleared = true;
            } else {
                root.sharedBorderSourceActionError = "";
            }
        }
        root.scheduleSharedBorderSourceRequestReview();
    }
    onSharedBorderSyncStateChanged: {
        if (root.projectionInitialized
                && root.sharedBorderSyncState === "pending")
            root.sharedBorderProjectionPending = true;
        root.scheduleProjectionReview();
    }
    onSharedSpacingConfigRevisionTokenChanged:
        root.scheduleProjectionReview()
    onSharedSpacingVerifiedRevisionTokenChanged:
        root.scheduleProjectionReview()
    onWindowSpacingSyncedChanged: {
        if (root.projectionInitialized)
            root.sharedSpacingProjectionPending = true;
        root.scheduleProjectionReview();
        root.scheduleSharedSpacingSourceRequestReview();
    }
    onSharedSpacingAvailableChanged: {
        if (root.sharedSpacingAvailable)
            root.sharedSpacingSourceActionError = "";
        root.scheduleSharedSpacingSourceRequestReview();
    }
    onSharedSpacingBusyChanged: {
        if (root.sharedSpacingBusy) {
            if (root.sharedSpacingSourceRequestPending)
                root.sharedSpacingSourceRequestSawBusy = true;
            else
                root.sharedSpacingSourceActionError = "";
        }
        root.scheduleProjectionReview();
        root.scheduleSharedSpacingSourceRequestReview();
    }
    onSharedSpacingClientErrorChanged: {
        if (root.sharedSpacingClientError.length === 0) {
            if (root.sharedSpacingSourceRequestPending) {
                root.sharedSpacingSourceRequestErrorCleared = true;
            } else {
                root.sharedSpacingSourceActionError = "";
            }
        }
        root.scheduleSharedSpacingSourceRequestReview();
    }
    onSharedSpacingSyncStateChanged: {
        if (root.projectionInitialized
                && root.sharedSpacingSyncState === "pending") {
            root.sharedSpacingProjectionPending = true;
        }
        root.scheduleProjectionReview();
    }
    onBusyChanged: {
        root.scheduleProjectionReview();
        if (appearanceRecoveryDialog.opened && root.busy)
            appearanceRecoveryDialog.close();
    }
    onErrorNameChanged: root.scheduleProjectionReview()
    onErrorMessageChanged: root.scheduleProjectionReview()
    onRecoveryAvailableChanged: {
        if (appearanceRecoveryDialog.opened && !root.recoveryAvailable)
            appearanceRecoveryDialog.close();
    }
    onApplyStateChanged: root.scheduleProjectionReview()
    onStatusIsDangerChanged: {
        if (!root.statusIsDanger)
            return;
        Qt.callLater(function() {
            if (root.statusIsDanger)
                appearanceOptionsScrollView.contentItem.contentY = 0;
        });
    }
    Component.onCompleted: root.scheduleProjectionReview()

    background: Rectangle { color: root.palette.window }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: root.compactPreview
            ? Math.min(root.contentTopMargin, 12)
            : root.contentTopMargin
        spacing: root.compactPreview ? 12 : 20

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: stickyPreview.implicitHeight
            Layout.minimumHeight: stickyPreview.implicitHeight

            Frame {
                id: stickyPreview

                objectName: "appearanceStickyPreview"
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.max(0, Math.min(parent.width - 48, 980))
                padding: root.compactPreview ? 8 : 18

                background: Rectangle {
                    color: root.palette.base
                    radius: 16
                    border.color: root.palette.mid
                }

                contentItem: ColumnLayout {
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        visible: !root.compactPreview

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Window preview")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            text: qsTr("Illustrative")
                            color: root.palette.placeholderText
                            font.pixelSize: 11
                        }
                    }

                    AppearancePreview {
                        objectName: "appearancePreview"
                        Layout.fillWidth: true
                        borderSize: Number(
                            root.draftValue(root.borderSizeId)
                        ) || 0
                        rounding: Number(
                            root.draftValue(root.roundingId)
                        ) || 0
                        roundingPower:
                            root.exactPreviewValue(root.roundingPowerId)
                        blurEnabled: root.draftValue(root.blurId) === true
                        shadowEnabled:
                            root.draftValue(root.shadowId) === true
                        shadowRange: Number(
                            root.draftValue(root.shadowRangeId)
                        ) || 0
                        shadowRenderPower: Number(
                            root.draftValue(root.shadowRenderPowerId)
                        ) || 0
                        shadowSharp:
                            root.draftValue(root.shadowSharpId) === true
                        shadowOffsetX: root.exactVectorPreviewComponent(
                            root.shadowOffsetId, 0
                        )
                        shadowOffsetY: root.exactVectorPreviewComponent(
                            root.shadowOffsetId, 1
                        )
                        shadowScale: root.exactPreviewValue(
                            root.shadowScaleId
                        )
                        glowEnabled:
                            root.draftValue(root.glowEnabledId) === true
                        glowRange: Number(
                            root.draftValue(root.glowRangeId)
                        ) || 0
                        glowRenderPower: Number(
                            root.draftValue(root.glowRenderPowerId)
                        ) || 0
                        borderPartOfWindow: root.draftValue(
                            root.borderPartOfWindowId
                        ) === true
                        dimInactive:
                            root.draftValue(root.dimInactiveId) === true
                        dimStrength: Number(
                            root.draftValue(root.dimStrengthId)
                        ) || 0
                        activeOpacity: Number(
                            root.draftValue(root.activeOpacityId)
                        ) || 0
                        inactiveOpacity: Number(
                            root.draftValue(root.inactiveOpacityId)
                        ) || 0
                        fullscreenOpacity: Number(
                            root.draftValue(root.fullscreenOpacityId)
                        ) || 0
                        dimModal:
                            root.draftValue(root.dimModalId) === true
                        dimSpecial: Number(
                            root.draftValue(root.dimSpecialId)
                        ) || 0
                        dimAround: Number(
                            root.draftValue(root.dimAroundId)
                        ) || 0
                        blurSize: Number(
                            root.draftValue(root.blurSizeId)
                        ) || 0
                        blurPasses: Number(
                            root.draftValue(root.blurPassesId)
                        ) || 0
                        blurIgnoreOpacity:
                            root.draftValue(
                                root.blurIgnoreOpacityId
                            ) === true
                        blurOptimizations:
                            root.draftValue(
                                root.blurOptimizationsId
                            ) === true
                        blurXray:
                            root.draftValue(root.blurXrayId) === true
                        blurBrightness:
                            root.exactPreviewValue(root.blurBrightnessId)
                        blurContrast:
                            root.exactPreviewValue(root.blurContrastId)
                        blurNoise:
                            root.exactPreviewValue(root.blurNoiseId)
                        blurVibrancy:
                            root.exactPreviewValue(root.blurVibrancyId)
                        blurVibrancyDarkness: root.exactPreviewValue(
                            root.blurVibrancyDarknessId
                        )
                        blurSpecial:
                            root.draftValue(root.blurSpecialId) === true
                        blurPopups:
                            root.draftValue(root.blurPopupsId) === true
                        blurPopupsIgnoreAlpha: Number(
                            root.draftValue(
                                root.blurPopupsIgnoreAlphaId
                            )
                        ) || 0
                        blurInputMethods:
                            root.draftValue(
                                root.blurInputMethodsId
                            ) === true
                        blurInputMethodsIgnoreAlpha: Number(
                            root.draftValue(
                                root.blurInputMethodsIgnoreAlphaId
                            )
                        ) || 0
                        animationsEnabled:
                            root.draftValue(root.animationsId) === true
                        layoutMode: "dwindle"
                        resizeOnBorder: false
                        snapEnabled: false
                    }

                    Label {
                        objectName: "appearanceAnimationPreviewDisclaimer"
                        Layout.fillWidth: true
                        text: root.compactPreview
                            ? qsTr("Inner glow size, falloff, color, opacity, blur, and motion are not simulated. Shadow rendering, window corner power, border-inclusive bounds, blur details, fullscreen/contextual dimming, and animation details are summary-only; blur is on/off only.")
                            : qsTr("Window corner power, true fullscreen, modal-dialog parent, special-workspace dimming, Dim around Rule, border-inclusive shadow bounds, custom curve shapes, animation speeds, styles, and rule order are not simulated. Shadow range, falloff, sharp edges, shadow offset, and shadow scale are not simulated. Inner glow size, falloff, color, opacity, blur, and motion are not simulated. Blur is illustrated only as on or off; Kawase renderer tuning, brightness, contrast, noise, vibrancy, dark-area vibrancy, opacity handling, optimized and X-ray rendering, special-workspace blur, popup blur, and input-method blur are not simulated.")
                        color: root.palette.placeholderText
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.name: text
                    }
                }
            }
        }

        ScrollView {
            id: appearanceOptionsScrollView

            objectName: "appearanceOptionsScrollView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                objectName: "appearanceOptionsContent"
                x: Math.max(24, (root.width - width) / 2)
                width: Math.max(0, Math.min(root.width - 48, 980))
                spacing: 20

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 3

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Appearance")
                            color: root.palette.text
                            font.pixelSize: 28
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Choose the shell color mode and shape window visuals through the managed compositor configuration.")
                            color: root.palette.placeholderText
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }

                    Button {
                        objectName: "refreshAppearanceButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Refresh")
                        enabled: !root.busy && !root.displayTestActive
                        icon.name: "view-refresh-symbolic"
                        Accessible.name: qsTr("Refresh compositor appearance settings")

                        onClicked: root.refreshRequested()
                    }
                }

                Frame {
                    objectName: "appearanceStatusCard"
                    Layout.fillWidth: true
                    visible: root.statusVisible
                    padding: 16

                    background: Rectangle {
                        color: root.statusIsDanger
                            ? ShellTheme.errorContainer
                            : ShellTheme.warningContainer
                        radius: 12
                        border.color: root.statusIsDanger
                            ? ShellTheme.errorOutline
                            : ShellTheme.warningOutline
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        Label {
                            objectName: "appearanceStatusMessage"
                            Layout.fillWidth: true
                            text: root.statusMessage
                            color: root.statusIsDanger
                                ? ShellTheme.onErrorContainer
                                : ShellTheme.onWarningContainer
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: text
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: childrenRect.height
                            spacing: 10

                            Button {
                                objectName: "appearanceOpenDisplaysButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                visible: root.serviceAvailable
                                    && root.managementState === "unmanaged"
                                text: qsTr("Review takeover in Displays")
                                enabled: !root.busy
                                Accessible.name: qsTr("Open Displays to review compositor takeover")

                                onClicked: root.openDisplaysRequested()
                            }

                            Button {
                                objectName: "loadCurrentAppearanceButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                visible: root.externalChangeWhileEditing
                                text: qsTr("Load current settings")
                                enabled: !root.busy
                                    && !root.saveSubmitted
                                    && !root.sharedVisualTransitionBusy
                                    && root.appearanceProjectionAvailable
                                    && root.trustedValuesValid
                                    && root.appearanceAnimationProjectionAvailable
                                    && root.trustedAnimationProjectionValid
                                Accessible.name: qsTr("Discard this draft and load the current compositor settings")

                                onClicked: root.synchronizeDraft()
                            }

                            Button {
                                objectName: "retryApplyAppearanceButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                visible: root.retryApplyAvailable
                                text: root.busyOperation === "compositor-apply"
                                    || root.busyOperation === "appearance-apply"
                                    ? qsTr("Retrying apply…")
                                    : qsTr("Retry apply")
                                enabled: root.retryApplyAvailable && !root.busy
                                    && root.sharedVisualApplySafe
                                    && root.authoritativeGlowSafe
                                Accessible.name: qsTr("Retry applying the exact saved compositor revision")

                                onClicked: root.retryApplyRequested()
                            }

                            Button {
                                objectName: "recoverAppearanceButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                visible: root.recoveryAvailable
                                text: qsTr("Restore last working configuration")
                                enabled: root.recoveryAvailable && !root.busy
                                Accessible.name: qsTr("Review whole-compositor recovery")

                                onClicked: appearanceRecoveryDialog.open()
                            }
                        }
                    }
                }

                ThemeModeSelector {
                    Layout.fillWidth: true
                    mode: root.shellAppearanceMode
                    effectiveMode: root.shellEffectiveAppearanceMode
                    serviceAvailable: root.shellAppearanceServiceAvailable
                    busy: root.shellAppearanceBusy
                    errorText: root.shellAppearanceError

                    onModeRequested: mode =>
                        root.shellAppearanceModeRequested(mode)
                }

                ThemeAutomationCard {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    visible: root.shellAppearanceMode === "automatic"
                    source: root.shellAppearanceAutomationSource
                    scheduleMode: root.shellAppearanceScheduleMode
                    darkStartMinute: root.shellAppearanceDarkStartMinute
                    lightStartMinute: root.shellAppearanceLightStartMinute
                    locationSource: root.shellAppearanceLocationSource
                    hasLocation: root.shellAppearanceHasLocation
                    latitude: root.shellAppearanceLatitude
                    longitude: root.shellAppearanceLongitude
                    effectiveMode: root.shellEffectiveAppearanceMode
                    nextTransition: root.shellAppearanceNextTransition
                    sunrise: root.shellAppearanceSunrise
                    sunset: root.shellAppearanceSunset
                    status: root.shellAppearanceAutomationStatus
                    serviceAvailable: root.shellAppearanceServiceAvailable
                    busy: root.shellAppearanceBusy
                    errorText: root.shellAppearanceAutomationError
                    nightLightReady: root.hyprsunsetAvailable
                        && root.nightLightAutomatic

                    onSettingsRequested: (
                        source,
                        scheduleMode,
                        darkStartMinute,
                        lightStartMinute,
                        locationSource,
                        hasLocation,
                        latitude,
                        longitude
                    ) => root.shellAppearanceAutomationRequested(
                        source,
                        scheduleMode,
                        darkStartMinute,
                        lightStartMinute,
                        locationSource,
                        hasLocation,
                        latitude,
                        longitude
                    )
                }

                NightLightCard {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    nightLightEnabled: root.nightLightEnabled
                    automatic: root.nightLightAutomatic
                    scheduleMode: root.nightLightScheduleMode
                    darkStartMinute: root.nightLightDarkStartMinute
                    lightStartMinute: root.nightLightLightStartMinute
                    locationSource: root.nightLightLocationSource
                    hasLocation: root.nightLightHasLocation
                    latitude: root.nightLightLatitude
                    longitude: root.nightLightLongitude
                    nightTemperature: root.nightLightTemperature
                    dayTemperature: root.nightLightDayTemperature
                    gradual: root.nightLightGradual
                    status: root.nightLightStatus
                    currentTemperature: root.nightLightCurrentTemperature
                    nextTransition: root.nightLightNextTransition
                    sunrise: root.nightLightSunrise
                    sunset: root.nightLightSunset
                    hyprsunsetAvailable: root.hyprsunsetAvailable
                    serviceAvailable: root.shellAppearanceServiceAvailable
                    busy: root.shellAppearanceBusy
                    errorText: root.nightLightSettingsError

                    onSettingsRequested: (
                        nightLightEnabled,
                        automatic,
                        scheduleMode,
                        darkStartMinute,
                        lightStartMinute,
                        locationSource,
                        hasLocation,
                        latitude,
                        longitude,
                        nightTemperature,
                        dayTemperature,
                        gradual
                    ) => root.nightLightSettingsRequested(
                        nightLightEnabled,
                        automatic,
                        scheduleMode,
                        darkStartMinute,
                        lightStartMinute,
                        locationSource,
                        hasLocation,
                        latitude,
                        longitude,
                        nightTemperature,
                        dayTemperature,
                        gradual
                    )
                }

                TabBar {
                    id: appearanceTabBar

                    objectName: "appearanceTabBar"
                    Layout.fillWidth: true
                    currentIndex: root.appearanceTabIndex

                    onCurrentIndexChanged:
                        root.appearanceTabIndex = currentIndex

                    TabButton {
                        objectName: "appearanceVisualsTab"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Visuals")
                        Accessible.name: qsTr("Window visual settings")
                    }

                    TabButton {
                        objectName: "appearanceAnimationsTab"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Animations")
                        Accessible.name: qsTr("Curve and animation settings")
                    }
                }

                Frame {
                    objectName: "windowSpacingSettingsCard"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 0
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        Label {
                            text: qsTr("Window spacing")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Frame {
                            Layout.fillWidth: true
                            padding: 14

                            background: Rectangle {
                                color: root.windowSpacingSynced
                                    ? Qt.rgba(
                                        root.palette.highlight.r,
                                        root.palette.highlight.g,
                                        root.palette.highlight.b,
                                        0.09
                                    )
                                    : root.palette.window
                                radius: 12
                                border.color: root.windowSpacingSynced
                                    ? Qt.rgba(
                                        root.palette.highlight.r,
                                        root.palette.highlight.g,
                                        root.palette.highlight.b,
                                        0.34
                                    )
                                    : root.palette.mid
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 10

                                Label {
                                    objectName: "windowSpacingAuthorityMessage"
                                    Layout.fillWidth: true
                                    text: root.windowSpacingAuthorityMessage
                                    color: root.palette.text
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                }

                                Label {
                                    objectName: "sharedSpacingMutationErrorMessage"
                                    Layout.fillWidth: true
                                    visible:
                                        root.sharedSpacingSourceActionError.length
                                            > 0
                                    text: qsTr("The shared spacing source could not be changed. %1").arg(
                                        root.sharedSpacingSourceActionError
                                    )
                                    color: ShellTheme.onErrorContainer
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                    Accessible.role: Accessible.AlertMessage
                                    Accessible.name: text
                                }

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Button {
                                        objectName: "windowSpacingSourceButton"
                                        implicitHeight: Math.max(
                                            root.minimumTargetSize,
                                            implicitBackgroundHeight,
                                            implicitContentHeight
                                                + topPadding + bottomPadding
                                        )
                                        text: root.windowSpacingSynced
                                            ? qsTr("Override window spacing")
                                            : qsTr("Sync with HyprShelld")
                                        enabled:
                                            root.sharedSpacingSourceActionEnabled
                                        Accessible.name: text

                                        onClicked: root.requestWindowSpacingSync(
                                            !root.windowSpacingSynced
                                        )
                                    }

                                    Button {
                                        objectName: "retrySharedSpacingSyncButton"
                                        implicitHeight: Math.max(
                                            root.minimumTargetSize,
                                            implicitBackgroundHeight,
                                            implicitContentHeight
                                                + topPadding + bottomPadding
                                        )
                                        visible:
                                            root.sharedSpacingRetryAvailable
                                        enabled: visible
                                            && !root.sharedSpacingBusy
                                            && !root.sharedSpacingSourceRequestPending
                                            && !root.sharedBorderTransitionBusy
                                            && !root.busy
                                            && !root.displayTestActive
                                        text: qsTr("Retry synchronization")
                                        Accessible.name: text

                                        onClicked:
                                            root.retrySharedSpacingSyncRequested()
                                    }
                                }
                            }
                        }

                        Repeater {
                            model: [
                                {
                                    id: root.gapsInId,
                                    title: qsTr("Inner window gaps"),
                                    description: qsTr("Set the gap on each side between neighboring windows."),
                                    prefix: "appearanceGapsIn"
                                },
                                {
                                    id: root.gapsOutId,
                                    title: qsTr("Outer window gaps"),
                                    description: qsTr("Set the gap on each side between windows and monitor edges. While synced, the top is zero because the Bar reservation already supplies that spacing."),
                                    prefix: "appearanceGapsOut"
                                }
                            ]

                            ColumnLayout {
                                id: gapGroup

                                required property var modelData
                                readonly property string gapId:
                                    modelData.id

                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: gapGroup.modelData.title
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: gapGroup.modelData.description
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: root.width < 640 ? 2 : 4
                                    columnSpacing: 12
                                    rowSpacing: 8

                                    Repeater {
                                        model: [
                                            { label: qsTr("Top"), suffix: "Top" },
                                            { label: qsTr("Right"), suffix: "Right" },
                                            { label: qsTr("Bottom"), suffix: "Bottom" },
                                            { label: qsTr("Left"), suffix: "Left" }
                                        ]

                                        ColumnLayout {
                                            id: gapPart

                                            required property int index
                                            required property var modelData
                                            property string projectedText:
                                                String(root.gapComponent(
                                                    gapGroup.gapId, index
                                                ))

                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 4

                                            Label {
                                                Layout.fillWidth: true
                                                text: gapPart.modelData.label
                                                color:
                                                    root.palette.placeholderText
                                                font.pixelSize: 12
                                                textFormat: Text.PlainText
                                            }

                                            TextField {
                                                id: gapField

                                                objectName:
                                                    gapGroup.modelData.prefix
                                                    + gapPart.modelData.suffix
                                                Layout.fillWidth: true
                                                implicitHeight:
                                                    root.minimumTargetSize
                                                enabled: root.controlsEnabled
                                                    && !root.windowSpacingSynced
                                                inputMethodHints:
                                                    Qt.ImhFormattedNumbersOnly
                                                Accessible.name: qsTr("%1 %2 edge gap").arg(
                                                    gapGroup.modelData.title
                                                ).arg(gapPart.modelData.label)
                                                validator:
                                                    RegularExpressionValidator {
                                                        regularExpression:
                                                            /^(0|-?[1-9][0-9]*)$/
                                                    }

                                                Component.onCompleted:
                                                    text = gapPart.projectedText
                                                onActiveFocusChanged: {
                                                    if (!activeFocus) {
                                                        text =
                                                            gapPart.projectedText;
                                                    }
                                                }
                                                onEditingFinished: {
                                                    if (!root.setGapComponent(
                                                            gapGroup.gapId,
                                                            gapPart.index,
                                                            text)) {
                                                        text =
                                                            gapPart.projectedText;
                                                    }
                                                }
                                            }

                                            onProjectedTextChanged: {
                                                if (!gapField.activeFocus)
                                                    gapField.text = projectedText;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("The Bar still attaches to a covering maximized window. Once the protected maximize rule has been safely applied, it keeps that window gapless even when normal spacing is overridden.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 0
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        Label {
                            text: qsTr("Window style")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Frame {
                            Layout.fillWidth: true
                            padding: 14

                            background: Rectangle {
                                color: root.windowBorderSynced
                                    ? Qt.rgba(
                                        root.palette.highlight.r,
                                        root.palette.highlight.g,
                                        root.palette.highlight.b,
                                        0.09
                                    )
                                    : root.palette.window
                                radius: 12
                                border.color: root.windowBorderSynced
                                    ? Qt.rgba(
                                        root.palette.highlight.r,
                                        root.palette.highlight.g,
                                        root.palette.highlight.b,
                                        0.34
                                    )
                                    : root.palette.mid
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 10

                                Label {
                                    objectName: "windowBorderAuthorityMessage"
                                    Layout.fillWidth: true
                                    text: root.windowBorderAuthorityMessage
                                    color: root.palette.text
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                }

                                Label {
                                    objectName: "sharedBorderMutationErrorMessage"
                                    Layout.fillWidth: true
                                    visible:
                                        root.sharedBorderSourceActionError.length
                                            > 0
                                    text: qsTr("The shared border source could not be changed. %1").arg(
                                        root.sharedBorderSourceActionError
                                    )
                                    color: ShellTheme.onErrorContainer
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                    Accessible.role: Accessible.AlertMessage
                                    Accessible.name: text
                                }

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Button {
                                        objectName: "windowBorderSourceButton"
                                        implicitHeight: Math.max(
                                            root.minimumTargetSize,
                                            implicitBackgroundHeight,
                                            implicitContentHeight
                                                + topPadding + bottomPadding
                                        )
                                        text: root.windowBorderSynced
                                            ? qsTr("Override window borders")
                                            : qsTr("Sync with HyprShelld")
                                        enabled:
                                            root.sharedBorderSourceActionEnabled
                                        Accessible.name: text

                                        onClicked: root.requestWindowBorderSync(
                                            !root.windowBorderSynced
                                        )
                                    }

                                    Button {
                                        objectName: "retrySharedBorderSyncButton"
                                        implicitHeight: Math.max(
                                            root.minimumTargetSize,
                                            implicitBackgroundHeight,
                                            implicitContentHeight
                                                + topPadding + bottomPadding
                                        )
                                        visible:
                                            root.sharedBorderRetryAvailable
                                        enabled: visible
                                            && !root.sharedBorderBusy
                                            && !root.sharedBorderSourceRequestPending
                                            && !root.sharedSpacingTransitionBusy
                                            && !root.busy
                                            && !root.displayTestActive
                                        text: qsTr("Retry synchronization")
                                        Accessible.name: text

                                        onClicked:
                                            root.retrySharedBorderSyncRequested()
                                    }
                                }
                            }
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Border thickness")
                            description: qsTr("Set the managed border width around windows in layout pixels.")
                            from: root.optionMinimum(root.borderSizeId)
                            to: root.optionMaximum(root.borderSizeId)
                            value: Number(root.draftValue(root.borderSizeId)) || 0
                            enabled: root.controlsEnabled
                                && !root.windowBorderSynced
                            controlObjectName: "appearanceBorderSize"
                            accessibleName: qsTr("Window border thickness")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.borderSizeId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Corner radius")
                            description: qsTr("Round window corners by this many layout pixels.")
                            from: root.optionMinimum(root.roundingId)
                            to: root.optionMaximum(root.roundingId)
                            value: Number(root.draftValue(root.roundingId)) || 0
                            enabled: root.controlsEnabled
                                && !root.windowBorderSynced
                            controlObjectName: "appearanceRounding"
                            accessibleName: qsTr("Window corner radius")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.roundingId, value
                            )
                        }

                        SettingsDecimalRow {
                            objectName: "appearanceRoundingPowerRow"
                            Layout.fillWidth: true
                            title: qsTr("Window corner power")
                            description: qsTr("Set the corner power from 2 through 10. The default 2 is circular; higher values make corners squarer, and Hyprland adjusts the effective corner radius with the power. This direct compositor choice stays editable while shared borders are synced or the global radius is zero because a Window Rule may still use it.")
                            value: root.draftValue(root.roundingPowerId)
                            minimumValue:
                                root.optionMinimum(root.roundingPowerId)
                            maximumValue:
                                root.optionMaximum(root.roundingPowerId)
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceRoundingPower"
                            validationObjectName:
                                "appearanceRoundingPowerValidation"
                            validationExample: "2.5"
                            accessibleName: qsTr("Window corner power")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setExactDecimalDraftValue(
                                    root.roundingPowerId, value
                                )
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 0
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        Label {
                            text: qsTr("Visual effects")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Blur backgrounds")
                            description: qsTr("Allow Hyprland to blur content behind translucent windows.")
                            checked: root.draftValue(root.blurId) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceBlurEnabled"
                            accessibleName: qsTr("Blur window backgrounds")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setDraftValue(root.blurId, value)
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Window shadows")
                            description: qsTr("Draw managed drop shadows behind windows.")
                            checked: root.draftValue(root.shadowId) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceShadowEnabled"
                            accessibleName: qsTr("Window shadows")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setDraftValue(root.shadowId, value)
                        }

                        SettingsToggleRow {
                            objectName: "appearanceGlowEnabledRow"
                            Layout.fillWidth: true
                            title: qsTr("Inner window glow")
                            description: qsTr("Draw a glow just inside each window edge. Set Glow range to at least 10 before turning it on. Glow colors are not edited on this page.")
                            checked: root.draftValue(
                                root.glowEnabledId
                            ) === true
                            enabled: root.glowEnabledControlEnabled
                            controlObjectName: "appearanceGlowEnabled"
                            accessibleName: qsTr("Inner window glow")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.glowEnabledId, value
                            )
                        }

                        SettingsToggleRow {
                            objectName: "appearanceBorderPartOfWindowRow"
                            Layout.fillWidth: true
                            title: qsTr("Include borders in window shadows")
                            description: qsTr("Size each window shadow from the outside edge of its visible border. The saved choice is retained while window shadows are off.")
                            checked: root.draftValue(
                                root.borderPartOfWindowId
                            ) === true
                            enabled: root.controlsEnabled
                                && root.draftValue(root.shadowId) === true
                            controlObjectName:
                                "appearanceBorderPartOfWindow"
                            accessibleName:
                                qsTr("Include borders in window shadows")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.borderPartOfWindowId, value
                            )
                        }

                        SettingsToggleRow {
                            objectName: "appearanceDimInactiveRow"
                            Layout.fillWidth: true
                            title: qsTr("Dim inactive windows")
                            description: qsTr("Darken windows that do not have focus. A matching Window Rule can keep a window undimmed.")
                            checked: root.draftValue(
                                root.dimInactiveId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceDimInactive"
                            accessibleName: qsTr("Dim inactive windows")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.dimInactiveId, value
                            )
                        }

                        SettingsSliderRow {
                            objectName: "appearanceDimStrengthRow"
                            Layout.fillWidth: true
                            title: qsTr("Inactive-window dimming strength")
                            description: qsTr("Choose how strongly inactive windows are darkened. The saved strength is retained while inactive-window dimming is off.")
                            from: root.optionMinimum(root.dimStrengthId)
                            to: root.optionMaximum(root.dimStrengthId)
                            value: Number(root.draftValue(
                                root.dimStrengthId
                            )) || 0
                            stepSize: 0.05
                            decimals: 2
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.controlsEnabled
                                && root.draftValue(root.dimInactiveId) === true
                            controlObjectName: "appearanceDimStrength"
                            valueObjectName: "appearanceDimStrengthValue"
                            accessibleName:
                                qsTr("Inactive-window dimming strength")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setDimStrength(value)
                        }

                    }
                }

                Frame {
                    objectName: "appearanceShadowRenderingCard"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 0
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        Label {
                            text: qsTr("Window shadow rendering")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsSpinBoxRow {
                            objectName: "appearanceShadowRangeRow"
                            Layout.fillWidth: true
                            title: qsTr("Shadow range")
                            description: qsTr("Set how far each window shadow extends beyond its window in layout pixels. This value is retained while window shadows are off and still controls the extent of sharp shadows.")
                            from: root.optionMinimum(root.shadowRangeId)
                            to: root.optionMaximum(root.shadowRangeId)
                            value: Number(
                                root.draftValue(root.shadowRangeId)
                            ) || 0
                            enabled: root.controlsEnabled
                                && root.draftValue(root.shadowId) === true
                            controlObjectName: "appearanceShadowRange"
                            accessibleName: qsTr("Shadow range")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.shadowRangeId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            objectName: "appearanceShadowRenderPowerRow"
                            Layout.fillWidth: true
                            title: qsTr("Soft-shadow falloff")
                            description: qsTr("Choose the soft-shadow falloff power from 1 through 4. Higher values fade more quickly. This value is retained while window shadows are off or sharp edges are enabled.")
                            from: root.optionMinimum(
                                root.shadowRenderPowerId
                            )
                            to: root.optionMaximum(
                                root.shadowRenderPowerId
                            )
                            value: Number(root.draftValue(
                                root.shadowRenderPowerId
                            )) || 0
                            enabled: root.controlsEnabled
                                && root.draftValue(root.shadowId) === true
                                && root.draftValue(
                                    root.shadowSharpId
                                ) !== true
                            controlObjectName:
                                "appearanceShadowRenderPower"
                            accessibleName: qsTr("Soft-shadow falloff")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.shadowRenderPowerId, value
                            )
                        }

                        SettingsToggleRow {
                            objectName: "appearanceShadowSharpRow"
                            Layout.fillWidth: true
                            title: qsTr("Sharp shadow edges")
                            description: qsTr("Draw a solid-edged shadow instead of a soft falloff. Shadow range still controls its extent; the saved soft falloff returns when this is off.")
                            checked: root.draftValue(
                                root.shadowSharpId
                            ) === true
                            enabled: root.controlsEnabled
                                && root.draftValue(root.shadowId) === true
                            controlObjectName: "appearanceShadowSharp"
                            accessibleName: qsTr("Sharp shadow edges")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.shadowSharpId, value
                            )
                        }

                        SettingsDecimalRow {
                            objectName: "appearanceShadowScaleRow"
                            Layout.fillWidth: true
                            title: qsTr("Shadow scale")
                            description: qsTr("Scale each window shadow around its center from 0 through 1. The default 1 keeps its full size; lower exact values shrink it, and 0 makes it invisible. The saved value is retained while window shadows are off and applies to both soft and sharp shadows.")
                            value: root.draftValue(root.shadowScaleId)
                            minimumValue: root.optionMinimum(
                                root.shadowScaleId
                            )
                            maximumValue: root.optionMaximum(
                                root.shadowScaleId
                            )
                            controlWidth: root.compactPreview ? 160 : 190
                            controlObjectName: "appearanceShadowScale"
                            validationObjectName:
                                "appearanceShadowScaleValidation"
                            validationExample: "0.75"
                            accessibleName: qsTr("Shadow scale")
                            minimumTargetSize: root.minimumTargetSize
                            enabled: root.shadowScaleControlEnabled

                            onValueModified: value =>
                                root.setExactDecimalDraftValue(
                                    root.shadowScaleId, value
                                )
                        }

                        SettingsDecimalRow {
                            objectName: "appearanceShadowOffsetXRow"
                            Layout.fillWidth: true
                            title: qsTr("Horizontal shadow offset")
                            description: qsTr("Move every window shadow horizontally in layout pixels. Positive values move right and negative values move left. The exact saved value is retained while window shadows are off.")
                            value: root.vectorDraftComponent(
                                root.shadowOffsetId, 0
                            )
                            minimumValue: root.optionComponentMinimum(
                                root.shadowOffsetId, 0
                            )
                            maximumValue: root.optionComponentMaximum(
                                root.shadowOffsetId, 0
                            )
                            controlWidth: root.compactPreview ? 160 : 190
                            controlObjectName: "appearanceShadowOffsetX"
                            validationObjectName:
                                "appearanceShadowOffsetXValidation"
                            validationExample: "-12.5"
                            accessibleName: qsTr("Horizontal shadow offset")
                            minimumTargetSize: root.minimumTargetSize
                            enabled: root.shadowOffsetControlsEnabled

                            onValueModified: value =>
                                root.setExactVectorComponentDraftValue(
                                    root.shadowOffsetId, 0, value
                                )
                        }

                        SettingsDecimalRow {
                            objectName: "appearanceShadowOffsetYRow"
                            Layout.fillWidth: true
                            title: qsTr("Vertical shadow offset")
                            description: qsTr("Move every window shadow vertically in layout pixels. Positive values move down and negative values move up. The exact saved value is retained while window shadows are off.")
                            value: root.vectorDraftComponent(
                                root.shadowOffsetId, 1
                            )
                            minimumValue: root.optionComponentMinimum(
                                root.shadowOffsetId, 1
                            )
                            maximumValue: root.optionComponentMaximum(
                                root.shadowOffsetId, 1
                            )
                            controlWidth: root.compactPreview ? 160 : 190
                            controlObjectName: "appearanceShadowOffsetY"
                            validationObjectName:
                                "appearanceShadowOffsetYValidation"
                            validationExample: "8.25"
                            accessibleName: qsTr("Vertical shadow offset")
                            minimumTargetSize: root.minimumTargetSize
                            enabled: root.shadowOffsetControlsEnabled

                            onValueModified: value =>
                                root.setExactVectorComponentDraftValue(
                                    root.shadowOffsetId, 1, value
                                )
                        }

                    }
                }

                Frame {
                    objectName: "appearanceGlowRenderingCard"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 0
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        Label {
                            text: qsTr("Window inner glow")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            objectName: "appearanceGlowSafetyMessage"
                            Layout.fillWidth: true
                            visible: root.glowSafetyViolation
                            text: qsTr("Inner window glow is on with a range below 10. Turn it off or set Glow range to at least 10 before saving or applying.")
                            color: ShellTheme.onErrorContainer
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: text
                        }

                        SettingsSpinBoxRow {
                            objectName: "appearanceGlowRangeRow"
                            Layout.fillWidth: true
                            title: qsTr("Glow range")
                            description: qsTr("Set the glow size from 0 through 100 layout pixels. Values below 10 are retained only while Inner window glow is off.")
                            from: root.optionMinimum(root.glowRangeId)
                            to: root.optionMaximum(root.glowRangeId)
                            value: Number(
                                root.draftValue(root.glowRangeId)
                            ) || 0
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceGlowRange"
                            accessibleName: qsTr("Glow range")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.glowRangeId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            objectName: "appearanceGlowRenderPowerRow"
                            Layout.fillWidth: true
                            title: qsTr("Glow falloff")
                            description: qsTr("Choose falloff power from 1 through 4. Higher values fade more quickly. The saved value is retained while Inner window glow is off.")
                            from: root.optionMinimum(
                                root.glowRenderPowerId
                            )
                            to: root.optionMaximum(
                                root.glowRenderPowerId
                            )
                            value: Number(root.draftValue(
                                root.glowRenderPowerId
                            )) || 0
                            enabled: root.glowFalloffControlEnabled
                            controlObjectName: "appearanceGlowRenderPower"
                            accessibleName: qsTr("Glow falloff")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.glowRenderPowerId, value
                            )
                        }
                    }
                }

                Frame {
                    objectName: "appearanceBlurRenderingCard"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 0
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        Label {
                            objectName: "appearanceBlurRenderingHeading"
                            text: qsTr("Blur rendering")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("These values tune Hyprland's Kawase blur. They remain saved while Blur backgrounds is off.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }

                        SettingsSpinBoxRow {
                            objectName: "appearanceBlurSizeRow"
                            Layout.fillWidth: true
                            title: qsTr("Blur size")
                            description: qsTr("Choose the blur distance from 0 to 100. Hyprland stores and uses this full range; larger values increase the blur distance and GPU work.")
                            from: root.optionMinimum(root.blurSizeId)
                            to: root.optionMaximum(root.blurSizeId)
                            value: Number(
                                root.draftValue(root.blurSizeId)
                            ) || 0
                            enabled: root.blurDetailsEnabled
                            controlObjectName: "appearanceBlurSize"
                            accessibleName: qsTr("Blur size")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.blurSizeId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            objectName: "appearanceBlurPassesRow"
                            Layout.fillWidth: true
                            title: qsTr("Blur passes")
                            description: qsTr("Choose the saved pass count from 0 to 10. Hyprland's renderer clamps the effective count to 1–8; within that effective range, additional passes cost more GPU work.")
                            from: root.optionMinimum(root.blurPassesId)
                            to: root.optionMaximum(root.blurPassesId)
                            value: Number(
                                root.draftValue(root.blurPassesId)
                            ) || 0
                            enabled: root.blurDetailsEnabled
                            controlObjectName: "appearanceBlurPasses"
                            accessibleName: qsTr("Blur passes")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.blurPassesId, value
                            )
                        }

                        SettingsToggleRow {
                            objectName: "appearanceBlurIgnoreOpacityRow"
                            Layout.fillWidth: true
                            title: qsTr("Ignore window opacity")
                            description: qsTr("Make the blur layer ignore window opacity. The saved setting is retained while blur is off.")
                            checked: root.draftValue(
                                root.blurIgnoreOpacityId
                            ) === true
                            enabled: root.blurDetailsEnabled
                            controlObjectName:
                                "appearanceBlurIgnoreOpacity"
                            accessibleName: qsTr("Ignore window opacity")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.blurIgnoreOpacityId, value
                            )
                        }

                        SettingsToggleRow {
                            objectName: "appearanceBlurOptimizationsRow"
                            Layout.fillWidth: true
                            title: qsTr("Optimized blur path")
                            description: qsTr("Use Hyprland's optimized blur path. Turning it off preserves the X-ray setting, but X-ray is inactive until this path is enabled again.")
                            checked: root.draftValue(
                                root.blurOptimizationsId
                            ) === true
                            enabled: root.blurDetailsEnabled
                            controlObjectName:
                                "appearanceBlurOptimizations"
                            accessibleName: qsTr("Optimized blur path")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.blurOptimizationsId, value
                            )
                        }

                        SettingsToggleRow {
                            objectName: "appearanceBlurXrayRow"
                            Layout.fillWidth: true
                            title: qsTr("X-ray blur")
                            description: qsTr("Make floating-window blur ignore tiled windows. This requires the optimized blur path. Window Rules can override individual windows; Layer Rules control layer-surface X-ray separately.")
                            checked:
                                root.draftValue(root.blurXrayId) === true
                            enabled: root.blurXrayEnabled
                            controlObjectName: "appearanceBlurXray"
                            accessibleName: qsTr("X-ray blur")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.blurXrayId, value
                            )
                        }
                    }
                }

                Frame {
                    objectName: "appearanceBlurModulationCard"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 0
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        Label {
                            objectName: "appearanceBlurModulationHeading"
                            text: qsTr("Blur color modulation")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Tune the color processing applied by Hyprland's blur. Exact decimal values remain saved while Blur backgrounds is off.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }

                        SettingsDecimalRow {
                            objectName: "appearanceBlurBrightnessRow"
                            Layout.fillWidth: true
                            title: qsTr("Blur brightness")
                            description: qsTr("Set brightness modulation from 0 through 2. The Hyprland default is 1.")
                            value: root.draftValue(root.blurBrightnessId)
                            minimumValue:
                                root.optionMinimum(root.blurBrightnessId)
                            maximumValue:
                                root.optionMaximum(root.blurBrightnessId)
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.blurDetailsEnabled
                            controlObjectName: "appearanceBlurBrightness"
                            validationObjectName:
                                "appearanceBlurBrightnessValidation"
                            accessibleName: qsTr("Blur brightness")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setExactDecimalDraftValue(
                                    root.blurBrightnessId, value
                                )
                        }

                        SettingsDecimalRow {
                            objectName: "appearanceBlurContrastRow"
                            Layout.fillWidth: true
                            title: qsTr("Blur contrast")
                            description: qsTr("Set contrast modulation from 0 through 2. The Hyprland default is 0.8916.")
                            value: root.draftValue(root.blurContrastId)
                            minimumValue:
                                root.optionMinimum(root.blurContrastId)
                            maximumValue:
                                root.optionMaximum(root.blurContrastId)
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.blurDetailsEnabled
                            controlObjectName: "appearanceBlurContrast"
                            validationObjectName:
                                "appearanceBlurContrastValidation"
                            accessibleName: qsTr("Blur contrast")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setExactDecimalDraftValue(
                                    root.blurContrastId, value
                                )
                        }

                        SettingsDecimalRow {
                            objectName: "appearanceBlurNoiseRow"
                            Layout.fillWidth: true
                            title: qsTr("Blur noise")
                            description: qsTr("Set how much noise Hyprland applies to blur, from 0 through 1. The Hyprland default is 0.0117.")
                            value: root.draftValue(root.blurNoiseId)
                            minimumValue:
                                root.optionMinimum(root.blurNoiseId)
                            maximumValue:
                                root.optionMaximum(root.blurNoiseId)
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.blurDetailsEnabled
                            controlObjectName: "appearanceBlurNoise"
                            validationObjectName:
                                "appearanceBlurNoiseValidation"
                            accessibleName: qsTr("Blur noise")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setExactDecimalDraftValue(
                                    root.blurNoiseId, value
                                )
                        }

                        SettingsDecimalRow {
                            objectName: "appearanceBlurVibrancyRow"
                            Layout.fillWidth: true
                            title: qsTr("Blur vibrancy")
                            description: qsTr("Increase the saturation of blurred colors from 0 through 1. The Hyprland default is 0.1696.")
                            value: root.draftValue(root.blurVibrancyId)
                            minimumValue:
                                root.optionMinimum(root.blurVibrancyId)
                            maximumValue:
                                root.optionMaximum(root.blurVibrancyId)
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.blurDetailsEnabled
                            controlObjectName: "appearanceBlurVibrancy"
                            validationObjectName:
                                "appearanceBlurVibrancyValidation"
                            accessibleName: qsTr("Blur vibrancy")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setExactDecimalDraftValue(
                                    root.blurVibrancyId, value
                                )
                        }

                        SettingsDecimalRow {
                            objectName: "appearanceBlurVibrancyDarknessRow"
                            Layout.fillWidth: true
                            title: qsTr("Dark-area vibrancy")
                            description: qsTr("Set how strongly vibrancy affects dark areas from 0 through 1. The Hyprland default is 0.")
                            value: root.draftValue(
                                root.blurVibrancyDarknessId
                            )
                            minimumValue: root.optionMinimum(
                                root.blurVibrancyDarknessId
                            )
                            maximumValue: root.optionMaximum(
                                root.blurVibrancyDarknessId
                            )
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.blurDetailsEnabled
                            controlObjectName:
                                "appearanceBlurVibrancyDarkness"
                            validationObjectName:
                                "appearanceBlurVibrancyDarknessValidation"
                            accessibleName: qsTr("Dark-area vibrancy")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setExactDecimalDraftValue(
                                    root.blurVibrancyDarknessId, value
                                )
                        }
                    }
                }

                Frame {
                    objectName: "appearanceBlurContextsCard"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 0
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        Label {
                            objectName: "appearanceBlurContextsHeading"
                            text: qsTr("Blur contexts")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Enable blur for additional compositor surfaces. Every context value remains saved while Blur backgrounds is off.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }

                        SettingsToggleRow {
                            objectName: "appearanceBlurSpecialRow"
                            Layout.fillWidth: true
                            title: qsTr("Special-workspace blur")
                            description: qsTr("Blur behind the special workspace. This is expensive. In Hyprland 0.56.1, changing it while a special workspace is already open takes effect after that workspace is closed and reopened.")
                            checked: root.draftValue(
                                root.blurSpecialId
                            ) === true
                            enabled: root.blurDetailsEnabled
                            controlObjectName: "appearanceBlurSpecial"
                            accessibleName: qsTr("Special-workspace blur")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.blurSpecialId, value
                            )
                        }

                        SettingsToggleRow {
                            objectName: "appearanceBlurPopupsRow"
                            Layout.fillWidth: true
                            title: qsTr("Popup blur")
                            description: qsTr("Blur popups such as right-click menus. The opacity threshold below is retained while popup blur is off.")
                            checked:
                                root.draftValue(root.blurPopupsId) === true
                            enabled: root.blurDetailsEnabled
                            controlObjectName: "appearanceBlurPopups"
                            accessibleName: qsTr("Popup blur")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.blurPopupsId, value
                            )
                        }

                        SettingsSliderRow {
                            objectName:
                                "appearanceBlurPopupsIgnoreAlphaRow"
                            Layout.fillWidth: true
                            title: qsTr("Popup ignore-alpha threshold")
                            description: qsTr("Do not blur popup pixels whose opacity is below this saved value. Live mapped popups use the saved 0.00–1.00 value directly. Only popup snapshot or fadeout capture applies a 0.01 minimum; on that capture path, a layer owner's Rule ignore-alpha value can replace this global threshold. The saved threshold is retained while popup blur is off.")
                            from: root.optionMinimum(
                                root.blurPopupsIgnoreAlphaId
                            )
                            to: root.optionMaximum(
                                root.blurPopupsIgnoreAlphaId
                            )
                            value: Number(root.draftValue(
                                root.blurPopupsIgnoreAlphaId
                            )) || 0
                            stepSize: 0.05
                            decimals: 2
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.blurPopupThresholdEnabled
                            controlObjectName:
                                "appearanceBlurPopupsIgnoreAlpha"
                            valueObjectName:
                                "appearanceBlurPopupsIgnoreAlphaValue"
                            accessibleName:
                                qsTr("Popup ignore-alpha threshold")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setUnitSliderValue(
                                    root.blurPopupsIgnoreAlphaId, value
                                )
                        }

                        SettingsToggleRow {
                            objectName: "appearanceBlurInputMethodsRow"
                            Layout.fillWidth: true
                            title: qsTr("Input-method blur")
                            description: qsTr("Blur input-method surfaces such as fcitx5. The opacity threshold below is retained while input-method blur is off.")
                            checked: root.draftValue(
                                root.blurInputMethodsId
                            ) === true
                            enabled: root.blurDetailsEnabled
                            controlObjectName:
                                "appearanceBlurInputMethods"
                            accessibleName: qsTr("Input-method blur")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.blurInputMethodsId, value
                            )
                        }

                        SettingsSliderRow {
                            objectName:
                                "appearanceBlurInputMethodsIgnoreAlphaRow"
                            Layout.fillWidth: true
                            title: qsTr("Input-method ignore-alpha threshold")
                            description: qsTr("Do not blur input-method pixels whose opacity is below this saved value. Live input-method rendering uses the saved 0.00–1.00 value directly and does not apply a 0.01 minimum. The saved threshold is retained while input-method blur is off.")
                            from: root.optionMinimum(
                                root.blurInputMethodsIgnoreAlphaId
                            )
                            to: root.optionMaximum(
                                root.blurInputMethodsIgnoreAlphaId
                            )
                            value: Number(root.draftValue(
                                root.blurInputMethodsIgnoreAlphaId
                            )) || 0
                            stepSize: 0.05
                            decimals: 2
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled:
                                root.blurInputMethodThresholdEnabled
                            controlObjectName:
                                "appearanceBlurInputMethodsIgnoreAlpha"
                            valueObjectName:
                                "appearanceBlurInputMethodsIgnoreAlphaValue"
                            accessibleName:
                                qsTr("Input-method ignore-alpha threshold")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setUnitSliderValue(
                                    root.blurInputMethodsIgnoreAlphaId,
                                    value
                                )
                        }
                    }
                }

                Frame {
                    objectName: "appearanceWindowOpacityCard"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 0
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        Label {
                            objectName: "appearanceWindowOpacityHeading"
                            text: qsTr("Window opacity")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsSliderRow {
                            objectName: "appearanceActiveOpacityRow"
                            Layout.fillWidth: true
                            title: qsTr("Active-window opacity")
                            description: qsTr("Set the opacity of focused windows. A matching Window Rule can change the resulting per-window opacity.")
                            from: root.optionMinimum(root.activeOpacityId)
                            to: root.optionMaximum(root.activeOpacityId)
                            value: Number(root.draftValue(
                                root.activeOpacityId
                            )) || 0
                            stepSize: 0.05
                            decimals: 2
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceActiveOpacity"
                            valueObjectName: "appearanceActiveOpacityValue"
                            accessibleName: qsTr("Active-window opacity")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setUnitSliderValue(
                                    root.activeOpacityId, value
                                )
                        }

                        SettingsSliderRow {
                            objectName: "appearanceInactiveOpacityRow"
                            Layout.fillWidth: true
                            title: qsTr("Inactive-window opacity")
                            description: qsTr("Set the opacity of windows without focus. A matching Window Rule can change the resulting per-window opacity; inactive-window dimming remains separate.")
                            from: root.optionMinimum(root.inactiveOpacityId)
                            to: root.optionMaximum(root.inactiveOpacityId)
                            value: Number(root.draftValue(
                                root.inactiveOpacityId
                            )) || 0
                            stepSize: 0.05
                            decimals: 2
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceInactiveOpacity"
                            valueObjectName: "appearanceInactiveOpacityValue"
                            accessibleName: qsTr("Inactive-window opacity")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setUnitSliderValue(
                                    root.inactiveOpacityId, value
                                )
                        }

                        SettingsSliderRow {
                            objectName: "appearanceFullscreenOpacityRow"
                            Layout.fillWidth: true
                            title: qsTr("Fullscreen-window opacity")
                            description: qsTr("Set the opacity of true fullscreen windows. Maximized windows use the focused or unfocused value; a matching Window Rule can change the resulting per-window opacity.")
                            from: root.optionMinimum(root.fullscreenOpacityId)
                            to: root.optionMaximum(root.fullscreenOpacityId)
                            value: Number(root.draftValue(
                                root.fullscreenOpacityId
                            )) || 0
                            stepSize: 0.05
                            decimals: 2
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceFullscreenOpacity"
                            valueObjectName:
                                "appearanceFullscreenOpacityValue"
                            accessibleName: qsTr("Fullscreen-window opacity")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setUnitSliderValue(
                                    root.fullscreenOpacityId, value
                                )
                        }
                    }
                }

                Frame {
                    objectName: "appearanceContextualDimmingCard"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 0
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        Label {
                            objectName: "appearanceContextualDimmingHeading"
                            text: qsTr("Contextual dimming")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsToggleRow {
                            objectName: "appearanceDimModalRow"
                            Layout.fillWidth: true
                            title: qsTr("Dim parents of modal dialogs")
                            description: qsTr("Darken a parent window while one of its modal dialogs is open. This is applied separately and can combine with inactive-window dimming.")
                            checked:
                                root.draftValue(root.dimModalId) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceDimModal"
                            accessibleName:
                                qsTr("Dim parents of modal dialogs")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.dimModalId, value
                            )
                        }

                        SettingsSliderRow {
                            objectName: "appearanceDimSpecialRow"
                            Layout.fillWidth: true
                            title: qsTr("Special-workspace dimming")
                            description: qsTr("Choose how strongly the ordinary workspace is darkened behind an open special workspace. In Hyprland 0.56.1, changing this while a special workspace is already open takes effect after it is closed and reopened.")
                            from: root.optionMinimum(root.dimSpecialId)
                            to: root.optionMaximum(root.dimSpecialId)
                            value: Number(root.draftValue(
                                root.dimSpecialId
                            )) || 0
                            stepSize: 0.05
                            decimals: 2
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceDimSpecial"
                            valueObjectName: "appearanceDimSpecialValue"
                            accessibleName:
                                qsTr("Special-workspace dimming")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setUnitSliderValue(
                                    root.dimSpecialId, value
                                )
                        }

                        SettingsSliderRow {
                            objectName: "appearanceDimAroundRow"
                            Layout.fillWidth: true
                            title: qsTr("Dim-around strength")
                            description: qsTr("Choose how strongly the rest of the screen is darkened when a Window or Layer Rule enables Dim around. The darkening follows the matched window or layer through its fade-out.")
                            from: root.optionMinimum(root.dimAroundId)
                            to: root.optionMaximum(root.dimAroundId)
                            value: Number(root.draftValue(
                                root.dimAroundId
                            )) || 0
                            stepSize: 0.05
                            decimals: 2
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceDimAround"
                            valueObjectName: "appearanceDimAroundValue"
                            accessibleName: qsTr("Dim-around strength")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setUnitSliderValue(
                                    root.dimAroundId, value
                                )
                        }
                    }
                }

                Frame {
                    objectName: "appearanceAnimationsOverviewCard"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 1
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 14

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Curves and animation rules")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Animations")
                            description: qsTr("Enable the saved curves and animation rules. Turning this off preserves every detailed value.")
                            checked:
                                root.draftValue(root.animationsId) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "appearanceAnimationsEnabled"
                            accessibleName: qsTr("Hyprland animations")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.animationsId, value
                            )
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("%1 custom curves · %2 animation rules").arg(
                                root.draftCurves.length
                            ).arg(root.draftAnimations.length)
                            color: root.palette.placeholderText
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }
                }

                AnimationCollectionsSummary {
                    objectName: "appearanceAnimationCollectionsSummary"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 1
                        && !root.animationDetailActive
                    curves: root.draftCurves
                    animations: root.draftAnimations
                    animationsEnabled:
                        root.draftValue(root.animationsId) === true
                    controlsEnabled: root.animationControlsEnabled
                    inspectionEnabled: root.controlsEnabled
                    draftDirty: root.draftDirty
                    draftValid: root.draftAnimationCollectionsValid
                    minimumTargetSize: root.minimumTargetSize

                    onEditCurveRequested: id => root.openCurve(id)
                    onMoveCurveRequested: (id, offset) =>
                        root.moveCurve(id, offset)
                    onAddAnimationRequested: root.addAnimation()
                    onEditAnimationRequested: id => root.openAnimation(id)
                    onEnabledAnimationRequested: (id, enabled) =>
                        root.setAnimationProperty(id, "enabled", enabled)
                    onMoveAnimationRequested: (id, offset) =>
                        root.moveAnimation(id, offset)
                    onRemoveAnimationRequested: id =>
                        root.removeAnimation(id)
                }

                AnimationCurveEditor {
                    objectName: "appearanceAnimationCurveEditor"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 1
                        && root.editingCurve !== null
                    curve: root.editingCurve
                    controlsEnabled: root.animationControlsEnabled
                    referenceCount: root.editingCurve
                        ? root.curveReferenceCount(root.editingCurve.name) : 0
                    curveIssue: root.curveIssue(root.editingCurve)
                    minimumTargetSize: root.minimumTargetSize

                    onCloseRequested: root.closeAnimationDetail()
                    onPropertyModified: (id, propertyName, value) =>
                        root.setCurveProperty(id, propertyName, value)
                    onPointModified: (
                        id, pointIndex, coordinateIndex, value
                    ) => root.setCurvePoint(
                        id, pointIndex, coordinateIndex, value
                    )
                }

                AnimationRuleEditor {
                    objectName: "appearanceAnimationRuleEditor"
                    Layout.fillWidth: true
                    visible: root.appearanceTabIndex === 1
                        && root.editingAnimation !== null
                    animation: root.editingAnimation
                    leafChoices: root.editingAnimation
                        ? root.animationLeafChoices(
                            root.editingAnimation.id
                        ) : []
                    curveChoices: root.curveChoices()
                    controlsEnabled: root.animationControlsEnabled
                    animationIssue:
                        root.animationIssue(root.editingAnimation)
                    minimumTargetSize: root.minimumTargetSize

                    onCloseRequested: root.closeAnimationDetail()
                    onRemoveRequested: id => root.removeAnimation(id)
                    onPropertyModified: (id, propertyName, value) =>
                        root.setAnimationProperty(id, propertyName, value)
                }

                Frame {
                    objectName: "appearanceDraftActions"
                    Layout.fillWidth: true
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.saveEnabled
                            ? root.palette.highlight : root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                text: root.externalChangeWhileEditing
                                    ? qsTr("Draft preserved")
                                    : root.draftDirty
                                        ? qsTr("Unsaved compositor appearance draft")
                                        : qsTr("No compositor appearance changes")
                                color: root.palette.text
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.externalChangeWhileEditing
                                    ? qsTr("Load the current settings before creating a new draft. HyprShelld never silently rebases this draft onto another compositor revision.")
                                    : qsTr("Save & apply first persists one validated desired-state revision, then reloads and verifies that exact revision. Custom user Lua is loaded afterward and can override managed values.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }
                        }

                        Label {
                            objectName: "appearanceDraftValidationMessage"
                            Layout.fillWidth: true
                            visible: root.draftDirty && !root.draftValid
                            text: !root.draftValuesValid
                                ? qsTr("Return to Visuals and correct every highlighted value before the combined compositor appearance draft can be saved.")
                                : !root.glowDraftSafe
                                    ? qsTr("Turn Inner window glow off or set Glow range to at least 10 before the combined compositor appearance draft can be saved.")
                                    : qsTr("Finish every curve and animation rule before the combined compositor appearance draft can be saved.")
                            color: ShellTheme.onErrorContainer
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: text
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: childrenRect.height
                            spacing: 10

                            Button {
                                objectName: "discardAppearanceDraftButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                text: qsTr("Discard draft")
                                visible: root.draftDirty
                                    && !root.externalChangeWhileEditing
                                enabled: root.serviceAvailable
                                    && root.appearanceProjectionAvailable
                                    && root.appearanceAnimationProjectionAvailable
                                    && root.revisionTokenValid
                                    && root.trustedValuesValid
                                    && root.trustedAnimationProjectionValid
                                    && !root.busy
                                    && !root.saveSubmitted
                                    && !root.sharedVisualTransitionBusy
                                Accessible.name: qsTr("Discard compositor appearance draft")

                                onClicked: root.synchronizeDraft()
                            }

                            Button {
                                objectName: "resetAppearanceDefaultsButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                text: qsTr("Reset to defaults")
                                enabled: root.controlsEnabled
                                    && root.resetTargetDiffers
                                Accessible.name: qsTr("Reset Appearance values, restore saved curves, and clear animation rules")

                                onClicked: root.resetDraftToDefaults()
                            }

                            Button {
                                objectName: "saveAppearanceButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                text: {
                                    if (root.busyOperation === "appearance-save")
                                        return qsTr("Saving…");
                                    if (root.busyOperation === "compositor-apply"
                                            || root.busyOperation
                                                === "appearance-apply")
                                        return qsTr("Applying…");
                                    return qsTr("Save & apply");
                                }
                                highlighted: true
                                enabled: root.saveEnabled
                                Accessible.name: qsTr("Save and apply the validated compositor appearance draft")

                                onClicked: root.submitDraft()
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 12 }
            }
        }
    }

    CompositorRecoveryDialog {
        id: appearanceRecoveryDialog

        objectName: "appearanceRecoveryDialog"
        recoveryAvailable: root.recoveryAvailable
        operationBusy: root.busy
        busyOperation: root.busyOperation
        settingsAreaName: qsTr("Appearance")
        warningObjectName: "appearanceRecoveryWarning"
        cancelObjectName: "cancelAppearanceRecoveryButton"
        confirmObjectName: "confirmAppearanceRecoveryButton"
        minimumTargetSize: root.minimumTargetSize

        onRecoveryRequested: root.recoveryRequested()
    }
}
