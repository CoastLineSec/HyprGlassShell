pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    property bool serviceAvailable: false
    property bool writable: false
    property bool catalogAvailable: false
    property bool advancedAvailable: false
    property bool advancedProjectionAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property var advancedOptions: []
    property var advancedValues: ({})
    property string revisionToken: "0"
    property double appliedRevision: 0
    property string loadState: "unavailable"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property string advancedErrorName: ""
    property string advancedErrorMessage: ""
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

    readonly property string allowSessionLockRestoreId:
        "hyprland.misc.allow_session_lock_restore"
    readonly property string lockdeadScreenDelayId:
        "hyprland.misc.lockdead_screen_delay"
    readonly property string disableScaleNotificationId:
        "hyprland.misc.disable_scale_notification"
    readonly property string renderUnfocusedFpsId:
        "hyprland.misc.render_unfocused_fps"
    readonly property string screencopyForce8BitId:
        "hyprland.misc.screencopy_force_8b"
    readonly property string disableHyprlandLogoId:
        "hyprland.misc.disable_hyprland_logo"
    readonly property string disableSplashRenderingId:
        "hyprland.misc.disable_splash_rendering"
    readonly property string sessionLockXrayId:
        "hyprland.misc.session_lock_xray"
    readonly property string sessionLockBlurId:
        "hyprland.misc.session_lock_blur"
    readonly property string xwaylandUseNearestNeighborId:
        "hyprland.xwayland.use_nearest_neighbor"
    readonly property string expandUndersizedTexturesId:
        "hyprland.render.expand_undersized_textures"
    readonly property string directScanoutId:
        "hyprland.render.direct_scanout"
    readonly property string fp16SdrTransferId:
        "hyprland.render.fp16_sdr_tf"
    readonly property string xpModeId:
        "hyprland.render.xp_mode"
    readonly property string captureModifiersId:
        "hyprland.input-capture.capture_modifiers"
    readonly property string enforceCaptureBarriersId:
        "hyprland.input-capture.enforce_barriers"
    readonly property real minimumTargetSize: 44
    readonly property bool compactPage: root.width < 560
    readonly property var expectedOptionIds: [
        root.allowSessionLockRestoreId,
        root.lockdeadScreenDelayId,
        root.disableScaleNotificationId,
        root.renderUnfocusedFpsId,
        root.screencopyForce8BitId,
        root.disableHyprlandLogoId,
        root.disableSplashRenderingId,
        root.sessionLockXrayId,
        root.sessionLockBlurId,
        root.xwaylandUseNearestNeighborId,
        root.expandUndersizedTexturesId,
        root.directScanoutId,
        root.fp16SdrTransferId,
        root.xpModeId,
        root.captureModifiersId,
        root.enforceCaptureBarriersId
    ]
    readonly property bool trustedDefinitionsValid: root.validateOptions()
    readonly property bool trustedValuesValid:
        root.advancedProjectionAvailable
        && root.trustedDefinitionsValid
        && root.validateValues(root.advancedValues)
    readonly property bool revisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.revisionToken)
    readonly property bool synchronizedRevisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.synchronizedRevisionToken)
    readonly property bool draftValid:
        root.trustedDefinitionsValid && root.validateValues(root.draftValues)
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
        && root.advancedAvailable
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
    readonly property bool sessionLockBlurEnabled:
        root.controlsEnabled
        && root.draftValue(root.sessionLockXrayId) === true
    readonly property bool resetTargetDiffers: {
        const target = root.resetTargetValues();
        return target !== null
            && !root.valuesEqual(root.draftValues, target);
    }
    readonly property bool statusVisible:
        !root.serviceAvailable
        || !root.writable
        || !root.catalogAvailable
        || !root.advancedAvailable
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
        || root.advancedErrorMessage.length > 0
        || root.sharedErrorMessage.length > 0
        || root.busy
        || root.sharedMutationBusy
        || !root.sharedApplySafe
    readonly property bool statusIsDanger:
        root.managementState === "conflict"
        || root.applyState === "failed"
        || root.loadState === "unsupported"
        || (root.catalogAvailable && !root.trustedDefinitionsValid)
        || (root.advancedProjectionAvailable
            && !root.advancedAvailable
            && root.advancedErrorMessage.length > 0)
        || root.externalChangeWhileEditing
    readonly property string statusMessage: {
        const advancedDetail = root.advancedErrorMessage.length > 0
            ? " " + root.advancedErrorMessage : "";
        const sharedDetail = root.sharedErrorMessage.length > 0
            ? " " + root.sharedErrorMessage : "";
        if (!root.serviceAvailable) {
            return qsTr("Advanced settings are unavailable. The compositor settings service may be restarting.%1").arg(sharedDetail);
        }
        if (root.displayTestActive) {
            return qsTr("A display test is active. Advanced changes stay locked until that test is kept or reverted.");
        }
        if (root.managementState === "unmanaged") {
            return qsTr("HyprShelld is not managing the Hyprland entrypoint yet. Review takeover from Displays before changing advanced settings.");
        }
        if (root.managementState === "conflict") {
            return qsTr("The managed compositor entrypoint or its ownership state changed unexpectedly. Advanced changes are locked to preserve it.%1").arg(sharedDetail);
        }
        if (!root.writable) {
            return qsTr("This compositor configuration is read-only and has been preserved.");
        }
        if (!root.catalogAvailable) {
            return qsTr("The trusted Hyprland option catalog is unavailable or does not match the compositor authority. Advanced changes are disabled.%1").arg(advancedDetail);
        }
        if (!root.trustedDefinitionsValid || !root.trustedValuesValid) {
            return qsTr("The trusted Advanced contract does not match this Settings build. No compositor values will be written.%1").arg(advancedDetail);
        }
        if (!root.revisionTokenValid) {
            return qsTr("The exact compositor revision token is unavailable. Advanced changes are disabled to prevent overwriting another revision.");
        }
        if (root.externalChangeWhileEditing) {
            return qsTr("Compositor settings changed outside this draft. Your Advanced draft is preserved, but it cannot be saved over the newer revision. Load the current settings to continue.");
        }
        if (root.busy) {
            if (root.busyOperation === "advanced-save")
                return qsTr("Saving the validated Advanced draft…");
            if (root.busyOperation === "compositor-apply"
                    || root.busyOperation === "advanced-apply") {
                return qsTr("Applying and verifying the saved compositor revision…");
            }
            if (root.busyOperation === "recover") {
                return qsTr("Restoring and verifying the last working compositor configuration…");
            }
            return qsTr("Another compositor operation is in progress. Advanced changes are temporarily locked.");
        }
        if (root.sharedMutationBusy) {
            return qsTr("A shared compositor setting is changing. Advanced changes remain locked until that transition is verified.");
        }
        if (root.applyState === "retained"
                || root.applyState === "failed"
                || root.applyState === "inactive") {
            if (root.requiredActivation === "reload") {
                return root.retryApplyAvailable
                    ? qsTr("The desired compositor settings were saved, but they are not active. Retry the exact saved revision or restore the last working compositor configuration.%1").arg(sharedDetail)
                    : qsTr("The desired compositor settings are saved but not active. Wait for the compositor service to make retry or recovery available.%1").arg(sharedDetail);
            }
            if (root.requiredActivation === "restart") {
                return qsTr("The saved desired state requires a verified compositor-restart workflow that HyprShelld does not have yet, so this revision cannot be activated from Settings. Restore the last working compositor configuration to continue.%1").arg(sharedDetail);
            }
            if (root.requiredActivation === "session") {
                return qsTr("The saved desired state requires a verified new-session workflow that HyprShelld does not have yet, so this revision cannot be activated from Settings. Restore the last working compositor configuration to continue.%1").arg(sharedDetail);
            }
            return qsTr("The desired compositor state is not the active state. Review recovery options before making another change.%1").arg(sharedDetail);
        }
        if (root.loadState === "recovered") {
            return qsTr("Compositor settings were restored from the last known good desired-state copy. Review the Advanced values before changing them.");
        }
        if (root.loadState === "defaulted") {
            return qsTr("Compositor settings could not be recovered, so safe desired-state defaults are in use. Review the Advanced values before continuing.");
        }
        if (root.loadState === "unsupported") {
            return qsTr("The compositor settings use a newer format. They were preserved and remain read-only.");
        }
        if (root.advancedProjectionAvailable
                && !root.advancedAvailable
                && root.advancedErrorMessage.length > 0) {
            return qsTr("Advanced authority verification failed. Current Advanced values remain readable, but changes are disabled until the managed action, schema, and full-state contract is authenticated.%1").arg(advancedDetail);
        }
        if (root.advancedErrorMessage.length > 0)
            return qsTr("The Advanced operation failed.%1").arg(advancedDetail);
        if (root.sharedErrorMessage.length > 0)
            return qsTr("The compositor operation failed.%1").arg(sharedDetail);
        if (!root.advancedAvailable) {
            return qsTr("Advanced settings are waiting for a current, verified compositor baseline.%1").arg(advancedDetail);
        }
        if (!root.sharedApplySafe) {
            return qsTr("A shared compositor setting is not at a verified activation point. Advanced controls remain locked until the exact compositor source transition is verified.");
        }
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
        if (Object.keys(left).length !== root.expectedOptionIds.length
                || Object.keys(right).length
                    !== root.expectedOptionIds.length) {
            return false;
        }
        for (const id of root.expectedOptionIds) {
            if (!Object.prototype.hasOwnProperty.call(left, id)
                    || !Object.prototype.hasOwnProperty.call(right, id)
                    || left[id] !== right[id]) {
                return false;
            }
        }
        return true;
    }

    function optionById(id) {
        if (!Array.isArray(root.advancedOptions))
            return null;
        for (const option of root.advancedOptions) {
            if (option && typeof option === "object" && option.id === id)
                return option;
        }
        return null;
    }

    function optionHasNoChoices(option) {
        return option.choices === undefined
            || (Array.isArray(option.choices) && option.choices.length === 0);
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

    function validateBooleanOption(option, id, defaultValue, risk) {
        return option && option.id === id
            && option.type === "boolean"
            && option.control === "toggle"
            && option.defaultValue === defaultValue
            && option.risk === risk
            && option.min === undefined
            && option.max === undefined
            && option.step === undefined
            && root.optionHasNoChoices(option);
    }

    function validateIntegerOption(
        option, id, defaultValue, minimum, maximum, risk
    ) {
        return option && option.id === id
            && option.type === "integer"
            && option.control === "spinBox"
            && option.defaultValue === defaultValue
            && option.risk === risk
            && option.min === minimum
            && option.max === maximum
            && option.step === undefined
            && root.optionHasNoChoices(option);
    }

    function validateEnumOption(
        option, id, defaultValue, expectedChoices, minimum, maximum, risk
    ) {
        return option && option.id === id
            && option.type === "enum"
            && option.control === "select"
            && option.defaultValue === defaultValue
            && option.risk === risk
            && option.min === minimum
            && option.max === maximum
            && option.step === undefined
            && JSON.stringify(root.choiceValues(option))
                === JSON.stringify(expectedChoices);
    }

    function validateOptions() {
        if (!Array.isArray(root.advancedOptions)
                || root.advancedOptions.length
                    !== root.expectedOptionIds.length) {
            return false;
        }
        const seen = Object.create(null);
        for (let index = 0; index < root.advancedOptions.length; ++index) {
            const option = root.advancedOptions[index];
            if (!option || typeof option !== "object"
                    || typeof option.id !== "string"
                    || option.id !== root.expectedOptionIds[index]
                    || seen[option.id]) {
                return false;
            }
            seen[option.id] = true;
        }
        return root.validateBooleanOption(
                root.optionById(root.allowSessionLockRestoreId),
                root.allowSessionLockRestoreId, false, "safe")
            && root.validateIntegerOption(
                root.optionById(root.lockdeadScreenDelayId),
                root.lockdeadScreenDelayId, 1000, 0, 5000, "safe")
            && root.validateBooleanOption(
                root.optionById(root.disableScaleNotificationId),
                root.disableScaleNotificationId, false, "safe")
            && root.validateIntegerOption(
                root.optionById(root.renderUnfocusedFpsId),
                root.renderUnfocusedFpsId, 15, 1, 120, "safe")
            && root.validateBooleanOption(
                root.optionById(root.screencopyForce8BitId),
                root.screencopyForce8BitId, true, "safe")
            && root.validateBooleanOption(
                root.optionById(root.disableHyprlandLogoId),
                root.disableHyprlandLogoId, false, "safe")
            && root.validateBooleanOption(
                root.optionById(root.disableSplashRenderingId),
                root.disableSplashRenderingId, false, "safe")
            && root.validateBooleanOption(
                root.optionById(root.sessionLockXrayId),
                root.sessionLockXrayId, false, "safe")
            && root.validateBooleanOption(
                root.optionById(root.sessionLockBlurId),
                root.sessionLockBlurId, false, "safe")
            && root.validateBooleanOption(
                root.optionById(root.xwaylandUseNearestNeighborId),
                root.xwaylandUseNearestNeighborId, true, "caution")
            && root.validateBooleanOption(
                root.optionById(root.expandUndersizedTexturesId),
                root.expandUndersizedTexturesId, true, "caution")
            && root.validateEnumOption(
                root.optionById(root.directScanoutId),
                root.directScanoutId, 0, [0, 1, 2], 0, 2, "caution")
            && root.validateEnumOption(
                root.optionById(root.fp16SdrTransferId),
                root.fp16SdrTransferId, 0, [0, 1], 0, 1, "caution")
            && root.validateBooleanOption(
                root.optionById(root.xpModeId),
                root.xpModeId, false, "caution")
            && root.validateBooleanOption(
                root.optionById(root.captureModifiersId),
                root.captureModifiersId, false, "caution")
            && root.validateBooleanOption(
                root.optionById(root.enforceCaptureBarriersId),
                root.enforceCaptureBarriersId, true, "caution");
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
            } else if (option.type === "integer"
                    || option.type === "enum") {
                if (typeof value !== "number" || !Number.isFinite(value)
                        || !Number.isInteger(value)
                        || value < option.min || value > option.max) {
                    return false;
                }
                if (option.type === "enum"
                        && !root.choiceValues(option).includes(value)) {
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

    function directScanoutLabels() {
        return [
            qsTr("Disabled"),
            qsTr("Enabled"),
            qsTr("Automatic (games only)")
        ];
    }

    function fp16SdrTransferLabels() {
        return [
            qsTr("Display transfer (default)"),
            qsTr("Linear")
        ];
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

    function synchronizeDraft() {
        if (!root.serviceAvailable || !root.advancedProjectionAvailable
                || !root.revisionTokenValid || !root.trustedValuesValid
                || root.busy || root.sharedMutationBusy) {
            return;
        }
        const next = root.clone(root.advancedValues);
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
        for (const id of root.expectedOptionIds)
            defaults[id] = root.optionDefault(id);
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
            if (root.valuesEqual(root.advancedValues, root.submittedValues)) {
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
            || !root.valuesEqual(
                root.advancedValues, root.synchronizedValues
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

    onAdvancedOptionsChanged: root.scheduleProjectionReview()
    onAdvancedValuesChanged: root.scheduleProjectionReview()
    onAdvancedProjectionAvailableChanged: root.scheduleProjectionReview()
    onRevisionTokenChanged: root.scheduleProjectionReview()
    onServiceAvailableChanged: root.scheduleProjectionReview()
    onBusyChanged: root.scheduleProjectionReview()
    onSharedMutationBusyChanged: root.scheduleProjectionReview()
    onAdvancedErrorNameChanged: root.scheduleProjectionReview()
    onAdvancedErrorMessageChanged: root.scheduleProjectionReview()
    onSharedErrorNameChanged: root.scheduleProjectionReview()
    onSharedErrorMessageChanged: root.scheduleProjectionReview()
    onApplyStateChanged: root.scheduleProjectionReview()
    onStatusIsDangerChanged: {
        if (!root.statusIsDanger)
            return;
        Qt.callLater(function() {
            if (root.statusIsDanger)
                advancedOptionsScrollView.contentItem.contentY = 0;
        });
    }
    Component.onCompleted: root.scheduleProjectionReview()

    background: Rectangle { color: root.palette.window }

    ScrollView {
        id: advancedOptionsScrollView

        objectName: "advancedOptionsScrollView"
        anchors.fill: parent
        anchors.topMargin: root.compactPage
            ? Math.min(root.contentTopMargin, 12)
            : root.contentTopMargin
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: advancedOptionsContent

            objectName: "advancedOptionsContent"
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
                        text: qsTr("Advanced")
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
                        text: qsTr("Configure narrowly scoped compositor recovery, lock, fallback, work-buffer and direct-scanout rendering, native Wayland and XWayland compatibility, input-capture protocol, and warning behavior.")
                        color: root.palette.placeholderText
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }

                Button {
                    objectName: "refreshAdvancedButton"
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Refresh")
                    enabled: !root.busy && !root.displayTestActive
                    icon.name: "view-refresh-symbolic"
                    Accessible.name: qsTr("Refresh advanced compositor settings")

                    onClicked: root.refreshRequested()
                }
            }

            Frame {
                objectName: "advancedStatusCard"
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
                        objectName: "advancedStatusMessage"
                        Layout.fillWidth: true
                        text: root.statusMessage
                        color: root.statusIsDanger ? "#ffb8c3" : "#ffd5a1"
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
                            objectName: "advancedOpenDisplaysButton"
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
                            objectName: "loadCurrentAdvancedButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight + topPadding + bottomPadding
                            )
                            visible: root.externalChangeWhileEditing
                            text: qsTr("Load current settings")
                            enabled: !root.busy && !root.sharedMutationBusy
                                && !root.saveSubmitted
                                && root.advancedProjectionAvailable
                                && root.trustedValuesValid
                            Accessible.name: qsTr("Discard this Advanced draft and load the current compositor settings")

                            onClicked: root.synchronizeDraft()
                        }

                        Button {
                            objectName: "retryApplyAdvancedButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight + topPadding + bottomPadding
                            )
                            visible: root.retryApplyAvailable
                            text: root.busyOperation === "compositor-apply"
                                || root.busyOperation === "advanced-apply"
                                ? qsTr("Retrying apply…")
                                : qsTr("Retry apply")
                            enabled: root.retryApplyAvailable && !root.busy
                                && !root.sharedMutationBusy
                                && root.sharedApplySafe
                            Accessible.name: qsTr("Retry applying the exact saved compositor revision")

                            onClicked: root.retryApplyRequested()
                        }

                        Button {
                            objectName: "recoverAdvancedButton"
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

                            onClicked: advancedRecoveryDialog.open()
                        }
                    }
                }
            }

            Frame {
                objectName: "advancedSessionLockCard"
                Layout.fillWidth: true
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
                        text: qsTr("Session lock recovery")
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
                        text: qsTr("Choose how Hyprland handles a failed or incomplete lock screen.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.name: text
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Allow another session-lock client")
                        description: qsTr("When Hyprland already considers the session locked, allow a new client after the current lock is locked, denied, or missing. This can recover after a crash; Hyprland does not launch or restart a client.")
                        checked: root.draftValue(
                            root.allowSessionLockRestoreId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedAllowSessionLockRestore"
                        accessibleName: qsTr("Allow another session-lock client while Hyprland is already locked")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.allowSessionLockRestoreId, value
                        )
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Incomplete lock screen grace period (ms)")
                        description: qsTr("After a lock request, wait this many milliseconds before Hyprland stops rendering ordinary workspaces if the lock client has not completed.")
                        from: root.optionMinimum(root.lockdeadScreenDelayId)
                        to: root.optionMaximum(root.lockdeadScreenDelayId)
                        value: root.numericDraftValue(
                            root.lockdeadScreenDelayId
                        )
                        editable: true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedLockdeadScreenDelay"
                        accessibleName: qsTr("Incomplete lock screen grace period in milliseconds")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.lockdeadScreenDelayId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "advancedSessionLockRenderingCard"
                Layout.fillWidth: true
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
                        text: qsTr("Session lock rendering")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        objectName: "advancedSessionLockRenderingCopy"
                        Layout.fillWidth: true
                        text: qsTr("These choices change only the composition beneath lock surfaces. The session remains locked, the lock client still controls its surfaces, and input routing does not change.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.name: text
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("X-ray lock composition")
                        description: qsTr("Keep ordinary workspaces rendering beneath the lock screen and remove Hyprland's opaque black primer. This does not unlock the session or change input.")
                        checked: root.draftValue(
                            root.sessionLockXrayId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedSessionLockXray"
                        accessibleName: qsTr("Keep ordinary workspaces rendering beneath session lock surfaces")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.sessionLockXrayId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Blur behind lock surfaces")
                        description: qsTr("With X-ray on, blur is visible only through non-opaque lock-surface pixels. Hyprland 0.55.0 has no session-lock blur setting; it was introduced in 0.56.0 and is present in 0.56.1. The saved choice is retained while X-ray is off.")
                        checked: root.draftValue(
                            root.sessionLockBlurId
                        ) === true
                        enabled: root.sessionLockBlurEnabled
                        controlObjectName: "advancedSessionLockBlur"
                        accessibleName: qsTr("Blur the workspace composition behind non-opaque session lock surfaces")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.sessionLockBlurId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "advancedBackgroundRenderingCard"
                Layout.fillWidth: true
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
                        text: qsTr("Background rendering and capture")
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
                        objectName: "advancedBackgroundRenderingCopy"
                        text: qsTr("Control Hyprland's compositor-owned fallback image, Rule-qualified frame callbacks, and the preferred screen-share format for new sessions.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.name: text
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Hide compositor fallback image")
                        description: qsTr("Suppress Hyprland's whole compositor-owned random fallback background image so the compositor background color is used instead. Splash text remains separately controlled. This does not hide, replace, or manage a user-configured wallpaper.")
                        checked: root.draftValue(
                            root.disableHyprlandLogoId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedDisableHyprlandLogo"
                        accessibleName: qsTr("Hide Hyprland's compositor-owned fallback background image")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.disableHyprlandLogoId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Hide Hyprland splash text")
                        description: qsTr("Suppress only Hyprland's splash text. The splash can still appear over the compositor background color when the fallback image is hidden; this neither suppresses that image nor hides or changes a user-configured wallpaper.")
                        checked: root.draftValue(
                            root.disableSplashRenderingId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedDisableSplashRendering"
                        accessibleName: qsTr("Hide Hyprland's splash text independently of the fallback image")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.disableSplashRenderingId, value
                        )
                    }

                    SettingsSpinBoxRow {
                        Layout.fillWidth: true
                        title: qsTr("Render-unfocused frame callbacks (FPS)")
                        description: qsTr("Set how often Hyprland sends frame callbacks to Rule-marked windows that otherwise are not being rendered.")
                        from: root.optionMinimum(root.renderUnfocusedFpsId)
                        to: root.optionMaximum(root.renderUnfocusedFpsId)
                        value: root.numericDraftValue(
                            root.renderUnfocusedFpsId
                        )
                        editable: true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedRenderUnfocusedFps"
                        accessibleName: qsTr("Render-unfocused frame callback rate")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.renderUnfocusedFpsId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Prefer 8-bit screen capture")
                        description: qsTr("For a new screen-share session, replace supported 10-bit monitor formats with XRGB8888. Other monitor formats are unchanged.")
                        checked: root.draftValue(
                            root.screencopyForce8BitId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedScreencopyForce8Bit"
                        accessibleName: qsTr("Prefer 8-bit format for new screen-share sessions")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.screencopyForce8BitId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "advancedWorkspaceUnderlayRenderingCard"
                Layout.fillWidth: true
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
                        objectName: "advancedWorkspaceUnderlayRenderingHeading"
                        Layout.fillWidth: true
                        text: qsTr("Workspace underlay rendering")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        objectName: "advancedWorkspaceUnderlayRenderingCopy"
                        Layout.fillWidth: true
                        text: qsTr("Control whether Hyprland constructs its normal background and lower-layer passes while rendering an active workspace.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.name: text
                    }

                    Frame {
                        objectName: "advancedWorkspaceUnderlayRenderingCaution"
                        Layout.fillWidth: true
                        padding: 14

                        background: Rectangle {
                            color: "#33251a"
                            radius: 10
                            border.color: "#8bf6ad55"
                        }

                        Label {
                            objectName: "advancedWorkspaceUnderlayRenderingCautionMessage"
                            anchors.fill: parent
                            text: qsTr("Caution: When this is on, the active-workspace render path skips construction of Hyprland's compositor fallback background, layer-shell background and bottom passes, and the post-wallpaper pass. Windows and layer-shell top and overlay surfaces still render. The no-workspace render path is unchanged. This does not stop those surfaces or their processes, and it does not clear prior buffer pixels, so uncovered pixels may be stale or undefined.")
                            color: "#ffd5a1"
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: text
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Skip workspace underlays")
                        description: qsTr("Off by default. Use Hyprland's active-workspace path without the underlay passes described above.")
                        checked: root.draftValue(root.xpModeId) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedSkipWorkspaceUnderlays"
                        accessibleName: qsTr("Skip active-workspace background and bottom-layer rendering")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.xpModeId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "advancedSdrWorkBufferTransferCard"
                Layout.fillWidth: true
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
                        objectName: "advancedSdrWorkBufferTransferHeading"
                        Layout.fillWidth: true
                        text: qsTr("SDR work-buffer transfer")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        objectName: "advancedSdrWorkBufferTransferCopy"
                        Layout.fillWidth: true
                        text: qsTr("Choose how Hyprland represents SDR content only while it passes through the internal FP16 or ICC work buffer.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.name: text
                    }

                    Frame {
                        objectName: "advancedSdrWorkBufferTransferCaution"
                        Layout.fillWidth: true
                        padding: 14

                        background: Rectangle {
                            color: "#33251a"
                            radius: 10
                            border.color: "#8bf6ad55"
                        }

                        Label {
                            objectName: "advancedSdrWorkBufferTransferCautionMessage"
                            anchors.fill: parent
                            text: qsTr("Caution: This changes only the internal FP16 or ICC SDR work-buffer transfer. It does not enable FP16, HDR, or ICC, and it does not change the output transfer function or color profile. On ordinary sRGB output without an ICC profile, the choice may remain dormant. HDR-like paths stay linear regardless of this setting.")
                            color: "#ffd5a1"
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: text
                        }
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("SDR work-buffer transfer")
                        description: qsTr("Display transfer (the default) applies the display transfer to SDR content in the internal work buffer. Linear keeps that SDR work-buffer content linear.")
                        model: root.fp16SdrTransferLabels()
                        currentIndex: root.choiceIndex(
                            root.fp16SdrTransferId
                        )
                        controlWidth: 190
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedFp16SdrTransfer"
                        accessibleName: qsTr("SDR work-buffer transfer function")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => root.setChoiceFromIndex(
                            root.fp16SdrTransferId, index
                        )
                    }
                }
            }

            Frame {
                objectName: "advancedDirectScanoutRenderingCard"
                Layout.fillWidth: true
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
                        objectName: "advancedDirectScanoutRenderingHeading"
                        Layout.fillWidth: true
                        text: qsTr("Direct scanout rendering")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        objectName: "advancedDirectScanoutRenderingCopy"
                        Layout.fillWidth: true
                        text: qsTr("Allow Hyprland to present an eligible fullscreen client buffer directly to an output instead of composing an ordinary frame.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.name: text
                    }

                    Frame {
                        objectName: "advancedDirectScanoutCaution"
                        Layout.fillWidth: true
                        padding: 14

                        background: Rectangle {
                            color: "#33251a"
                            radius: 10
                            border.color: "#8bf6ad55"
                        }

                        Label {
                            objectName: "advancedDirectScanoutCautionMessage"
                            anchors.fill: parent
                            text: qsTr("Caution: Eligible direct scanout bypasses normal composition, but activation is never guaranteed. Overlays such as the lock screen or notifications, screen capture, mirrored outputs, a software cursor, and incompatible buffers, transforms, or color management can keep normal composition active. Settings does not test display or GPU support.")
                            color: "#ffd5a1"
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: text
                        }
                    }

                    SettingsSelectRow {
                        Layout.fillWidth: true
                        title: qsTr("Direct scanout")
                        description: qsTr("Disabled (the default) always uses normal composition. Enabled permits an otherwise-eligible exact-fullscreen, solitary DMA surface. Automatic adds a requirement that the client advertises game content.")
                        model: root.directScanoutLabels()
                        currentIndex: root.choiceIndex(root.directScanoutId)
                        controlWidth: 190
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedDirectScanout"
                        accessibleName: qsTr("Direct scanout rendering mode")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: index => root.setChoiceFromIndex(
                            root.directScanoutId, index
                        )
                    }
                }
            }

            Frame {
                objectName: "advancedNativeWaylandResizeCompatibilityCard"
                Layout.fillWidth: true
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
                        objectName: "advancedNativeWaylandResizeCompatibilityHeading"
                        Layout.fillWidth: true
                        text: qsTr("Native Wayland resize compatibility")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        objectName: "advancedNativeWaylandResizeCompatibilityCopy"
                        Layout.fillWidth: true
                        text: qsTr("When a native Wayland client's submitted buffer is temporarily undersized for the current surface mapping, Hyprland can extend texture sampling across the mapped area while the client catches up.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.name: text
                    }

                    Frame {
                        objectName: "advancedNativeWaylandResizeCaution"
                        Layout.fillWidth: true
                        padding: 14

                        background: Rectangle {
                            color: "#33251a"
                            radius: 10
                            border.color: "#8bf6ad55"
                        }

                        Label {
                            objectName: "advancedNativeWaylandResizeCautionMessage"
                            anchors.fill: parent
                            text: qsTr("Caution: This affects native Wayland surfaces only. It does not resize the client buffer, window, or display, and it does not make the application respond sooner. X11 windows handled through XWayland are unaffected.")
                            color: "#ffd5a1"
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: text
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Extend undersized surface textures")
                        description: qsTr("Extend edge texture coordinates when a submitted native Wayland buffer is temporarily undersized for its current surface mapping. Disabling this may expose unfilled or stale-size edges until the client submits a matching buffer. Some scale-unaware and misaligned-fullscreen correction paths bypass this behavior.")
                        checked: root.draftValue(
                            root.expandUndersizedTexturesId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedExpandUndersizedTextures"
                        accessibleName: qsTr("Extend temporarily undersized native Wayland surface textures")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.expandUndersizedTexturesId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "advancedXWaylandCompatibilityCard"
                Layout.fillWidth: true
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
                        text: qsTr("XWayland compatibility")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        objectName: "advancedXWaylandCompatibilityCopy"
                        Layout.fillWidth: true
                        text: qsTr("Control how Hyprland samples X11 application surfaces handled through XWayland.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.name: text
                    }

                    Frame {
                        objectName: "advancedXWaylandCaution"
                        Layout.fillWidth: true
                        padding: 14

                        background: Rectangle {
                            color: "#33251a"
                            radius: 10
                            border.color: "#8bf6ad55"
                        }

                        Label {
                            objectName: "advancedXWaylandCautionMessage"
                            anchors.fill: parent
                            text: qsTr("Caution: This global choice affects only X11 windows handled through XWayland. Native Wayland windows are unchanged, and a per-window Nearest-neighbor scaling Rule can still enable the filter when this choice is off.")
                            color: "#ffd5a1"
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: text
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Use nearest-neighbor filtering")
                        description: qsTr("Keep the default nearest-neighbor filter whenever Hyprland scales or transforms an X11 surface. This can keep pixel art crisp but can look pixelated; turning it off uses smoother filtering that can look blurry. A per-window Nearest-neighbor scaling Rule can still enable it. This changes surface filtering only; it does not change display scale or XWayland coordinate scaling.")
                        checked: root.draftValue(
                            root.xwaylandUseNearestNeighborId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedXWaylandUseNearestNeighbor"
                        accessibleName: qsTr("Use nearest-neighbor filtering for scaled or transformed X11 surfaces")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.xwaylandUseNearestNeighborId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "advancedInputCaptureProtocolCard"
                Layout.fillWidth: true
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
                        objectName: "advancedInputCaptureProtocolHeading"
                        Layout.fillWidth: true
                        text: qsTr("Input capture protocol")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        objectName: "advancedInputCaptureProtocolCopy"
                        Layout.fillWidth: true
                        text: qsTr("Control how Hyprland handles later keyboard-modifier events and new barrier requests from input-capture clients.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.name: text
                    }

                    Frame {
                        objectName: "advancedInputCaptureProtocolCaution"
                        Layout.fillWidth: true
                        padding: 14

                        background: Rectangle {
                            color: "#33251a"
                            radius: 10
                            border.color: "#8bf6ad55"
                        }

                        Label {
                            objectName: "advancedInputCaptureProtocolCautionMessage"
                            anchors.fill: parent
                            text: qsTr("Caution: These choices do not grant input-capture permission, create or enable a capture session, or release an active one. They affect only later modifier events and new barrier requests. Changing them does not synthesize or retract held modifier state, and it does not revalidate, repair, or remove existing barriers.")
                            color: "#ffd5a1"
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: text
                        }
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Send modifiers to input capture")
                        description: qsTr("Off by default. When an authorized capture is active, send later keyboard modifier-state changes to it. Those forwarded changes stop before ordinary seat or input-method delivery while the capture remains active. Turning this off sends no modifier state to the capture client and leaves ordinary modifier routing in place.")
                        checked: root.draftValue(
                            root.captureModifiersId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedCaptureModifiers"
                        accessibleName: qsTr("Send later keyboard modifier changes to an active input-capture client")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.captureModifiersId, value
                        )
                    }

                    SettingsToggleRow {
                        Layout.fillWidth: true
                        title: qsTr("Reject invalid capture barriers")
                        description: qsTr("On by default. For each new request, reject a barrier that is not a valid full edge of exactly one output with a protocol error. Turning this off logs the invalid request and adds the barrier. Existing barriers are not rechecked or removed.")
                        checked: root.draftValue(
                            root.enforceCaptureBarriersId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedEnforceCaptureBarriers"
                        accessibleName: qsTr("Reject invalid new input-capture barriers")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.enforceCaptureBarriersId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "advancedDisplayWarningsCard"
                Layout.fillWidth: true
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
                        text: qsTr("Display warnings")
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
                        title: qsTr("Hide clean-divisor scale warning")
                        description: qsTr("Hide the warning shown for a display scale that is not a clean divisor. This does not change display scale, topology, or the Displays test preview.")
                        checked: root.draftValue(
                            root.disableScaleNotificationId
                        ) === true
                        enabled: root.controlsEnabled
                        controlObjectName: "advancedDisableScaleNotification"
                        accessibleName: qsTr("Hide the clean-divisor display scale warning")
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => root.setDraftValue(
                            root.disableScaleNotificationId, value
                        )
                    }
                }
            }

            Frame {
                objectName: "advancedDraftActions"
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
                                    ? qsTr("Unsaved Advanced draft")
                                    : qsTr("No Advanced changes")
                            color: root.palette.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.externalChangeWhileEditing
                                ? qsTr("Load the current settings before creating a new draft. HyprShelld never silently rebases this draft onto another compositor revision.")
                                : qsTr("Save & apply persists these sixteen values as one validated Advanced revision, then reloads and verifies that exact revision.")
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
                            objectName: "discardAdvancedDraftButton"
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
                            Accessible.name: qsTr("Discard the complete Advanced draft")

                            onClicked: root.synchronizeDraft()
                        }

                        Button {
                            objectName: "resetAdvancedDefaultsButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight + topPadding + bottomPadding
                            )
                            text: qsTr("Reset to defaults")
                            enabled: root.controlsEnabled
                                && root.resetTargetDiffers
                            Accessible.name: qsTr("Reset all Advanced values to trusted defaults")

                            onClicked: root.resetDraftToDefaults()
                        }

                        Button {
                            objectName: "saveAdvancedButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight + topPadding + bottomPadding
                            )
                            text: {
                                if (root.busyOperation === "advanced-save")
                                    return qsTr("Saving…");
                                if (root.busyOperation === "compositor-apply"
                                        || root.busyOperation
                                            === "advanced-apply") {
                                    return qsTr("Applying…");
                                }
                                return qsTr("Save & apply");
                            }
                            highlighted: true
                            enabled: root.saveEnabled
                            Accessible.name: qsTr("Save and apply the validated Advanced draft")

                            onClicked: root.submitDraft()
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 12 }
        }
    }

    CompositorRecoveryDialog {
        id: advancedRecoveryDialog

        objectName: "advancedRecoveryDialog"
        recoveryAvailable: root.recoveryAvailable
        operationBusy: root.busy || root.sharedMutationBusy
        busyOperation: root.busyOperation
        settingsAreaName: qsTr("Advanced")
        warningObjectName: "advancedRecoveryWarning"
        cancelObjectName: "cancelAdvancedRecoveryButton"
        confirmObjectName: "confirmAdvancedRecoveryButton"
        minimumTargetSize: root.minimumTargetSize

        onRecoveryRequested: root.recoveryRequested()
    }
}
