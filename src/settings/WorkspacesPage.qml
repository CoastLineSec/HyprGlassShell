pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    property bool serviceAvailable: false
    property bool writable: false
    property bool catalogAvailable: false
    property bool workspacesAvailable: false
    property bool workspacesProjectionAvailable: false
    property bool workspaceRulesProjectionAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property var workspacesOptions: []
    property var workspacesValues: ({})
    property var workspaceRules: []
    property string revisionToken: "0"
    property double appliedRevision: 0
    property string loadState: "unavailable"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property string workspacesErrorName: ""
    property string workspacesErrorMessage: ""
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
    property var draftWorkspaceRules: []
    property var synchronizedWorkspaceRules: []
    property var submittedWorkspaceRules: []
    property string synchronizedRevisionToken: ""
    property string submittedRevisionToken: ""
    property bool projectionInitialized: false
    property bool externalChangeWhileEditing: false
    property bool saveSubmitted: false
    property int workspacesTabIndex: 0
    property string editingWorkspaceRuleId: ""

    signal refreshRequested()
    signal openDisplaysRequested()
    signal saveRequested(var values, var workspaceRules)
    signal retryApplyRequested()
    signal recoveryRequested()

    readonly property string workspaceWraparoundId:
        "hyprland.animations.workspace_wraparound"
    readonly property string swipeCancelRatioId:
        "hyprland.gestures.workspace_swipe_cancel_ratio"
    readonly property string swipeCreateNewId:
        "hyprland.gestures.workspace_swipe_create_new"
    readonly property string swipeDirectionLockId:
        "hyprland.gestures.workspace_swipe_direction_lock"
    readonly property string swipeDirectionLockThresholdId:
        "hyprland.gestures.workspace_swipe_direction_lock_threshold"
    readonly property string swipeDistanceId:
        "hyprland.gestures.workspace_swipe_distance"
    readonly property string swipeForeverId:
        "hyprland.gestures.workspace_swipe_forever"
    readonly property string swipeInvertId:
        "hyprland.gestures.workspace_swipe_invert"
    readonly property string swipeMinimumSpeedId:
        "hyprland.gestures.workspace_swipe_min_speed_to_force"
    readonly property string swipeTouchId:
        "hyprland.gestures.workspace_swipe_touch"
    readonly property string swipeTouchInvertId:
        "hyprland.gestures.workspace_swipe_touch_invert"
    readonly property string swipeUseRelativeId:
        "hyprland.gestures.workspace_swipe_use_r"
    readonly property string closeSpecialOnEmptyId:
        "hyprland.misc.close_special_on_empty"
    readonly property string initialWorkspaceTrackingId:
        "hyprland.misc.initial_workspace_tracking"
    readonly property string initialWorkspaceTokenTimeoutId:
        "hyprland.misc.initial_workspace_token_timeout"
    readonly property string allowWorkspaceCyclesId:
        "hyprland.binds.allow_workspace_cycles"
    readonly property string hideSpecialOnWorkspaceChangeId:
        "hyprland.binds.hide_special_on_workspace_change"
    readonly property string workspaceBackAndForthId:
        "hyprland.binds.workspace_back_and_forth"
    readonly property string workspaceCenterOnId:
        "hyprland.binds.workspace_center_on"
    readonly property string warpOnChangeWorkspaceId:
        "hyprland.cursor.warp_on_change_workspace"
    readonly property string warpOnToggleSpecialId:
        "hyprland.cursor.warp_on_toggle_special"
    readonly property real minimumTargetSize: 44
    readonly property bool compactPage: root.width < 560
    readonly property var expectedOptionIds: [
        root.workspaceWraparoundId,
        root.swipeCancelRatioId,
        root.swipeCreateNewId,
        root.swipeDirectionLockId,
        root.swipeDirectionLockThresholdId,
        root.swipeDistanceId,
        root.swipeForeverId,
        root.swipeInvertId,
        root.swipeMinimumSpeedId,
        root.swipeTouchId,
        root.swipeTouchInvertId,
        root.swipeUseRelativeId,
        root.closeSpecialOnEmptyId,
        root.initialWorkspaceTrackingId,
        root.initialWorkspaceTokenTimeoutId,
        root.allowWorkspaceCyclesId,
        root.hideSpecialOnWorkspaceChangeId,
        root.workspaceBackAndForthId,
        root.workspaceCenterOnId,
        root.warpOnChangeWorkspaceId,
        root.warpOnToggleSpecialId
    ]
    readonly property bool trustedDefinitionsValid: root.validateOptions()
    readonly property bool trustedValuesValid:
        root.workspacesProjectionAvailable
        && root.trustedDefinitionsValid
        && root.validateValues(root.workspacesValues)
    readonly property bool trustedWorkspaceRulesValid:
        root.workspaceRulesProjectionAvailable
        && root.validateWorkspaceRuleCollection(root.workspaceRules, false)
    readonly property bool revisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.revisionToken)
    readonly property bool synchronizedRevisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.synchronizedRevisionToken)
    readonly property bool draftValid:
        root.trustedDefinitionsValid && root.validateValues(root.draftValues)
        && root.validateWorkspaceRuleCollection(
            root.draftWorkspaceRules, false
        )
    readonly property bool draftDirty:
        root.projectionInitialized
        && (!root.valuesEqual(root.draftValues, root.synchronizedValues)
            || !root.valueEqual(
                root.draftWorkspaceRules,
                root.synchronizedWorkspaceRules
            ))
    readonly property bool displayTestActive:
        root.confirmationState !== "idle"
        || root.managementState === "preview"
    readonly property bool controlsEnabled:
        root.serviceAvailable
        && root.writable
        && root.catalogAvailable
        && root.workspacesAvailable
        && root.revisionTokenValid
        && root.trustedDefinitionsValid
        && root.trustedValuesValid
        && root.trustedWorkspaceRulesValid
        && !root.busy
        && !root.saveSubmitted
        && !root.sharedMutationBusy
        && root.sharedApplySafe
        && !root.externalChangeWhileEditing
        && !root.displayTestActive
    readonly property bool saveEnabled:
        root.controlsEnabled && root.draftDirty && root.draftValid
        && !root.saveSubmitted && root.sharedApplySafe
    readonly property bool initialTokenTimeoutEnabled:
        root.controlsEnabled
        && root.draftValue(root.initialWorkspaceTrackingId) === 1
    readonly property bool directionLockThresholdEnabled:
        root.controlsEnabled
        && root.draftValue(root.swipeDirectionLockId) === true
    readonly property bool touchInvertEnabled:
        root.controlsEnabled
        && root.draftValue(root.swipeTouchId) === true
    readonly property bool workspaceRuleEditorActive:
        root.workspaceRuleById(root.editingWorkspaceRuleId) !== null
    readonly property var editingWorkspaceRule:
        root.workspaceRuleById(root.editingWorkspaceRuleId)
    readonly property string workspaceRuleEditorIssue:
        root.currentWorkspaceRuleIssue()
    readonly property var editingWorkspaceRuleInvalidOverrideKeys:
        root.currentWorkspaceRuleInvalidOverrideKeys()
    readonly property var workspaceRuleOverrideDefinitions: [
        root.workspaceRuleField(
            "gaps_in", qsTr("Inner gaps"),
            qsTr("Set the signed top, right, bottom, and left gaps between tiled windows."),
            "cssGap", [5, 5, 5, 5],
            "workspaceRuleOverrideGapsIn", "spacing"
        ),
        root.workspaceRuleField(
            "gaps_out", qsTr("Outer gaps"),
            qsTr("Set the signed top, right, bottom, and left gaps between windows and monitor edges."),
            "cssGap", [20, 20, 20, 20],
            "workspaceRuleOverrideGapsOut", "spacing"
        ),
        root.workspaceRuleField(
            "float_gaps", qsTr("Floating-window gaps"),
            qsTr("Set the signed top, right, bottom, and left monitor-edge gaps for floating windows."),
            "cssGap", [0, 0, 0, 0],
            "workspaceRuleOverrideFloatGaps", "spacing"
        ),
        root.workspaceRuleField(
            "border_size", qsTr("Border size"),
            qsTr("Set the full lossless signed safe-integer border size for this workspace."),
            "safeInteger", -1,
            "workspaceRuleOverrideBorderSize", "spacing"
        ),
        root.workspaceRuleField(
            "no_border", qsTr("Disable borders"),
            qsTr("Set whether windows on this workspace omit their border."),
            "boolean", true,
            "workspaceRuleOverrideNoBorder", "appearance"
        ),
        root.workspaceRuleField(
            "no_rounding", qsTr("Disable rounding"),
            qsTr("Set whether windows on this workspace omit rounded corners."),
            "boolean", true,
            "workspaceRuleOverrideNoRounding", "appearance"
        ),
        root.workspaceRuleField(
            "decorate", qsTr("Decorations"),
            qsTr("Set whether Hyprland draws decorations for windows on this workspace."),
            "boolean", true,
            "workspaceRuleOverrideDecorate", "appearance"
        ),
        root.workspaceRuleField(
            "no_shadow", qsTr("Disable shadows"),
            qsTr("Set whether windows on this workspace omit their shadow."),
            "boolean", true,
            "workspaceRuleOverrideNoShadow", "appearance"
        ),
        root.workspaceRuleField(
            "default_name", qsTr("Default workspace name"),
            qsTr("Give the workspace this name when Hyprland creates it."),
            "text", "",
            "workspaceRuleOverrideDefaultName", "identity",
            { maximumLength: 256, allowEmpty: true }
        ),
        root.workspaceRuleField(
            "animation", qsTr("Workspace animation"),
            qsTr("Select one pinned transition style for this workspace."),
            "workspaceAnimation", "",
            "workspaceRuleOverrideAnimation", "identity"
        ),
        root.workspaceRuleField(
            "layout_opts", qsTr("Layout engine details"),
            qsTr("Optionally set Master orientation and Scrolling direction for this workspace."),
            "layoutOptions", {},
            "workspaceRuleOverrideLayoutOptions", "identity"
        )
    ]
    readonly property bool resetTargetDiffers: {
        const target = root.resetTargetValues();
        return target !== null
            && (!root.valuesEqual(root.draftValues, target)
                || root.draftWorkspaceRules.length > 0);
    }
    readonly property bool statusVisible:
        !root.serviceAvailable
        || !root.writable
        || !root.catalogAvailable
        || !root.workspacesAvailable
        || !root.workspaceRulesProjectionAvailable
        || !root.revisionTokenValid
        || !root.trustedDefinitionsValid
        || !root.trustedValuesValid
        || !root.trustedWorkspaceRulesValid
        || root.loadState === "recovered"
        || root.loadState === "defaulted"
        || root.loadState === "unsupported"
        || root.managementState !== "managed"
        || root.applyState !== "current"
        || root.requiredActivation !== "none"
        || root.displayTestActive
        || root.externalChangeWhileEditing
        || root.workspacesErrorMessage.length > 0
        || root.sharedErrorMessage.length > 0
        || root.busy
        || root.sharedMutationBusy
        || !root.sharedApplySafe
    readonly property bool statusIsDanger:
        root.managementState === "conflict"
        || root.applyState === "failed"
        || root.loadState === "unsupported"
        || (root.catalogAvailable && !root.trustedDefinitionsValid)
        || (root.workspacesProjectionAvailable
            && !root.workspacesAvailable
            && root.workspacesErrorMessage.length > 0)
        || (root.workspaceRulesProjectionAvailable
            && !root.trustedWorkspaceRulesValid)
        || root.externalChangeWhileEditing
    readonly property string statusMessage: {
        const workspaceDetail = root.workspacesErrorMessage.length > 0
            ? " " + root.workspacesErrorMessage : "";
        const sharedDetail = root.sharedErrorMessage.length > 0
            ? " " + root.sharedErrorMessage : "";
        if (!root.serviceAvailable)
            return qsTr("Workspace settings are unavailable. The compositor settings service may be restarting.%1").arg(sharedDetail);
        if (root.displayTestActive)
            return qsTr("A display test is active. Workspace changes stay locked until that test is kept or reverted.");
        if (root.managementState === "unmanaged")
            return qsTr("HyprShelld is not managing the Hyprland entrypoint yet. Review takeover from Displays before changing workspace behavior.");
        if (root.managementState === "conflict")
            return qsTr("The managed compositor entrypoint or its ownership state changed unexpectedly. Workspace changes are locked to preserve it.%1").arg(sharedDetail);
        if (!root.writable)
            return qsTr("This compositor configuration is read-only and has been preserved.");
        if (!root.catalogAvailable)
            return qsTr("The trusted Hyprland option catalog is unavailable or does not match the compositor authority. Workspace changes are disabled.%1").arg(workspaceDetail);
        if (!root.trustedDefinitionsValid || !root.trustedValuesValid)
            return qsTr("The trusted Workspaces contract does not match this Settings build. No compositor values will be written.%1").arg(workspaceDetail);
        if (!root.workspaceRulesProjectionAvailable) {
            return root.workspacesErrorMessage.length > 0
                ? qsTr("Workspace Rules authority verification failed. Workspace behavior values may remain readable, but the combined draft cannot be changed until the protected and user-rule projection is trusted.%1").arg(workspaceDetail)
                : qsTr("Workspace Rules are waiting for a current verified projection. The protected maximized-window rule remains internal and cannot be edited here.");
        }
        if (!root.trustedWorkspaceRulesValid)
            return qsTr("The current user Workspace Rules projection is not a valid managed-v1 collection. No compositor values will be written.%1").arg(workspaceDetail);
        if (!root.revisionTokenValid)
            return qsTr("The exact compositor revision token is unavailable. Workspace changes are disabled to prevent overwriting another revision.");
        if (root.externalChangeWhileEditing)
            return qsTr("Compositor settings changed outside this draft. Your Workspaces draft is preserved, but it cannot be saved over the newer revision. Load the current settings to continue.");
        if (root.busy) {
            if (root.busyOperation === "workspaces-save")
                return qsTr("Saving the validated Workspaces draft…");
            if (root.busyOperation === "compositor-apply"
                    || root.busyOperation === "workspaces-apply") {
                return qsTr("Applying and verifying the saved compositor revision…");
            }
            if (root.busyOperation === "recover")
                return qsTr("Restoring and verifying the last working compositor configuration…");
            return qsTr("Another compositor operation is in progress. Workspace changes are temporarily locked.");
        }
        if (root.sharedMutationBusy)
            return qsTr("A shared compositor setting is changing. Workspace changes remain locked until that transition is verified.");
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
            return qsTr("Compositor settings were restored from the last known good desired-state copy. Review the workspace values before changing them.");
        if (root.loadState === "defaulted")
            return qsTr("Compositor settings could not be recovered, so safe desired-state defaults are in use. Review the workspace values before continuing.");
        if (root.loadState === "unsupported")
            return qsTr("The compositor settings use a newer format. They were preserved and remain read-only.");
        if (root.workspacesProjectionAvailable
                && !root.workspacesAvailable
                && root.workspacesErrorMessage.length > 0) {
            return qsTr("Workspaces authority verification failed. Current workspace values remain readable, but changes are disabled until the managed action, schema, and full-state contract is authenticated.%1").arg(workspaceDetail);
        }
        if (root.workspacesErrorMessage.length > 0)
            return qsTr("The Workspaces operation failed.%1").arg(workspaceDetail);
        if (root.sharedErrorMessage.length > 0)
            return qsTr("The compositor operation failed.%1").arg(sharedDetail);
        if (!root.workspacesAvailable)
            return qsTr("Workspace settings are waiting for a current, verified compositor baseline.%1").arg(workspaceDetail);
        if (!root.sharedApplySafe)
            return qsTr("A shared compositor setting is not at a verified activation point. Workspace controls remain locked until the exact compositor source transition is verified.");
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

    function workspaceRuleField(
        key, title, description, kind, defaultValue,
        controlObjectName, group, extra
    ) {
        const result = {
            key,
            title,
            description,
            kind,
            defaultValue,
            controlObjectName,
            includeObjectName: controlObjectName + "Include",
            group
        };
        if (extra && typeof extra === "object") {
            for (const name of Object.keys(extra))
                result[name] = extra[name];
        }
        return result;
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

    function isUnicodeFormatCharacter(codePoint) {
        return codePoint === 0x00AD
            || (codePoint >= 0x0600 && codePoint <= 0x0605)
            || codePoint === 0x061C
            || codePoint === 0x06DD
            || codePoint === 0x070F
            || (codePoint >= 0x0890 && codePoint <= 0x0891)
            || codePoint === 0x08E2
            || codePoint === 0x180E
            || (codePoint >= 0x200B && codePoint <= 0x200F)
            || (codePoint >= 0x202A && codePoint <= 0x202E)
            || (codePoint >= 0x2060 && codePoint <= 0x2064)
            || (codePoint >= 0x2066 && codePoint <= 0x206F)
            || codePoint === 0xFEFF
            || (codePoint >= 0xFFF9 && codePoint <= 0xFFFB)
            || codePoint === 0x110BD
            || codePoint === 0x110CD
            || (codePoint >= 0x13430 && codePoint <= 0x1343F)
            || (codePoint >= 0x1BCA0 && codePoint <= 0x1BCA3)
            || (codePoint >= 0x1D173 && codePoint <= 0x1D17A)
            || codePoint === 0xE0001
            || (codePoint >= 0xE0020 && codePoint <= 0xE007F);
    }

    function isSchemaString(value, maximumLength, allowEmpty) {
        return typeof value === "string"
            && value.length <= maximumLength
            && (allowEmpty || value.length > 0)
            && value === value.normalize("NFC")
            && !root.hasDisallowedCharacter(value);
    }

    function isStableWorkspaceRuleId(value) {
        return typeof value === "string" && value.length >= 1
            && value.length <= 128
            && value !== "hyprshelld.internal.shared-spacing.maximized"
            && /^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/.test(value);
    }

    function isWorkspaceRuleSelector(value, allowIncomplete) {
        if (allowIncomplete && (value === "" || value === "name:"
                || value === "special:")) {
            return true;
        }
        if (typeof value !== "string")
            return false;
        if (/^[1-9][0-9]*$/.test(value)) {
            const numeric = Number(value);
            return Number.isInteger(numeric) && numeric <= 2147483647;
        }
        return /^name:[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$/.test(value)
            || /^special(?::[A-Za-z0-9_][A-Za-z0-9_.-]{0,127})?$/.test(value);
    }

    function isStaticMonitorSelector(value, allowIncomplete) {
        if (typeof value !== "string")
            return false;
        if (value === "")
            return true;
        if (allowIncomplete && value === "desc:")
            return true;
        if (value.startsWith("desc:")) {
            const description = value.slice(5);
            return description.length >= 1 && description.length <= 256
                && description === description.trim()
                && description === description.normalize("NFC")
                && !root.hasDisallowedCharacter(description);
        }
        return /^[A-Za-z][A-Za-z0-9_.-]{0,127}$/.test(value)
            && !["current", "left", "right", "up", "down"].includes(value);
    }

    function isWorkspaceAnimation(value) {
        return typeof value === "string" && value.length <= 128
            && /^(?:|fade|(?:slide|slidevert|slidefade|slidefadevert)(?: (?:top|bottom|left|right))?(?: (?:0|[1-9][0-9]?|100)%)?)$/.test(value);
    }

    function validateWorkspaceRuleOverride(key, value) {
        if (["gaps_in", "gaps_out", "float_gaps"].includes(key)) {
            return Array.isArray(value) && value.length === 4
                && value.every(item => typeof item === "number"
                    && Number.isSafeInteger(item));
        }
        if (key === "border_size")
            return typeof value === "number" && Number.isSafeInteger(value);
        if (["no_border", "no_rounding", "decorate", "no_shadow"]
                .includes(key)) {
            return typeof value === "boolean";
        }
        if (key === "default_name")
            return root.isSchemaString(value, 256, true);
        if (key === "animation")
            return root.isWorkspaceAnimation(value);
        if (key === "layout_opts") {
            if (!value || typeof value !== "object" || Array.isArray(value))
                return false;
            const allowed = ["orientation", "direction"];
            for (const option of Object.keys(value)) {
                if (!allowed.includes(option))
                    return false;
                if (option === "orientation"
                        && !["left", "right", "top", "bottom", "center"]
                            .includes(value[option])) {
                    return false;
                }
                if (option === "direction"
                        && !["left", "right", "up", "down"]
                            .includes(value[option])) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    function workspaceRuleInvalidOverrideKeys(record) {
        if (!record || !record.overrides
                || typeof record.overrides !== "object"
                || Array.isArray(record.overrides)) {
            return ["overrides"];
        }
        const invalid = [];
        for (const key of Object.keys(record.overrides)) {
            if (!root.validateWorkspaceRuleOverride(
                    key, record.overrides[key])) {
                invalid.push(key);
            }
        }
        return invalid;
    }

    function validateWorkspaceRuleRecord(record, allowIncomplete) {
        if (!record || typeof record !== "object" || Array.isArray(record))
            return false;
        const required = [
            "id", "selector", "enabled", "monitor", "persistent",
            "isDefault", "layout", "overrides"
        ];
        if (!root.valueEqual(Object.keys(record).sort(), required.sort()))
            return false;
        return root.isStableWorkspaceRuleId(record.id)
            && root.isWorkspaceRuleSelector(record.selector, allowIncomplete)
            && typeof record.enabled === "boolean"
            && root.isStaticMonitorSelector(record.monitor, allowIncomplete)
            && typeof record.persistent === "boolean"
            && typeof record.isDefault === "boolean"
            && ["", "dwindle", "master", "scrolling", "monocle"]
                .includes(record.layout)
            && root.workspaceRuleInvalidOverrideKeys(record).length === 0;
    }

    function validateWorkspaceRuleCollection(rules, allowIncomplete) {
        if (!Array.isArray(rules) || rules.length > 1024)
            return false;
        const ids = new Set();
        const selectors = new Set();
        for (const record of rules) {
            if (!root.validateWorkspaceRuleRecord(record, allowIncomplete)
                    || ids.has(record.id)) {
                return false;
            }
            if (!allowIncomplete || root.isWorkspaceRuleSelector(
                    record.selector, false)) {
                if (selectors.has(record.selector))
                    return false;
                selectors.add(record.selector);
            }
            ids.add(record.id);
        }
        return true;
    }

    function workspaceRuleIndex(id) {
        for (let index = 0; index < root.draftWorkspaceRules.length; ++index) {
            if (root.draftWorkspaceRules[index]
                    && root.draftWorkspaceRules[index].id === id) {
                return index;
            }
        }
        return -1;
    }

    function workspaceRuleById(id) {
        const index = root.workspaceRuleIndex(id);
        return index >= 0 ? root.draftWorkspaceRules[index] : null;
    }

    function replaceWorkspaceRule(id, record) {
        if (!root.controlsEnabled)
            return;
        const rules = root.clone(root.draftWorkspaceRules);
        const index = root.workspaceRuleIndex(id);
        if (!rules || index < 0)
            return;
        rules[index] = record;
        root.draftWorkspaceRules = rules;
    }

    function setWorkspaceRuleProperty(id, propertyName, value) {
        const record = root.clone(root.workspaceRuleById(id));
        if (!record)
            return;
        record[propertyName] = value;
        root.replaceWorkspaceRule(id, record);
    }

    function setWorkspaceRuleOverride(id, key, included, value) {
        const record = root.clone(root.workspaceRuleById(id));
        if (!record || !record.overrides)
            return;
        if (included)
            record.overrides[key] = root.clone(value);
        else
            delete record.overrides[key];
        root.replaceWorkspaceRule(id, record);
    }

    function nextWorkspaceRuleIdentity() {
        const ids = new Set(root.draftWorkspaceRules.map(item => item.id));
        let suffix = 1;
        while (ids.has("workspace-rule-" + suffix))
            ++suffix;
        return "workspace-rule-" + suffix;
    }

    function addWorkspaceRule() {
        if (!root.controlsEnabled || root.draftWorkspaceRules.length >= 1024)
            return;
        const rules = root.clone(root.draftWorkspaceRules);
        const id = root.nextWorkspaceRuleIdentity();
        rules.push({
            id,
            selector: "",
            enabled: false,
            monitor: "",
            persistent: false,
            isDefault: false,
            layout: "",
            overrides: {}
        });
        root.draftWorkspaceRules = rules;
        root.workspacesTabIndex = 1;
        root.editingWorkspaceRuleId = id;
    }

    function removeWorkspaceRule(id) {
        if (!root.controlsEnabled)
            return;
        const rules = root.clone(root.draftWorkspaceRules);
        const index = root.workspaceRuleIndex(id);
        if (!rules || index < 0)
            return;
        rules.splice(index, 1);
        root.draftWorkspaceRules = rules;
        if (root.editingWorkspaceRuleId === id)
            root.editingWorkspaceRuleId = "";
    }

    function moveWorkspaceRule(id, offset) {
        if (!root.controlsEnabled || (offset !== -1 && offset !== 1))
            return;
        const rules = root.clone(root.draftWorkspaceRules);
        const index = root.workspaceRuleIndex(id);
        const target = index + offset;
        if (!rules || index < 0 || target < 0 || target >= rules.length)
            return;
        const record = rules[index];
        rules[index] = rules[target];
        rules[target] = record;
        root.draftWorkspaceRules = rules;
    }

    function openWorkspaceRule(id) {
        if (root.workspaceRuleIndex(id) >= 0) {
            root.workspacesTabIndex = 1;
            root.editingWorkspaceRuleId = id;
        }
    }

    function closeWorkspaceRuleEditor() {
        root.editingWorkspaceRuleId = "";
    }

    function currentWorkspaceRuleInvalidOverrideKeys() {
        const record = root.editingWorkspaceRule;
        return record ? root.workspaceRuleInvalidOverrideKeys(record) : [];
    }

    function currentWorkspaceRuleIssue() {
        const record = root.editingWorkspaceRule;
        if (!record)
            return "";
        if (!root.isWorkspaceRuleSelector(record.selector, false))
            return qsTr("Choose one positive numeric, named, or special workspace.");
        if (root.draftWorkspaceRules.some(item => item.id !== record.id
                && item.selector === record.selector)) {
            return qsTr("Each user Workspace Rule needs a unique selector.");
        }
        if (!root.isStaticMonitorSelector(record.monitor, false))
            return qsTr("Finish the exact output name or description, or leave the rule unassigned.");
        const invalid = root.workspaceRuleInvalidOverrideKeys(record);
        if (invalid.length > 0)
            return qsTr("Finish every included override before saving.");
        return "";
    }

    function valuesEqual(left, right) {
        if (!left || !right || typeof left !== "object"
                || typeof right !== "object"
                || Array.isArray(left) || Array.isArray(right)) {
            return false;
        }
        for (const id of root.expectedOptionIds) {
            if (left[id] !== right[id])
                return false;
        }
        return Object.keys(left).length === root.expectedOptionIds.length
            && Object.keys(right).length === root.expectedOptionIds.length;
    }

    function optionById(id) {
        if (!Array.isArray(root.workspacesOptions))
            return null;
        for (const option of root.workspacesOptions) {
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
        if (!Array.isArray(root.workspacesOptions)
                || root.workspacesOptions.length
                    !== root.expectedOptionIds.length) {
            return false;
        }
        const seen = Object.create(null);
        for (const option of root.workspacesOptions) {
            if (!option || typeof option !== "object"
                    || typeof option.id !== "string" || seen[option.id]) {
                return false;
            }
            seen[option.id] = true;
        }
        return root.validateBooleanOption(
                root.optionById(root.workspaceWraparoundId),
                root.workspaceWraparoundId, false)
            && root.validateNumberOption(
                root.optionById(root.swipeCancelRatioId),
                root.swipeCancelRatioId, 0.5, 0, 1)
            && root.validateBooleanOption(
                root.optionById(root.swipeCreateNewId),
                root.swipeCreateNewId, true)
            && root.validateBooleanOption(
                root.optionById(root.swipeDirectionLockId),
                root.swipeDirectionLockId, true)
            && root.validateIntegerOption(
                root.optionById(root.swipeDirectionLockThresholdId),
                root.swipeDirectionLockThresholdId, 10, 0, 200)
            && root.validateIntegerOption(
                root.optionById(root.swipeDistanceId),
                root.swipeDistanceId, 300, 0, 2000)
            && root.validateBooleanOption(
                root.optionById(root.swipeForeverId),
                root.swipeForeverId, false)
            && root.validateBooleanOption(
                root.optionById(root.swipeInvertId),
                root.swipeInvertId, true)
            && root.validateIntegerOption(
                root.optionById(root.swipeMinimumSpeedId),
                root.swipeMinimumSpeedId, 30, 0, 200)
            && root.validateBooleanOption(
                root.optionById(root.swipeTouchId),
                root.swipeTouchId, false)
            && root.validateBooleanOption(
                root.optionById(root.swipeTouchInvertId),
                root.swipeTouchInvertId, false)
            && root.validateBooleanOption(
                root.optionById(root.swipeUseRelativeId),
                root.swipeUseRelativeId, false)
            && root.validateBooleanOption(
                root.optionById(root.closeSpecialOnEmptyId),
                root.closeSpecialOnEmptyId, true)
            && root.validateEnumOption(
                root.optionById(root.initialWorkspaceTrackingId),
                root.initialWorkspaceTrackingId,
                1, [0, 1, 2], 0, 2)
            && root.validateIntegerOption(
                root.optionById(root.initialWorkspaceTokenTimeoutId),
                root.initialWorkspaceTokenTimeoutId, 10, 1, 3600)
            && root.validateBooleanOption(
                root.optionById(root.allowWorkspaceCyclesId),
                root.allowWorkspaceCyclesId, false)
            && root.validateBooleanOption(
                root.optionById(root.hideSpecialOnWorkspaceChangeId),
                root.hideSpecialOnWorkspaceChangeId, false)
            && root.validateBooleanOption(
                root.optionById(root.workspaceBackAndForthId),
                root.workspaceBackAndForthId, false)
            && root.validateEnumOption(
                root.optionById(root.workspaceCenterOnId),
                root.workspaceCenterOnId,
                1, [0, 1], 0, 1)
            && root.validateEnumOption(
                root.optionById(root.warpOnChangeWorkspaceId),
                root.warpOnChangeWorkspaceId,
                0, [0, 1, 2], 0, 2)
            && root.validateEnumOption(
                root.optionById(root.warpOnToggleSpecialId),
                root.warpOnToggleSpecialId,
                0, [0, 1, 2], 0, 2);
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
            } else if (option.type === "enum") {
                if (!root.choiceValues(option).includes(value))
                    return false;
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

    function trackingLabels() {
        return [
            qsTr("Disabled"), qsTr("First window"),
            qsTr("Window and children")
        ];
    }

    function workspaceCenterLabels() {
        return [qsTr("Workspace center"), qsTr("Last active window")];
    }

    function pointerWarpLabels() {
        return [qsTr("Disabled"), qsTr("Enabled"), qsTr("Force")];
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

    function setCancelRatio(value) {
        if (typeof value !== "number" || !Number.isFinite(value))
            return;
        let result = Number((Math.round(value / 0.05) * 0.05).toFixed(2));
        if (Object.is(result, -0))
            result = 0;
        root.setDraftValue(root.swipeCancelRatioId, result);
    }

    function synchronizeDraft() {
        if (!root.serviceAvailable || !root.workspacesProjectionAvailable
                || !root.workspaceRulesProjectionAvailable
                || !root.revisionTokenValid
                || !root.trustedValuesValid
                || !root.trustedWorkspaceRulesValid
                || root.busy || root.sharedMutationBusy) {
            return;
        }
        const next = root.clone(root.workspacesValues);
        const rules = root.clone(root.workspaceRules);
        if (!next || !rules)
            return;
        root.synchronizedValues = root.clone(next);
        root.draftValues = next;
        root.synchronizedWorkspaceRules = root.clone(rules);
        root.draftWorkspaceRules = rules;
        root.synchronizedRevisionToken = root.revisionToken;
        root.projectionInitialized = true;
        root.externalChangeWhileEditing = false;
        root.saveSubmitted = false;
        root.submittedValues = ({});
        root.submittedWorkspaceRules = [];
        root.submittedRevisionToken = "";
        root.closeWorkspaceRuleEditor();
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
            root.draftWorkspaceRules = [];
            root.closeWorkspaceRuleEditor();
        }
    }

    function submitDraft() {
        if (!root.saveEnabled)
            return;
        const candidate = root.clone(root.draftValues);
        const rules = root.clone(root.draftWorkspaceRules);
        if (!candidate || !rules || !root.validateValues(candidate)
                || !root.validateWorkspaceRuleCollection(rules, false)) {
            return;
        }
        root.saveSubmitted = true;
        root.submittedValues = root.clone(candidate);
        root.submittedWorkspaceRules = root.clone(rules);
        root.submittedRevisionToken = root.revisionToken;
        root.saveRequested(candidate, rules);
        root.scheduleProjectionReview();
    }

    function reviewProjection() {
        if (!root.serviceAvailable || !root.trustedValuesValid
                || !root.trustedWorkspaceRulesValid) {
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
            if (root.valuesEqual(
                    root.workspacesValues, root.submittedValues)
                    && root.valueEqual(
                        root.workspaceRules,
                        root.submittedWorkspaceRules
                    )) {
                root.synchronizeDraft();
                return;
            }
            if (root.revisionToken === root.submittedRevisionToken) {
                root.saveSubmitted = false;
                root.submittedValues = ({});
                root.submittedWorkspaceRules = [];
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
                root.workspacesValues, root.synchronizedValues
            ) || !root.valueEqual(
                root.workspaceRules, root.synchronizedWorkspaceRules
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

    onWorkspacesOptionsChanged: root.scheduleProjectionReview()
    onWorkspacesValuesChanged: root.scheduleProjectionReview()
    onWorkspacesProjectionAvailableChanged: root.scheduleProjectionReview()
    onWorkspaceRulesChanged: root.scheduleProjectionReview()
    onWorkspaceRulesProjectionAvailableChanged:
        root.scheduleProjectionReview()
    onRevisionTokenChanged: root.scheduleProjectionReview()
    onServiceAvailableChanged: root.scheduleProjectionReview()
    onBusyChanged: {
        root.scheduleProjectionReview();
        if (workspacesRecoveryDialog.opened && root.busy)
            workspacesRecoveryDialog.close();
    }
    onSharedMutationBusyChanged: root.scheduleProjectionReview()
    onWorkspacesErrorNameChanged: root.scheduleProjectionReview()
    onWorkspacesErrorMessageChanged: root.scheduleProjectionReview()
    onSharedErrorNameChanged: root.scheduleProjectionReview()
    onSharedErrorMessageChanged: root.scheduleProjectionReview()
    onApplyStateChanged: root.scheduleProjectionReview()
    onRecoveryAvailableChanged: {
        if (workspacesRecoveryDialog.opened && !root.recoveryAvailable)
            workspacesRecoveryDialog.close();
    }
    onStatusIsDangerChanged: {
        if (!root.statusIsDanger)
            return;
        Qt.callLater(function() {
            if (root.statusIsDanger)
                workspacesOptionsScrollView.contentItem.contentY = 0;
        });
    }
    Component.onCompleted: root.scheduleProjectionReview()

    background: Rectangle { color: root.palette.window }

    ScrollView {
        id: workspacesOptionsScrollView

        objectName: "workspacesOptionsScrollView"
        anchors.fill: parent
        anchors.topMargin: root.compactPage
            ? Math.min(root.contentTopMargin, 12)
            : root.contentTopMargin
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            objectName: "workspacesOptionsContent"
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
                        text: qsTr("Workspaces")
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
                        text: qsTr("Tune managed workspace behavior, application placement, and gesture response.")
                        color: root.palette.placeholderText
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }

                Button {
                    objectName: "refreshWorkspacesButton"
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Refresh")
                    enabled: !root.busy && !root.displayTestActive
                    icon.name: "view-refresh-symbolic"
                    Accessible.name:
                        qsTr("Refresh compositor workspace settings")

                    onClicked: root.refreshRequested()
                }
            }

            Frame {
                objectName: "workspacesStatusCard"
                Layout.fillWidth: true
                visible: root.statusVisible
                padding: 16

                background: Rectangle {
                    color: root.statusIsDanger ? "#382125" : "#33251a"
                    radius: 12
                    border.color: root.statusIsDanger
                        ? "#8bfb7185" : "#8bf6ad55"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    Label {
                        objectName: "workspacesStatusMessage"
                        Layout.fillWidth: true
                        text: root.statusMessage
                        color: root.statusIsDanger
                            ? "#ffb8c3" : "#ffd5a1"
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
                            objectName: "workspacesOpenDisplaysButton"
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
                            objectName: "loadCurrentWorkspacesButton"
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
                                && root.workspacesProjectionAvailable
                                && root.trustedValuesValid
                                && root.workspaceRulesProjectionAvailable
                                && root.trustedWorkspaceRulesValid
                            Accessible.name: qsTr("Discard this Workspaces draft and load the current compositor settings")

                            onClicked: root.synchronizeDraft()
                        }

                        Button {
                            objectName: "retryApplyWorkspacesButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight
                                    + topPadding + bottomPadding
                            )
                            visible: root.retryApplyAvailable
                            text: root.busyOperation === "compositor-apply"
                                || root.busyOperation === "workspaces-apply"
                                ? qsTr("Retrying apply…")
                                : qsTr("Retry apply")
                            enabled: root.retryApplyAvailable && !root.busy
                                && !root.sharedMutationBusy
                                && root.sharedApplySafe
                            Accessible.name: qsTr("Retry applying the exact saved compositor revision")

                            onClicked: root.retryApplyRequested()
                        }

                        Button {
                            objectName: "recoverWorkspacesButton"
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
                            Accessible.name:
                                qsTr("Review whole-compositor recovery")

                            onClicked: workspacesRecoveryDialog.open()
                        }
                    }
                }
            }

            TabBar {
                id: workspacesTabBar

                objectName: "workspacesTabBar"
                Layout.fillWidth: true
                currentIndex: root.workspacesTabIndex

                onCurrentIndexChanged: {
                    root.workspacesTabIndex = currentIndex;
                    if (currentIndex === 0)
                        root.closeWorkspaceRuleEditor();
                }

                TabButton {
                    objectName: "workspacesBehaviorTab"
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Behavior")
                    Accessible.name: qsTr("Workspace behavior settings")
                }

                TabButton {
                    objectName: "workspaceRulesTab"
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Workspace Rules")
                    Accessible.name: qsTr("User Workspace Rules")
                }
            }

            WorkspaceRuleSummaryList {
                id: workspaceRuleList

                objectName: "workspaceRuleList"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(
                    300, root.height - (root.compactPage ? 150 : 190)
                )
                visible: root.workspacesTabIndex === 1
                    && !root.workspaceRuleEditorActive
                rules: root.draftWorkspaceRules
                controlsEnabled: root.controlsEnabled
                discardEnabled: !root.busy && !root.saveSubmitted
                    && !root.sharedMutationBusy
                    && root.trustedValuesValid
                    && root.trustedWorkspaceRulesValid
                draftDirty: root.draftDirty
                draftValid: root.draftValid
                saveEnabled: root.saveEnabled
                resetEnabled: root.resetTargetDiffers
                busy: root.busy
                busyOperation: root.busyOperation
                minimumTargetSize: root.minimumTargetSize

                onAddRequested: root.addWorkspaceRule()
                onEditRequested: id => root.openWorkspaceRule(id)
                onEnabledRequested: (id, enabled) =>
                    root.setWorkspaceRuleProperty(id, "enabled", enabled)
                onMoveRequested: (id, offset) =>
                    root.moveWorkspaceRule(id, offset)
                onRemoveRequested: id => root.removeWorkspaceRule(id)
                onDiscardRequested: root.synchronizeDraft()
                onResetRequested: root.resetDraftToDefaults()
                onSaveRequested: root.submitDraft()
            }

            WorkspaceRuleEditor {
                id: workspaceRuleEditor

                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(
                    300, root.height - (root.compactPage ? 150 : 190)
                )
                visible: root.workspacesTabIndex === 1
                    && root.workspaceRuleEditorActive
                rule: root.editingWorkspaceRule
                overrideDefinitions: root.workspaceRuleOverrideDefinitions
                invalidOverrideKeys:
                    root.editingWorkspaceRuleInvalidOverrideKeys
                controlsEnabled: root.controlsEnabled
                ruleIssue: root.workspaceRuleEditorIssue
                minimumTargetSize: root.minimumTargetSize

                onCloseRequested: root.closeWorkspaceRuleEditor()
                onRemoveRequested: id => root.removeWorkspaceRule(id)
                onPropertyModified: (id, propertyName, value) =>
                    root.setWorkspaceRuleProperty(id, propertyName, value)
                onOverrideModified: (id, key, included, value) =>
                    root.setWorkspaceRuleOverride(
                        id, key, included, value
                    )
            }

            Frame {
                objectName: "workspacesBehaviorCard"
                Layout.fillWidth: true
                visible: root.workspacesTabIndex === 0
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
                        text: qsTr("Workspace behavior")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Wrap slide direction at the ends")
                        description: qsTr("Reverse the slide direction when moving between the first and last workspace. This applies to slide-style workspace animations.")
                        checked: root.draftValue(
                            root.workspaceWraparoundId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "workspacesWraparound"
                        accessibleName:
                            qsTr("Wrap workspace slide direction")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.workspaceWraparoundId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Close empty special workspaces")
                        description: qsTr("Hide a special workspace when its final window is removed.")
                        checked: root.draftValue(
                            root.closeSpecialOnEmptyId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "workspacesCloseSpecialOnEmpty"
                        accessibleName:
                            qsTr("Close special workspaces when empty")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.closeSpecialOnEmptyId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "workspacesSwitchingHistoryCard"
                Layout.fillWidth: true
                visible: root.workspacesTabIndex === 0
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
                        text: qsTr("Switching history")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Keep workspace cycle history")
                        description: qsTr("Keep each previous-workspace link after a workspace cycle so repeated cycling can continue through recently visited workspaces.")
                        checked: root.draftValue(
                            root.allowWorkspaceCyclesId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "workspacesAllowWorkspaceCycles"
                        accessibleName:
                            qsTr("Keep workspace cycle history")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.allowWorkspaceCyclesId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Hide special workspace after a change")
                        description: qsTr("Hide the monitor's active special workspace when switching workspaces or moving the active workspace to another output.")
                        checked: root.draftValue(
                            root.hideSpecialOnWorkspaceChangeId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "workspacesHideSpecialOnWorkspaceChange"
                        accessibleName:
                            qsTr("Hide special workspace after a workspace change")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.hideSpecialOnWorkspaceChangeId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Switch back from the current workspace")
                        description: qsTr("When a workspace command targets the workspace that is already focused, return to the previously focused workspace.")
                        checked: root.draftValue(
                            root.workspaceBackAndForthId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "workspacesBackAndForth"
                        accessibleName:
                            qsTr("Switch back from the current workspace")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.workspaceBackAndForthId, value
                        )
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("Cross-output pointer target")
                        description: qsTr("When a workspace switch moves focus to another output, target the workspace center or its last active window.")
                        model: root.workspaceCenterLabels()
                        currentIndex: root.choiceIndex(
                            root.workspaceCenterOnId
                        )
                        controlWidth: 190
                        enabled: root.controlsEnabled
                        controlObjectName: "workspacesCenterOn"
                        accessibleName:
                            qsTr("Pointer target for cross-output workspace switches")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => root.setChoiceFromIndex(
                            root.workspaceCenterOnId, index
                        )
                    }
                }
            }

            Frame {
                objectName: "workspacesPointerPlacementCard"
                Layout.fillWidth: true
                visible: root.workspacesTabIndex === 0
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
                            text: qsTr("Pointer placement")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            objectName: "workspacesPointerPlacementCopy"
                            Layout.fillWidth: true
                            text: qsTr("Enabled moves the pointer unless cursor:no_warps blocks it; Force bypasses cursor:no_warps. The separate cursor:persistent_warps value decides whether Hyprland restores the remembered position inside the window or uses its center. These choices do not change either separate cursor setting.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("After changing workspace")
                        description: qsTr("Choose whether to move the pointer to the target workspace's last-focused window after a workspace change.")
                        model: root.pointerWarpLabels()
                        currentIndex: root.choiceIndex(
                            root.warpOnChangeWorkspaceId
                        )
                        controlWidth: 160
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "workspacesWarpOnChangeWorkspace"
                        accessibleName:
                            qsTr("Pointer placement after changing workspace")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => root.setChoiceFromIndex(
                            root.warpOnChangeWorkspaceId, index
                        )
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("When toggling a special workspace")
                        description: qsTr("Choose whether to move the pointer to the target workspace's last-focused window when showing or hiding a special workspace.")
                        model: root.pointerWarpLabels()
                        currentIndex: root.choiceIndex(
                            root.warpOnToggleSpecialId
                        )
                        controlWidth: 160
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "workspacesWarpOnToggleSpecial"
                        accessibleName:
                            qsTr("Pointer placement when toggling a special workspace")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => root.setChoiceFromIndex(
                            root.warpOnToggleSpecialId, index
                        )
                    }
                }
            }

            Frame {
                objectName: "workspacesLaunchCard"
                Layout.fillWidth: true
                visible: root.workspacesTabIndex === 0
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
                        text: qsTr("Application placement")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("Track launch workspace")
                        description: qsTr("Open managed launches on the workspace where they were started, either for the first window or for its child processes as well.")
                        model: root.trackingLabels()
                        currentIndex: root.choiceIndex(
                            root.initialWorkspaceTrackingId
                        )
                        controlWidth: 180
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "workspacesInitialTracking"
                        accessibleName:
                            qsTr("Initial workspace tracking mode")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => root.setChoiceFromIndex(
                            root.initialWorkspaceTrackingId, index
                        )
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("First-window timeout")
                        description: qsTr("Expire a single-use launch token after this many seconds. Persistent child tracking does not use this timeout.")
                        from: root.optionMinimum(
                            root.initialWorkspaceTokenTimeoutId
                        )
                        to: root.optionMaximum(
                            root.initialWorkspaceTokenTimeoutId
                        )
                        value: Number(root.draftValue(
                            root.initialWorkspaceTokenTimeoutId
                        )) || 0
                        enabled: root.initialTokenTimeoutEnabled
                        controlObjectName:
                            "workspacesInitialTokenTimeout"
                        accessibleName:
                            qsTr("Initial workspace token timeout in seconds")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.initialWorkspaceTokenTimeoutId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "workspacesSwipeCard"
                Layout.fillWidth: true
                visible: root.workspacesTabIndex === 0
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
                            text: qsTr("Workspace swipe")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            objectName: "workspacesSwipeBindingCopy"
                            Layout.fillWidth: true
                            text: qsTr("These choices tune a configured workspace-swipe gesture; they do not create a touchpad gesture binding. The touchscreen edge switch below enables its own touch gesture.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.name: text
                        }
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Swipe distance")
                        description: qsTr("Set the travel distance in pixels for a complete workspace swipe.")
                        from: root.optionMinimum(root.swipeDistanceId)
                        to: root.optionMaximum(root.swipeDistanceId)
                        value: Number(root.draftValue(
                            root.swipeDistanceId
                        )) || 0
                        enabled: root.controlsEnabled
                        controlObjectName: "workspacesSwipeDistance"
                        accessibleName:
                            qsTr("Workspace swipe distance in pixels")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.swipeDistanceId, value
                        )
                    }

                    SettingsSliderRow {
                        Layout.fillWidth: true
                        title: qsTr("Completion threshold")
                        description: qsTr("Complete the workspace change after the swipe reaches this fraction of its distance.")
                        from: root.optionMinimum(root.swipeCancelRatioId)
                        to: root.optionMaximum(root.swipeCancelRatioId)
                        value: Number(root.draftValue(
                            root.swipeCancelRatioId
                        )) || 0
                        stepSize: 0.05
                        decimals: 2
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "workspacesSwipeCancelRatio"
                        valueObjectName:
                            "workspacesSwipeCancelRatioValue"
                        accessibleName:
                            qsTr("Workspace swipe completion threshold")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setCancelRatio(value)
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Force-change speed")
                        description: qsTr("Complete a faster swipe above this speed even when it has not reached the distance threshold. Set 0 to disable speed forcing.")
                        from: root.optionMinimum(root.swipeMinimumSpeedId)
                        to: root.optionMaximum(root.swipeMinimumSpeedId)
                        value: Number(root.draftValue(
                            root.swipeMinimumSpeedId
                        )) || 0
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "workspacesSwipeMinimumSpeed"
                        accessibleName:
                            qsTr("Minimum workspace swipe speed to force a change")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.swipeMinimumSpeedId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Create a workspace at the end")
                        description: qsTr("Allow a forward swipe beyond the last existing workspace to create the next workspace.")
                        checked: root.draftValue(
                            root.swipeCreateNewId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "workspacesSwipeCreateNew"
                        accessibleName:
                            qsTr("Create a new workspace from an end swipe")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.swipeCreateNewId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Lock swipe direction")
                        description: qsTr("Keep a swipe moving in its initial direction after it passes the lock threshold.")
                        checked: root.draftValue(
                            root.swipeDirectionLockId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "workspacesSwipeDirectionLock"
                        accessibleName:
                            qsTr("Lock workspace swipe direction")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.swipeDirectionLockId, value
                        )
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Direction-lock distance")
                        description: qsTr("Lock the initial direction after this many pixels of travel.")
                        from: root.optionMinimum(
                            root.swipeDirectionLockThresholdId
                        )
                        to: root.optionMaximum(
                            root.swipeDirectionLockThresholdId
                        )
                        value: Number(root.draftValue(
                            root.swipeDirectionLockThresholdId
                        )) || 0
                        enabled: root.directionLockThresholdEnabled
                        controlObjectName:
                            "workspacesSwipeDirectionLockThreshold"
                        accessibleName:
                            qsTr("Workspace swipe direction-lock distance in pixels")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.swipeDirectionLockThresholdId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Continue across workspaces")
                        description: qsTr("Let one continuous swipe travel beyond the neighboring workspace.")
                        checked: root.draftValue(
                            root.swipeForeverId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "workspacesSwipeForever"
                        accessibleName:
                            qsTr("Continue a workspace swipe across multiple workspaces")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.swipeForeverId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Invert touchpad direction")
                        description: qsTr("Reverse the workspace direction for touchpad swipe gestures.")
                        checked: root.draftValue(
                            root.swipeInvertId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "workspacesSwipeInvert"
                        accessibleName:
                            qsTr("Invert touchpad workspace swipe direction")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.swipeInvertId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Use adjacent workspace numbers")
                        description: qsTr("Move through adjacent valid workspace identifiers instead of cycling only the existing workspaces on this monitor.")
                        checked: root.draftValue(
                            root.swipeUseRelativeId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName:
                            "workspacesSwipeUseRelative"
                        accessibleName:
                            qsTr("Use adjacent workspace identifiers for swipes")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.swipeUseRelativeId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Touchscreen edge swipe")
                        description: qsTr("Enable workspace switching from a swipe that begins at the edge of a touchscreen.")
                        checked: root.draftValue(root.swipeTouchId) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "workspacesSwipeTouch"
                        accessibleName:
                            qsTr("Enable touchscreen workspace edge swipes")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.swipeTouchId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Invert touchscreen direction")
                        description: qsTr("Reverse the workspace direction for touchscreen edge swipes.")
                        checked: root.draftValue(
                            root.swipeTouchInvertId
                        ) === true
                        enabled: root.touchInvertEnabled
                        controlObjectName:
                            "workspacesSwipeTouchInvert"
                        accessibleName:
                            qsTr("Invert touchscreen workspace swipe direction")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.swipeTouchInvertId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "workspacesDraftActions"
                Layout.fillWidth: true
                visible: root.workspacesTabIndex === 0
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
                                    ? qsTr("Unsaved Workspaces draft")
                                    : qsTr("No workspace changes")
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
                            objectName: "discardWorkspacesDraftButton"
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
                                && root.trustedWorkspaceRulesValid
                            Accessible.name:
                                qsTr("Discard Workspaces draft")

                            onClicked: root.synchronizeDraft()
                        }

                        Button {
                            objectName: "resetWorkspacesDefaultsButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight
                                    + topPadding + bottomPadding
                            )
                            text: qsTr("Reset to defaults")
                            enabled: {
                                return root.controlsEnabled
                                    && root.resetTargetDiffers;
                            }
                            Accessible.name: qsTr("Reset Workspaces draft to trusted catalog defaults")

                            onClicked: root.resetDraftToDefaults()
                        }

                        Button {
                            objectName: "saveWorkspacesButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight
                                    + topPadding + bottomPadding
                            )
                            text: {
                                if (root.busyOperation === "workspaces-save")
                                    return qsTr("Saving…");
                                if (root.busyOperation === "compositor-apply"
                                        || root.busyOperation
                                            === "workspaces-apply") {
                                    return qsTr("Applying…");
                                }
                                return qsTr("Save & apply");
                            }
                            highlighted: true
                            enabled: root.saveEnabled
                            Accessible.name: qsTr("Save and apply the validated Workspaces draft")

                            onClicked: root.submitDraft()
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 12 }
        }
    }

    CompositorRecoveryDialog {
        id: workspacesRecoveryDialog

        objectName: "workspacesRecoveryDialog"
        recoveryAvailable: root.recoveryAvailable
        operationBusy: root.busy || root.sharedMutationBusy
        busyOperation: root.busyOperation
        settingsAreaName: qsTr("Workspaces")
        warningObjectName: "workspacesRecoveryWarning"
        cancelObjectName: "cancelWorkspacesRecoveryButton"
        confirmObjectName: "confirmWorkspacesRecoveryButton"
        minimumTargetSize: root.minimumTargetSize

        onRecoveryRequested: root.recoveryRequested()
    }
}
