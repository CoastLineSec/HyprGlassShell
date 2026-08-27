pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Page {
    id: root

    property bool serviceAvailable: false
    property bool writable: false
    property bool catalogAvailable: false
    property bool windowsAvailable: false
    property bool windowsProjectionAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property var windowsOptions: []
    property var windowsValues: ({})
    property string revisionToken: "0"
    property double appliedRevision: 0
    property string loadState: "unavailable"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property string windowsErrorName: ""
    property string windowsErrorMessage: ""
    property string sharedErrorName: ""
    property string sharedErrorMessage: ""
    property bool retryApplyAvailable: false
    property bool recoveryAvailable: false
    property bool sharedMutationBusy: false
    property bool sharedApplySafe: false
    property bool previewAnimationsEnabled: true
    property real contentTopMargin: 28
    property int engineTabIndex: 0

    property var draftValues: ({})
    property var synchronizedValues: ({})
    property var submittedValues: ({})
    property string synchronizedRevisionToken: ""
    property string submittedRevisionToken: ""
    property bool projectionInitialized: false
    property bool externalChangeWhileEditing: false
    property bool saveSubmitted: false

    signal refreshRequested()
    signal openDisplaysRequested()
    signal saveRequested(var values)
    signal retryApplyRequested()
    signal recoveryRequested()

    readonly property string layoutId: "hyprland.general.layout"
    readonly property string resizeOnBorderId:
        "hyprland.general.resize_on_border"
    readonly property string extendBorderGrabAreaId:
        "hyprland.general.extend_border_grab_area"
    readonly property string hoverIconOnBorderId:
        "hyprland.general.hover_icon_on_border"
    readonly property string resizeCornerId:
        "hyprland.general.resize_corner"
    readonly property string snapEnabledId:
        "hyprland.general.snap.enabled"
    readonly property string snapBorderOverlapId:
        "hyprland.general.snap.border_overlap"
    readonly property string snapMonitorGapId:
        "hyprland.general.snap.monitor_gap"
    readonly property string snapRespectGapsId:
        "hyprland.general.snap.respect_gaps"
    readonly property string snapWindowGapId:
        "hyprland.general.snap.window_gap"
    readonly property string followMouseId: "hyprland.input.follow_mouse"
    readonly property string mouseRefocusId:
        "hyprland.input.mouse_refocus"
    readonly property string followMouseShrinkId:
        "hyprland.input.follow_mouse_shrink"
    readonly property string floatSwitchOverrideFocusId:
        "hyprland.input.float_switch_override_focus"
    readonly property string focusOnCloseId:
        "hyprland.input.focus_on_close"
    readonly property string specialFallthroughId:
        "hyprland.input.special_fallthrough"
    readonly property string noFocusFallbackId:
        "hyprland.general.no_focus_fallback"
    readonly property string modalParentBlockingId:
        "hyprland.general.modal_parent_blocking"
    readonly property string floatGapsId:
        "hyprland.general.float_gaps"
    readonly property string workspaceGapsId:
        "hyprland.general.gaps_workspaces"
    readonly property string singleWindowAspectRatioId:
        "hyprland.layout.single_window_aspect_ratio"
    readonly property string singleWindowAspectRatioToleranceId:
        "hyprland.layout.single_window_aspect_ratio_tolerance"
    readonly property string dwindleDefaultSplitRatioId:
        "hyprland.dwindle.default_split_ratio"
    readonly property string dwindleForceSplitId:
        "hyprland.dwindle.force_split"
    readonly property string dwindlePermanentDirectionOverrideId:
        "hyprland.dwindle.permanent_direction_override"
    readonly property string dwindlePreciseMouseMoveId:
        "hyprland.dwindle.precise_mouse_move"
    readonly property string dwindlePreserveSplitId:
        "hyprland.dwindle.preserve_split"
    readonly property string dwindleSmartResizingId:
        "hyprland.dwindle.smart_resizing"
    readonly property string dwindleSmartSplitId:
        "hyprland.dwindle.smart_split"
    readonly property string dwindleSpecialScaleFactorId:
        "hyprland.dwindle.special_scale_factor"
    readonly property string dwindleSplitBiasId:
        "hyprland.dwindle.split_bias"
    readonly property string dwindleSplitWidthMultiplierId:
        "hyprland.dwindle.split_width_multiplier"
    readonly property string dwindleUseActiveForSplitsId:
        "hyprland.dwindle.use_active_for_splits"
    readonly property string masterAllowSmallSplitId:
        "hyprland.master.allow_small_split"
    readonly property string masterAlwaysKeepPositionId:
        "hyprland.master.always_keep_position"
    readonly property string masterCenterIgnoresReservedId:
        "hyprland.master.center_ignores_reserved"
    readonly property string masterCenterFallbackId:
        "hyprland.master.center_master_fallback"
    readonly property string masterDropAtCursorId:
        "hyprland.master.drop_at_cursor"
    readonly property string masterFocusOnCloseId:
        "hyprland.master.focus_master_on_close"
    readonly property string masterFactorId: "hyprland.master.mfact"
    readonly property string masterNewOnActiveId:
        "hyprland.master.new_on_active"
    readonly property string masterNewOnTopId:
        "hyprland.master.new_on_top"
    readonly property string masterNewStatusId:
        "hyprland.master.new_status"
    readonly property string masterOrientationId:
        "hyprland.master.orientation"
    readonly property string masterCenterSlaveCountId:
        "hyprland.master.slave_count_for_center_master"
    readonly property string masterSmartResizingId:
        "hyprland.master.smart_resizing"
    readonly property string masterSpecialScaleFactorId:
        "hyprland.master.special_scale_factor"
    readonly property string scrollingColumnWidthId:
        "hyprland.scrolling.column_width"
    readonly property string scrollingDirectionId:
        "hyprland.scrolling.direction"
    readonly property string scrollingFocusFitMethodId:
        "hyprland.scrolling.focus_fit_method"
    readonly property string scrollingFollowFocusId:
        "hyprland.scrolling.follow_focus"
    readonly property string scrollingFollowMinimumVisibleId:
        "hyprland.scrolling.follow_min_visible"
    readonly property string scrollingFullscreenOneColumnId:
        "hyprland.scrolling.fullscreen_on_one_column"
    readonly property string scrollingWrapFocusId:
        "hyprland.scrolling.wrap_focus"
    readonly property string scrollingWrapSwapColumnId:
        "hyprland.scrolling.wrap_swapcol"
    readonly property string scrollingMoveSnapCursorId:
        "hyprland.gestures.scrolling.move_snap_cursor"
    readonly property string scrollingMoveSnapGridId:
        "hyprland.gestures.scrolling.move_snap_to_grid"
    readonly property string groupAutoGroupId:
        "hyprland.group.auto_group"
    readonly property string groupInsertAfterCurrentId:
        "hyprland.group.insert_after_current"
    readonly property string groupFocusRemovedWindowId:
        "hyprland.group.focus_removed_window"
    readonly property string groupDragIntoGroupId:
        "hyprland.group.drag_into_group"
    readonly property string groupMergeGroupsOnDragId:
        "hyprland.group.merge_groups_on_drag"
    readonly property string groupMergeGroupsOnGroupbarId:
        "hyprland.group.merge_groups_on_groupbar"
    readonly property string groupMergeFloatedIntoTiledOnGroupbarId:
        "hyprland.group.merge_floated_into_tiled_on_groupbar"
    readonly property string groupOnMoveToWorkspaceId:
        "hyprland.group.group_on_movetoworkspace"
    readonly property string groupbarEnabledId:
        "hyprland.group.groupbar.enabled"
    readonly property string groupbarDisableWhenOnlyId:
        "hyprland.group.groupbar.disable_when_only"
    readonly property string groupbarFontFamilyId:
        "hyprland.group.groupbar.font_family"
    readonly property string groupbarFontWeightActiveId:
        "hyprland.group.groupbar.font_weight_active"
    readonly property string groupbarFontWeightInactiveId:
        "hyprland.group.groupbar.font_weight_inactive"
    readonly property string groupbarFontSizeId:
        "hyprland.group.groupbar.font_size"
    readonly property string groupbarGradientsId:
        "hyprland.group.groupbar.gradients"
    readonly property string groupbarHeightId:
        "hyprland.group.groupbar.height"
    readonly property string groupbarIndicatorGapId:
        "hyprland.group.groupbar.indicator_gap"
    readonly property string groupbarIndicatorHeightId:
        "hyprland.group.groupbar.indicator_height"
    readonly property string groupbarStackedId:
        "hyprland.group.groupbar.stacked"
    readonly property string groupbarPriorityId:
        "hyprland.group.groupbar.priority"
    readonly property string groupbarRenderTitlesId:
        "hyprland.group.groupbar.render_titles"
    readonly property string groupbarScrollingId:
        "hyprland.group.groupbar.scrolling"
    readonly property string groupbarMiddleClickCloseId:
        "hyprland.group.groupbar.middle_click_close"
    readonly property string groupbarRoundingId:
        "hyprland.group.groupbar.rounding"
    readonly property string groupbarRoundingPowerId:
        "hyprland.group.groupbar.rounding_power"
    readonly property string groupbarGradientRoundingId:
        "hyprland.group.groupbar.gradient_rounding"
    readonly property string groupbarGradientRoundingPowerId:
        "hyprland.group.groupbar.gradient_rounding_power"
    readonly property string groupbarRoundOnlyEdgesId:
        "hyprland.group.groupbar.round_only_edges"
    readonly property string groupbarGradientRoundOnlyEdgesId:
        "hyprland.group.groupbar.gradient_round_only_edges"
    readonly property string groupbarGapsOutId:
        "hyprland.group.groupbar.gaps_out"
    readonly property string groupbarGapsInId:
        "hyprland.group.groupbar.gaps_in"
    readonly property string groupbarKeepUpperGapId:
        "hyprland.group.groupbar.keep_upper_gap"
    readonly property string groupbarTextOffsetId:
        "hyprland.group.groupbar.text_offset"
    readonly property string groupbarTextPaddingId:
        "hyprland.group.groupbar.text_padding"
    readonly property string groupbarBlurId:
        "hyprland.group.groupbar.blur"
    readonly property string allowPinFullscreenId:
        "hyprland.binds.allow_pin_fullscreen"
    readonly property string focusPreferredMethodId:
        "hyprland.binds.focus_preferred_method"
    readonly property string ignoreGroupLockId:
        "hyprland.binds.ignore_group_lock"
    readonly property string movefocusCyclesFullscreenId:
        "hyprland.binds.movefocus_cycles_fullscreen"
    readonly property string movefocusCyclesGroupfirstId:
        "hyprland.binds.movefocus_cycles_groupfirst"
    readonly property string windowDirectionMonitorFallbackId:
        "hyprland.binds.window_direction_monitor_fallback"
    readonly property string anrDialogEnabledId:
        "hyprland.misc.enable_anr_dialog"
    readonly property string anrMissedPingsId:
        "hyprland.misc.anr_missed_pings"
    readonly property string sizeLimitsTiledId:
        "hyprland.misc.size_limits_tiled"
    readonly property string alwaysFollowOnDndId:
        "hyprland.misc.always_follow_on_dnd"
    readonly property string focusOnActivateId:
        "hyprland.misc.focus_on_activate"
    readonly property string mouseMoveFocusesMonitorId:
        "hyprland.misc.mouse_move_focuses_monitor"
    readonly property string onFocusUnderFullscreenId:
        "hyprland.misc.on_focus_under_fullscreen"
    readonly property string exitWindowRetainsFullscreenId:
        "hyprland.misc.exit_window_retains_fullscreen"
    readonly property string enableSwallowId:
        "hyprland.misc.enable_swallow"
    readonly property string swallowRegexId:
        "hyprland.misc.swallow_regex"
    readonly property string swallowExceptionRegexId:
        "hyprland.misc.swallow_exception_regex"
    readonly property string followMouseThresholdId:
        "hyprland.input.follow_mouse_threshold"
    readonly property real minimumTargetSize: 44
    readonly property bool compactPreview:
        root.width < 560 || root.height < 640
    readonly property var expectedOptionIds: [
        root.layoutId,
        root.resizeOnBorderId,
        root.extendBorderGrabAreaId,
        root.hoverIconOnBorderId,
        root.resizeCornerId,
        root.snapEnabledId,
        root.snapBorderOverlapId,
        root.snapMonitorGapId,
        root.snapRespectGapsId,
        root.snapWindowGapId,
        root.followMouseId,
        root.mouseRefocusId,
        root.followMouseShrinkId,
        root.floatSwitchOverrideFocusId,
        root.focusOnCloseId,
        root.specialFallthroughId,
        root.noFocusFallbackId,
        root.modalParentBlockingId,
        root.floatGapsId,
        root.workspaceGapsId,
        root.singleWindowAspectRatioId,
        root.singleWindowAspectRatioToleranceId,
        root.dwindleDefaultSplitRatioId,
        root.dwindleForceSplitId,
        root.dwindlePermanentDirectionOverrideId,
        root.dwindlePreciseMouseMoveId,
        root.dwindlePreserveSplitId,
        root.dwindleSmartResizingId,
        root.dwindleSmartSplitId,
        root.dwindleSpecialScaleFactorId,
        root.dwindleSplitBiasId,
        root.dwindleSplitWidthMultiplierId,
        root.dwindleUseActiveForSplitsId,
        root.masterAllowSmallSplitId,
        root.masterAlwaysKeepPositionId,
        root.masterCenterIgnoresReservedId,
        root.masterCenterFallbackId,
        root.masterDropAtCursorId,
        root.masterFocusOnCloseId,
        root.masterFactorId,
        root.masterNewOnActiveId,
        root.masterNewOnTopId,
        root.masterNewStatusId,
        root.masterOrientationId,
        root.masterCenterSlaveCountId,
        root.masterSmartResizingId,
        root.masterSpecialScaleFactorId,
        root.scrollingColumnWidthId,
        root.scrollingDirectionId,
        root.scrollingFocusFitMethodId,
        root.scrollingFollowFocusId,
        root.scrollingFollowMinimumVisibleId,
        root.scrollingFullscreenOneColumnId,
        root.scrollingWrapFocusId,
        root.scrollingWrapSwapColumnId,
        root.scrollingMoveSnapCursorId,
        root.scrollingMoveSnapGridId,
        root.groupAutoGroupId,
        root.groupInsertAfterCurrentId,
        root.groupFocusRemovedWindowId,
        root.groupDragIntoGroupId,
        root.groupMergeGroupsOnDragId,
        root.groupMergeGroupsOnGroupbarId,
        root.groupMergeFloatedIntoTiledOnGroupbarId,
        root.groupOnMoveToWorkspaceId,
        root.groupbarEnabledId,
        root.groupbarDisableWhenOnlyId,
        root.groupbarFontFamilyId,
        root.groupbarFontWeightActiveId,
        root.groupbarFontWeightInactiveId,
        root.groupbarFontSizeId,
        root.groupbarGradientsId,
        root.groupbarHeightId,
        root.groupbarIndicatorGapId,
        root.groupbarIndicatorHeightId,
        root.groupbarStackedId,
        root.groupbarPriorityId,
        root.groupbarRenderTitlesId,
        root.groupbarScrollingId,
        root.groupbarMiddleClickCloseId,
        root.groupbarRoundingId,
        root.groupbarRoundingPowerId,
        root.groupbarGradientRoundingId,
        root.groupbarGradientRoundingPowerId,
        root.groupbarRoundOnlyEdgesId,
        root.groupbarGradientRoundOnlyEdgesId,
        root.groupbarGapsOutId,
        root.groupbarGapsInId,
        root.groupbarKeepUpperGapId,
        root.groupbarTextOffsetId,
        root.groupbarTextPaddingId,
        root.groupbarBlurId,
        root.allowPinFullscreenId,
        root.focusPreferredMethodId,
        root.ignoreGroupLockId,
        root.movefocusCyclesFullscreenId,
        root.movefocusCyclesGroupfirstId,
        root.windowDirectionMonitorFallbackId,
        root.anrDialogEnabledId,
        root.anrMissedPingsId,
        root.sizeLimitsTiledId,
        root.alwaysFollowOnDndId,
        root.focusOnActivateId,
        root.mouseMoveFocusesMonitorId,
        root.onFocusUnderFullscreenId,
        root.exitWindowRetainsFullscreenId,
        root.enableSwallowId,
        root.swallowRegexId,
        root.swallowExceptionRegexId,
        root.followMouseThresholdId
    ]
    readonly property bool trustedDefinitionsValid: root.validateOptions()
    readonly property bool trustedValuesValid:
        root.windowsProjectionAvailable
        && root.trustedDefinitionsValid
        && root.validateValues(root.windowsValues)
    readonly property bool revisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.revisionToken)
    readonly property bool synchronizedRevisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.synchronizedRevisionToken)
    readonly property bool draftValid:
        root.trustedDefinitionsValid && root.validateValues(root.draftValues)
        && root.aspectRatioCoherent
    readonly property bool draftDirty:
        root.projectionInitialized
        && !root.valuesEqual(root.draftValues, root.synchronizedValues)
    readonly property bool displayTestActive:
        root.confirmationState !== "idle"
        || root.managementState === "preview"
    readonly property bool controlsEnabled:
        root.serviceAvailable
        && root.writable
        && root.catalogAvailable
        && root.windowsAvailable
        && root.revisionTokenValid
        && root.trustedDefinitionsValid
        && root.trustedValuesValid
        && !root.busy
        && !root.saveSubmitted
        && !root.sharedMutationBusy
        && root.sharedApplySafe
        && !root.externalChangeWhileEditing
        && !root.displayTestActive
    readonly property bool saveEnabled:
        root.controlsEnabled && root.draftDirty && root.draftValid
        && !root.saveSubmitted && root.sharedApplySafe
    readonly property bool resizeChildrenEnabled:
        root.controlsEnabled
        && root.draftValue(root.resizeOnBorderId) === true
    readonly property bool snapChildrenEnabled:
        root.controlsEnabled
        && root.draftValue(root.snapEnabledId) === true
    readonly property bool pointerFocusChildrenEnabled:
        root.controlsEnabled && root.draftValue(root.followMouseId) === 1
    readonly property bool dragAndDropFollowOverrideEnabled:
        root.controlsEnabled && root.draftValue(root.followMouseId) !== 1
    readonly property bool followMouseThresholdEnabled:
        root.controlsEnabled
        && (root.draftValue(root.followMouseId) === 1
            || root.draftValue(root.alwaysFollowOnDndId) === true)
    readonly property bool anrChildrenEnabled:
        root.controlsEnabled
        && root.draftValue(root.anrDialogEnabledId) === true
    readonly property bool swallowChildrenEnabled:
        root.controlsEnabled
        && root.draftValue(root.enableSwallowId) === true
    readonly property bool aspectRatioCoherent: {
        const ratio = root.draftValue(root.singleWindowAspectRatioId);
        return Array.isArray(ratio) && ratio.length === 2
            && ((ratio[0] === 0 && ratio[1] === 0)
                || (ratio[0] > 0 && ratio[1] > 0));
    }
    readonly property bool aspectRatioEnabled: {
        const ratio = root.draftValue(root.singleWindowAspectRatioId);
        return root.aspectRatioCoherent && Array.isArray(ratio)
            && ratio[0] > 0;
    }
    readonly property bool dwindleForceSplitEnabled:
        root.controlsEnabled
        && root.draftValue(root.dwindleSmartSplitId) !== true
    readonly property bool masterCenterChildrenEnabled:
        root.controlsEnabled
        && root.draftValue(root.masterOrientationId) === "center"
    readonly property bool scrollingFollowChildrenEnabled:
        root.controlsEnabled
        && root.draftValue(root.scrollingFollowFocusId) === true
    readonly property bool groupDragChildrenEnabled:
        root.controlsEnabled
        && root.draftValue(root.groupDragIntoGroupId) !== 0
    readonly property bool groupGroupbarMergeEnabled:
        root.groupbarChildrenEnabled
        && root.groupDragChildrenEnabled
        && root.draftValue(root.groupMergeGroupsOnDragId) === true
    readonly property bool groupbarChildrenEnabled:
        root.controlsEnabled
        && root.draftValue(root.groupbarEnabledId) === true
    readonly property bool groupbarTitleChildrenEnabled:
        root.groupbarChildrenEnabled
        && root.draftValue(root.groupbarRenderTitlesId) === true
    readonly property bool groupbarHeightEnabled:
        root.groupbarChildrenEnabled
        && (root.draftValue(root.groupbarRenderTitlesId) === true
            || root.draftValue(root.groupbarGradientsId) === true)
    readonly property bool groupbarHorizontalGapEnabled:
        root.groupbarChildrenEnabled
        && root.draftValue(root.groupbarStackedId) !== true
    readonly property bool groupbarKeepUpperGapEnabled:
        root.groupbarChildrenEnabled
        && Number(root.draftValue(root.groupbarGapsOutId)) > 0
    readonly property bool groupbarRoundingChildrenEnabled:
        root.groupbarChildrenEnabled
        && Number(root.draftValue(root.groupbarRoundingId)) > 0
    readonly property bool groupbarGradientEnabled:
        root.groupbarChildrenEnabled
        && root.draftValue(root.groupbarGradientsId) === true
    readonly property bool groupbarGradientRoundingChildrenEnabled:
        root.groupbarGradientEnabled
        && Number(root.draftValue(root.groupbarGradientRoundingId)) > 0
    readonly property bool statusVisible:
        !root.serviceAvailable
        || !root.writable
        || !root.catalogAvailable
        || !root.windowsAvailable
        || !root.revisionTokenValid
        || !root.trustedDefinitionsValid
        || !root.trustedValuesValid
        || root.loadState === "recovered"
        || root.loadState === "defaulted"
        || root.loadState === "unsupported"
        || root.managementState !== "managed"
        || root.applyState !== "current"
        || root.requiredActivation !== "none"
        || root.displayTestActive
        || root.externalChangeWhileEditing
        || root.windowsErrorMessage.length > 0
        || root.sharedErrorMessage.length > 0
        || root.busy
        || root.sharedMutationBusy
        || !root.sharedApplySafe
    readonly property bool statusIsDanger:
        root.managementState === "conflict"
        || root.applyState === "failed"
        || root.loadState === "unsupported"
        || (root.catalogAvailable && !root.trustedDefinitionsValid)
        || (root.windowsProjectionAvailable && !root.windowsAvailable
            && root.windowsErrorMessage.length > 0)
        || root.externalChangeWhileEditing
    readonly property string statusMessage: {
        const windowsDetail = root.windowsErrorMessage.length > 0
            ? " " + root.windowsErrorMessage : "";
        const sharedDetail = root.sharedErrorMessage.length > 0
            ? " " + root.sharedErrorMessage : "";
        if (!root.serviceAvailable)
            return qsTr("Window settings are unavailable. The compositor settings service may be restarting.%1").arg(sharedDetail);
        if (root.displayTestActive)
            return qsTr("A display test is active. Window changes stay locked until that test is kept or reverted.");
        if (root.managementState === "unmanaged")
            return qsTr("HyprShelld is not managing the Hyprland entrypoint yet. Review takeover from Displays before changing window behavior.");
        if (root.managementState === "conflict")
            return qsTr("The managed compositor entrypoint or its ownership state changed unexpectedly. Window changes are locked to preserve it.%1").arg(sharedDetail);
        if (!root.writable)
            return qsTr("This compositor configuration is read-only and has been preserved.");
        if (!root.catalogAvailable)
            return qsTr("The trusted Hyprland option catalog is unavailable or does not match the compositor authority. Window changes are disabled.%1").arg(windowsDetail);
        if (!root.trustedDefinitionsValid || !root.trustedValuesValid)
            return qsTr("The trusted Windows & Layout contract does not match this Settings build. No compositor values will be written.%1").arg(windowsDetail);
        if (!root.revisionTokenValid)
            return qsTr("The exact compositor revision token is unavailable. Window changes are disabled to prevent overwriting another revision.");
        if (root.externalChangeWhileEditing)
            return qsTr("Compositor settings changed outside this draft. Your Windows & Layout draft is preserved, but it cannot be saved over the newer revision. Load the current settings to continue.");
        if (root.busy) {
            if (root.busyOperation === "windows-save")
                return qsTr("Saving the validated Windows & Layout draft…");
            if (root.busyOperation === "compositor-apply"
                    || root.busyOperation === "windows-apply") {
                return qsTr("Applying and verifying the saved compositor revision…");
            }
            if (root.busyOperation === "recover")
                return qsTr("Restoring and verifying the last working compositor configuration…");
            return qsTr("Another compositor operation is in progress. Window changes are temporarily locked.");
        }
        if (root.sharedMutationBusy)
            return qsTr("A shared compositor setting is changing. Window changes remain locked until that transition is verified.");
        if (root.applyState === "retained"
                || root.applyState === "failed"
                || root.applyState === "inactive") {
            if (root.requiredActivation === "reload") {
                return root.retryApplyAvailable
                    ? qsTr("The desired compositor settings were saved, but they are not active. Retry the exact saved revision or restore the last working compositor configuration.%1").arg(sharedDetail)
                    : qsTr("The desired compositor settings are saved but not active. Wait for the compositor service to make retry or recovery available.%1").arg(sharedDetail);
            }
            if (root.requiredActivation === "restart")
                return qsTr("The saved desired state requires a verified compositor-restart workflow that HyprShelld does not have yet, so this revision cannot be activated from Settings. Restore the last working compositor configuration to continue.%1").arg(sharedDetail);
            if (root.requiredActivation === "session")
                return qsTr("The saved desired state requires a verified new-session workflow that HyprShelld does not have yet, so this revision cannot be activated from Settings. Restore the last working compositor configuration to continue.%1").arg(sharedDetail);
            return qsTr("The desired compositor state is not the active state. Review recovery options before making another change.%1").arg(sharedDetail);
        }
        if (root.loadState === "recovered")
            return qsTr("Compositor settings were restored from the last known good desired-state copy. Review the window values before changing them.");
        if (root.loadState === "defaulted")
            return qsTr("Compositor settings could not be recovered, so safe desired-state defaults are in use. Review the window values before continuing.");
        if (root.loadState === "unsupported")
            return qsTr("The compositor settings use a newer format. They were preserved and remain read-only.");
        if (root.windowsProjectionAvailable && !root.windowsAvailable
                && root.windowsErrorMessage.length > 0) {
            return qsTr("Windows & Layout authority verification failed. Current window values remain readable, but changes are disabled until the managed action, schema, and full-state contract is authenticated.%1").arg(windowsDetail);
        }
        if (root.windowsErrorMessage.length > 0)
            return qsTr("The Windows & Layout operation failed.%1").arg(windowsDetail);
        if (root.sharedErrorMessage.length > 0)
            return qsTr("The compositor operation failed.%1").arg(sharedDetail);
        if (!root.windowsAvailable)
            return qsTr("Windows & Layout settings are waiting for a current, verified compositor baseline.%1").arg(windowsDetail);
        if (!root.sharedApplySafe)
            return qsTr("A shared compositor setting is not at a verified activation point. Window controls remain locked until the exact compositor source transition is verified.");
        return "";
    }

    function clone(value) {
        try {
            return JSON.parse(JSON.stringify(value));
        } catch (error) {
            return null;
        }
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
        return left === right;
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

    function optionById(id) {
        if (!Array.isArray(root.windowsOptions))
            return null;
        for (const option of root.windowsOptions) {
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
            if (typeof choice === "string" || typeof choice === "number") {
                values.push(choice);
            } else if (choice && typeof choice === "object"
                    && (typeof choice.value === "string"
                        || typeof choice.value === "number")) {
                values.push(choice.value);
            } else {
                return [];
            }
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
            && option.step === undefined
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

    function validateFontWeightOption(
        option, id, defaultValue, minimum, maximum
    ) {
        return option && option.id === id
            && option.type === "fontWeight"
            && option.control === "spinBox"
            && option.defaultValue === defaultValue
            && option.min === minimum
            && option.max === maximum
            && option.step === undefined
            && (option.choices === undefined
                || (Array.isArray(option.choices)
                    && option.choices.length === 0));
    }

    function validateStringOption(
        option, id, defaultValue, maximumLength
    ) {
        return option && option.id === id
            && option.type === "string"
            && option.control === "text"
            && option.defaultValue === defaultValue
            && option.min === undefined
            && option.max === undefined
            && option.step === undefined
            && option.maxLength === maximumLength
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

    function validateVectorOption(
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

    function validateEnumOption(
        option, id, defaultValue, expectedChoices, minimum, maximum
    ) {
        return option && option.id === id
            && option.type === "enum"
            && option.control === "select"
            && option.defaultValue === defaultValue
            && option.min === minimum
            && option.max === maximum
            && option.step === undefined
            && JSON.stringify(root.choiceValues(option))
                === JSON.stringify(expectedChoices);
    }

    function validateOptions() {
        if (!Array.isArray(root.windowsOptions)
                || root.windowsOptions.length
                    !== root.expectedOptionIds.length) {
            return false;
        }
        const seen = Object.create(null);
        for (let index = 0; index < root.windowsOptions.length; ++index) {
            const option = root.windowsOptions[index];
            if (!option || typeof option !== "object"
                    || typeof option.id !== "string" || seen[option.id]
                    || option.id !== root.expectedOptionIds[index]) {
                return false;
            }
            seen[option.id] = true;
        }
        return root.validateEnumOption(
                root.optionById(root.layoutId),
                root.layoutId, "dwindle",
                ["dwindle", "master", "scrolling", "monocle"],
                undefined, undefined)
            && root.validateBooleanOption(
                root.optionById(root.resizeOnBorderId),
                root.resizeOnBorderId, false)
            && root.validateIntegerOption(
                root.optionById(root.extendBorderGrabAreaId),
                root.extendBorderGrabAreaId, 15, 0, 100)
            && root.validateBooleanOption(
                root.optionById(root.hoverIconOnBorderId),
                root.hoverIconOnBorderId, true)
            && root.validateEnumOption(
                root.optionById(root.resizeCornerId),
                root.resizeCornerId, 0, [0, 1, 2, 3, 4], 0, 4)
            && root.validateBooleanOption(
                root.optionById(root.snapEnabledId),
                root.snapEnabledId, false)
            && root.validateBooleanOption(
                root.optionById(root.snapBorderOverlapId),
                root.snapBorderOverlapId, false)
            && root.validateIntegerOption(
                root.optionById(root.snapMonitorGapId),
                root.snapMonitorGapId, 10, 0, 100)
            && root.validateBooleanOption(
                root.optionById(root.snapRespectGapsId),
                root.snapRespectGapsId, false)
            && root.validateIntegerOption(
                root.optionById(root.snapWindowGapId),
                root.snapWindowGapId, 10, 0, 100)
            && root.validateEnumOption(
                root.optionById(root.followMouseId),
                root.followMouseId, 1, [0, 1, 2, 3], 0, 3)
            && root.validateBooleanOption(
                root.optionById(root.mouseRefocusId),
                root.mouseRefocusId, true)
            && root.validateIntegerOption(
                root.optionById(root.followMouseShrinkId),
                root.followMouseShrinkId, 0, 0, 300)
            && root.validateEnumOption(
                root.optionById(root.floatSwitchOverrideFocusId),
                root.floatSwitchOverrideFocusId,
                1, [0, 1, 2], 0, 2)
            && root.validateEnumOption(
                root.optionById(root.focusOnCloseId),
                root.focusOnCloseId, 0, [0, 1, 2], 0, 2)
            && root.validateBooleanOption(
                root.optionById(root.specialFallthroughId),
                root.specialFallthroughId, false)
            && root.validateBooleanOption(
                root.optionById(root.noFocusFallbackId),
                root.noFocusFallbackId, false)
            && root.validateBooleanOption(
                root.optionById(root.modalParentBlockingId),
                root.modalParentBlockingId, true)
            && root.validateCssGapOption(
                root.optionById(root.floatGapsId),
                root.floatGapsId, [0, 0, 0, 0])
            && root.validateIntegerOption(
                root.optionById(root.workspaceGapsId),
                root.workspaceGapsId, 0, 0, 100)
            && root.validateVectorOption(
                root.optionById(root.singleWindowAspectRatioId),
                root.singleWindowAspectRatioId,
                [0, 0], [0, 0], [1000, 1000])
            && root.validateNumberOption(
                root.optionById(root.singleWindowAspectRatioToleranceId),
                root.singleWindowAspectRatioToleranceId, 0.1, 0, 1)
            && root.validateNumberOption(
                root.optionById(root.dwindleDefaultSplitRatioId),
                root.dwindleDefaultSplitRatioId, 1, 0.1, 1.9)
            && root.validateEnumOption(
                root.optionById(root.dwindleForceSplitId),
                root.dwindleForceSplitId, 0, [0, 1, 2], 0, 2)
            && root.validateBooleanOption(
                root.optionById(root.dwindlePermanentDirectionOverrideId),
                root.dwindlePermanentDirectionOverrideId, false)
            && root.validateBooleanOption(
                root.optionById(root.dwindlePreciseMouseMoveId),
                root.dwindlePreciseMouseMoveId, false)
            && root.validateBooleanOption(
                root.optionById(root.dwindlePreserveSplitId),
                root.dwindlePreserveSplitId, false)
            && root.validateBooleanOption(
                root.optionById(root.dwindleSmartResizingId),
                root.dwindleSmartResizingId, true)
            && root.validateBooleanOption(
                root.optionById(root.dwindleSmartSplitId),
                root.dwindleSmartSplitId, false)
            && root.validateNumberOption(
                root.optionById(root.dwindleSpecialScaleFactorId),
                root.dwindleSpecialScaleFactorId, 1, 0, 1)
            && root.validateEnumOption(
                root.optionById(root.dwindleSplitBiasId),
                root.dwindleSplitBiasId, 0, [0, 1], 0, 1)
            && root.validateNumberOption(
                root.optionById(root.dwindleSplitWidthMultiplierId),
                root.dwindleSplitWidthMultiplierId, 1, 0.1, 3)
            && root.validateBooleanOption(
                root.optionById(root.dwindleUseActiveForSplitsId),
                root.dwindleUseActiveForSplitsId, true)
            && root.validateBooleanOption(
                root.optionById(root.masterAllowSmallSplitId),
                root.masterAllowSmallSplitId, false)
            && root.validateBooleanOption(
                root.optionById(root.masterAlwaysKeepPositionId),
                root.masterAlwaysKeepPositionId, false)
            && root.validateBooleanOption(
                root.optionById(root.masterCenterIgnoresReservedId),
                root.masterCenterIgnoresReservedId, false)
            && root.validateEnumOption(
                root.optionById(root.masterCenterFallbackId),
                root.masterCenterFallbackId, "left",
                ["left", "right", "top", "bottom"],
                undefined, undefined)
            && root.validateBooleanOption(
                root.optionById(root.masterDropAtCursorId),
                root.masterDropAtCursorId, true)
            && root.validateBooleanOption(
                root.optionById(root.masterFocusOnCloseId),
                root.masterFocusOnCloseId, false)
            && root.validateNumberOption(
                root.optionById(root.masterFactorId),
                root.masterFactorId, 0.55, 0, 1)
            && root.validateEnumOption(
                root.optionById(root.masterNewOnActiveId),
                root.masterNewOnActiveId, "none",
                ["none", "before", "after"], undefined, undefined)
            && root.validateBooleanOption(
                root.optionById(root.masterNewOnTopId),
                root.masterNewOnTopId, false)
            && root.validateEnumOption(
                root.optionById(root.masterNewStatusId),
                root.masterNewStatusId, "slave",
                ["master", "slave", "inherit"], undefined, undefined)
            && root.validateEnumOption(
                root.optionById(root.masterOrientationId),
                root.masterOrientationId, "left",
                ["left", "right", "top", "bottom", "center"],
                undefined, undefined)
            && root.validateIntegerOption(
                root.optionById(root.masterCenterSlaveCountId),
                root.masterCenterSlaveCountId, 2, 0, 10)
            && root.validateBooleanOption(
                root.optionById(root.masterSmartResizingId),
                root.masterSmartResizingId, true)
            && root.validateNumberOption(
                root.optionById(root.masterSpecialScaleFactorId),
                root.masterSpecialScaleFactorId, 1, 0, 1)
            && root.validateNumberOption(
                root.optionById(root.scrollingColumnWidthId),
                root.scrollingColumnWidthId, 0.5, 0.1, 1)
            && root.validateEnumOption(
                root.optionById(root.scrollingDirectionId),
                root.scrollingDirectionId, "right",
                ["left", "right", "up", "down"],
                undefined, undefined)
            && root.validateEnumOption(
                root.optionById(root.scrollingFocusFitMethodId),
                root.scrollingFocusFitMethodId, 1, [0, 1], 0, 1)
            && root.validateBooleanOption(
                root.optionById(root.scrollingFollowFocusId),
                root.scrollingFollowFocusId, true)
            && root.validateNumberOption(
                root.optionById(root.scrollingFollowMinimumVisibleId),
                root.scrollingFollowMinimumVisibleId, 0.4, 0, 1)
            && root.validateBooleanOption(
                root.optionById(root.scrollingFullscreenOneColumnId),
                root.scrollingFullscreenOneColumnId, true)
            && root.validateBooleanOption(
                root.optionById(root.scrollingWrapFocusId),
                root.scrollingWrapFocusId, true)
            && root.validateBooleanOption(
                root.optionById(root.scrollingWrapSwapColumnId),
                root.scrollingWrapSwapColumnId, true)
            && root.validateBooleanOption(
                root.optionById(root.scrollingMoveSnapCursorId),
                root.scrollingMoveSnapCursorId, true)
            && root.validateBooleanOption(
                root.optionById(root.scrollingMoveSnapGridId),
                root.scrollingMoveSnapGridId, true)
            && root.validateBooleanOption(
                root.optionById(root.groupAutoGroupId),
                root.groupAutoGroupId, true)
            && root.validateBooleanOption(
                root.optionById(root.groupInsertAfterCurrentId),
                root.groupInsertAfterCurrentId, true)
            && root.validateBooleanOption(
                root.optionById(root.groupFocusRemovedWindowId),
                root.groupFocusRemovedWindowId, true)
            && root.validateEnumOption(
                root.optionById(root.groupDragIntoGroupId),
                root.groupDragIntoGroupId, 1, [0, 1, 2], 0, 2)
            && root.validateBooleanOption(
                root.optionById(root.groupMergeGroupsOnDragId),
                root.groupMergeGroupsOnDragId, true)
            && root.validateBooleanOption(
                root.optionById(root.groupMergeGroupsOnGroupbarId),
                root.groupMergeGroupsOnGroupbarId, true)
            && root.validateBooleanOption(
                root.optionById(
                    root.groupMergeFloatedIntoTiledOnGroupbarId
                ),
                root.groupMergeFloatedIntoTiledOnGroupbarId, false)
            && root.validateBooleanOption(
                root.optionById(root.groupOnMoveToWorkspaceId),
                root.groupOnMoveToWorkspaceId, false)
            && root.validateBooleanOption(
                root.optionById(root.groupbarEnabledId),
                root.groupbarEnabledId, true)
            && root.validateBooleanOption(
                root.optionById(root.groupbarDisableWhenOnlyId),
                root.groupbarDisableWhenOnlyId, false)
            && root.validateStringOption(
                root.optionById(root.groupbarFontFamilyId),
                root.groupbarFontFamilyId, "", 4096)
            && root.validateFontWeightOption(
                root.optionById(root.groupbarFontWeightActiveId),
                root.groupbarFontWeightActiveId, 400, 0, 2147483647)
            && root.validateFontWeightOption(
                root.optionById(root.groupbarFontWeightInactiveId),
                root.groupbarFontWeightInactiveId, 400, 0, 2147483647)
            && root.validateIntegerOption(
                root.optionById(root.groupbarFontSizeId),
                root.groupbarFontSizeId, 8, 2, 64)
            && root.validateBooleanOption(
                root.optionById(root.groupbarGradientsId),
                root.groupbarGradientsId, false)
            && root.validateIntegerOption(
                root.optionById(root.groupbarHeightId),
                root.groupbarHeightId, 14, 1, 64)
            && root.validateIntegerOption(
                root.optionById(root.groupbarIndicatorGapId),
                root.groupbarIndicatorGapId, 0, 0, 64)
            && root.validateIntegerOption(
                root.optionById(root.groupbarIndicatorHeightId),
                root.groupbarIndicatorHeightId, 3, 1, 64)
            && root.validateBooleanOption(
                root.optionById(root.groupbarStackedId),
                root.groupbarStackedId, false)
            && root.validateIntegerOption(
                root.optionById(root.groupbarPriorityId),
                root.groupbarPriorityId, 3, 0, 6)
            && root.validateBooleanOption(
                root.optionById(root.groupbarRenderTitlesId),
                root.groupbarRenderTitlesId, true)
            && root.validateBooleanOption(
                root.optionById(root.groupbarScrollingId),
                root.groupbarScrollingId, true)
            && root.validateBooleanOption(
                root.optionById(root.groupbarMiddleClickCloseId),
                root.groupbarMiddleClickCloseId, true)
            && root.validateIntegerOption(
                root.optionById(root.groupbarRoundingId),
                root.groupbarRoundingId, 1, 0, 20)
            && root.validateNumberOption(
                root.optionById(root.groupbarRoundingPowerId),
                root.groupbarRoundingPowerId, 2, 2, 10)
            && root.validateIntegerOption(
                root.optionById(root.groupbarGradientRoundingId),
                root.groupbarGradientRoundingId, 2, 0, 20)
            && root.validateNumberOption(
                root.optionById(root.groupbarGradientRoundingPowerId),
                root.groupbarGradientRoundingPowerId, 2, 2, 10)
            && root.validateBooleanOption(
                root.optionById(root.groupbarRoundOnlyEdgesId),
                root.groupbarRoundOnlyEdgesId, true)
            && root.validateBooleanOption(
                root.optionById(root.groupbarGradientRoundOnlyEdgesId),
                root.groupbarGradientRoundOnlyEdgesId, true)
            && root.validateIntegerOption(
                root.optionById(root.groupbarGapsOutId),
                root.groupbarGapsOutId, 2, 0, 20)
            && root.validateIntegerOption(
                root.optionById(root.groupbarGapsInId),
                root.groupbarGapsInId, 2, 0, 20)
            && root.validateBooleanOption(
                root.optionById(root.groupbarKeepUpperGapId),
                root.groupbarKeepUpperGapId, true)
            && root.validateIntegerOption(
                root.optionById(root.groupbarTextOffsetId),
                root.groupbarTextOffsetId, 0, -20, 20)
            && root.validateIntegerOption(
                root.optionById(root.groupbarTextPaddingId),
                root.groupbarTextPaddingId, 0, 0, 22)
            && root.validateBooleanOption(
                root.optionById(root.groupbarBlurId),
                root.groupbarBlurId, false)
            && root.validateBooleanOption(
                root.optionById(root.allowPinFullscreenId),
                root.allowPinFullscreenId, false)
            && root.validateEnumOption(
                root.optionById(root.focusPreferredMethodId),
                root.focusPreferredMethodId, 0, [0, 1], 0, 1)
            && root.validateBooleanOption(
                root.optionById(root.ignoreGroupLockId),
                root.ignoreGroupLockId, false)
            && root.validateBooleanOption(
                root.optionById(root.movefocusCyclesFullscreenId),
                root.movefocusCyclesFullscreenId, false)
            && root.validateBooleanOption(
                root.optionById(root.movefocusCyclesGroupfirstId),
                root.movefocusCyclesGroupfirstId, false)
            && root.validateBooleanOption(
                root.optionById(root.windowDirectionMonitorFallbackId),
                root.windowDirectionMonitorFallbackId, true)
            && root.validateBooleanOption(
                root.optionById(root.anrDialogEnabledId),
                root.anrDialogEnabledId, true)
            && root.validateIntegerOption(
                root.optionById(root.anrMissedPingsId),
                root.anrMissedPingsId, 5, 1, 20)
            && root.validateBooleanOption(
                root.optionById(root.sizeLimitsTiledId),
                root.sizeLimitsTiledId, false)
            && root.validateBooleanOption(
                root.optionById(root.alwaysFollowOnDndId),
                root.alwaysFollowOnDndId, true)
            && root.validateBooleanOption(
                root.optionById(root.focusOnActivateId),
                root.focusOnActivateId, false)
            && root.validateBooleanOption(
                root.optionById(root.mouseMoveFocusesMonitorId),
                root.mouseMoveFocusesMonitorId, true)
            && root.validateEnumOption(
                root.optionById(root.onFocusUnderFullscreenId),
                root.onFocusUnderFullscreenId,
                2, [0, 1, 2], 0, 2)
            && root.validateBooleanOption(
                root.optionById(root.exitWindowRetainsFullscreenId),
                root.exitWindowRetainsFullscreenId, false)
            && root.validateBooleanOption(
                root.optionById(root.enableSwallowId),
                root.enableSwallowId, false)
            && root.validateStringOption(
                root.optionById(root.swallowRegexId),
                root.swallowRegexId, "", 4096)
            && root.validateStringOption(
                root.optionById(root.swallowExceptionRegexId),
                root.swallowExceptionRegexId, "", 4096)
            && root.validateNumberOption(
                root.optionById(root.followMouseThresholdId),
                root.followMouseThresholdId, 0, 0, 1000000)
            && root.optionById(root.followMouseThresholdId).risk === "safe";
    }

    function validateValue(option, value) {
        if (!option)
            return false;
        if (option.type === "boolean")
            return typeof value === "boolean";
        if (option.type === "integer") {
            return typeof value === "number" && Number.isFinite(value)
                && Number.isInteger(value)
                && value >= option.min && value <= option.max;
        }
        if (option.type === "fontWeight") {
            return typeof value === "number" && Number.isSafeInteger(value)
                && value >= option.min && value <= option.max;
        }
        if (option.type === "number") {
            return typeof value === "number" && Number.isFinite(value)
                && value >= option.min && value <= option.max;
        }
        if (option.type === "enum")
            return root.choiceValues(option).includes(value);
        if (option.type === "string") {
            return typeof value === "string"
                && typeof option.maxLength === "number"
                && Number.isSafeInteger(option.maxLength)
                && value.length <= option.maxLength
                && !value.includes("\u0000");
        }
        if (option.type === "cssGap") {
            return Array.isArray(value) && value.length === 4
                && value.every(part => typeof part === "number"
                    && Number.isSafeInteger(part));
        }
        if (option.type === "vector2") {
            if (!Array.isArray(value) || value.length !== 2
                    || !Array.isArray(option.min)
                    || !Array.isArray(option.max)) {
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
            return true;
        }
        return false;
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
            if (!root.validateValue(option, value))
                return false;
        }
        return true;
    }

    function optionMinimum(id) {
        const option = root.optionById(id);
        return option && typeof option.min === "number"
                && Number.isFinite(option.min) ? option.min : 0;
    }

    function optionMaximum(id) {
        const option = root.optionById(id);
        return option && typeof option.max === "number"
                && Number.isFinite(option.max) ? option.max : 0;
    }

    function optionDefault(id) {
        const option = root.optionById(id);
        return option ? option.defaultValue : undefined;
    }

    function draftValue(id) {
        return root.draftValues
            && Object.prototype.hasOwnProperty.call(root.draftValues, id)
            ? root.draftValues[id] : root.optionDefault(id);
    }

    function choiceIndex(id) {
        const choices = root.choiceValues(root.optionById(id));
        const index = choices.indexOf(root.draftValue(id));
        return index >= 0 ? index : 0;
    }

    function setChoiceFromIndex(id, index) {
        const choices = root.choiceValues(root.optionById(id));
        if (index >= 0 && index < choices.length)
            root.setDraftValue(id, choices[index]);
    }

    function layoutLabels() {
        return [qsTr("Dwindle"), qsTr("Master"), qsTr("Scrolling"), qsTr("Monocle")];
    }

    function resizeCornerLabels() {
        return [
            qsTr("Automatic"), qsTr("Top left"), qsTr("Top right"),
            qsTr("Bottom right"), qsTr("Bottom left")
        ];
    }

    function followMouseLabels() {
        return [
            qsTr("Click to focus"), qsTr("Follow pointer"),
            qsTr("Detached pointer focus"), qsTr("Separate pointer focus")
        ];
    }

    function floatSwitchFocusLabels() {
        return [
            qsTr("Keep current focus"),
            qsTr("Tiled and floating transitions"),
            qsTr("All floating transitions")
        ];
    }

    function focusOnCloseLabels() {
        return [
            qsTr("Next window"), qsTr("Window under pointer"),
            qsTr("Most recently used")
        ];
    }

    function focusPreferredMethodLabels() {
        return [qsTr("Recent focus"), qsTr("Longest shared edge")];
    }

    function focusUnderFullscreenLabels() {
        return [
            qsTr("Keep current mode"),
            qsTr("Transfer current mode"),
            qsTr("Exit current mode")
        ];
    }

    function dwindleForceSplitLabels() {
        return [
            qsTr("Automatic"), qsTr("Left or top"),
            qsTr("Right or bottom")
        ];
    }

    function dwindleSplitBiasLabels() {
        return [qsTr("Split direction"), qsTr("Current window")];
    }

    function masterOrientationLabels() {
        return [
            qsTr("Left"), qsTr("Right"), qsTr("Top"),
            qsTr("Bottom"), qsTr("Center")
        ];
    }

    function masterCenterFallbackLabels() {
        return [qsTr("Left"), qsTr("Right"), qsTr("Top"), qsTr("Bottom")];
    }

    function masterNewOnActiveLabels() {
        return [
            qsTr("Normal placement"), qsTr("Before focused window"),
            qsTr("After focused window")
        ];
    }

    function masterNewStatusLabels() {
        return [qsTr("Master"), qsTr("Stack"), qsTr("Inherit")];
    }

    function scrollingDirectionLabels() {
        return [qsTr("Left"), qsTr("Right"), qsTr("Up"), qsTr("Down")];
    }

    function scrollingFitLabels() {
        return [qsTr("Center"), qsTr("Fit")];
    }

    function groupDragIntoGroupLabels() {
        return [
            qsTr("Disabled"), qsTr("Window or group bar"),
            qsTr("Group bar only")
        ];
    }

    function formattedNumber(value) {
        const numeric = Number(value);
        return Number.isFinite(numeric) ? String(numeric) : "";
    }

    function vectorComponent(id, index) {
        const value = root.draftValue(id);
        return Array.isArray(value) && index >= 0 && index < value.length
            ? value[index] : 0;
    }

    function setVectorComponent(id, index, text) {
        if (!root.controlsEnabled || index < 0 || index > 1)
            return false;
        const value = Number(text);
        const option = root.optionById(id);
        if (!option || !Number.isFinite(value)
                || !Array.isArray(option.min)
                || !Array.isArray(option.max)
                || value < option.min[index] || value > option.max[index]) {
            return false;
        }
        const next = root.clone(root.draftValue(id));
        if (!Array.isArray(next) || next.length !== 2)
            return false;
        next[index] = value;
        root.setDraftValue(id, next);
        return root.valueEqual(root.draftValue(id), next);
    }

    function gapComponent(index) {
        const value = root.draftValue(root.floatGapsId);
        return Array.isArray(value) && index >= 0 && index < value.length
            ? value[index] : 0;
    }

    function setGapComponent(index, text) {
        if (!root.controlsEnabled || index < 0 || index > 3
                || !/^-?(0|[1-9][0-9]*)$/.test(String(text))) {
            return false;
        }
        const value = Number(text);
        if (!Number.isSafeInteger(value))
            return false;
        const next = root.clone(root.draftValue(root.floatGapsId));
        if (!Array.isArray(next) || next.length !== 4)
            return false;
        next[index] = value;
        root.setDraftValue(root.floatGapsId, next);
        return root.valueEqual(root.draftValue(root.floatGapsId), next);
    }

    function setStringValue(id, value) {
        if (!root.controlsEnabled || typeof value !== "string")
            return false;
        root.setDraftValue(id, value);
        return root.draftValue(id) === value;
    }

    function setDraftValue(id, value) {
        if (!root.controlsEnabled || !root.trustedDefinitionsValid)
            return;
        const option = root.optionById(id);
        if (!root.validateValue(option, value))
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
                || id !== root.followMouseThresholdId
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

    function synchronizeDraft() {
        if (!root.serviceAvailable || !root.windowsProjectionAvailable
                || !root.revisionTokenValid
                || !root.trustedValuesValid
                || root.busy || root.sharedMutationBusy) {
            return;
        }
        const next = root.clone(root.windowsValues);
        if (!next)
            return;
        root.synchronizedValues = root.clone(next);
        root.draftValues = next;
        root.synchronizedRevisionToken = root.revisionToken;
        root.projectionInitialized = true;
        root.externalChangeWhileEditing = false;
        root.saveSubmitted = false;
        root.submittedValues = ({});
        root.submittedRevisionToken = "";
    }

    function resetTargetValues() {
        if (!root.trustedDefinitionsValid)
            return null;
        const defaults = {};
        for (const id of root.expectedOptionIds) {
            defaults[id] = root.clone(root.optionDefault(id));
            if (defaults[id] === null && root.optionDefault(id) !== null)
                return null;
        }
        return root.validateValues(defaults) ? defaults : null;
    }

    function resetDraftToDefaults() {
        if (!root.controlsEnabled || !root.trustedDefinitionsValid)
            return;
        const target = root.resetTargetValues();
        if (target)
            root.draftValues = target;
    }

    function submitDraft() {
        if (!root.saveEnabled)
            return;
        const candidate = root.clone(root.draftValues);
        if (!candidate || !root.validateValues(candidate))
            return;
        root.saveSubmitted = true;
        root.submittedValues = root.clone(candidate);
        root.submittedRevisionToken = root.revisionToken;
        root.saveRequested(candidate);
        root.scheduleProjectionReview();
    }

    function reviewProjection() {
        if (!root.serviceAvailable || !root.trustedValuesValid) {
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
        if (root.sharedMutationBusy)
            return;
        if (!root.projectionInitialized) {
            root.synchronizeDraft();
            return;
        }
        if (root.saveSubmitted) {
            if (root.busy)
                return;
            if (root.valuesEqual(root.windowsValues, root.submittedValues)) {
                root.synchronizeDraft();
                return;
            }
            if (root.revisionToken === root.submittedRevisionToken) {
                root.saveSubmitted = false;
                root.submittedValues = ({});
                root.submittedRevisionToken = "";
                return;
            }
            root.saveSubmitted = false;
            root.externalChangeWhileEditing = true;
            return;
        }
        const projectionChanged = root.revisionToken
                !== root.synchronizedRevisionToken
            || !root.valuesEqual(root.windowsValues, root.synchronizedValues);
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

    onWindowsOptionsChanged: root.scheduleProjectionReview()
    onWindowsValuesChanged: root.scheduleProjectionReview()
    onWindowsProjectionAvailableChanged: root.scheduleProjectionReview()
    onRevisionTokenChanged: root.scheduleProjectionReview()
    onServiceAvailableChanged: root.scheduleProjectionReview()
    onBusyChanged: {
        root.scheduleProjectionReview();
        if (windowsRecoveryDialog.opened && root.busy)
            windowsRecoveryDialog.close();
    }
    onSharedMutationBusyChanged: root.scheduleProjectionReview()
    onWindowsErrorNameChanged: root.scheduleProjectionReview()
    onWindowsErrorMessageChanged: root.scheduleProjectionReview()
    onSharedErrorNameChanged: root.scheduleProjectionReview()
    onSharedErrorMessageChanged: root.scheduleProjectionReview()
    onApplyStateChanged: root.scheduleProjectionReview()
    onRecoveryAvailableChanged: {
        if (windowsRecoveryDialog.opened && !root.recoveryAvailable)
            windowsRecoveryDialog.close();
    }
    onStatusIsDangerChanged: {
        if (!root.statusIsDanger)
            return;
        Qt.callLater(function() {
            if (root.statusIsDanger)
                windowsOptionsScrollView.contentItem.contentY = 0;
        });
    }
    Component.onCompleted: root.scheduleProjectionReview()

    background: Rectangle { color: root.palette.window }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: root.compactPreview
            ? Math.min(root.contentTopMargin, 4)
            : root.contentTopMargin
        spacing: root.compactPreview ? 4 : 20

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: stickyPreview.implicitHeight
            Layout.minimumHeight: stickyPreview.implicitHeight

            Frame {
                id: stickyPreview

                objectName: "windowsLayoutStickyPreview"
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.max(0, Math.min(parent.width - 48, 980))
                padding: root.compactPreview ? 6 : 18

                background: Rectangle {
                    color: root.palette.base
                    radius: 16
                    border.color: root.palette.mid
                }

                contentItem: ColumnLayout {
                    spacing: root.compactPreview ? 4 : 12

                    RowLayout {
                        Layout.fillWidth: true
                        visible: !root.compactPreview

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Layout preview")
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
                        objectName: "windowsLayoutPreview"
                        Layout.fillWidth: true
                        borderSize: 1
                        rounding: 10
                        blurEnabled: true
                        shadowEnabled: true
                        animationsEnabled: root.previewAnimationsEnabled
                        layoutMode: String(
                            root.draftValue(root.layoutId) || "dwindle"
                        )
                        resizeOnBorder:
                            root.draftValue(root.resizeOnBorderId) === true
                        snapEnabled:
                            root.draftValue(root.snapEnabledId) === true
                        summaryMode: "windows"
                        motionButtonObjectName:
                            "toggleWindowsLayoutMotionButton"
                    }

                    Label {
                        objectName: "windowsLayoutPreviewDisclaimer"
                        Layout.fillWidth: true
                        text: root.compactPreview
                            ? qsTr("Illustrative only. Engine details, spacing, fullscreen, focus, grouping, group bars, and swallowing are not simulated.")
                            : qsTr("Illustrative layout-family preview. Engine details, spacing, fullscreen, focus, grouping, group-bar appearance or interaction, and swallowing are saved to Hyprland but are not simulated here.")
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
            id: windowsOptionsScrollView

            objectName: "windowsOptionsScrollView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                objectName: "windowsOptionsContent"
                x: Math.max(24, (root.width - width) / 2)
                width: Math.max(0, Math.min(root.width - 48, 980))
                spacing: root.compactPreview ? 16 : 20

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 3

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Windows & Layout")
                            color: root.palette.text
                            font.pixelSize: 28
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Choose window layouts and tune spacing, grouping, group bars, resizing, snapping, and focus behavior.")
                            color: root.palette.placeholderText
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }

                    Button {
                        objectName: "refreshWindowsButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Refresh")
                        enabled: !root.busy && !root.displayTestActive
                        icon.name: "view-refresh-symbolic"
                        Accessible.name: qsTr("Refresh compositor window settings")

                        onClicked: root.refreshRequested()
                    }
                }

                Frame {
                    objectName: "windowsStatusCard"
                    Layout.fillWidth: true
                    visible: root.statusVisible
                    padding: 16

                    background: Rectangle {
                        color: root.statusIsDanger ? ShellTheme.errorContainer : ShellTheme.warningContainer
                        radius: 12
                        border.color: root.statusIsDanger
                            ? ShellTheme.errorOutline : ShellTheme.warningOutline
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        Label {
                            objectName: "windowsStatusMessage"
                            Layout.fillWidth: true
                            text: root.statusMessage
                            color: root.statusIsDanger
                                ? ShellTheme.onErrorContainer : ShellTheme.onWarningContainer
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
                                objectName: "windowsOpenDisplaysButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                visible: root.serviceAvailable
                                    && root.managementState === "unmanaged"
                                text: qsTr("Review takeover in Displays")
                                enabled: !root.busy
                                Accessible.name: qsTr("Open Displays to review compositor takeover")

                                onClicked: root.openDisplaysRequested()
                            }

                            Button {
                                objectName: "loadCurrentWindowsButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                visible: root.externalChangeWhileEditing
                                text: qsTr("Load current settings")
                                enabled: !root.busy
                                    && !root.sharedMutationBusy
                                    && !root.saveSubmitted
                                    && root.windowsProjectionAvailable
                                    && root.trustedValuesValid
                                Accessible.name: qsTr("Discard this Windows & Layout draft and load the current compositor settings")

                                onClicked: root.synchronizeDraft()
                            }

                            Button {
                                objectName: "retryApplyWindowsButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                visible: root.retryApplyAvailable
                                text: root.busyOperation === "compositor-apply"
                                    || root.busyOperation === "windows-apply"
                                    ? qsTr("Retrying apply…")
                                    : qsTr("Retry apply")
                                enabled: root.retryApplyAvailable && !root.busy
                                    && !root.sharedMutationBusy
                                    && root.sharedApplySafe
                                Accessible.name: qsTr("Retry applying the exact saved compositor revision")

                                onClicked: root.retryApplyRequested()
                            }

                            Button {
                                objectName: "recoverWindowsButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                visible: root.recoveryAvailable
                                text: qsTr("Restore last working configuration")
                                enabled: root.recoveryAvailable && !root.busy
                                    && !root.sharedMutationBusy
                                Accessible.name: qsTr("Review whole-compositor recovery")

                                onClicked: windowsRecoveryDialog.open()
                            }
                        }
                    }
                }

                Frame {
                    objectName: "windowsLayoutCard"
                    Layout.fillWidth: true
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
                            text: qsTr("Layout")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsSelectRow {
                            Layout.fillWidth: true
                            title: qsTr("Default layout")
                            description: qsTr("Choose the layout used when a workspace has no more specific rule.")
                            model: root.layoutLabels()
                            currentIndex: root.choiceIndex(root.layoutId)
                            controlWidth: 150
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsDefaultLayout"
                            accessibleName: qsTr("Default window layout")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: index =>
                                root.setChoiceFromIndex(root.layoutId, index)
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Allow pinned windows to go fullscreen")
                            description: qsTr("Allow a later fullscreen request to temporarily unpin a pinned window, then restore its pin when fullscreen ends.")
                            checked: root.draftValue(
                                root.allowPinFullscreenId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsAllowPinFullscreen"
                            accessibleName: qsTr("Allow pinned windows to enter fullscreen and restore their pin afterward")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.allowPinFullscreenId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Apply size rules to tiled windows")
                            description: qsTr("Apply matching Window Rule minimum and maximum sizes to tiled windows. A constrained window is centered inside its assigned tile without redistributing the layout, and maximum size is ignored while fullscreen or maximized.")
                            checked: root.draftValue(
                                root.sizeLimitsTiledId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsSizeLimitsTiled"
                            accessibleName: qsTr("Apply Window Rule size limits to tiled windows")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.sizeLimitsTiledId, value
                            )
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Single-window aspect ratio")
                                color: root.palette.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Constrain a lone tiled window to this width-to-height ratio. Set both values to 0 to turn the constraint off.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 12
                                rowSpacing: 8

                                Repeater {
                                    model: [
                                        {
                                            label: qsTr("Width"),
                                            name: "windowsAspectRatioWidth"
                                        },
                                        {
                                            label: qsTr("Height"),
                                            name: "windowsAspectRatioHeight"
                                        }
                                    ]

                                    ColumnLayout {
                                        required property int index
                                        required property var modelData
                                        property string projectedText:
                                            root.formattedNumber(
                                                root.vectorComponent(
                                                    root.singleWindowAspectRatioId,
                                                    index
                                                )
                                            )

                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: 4

                                        Label {
                                            Layout.fillWidth: true
                                            text: parent.modelData.label
                                            color: root.palette.placeholderText
                                            font.pixelSize: 12
                                            textFormat: Text.PlainText
                                        }

                                        TextField {
                                            id: aspectRatioField

                                            objectName: parent.modelData.name
                                            Layout.fillWidth: true
                                            implicitHeight: root.minimumTargetSize
                                            enabled: root.controlsEnabled
                                            inputMethodHints:
                                                Qt.ImhFormattedNumbersOnly
                                            Accessible.name: qsTr("Single-window aspect ratio %1").arg(parent.modelData.label)
                                            validator: DoubleValidator {
                                                bottom: 0
                                                top: 1000
                                                notation:
                                                    DoubleValidator.StandardNotation
                                            }

                                            Component.onCompleted:
                                                text = parent.projectedText
                                            onActiveFocusChanged: {
                                                if (!activeFocus)
                                                    text = parent.projectedText;
                                            }
                                            onEditingFinished: {
                                                if (!root.setVectorComponent(
                                                        root.singleWindowAspectRatioId,
                                                        parent.index,
                                                        text)) {
                                                    text = parent.projectedText;
                                                }
                                            }
                                        }

                                        onProjectedTextChanged: {
                                            if (!aspectRatioField.activeFocus)
                                                aspectRatioField.text = projectedText;
                                        }
                                    }
                                }
                            }

                            Label {
                                objectName: "windowsAspectRatioError"
                                Layout.fillWidth: true
                                visible: root.projectionInitialized
                                    && !root.aspectRatioCoherent
                                text: qsTr("Width and height must both be 0, or both be greater than 0, before this draft can be saved.")
                                color: ShellTheme.onErrorContainer
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.AlertMessage
                                Accessible.name: text
                            }
                        }

                        SettingsSliderRow {
                            Layout.fillWidth: true
                            title: qsTr("Aspect-ratio tolerance")
                            description: qsTr("Leave small differences alone until the ratio adjustment exceeds this fraction of the available area.")
                            from: root.optionMinimum(
                                root.singleWindowAspectRatioToleranceId
                            )
                            to: root.optionMaximum(
                                root.singleWindowAspectRatioToleranceId
                            )
                            value: Number(root.draftValue(
                                root.singleWindowAspectRatioToleranceId
                            )) || 0
                            stepSize: 0.05
                            decimals: 2
                            enabled: root.controlsEnabled
                                && root.aspectRatioEnabled
                            controlObjectName:
                                "windowsAspectRatioTolerance"
                            valueObjectName:
                                "windowsAspectRatioToleranceValue"
                            accessibleName:
                                qsTr("Single-window aspect ratio tolerance")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.singleWindowAspectRatioToleranceId,
                                value
                            )
                        }
                    }
                }

                Frame {
                    objectName: "windowsSpacingCard"
                    Layout.fillWidth: true
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
                            text: qsTr("Spacing")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Floating-window edge gaps")
                                color: root.palette.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Set the signed gap between floating windows and each monitor edge.")
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
                                        { label: qsTr("Top"), name: "windowsFloatGapTop" },
                                        { label: qsTr("Right"), name: "windowsFloatGapRight" },
                                        { label: qsTr("Bottom"), name: "windowsFloatGapBottom" },
                                        { label: qsTr("Left"), name: "windowsFloatGapLeft" }
                                    ]

                                    ColumnLayout {
                                        required property int index
                                        required property var modelData
                                        property string projectedText:
                                            String(root.gapComponent(index))

                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: 4

                                        Label {
                                            Layout.fillWidth: true
                                            text: parent.modelData.label
                                            color: root.palette.placeholderText
                                            font.pixelSize: 12
                                            textFormat: Text.PlainText
                                        }

                                        TextField {
                                            id: floatGapField

                                            objectName: parent.modelData.name
                                            Layout.fillWidth: true
                                            implicitHeight: root.minimumTargetSize
                                            enabled: root.controlsEnabled
                                            inputMethodHints:
                                                Qt.ImhFormattedNumbersOnly
                                            Accessible.name: qsTr("Floating-window %1 edge gap").arg(parent.modelData.label)
                                            validator: RegularExpressionValidator {
                                                regularExpression:
                                                    /^-?(0|[1-9][0-9]*)$/
                                            }

                                            Component.onCompleted:
                                                text = parent.projectedText
                                            onActiveFocusChanged: {
                                                if (!activeFocus)
                                                    text = parent.projectedText;
                                            }
                                            onEditingFinished: {
                                                if (!root.setGapComponent(
                                                        parent.index, text)) {
                                                    text = parent.projectedText;
                                                }
                                            }
                                        }

                                        onProjectedTextChanged: {
                                            if (!floatGapField.activeFocus)
                                                floatGapField.text = projectedText;
                                        }
                                    }
                                }
                            }
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Space between workspace slides")
                            description: qsTr("Add this many layout pixels between workspaces during animated transitions. This stacks with ordinary outer gaps.")
                            from: root.optionMinimum(root.workspaceGapsId)
                            to: root.optionMaximum(root.workspaceGapsId)
                            value: Number(root.draftValue(
                                root.workspaceGapsId
                            )) || 0
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsWorkspaceGaps"
                            accessibleName:
                                qsTr("Gap between workspace slides in layout pixels")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.workspaceGapsId, value
                            )
                        }
                    }
                }

                Frame {
                    objectName: "windowsEngineCard"
                    Layout.fillWidth: true
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
                            text: qsTr("Layout-specific behavior")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Choose which layout engine to configure. This does not change the default layout, and the values also apply when a workspace rule selects that engine.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }

                        TabBar {
                            objectName: "windowsEngineTabs"
                            Layout.fillWidth: true
                            currentIndex: root.engineTabIndex

                            onCurrentIndexChanged: {
                                if (root.engineTabIndex !== currentIndex)
                                    root.engineTabIndex = currentIndex;
                            }

                            TabButton {
                                objectName: "windowsDwindleTab"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Dwindle")
                                Accessible.name:
                                    qsTr("Configure Dwindle layout")
                            }

                            TabButton {
                                objectName: "windowsMasterTab"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Master")
                                Accessible.name:
                                    qsTr("Configure Master layout")
                            }

                            TabButton {
                                objectName: "windowsScrollingTab"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Scrolling")
                                Accessible.name:
                                    qsTr("Configure Scrolling layout")
                            }
                        }

                        StackLayout {
                            Layout.fillWidth: true
                            currentIndex: root.engineTabIndex

                            ColumnLayout {
                                objectName: "windowsDwindleSettings"
                                Layout.fillWidth: true
                                spacing: 18

                                SettingsSliderRow {
                                    Layout.fillWidth: true
                                    title: qsTr("New-window split ratio")
                                    description: qsTr("Choose how much of the split a newly opened window receives. A value of 1 makes an even split.")
                                    from: root.optionMinimum(
                                        root.dwindleDefaultSplitRatioId
                                    )
                                    to: root.optionMaximum(
                                        root.dwindleDefaultSplitRatioId
                                    )
                                    value: Number(root.draftValue(
                                        root.dwindleDefaultSplitRatioId
                                    )) || 0
                                    stepSize: 0.05
                                    decimals: 2
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsDwindleDefaultSplitRatio"
                                    valueObjectName:
                                        "windowsDwindleDefaultSplitRatioValue"
                                    accessibleName:
                                        qsTr("Dwindle new-window split ratio")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.dwindleDefaultSplitRatioId,
                                            value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Choose split from pointer position")
                                    description: qsTr("Use the pointer's position inside the target window to choose the new split direction.")
                                    checked: root.draftValue(
                                        root.dwindleSmartSplitId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsDwindleSmartSplit"
                                    accessibleName:
                                        qsTr("Choose Dwindle splits from the pointer position")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.dwindleSmartSplitId, value
                                        )
                                }

                                SettingsSelectRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Forced split direction")
                                    description: qsTr("Choose automatic placement or force new windows toward the first or second side of a split.")
                                    model: root.dwindleForceSplitLabels()
                                    currentIndex: root.choiceIndex(
                                        root.dwindleForceSplitId
                                    )
                                    controlWidth: 160
                                    enabled: root.dwindleForceSplitEnabled
                                    controlObjectName:
                                        "windowsDwindleForceSplit"
                                    accessibleName:
                                        qsTr("Dwindle forced split direction")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: index =>
                                        root.setChoiceFromIndex(
                                            root.dwindleForceSplitId, index
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Split the active window")
                                    description: qsTr("Prefer the active window instead of the window under the pointer when choosing what to divide.")
                                    checked: root.draftValue(
                                        root.dwindleUseActiveForSplitsId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsDwindleUseActiveForSplits"
                                    accessibleName:
                                        qsTr("Split the active window in Dwindle")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.dwindleUseActiveForSplitsId,
                                            value
                                        )
                                }

                                SettingsSelectRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Split-ratio bias")
                                    description: qsTr("Choose whether the configured split ratio favors the split direction or the current window.")
                                    model: root.dwindleSplitBiasLabels()
                                    currentIndex: root.choiceIndex(
                                        root.dwindleSplitBiasId
                                    )
                                    controlWidth: 160
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsDwindleSplitBias"
                                    accessibleName:
                                        qsTr("Dwindle split-ratio bias")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: index =>
                                        root.setChoiceFromIndex(
                                            root.dwindleSplitBiasId, index
                                        )
                                }

                                SettingsSliderRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Horizontal split threshold")
                                    description: qsTr("Require a wider container before Dwindle chooses a side-by-side split.")
                                    from: root.optionMinimum(
                                        root.dwindleSplitWidthMultiplierId
                                    )
                                    to: root.optionMaximum(
                                        root.dwindleSplitWidthMultiplierId
                                    )
                                    value: Number(root.draftValue(
                                        root.dwindleSplitWidthMultiplierId
                                    )) || 0
                                    stepSize: 0.05
                                    decimals: 2
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsDwindleSplitWidthMultiplier"
                                    valueObjectName:
                                        "windowsDwindleSplitWidthMultiplierValue"
                                    accessibleName:
                                        qsTr("Dwindle horizontal split threshold")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.dwindleSplitWidthMultiplierId,
                                            value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Preserve split orientation")
                                    description: qsTr("Keep each container's split orientation as windows are added, moved, or removed.")
                                    checked: root.draftValue(
                                        root.dwindlePreserveSplitId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsDwindlePreserveSplit"
                                    accessibleName:
                                        qsTr("Preserve Dwindle split orientation")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.dwindlePreserveSplitId, value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Keep manual preselection")
                                    description: qsTr("Keep a manually selected split direction after the next window opens.")
                                    checked: root.draftValue(
                                        root.dwindlePermanentDirectionOverrideId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsDwindlePermanentDirectionOverride"
                                    accessibleName:
                                        qsTr("Keep Dwindle manual split preselection")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.dwindlePermanentDirectionOverrideId,
                                            value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Pointer-aware resizing")
                                    description: qsTr("Choose the Dwindle resize direction from the pointer's position on the window.")
                                    checked: root.draftValue(
                                        root.dwindleSmartResizingId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsDwindleSmartResizing"
                                    accessibleName:
                                        qsTr("Use pointer-aware Dwindle resizing")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.dwindleSmartResizingId, value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Precise drag placement")
                                    description: qsTr("Use the exact pointer position to choose where a moved window is inserted.")
                                    checked: root.draftValue(
                                        root.dwindlePreciseMouseMoveId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsDwindlePreciseMouseMove"
                                    accessibleName:
                                        qsTr("Use precise Dwindle drag placement")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.dwindlePreciseMouseMoveId,
                                            value
                                        )
                                }

                                SettingsSliderRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Special-workspace scale")
                                    description: qsTr("Scale Dwindle windows on special workspaces by this factor.")
                                    from: root.optionMinimum(
                                        root.dwindleSpecialScaleFactorId
                                    )
                                    to: root.optionMaximum(
                                        root.dwindleSpecialScaleFactorId
                                    )
                                    value: Number(root.draftValue(
                                        root.dwindleSpecialScaleFactorId
                                    )) || 0
                                    stepSize: 0.05
                                    decimals: 2
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsDwindleSpecialScaleFactor"
                                    valueObjectName:
                                        "windowsDwindleSpecialScaleFactorValue"
                                    accessibleName:
                                        qsTr("Dwindle special-workspace scale factor")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.dwindleSpecialScaleFactorId,
                                            value
                                        )
                                }
                            }

                            ColumnLayout {
                                objectName: "windowsMasterSettings"
                                Layout.fillWidth: true
                                spacing: 18

                                SettingsSelectRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Master position")
                                    description: qsTr("Place the master area at an edge or in the center.")
                                    model: root.masterOrientationLabels()
                                    currentIndex: root.choiceIndex(
                                        root.masterOrientationId
                                    )
                                    controlWidth: 150
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsMasterOrientation"
                                    accessibleName:
                                        qsTr("Master layout position")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: index =>
                                        root.setChoiceFromIndex(
                                            root.masterOrientationId, index
                                        )
                                }

                                SettingsSliderRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Master-area share")
                                    description: qsTr("Set the fraction of available space assigned to the master area.")
                                    from: root.optionMinimum(root.masterFactorId)
                                    to: root.optionMaximum(root.masterFactorId)
                                    value: Number(root.draftValue(
                                        root.masterFactorId
                                    )) || 0
                                    stepSize: 0.05
                                    decimals: 2
                                    enabled: root.controlsEnabled
                                    controlObjectName: "windowsMasterFactor"
                                    valueObjectName:
                                        "windowsMasterFactorValue"
                                    accessibleName:
                                        qsTr("Master-area size fraction")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.masterFactorId, value
                                        )
                                }

                                SettingsSelectRow {
                                    Layout.fillWidth: true
                                    title: qsTr("New-window role")
                                    description: qsTr("Make a new window a master, add it to the stack, or inherit the focused window's role.")
                                    model: root.masterNewStatusLabels()
                                    currentIndex: root.choiceIndex(
                                        root.masterNewStatusId
                                    )
                                    controlWidth: 150
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsMasterNewStatus"
                                    accessibleName:
                                        qsTr("New-window role in Master layout")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: index =>
                                        root.setChoiceFromIndex(
                                            root.masterNewStatusId, index
                                        )
                                }

                                SettingsSelectRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Place near focused window")
                                    description: qsTr("Use normal stack placement or insert a new stack window before or after the focused window.")
                                    model: root.masterNewOnActiveLabels()
                                    currentIndex: root.choiceIndex(
                                        root.masterNewOnActiveId
                                    )
                                    controlWidth: 180
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsMasterNewOnActive"
                                    accessibleName:
                                        qsTr("Master layout relative new-window placement")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: index =>
                                        root.setChoiceFromIndex(
                                            root.masterNewOnActiveId, index
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Add to top of stack")
                                    description: qsTr("Put new windows at the start of the applicable stack when relative placement does not decide the position.")
                                    checked: root.draftValue(
                                        root.masterNewOnTopId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsMasterNewOnTop"
                                    accessibleName:
                                        qsTr("Add new Master-layout windows to the top of the stack")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.masterNewOnTopId, value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Drop moved windows at pointer")
                                    description: qsTr("Insert a dragged window where the pointer is released instead of using normal stack placement.")
                                    checked: root.draftValue(
                                        root.masterDropAtCursorId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsMasterDropAtCursor"
                                    accessibleName:
                                        qsTr("Drop moved Master-layout windows at the pointer")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.masterDropAtCursorId, value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Focus master after closing")
                                    description: qsTr("Move focus to the master window after another window closes.")
                                    checked: root.draftValue(
                                        root.masterFocusOnCloseId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsMasterFocusOnClose"
                                    accessibleName:
                                        qsTr("Focus the master window after closing")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.masterFocusOnCloseId, value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Split multiple masters")
                                    description: qsTr("Allow additional master windows to share a smaller split area.")
                                    checked: root.draftValue(
                                        root.masterAllowSmallSplitId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsMasterAllowSmallSplit"
                                    accessibleName:
                                        qsTr("Allow small splits for multiple master windows")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.masterAllowSmallSplitId, value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Keep master placement when alone")
                                    description: qsTr("Retain the configured master position and size when only one tiled window remains.")
                                    checked: root.draftValue(
                                        root.masterAlwaysKeepPositionId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsMasterAlwaysKeepPosition"
                                    accessibleName:
                                        qsTr("Keep Master layout position with one window")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.masterAlwaysKeepPositionId,
                                            value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Pointer-aware resizing")
                                    description: qsTr("Choose the Master resize direction from the pointer's position on the window.")
                                    checked: root.draftValue(
                                        root.masterSmartResizingId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsMasterSmartResizing"
                                    accessibleName:
                                        qsTr("Use pointer-aware Master resizing")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.masterSmartResizingId, value
                                        )
                                }

                                SettingsSliderRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Special-workspace scale")
                                    description: qsTr("Scale Master windows on special workspaces by this factor.")
                                    from: root.optionMinimum(
                                        root.masterSpecialScaleFactorId
                                    )
                                    to: root.optionMaximum(
                                        root.masterSpecialScaleFactorId
                                    )
                                    value: Number(root.draftValue(
                                        root.masterSpecialScaleFactorId
                                    )) || 0
                                    stepSize: 0.05
                                    decimals: 2
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsMasterSpecialScaleFactor"
                                    valueObjectName:
                                        "windowsMasterSpecialScaleFactorValue"
                                    accessibleName:
                                        qsTr("Master special-workspace scale factor")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.masterSpecialScaleFactorId,
                                            value
                                        )
                                }

                                SettingsSpinBoxRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Windows before centered master")
                                    description: qsTr("Center the master only after at least this many stack windows are open.")
                                    from: root.optionMinimum(
                                        root.masterCenterSlaveCountId
                                    )
                                    to: root.optionMaximum(
                                        root.masterCenterSlaveCountId
                                    )
                                    value: Number(root.draftValue(
                                        root.masterCenterSlaveCountId
                                    )) || 0
                                    enabled: root.masterCenterChildrenEnabled
                                    controlObjectName:
                                        "windowsMasterCenterSlaveCount"
                                    accessibleName:
                                        qsTr("Minimum stack windows for a centered master")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.masterCenterSlaveCountId,
                                            value
                                        )
                                }

                                SettingsSelectRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Centered-master fallback")
                                    description: qsTr("Place the master on this side until enough stack windows exist to center it.")
                                    model: root.masterCenterFallbackLabels()
                                    currentIndex: root.choiceIndex(
                                        root.masterCenterFallbackId
                                    )
                                    controlWidth: 150
                                    enabled: root.masterCenterChildrenEnabled
                                    controlObjectName:
                                        "windowsMasterCenterFallback"
                                    accessibleName:
                                        qsTr("Centered Master fallback side")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: index =>
                                        root.setChoiceFromIndex(
                                            root.masterCenterFallbackId, index
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Ignore reserved side edges")
                                    description: qsTr("Allow a centered master arrangement to span monitor side areas reserved by shell surfaces.")
                                    checked: root.draftValue(
                                        root.masterCenterIgnoresReservedId
                                    ) === true
                                    enabled: root.masterCenterChildrenEnabled
                                    controlObjectName:
                                        "windowsMasterCenterIgnoresReserved"
                                    accessibleName:
                                        qsTr("Ignore reserved side edges for centered Master layout")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.masterCenterIgnoresReservedId,
                                            value
                                        )
                                }
                            }

                            ColumnLayout {
                                objectName: "windowsScrollingSettings"
                                Layout.fillWidth: true
                                spacing: 18

                                SettingsSelectRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Scroll direction")
                                    description: qsTr("Choose the direction in which new columns appear and the layout moves.")
                                    model: root.scrollingDirectionLabels()
                                    currentIndex: root.choiceIndex(
                                        root.scrollingDirectionId
                                    )
                                    controlWidth: 150
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsScrollingDirection"
                                    accessibleName:
                                        qsTr("Scrolling layout direction")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: index =>
                                        root.setChoiceFromIndex(
                                            root.scrollingDirectionId, index
                                        )
                                }

                                SettingsSliderRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Default column width")
                                    description: qsTr("Set the fraction of the usable area assigned to each new column.")
                                    from: root.optionMinimum(
                                        root.scrollingColumnWidthId
                                    )
                                    to: root.optionMaximum(
                                        root.scrollingColumnWidthId
                                    )
                                    value: Number(root.draftValue(
                                        root.scrollingColumnWidthId
                                    )) || 0
                                    stepSize: 0.05
                                    decimals: 2
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsScrollingColumnWidth"
                                    valueObjectName:
                                        "windowsScrollingColumnWidthValue"
                                    accessibleName:
                                        qsTr("Scrolling default column width")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.scrollingColumnWidthId, value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Fill one-column workspaces")
                                    description: qsTr("Let a single Scrolling column use the entire available area.")
                                    checked: root.draftValue(
                                        root.scrollingFullscreenOneColumnId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsScrollingFullscreenOneColumn"
                                    accessibleName:
                                        qsTr("Fill Scrolling workspaces with one column")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.scrollingFullscreenOneColumnId,
                                            value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Follow focused columns")
                                    description: qsTr("Move the Scrolling view automatically to bring an ordinarily focused column into view.")
                                    checked: root.draftValue(
                                        root.scrollingFollowFocusId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsScrollingFollowFocus"
                                    accessibleName:
                                        qsTr("Follow focused Scrolling columns")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.scrollingFollowFocusId, value
                                        )
                                }

                                SettingsSliderRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Minimum visible share")
                                    description: qsTr("Move for ordinary focus only when less than this fraction of the target window is visible.")
                                    from: root.optionMinimum(
                                        root.scrollingFollowMinimumVisibleId
                                    )
                                    to: root.optionMaximum(
                                        root.scrollingFollowMinimumVisibleId
                                    )
                                    value: Number(root.draftValue(
                                        root.scrollingFollowMinimumVisibleId
                                    )) || 0
                                    stepSize: 0.05
                                    decimals: 2
                                    enabled:
                                        root.scrollingFollowChildrenEnabled
                                    controlObjectName:
                                        "windowsScrollingFollowMinimumVisible"
                                    valueObjectName:
                                        "windowsScrollingFollowMinimumVisibleValue"
                                    accessibleName:
                                        qsTr("Minimum visible Scrolling window share")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.scrollingFollowMinimumVisibleId,
                                            value
                                        )
                                }

                                SettingsSelectRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Bring focused columns into view")
                                    description: qsTr("Center a newly focused column or fit the complete column into the visible area.")
                                    model: root.scrollingFitLabels()
                                    currentIndex: root.choiceIndex(
                                        root.scrollingFocusFitMethodId
                                    )
                                    controlWidth: 150
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsScrollingFocusFitMethod"
                                    accessibleName:
                                        qsTr("Scrolling focus fit method")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: index =>
                                        root.setChoiceFromIndex(
                                            root.scrollingFocusFitMethodId,
                                            index
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Wrap column focus")
                                    description: qsTr("Continue focus from the last Scrolling column to the first, and vice versa.")
                                    checked: root.draftValue(
                                        root.scrollingWrapFocusId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsScrollingWrapFocus"
                                    accessibleName:
                                        qsTr("Wrap Scrolling column focus")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.scrollingWrapFocusId, value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Wrap moved columns")
                                    description: qsTr("Continue a column moved past one end of the Scrolling strip at the other end.")
                                    checked: root.draftValue(
                                        root.scrollingWrapSwapColumnId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsScrollingWrapSwapColumn"
                                    accessibleName:
                                        qsTr("Wrap moved Scrolling columns")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.scrollingWrapSwapColumnId,
                                            value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Snap move gestures to columns")
                                    description: qsTr("Settle a Scrolling move gesture on the nearest column grid when it ends.")
                                    checked: root.draftValue(
                                        root.scrollingMoveSnapGridId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsScrollingMoveSnapGrid"
                                    accessibleName:
                                        qsTr("Snap Scrolling move gestures to the column grid")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.scrollingMoveSnapGridId, value
                                        )
                                }

                                SettingsToggleRow {
                                    Layout.fillWidth: true
                                    title: qsTr("Move pointer after gestures")
                                    description: qsTr("Move the pointer to the newly focused window after a Scrolling move gesture changes focus.")
                                    checked: root.draftValue(
                                        root.scrollingMoveSnapCursorId
                                    ) === true
                                    enabled: root.controlsEnabled
                                    controlObjectName:
                                        "windowsScrollingMoveSnapCursor"
                                    accessibleName:
                                        qsTr("Move the pointer after Scrolling move gestures")
                                    minimumTargetSize: root.minimumTargetSize

                                    onValueModified: value =>
                                        root.setDraftValue(
                                            root.scrollingMoveSnapCursorId,
                                            value
                                        )
                                }
                            }
                        }
                    }
                }

                Frame {
                    objectName: "windowsGroupsCard"
                    Layout.fillWidth: true
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Window groups")
                                color: root.palette.text
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("These choices affect future grouping actions and do not rearrange existing groups. Window rules and group locks can still prevent a window from joining.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.name: text
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Joining and leaving")
                            color: root.palette.text
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Automatically group new windows")
                            description: qsTr("Add eligible new windows to the focused unlocked group.")
                            checked: root.draftValue(
                                root.groupAutoGroupId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsGroupAutoGroup"
                            accessibleName: qsTr("Automatically add eligible new windows to the focused group")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupAutoGroupId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Place new members after the active window")
                            description: qsTr("Insert a window added to a group beside its active member instead of at the end.")
                            checked: root.draftValue(
                                root.groupInsertAfterCurrentId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsGroupInsertAfterCurrent"
                            accessibleName: qsTr("Place new group members after the active window")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupInsertAfterCurrentId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Focus windows moved out")
                            description: qsTr("After moving a window out of a group, keep focus on it instead of the group's remaining active window.")
                            checked: root.draftValue(
                                root.groupFocusRemovedWindowId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsGroupFocusRemovedWindow"
                            accessibleName: qsTr("Focus a window after moving it out of a group")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupFocusRemovedWindowId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Join the destination's only group")
                            description: qsTr("When a future move-to-workspace action targets a workspace whose only visible window belongs to an unlocked group, add the moved window to that group.")
                            checked: root.draftValue(
                                root.groupOnMoveToWorkspaceId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsGroupOnMoveToWorkspace"
                            accessibleName: qsTr("Join a moved window to the destination workspace's only group")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupOnMoveToWorkspaceId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Let movement bypass group locks")
                            description: qsTr("Allow supported move-into-group, move-out-of-group, and move-window-or-group actions to bypass global and per-group locks.")
                            checked: root.draftValue(
                                root.ignoreGroupLockId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsIgnoreGroupLock"
                            accessibleName: qsTr("Let supported group movement actions bypass group locks")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.ignoreGroupLockId, value
                            )
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: root.palette.mid
                            Accessible.ignored: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Dragging and merging")
                                color: root.palette.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Group-bar drop choices take effect only while a group bar is visible. Configure group-bar visibility and presentation in the cards below.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.name: text
                            }
                        }

                        SettingsSelectRow {
                            Layout.fillWidth: true
                            title: qsTr("Drag windows into groups")
                            description: qsTr("Choose which part of a group accepts a future window drop.")
                            model: root.groupDragIntoGroupLabels()
                            currentIndex: root.choiceIndex(
                                root.groupDragIntoGroupId
                            )
                            enabled: root.controlsEnabled
                            controlWidth: root.compactPreview ? 160 : 190
                            controlObjectName:
                                "windowsGroupDragIntoGroup"
                            accessibleName: qsTr("Where dragged windows can join groups")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: index =>
                                root.setChoiceFromIndex(
                                    root.groupDragIntoGroupId, index
                                )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Merge dragged groups")
                            description: qsTr("Allow an entire dragged group to merge into another group.")
                            checked: root.draftValue(
                                root.groupMergeGroupsOnDragId
                            ) === true
                            enabled: root.groupDragChildrenEnabled
                            controlObjectName:
                                "windowsGroupMergeGroupsOnDrag"
                            accessibleName: qsTr("Merge one dragged group into another group")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupMergeGroupsOnDragId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Merge groups through the group bar")
                            description: qsTr("Allow one group to merge with another when it is dropped on the target group bar.")
                            checked: root.draftValue(
                                root.groupMergeGroupsOnGroupbarId
                            ) === true
                            enabled: root.groupGroupbarMergeEnabled
                            controlObjectName:
                                "windowsGroupMergeGroupsOnGroupbar"
                            accessibleName: qsTr("Merge groups through the target group bar")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupMergeGroupsOnGroupbarId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Merge floating windows into tiled groups")
                            description: qsTr("Allow a floating window dropped on a tiled group bar to join that group.")
                            checked: root.draftValue(
                                root.groupMergeFloatedIntoTiledOnGroupbarId
                            ) === true
                            enabled: root.groupbarChildrenEnabled
                                && root.groupDragChildrenEnabled
                            controlObjectName:
                                "windowsGroupMergeFloatedIntoTiledOnGroupbar"
                            accessibleName: qsTr("Merge floating windows into tiled groups through their group bar")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupMergeFloatedIntoTiledOnGroupbarId,
                                value
                            )
                        }
                    }
                }

                Frame {
                    objectName: "windowsGroupbarBehaviorCard"
                    Layout.fillWidth: true
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Group bars")
                                color: root.palette.text
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Group bars are Hyprland window decorations for grouped windows. Window decoration rules can still hide them, and the preview does not show them.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.name: text
                            }
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Show group bars")
                            description: qsTr("Draw a group bar for grouped windows unless a window decoration rule hides it.")
                            checked: root.draftValue(
                                root.groupbarEnabledId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsGroupbarEnabled"
                            accessibleName: qsTr("Show group bars for grouped windows")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarEnabledId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Hide one-member group bars")
                            description: qsTr("Hide the decoration while a group contains only one window.")
                            checked: root.draftValue(
                                root.groupbarDisableWhenOnlyId
                            ) === true
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarDisableWhenOnly"
                            accessibleName: qsTr("Hide group bars for groups with one member")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarDisableWhenOnlyId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Switch members by scrolling")
                            description: qsTr("Cycle through group members when scrolling over the group bar.")
                            checked: root.draftValue(
                                root.groupbarScrollingId
                            ) === true
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName: "windowsGroupbarScrolling"
                            accessibleName: qsTr("Switch group members by scrolling over the group bar")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarScrollingId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Middle-click to close")
                            description: qsTr("Close a group member by middle-clicking its part of the group bar.")
                            checked: root.draftValue(
                                root.groupbarMiddleClickCloseId
                            ) === true
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarMiddleClickClose"
                            accessibleName: qsTr("Close group members by middle-clicking the group bar")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarMiddleClickCloseId, value
                            )
                        }
                    }
                }

                Frame {
                    objectName: "windowsGroupbarLayoutCard"
                    Layout.fillWidth: true
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Group-bar layout")
                                color: root.palette.text
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Set the group bar's arrangement, spacing, indicator shape, and decoration priority. Disabled choices keep their draft values.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.name: text
                            }
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Stack members vertically")
                            description: qsTr("Place each group member on its own row instead of arranging members horizontally.")
                            checked: root.draftValue(
                                root.groupbarStackedId
                            ) === true
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName: "windowsGroupbarStacked"
                            accessibleName: qsTr("Stack group-bar members vertically")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarStackedId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Title and background height")
                            description: qsTr("Set the height used by drawn titles or gradient backgrounds, from 1 through 64 layout pixels.")
                            from: root.optionMinimum(root.groupbarHeightId)
                            to: root.optionMaximum(root.groupbarHeightId)
                            value: Number(root.draftValue(
                                root.groupbarHeightId
                            )) || 0
                            enabled: root.groupbarHeightEnabled
                            controlObjectName: "windowsGroupbarHeight"
                            accessibleName: qsTr("Group-bar title and background height")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarHeightId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Indicator height")
                            description: qsTr("Set the member indicator height from 1 through 64 layout pixels.")
                            from: root.optionMinimum(
                                root.groupbarIndicatorHeightId
                            )
                            to: root.optionMaximum(
                                root.groupbarIndicatorHeightId
                            )
                            value: Number(root.draftValue(
                                root.groupbarIndicatorHeightId
                            )) || 0
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarIndicatorHeight"
                            accessibleName: qsTr("Group-bar member indicator height")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarIndicatorHeightId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Indicator-to-title gap")
                            description: qsTr("Add 0 through 64 layout pixels between the indicator and title area.")
                            from: root.optionMinimum(
                                root.groupbarIndicatorGapId
                            )
                            to: root.optionMaximum(
                                root.groupbarIndicatorGapId
                            )
                            value: Number(root.draftValue(
                                root.groupbarIndicatorGapId
                            )) || 0
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarIndicatorGap"
                            accessibleName: qsTr("Gap between group-bar indicators and titles")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarIndicatorGapId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Space between horizontal members")
                            description: qsTr("Add 0 through 20 layout pixels between members when the group bar is horizontal.")
                            from: root.optionMinimum(root.groupbarGapsInId)
                            to: root.optionMaximum(root.groupbarGapsInId)
                            value: Number(root.draftValue(
                                root.groupbarGapsInId
                            )) || 0
                            enabled: root.groupbarHorizontalGapEnabled
                            controlObjectName: "windowsGroupbarGapsIn"
                            accessibleName: qsTr("Space between horizontal group-bar members")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarGapsInId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Gap from window or stacked rows")
                            description: qsTr("Add 0 through 20 layout pixels between the group bar and window, or between stacked member rows.")
                            from: root.optionMinimum(root.groupbarGapsOutId)
                            to: root.optionMaximum(root.groupbarGapsOutId)
                            value: Number(root.draftValue(
                                root.groupbarGapsOutId
                            )) || 0
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName: "windowsGroupbarGapsOut"
                            accessibleName: qsTr("Gap from the window or between stacked group-bar rows")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarGapsOutId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Keep space above")
                            description: qsTr("Keep the configured outer gap above the group bar instead of closing that space.")
                            checked: root.draftValue(
                                root.groupbarKeepUpperGapId
                            ) === true
                            enabled: root.groupbarKeepUpperGapEnabled
                            controlObjectName:
                                "windowsGroupbarKeepUpperGap"
                            accessibleName: qsTr("Keep the outer group-bar gap above the decoration")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarKeepUpperGapId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Indicator corner radius")
                            description: qsTr("Round member indicators by 0 through 20 layout pixels. Set 0 for square corners.")
                            from: root.optionMinimum(root.groupbarRoundingId)
                            to: root.optionMaximum(root.groupbarRoundingId)
                            value: Number(root.draftValue(
                                root.groupbarRoundingId
                            )) || 0
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName: "windowsGroupbarRounding"
                            accessibleName: qsTr("Group-bar indicator corner radius")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarRoundingId, value
                            )
                        }

                        SettingsSliderRow {
                            Layout.fillWidth: true
                            title: qsTr("Indicator corner power")
                            description: qsTr("Shape indicator corners from circular at 2 toward squarer curves at 10.")
                            from: root.optionMinimum(
                                root.groupbarRoundingPowerId
                            )
                            to: root.optionMaximum(
                                root.groupbarRoundingPowerId
                            )
                            value: Number(root.draftValue(
                                root.groupbarRoundingPowerId
                            )) || 0
                            stepSize: 0.1
                            decimals: 1
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.groupbarRoundingChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarRoundingPower"
                            valueObjectName:
                                "windowsGroupbarRoundingPowerValue"
                            accessibleName: qsTr("Group-bar indicator corner power")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarRoundingPowerId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Round only outer indicators")
                            description: qsTr("Round only the indicators at the outside edges of the group bar.")
                            checked: root.draftValue(
                                root.groupbarRoundOnlyEdgesId
                            ) === true
                            enabled: root.groupbarRoundingChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarRoundOnlyEdges"
                            accessibleName: qsTr("Round only the outer group-bar indicators")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarRoundOnlyEdgesId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Decoration priority")
                            description: qsTr("Choose a priority from 0 through 6. Hyprland evaluates higher-priority decorations first.")
                            from: root.optionMinimum(root.groupbarPriorityId)
                            to: root.optionMaximum(root.groupbarPriorityId)
                            value: Number(root.draftValue(
                                root.groupbarPriorityId
                            )) || 0
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName: "windowsGroupbarPriority"
                            accessibleName: qsTr("Group-bar decoration priority")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarPriorityId, value
                            )
                        }
                    }
                }

                Frame {
                    objectName: "windowsGroupbarTitlesCard"
                    Layout.fillWidth: true
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Group-bar titles")
                                color: root.palette.text
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Choose whether titles are drawn and tune their typography. Settings stores a font family name but does not verify installed fonts.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.name: text
                            }
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Show window titles")
                            description: qsTr("Draw each group member's window title in the group bar.")
                            checked: root.draftValue(
                                root.groupbarRenderTitlesId
                            ) === true
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarRenderTitles"
                            accessibleName: qsTr("Show window titles in group bars")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarRenderTitlesId, value
                            )
                        }

                        ColumnLayout {
                            id: groupbarFontFamilyRow

                            property string projectedText: {
                                const value = root.draftValue(
                                    root.groupbarFontFamilyId
                                );
                                return typeof value === "string" ? value : "";
                            }

                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Title font family")
                                color: root.palette.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Leave empty to use Hyprland's global font. The family name is stored exactly as entered and is not checked against installed fonts.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            TextField {
                                id: groupbarFontFamilyField

                                objectName: "windowsGroupbarFontFamily"
                                Layout.fillWidth: true
                                implicitHeight: root.minimumTargetSize
                                maximumLength: 4096
                                enabled: root.groupbarTitleChildrenEnabled
                                placeholderText: qsTr("Use Hyprland's global font")
                                Accessible.name: qsTr("Group-bar title font family")
                                Accessible.description: qsTr("Leave empty to use Hyprland's global font")
                                Component.onCompleted:
                                    text = groupbarFontFamilyRow.projectedText
                                onActiveFocusChanged: {
                                    if (!activeFocus) {
                                        text = groupbarFontFamilyRow.projectedText;
                                    }
                                }
                                onEditingFinished: {
                                    if (!root.setStringValue(
                                            root.groupbarFontFamilyId,
                                            text)) {
                                        text = groupbarFontFamilyRow.projectedText;
                                    }
                                }
                            }

                            onProjectedTextChanged: {
                                if (!groupbarFontFamilyField.activeFocus) {
                                    groupbarFontFamilyField.text = projectedText;
                                }
                            }
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Font size")
                            description: qsTr("Set title text from 2 through 64 layout pixels.")
                            from: root.optionMinimum(root.groupbarFontSizeId)
                            to: root.optionMaximum(root.groupbarFontSizeId)
                            value: Number(root.draftValue(
                                root.groupbarFontSizeId
                            )) || 0
                            enabled: root.groupbarTitleChildrenEnabled
                            controlObjectName: "windowsGroupbarFontSize"
                            accessibleName: qsTr("Group-bar title font size")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarFontSizeId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Active-title weight")
                            description: qsTr("Set the active title's numeric font weight. 400 is normal and 700 is bold.")
                            from: root.optionMinimum(
                                root.groupbarFontWeightActiveId
                            )
                            to: root.optionMaximum(
                                root.groupbarFontWeightActiveId
                            )
                            value: Number(root.draftValue(
                                root.groupbarFontWeightActiveId
                            )) || 0
                            editable: true
                            stepSize: 10
                            enabled: root.groupbarTitleChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarFontWeightActive"
                            accessibleName: qsTr("Active group-bar title font weight")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarFontWeightActiveId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Inactive-title weight")
                            description: qsTr("Set inactive titles' numeric font weight. 400 is normal and 700 is bold.")
                            from: root.optionMinimum(
                                root.groupbarFontWeightInactiveId
                            )
                            to: root.optionMaximum(
                                root.groupbarFontWeightInactiveId
                            )
                            value: Number(root.draftValue(
                                root.groupbarFontWeightInactiveId
                            )) || 0
                            editable: true
                            stepSize: 10
                            enabled: root.groupbarTitleChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarFontWeightInactive"
                            accessibleName: qsTr("Inactive group-bar title font weight")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarFontWeightInactiveId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Horizontal title padding")
                            description: qsTr("Add 0 through 22 layout pixels beside each title.")
                            from: root.optionMinimum(
                                root.groupbarTextPaddingId
                            )
                            to: root.optionMaximum(
                                root.groupbarTextPaddingId
                            )
                            value: Number(root.draftValue(
                                root.groupbarTextPaddingId
                            )) || 0
                            enabled: root.groupbarTitleChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarTextPadding"
                            accessibleName: qsTr("Horizontal group-bar title padding")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarTextPaddingId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Vertical title offset")
                            description: qsTr("Move title text from -20 through 20 layout pixels. Positive values move it upward.")
                            from: root.optionMinimum(
                                root.groupbarTextOffsetId
                            )
                            to: root.optionMaximum(
                                root.groupbarTextOffsetId
                            )
                            value: Number(root.draftValue(
                                root.groupbarTextOffsetId
                            )) || 0
                            enabled: root.groupbarTitleChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarTextOffset"
                            accessibleName: qsTr("Vertical group-bar title offset")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarTextOffsetId, value
                            )
                        }
                    }
                }

                Frame {
                    objectName: "windowsGroupbarBackgroundCard"
                    Layout.fillWidth: true
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Group-bar backgrounds")
                                color: root.palette.text
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Tune blur and drawn background geometry. This page does not configure group-bar colors.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.name: text
                            }
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Blur group-bar surfaces")
                            description: qsTr("Apply blur behind member indicators and any drawn gradient backgrounds.")
                            checked: root.draftValue(
                                root.groupbarBlurId
                            ) === true
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName: "windowsGroupbarBlur"
                            accessibleName: qsTr("Blur group-bar indicators and backgrounds")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarBlurId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Draw gradient backgrounds")
                            description: qsTr("Draw a gradient background behind each group member using its configured group-bar colors.")
                            checked: root.draftValue(
                                root.groupbarGradientsId
                            ) === true
                            enabled: root.groupbarChildrenEnabled
                            controlObjectName: "windowsGroupbarGradients"
                            accessibleName: qsTr("Draw group-bar gradient backgrounds")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarGradientsId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Background corner radius")
                            description: qsTr("Round drawn backgrounds by 0 through 20 layout pixels. Set 0 for square corners.")
                            from: root.optionMinimum(
                                root.groupbarGradientRoundingId
                            )
                            to: root.optionMaximum(
                                root.groupbarGradientRoundingId
                            )
                            value: Number(root.draftValue(
                                root.groupbarGradientRoundingId
                            )) || 0
                            enabled: root.groupbarGradientEnabled
                            controlObjectName:
                                "windowsGroupbarGradientRounding"
                            accessibleName: qsTr("Group-bar background corner radius")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarGradientRoundingId, value
                            )
                        }

                        SettingsSliderRow {
                            Layout.fillWidth: true
                            title: qsTr("Background corner power")
                            description: qsTr("Shape background corners from circular at 2 toward squarer curves at 10.")
                            from: root.optionMinimum(
                                root.groupbarGradientRoundingPowerId
                            )
                            to: root.optionMaximum(
                                root.groupbarGradientRoundingPowerId
                            )
                            value: Number(root.draftValue(
                                root.groupbarGradientRoundingPowerId
                            )) || 0
                            stepSize: 0.1
                            decimals: 1
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled:
                                root.groupbarGradientRoundingChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarGradientRoundingPower"
                            valueObjectName:
                                "windowsGroupbarGradientRoundingPowerValue"
                            accessibleName: qsTr("Group-bar background corner power")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarGradientRoundingPowerId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Round only outer backgrounds")
                            description: qsTr("Round only the backgrounds at the outside edges of the group bar.")
                            checked: root.draftValue(
                                root.groupbarGradientRoundOnlyEdgesId
                            ) === true
                            enabled:
                                root.groupbarGradientRoundingChildrenEnabled
                            controlObjectName:
                                "windowsGroupbarGradientRoundOnlyEdges"
                            accessibleName: qsTr("Round only the outer group-bar backgrounds")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.groupbarGradientRoundOnlyEdgesId, value
                            )
                        }
                    }
                }

                Frame {
                    objectName: "windowsResizeCard"
                    Layout.fillWidth: true
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
                            text: qsTr("Resize")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Resize from borders and gaps")
                            description: qsTr("Let pointer drags on window borders and surrounding gaps resize windows.")
                            checked: root.draftValue(
                                root.resizeOnBorderId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsResizeOnBorder"
                            accessibleName: qsTr("Resize windows from borders and gaps")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.resizeOnBorderId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Border grab area")
                            description: qsTr("Extend the draggable resize area beyond the visible border by this many layout pixels.")
                            from: root.optionMinimum(
                                root.extendBorderGrabAreaId
                            )
                            to: root.optionMaximum(
                                root.extendBorderGrabAreaId
                            )
                            value: Number(root.draftValue(
                                root.extendBorderGrabAreaId
                            )) || 0
                            enabled: root.resizeChildrenEnabled
                            controlObjectName:
                                "windowsExtendBorderGrabArea"
                            accessibleName: qsTr("Window border resize grab area in layout pixels")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.extendBorderGrabAreaId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Show resize cursor")
                            description: qsTr("Change the pointer icon when it is over a draggable window border.")
                            checked: root.draftValue(
                                root.hoverIconOnBorderId
                            ) === true
                            enabled: root.resizeChildrenEnabled
                            controlObjectName: "windowsHoverIconOnBorder"
                            accessibleName: qsTr("Show a resize cursor over window borders")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.hoverIconOnBorderId, value
                            )
                        }

                        SettingsSelectRow {
                            Layout.fillWidth: true
                            title: qsTr("Floating resize corner")
                            description: qsTr("Keep floating-window resize drags attached to an automatic or fixed corner.")
                            model: root.resizeCornerLabels()
                            currentIndex: root.choiceIndex(root.resizeCornerId)
                            controlWidth: 150
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsResizeCorner"
                            accessibleName: qsTr("Floating window resize corner")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: index => root.setChoiceFromIndex(
                                root.resizeCornerId, index
                            )
                        }
                    }
                }

                Frame {
                    objectName: "windowsSnapCard"
                    Layout.fillWidth: true
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
                            text: qsTr("Snap")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Snap floating windows")
                            description: qsTr("Enable managed snapping for floating windows.")
                            checked:
                                root.draftValue(root.snapEnabledId) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsSnapEnabled"
                            accessibleName: qsTr("Snap floating windows")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.snapEnabledId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Overlap adjacent borders")
                            description: qsTr("Snap two windows with only one border width between them.")
                            checked: root.draftValue(
                                root.snapBorderOverlapId
                            ) === true
                            enabled: root.snapChildrenEnabled
                            controlObjectName: "windowsSnapBorderOverlap"
                            accessibleName: qsTr("Overlap adjacent window borders while snapping")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.snapBorderOverlapId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Monitor snap distance")
                            description: qsTr("Start snapping this many layout pixels from a monitor edge.")
                            from: root.optionMinimum(root.snapMonitorGapId)
                            to: root.optionMaximum(root.snapMonitorGapId)
                            value: Number(root.draftValue(
                                root.snapMonitorGapId
                            )) || 0
                            enabled: root.snapChildrenEnabled
                            controlObjectName: "windowsSnapMonitorGap"
                            accessibleName: qsTr("Monitor edge snap distance in layout pixels")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.snapMonitorGapId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Respect window gaps")
                            description: qsTr("Keep configured window gaps when a floating window snaps into place.")
                            checked: root.draftValue(
                                root.snapRespectGapsId
                            ) === true
                            enabled: root.snapChildrenEnabled
                            controlObjectName: "windowsSnapRespectGaps"
                            accessibleName: qsTr("Respect configured window gaps while snapping")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.snapRespectGapsId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Window snap distance")
                            description: qsTr("Start snapping this many layout pixels from another window.")
                            from: root.optionMinimum(root.snapWindowGapId)
                            to: root.optionMaximum(root.snapWindowGapId)
                            value: Number(root.draftValue(
                                root.snapWindowGapId
                            )) || 0
                            enabled: root.snapChildrenEnabled
                            controlObjectName: "windowsSnapWindowGap"
                            accessibleName: qsTr("Window snap distance in layout pixels")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.snapWindowGapId, value
                            )
                        }
                    }
                }

                Frame {
                    objectName: "windowsFocusCard"
                    Layout.fillWidth: true
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
                            text: qsTr("Focus")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsSelectRow {
                            Layout.fillWidth: true
                            title: qsTr("Pointer focus")
                            description: qsTr("Choose whether and how pointer movement changes window focus.")
                            model: root.followMouseLabels()
                            currentIndex: root.choiceIndex(root.followMouseId)
                            controlWidth: 190
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsFollowMouse"
                            accessibleName: qsTr("Pointer window focus mode")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: index => root.setChoiceFromIndex(
                                root.followMouseId, index
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Follow pointer during drag and drop")
                            description: root.draftValue(root.followMouseId) === 1
                                ? qsTr("Pointer focus already uses follow mode, so this saved choice is dormant until another pointer focus mode is selected.")
                                : qsTr("Force pointer focus mode 1 while a protocol drag-and-drop operation is active. This does not affect dragging a window.")
                            checked: root.draftValue(
                                root.alwaysFollowOnDndId
                            ) === true
                            enabled: root.dragAndDropFollowOverrideEnabled
                            controlObjectName: "windowsAlwaysFollowOnDnd"
                            accessibleName: qsTr("Follow pointer focus during drag-and-drop operations")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.alwaysFollowOnDndId, value
                            )
                        }

                        SettingsDecimalRow {
                            objectName: "windowsFollowMouseThresholdRow"
                            Layout.fillWidth: true
                            title: qsTr("Focus movement threshold")
                            description: qsTr("Accumulate pointer movement and require it to strictly exceed this many logical pixels before focusing a different hovered window in effective Follow pointer mode, including the protocol drag-and-drop override. A gap of 0.5 seconds or more between movement events resets the accumulator; explicit refocus bypasses the threshold, and a No follow mouse Window Rule still blocks ordinary focus. A value of 0 still requires positive movement.")
                            value: root.draftValue(
                                root.followMouseThresholdId
                            )
                            minimumValue: root.optionMinimum(
                                root.followMouseThresholdId
                            )
                            maximumValue: root.optionMaximum(
                                root.followMouseThresholdId
                            )
                            controlWidth: root.compactPreview ? 160 : 190
                            enabled: root.followMouseThresholdEnabled
                            controlObjectName:
                                "windowsFollowMouseThreshold"
                            validationObjectName:
                                "windowsFollowMouseThresholdValidation"
                            accessibleName: qsTr("Pointer focus movement threshold in logical pixels")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value =>
                                root.setExactDecimalDraftValue(
                                    root.followMouseThresholdId, value
                                )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Focus monitors under the pointer")
                            description: qsTr("Move monitor focus on passive pointer crossing into another monitor. Clicks, explicit refocus, and focus on a hovered window can still select that monitor when this is off.")
                            checked: root.draftValue(
                                root.mouseMoveFocusesMonitorId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsMouseMoveFocusesMonitor"
                            accessibleName: qsTr("Focus another monitor when the pointer moves onto it")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.mouseMoveFocusesMonitorId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Refocus hovered windows")
                            description: qsTr("Allow pointer movement inside the current window boundary to refresh focus under the pointer.")
                            checked:
                                root.draftValue(root.mouseRefocusId) === true
                            enabled: root.pointerFocusChildrenEnabled
                            controlObjectName: "windowsMouseRefocus"
                            accessibleName: qsTr("Refocus hovered windows as the pointer moves")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.mouseRefocusId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Focus dead zone")
                            description: qsTr("Shrink inactive focus hitboxes to leave a dead zone between windows when focus follows the pointer.")
                            from: root.optionMinimum(root.followMouseShrinkId)
                            to: root.optionMaximum(root.followMouseShrinkId)
                            value: Number(root.draftValue(
                                root.followMouseShrinkId
                            )) || 0
                            enabled: root.pointerFocusChildrenEnabled
                            controlObjectName: "windowsFollowMouseShrink"
                            accessibleName: qsTr("Pointer focus dead zone in pixels")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.followMouseShrinkId, value
                            )
                        }

                        SettingsSelectRow {
                            Layout.fillWidth: true
                            title: qsTr("Focus after floating changes")
                            description: qsTr("Choose when changing a window between tiled and floating states follows the pointer.")
                            model: root.floatSwitchFocusLabels()
                            currentIndex: root.choiceIndex(
                                root.floatSwitchOverrideFocusId
                            )
                            controlWidth: 210
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsFloatSwitchOverrideFocus"
                            accessibleName: qsTr("Focus behavior after tiled and floating transitions")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: index => root.setChoiceFromIndex(
                                root.floatSwitchOverrideFocusId, index
                            )
                        }

                        SettingsSelectRow {
                            Layout.fillWidth: true
                            title: qsTr("Focus after closing")
                            description: qsTr("Choose which window receives focus after the focused window closes.")
                            model: root.focusOnCloseLabels()
                            currentIndex: root.choiceIndex(root.focusOnCloseId)
                            controlWidth: 180
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsFocusOnClose"
                            accessibleName: qsTr("Window focus after closing")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: index => root.setChoiceFromIndex(
                                root.focusOnCloseId, index
                            )
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: root.palette.mid
                            Accessible.ignored: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Application focus and fullscreen requests")
                                color: root.palette.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("These choices affect later application activation, focus, and window-close events. Changing them does not focus or change fullscreen immediately.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.name: text
                            }
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Honor application focus requests")
                            description: qsTr("Let an application activation request focus its window and raise it when floating; the normal cursor-warp policy may also move the pointer. When disabled, the request still marks the window urgent; a matching Window Rule can override this choice.")
                            checked: root.draftValue(
                                root.focusOnActivateId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsFocusOnActivate"
                            accessibleName: qsTr("Honor application activation focus requests")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.focusOnActivateId, value
                            )
                        }

                        SettingsSelectRow {
                            Layout.fillWidth: true
                            title: qsTr("Focus request under fullscreen or maximized")
                            description: qsTr("Choose what happens when a tiled window requests focus underneath another window that is fullscreen or maximized.")
                            model: root.focusUnderFullscreenLabels()
                            currentIndex: root.choiceIndex(
                                root.onFocusUnderFullscreenId
                            )
                            controlWidth: root.compactPreview ? 205 : 250
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsOnFocusUnderFullscreen"
                            accessibleName: qsTr("Tiled window focus requests under fullscreen or maximized")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: index => root.setChoiceFromIndex(
                                root.onFocusUnderFullscreenId, index
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Keep fullscreen or maximized after closing")
                            description: qsTr("When the focused fullscreen or maximized window closes, transfer its internal mode to the chosen replacement. The next member of the same group inherits it regardless of this setting.")
                            checked: root.draftValue(
                                root.exitWindowRetainsFullscreenId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsExitWindowRetainsFullscreen"
                            accessibleName: qsTr("Keep fullscreen or maximized mode after closing its window")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.exitWindowRetainsFullscreenId, value
                            )
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: root.palette.mid
                            Accessible.ignored: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Directional actions")
                                color: root.palette.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("These choices affect future directional focus and tiled-window movement actions. They do not move or refocus a window when changed.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.name: text
                            }
                        }

                        SettingsSelectRow {
                            Layout.fillWidth: true
                            title: qsTr("Choose directional targets by")
                            description: qsTr("When several adjacent tiled windows match, prefer the most recently focused candidate or the candidate sharing the longest edge.")
                            model: root.focusPreferredMethodLabels()
                            currentIndex: root.choiceIndex(
                                root.focusPreferredMethodId
                            )
                            controlWidth: root.compactPreview ? 170 : 200
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsFocusPreferredMethod"
                            accessibleName: qsTr("Directional tiled-window target preference")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: index =>
                                root.setChoiceFromIndex(
                                    root.focusPreferredMethodId, index
                                )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Cycle fullscreen windows")
                            description: qsTr("Allow directional focus from ordinary fullscreen to cycle through windows that fullscreen otherwise blocks. Layout-managed fullscreen keeps its own behavior.")
                            checked: root.draftValue(
                                root.movefocusCyclesFullscreenId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsMovefocusCyclesFullscreen"
                            accessibleName: qsTr("Cycle otherwise blocked windows from fullscreen directional focus")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.movefocusCyclesFullscreenId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Cycle group members first")
                            description: qsTr("When left or right directional focus starts inside a group, switch group members before moving outside the group.")
                            checked: root.draftValue(
                                root.movefocusCyclesGroupfirstId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsMovefocusCyclesGroupfirst"
                            accessibleName: qsTr("Cycle group members before leaving with left or right focus")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.movefocusCyclesGroupfirstId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Continue across monitors")
                            description: qsTr("Continue directional focus or tiled-window movement onto the next monitor in that direction when no target remains on the current monitor.")
                            checked: root.draftValue(
                                root.windowDirectionMonitorFallbackId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName:
                                "windowsWindowDirectionMonitorFallback"
                            accessibleName: qsTr("Continue directional focus and tiled-window movement across monitors")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.windowDirectionMonitorFallbackId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Focus through special workspaces")
                            description: qsTr("Let focus pass to the regular workspace when a special workspace contains only floating windows.")
                            checked: root.draftValue(
                                root.specialFallthroughId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsSpecialFallthrough"
                            accessibleName: qsTr("Allow focus through a floating-only special workspace")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.specialFallthroughId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Stop directional focus at the edge")
                            description: qsTr("Do not fall back to another window when directional focus finds no window in that direction.")
                            checked: root.draftValue(
                                root.noFocusFallbackId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsNoFocusFallback"
                            accessibleName: qsTr("Stop directional focus when no window is found")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.noFocusFallbackId, value
                            )
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Block modal parent interaction")
                            description: qsTr("Prevent interaction with a parent window while one of its modal dialogs is open.")
                            checked: root.draftValue(
                                root.modalParentBlockingId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsModalParentBlocking"
                            accessibleName: qsTr("Block parent windows while modal dialogs are open")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.modalParentBlockingId, value
                            )
                        }
                    }
                }

                Frame {
                    objectName: "windowsSwallowingCard"
                    Layout.fillWidth: true
                    padding: 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Window swallowing")
                                color: root.palette.text
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("For future window maps, Hyprland can let a child replace a matching process-ancestor window. The parent returns when the child unmaps or closes; changing these settings does not alter an existing swallowed pair.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.name: text
                            }
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Swallow matching parent windows")
                            description: qsTr("Enable process-ancestor matching for future mapped windows. A nonempty parent class pattern is required before any window can match; an empty pattern keeps swallowing dormant. Turning this off preserves both patterns.")
                            checked: root.draftValue(
                                root.enableSwallowId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsEnableSwallow"
                            accessibleName: qsTr("Enable future window swallowing")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.enableSwallowId, value
                            )
                        }

                        ColumnLayout {
                            id: swallowRegexRow

                            property string projectedText: {
                                const value = root.draftValue(
                                    root.swallowRegexId
                                );
                                return typeof value === "string" ? value : "";
                            }

                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Parent class pattern")
                                color: root.palette.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Enter an RE2 pattern that must match the complete class of a mapped, input-capable process ancestor. Use .* for substring matching. Syntax is checked on Save.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            TextField {
                                id: swallowRegexField

                                objectName: "windowsSwallowRegex"
                                Layout.fillWidth: true
                                implicitHeight: root.minimumTargetSize
                                maximumLength: 4096
                                enabled: root.swallowChildrenEnabled
                                placeholderText: qsTr("Example: ^ExampleClass$")
                                Accessible.name: qsTr("Parent window class RE2 pattern")
                                Accessible.description: qsTr("Full-match pattern checked when settings are saved")
                                Component.onCompleted:
                                    text = swallowRegexRow.projectedText
                                onActiveFocusChanged: {
                                    if (!activeFocus) {
                                        text = swallowRegexRow.projectedText;
                                    }
                                }
                                onEditingFinished: {
                                    if (!root.setStringValue(
                                            root.swallowRegexId, text)) {
                                        text = swallowRegexRow.projectedText;
                                    }
                                }
                            }

                            onProjectedTextChanged: {
                                if (!swallowRegexField.activeFocus) {
                                    swallowRegexField.text = projectedText;
                                }
                            }
                        }

                        ColumnLayout {
                            id: swallowExceptionRegexRow

                            property string projectedText: {
                                const value = root.draftValue(
                                    root.swallowExceptionRegexId
                                );
                                return typeof value === "string" ? value : "";
                            }

                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Parent title exception")
                                color: root.palette.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Optionally exclude a candidate when this RE2 pattern matches its complete window title. Leave empty for no exceptions. Syntax is checked on Save.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            TextField {
                                id: swallowExceptionRegexField

                                objectName: "windowsSwallowExceptionRegex"
                                Layout.fillWidth: true
                                implicitHeight: root.minimumTargetSize
                                maximumLength: 4096
                                enabled: root.swallowChildrenEnabled
                                placeholderText: qsTr("No title exceptions")
                                Accessible.name: qsTr("Parent window title exception RE2 pattern")
                                Accessible.description: qsTr("Optional full-match exception checked when settings are saved")
                                Component.onCompleted:
                                    text = swallowExceptionRegexRow.projectedText
                                onActiveFocusChanged: {
                                    if (!activeFocus) {
                                        text = swallowExceptionRegexRow.projectedText;
                                    }
                                }
                                onEditingFinished: {
                                    if (!root.setStringValue(
                                            root.swallowExceptionRegexId,
                                            text)) {
                                        text = swallowExceptionRegexRow.projectedText;
                                    }
                                }
                            }

                            onProjectedTextChanged: {
                                if (!swallowExceptionRegexField.activeFocus) {
                                    swallowExceptionRegexField.text =
                                        projectedText;
                                }
                            }
                        }
                    }
                }

                Frame {
                    objectName: "windowsUnresponsiveApplicationsCard"
                    Layout.fillWidth: true
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
                            text: qsTr("Unresponsive applications")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        SettingsToggleRow {
                            Layout.fillWidth: true
                            title: qsTr("Show unresponsive app dialogs")
                            description: qsTr("Allow Hyprland to offer Wait and Terminate choices when an application stops responding. This requires the hyprland-dialog helper.")
                            checked: root.draftValue(
                                root.anrDialogEnabledId
                            ) === true
                            enabled: root.controlsEnabled
                            controlObjectName: "windowsAnrDialogEnabled"
                            accessibleName: qsTr("Show Hyprland dialogs for unresponsive applications")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.anrDialogEnabledId, value
                            )
                        }

                        SettingsSpinBoxRow {
                            Layout.fillWidth: true
                            title: qsTr("Missed-response threshold")
                            description: qsTr("Set how many missed response checks Hyprland allows before it offers the dialog.")
                            from: root.optionMinimum(root.anrMissedPingsId)
                            to: root.optionMaximum(root.anrMissedPingsId)
                            value: Number(root.draftValue(
                                root.anrMissedPingsId
                            )) || 1
                            enabled: root.anrChildrenEnabled
                            controlObjectName: "windowsAnrMissedPings"
                            accessibleName: qsTr("Missed responses before showing the unresponsive application dialog")
                            minimumTargetSize: root.minimumTargetSize

                            onValueModified: value => root.setDraftValue(
                                root.anrMissedPingsId, value
                            )
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Turning dialogs off preserves the threshold. A dialog that is already open may remain until the application responds or you choose an action.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }
                }

                Frame {
                    objectName: "windowsDraftActions"
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
                                        ? qsTr("Unsaved Windows & Layout draft")
                                        : qsTr("No window changes")
                                color: root.palette.text
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                textFormat: Text.PlainText
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

                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: childrenRect.height
                            spacing: 10

                            Button {
                                objectName: "discardWindowsDraftButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Discard draft")
                                visible: root.draftDirty
                                    && !root.externalChangeWhileEditing
                                enabled: !root.busy && !root.saveSubmitted
                                    && !root.sharedMutationBusy
                                    && root.trustedValuesValid
                                Accessible.name: qsTr("Discard Windows & Layout draft")

                                onClicked: root.synchronizeDraft()
                            }

                            Button {
                                objectName: "resetWindowsDefaultsButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Reset to defaults")
                                enabled: {
                                    const target = root.resetTargetValues();
                                    return root.controlsEnabled
                                        && target !== null
                                        && !root.valuesEqual(
                                            root.draftValues, target
                                        );
                                }
                                Accessible.name: qsTr("Reset Windows & Layout draft to trusted catalog defaults")

                                onClicked: root.resetDraftToDefaults()
                            }

                            Button {
                                objectName: "saveWindowsButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: {
                                    if (root.busyOperation === "windows-save")
                                        return qsTr("Saving…");
                                    if (root.busyOperation === "compositor-apply"
                                            || root.busyOperation
                                                === "windows-apply") {
                                        return qsTr("Applying…");
                                    }
                                    return qsTr("Save & apply");
                                }
                                highlighted: true
                                enabled: root.saveEnabled
                                Accessible.name: qsTr("Save and apply the validated Windows & Layout draft")

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
        id: windowsRecoveryDialog

        objectName: "windowsRecoveryDialog"
        recoveryAvailable: root.recoveryAvailable
        operationBusy: root.busy || root.sharedMutationBusy
        busyOperation: root.busyOperation
        settingsAreaName: qsTr("Windows & Layout")
        warningObjectName: "windowsRecoveryWarning"
        cancelObjectName: "cancelWindowsRecoveryButton"
        confirmObjectName: "confirmWindowsRecoveryButton"
        minimumTargetSize: root.minimumTargetSize

        onRecoveryRequested: root.recoveryRequested()
    }
}
