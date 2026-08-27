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
    property bool inputAvailable: false
    property bool inputProjectionAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property var inputOptions: []
    property var inputValues: ({})
    property bool inputGesturesProjectionAvailable: false
    property var inputGestures: []
    property var inputGestureCompatibility: []
    property var inputGestureActions: []
    property string revisionToken: "0"
    property double appliedRevision: 0
    property string loadState: "unavailable"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property string inputErrorName: ""
    property string inputErrorMessage: ""
    property bool inputDeviceDiscoveryAvailable: false
    property bool inputDeviceDiscoveryBusy: false
    property var connectedInputDevices: []
    property double inputDevicesObservedAtMs: 0
    property string inputDeviceInventoryDigest: ""
    property var inputDeviceUnaddressableCounts: ({})
    property string inputDeviceDiscoveryErrorName: ""
    property string inputDeviceDiscoveryErrorMessage: ""
    property bool inputDeviceProjectionAvailable: false
    property var savedInputDevices: []
    property var otherSavedInputDevices: []
    property string inputDeviceProjectionRevisionToken: ""
    property string inputDeviceProjectionInventoryDigest: ""
    property string inputDeviceProjectionErrorName: ""
    property string inputDeviceProjectionErrorMessage: ""
    property string sharedErrorName: ""
    property string sharedErrorMessage: ""
    property bool retryApplyAvailable: false
    property bool recoveryAvailable: false
    property bool sharedMutationBusy: false
    property bool sharedApplySafe: false
    property real contentTopMargin: 28

    property var draftValues: ({})
    property var synchronizedValues: ({})
    property var submittedValues: ({})
    property var draftGestures: []
    property var synchronizedGestures: []
    property var submittedGestures: []
    property string synchronizedRevisionToken: ""
    property string submittedRevisionToken: ""
    property bool projectionInitialized: false
    property bool externalChangeWhileEditing: false
    property bool saveSubmitted: false
    property int inputTabIndex: 0
    property string editingGestureId: ""
    property string gestureEditorIssue: ""

    signal refreshRequested()
    signal refreshConnectedInputDevicesRequested()
    signal manageInputDeviceProfilesRequested()
    signal openDisplaysRequested()
    signal saveRequested(var values, var gestures)
    signal retryApplyRequested()
    signal recoveryRequested()

    readonly property string repeatRateId: "hyprland.input.repeat_rate"
    readonly property string repeatDelayId: "hyprland.input.repeat_delay"
    readonly property string sensitivityId: "hyprland.input.sensitivity"
    readonly property string accelerationProfileId:
        "hyprland.input.accel_profile"
    readonly property string naturalScrollId:
        "hyprland.input.natural_scroll"
    readonly property string leftHandedId: "hyprland.input.left_handed"
    readonly property string scrollFactorId: "hyprland.input.scroll_factor"
    readonly property string touchpadTapToClickId:
        "hyprland.input.touchpad.tap-to-click"
    readonly property string touchpadTapAndDragId:
        "hyprland.input.touchpad.tap-and-drag"
    readonly property string touchpadNaturalScrollId:
        "hyprland.input.touchpad.natural_scroll"
    readonly property string touchpadDisableWhileTypingId:
        "hyprland.input.touchpad.disable_while_typing"
    readonly property string touchpadScrollFactorId:
        "hyprland.input.touchpad.scroll_factor"
    readonly property string scrollMethodId:
        "hyprland.input.scroll_method"
    readonly property string scrollButtonId:
        "hyprland.input.scroll_button"
    readonly property string scrollButtonLockId:
        "hyprland.input.scroll_button_lock"
    readonly property string offWindowAxisEventsId:
        "hyprland.input.off_window_axis_events"
    readonly property string emulateDiscreteScrollId:
        "hyprland.input.emulate_discrete_scroll"
    readonly property string touchpadClickfingerBehaviorId:
        "hyprland.input.touchpad.clickfinger_behavior"
    readonly property string touchpadMultiFingerDragId:
        "hyprland.input.touchpad.drag_3fg"
    readonly property string touchpadDragLockId:
        "hyprland.input.touchpad.drag_lock"
    readonly property string touchpadFlipHorizontalId:
        "hyprland.input.touchpad.flip_x"
    readonly property string touchpadFlipVerticalId:
        "hyprland.input.touchpad.flip_y"
    readonly property string touchpadMiddleButtonEmulationId:
        "hyprland.input.touchpad.middle_button_emulation"
    readonly property string touchpadTapButtonMapId:
        "hyprland.input.touchpad.tap_button_map"
    readonly property string numLockByDefaultId:
        "hyprland.input.numlock_by_default"
    readonly property string resolveBindsBySymbolId:
        "hyprland.input.resolve_binds_by_sym"
    readonly property string virtualKeyboardShareStatesId:
        "hyprland.input.virtualkeyboard.share_states"
    readonly property string virtualKeyboardReleasePressedOnCloseId:
        "hyprland.input.virtualkeyboard.release_pressed_on_close"
    readonly property string virtualKeyboardNameAfterProcessId:
        "hyprland.misc.name_vk_after_proc"
    readonly property string forceNoAccelId:
        "hyprland.input.force_no_accel"
    readonly property string rotationId: "hyprland.input.rotation"
    readonly property string middleClickPasteId:
        "hyprland.misc.middle_click_paste"
    readonly property string closeGestureTimeoutId:
        "hyprland.gestures.close_max_timeout"
    readonly property string touchDeviceEnabledId:
        "hyprland.input.touchdevice.enabled"
    readonly property string touchDeviceTransformId:
        "hyprland.input.touchdevice.transform"
    readonly property string tabletRelativeInputId:
        "hyprland.input.tablet.relative_input"
    readonly property string tabletLeftHandedId:
        "hyprland.input.tablet.left_handed"
    readonly property string tabletTransformId:
        "hyprland.input.tablet.transform"
    readonly property string tabletRegionPositionId:
        "hyprland.input.tablet.region_position"
    readonly property string tabletAbsoluteRegionPositionId:
        "hyprland.input.tablet.absolute_region_position"
    readonly property string tabletRegionSizeId:
        "hyprland.input.tablet.region_size"
    readonly property string cursorHideOnKeyPressId:
        "hyprland.cursor.hide_on_key_press"
    readonly property string cursorHideOnTouchId:
        "hyprland.cursor.hide_on_touch"
    readonly property string cursorHideOnTabletId:
        "hyprland.cursor.hide_on_tablet"
    readonly property string cursorInactiveTimeoutId:
        "hyprland.cursor.inactive_timeout"
    readonly property string cursorHotspotPaddingId:
        "hyprland.cursor.hotspot_padding"
    readonly property string cursorNoWarpsId:
        "hyprland.cursor.no_warps"
    readonly property string cursorPersistentWarpsId:
        "hyprland.cursor.persistent_warps"
    readonly property string cursorWarpBackAfterNonMouseInputId:
        "hyprland.cursor.warp_back_after_non_mouse_input"
    readonly property real minimumTargetSize: 44
    readonly property bool compactPage: root.width < 560
    readonly property var expectedOptionIds: [
        root.repeatRateId,
        root.repeatDelayId,
        root.sensitivityId,
        root.accelerationProfileId,
        root.naturalScrollId,
        root.leftHandedId,
        root.scrollFactorId,
        root.touchpadTapToClickId,
        root.touchpadTapAndDragId,
        root.touchpadNaturalScrollId,
        root.touchpadDisableWhileTypingId,
        root.touchpadScrollFactorId,
        root.scrollMethodId,
        root.scrollButtonId,
        root.scrollButtonLockId,
        root.offWindowAxisEventsId,
        root.emulateDiscreteScrollId,
        root.touchpadClickfingerBehaviorId,
        root.touchpadMultiFingerDragId,
        root.touchpadDragLockId,
        root.touchpadFlipHorizontalId,
        root.touchpadFlipVerticalId,
        root.touchpadMiddleButtonEmulationId,
        root.touchpadTapButtonMapId,
        root.numLockByDefaultId,
        root.virtualKeyboardShareStatesId,
        root.virtualKeyboardReleasePressedOnCloseId,
        root.virtualKeyboardNameAfterProcessId,
        root.forceNoAccelId,
        root.rotationId,
        root.middleClickPasteId,
        root.closeGestureTimeoutId,
        root.touchDeviceEnabledId,
        root.touchDeviceTransformId,
        root.tabletRelativeInputId,
        root.tabletLeftHandedId,
        root.tabletTransformId,
        root.cursorHideOnKeyPressId,
        root.cursorHideOnTouchId,
        root.cursorHideOnTabletId,
        root.cursorInactiveTimeoutId,
        root.cursorHotspotPaddingId,
        root.cursorNoWarpsId,
        root.cursorPersistentWarpsId,
        root.cursorWarpBackAfterNonMouseInputId,
        root.tabletRegionPositionId,
        root.tabletAbsoluteRegionPositionId,
        root.tabletRegionSizeId,
        root.resolveBindsBySymbolId
    ]
    readonly property var exactVectorOptionIds: [
        root.tabletRegionPositionId,
        root.tabletRegionSizeId
    ]
    readonly property var expectedGestureActionIds: [
        "close", "cursorZoom", "float", "fullscreen", "move", "resize",
        "scrollMove", "special", "workspace"
    ]
    readonly property bool trustedDefinitionsValid: root.validateOptions()
    readonly property bool trustedValuesValid:
        root.inputProjectionAvailable
        && root.trustedDefinitionsValid
        && root.validateValues(root.inputValues)
    readonly property bool trustedGestureActionsValid:
        root.validateGestureActions(root.inputGestureActions)
    readonly property bool trustedGesturesValid:
        root.inputGesturesProjectionAvailable
        && root.trustedGestureActionsValid
        && root.validateGestures(root.inputGestures)
        && root.validateGestureCompatibility(
            root.inputGestures, root.inputGestureCompatibility
        )
    readonly property bool revisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.revisionToken)
    readonly property bool synchronizedRevisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.synchronizedRevisionToken)
    readonly property bool draftValuesValid:
        root.trustedDefinitionsValid && root.validateValues(root.draftValues)
    readonly property bool draftGesturesValid:
        root.trustedGestureActionsValid
        && root.validateGestures(root.draftGestures)
    readonly property bool draftValid:
        root.draftValuesValid && root.draftGesturesValid
    readonly property bool valuesDraftDirty:
        root.projectionInitialized
        && !root.valuesEqual(root.draftValues, root.synchronizedValues)
    readonly property bool globalDraftDirty:
        root.projectionInitialized
        && root.expectedOptionIds.some(id =>
            id !== root.closeGestureTimeoutId
            && !root.valueEqual(
                root.draftValues[id], root.synchronizedValues[id]
            )
        )
    readonly property bool closeGestureTimeoutDraftDirty:
        root.projectionInitialized
        && root.draftValues[root.closeGestureTimeoutId]
            !== root.synchronizedValues[root.closeGestureTimeoutId]
    readonly property bool gestureRecordsDraftDirty:
        root.projectionInitialized
        && !root.gesturesEqual(
            root.draftGestures, root.synchronizedGestures
        )
    readonly property bool gesturesDraftDirty:
        root.closeGestureTimeoutDraftDirty || root.gestureRecordsDraftDirty
    readonly property bool draftDirty:
        root.valuesDraftDirty || root.gestureRecordsDraftDirty
    readonly property bool displayTestActive:
        root.confirmationState !== "idle"
        || root.managementState === "preview"
    readonly property bool controlsEnabled:
        root.serviceAvailable
        && root.writable
        && root.catalogAvailable
        && root.inputAvailable
        && root.revisionTokenValid
        && root.trustedDefinitionsValid
        && root.trustedValuesValid
        && root.trustedGesturesValid
        && !root.busy
        && !root.saveSubmitted
        && !root.sharedMutationBusy
        && root.sharedApplySafe
        && !root.externalChangeWhileEditing
        && !root.displayTestActive
    readonly property bool saveEnabled:
        root.controlsEnabled && root.draftDirty && root.draftValid
        && !root.saveSubmitted && root.sharedApplySafe
    readonly property bool buttonScrollingEnabled:
        root.controlsEnabled
        && root.draftValue(root.scrollMethodId) === "on_button_down"
    readonly property bool pointerAccelerationControlsEnabled:
        root.controlsEnabled
        && root.draftValue(root.forceNoAccelId) !== true
    readonly property bool touchpadTapMappingEnabled:
        root.controlsEnabled
        && root.draftValue(root.touchpadTapToClickId) === true
    readonly property bool touchpadDragLockEnabled:
        root.touchpadTapMappingEnabled
        && root.draftValue(root.touchpadTapAndDragId) === true
    readonly property bool tabletMappedRegionControlsEnabled:
        root.controlsEnabled
        && root.draftValue(root.tabletRelativeInputId) !== true
    readonly property bool statusVisible:
        !root.serviceAvailable
        || !root.writable
        || !root.catalogAvailable
        || !root.inputAvailable
        || !root.revisionTokenValid
        || !root.trustedDefinitionsValid
        || !root.trustedValuesValid
        || !root.trustedGestureActionsValid
        || !root.trustedGesturesValid
        || root.loadState === "recovered"
        || root.loadState === "defaulted"
        || root.loadState === "unsupported"
        || root.managementState !== "managed"
        || root.applyState !== "current"
        || root.requiredActivation !== "none"
        || root.displayTestActive
        || root.externalChangeWhileEditing
        || root.inputErrorMessage.length > 0
        || root.sharedErrorMessage.length > 0
        || root.busy
        || root.sharedMutationBusy
        || !root.sharedApplySafe
    readonly property bool statusIsDanger:
        root.managementState === "conflict"
        || root.applyState === "failed"
        || root.loadState === "unsupported"
        || (root.catalogAvailable && !root.trustedDefinitionsValid)
        || (root.inputGesturesProjectionAvailable
            && (!root.trustedGestureActionsValid
                || !root.trustedGesturesValid))
        || (!root.inputGesturesProjectionAvailable
            && root.inputErrorMessage.length > 0)
        || (root.inputProjectionAvailable && !root.inputAvailable
            && root.inputErrorMessage.length > 0)
        || root.externalChangeWhileEditing
    readonly property string statusMessage: {
        const inputDetail = root.inputErrorMessage.length > 0
            ? " " + root.inputErrorMessage : "";
        const sharedDetail = root.sharedErrorMessage.length > 0
            ? " " + root.sharedErrorMessage : "";
        if (!root.serviceAvailable)
            return qsTr("Input settings are unavailable. The compositor settings service may be restarting.%1").arg(sharedDetail);
        if (root.displayTestActive)
            return qsTr("A display test is active. Input changes stay locked until that test is kept or reverted.");
        if (root.managementState === "unmanaged")
            return qsTr("HyprShelld is not managing the Hyprland entrypoint yet. Review takeover from Displays before changing input settings.");
        if (root.managementState === "conflict")
            return qsTr("The managed compositor entrypoint or its ownership state changed unexpectedly. Input changes are locked to preserve it.%1").arg(sharedDetail);
        if (!root.writable)
            return qsTr("This compositor configuration is read-only and has been preserved.");
        if (!root.catalogAvailable)
            return qsTr("The trusted Hyprland option catalog is unavailable or does not match the compositor authority. Input changes are disabled.%1").arg(inputDetail);
        if (!root.trustedDefinitionsValid || !root.trustedValuesValid) {
            return qsTr("The trusted Input contract does not match this Settings build. No compositor values will be written.%1").arg(inputDetail);
        }
        if (!root.inputGesturesProjectionAvailable) {
            return root.inputErrorMessage.length > 0
                ? qsTr("Gesture authority verification failed. Global values remain readable, but the aggregate Input draft cannot be changed until the action and full-state contract is authenticated.%1").arg(inputDetail)
                : qsTr("Gesture settings are waiting for a current authenticated action catalog and full-state baseline. The aggregate Input draft remains read-only until both are available.");
        }
        if (!root.trustedGestureActionsValid || !root.trustedGesturesValid)
            return qsTr("The trusted Gestures contract does not match this Settings build. No Input values will be written.%1").arg(inputDetail);
        if (!root.revisionTokenValid)
            return qsTr("The exact compositor revision token is unavailable. Input changes are disabled to prevent overwriting another revision.");
        if (root.externalChangeWhileEditing)
            return qsTr("Compositor settings changed outside this draft. Your Input draft is preserved, but it cannot be saved over the newer revision. Load the current settings to continue.");
        if (root.busy) {
            if (root.busyOperation === "input-save")
                return qsTr("Saving the validated Input draft…");
            if (root.busyOperation === "compositor-apply"
                    || root.busyOperation === "input-apply") {
                return qsTr("Applying and verifying the saved compositor revision…");
            }
            if (root.busyOperation === "recover")
                return qsTr("Restoring and verifying the last working compositor configuration…");
            return qsTr("Another compositor operation is in progress. Input changes are temporarily locked.");
        }
        if (root.sharedMutationBusy)
            return qsTr("A shared compositor setting is changing. Input changes remain locked until that transition is verified.");
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
            return qsTr("Compositor settings were restored from the last known good desired-state copy. Review the Input values before changing them.");
        if (root.loadState === "defaulted")
            return qsTr("Compositor settings could not be recovered, so safe desired-state defaults are in use. Review the Input values before continuing.");
        if (root.loadState === "unsupported")
            return qsTr("The compositor settings use a newer format. They were preserved and remain read-only.");
        if (root.inputProjectionAvailable && !root.inputAvailable
                && root.inputErrorMessage.length > 0) {
            return qsTr("Input authority verification failed. Current input values remain readable, but changes are disabled until the managed action, schema, and full-state contract is authenticated.%1").arg(inputDetail);
        }
        if (root.inputErrorMessage.length > 0)
            return qsTr("The Input operation failed.%1").arg(inputDetail);
        if (root.sharedErrorMessage.length > 0)
            return qsTr("The compositor operation failed.%1").arg(sharedDetail);
        if (!root.inputAvailable)
            return qsTr("Input settings are waiting for a current, verified compositor baseline.%1").arg(inputDetail);
        if (!root.sharedApplySafe)
            return qsTr("A shared compositor setting is not at a verified activation point. Input controls remain locked until the exact compositor source transition is verified.");
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
        if (left && right && typeof left === "object"
                && typeof right === "object") {
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

    function gesturesEqual(left, right) {
        return root.valueEqual(left, right);
    }

    function exactKeys(value, expected) {
        return value && typeof value === "object" && !Array.isArray(value)
            && root.valueEqual(Object.keys(value).sort(), expected.slice().sort());
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

    function validateGestureActions(actions) {
        if (!Array.isArray(actions)
                || actions.length !== root.expectedGestureActionIds.length) {
            return false;
        }
        for (let index = 0; index < actions.length; ++index) {
            const action = actions[index];
            if (!root.exactKeys(action, ["id", "label", "description"])
                    || action.id !== root.expectedGestureActionIds[index]
                    || typeof action.label !== "string"
                    || action.label.length < 1 || action.label.length > 256
                    || typeof action.description !== "string"
                    || action.description.length < 1
                    || action.description.length > 512) {
                return false;
            }
        }
        return true;
    }

    function validateGestureAction(action) {
        if (!action || typeof action !== "object" || Array.isArray(action)
                || typeof action.type !== "string") {
            return false;
        }
        if (["workspace", "resize", "move", "close", "scrollMove", "unset"]
                .includes(action.type)) {
            return root.exactKeys(action, ["type"]);
        }
        if (action.type === "special") {
            return root.exactKeys(action, ["type", "workspace"])
                && root.isSchemaString(action.workspace, 256, false);
        }
        if (action.type === "float") {
            return root.exactKeys(action, ["type", "mode"])
                && ["float", "tile", "toggle"].includes(action.mode);
        }
        if (action.type === "fullscreen") {
            return root.exactKeys(action, ["type", "mode"])
                && ["fullscreen", "maximize"].includes(action.mode);
        }
        if (action.type === "cursorZoom") {
            return root.exactKeys(
                    action, ["type", "zoomLevel", "mode"]
                )
                && typeof action.zoomLevel === "number"
                && Number.isFinite(action.zoomLevel)
                && action.zoomLevel >= 0.01 && action.zoomLevel <= 100
                && ["toggle", "mult", "live"].includes(action.mode);
        }
        return false;
    }

    function canonicalGestureModifiers(modifiers) {
        const order = [
            "shift", "caps", "ctrl", "alt",
            "mod2", "mod3", "super", "mod5"
        ];
        if (!Array.isArray(modifiers))
            return [];
        return order.filter(modifier => modifiers.includes(modifier));
    }

    function gestureModifierIdentity(modifiers) {
        return root.canonicalGestureModifiers(modifiers).join("+");
    }

    function gestureAxis(direction) {
        if (["up", "down", "vertical"].includes(direction))
            return "vertical";
        if (["left", "right", "horizontal"].includes(direction))
            return "horizontal";
        if (["pinch", "pinchIn", "pinchOut"].includes(direction))
            return "pinch";
        return "swipe";
    }

    function validateGestureRecord(record) {
        if (!root.exactKeys(record, [
                "id", "fingers", "direction", "modifiers", "scale",
                "disableInhibit", "action"
            ])) {
            return false;
        }
        if (typeof record.id !== "string" || record.id.length > 128
                || !/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/.test(record.id)
                || typeof record.fingers !== "number"
                || !Number.isInteger(record.fingers)
                || record.fingers < 2 || record.fingers > 9
                || !["swipe", "left", "right", "up", "down",
                    "horizontal", "vertical", "pinch", "pinchIn",
                    "pinchOut"].includes(record.direction)
                || !Array.isArray(record.modifiers)
                || record.modifiers.length > 8
                || typeof record.scale !== "number"
                || !Number.isFinite(record.scale)
                || record.scale < 0.1 || record.scale > 10
                || typeof record.disableInhibit !== "boolean"
                || !root.validateGestureAction(record.action)) {
            return false;
        }
        const allowedModifiers = [
            "shift", "caps", "ctrl", "alt",
            "mod2", "mod3", "super", "mod5"
        ];
        const seen = new Set();
        for (const modifier of record.modifiers) {
            if (typeof modifier !== "string"
                    || !allowedModifiers.includes(modifier)
                    || seen.has(modifier)) {
                return false;
            }
            seen.add(modifier);
        }
        return true;
    }

    function validateAuthoredGestureRecord(record) {
        if (!root.validateGestureRecord(record)
                || record.action.type === "unset") {
            return false;
        }
        const pinch = ["pinch", "pinchIn", "pinchOut"]
            .includes(record.direction);
        if (pinch && record.scale !== 1)
            return false;
        if (pinch && record.action.type === "scrollMove")
            return false;
        return pinch || record.action.type !== "cursorZoom"
            || record.action.mode !== "live";
    }

    function gesturesHaveValidOrder(gestures) {
        const active = [];
        for (const gesture of gestures) {
            const modifiers = root.gestureModifierIdentity(gesture.modifiers);
            const sameTuple = candidate =>
                candidate.fingers === gesture.fingers
                && candidate.direction === gesture.direction
                && root.gestureModifierIdentity(candidate.modifiers)
                    === modifiers
                && candidate.scale === gesture.scale
                && candidate.disableInhibit === gesture.disableInhibit;
            if (gesture.action.type === "unset") {
                const match = active.findIndex(sameTuple);
                if (match < 0)
                    return false;
                active.splice(match, 1);
                continue;
            }
            const axis = root.gestureAxis(gesture.direction);
            const shadowed = active.some(prior => {
                if (prior.fingers !== gesture.fingers
                        || root.gestureModifierIdentity(prior.modifiers)
                            !== modifiers) {
                    return false;
                }
                return prior.direction === axis
                    || prior.direction === gesture.direction
                    || ((axis === "vertical" || axis === "horizontal")
                        && prior.direction === "swipe");
            });
            if (shadowed)
                return false;
            active.push(gesture);
        }
        return true;
    }

    function validateGestures(gestures) {
        if (!Array.isArray(gestures) || gestures.length > 64)
            return false;
        const ids = new Set();
        for (const record of gestures) {
            if (!root.validateGestureRecord(record) || ids.has(record.id))
                return false;
            ids.add(record.id);
        }
        return root.gesturesHaveValidOrder(gestures);
    }

    function validateGestureCompatibility(gestures, compatibility) {
        if (!Array.isArray(compatibility)
                || compatibility.length !== gestures.length) {
            return false;
        }
        for (let index = 0; index < compatibility.length; ++index) {
            const row = compatibility[index];
            if (!root.exactKeys(row, ["id", "editable", "reason"])
                    || row.id !== gestures[index].id
                    || typeof row.editable !== "boolean"
                    || typeof row.reason !== "string"
                    || row.reason.length > 512
                    || (row.editable && row.reason.length !== 0)
                    || (!row.editable && row.reason.length === 0)) {
                return false;
            }
        }
        return true;
    }

    function gestureCompatibilityForId(id) {
        if (!Array.isArray(root.inputGestureCompatibility))
            return null;
        const current = root.gestureById(id);
        const baseline = Array.isArray(root.inputGestures)
            ? root.inputGestures.find(record => record && record.id === id)
            : null;
        if (!current || !baseline || !root.valueEqual(current, baseline))
            return null;
        for (const row of root.inputGestureCompatibility) {
            if (row && row.id === id)
                return row;
        }
        return null;
    }

    function gestureIsEditable(id) {
        const row = root.gestureCompatibilityForId(id);
        return row === null || row.editable === true;
    }

    function gestureById(id) {
        if (!Array.isArray(root.draftGestures))
            return null;
        for (const gesture of root.draftGestures) {
            if (gesture && gesture.id === id)
                return gesture;
        }
        return null;
    }

    function editingGesture() {
        return root.gestureById(root.editingGestureId);
    }

    function firstUnusedGestureId() {
        const ids = new Set();
        for (const collection of [
                root.draftGestures,
                root.synchronizedGestures,
                root.inputGestures
            ]) {
            if (!Array.isArray(collection))
                continue;
            for (const record of collection) {
                if (record && typeof record.id === "string")
                    ids.add(record.id);
            }
        }
        for (let suffix = 1; ; ++suffix) {
            const id = "gesture-" + suffix;
            if (!ids.has(id))
                return id;
        }
    }

    function availableGestureSeed() {
        if (!root.draftGesturesValid || root.draftGestures.length >= 64)
            return null;
        const id = root.firstUnusedGestureId();
        if (id.length === 0)
            return null;
        const modifierOrder = [
            "shift", "caps", "ctrl", "alt",
            "mod2", "mod3", "super", "mod5"
        ];
        const fingerOrder = [3, 4, 2, 5, 6, 7, 8, 9];
        const occupied = new Set(root.draftGestures.map(record =>
            record.fingers + "|"
                + root.gestureModifierIdentity(record.modifiers)
        ));
        for (const fingers of fingerOrder) {
            for (let mask = 0; mask < 256; ++mask) {
                const modifiers = modifierOrder.filter(
                    (modifier, index) => (mask & (1 << index)) !== 0
                );
                if (occupied.has(
                        fingers + "|"
                            + root.gestureModifierIdentity(modifiers)
                    )) {
                    continue;
                }
                return {
                    id,
                    fingers,
                    direction: "swipe",
                    modifiers,
                    scale: 1,
                    disableInhibit: false,
                    action: {type: "workspace"}
                };
            }
        }
        return null;
    }

    function canAddGesture() {
        return root.controlsEnabled && root.availableGestureSeed() !== null;
    }

    function addGesture() {
        if (!root.controlsEnabled)
            return;
        const record = root.availableGestureSeed();
        if (record === null)
            return;
        const next = root.clone(root.draftGestures);
        next.push(record);
        if (!root.validateGestures(next))
            return;
        root.draftGestures = next;
        root.editingGestureId = record.id;
        root.gestureEditorIssue = "";
    }

    function replaceGesture(id, record) {
        if (!root.controlsEnabled || !root.gestureIsEditable(id)
                || !root.validateAuthoredGestureRecord(record)
                || record.id !== id) {
            root.gestureEditorIssue = qsTr("Finish every gesture value before applying this edit.");
            return false;
        }
        const next = root.clone(root.draftGestures);
        const index = next.findIndex(item => item.id === id);
        if (index < 0)
            return false;
        next[index] = root.clone(record);
        if (!root.validateGestures(next)) {
            root.gestureEditorIssue = qsTr("This change overlaps an earlier gesture or would make the ordered gesture draft invalid.");
            return false;
        }
        root.draftGestures = next;
        root.gestureEditorIssue = "";
        return true;
    }

    function removeGesture(id) {
        if (!root.controlsEnabled)
            return;
        const next = root.clone(root.draftGestures);
        const index = next.findIndex(item => item.id === id);
        if (index < 0)
            return;
        next.splice(index, 1);
        root.draftGestures = next;
        if (root.editingGestureId === id)
            root.editingGestureId = "";
        root.gestureEditorIssue = "";
    }

    function gestureMoveCandidate(id, offset) {
        if (!root.controlsEnabled || (offset !== -1 && offset !== 1))
            return null;
        const next = root.clone(root.draftGestures);
        const index = next.findIndex(item => item.id === id);
        const target = index + offset;
        if (index < 0 || target < 0 || target >= next.length)
            return null;
        const moved = next.splice(index, 1)[0];
        next.splice(target, 0, moved);
        return root.validateGestures(next) ? next : null;
    }

    function canMoveGesture(id, offset) {
        return root.gestureMoveCandidate(id, offset) !== null;
    }

    function moveGesture(id, offset) {
        const next = root.gestureMoveCandidate(id, offset);
        if (next !== null)
            root.draftGestures = next;
    }

    function closeGestureEditor() {
        root.editingGestureId = "";
        root.gestureEditorIssue = "";
    }

    function optionById(id) {
        if (!Array.isArray(root.inputOptions))
            return null;
        for (const option of root.inputOptions) {
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
            if (typeof choice === "string"
                    || (typeof choice === "number"
                        && Number.isFinite(choice))) {
                values.push(choice);
            }
            else if (choice && typeof choice === "object"
                    && (typeof choice.value === "string"
                        || (typeof choice.value === "number"
                            && Number.isFinite(choice.value)))) {
                values.push(choice.value);
            } else {
                return [];
            }
        }
        return values;
    }

    function validateStringEnumerationOption(option, id, defaultValue,
                                             choices) {
        return option && option.id === id
            && option.type === "enum"
            && option.control === "select"
            && option.defaultValue === defaultValue
            && option.min === undefined
            && option.max === undefined
            && JSON.stringify(root.choiceValues(option))
                === JSON.stringify(choices);
    }

    function validateIntegerEnumerationOption(option, id, defaultValue,
                                              minimum, maximum, choices) {
        return option && option.id === id
            && option.type === "enum"
            && option.control === "select"
            && option.defaultValue === defaultValue
            && option.min === minimum
            && option.max === maximum
            && JSON.stringify(root.choiceValues(option))
                === JSON.stringify(choices);
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

    function validateIntegerOption(option, id, defaultValue, minimum, maximum) {
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

    function validateNumberOption(option, id, defaultValue, minimum, maximum) {
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

    function validateVector2Option(option, id, defaultValue, minimum,
                                   maximum) {
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

    function validateOptions() {
        if (!Array.isArray(root.inputOptions)
                || root.inputOptions.length !== root.expectedOptionIds.length) {
            return false;
        }
        const seen = Object.create(null);
        for (let index = 0; index < root.inputOptions.length; ++index) {
            const option = root.inputOptions[index];
            if (!option || typeof option !== "object"
                    || typeof option.id !== "string" || seen[option.id]
                    || option.id !== root.expectedOptionIds[index]) {
                return false;
            }
            seen[option.id] = true;
        }
        return root.validateIntegerOption(
                root.optionById(root.repeatRateId),
                root.repeatRateId, 25, 0, 200)
            && root.validateIntegerOption(
                root.optionById(root.repeatDelayId),
                root.repeatDelayId, 600, 0, 2000)
            && root.validateNumberOption(
                root.optionById(root.sensitivityId),
                root.sensitivityId, 0, -1, 1)
            && root.validateStringEnumerationOption(
                root.optionById(root.accelerationProfileId),
                root.accelerationProfileId, "", ["", "adaptive", "flat"])
            && root.validateBooleanOption(
                root.optionById(root.naturalScrollId),
                root.naturalScrollId, false)
            && root.validateBooleanOption(
                root.optionById(root.leftHandedId),
                root.leftHandedId, false)
            && root.validateNumberOption(
                root.optionById(root.scrollFactorId),
                root.scrollFactorId, 1, 0, 2)
            && root.validateBooleanOption(
                root.optionById(root.touchpadTapToClickId),
                root.touchpadTapToClickId, true)
            && root.validateBooleanOption(
                root.optionById(root.touchpadTapAndDragId),
                root.touchpadTapAndDragId, true)
            && root.validateBooleanOption(
                root.optionById(root.touchpadNaturalScrollId),
                root.touchpadNaturalScrollId, false)
            && root.validateBooleanOption(
                root.optionById(root.touchpadDisableWhileTypingId),
                root.touchpadDisableWhileTypingId, true)
            && root.validateNumberOption(
                root.optionById(root.touchpadScrollFactorId),
                root.touchpadScrollFactorId, 1, 0, 2)
            && root.validateStringEnumerationOption(
                root.optionById(root.scrollMethodId),
                root.scrollMethodId, "",
                ["", "2fg", "edge", "on_button_down", "no_scroll"])
            && root.validateIntegerOption(
                root.optionById(root.scrollButtonId),
                root.scrollButtonId, 0, 0, 300)
            && root.validateBooleanOption(
                root.optionById(root.scrollButtonLockId),
                root.scrollButtonLockId, false)
            && root.validateIntegerEnumerationOption(
                root.optionById(root.offWindowAxisEventsId),
                root.offWindowAxisEventsId, 1, 0, 3, [0, 1, 2, 3])
            && root.validateIntegerEnumerationOption(
                root.optionById(root.emulateDiscreteScrollId),
                root.emulateDiscreteScrollId, 1, 0, 2, [0, 1, 2])
            && root.validateBooleanOption(
                root.optionById(root.touchpadClickfingerBehaviorId),
                root.touchpadClickfingerBehaviorId, false)
            && root.validateIntegerEnumerationOption(
                root.optionById(root.touchpadMultiFingerDragId),
                root.touchpadMultiFingerDragId, 0, 0, 2, [0, 1, 2])
            && root.validateIntegerEnumerationOption(
                root.optionById(root.touchpadDragLockId),
                root.touchpadDragLockId, 0, 0, 2, [0, 1, 2])
            && root.validateBooleanOption(
                root.optionById(root.touchpadFlipHorizontalId),
                root.touchpadFlipHorizontalId, false)
            && root.validateBooleanOption(
                root.optionById(root.touchpadFlipVerticalId),
                root.touchpadFlipVerticalId, false)
            && root.validateBooleanOption(
                root.optionById(root.touchpadMiddleButtonEmulationId),
                root.touchpadMiddleButtonEmulationId, false)
            && root.validateStringEnumerationOption(
                root.optionById(root.touchpadTapButtonMapId),
                root.touchpadTapButtonMapId, "", ["", "lrm", "lmr"])
            && root.validateBooleanOption(
                root.optionById(root.numLockByDefaultId),
                root.numLockByDefaultId, false)
            && root.validateIntegerEnumerationOption(
                root.optionById(root.virtualKeyboardShareStatesId),
                root.virtualKeyboardShareStatesId, 2, 0, 2, [0, 1, 2])
            && root.validateBooleanOption(
                root.optionById(
                    root.virtualKeyboardReleasePressedOnCloseId
                ),
                root.virtualKeyboardReleasePressedOnCloseId, false)
            && root.validateBooleanOption(
                root.optionById(root.virtualKeyboardNameAfterProcessId),
                root.virtualKeyboardNameAfterProcessId, true)
            && root.validateBooleanOption(
                root.optionById(root.forceNoAccelId),
                root.forceNoAccelId, false)
            && root.validateIntegerOption(
                root.optionById(root.rotationId),
                root.rotationId, 0, 0, 359)
            && root.validateBooleanOption(
                root.optionById(root.middleClickPasteId),
                root.middleClickPasteId, true)
            && root.validateIntegerOption(
                root.optionById(root.closeGestureTimeoutId),
                root.closeGestureTimeoutId, 1000, 10, 2000)
            && root.validateBooleanOption(
                root.optionById(root.touchDeviceEnabledId),
                root.touchDeviceEnabledId, true)
            && root.validateIntegerOption(
                root.optionById(root.touchDeviceTransformId),
                root.touchDeviceTransformId, 0, 0, 6)
            && root.validateBooleanOption(
                root.optionById(root.tabletRelativeInputId),
                root.tabletRelativeInputId, false)
            && root.validateBooleanOption(
                root.optionById(root.tabletLeftHandedId),
                root.tabletLeftHandedId, false)
            && root.validateIntegerOption(
                root.optionById(root.tabletTransformId),
                root.tabletTransformId, 0, 0, 6)
            && root.validateBooleanOption(
                root.optionById(root.cursorHideOnKeyPressId),
                root.cursorHideOnKeyPressId, false)
            && root.validateBooleanOption(
                root.optionById(root.cursorHideOnTouchId),
                root.cursorHideOnTouchId, true)
            && root.validateBooleanOption(
                root.optionById(root.cursorHideOnTabletId),
                root.cursorHideOnTabletId, false)
            && root.validateNumberOption(
                root.optionById(root.cursorInactiveTimeoutId),
                root.cursorInactiveTimeoutId, 0, 0, 20)
            && root.validateIntegerOption(
                root.optionById(root.cursorHotspotPaddingId),
                root.cursorHotspotPaddingId, 0, 0, 20)
            && root.validateBooleanOption(
                root.optionById(root.cursorNoWarpsId),
                root.cursorNoWarpsId, false)
            && root.validateBooleanOption(
                root.optionById(root.cursorPersistentWarpsId),
                root.cursorPersistentWarpsId, false)
            && root.validateBooleanOption(
                root.optionById(
                    root.cursorWarpBackAfterNonMouseInputId
                ),
                root.cursorWarpBackAfterNonMouseInputId, false)
            && root.validateVector2Option(
                root.optionById(root.tabletRegionPositionId),
                root.tabletRegionPositionId, [0, 0],
                [-20000, -20000], [20000, 20000])
            && root.validateBooleanOption(
                root.optionById(root.tabletAbsoluteRegionPositionId),
                root.tabletAbsoluteRegionPositionId, false)
            && root.validateVector2Option(
                root.optionById(root.tabletRegionSizeId),
                root.tabletRegionSizeId, [0, 0],
                [-100, -100], [4000, 4000])
            && root.validateBooleanOption(
                root.optionById(root.resolveBindsBySymbolId),
                root.resolveBindsBySymbolId, false);
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
                if (typeof value !== "number" || !Number.isFinite(value)
                        || !Number.isInteger(value)
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
            } else if (option.type === "enum") {
                const typeMatches = typeof value
                    === typeof option.defaultValue;
                const numericValueValid = typeof value !== "number"
                    || (Number.isFinite(value) && Number.isInteger(value));
                if (!typeMatches || !numericValueValid
                        || !root.choiceValues(option).includes(value)) {
                    return false;
                }
            } else {
                return false;
            }
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
        return root.draftValues
            && Object.prototype.hasOwnProperty.call(root.draftValues, id)
            ? root.draftValues[id] : root.optionDefault(id);
    }

    function numericDraftValue(id) {
        const value = root.draftValue(id);
        return typeof value === "number" && Number.isFinite(value) ? value : 0;
    }

    function vectorDraftComponent(id, index) {
        const value = root.draftValue(id);
        return Array.isArray(value) && value.length === 2
                && index >= 0 && index < 2
            ? value[index] : 0;
    }

    function accelerationChoices() {
        return root.choiceValues(root.optionById(root.accelerationProfileId));
    }

    function accelerationLabels() {
        return [qsTr("Automatic"), qsTr("Adaptive"), qsTr("Flat")];
    }

    function accelerationIndex(value) {
        const index = root.accelerationChoices().indexOf(value);
        return index >= 0 ? index : 0;
    }

    function choiceIndex(id, value) {
        const index = root.choiceValues(root.optionById(id)).indexOf(value);
        return index >= 0 ? index : 0;
    }

    function canonicalInteractionValue(id, value) {
        if (typeof value !== "number" || !Number.isFinite(value))
            return null;
        const step = id === root.sensitivityId ? 0.05 : 0.10;
        const decimals = id === root.sensitivityId ? 2 : 1;
        let result = Number((Math.round(value / step) * step).toFixed(decimals));
        if (Object.is(result, -0))
            result = 0;
        return result;
    }

    function setDraftValue(id, value) {
        if (!root.controlsEnabled || !root.trustedDefinitionsValid)
            return;
        const next = root.clone(root.draftValues);
        if (!next)
            return;
        next[id] = value;
        if (!root.validateValues(next))
            return;
        root.draftValues = next;
    }

    function setSteppedDraftValue(id, value) {
        const canonical = root.canonicalInteractionValue(id, value);
        if (canonical !== null)
            root.setDraftValue(id, canonical);
    }

    function setExactVectorComponentDraftValue(id, index, value) {
        if (!root.tabletMappedRegionControlsEnabled
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

    function synchronizeDraft() {
        if (!root.serviceAvailable || !root.inputProjectionAvailable
                || !root.inputGesturesProjectionAvailable
                || !root.revisionTokenValid
                || !root.trustedValuesValid || !root.trustedGesturesValid
                || root.busy || root.sharedMutationBusy) {
            return;
        }
        const next = root.clone(root.inputValues);
        const gestures = root.clone(root.inputGestures);
        if (!next || !gestures)
            return;
        root.synchronizedValues = root.clone(next);
        root.draftValues = next;
        root.synchronizedGestures = root.clone(gestures);
        root.draftGestures = gestures;
        root.synchronizedRevisionToken = root.revisionToken;
        root.projectionInitialized = true;
        root.externalChangeWhileEditing = false;
        root.saveSubmitted = false;
        root.submittedValues = ({});
        root.submittedGestures = [];
        root.submittedRevisionToken = "";
        root.closeGestureEditor();
    }

    function resetTargetValues() {
        if (!root.trustedDefinitionsValid)
            return null;
        const defaults = {};
        for (const id of root.expectedOptionIds)
            defaults[id] = root.optionDefault(id);
        return root.validateValues(defaults) ? defaults : null;
    }

    function resetDraftToDefaults() {
        if (!root.controlsEnabled || !root.trustedDefinitionsValid)
            return;
        const target = root.resetTargetValues();
        if (target) {
            root.draftValues = target;
            root.draftGestures = [];
            root.closeGestureEditor();
        }
    }

    function submitDraft() {
        if (!root.saveEnabled)
            return;
        const candidate = root.clone(root.draftValues);
        const gestures = root.clone(root.draftGestures);
        if (!candidate || !gestures || !root.validateValues(candidate)
                || !root.validateGestures(gestures)) {
            return;
        }
        root.saveSubmitted = true;
        root.submittedValues = root.clone(candidate);
        root.submittedGestures = root.clone(gestures);
        root.submittedRevisionToken = root.revisionToken;
        root.saveRequested(candidate, gestures);
        root.scheduleProjectionReview();
    }

    function reviewProjection() {
        if (!root.serviceAvailable || !root.trustedValuesValid
                || !root.trustedGesturesValid) {
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
        if (root.sharedMutationBusy) {
            return;
        }
        if (!root.projectionInitialized) {
            root.synchronizeDraft();
            return;
        }
        if (root.saveSubmitted) {
            if (root.busy)
                return;
            if (root.valuesEqual(root.inputValues, root.submittedValues)
                    && root.gesturesEqual(
                        root.inputGestures, root.submittedGestures
                    )) {
                root.synchronizeDraft();
                return;
            }
            if (root.revisionToken === root.submittedRevisionToken) {
                root.saveSubmitted = false;
                root.submittedValues = ({});
                root.submittedGestures = [];
                root.submittedRevisionToken = "";
                return;
            }
            root.saveSubmitted = false;
            root.externalChangeWhileEditing = true;
            return;
        }
        const projectionChanged = root.revisionToken
                !== root.synchronizedRevisionToken
            || !root.valuesEqual(root.inputValues, root.synchronizedValues)
            || !root.gesturesEqual(
                root.inputGestures, root.synchronizedGestures
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

    onInputOptionsChanged: root.scheduleProjectionReview()
    onInputValuesChanged: root.scheduleProjectionReview()
    onInputGesturesChanged: root.scheduleProjectionReview()
    onInputGestureCompatibilityChanged: root.scheduleProjectionReview()
    onInputGestureActionsChanged: root.scheduleProjectionReview()
    onInputGesturesProjectionAvailableChanged:
        root.scheduleProjectionReview()
    onRevisionTokenChanged: root.scheduleProjectionReview()
    onServiceAvailableChanged: root.scheduleProjectionReview()
    onBusyChanged: root.scheduleProjectionReview()
    onSharedMutationBusyChanged: root.scheduleProjectionReview()
    onInputErrorNameChanged: root.scheduleProjectionReview()
    onInputErrorMessageChanged: root.scheduleProjectionReview()
    onSharedErrorNameChanged: root.scheduleProjectionReview()
    onSharedErrorMessageChanged: root.scheduleProjectionReview()
    onApplyStateChanged: root.scheduleProjectionReview()
    onStatusIsDangerChanged: {
        if (!root.statusIsDanger)
            return;
        Qt.callLater(function() {
            if (root.statusIsDanger)
                inputOptionsScrollView.contentItem.contentY = 0;
        });
    }
    Component.onCompleted: root.scheduleProjectionReview()

    background: Rectangle { color: root.palette.window }

    ScrollView {
        id: inputOptionsScrollView

        objectName: "inputOptionsScrollView"
        anchors.fill: parent
        anchors.topMargin: root.compactPage
            ? Math.min(root.contentTopMargin, 12)
            : root.contentTopMargin
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: inputOptionsContent

            objectName: "inputOptionsContent"
            x: Math.max(24, (root.width - width) / 2)
            width: Math.max(0, Math.min(root.width - 48, 980))
            spacing: root.compactPage ? 16 : 20

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 3

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Input")
                        color: root.palette.text
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        objectName: "inputIntroduction"
                        Layout.fillWidth: true
                        text: {
                            if (root.inputTabIndex === 0) {
                                return qsTr("Tune global keyboard, virtual-keyboard, mouse, pointer, cursor visibility and placement, touchpad, touch-device, and drawing-tablet behavior through the managed compositor configuration.");
                            }
                            if (root.inputTabIndex === 1) {
                                return qsTr("Inspect a read-only observation of the current Hyprland session and preserved saved device settings.");
                            }
                            return qsTr("Create an ordered set of authenticated touchpad gesture bindings in the same Input draft.");
                        }
                        color: root.palette.placeholderText
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }

                Button {
                    objectName: "refreshInputButton"
                    visible: root.inputTabIndex !== 1
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Refresh")
                    enabled: !root.busy && !root.displayTestActive
                    icon.name: "view-refresh-symbolic"
                    Accessible.name: qsTr("Refresh compositor input settings")

                    onClicked: root.refreshRequested()
                }
            }

            Frame {
                objectName: "inputStatusCard"
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
                        objectName: "inputStatusMessage"
                        Layout.fillWidth: true
                        text: root.statusMessage
                        color: root.statusIsDanger ? ShellTheme.onErrorContainer : ShellTheme.onWarningContainer
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
                            objectName: "inputOpenDisplaysButton"
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
                            objectName: "loadCurrentInputButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight + topPadding + bottomPadding
                            )
                            visible: root.externalChangeWhileEditing
                            text: qsTr("Load current settings")
                            enabled: !root.busy && !root.sharedMutationBusy
                                && !root.saveSubmitted
                                && root.inputProjectionAvailable
                                && root.trustedValuesValid
                                && root.inputGesturesProjectionAvailable
                                && root.trustedGesturesValid
                            Accessible.name: qsTr("Discard this Input draft and load the current compositor settings")

                            onClicked: root.synchronizeDraft()
                        }

                        Button {
                            objectName: "retryApplyInputButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight + topPadding + bottomPadding
                            )
                            visible: root.retryApplyAvailable
                            text: root.busyOperation === "compositor-apply"
                                || root.busyOperation === "input-apply"
                                ? qsTr("Retrying apply…")
                                : qsTr("Retry apply")
                            enabled: root.retryApplyAvailable && !root.busy
                                && !root.sharedMutationBusy
                                && root.sharedApplySafe
                            Accessible.name: qsTr("Retry applying the exact saved compositor revision")

                            onClicked: root.retryApplyRequested()
                        }

                        Button {
                            objectName: "recoverInputButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight + topPadding + bottomPadding
                            )
                            visible: root.recoveryAvailable
                            text: qsTr("Restore last working configuration")
                            enabled: root.recoveryAvailable && !root.busy
                                && !root.sharedMutationBusy
                            Accessible.name: qsTr("Review whole-compositor recovery")

                            onClicked: inputRecoveryDialog.open()
                        }
                    }
                }
            }

            TabBar {
                id: inputTabBar

                objectName: "inputTabBar"
                Layout.fillWidth: true
                currentIndex: root.inputTabIndex

                onCurrentIndexChanged: {
                    root.inputTabIndex = currentIndex;
                    if (currentIndex !== 2)
                        root.closeGestureEditor();
                }

                TabButton {
                    objectName: "inputGlobalTab"
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: root.globalDraftDirty
                        ? qsTr("Global •") : qsTr("Global")
                    Accessible.name: root.globalDraftDirty
                        ? qsTr("Global input settings, unsaved draft")
                        : qsTr("Global input settings")
                }

                TabButton {
                    objectName: "inputDevicesTab"
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Devices")
                    Accessible.name: qsTr("Read-only input-device diagnostics")
                }

                TabButton {
                    objectName: "inputGesturesTab"
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: root.gesturesDraftDirty
                        ? qsTr("Gestures •") : qsTr("Gestures")
                    Accessible.name: root.gesturesDraftDirty
                        ? qsTr("Gesture bindings, unsaved draft")
                        : qsTr("Gesture bindings")
                }
            }

            Label {
                objectName: "inputGlobalDraftStatus"
                Layout.fillWidth: true
                visible: (root.inputTabIndex === 1 && root.draftDirty)
                    || (root.inputTabIndex === 0 && root.gesturesDraftDirty)
                    || (root.inputTabIndex === 2 && root.globalDraftDirty)
                text: {
                    if (root.inputTabIndex === 1) {
                        return qsTr("An unsaved Input draft is preserved. Return to Global or Gestures to review it.");
                    }
                    if (root.inputTabIndex === 0) {
                        return qsTr("Unsaved gesture changes are preserved in this same Input draft.");
                    }
                    return qsTr("Unsaved Global changes are preserved in this same Input draft.");
                }
                color: root.palette.highlight
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            InputDevicesPane {
                objectName: "inputDevicesPane"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: root.inputTabIndex === 1
                discoveryAvailable: root.inputDeviceDiscoveryAvailable
                discoveryBusy: root.inputDeviceDiscoveryBusy
                connectedDevices: root.connectedInputDevices
                observedAtMs: root.inputDevicesObservedAtMs
                inventoryDigest: root.inputDeviceInventoryDigest
                unaddressableCounts: root.inputDeviceUnaddressableCounts
                discoveryErrorName: root.inputDeviceDiscoveryErrorName
                discoveryErrorMessage: root.inputDeviceDiscoveryErrorMessage
                projectionAvailable: root.inputDeviceProjectionAvailable
                savedDevices: root.savedInputDevices
                otherSavedDevices: root.otherSavedInputDevices
                projectionRevisionToken:
                    root.inputDeviceProjectionRevisionToken
                projectionInventoryDigest:
                    root.inputDeviceProjectionInventoryDigest
                projectionErrorName: root.inputDeviceProjectionErrorName
                projectionErrorMessage:
                    root.inputDeviceProjectionErrorMessage
                minimumTargetSize: root.minimumTargetSize

                onRefreshRequested:
                    root.refreshConnectedInputDevicesRequested()
                onManageProfilesRequested:
                    root.manageInputDeviceProfilesRequested()
            }

            Frame {
                objectName: "inputGestureBehaviorCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 2
                    && root.editingGestureId.length === 0
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
                            text: qsTr("Close action")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("This timeout belongs to managed Close gesture bindings and is saved with the complete Input draft.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Close timeout")
                        description: qsTr("Set the timeout, in milliseconds, for closing a window through the Close gesture action. The default is 1000 ms.")
                        from: root.optionMinimum(root.closeGestureTimeoutId)
                        to: root.optionMaximum(root.closeGestureTimeoutId)
                        value: Number(root.draftValue(
                            root.closeGestureTimeoutId
                        )) || 0
                        enabled: root.controlsEnabled
                        controlObjectName: "inputGestureCloseTimeout"
                        accessibleName: qsTr("Maximum close gesture time in milliseconds")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.closeGestureTimeoutId, value
                        )
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Settings validates and persists gesture structure, but physical gesture behavior remains unverified until the later live acceptance pass.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }
            }

            Frame {
                objectName: "inputGestureListCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 2
                    && root.editingGestureId.length === 0
                padding: 18

                background: Rectangle {
                    color: root.palette.base
                    radius: 16
                    border.color: root.palette.mid
                }

                GestureSummaryList {
                    anchors.fill: parent
                    gestures: root.draftGestures
                    controlsEnabled: root.controlsEnabled
                    draftValid: root.draftGesturesValid
                    canAddGesture: root.canAddGesture()
                    minimumTargetSize: root.minimumTargetSize
                    compatibilityForId: id =>
                        root.gestureCompatibilityForId(id)
                    canMoveGesture: (id, offset) =>
                        root.canMoveGesture(id, offset)

                    onAddRequested: root.addGesture()
                    onEditRequested: id => {
                        if (root.gestureIsEditable(id)) {
                            root.editingGestureId = id;
                            root.gestureEditorIssue = "";
                        }
                    }
                    onMoveRequested: (id, offset) =>
                        root.moveGesture(id, offset)
                    onRemoveRequested: id => root.removeGesture(id)
                }
            }

            GestureEditor {
                objectName: "inputGestureEditor"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: inputOptionsContent.width
                Layout.maximumWidth: inputOptionsContent.width
                implicitWidth: inputOptionsContent.width
                visible: root.inputTabIndex === 2
                    && root.editingGestureId.length > 0
                gesture: root.editingGesture()
                controlsEnabled: root.controlsEnabled
                issue: root.gestureEditorIssue
                minimumTargetSize: root.minimumTargetSize

                onRecordModified: record => root.replaceGesture(
                    root.editingGestureId, record
                )
                onCloseRequested: root.closeGestureEditor()
                onRemoveRequested: id => root.removeGesture(id)
            }

            Frame {
                objectName: "inputKeyboardCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                        text: qsTr("Keyboard")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Repeat rate")
                        description: qsTr("Set how many times per second a held key repeats.")
                        from: root.optionMinimum(root.repeatRateId)
                        to: root.optionMaximum(root.repeatRateId)
                        value: Number(root.draftValue(root.repeatRateId)) || 0
                        enabled: root.controlsEnabled
                        controlObjectName: "inputRepeatRate"
                        accessibleName: qsTr("Keyboard repeat rate in repeats per second")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value =>
                            root.setDraftValue(root.repeatRateId, value)
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Repeat delay")
                        description: qsTr("Set how many milliseconds a key is held before repeating begins.")
                        from: root.optionMinimum(root.repeatDelayId)
                        to: root.optionMaximum(root.repeatDelayId)
                        value: Number(root.draftValue(root.repeatDelayId)) || 0
                        enabled: root.controlsEnabled
                        controlObjectName: "inputRepeatDelay"
                        accessibleName: qsTr("Keyboard repeat delay in milliseconds")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value =>
                            root.setDraftValue(root.repeatDelayId, value)
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Num Lock by default")
                        description: qsTr("Engage Num Lock when Hyprland configures a keyboard. Turning this off does not clear a Num Lock state that is already set.")
                        checked: root.draftValue(
                            root.numLockByDefaultId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputNumLockByDefault"
                        accessibleName: qsTr("Engage Num Lock when configuring a keyboard")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.numLockByDefaultId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Shortcuts follow the active layout")
                        description: qsTr("Resolve symbol-based shortcuts from each keyboard's active layout instead of the primary globally configured layout. Exact saved per-device values win. Keycode-based shortcuts are unchanged.")
                        checked: root.draftValue(
                            root.resolveBindsBySymbolId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputResolveBindsBySymbol"
                        accessibleName: qsTr("Resolve symbol-based shortcuts from each keyboard's active layout")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.resolveBindsBySymbolId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "inputVirtualKeyboardCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                            text: qsTr("Virtual keyboards")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            objectName: "inputVirtualKeyboardLifecycleCopy"
                            Layout.fillWidth: true
                            text: qsTr("These settings apply without device discovery. State sharing and device names update when a virtual keyboard next connects; releasing held keys is checked when it closes.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("Share key states")
                        description: qsTr("Choose whether virtual keyboards combine their pressed keys and modifiers with other keyboards.")
                        model: [
                            qsTr("Never"),
                            qsTr("Always"),
                            qsTr("Except input methods")
                        ]
                        currentIndex: root.choiceIndex(
                            root.virtualKeyboardShareStatesId,
                            root.draftValue(
                                root.virtualKeyboardShareStatesId
                            )
                        )
                        enabled: root.controlsEnabled
                        controlWidth: root.compactPage ? 160 : 190
                        controlObjectName: "inputVirtualKeyboardShareStates"
                        accessibleName: qsTr("Virtual keyboard key state sharing")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => {
                            const choices = root.choiceValues(root.optionById(
                                root.virtualKeyboardShareStatesId
                            ));
                            if (index >= 0 && index < choices.length) {
                                root.setDraftValue(
                                    root.virtualKeyboardShareStatesId,
                                    choices[index]
                                );
                            }
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Release held keys on close")
                        description: qsTr("Send release events for keys still held when a virtual keyboard closes.")
                        checked: root.draftValue(
                            root.virtualKeyboardReleasePressedOnCloseId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "inputVirtualKeyboardReleasePressedOnClose"
                        accessibleName: qsTr("Release virtual keyboard keys when it closes")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.virtualKeyboardReleasePressedOnCloseId,
                            value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Name after creating application")
                        description: qsTr("Name newly connected virtual keyboards after their creating process instead of using a generic name. This can change which future per-device rules match.")
                        checked: root.draftValue(
                            root.virtualKeyboardNameAfterProcessId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "inputVirtualKeyboardNameAfterProcess"
                        accessibleName: qsTr("Name virtual keyboards after their creating application")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.virtualKeyboardNameAfterProcessId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "inputMouseCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                        text: qsTr("Mouse")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    SettingsSliderRow {
                        Layout.fillWidth: true
                        title: qsTr("Pointer sensitivity")
                        description: qsTr("Adjust pointer response from slower (-1.00) to faster (1.00).")
                        from: root.optionMinimum(root.sensitivityId)
                        to: root.optionMaximum(root.sensitivityId)
                        value: root.numericDraftValue(root.sensitivityId)
                        stepSize: 0.05
                        decimals: 2
                        enabled: root.pointerAccelerationControlsEnabled
                        controlObjectName: "inputSensitivity"
                        valueObjectName: "inputSensitivityValue"
                        accessibleName: qsTr("Mouse pointer sensitivity")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setSteppedDraftValue(
                            root.sensitivityId, value
                        )
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("Acceleration profile")
                        description: qsTr("Automatic follows the device default; Adaptive changes with speed; Flat keeps a constant response.")
                        model: root.accelerationLabels()
                        currentIndex: root.accelerationIndex(
                            root.draftValue(root.accelerationProfileId)
                        )
                        enabled: root.pointerAccelerationControlsEnabled
                        controlObjectName: "inputAccelerationProfile"
                        accessibleName: qsTr("Mouse acceleration profile")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => {
                            const choices = root.accelerationChoices();
                            if (index >= 0 && index < choices.length)
                                root.setDraftValue(
                                    root.accelerationProfileId,
                                    choices[index]
                                );
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Natural scrolling")
                        description: qsTr("Move content in the same direction as the scroll gesture.")
                        checked: root.draftValue(root.naturalScrollId) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputNaturalScroll"
                        accessibleName: qsTr("Mouse natural scrolling")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value =>
                            root.setDraftValue(root.naturalScrollId, value)
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Left-handed buttons")
                        description: qsTr("Swap the primary and secondary mouse buttons.")
                        checked: root.draftValue(root.leftHandedId) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputLeftHanded"
                        accessibleName: qsTr("Left-handed mouse buttons")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value =>
                            root.setDraftValue(root.leftHandedId, value)
                    }

                    SettingsSliderRow {
                        Layout.fillWidth: true
                        title: qsTr("Scroll speed")
                        description: qsTr("Multiply external mouse scroll movement from 0.0 to 2.0.")
                        from: root.optionMinimum(root.scrollFactorId)
                        to: root.optionMaximum(root.scrollFactorId)
                        value: root.numericDraftValue(root.scrollFactorId)
                        stepSize: 0.10
                        decimals: 1
                        valueSuffix: "×"
                        enabled: root.controlsEnabled
                        controlObjectName: "inputScrollFactor"
                        valueObjectName: "inputScrollFactorValue"
                        accessibleName: qsTr("Mouse scroll speed multiplier")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setSteppedDraftValue(
                            root.scrollFactorId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "inputPointerBehaviorCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                        text: qsTr("Pointer behavior")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Raw cursor movement")
                        description: qsTr("Move Hyprland's compositor cursor using unaccelerated device motion. Pointer sensitivity and Acceleration profile remain saved but do not affect that cursor while this is on.")
                        checked: root.draftValue(root.forceNoAccelId) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputForceNoAccel"
                        accessibleName: qsTr("Use unaccelerated device motion for the Hyprland cursor")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value =>
                            root.setDraftValue(root.forceNoAccelId, value)
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Pointer rotation")
                        description: qsTr("Rotate motion clockwise for compatible pointing devices. Unsupported devices ignore this setting.")
                        from: root.optionMinimum(root.rotationId)
                        to: root.optionMaximum(root.rotationId)
                        value: Number(root.draftValue(root.rotationId)) || 0
                        enabled: root.controlsEnabled
                        controlObjectName: "inputRotation"
                        accessibleName: qsTr("Pointing device rotation in clockwise degrees")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value =>
                            root.setDraftValue(root.rotationId, value)
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Middle-click paste")
                        description: qsTr("Allow applications to update the primary selection used for middle-click paste. Turning this off takes effect when an application next tries to set that selection.")
                        checked: root.draftValue(
                            root.middleClickPasteId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputMiddleClickPaste"
                        accessibleName: qsTr("Allow primary-selection updates for middle-click paste")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.middleClickPasteId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "inputCursorVisibilityCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                        text: qsTr("Cursor visibility")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Hide after keyboard input")
                        description: qsTr("Hide the cursor after a keyboard key event until physical mouse movement.")
                        checked: root.draftValue(
                            root.cursorHideOnKeyPressId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputCursorHideOnKeyPress"
                        accessibleName: qsTr("Hide the cursor after keyboard input")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.cursorHideOnKeyPressId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Hide after touch input")
                        description: qsTr("Hide the cursor after touch input. Hyprland can keep it hidden through the first following mouse movement; a subsequent movement reveals it.")
                        checked: root.draftValue(
                            root.cursorHideOnTouchId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputCursorHideOnTouch"
                        accessibleName: qsTr("Hide the cursor after touch input")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.cursorHideOnTouchId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Hide after tablet input")
                        description: qsTr("Hide the cursor after drawing-tablet input. Hyprland can keep it hidden through the first following mouse movement; a subsequent movement reveals it.")
                        checked: root.draftValue(
                            root.cursorHideOnTabletId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputCursorHideOnTablet"
                        accessibleName: qsTr("Hide the cursor after drawing-tablet input")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.cursorHideOnTabletId, value
                        )
                    }

                    SettingsSliderRow {
                        Layout.fillWidth: true
                        title: qsTr("Inactivity timeout")
                        description: qsTr("Hide the cursor after this many seconds without pointer, touch, or tablet movement or a pointer-button event. Zero disables only inactivity-based hiding. Hyprland checks every 500 ms; wheel scrolling alone does not restart the timer.")
                        from: root.optionMinimum(
                            root.cursorInactiveTimeoutId
                        )
                        to: root.optionMaximum(
                            root.cursorInactiveTimeoutId
                        )
                        value: root.numericDraftValue(
                            root.cursorInactiveTimeoutId
                        )
                        stepSize: 0.10
                        decimals: 1
                        valueSuffix: " s"
                        enabled: root.controlsEnabled
                        controlObjectName: "inputCursorInactiveTimeout"
                        valueObjectName: "inputCursorInactiveTimeoutValue"
                        accessibleName: qsTr("Cursor inactivity timeout in seconds")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value =>
                            root.setSteppedDraftValue(
                                root.cursorInactiveTimeoutId, value
                            )
                    }
                }
            }

            Frame {
                objectName: "inputCursorPlacementCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                        text: qsTr("Cursor placement")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Edge padding")
                        description: qsTr("Clamp a hotspot-centered square by checking its corners against the active display layout on the next pointer move or warp. At a display seam the square may span adjacent displays. This does not change cursor artwork.")
                        from: root.optionMinimum(
                            root.cursorHotspotPaddingId
                        )
                        to: root.optionMaximum(
                            root.cursorHotspotPaddingId
                        )
                        value: Number(root.draftValue(
                            root.cursorHotspotPaddingId
                        )) || 0
                        enabled: root.controlsEnabled
                        controlObjectName: "inputCursorHotspotPadding"
                        accessibleName: qsTr("Cursor hotspot layout padding in logical pixels")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.cursorHotspotPaddingId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Suppress ordinary pointer jumps")
                        description: qsTr("Prevent ordinary compositor actions from moving the pointer automatically. Explicitly forced moves can still occur.")
                        checked: root.draftValue(
                            root.cursorNoWarpsId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputCursorNoWarps"
                        accessibleName: qsTr("Suppress ordinary automatic pointer jumps")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.cursorNoWarpsId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Remember position in each window")
                        description: qsTr("When an action moves the pointer to a refocused window, return to its last remembered position there instead of the center. This does not force a move when ordinary jumps are suppressed.")
                        checked: root.draftValue(
                            root.cursorPersistentWarpsId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputCursorPersistentWarps"
                        accessibleName: qsTr("Remember the pointer position in each window")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.cursorPersistentWarpsId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Restore mouse position after other input")
                        description: qsTr("When the pointer was last moved by non-mouse input, return to the last physical-mouse position when the mouse is next used. This return is not blocked by Suppress ordinary pointer jumps.")
                        checked: root.draftValue(
                            root.cursorWarpBackAfterNonMouseInputId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "inputCursorWarpBackAfterNonMouseInput"
                        accessibleName: qsTr("Restore the physical mouse position after non-mouse input")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.cursorWarpBackAfterNonMouseInputId,
                            value
                        )
                    }
                }
            }

            Frame {
                objectName: "inputTouchpadCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                            text: qsTr("Touchpad")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            objectName: "inputTouchpadAvailabilityCopy"
                            Layout.fillWidth: true
                            text: qsTr("These choices apply when a touchpad is present. HyprShelld does not need to detect one before preserving your settings.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Tap to click")
                        description: qsTr("Use one-, two-, or three-finger taps for primary, secondary, or middle click.")
                        checked:
                            root.draftValue(root.touchpadTapToClickId) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchpadTapToClick"
                        accessibleName: qsTr("Touchpad tap to click")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.touchpadTapToClickId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Tap and drag")
                        description: qsTr("Keep dragging after a tap while the finger remains on the touchpad.")
                        checked:
                            root.draftValue(root.touchpadTapAndDragId) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchpadTapAndDrag"
                        accessibleName: qsTr("Touchpad tap and drag")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.touchpadTapAndDragId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Natural scrolling")
                        description: qsTr("Move content in the same direction as the touchpad gesture.")
                        checked: root.draftValue(
                            root.touchpadNaturalScrollId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchpadNaturalScroll"
                        accessibleName: qsTr("Touchpad natural scrolling")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.touchpadNaturalScrollId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Disable while typing")
                        description: qsTr("Temporarily ignore touchpad movement while typing.")
                        checked: root.draftValue(
                            root.touchpadDisableWhileTypingId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchpadDisableWhileTyping"
                        accessibleName: qsTr("Disable the touchpad while typing")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.touchpadDisableWhileTypingId, value
                        )
                    }

                    SettingsSliderRow {
                        Layout.fillWidth: true
                        title: qsTr("Scroll speed")
                        description: qsTr("Multiply touchpad scroll movement from 0.0 to 2.0.")
                        from: root.optionMinimum(root.touchpadScrollFactorId)
                        to: root.optionMaximum(root.touchpadScrollFactorId)
                        value: root.numericDraftValue(
                            root.touchpadScrollFactorId
                        )
                        stepSize: 0.10
                        decimals: 1
                        valueSuffix: "×"
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchpadScrollFactor"
                        valueObjectName: "inputTouchpadScrollFactorValue"
                        accessibleName: qsTr("Touchpad scroll speed multiplier")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setSteppedDraftValue(
                            root.touchpadScrollFactorId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "inputTouchpadButtonsGesturesCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                            text: qsTr("Touchpad buttons & gestures")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            objectName: "inputTouchpadButtonsGesturesAvailabilityCopy"
                            Layout.fillWidth: true
                            text: qsTr("These choices remain available without hardware detection. Hyprland applies each one only when the touchpad and its input capabilities support it.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Click with fingers")
                        description: qsTr("Map physical one-, two-, or three-finger clickpad presses to primary, secondary, or middle click instead of using button areas.")
                        checked: root.draftValue(
                            root.touchpadClickfingerBehaviorId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchpadClickfingerBehavior"
                        accessibleName: qsTr("Use finger count for physical touchpad clicks")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.touchpadClickfingerBehaviorId, value
                        )
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("Tap button order")
                        description: qsTr("Choose what one-, two-, and three-finger taps send while Tap to click is on.")
                        model: [
                            qsTr("Automatic"),
                            qsTr("Primary, secondary, middle"),
                            qsTr("Primary, middle, secondary")
                        ]
                        currentIndex: root.choiceIndex(
                            root.touchpadTapButtonMapId,
                            root.draftValue(root.touchpadTapButtonMapId)
                        )
                        enabled: root.touchpadTapMappingEnabled
                        controlWidth: root.compactPage ? 178 : 220
                        controlObjectName: "inputTouchpadTapButtonMap"
                        accessibleName: qsTr("Touchpad tap button order")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => {
                            const choices = root.choiceValues(
                                root.optionById(root.touchpadTapButtonMapId)
                            );
                            if (index >= 0 && index < choices.length)
                                root.setDraftValue(
                                    root.touchpadTapButtonMapId,
                                    choices[index]
                                );
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Two-button middle click")
                        description: qsTr("Treat pressing the primary and secondary physical buttons together as a middle click.")
                        checked: root.draftValue(
                            root.touchpadMiddleButtonEmulationId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchpadMiddleButtonEmulation"
                        accessibleName: qsTr("Emulate a middle click with two touchpad buttons")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.touchpadMiddleButtonEmulationId, value
                        )
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("Drag lock")
                        description: qsTr("Keep tap-and-drag active after lifting a finger. Timed permits a short lift; Sticky waits for another tap.")
                        model: [
                            qsTr("Off"),
                            qsTr("Timed"),
                            qsTr("Sticky")
                        ]
                        currentIndex: root.choiceIndex(
                            root.touchpadDragLockId,
                            root.draftValue(root.touchpadDragLockId)
                        )
                        enabled: root.touchpadDragLockEnabled
                        controlObjectName: "inputTouchpadDragLock"
                        accessibleName: qsTr("Touchpad drag lock mode")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => {
                            const choices = root.choiceValues(
                                root.optionById(root.touchpadDragLockId)
                            );
                            if (index >= 0 && index < choices.length)
                                root.setDraftValue(
                                    root.touchpadDragLockId, choices[index]
                                );
                        }
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("Multi-finger drag")
                        description: qsTr("Use three or four fingers to press and drag on compatible touchpads.")
                        model: [
                            qsTr("Off"),
                            qsTr("Three fingers"),
                            qsTr("Four fingers")
                        ]
                        currentIndex: root.choiceIndex(
                            root.touchpadMultiFingerDragId,
                            root.draftValue(root.touchpadMultiFingerDragId)
                        )
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchpadMultiFingerDrag"
                        accessibleName: qsTr("Touchpad multi-finger drag mode")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => {
                            const choices = root.choiceValues(
                                root.optionById(root.touchpadMultiFingerDragId)
                            );
                            if (index >= 0 && index < choices.length)
                                root.setDraftValue(
                                    root.touchpadMultiFingerDragId,
                                    choices[index]
                                );
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Reverse horizontal movement")
                        description: qsTr("Reverse left and right pointer movement from the touchpad.")
                        checked: root.draftValue(
                            root.touchpadFlipHorizontalId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchpadFlipHorizontal"
                        accessibleName: qsTr("Reverse horizontal touchpad pointer movement")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.touchpadFlipHorizontalId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Reverse vertical movement")
                        description: qsTr("Reverse up and down pointer movement from the touchpad.")
                        checked: root.draftValue(
                            root.touchpadFlipVerticalId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchpadFlipVertical"
                        accessibleName: qsTr("Reverse vertical touchpad pointer movement")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.touchpadFlipVerticalId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "inputTouchDeviceCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                            text: qsTr("Touch devices")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            objectName: "inputTouchDeviceAvailabilityCopy"
                            Layout.fillWidth: true
                            text: qsTr("These managed values are global fallbacks for compatible libinput touch devices. An exact saved per-device override wins. The Devices tab remains read-only diagnostics and does not prove that a device is present or supports either setting.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Enable touch input")
                        description: qsTr("Allow input from compatible libinput touch devices. This global fallback is on by default; an exact saved per-device value can override it.")
                        checked: root.draftValue(
                            root.touchDeviceEnabledId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchDeviceEnabled"
                        accessibleName: qsTr("Enable input from compatible libinput touch devices")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.touchDeviceEnabledId, value
                        )
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Touch transform code")
                        description: qsTr("Use 0 for identity (normal), 1 for rotate 90°, 2 for rotate 180°, 3 for rotate 270°, 4 for flipped, 5 for flipped + 90°, or 6 for flipped + 180°. A compatible libinput touch device applies this only when it exposes calibration-matrix support.")
                        from: root.optionMinimum(
                            root.touchDeviceTransformId
                        )
                        to: root.optionMaximum(
                            root.touchDeviceTransformId
                        )
                        value: Number(root.draftValue(
                            root.touchDeviceTransformId
                        )) || 0
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTouchDeviceTransform"
                        accessibleName: qsTr("Touch device transform code from zero through six")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.touchDeviceTransformId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "inputDrawingTabletCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                            text: qsTr("Drawing tablets")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            objectName: "inputDrawingTabletAvailabilityCopy"
                            Layout.fillWidth: true
                            text: qsTr("These managed values are global fallbacks for compatible libinput drawing tablets. An exact saved per-device override wins. The Devices tab remains read-only diagnostics and does not prove that a tablet is present or supports these settings.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Relative tablet motion")
                        description: qsTr("Move the pointer by pen-motion deltas on compatible libinput tablets instead of using absolute pen placement. This global fallback is off by default; tablet tools reported as mouse tools still move relatively.")
                        checked: root.draftValue(
                            root.tabletRelativeInputId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTabletRelativeInput"
                        accessibleName: qsTr("Use relative pointer motion for compatible libinput tablets")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.tabletRelativeInputId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Left-handed tablet orientation")
                        description: qsTr("Ask libinput to rotate a compatible drawing tablet by 180°. This global fallback is off by default; an exact saved per-device value can override it.")
                        checked: root.draftValue(
                            root.tabletLeftHandedId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTabletLeftHanded"
                        accessibleName: qsTr("Use left-handed orientation for compatible libinput tablets")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.tabletLeftHandedId, value
                        )
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Tablet transform code")
                        description: qsTr("Use 0 for identity (normal), 1 for rotate 90°, 2 for rotate 180°, 3 for rotate 270°, 4 for flipped, 5 for flipped + 90°, or 6 for flipped + 180° on compatible libinput tablets.")
                        from: root.optionMinimum(root.tabletTransformId)
                        to: root.optionMaximum(root.tabletTransformId)
                        value: Number(root.draftValue(
                            root.tabletTransformId
                        )) || 0
                        enabled: root.controlsEnabled
                        controlObjectName: "inputTabletTransform"
                        accessibleName: qsTr("Drawing tablet transform code from zero through six")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.tabletTransformId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "inputTabletMappedRegionCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                            text: qsTr("Tablet mapped region")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            objectName: "inputTabletMappedRegionAvailabilityCopy"
                            Layout.fillWidth: true
                            text: qsTr("These global fallbacks map absolute pen input for compatible libinput drawing tablets. Exact saved per-device values win. Relative tablet motion keeps these values but does not use them.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }
                    }

                    SettingsDecimalRow {
                        objectName: "inputTabletRegionPositionXRow"
                        Layout.fillWidth: true
                        title: qsTr("Mapped position X")
                        description: qsTr("Offset the mapped output or complete monitor layout horizontally. With no saved output binding and Exact layout position on, this is an exact compositor-space X position.")
                        value: root.vectorDraftComponent(
                            root.tabletRegionPositionId, 0
                        )
                        minimumValue: root.optionComponentMinimum(
                            root.tabletRegionPositionId, 0
                        )
                        maximumValue: root.optionComponentMaximum(
                            root.tabletRegionPositionId, 0
                        )
                        controlWidth: root.compactPage ? 160 : 190
                        controlObjectName: "inputTabletRegionPositionX"
                        validationObjectName:
                            "inputTabletRegionPositionXValidation"
                        validationExample: "125.5"
                        accessibleName: qsTr("Mapped tablet position X in logical layout units")
                        minimumTargetSize: root.minimumTargetSize
                        enabled: root.tabletMappedRegionControlsEnabled

                        onValueModified: value =>
                            root.setExactVectorComponentDraftValue(
                                root.tabletRegionPositionId, 0, value
                            )
                    }

                    SettingsDecimalRow {
                        objectName: "inputTabletRegionPositionYRow"
                        Layout.fillWidth: true
                        title: qsTr("Mapped position Y")
                        description: qsTr("Offset the mapped output or complete monitor layout vertically. With no saved output binding and Exact layout position on, this is an exact compositor-space Y position.")
                        value: root.vectorDraftComponent(
                            root.tabletRegionPositionId, 1
                        )
                        minimumValue: root.optionComponentMinimum(
                            root.tabletRegionPositionId, 1
                        )
                        maximumValue: root.optionComponentMaximum(
                            root.tabletRegionPositionId, 1
                        )
                        controlWidth: root.compactPage ? 160 : 190
                        controlObjectName: "inputTabletRegionPositionY"
                        validationObjectName:
                            "inputTabletRegionPositionYValidation"
                        validationExample: "-80.25"
                        accessibleName: qsTr("Mapped tablet position Y in logical layout units")
                        minimumTargetSize: root.minimumTargetSize
                        enabled: root.tabletMappedRegionControlsEnabled

                        onValueModified: value =>
                            root.setExactVectorComponentDraftValue(
                                root.tabletRegionPositionId, 1, value
                            )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Exact layout position")
                        description: qsTr("With no saved per-device output binding, use X and Y as exact compositor-space coordinates instead of offsets from the complete monitor layout. A saved output binding always keeps them as offsets.")
                        checked: root.draftValue(
                            root.tabletAbsoluteRegionPositionId
                        ) === true
                        enabled: root.tabletMappedRegionControlsEnabled
                        controlObjectName:
                            "inputTabletAbsoluteRegionPosition"
                        accessibleName: qsTr("Use exact compositor-space tablet position when no output is bound")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.tabletAbsoluteRegionPositionId, value
                        )
                    }

                    SettingsDecimalRow {
                        objectName: "inputTabletRegionSizeWidthRow"
                        Layout.fillWidth: true
                        title: qsTr("Mapped width")
                        description: qsTr("Replace the selected width only when both width and height have magnitude at least 0.000000001. A negative width reverses the horizontal axis.")
                        value: root.vectorDraftComponent(
                            root.tabletRegionSizeId, 0
                        )
                        minimumValue: root.optionComponentMinimum(
                            root.tabletRegionSizeId, 0
                        )
                        maximumValue: root.optionComponentMaximum(
                            root.tabletRegionSizeId, 0
                        )
                        controlWidth: root.compactPage ? 160 : 190
                        controlObjectName: "inputTabletRegionSizeWidth"
                        validationObjectName:
                            "inputTabletRegionSizeWidthValidation"
                        validationExample: "1920"
                        accessibleName: qsTr("Mapped tablet width in logical layout units")
                        minimumTargetSize: root.minimumTargetSize
                        enabled: root.tabletMappedRegionControlsEnabled

                        onValueModified: value =>
                            root.setExactVectorComponentDraftValue(
                                root.tabletRegionSizeId, 0, value
                            )
                    }

                    SettingsDecimalRow {
                        objectName: "inputTabletRegionSizeHeightRow"
                        Layout.fillWidth: true
                        title: qsTr("Mapped height")
                        description: qsTr("Replace the selected height only when both width and height have magnitude at least 0.000000001. A negative height reverses the vertical axis.")
                        value: root.vectorDraftComponent(
                            root.tabletRegionSizeId, 1
                        )
                        minimumValue: root.optionComponentMinimum(
                            root.tabletRegionSizeId, 1
                        )
                        maximumValue: root.optionComponentMaximum(
                            root.tabletRegionSizeId, 1
                        )
                        controlWidth: root.compactPage ? 160 : 190
                        controlObjectName: "inputTabletRegionSizeHeight"
                        validationObjectName:
                            "inputTabletRegionSizeHeightValidation"
                        validationExample: "1080"
                        accessibleName: qsTr("Mapped tablet height in logical layout units")
                        minimumTargetSize: root.minimumTargetSize
                        enabled: root.tabletMappedRegionControlsEnabled

                        onValueModified: value =>
                            root.setExactVectorComponentDraftValue(
                                root.tabletRegionSizeId, 1, value
                            )
                    }
                }
            }

            Frame {
                objectName: "inputAdvancedScrollingCard"
                Layout.fillWidth: true
                visible: root.inputTabIndex === 0
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
                            text: qsTr("Advanced scrolling")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            objectName: "inputAdvancedScrollingAvailabilityCopy"
                            Layout.fillWidth: true
                            text: qsTr("These global choices apply to compatible pointing devices. A device can ignore a scrolling method it does not support.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("Scroll method")
                        description: qsTr("Choose Automatic, two-finger, edge, button, or disabled scrolling for compatible pointing devices.")
                        model: [
                            qsTr("Automatic"),
                            qsTr("Two-finger"),
                            qsTr("Edge"),
                            qsTr("Button scrolling"),
                            qsTr("Disabled")
                        ]
                        currentIndex: root.choiceIndex(
                            root.scrollMethodId,
                            root.draftValue(root.scrollMethodId)
                        )
                        enabled: root.controlsEnabled
                        controlObjectName: "inputScrollMethod"
                        accessibleName: qsTr("Pointing device scroll method")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => {
                            const choices = root.choiceValues(
                                root.optionById(root.scrollMethodId)
                            );
                            if (index >= 0 && index < choices.length)
                                root.setDraftValue(
                                    root.scrollMethodId, choices[index]
                                );
                        }
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Scroll button")
                        description: qsTr("Set the button number held for Button scrolling. Zero uses the device default.")
                        from: root.optionMinimum(root.scrollButtonId)
                        to: root.optionMaximum(root.scrollButtonId)
                        value: Number(root.draftValue(root.scrollButtonId)) || 0
                        enabled: root.buttonScrollingEnabled
                        controlObjectName: "inputScrollButton"
                        accessibleName: qsTr("Button number used for button scrolling")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value =>
                            root.setDraftValue(root.scrollButtonId, value)
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Keep button scrolling active")
                        description: qsTr("Let Button scrolling remain active without holding the selected button.")
                        checked: root.draftValue(
                            root.scrollButtonLockId
                        ) === true
                        enabled: root.buttonScrollingEnabled
                        controlObjectName: "inputScrollButtonLock"
                        accessibleName: qsTr("Keep button scrolling active without holding the button")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.scrollButtonLockId, value
                        )
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("Scroll outside a window")
                        description: qsTr("Choose how scrolling is delivered when the pointer is outside the focused window.")
                        model: [
                            qsTr("Ignore"),
                            qsTr("Send to window"),
                            qsTr("Clamp to window edge"),
                            qsTr("Move pointer to window edge")
                        ]
                        currentIndex: root.choiceIndex(
                            root.offWindowAxisEventsId,
                            root.draftValue(root.offWindowAxisEventsId)
                        )
                        enabled: root.controlsEnabled
                        controlWidth: root.compactPage ? 170 : 210
                        controlObjectName: "inputOffWindowAxisEvents"
                        accessibleName: qsTr("Scrolling outside the focused window")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => {
                            const choices = root.choiceValues(
                                root.optionById(root.offWindowAxisEventsId)
                            );
                            if (index >= 0 && index < choices.length)
                                root.setDraftValue(
                                    root.offWindowAxisEventsId,
                                    choices[index]
                                );
                        }
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("High-resolution wheel compatibility")
                        description: qsTr("Choose when Hyprland synthesizes traditional wheel steps from high-resolution wheel events.")
                        model: [
                            qsTr("Off"),
                            qsTr("When needed"),
                            qsTr("All wheel events")
                        ]
                        currentIndex: root.choiceIndex(
                            root.emulateDiscreteScrollId,
                            root.draftValue(root.emulateDiscreteScrollId)
                        )
                        enabled: root.controlsEnabled
                        controlObjectName: "inputEmulateDiscreteScroll"
                        accessibleName: qsTr("High-resolution wheel compatibility")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => {
                            const choices = root.choiceValues(
                                root.optionById(root.emulateDiscreteScrollId)
                            );
                            if (index >= 0 && index < choices.length)
                                root.setDraftValue(
                                    root.emulateDiscreteScrollId,
                                    choices[index]
                                );
                        }
                    }
                }
            }

            Frame {
                objectName: "inputDraftActions"
                Layout.fillWidth: true
                visible: root.inputTabIndex !== 1
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
                                    ? qsTr("Unsaved Input draft")
                                    : qsTr("No Input changes")
                            color: root.palette.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.externalChangeWhileEditing
                                ? qsTr("Load the current settings before creating a new draft. HyprShelld never silently rebases this draft onto another compositor revision.")
                                : qsTr("Save & apply persists all Global values and the ordered gesture collection as one validated Input revision, then reloads and verifies that exact revision. Reset to defaults also clears every managed gesture binding.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }

                    Label {
                        objectName: "inputGlobalValuesValidationMessage"
                        Layout.fillWidth: true
                        visible: root.draftDirty && !root.draftValuesValid
                        text: qsTr("Correct the highlighted Global values before saving.")
                        color: ShellTheme.onErrorContainer
                        font.pixelSize: 12
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
                            objectName: "discardInputDraftButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight + topPadding + bottomPadding
                            )
                            text: qsTr("Discard draft")
                            visible: root.draftDirty
                                && !root.externalChangeWhileEditing
                            enabled: !root.busy && !root.saveSubmitted
                                && !root.sharedMutationBusy
                                && root.trustedValuesValid
                                && root.trustedGesturesValid
                            Accessible.name: qsTr("Discard the complete Global values and gestures Input draft")

                            onClicked: root.synchronizeDraft()
                        }

                        Button {
                            objectName: "resetInputDefaultsButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight + topPadding + bottomPadding
                            )
                            text: qsTr("Reset to defaults")
                            enabled: {
                                const target = root.resetTargetValues();
                                return root.controlsEnabled
                                    && target !== null
                                    && (!root.valuesEqual(
                                            root.draftValues, target
                                        )
                                        || root.draftGestures.length > 0);
                            }
                            Accessible.name: qsTr("Reset all Input values to trusted defaults and clear every managed gesture binding")

                            onClicked: root.resetDraftToDefaults()
                        }

                        Button {
                            objectName: "saveInputButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight + topPadding + bottomPadding
                            )
                            text: {
                                if (root.busyOperation === "input-save")
                                    return qsTr("Saving…");
                                if (root.busyOperation === "compositor-apply"
                                        || root.busyOperation === "input-apply") {
                                    return qsTr("Applying…");
                                }
                                return qsTr("Save & apply");
                            }
                            highlighted: true
                            enabled: root.saveEnabled
                            Accessible.name: qsTr("Save and apply the validated Global values and gestures Input draft")

                            onClicked: root.submitDraft()
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 12 }
        }
    }

    CompositorRecoveryDialog {
        id: inputRecoveryDialog

        objectName: "inputRecoveryDialog"
        recoveryAvailable: root.recoveryAvailable
        operationBusy: root.busy || root.sharedMutationBusy
        busyOperation: root.busyOperation
        settingsAreaName: qsTr("Input")
        warningObjectName: "inputRecoveryWarning"
        cancelObjectName: "cancelInputRecoveryButton"
        confirmObjectName: "confirmInputRecoveryButton"
        minimumTargetSize: root.minimumTargetSize

        onRecoveryRequested: root.recoveryRequested()
    }
}
