pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    property bool serviceAvailable: false
    property bool writable: false
    property bool catalogAvailable: false
    property bool appearanceAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property var appearanceOptions: []
    property var appearanceValues: ({})
    property string revisionToken: "0"
    property double appliedRevision: 0
    property string loadState: "unavailable"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property string catalogErrorName: ""
    property string catalogErrorMessage: ""
    property string errorName: ""
    property string errorMessage: ""
    property bool retryApplyAvailable: false
    property bool recoveryAvailable: false
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

    readonly property string borderSizeId: "hyprland.general.border_size"
    readonly property string roundingId: "hyprland.decoration.rounding"
    readonly property string blurId: "hyprland.decoration.blur.enabled"
    readonly property string shadowId: "hyprland.decoration.shadow.enabled"
    readonly property string animationsId: "hyprland.animations.enabled"
    readonly property string layoutId: "hyprland.general.layout"
    readonly property string resizeId: "hyprland.general.resize_on_border"
    readonly property string snapId: "hyprland.general.snap.enabled"
    readonly property real minimumTargetSize: 44
    readonly property bool compactPreview:
        root.width < 560 || root.height < 640
    readonly property var expectedOptionIds: [
        root.borderSizeId,
        root.roundingId,
        root.blurId,
        root.shadowId,
        root.animationsId,
        root.layoutId,
        root.resizeId,
        root.snapId
    ]
    readonly property bool trustedDefinitionsValid: root.validateOptions()
    readonly property bool trustedValuesValid:
        root.trustedDefinitionsValid
        && root.validateValues(root.appearanceValues)
    readonly property bool revisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.revisionToken)
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
        && root.appearanceAvailable
        && root.revisionTokenValid
        && root.trustedDefinitionsValid
        && root.trustedValuesValid
        && !root.busy
        && !root.externalChangeWhileEditing
        && !root.displayTestActive
    readonly property bool saveEnabled:
        root.controlsEnabled && root.draftDirty && root.draftValid
        && !root.saveSubmitted
    readonly property bool statusVisible:
        !root.serviceAvailable
        || !root.writable
        || !root.catalogAvailable
        || !root.appearanceAvailable
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
        || root.errorMessage.length > 0
        || root.busy
    readonly property bool statusIsDanger:
        root.managementState === "conflict"
        || root.applyState === "failed"
        || root.loadState === "unsupported"
        || (root.catalogAvailable && !root.trustedDefinitionsValid)
    readonly property string statusMessage: {
        const detail = root.errorMessage.length > 0
            ? " " + root.errorMessage : "";
        const catalogDetail = root.catalogErrorMessage.length > 0
            ? " " + root.catalogErrorMessage : "";
        if (!root.serviceAvailable)
            return qsTr("Appearance settings are unavailable. The compositor settings service may be restarting.%1").arg(detail);
        if (root.displayTestActive)
            return qsTr("A display test is active. Appearance changes stay locked until that test is kept or reverted.");
        if (root.managementState === "unmanaged")
            return qsTr("HyprShelld is not managing the Hyprland entrypoint yet. Review takeover from Displays before changing appearance.");
        if (root.managementState === "conflict")
            return qsTr("The managed compositor entrypoint or its ownership state changed unexpectedly. Appearance changes are locked to preserve it.%1").arg(detail);
        if (!root.writable)
            return qsTr("This compositor configuration is read-only and has been preserved.");
        if (!root.catalogAvailable)
            return qsTr("The trusted Hyprland option catalog is unavailable or does not match the compositor authority. Appearance changes are disabled.%1").arg(catalogDetail);
        if (!root.trustedDefinitionsValid || !root.trustedValuesValid)
            return qsTr("The trusted Appearance contract does not match this Settings build. No compositor values will be written.%1").arg(catalogDetail);
        if (!root.revisionTokenValid)
            return qsTr("The exact compositor revision token is unavailable. Appearance changes are disabled to prevent overwriting another revision.");
        if (root.externalChangeWhileEditing)
            return qsTr("Compositor settings changed outside this draft. Your draft is preserved, but it cannot be saved over the newer revision. Load the current settings to continue.");
        if (root.busy) {
            if (root.busyOperation === "appearance-save")
                return qsTr("Saving the validated Appearance draft…");
            if (root.busyOperation === "appearance-apply")
                return qsTr("Applying and verifying the saved compositor revision…");
            if (root.busyOperation === "recover")
                return qsTr("Restoring and verifying the last working compositor configuration…");
            return qsTr("Another compositor operation is in progress. Appearance changes are temporarily locked.");
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
                return qsTr("The saved compositor settings require a compositor restart. HyprShelld will not perform that transition automatically.%1").arg(detail);
            if (root.requiredActivation === "session")
                return qsTr("The saved compositor settings require a new desktop session. HyprShelld will not perform that transition automatically.%1").arg(detail);
            return qsTr("The desired compositor state is not the active state. Review recovery options before making another change.%1").arg(detail);
        }
        if (root.loadState === "recovered")
            return qsTr("Compositor settings were restored from the last known good desired-state copy. Review the values before changing them.");
        if (root.loadState === "defaulted")
            return qsTr("Compositor settings could not be recovered, so safe desired-state defaults are in use. Review them before continuing.");
        if (root.loadState === "unsupported")
            return qsTr("The compositor settings use a newer format. They were preserved and remain read-only.");
        if (root.errorMessage.length > 0)
            return qsTr("The compositor operation failed.%1").arg(detail);
        if (!root.appearanceAvailable)
            return qsTr("Appearance settings are waiting for a current, verified compositor baseline.");
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
            if (left[id] !== right[id])
                return false;
        }
        return Object.keys(left).length === root.expectedOptionIds.length
            && Object.keys(right).length === root.expectedOptionIds.length;
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

    function validateIntegerOption(option, id, defaultValue) {
        return option && option.id === id
            && option.type === "integer"
            && option.control === "spinBox"
            && option.defaultValue === defaultValue
            && option.min === 0
            && option.max === 20;
    }

    function validateOptions() {
        if (!Array.isArray(root.appearanceOptions)
                || root.appearanceOptions.length
                    !== root.expectedOptionIds.length) {
            return false;
        }
        const seen = Object.create(null);
        for (const option of root.appearanceOptions) {
            if (!option || typeof option !== "object"
                    || typeof option.id !== "string" || seen[option.id]) {
                return false;
            }
            seen[option.id] = true;
        }
        const layout = root.optionById(root.layoutId);
        const layoutChoices = root.choiceValues(layout);
        return root.validateIntegerOption(
                root.optionById(root.borderSizeId), root.borderSizeId, 1)
            && root.validateIntegerOption(
                root.optionById(root.roundingId), root.roundingId, 0)
            && root.validateBooleanOption(
                root.optionById(root.blurId), root.blurId, true)
            && root.validateBooleanOption(
                root.optionById(root.shadowId), root.shadowId, true)
            && root.validateBooleanOption(
                root.optionById(root.animationsId), root.animationsId, true)
            && root.validateBooleanOption(
                root.optionById(root.resizeId), root.resizeId, false)
            && root.validateBooleanOption(
                root.optionById(root.snapId), root.snapId, false)
            && layout && layout.id === root.layoutId
            && layout.type === "enum" && layout.control === "select"
            && layout.defaultValue === "dwindle"
            && JSON.stringify(layoutChoices)
                === JSON.stringify([
                    "dwindle", "master", "scrolling", "monocle"
                ]);
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

    function optionMinimum(id) {
        const option = root.optionById(id);
        return option && Number.isInteger(option.min) ? option.min : 0;
    }

    function optionMaximum(id) {
        const option = root.optionById(id);
        return option && Number.isInteger(option.max) ? option.max : 0;
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

    function layoutChoices() {
        return root.choiceValues(root.optionById(root.layoutId));
    }

    function layoutLabels() {
        const labels = [];
        for (const value of root.layoutChoices()) {
            if (value === "master")
                labels.push(qsTr("Master"));
            else if (value === "scrolling")
                labels.push(qsTr("Scrolling"));
            else if (value === "monocle")
                labels.push(qsTr("Monocle"));
            else
                labels.push(qsTr("Dwindle"));
        }
        return labels;
    }

    function layoutIndex(value) {
        const index = root.layoutChoices().indexOf(value);
        return index >= 0 ? index : 0;
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
        if (!root.trustedValuesValid)
            return;
        const next = root.clone(root.appearanceValues);
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

    function resetDraftToDefaults() {
        if (!root.controlsEnabled || !root.trustedDefinitionsValid)
            return;
        const defaults = {};
        for (const id of root.expectedOptionIds)
            defaults[id] = root.optionDefault(id);
        if (root.validateValues(defaults))
            root.draftValues = defaults;
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
        // Main forwards this signal synchronously. If the client rejects the
        // request at the authorization boundary without entering busy state,
        // release the submission guard while preserving the draft.
        root.scheduleProjectionReview();
    }

    function reviewProjection() {
        if (!root.trustedValuesValid)
            return;
        if (!root.projectionInitialized) {
            root.synchronizeDraft();
            return;
        }
        if (root.saveSubmitted) {
            if (root.busy)
                return;
            if (root.valuesEqual(
                    root.appearanceValues, root.submittedValues)) {
                root.synchronizeDraft();
                return;
            }
            if (root.revisionToken === root.submittedRevisionToken) {
                // Replace did not commit. Preserve the draft for retry.
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
                root.appearanceValues, root.synchronizedValues
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
    onRevisionTokenChanged: root.scheduleProjectionReview()
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
                        blurEnabled: root.draftValue(root.blurId) === true
                        shadowEnabled:
                            root.draftValue(root.shadowId) === true
                        animationsEnabled:
                            root.draftValue(root.animationsId) === true
                        layoutMode: String(
                            root.draftValue(root.layoutId) || "dwindle"
                        )
                        resizeOnBorder:
                            root.draftValue(root.resizeId) === true
                        snapEnabled:
                            root.draftValue(root.snapId) === true
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
                            text: qsTr("Appearance & Behavior")
                            color: root.palette.text
                            font.pixelSize: 28
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Shape common window visuals and interactions through the managed compositor configuration.")
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
                        color: root.statusIsDanger ? "#382125" : "#33251a"
                        radius: 12
                        border.color: root.statusIsDanger
                            ? "#8bfb7185" : "#8bf6ad55"
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        Label {
                            objectName: "appearanceStatusMessage"
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
                                enabled: !root.busy && root.trustedValuesValid
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
                                text: root.busyOperation === "appearance-apply"
                                    ? qsTr("Retrying apply…")
                                    : qsTr("Retry apply")
                                enabled: root.retryApplyAvailable && !root.busy
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

                Frame {
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
                            text: qsTr("Window style")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: qsTr("Border thickness")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Set the managed border width around windows in layout pixels.")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }

                            SpinBox {
                                objectName: "appearanceBorderSize"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                from: root.optionMinimum(root.borderSizeId)
                                to: root.optionMaximum(root.borderSizeId)
                                value: Number(root.draftValue(root.borderSizeId)) || 0
                                editable: false
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Window border thickness")

                                onValueModified: root.setDraftValue(
                                    root.borderSizeId, value
                                )
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: qsTr("Corner radius")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Round window corners by this many layout pixels.")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }

                            SpinBox {
                                objectName: "appearanceRounding"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                from: root.optionMinimum(root.roundingId)
                                to: root.optionMaximum(root.roundingId)
                                value: Number(root.draftValue(root.roundingId)) || 0
                                editable: false
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Window corner radius")

                                onValueModified: root.setDraftValue(
                                    root.roundingId, value
                                )
                            }
                        }
                    }
                }

                Frame {
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
                            text: qsTr("Visual effects")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    text: qsTr("Blur backgrounds")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Allow Hyprland to blur content behind translucent windows.")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }

                            Switch {
                                objectName: "appearanceBlurEnabled"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                checked: root.draftValue(root.blurId) === true
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Blur window backgrounds")
                                onClicked: root.setDraftValue(root.blurId, checked)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    text: qsTr("Window shadows")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Draw managed drop shadows behind windows.")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }

                            Switch {
                                objectName: "appearanceShadowEnabled"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                checked: root.draftValue(root.shadowId) === true
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Window shadows")
                                onClicked: root.setDraftValue(root.shadowId, checked)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    text: qsTr("Animations")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Enable Hyprland's configured window and workspace animations.")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }

                            Switch {
                                objectName: "appearanceAnimationsEnabled"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                checked:
                                    root.draftValue(root.animationsId) === true
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Hyprland animations")
                                onClicked: root.setDraftValue(
                                    root.animationsId, checked
                                )
                            }
                        }
                    }
                }

                Frame {
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
                            text: qsTr("Window behavior")
                            color: root.palette.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    text: qsTr("Default layout")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Choose the layout used when a workspace has no more specific rule.")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }

                            ComboBox {
                                objectName: "appearanceLayout"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                Layout.preferredWidth: 140
                                model: root.layoutLabels()
                                currentIndex: root.layoutIndex(
                                    root.draftValue(root.layoutId)
                                )
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Default window layout")

                                onActivated: index => {
                                    const choices = root.layoutChoices();
                                    if (index >= 0 && index < choices.length)
                                        root.setDraftValue(
                                            root.layoutId, choices[index]
                                        );
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    text: qsTr("Resize from borders and gaps")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Let pointer drags on window borders and surrounding gaps resize windows.")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }

                            Switch {
                                objectName: "appearanceResizeOnBorder"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                checked: root.draftValue(root.resizeId) === true
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Resize windows from borders and gaps")
                                onClicked: root.setDraftValue(
                                    root.resizeId, checked
                                )
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    text: qsTr("Snap floating windows")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Enable Hyprland's managed snapping for floating windows.")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }

                            Switch {
                                objectName: "appearanceSnapEnabled"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                checked: root.draftValue(root.snapId) === true
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Snap floating windows")
                                onClicked: root.setDraftValue(root.snapId, checked)
                            }
                        }
                    }
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
                                        ? qsTr("Unsaved Appearance draft")
                                        : qsTr("No Appearance changes")
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
                                enabled: !root.busy && root.trustedValuesValid
                                Accessible.name: qsTr("Discard Appearance draft")

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
                                    && root.trustedDefinitionsValid
                                    && !root.valuesEqual(
                                        root.draftValues,
                                        root.expectedOptionIds.reduce(
                                            function(result, id) {
                                                result[id] = root.optionDefault(id);
                                                return result;
                                            }, {}
                                        )
                                    )
                                Accessible.name: qsTr("Reset Appearance draft to trusted catalog defaults")

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
                                    if (root.busyOperation === "appearance-apply")
                                        return qsTr("Applying…");
                                    return qsTr("Save & apply");
                                }
                                highlighted: true
                                enabled: root.saveEnabled
                                Accessible.name: qsTr("Save and apply the validated Appearance draft")

                                onClicked: root.submitDraft()
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 12 }
            }
        }
    }

    Dialog {
        id: appearanceRecoveryDialog

        property bool requestSubmitted: false

        objectName: "appearanceRecoveryDialog"
        title: qsTr("Restore the last working compositor configuration?")
        modal: true
        width: Math.min(
            620,
            Math.max(280, parent ? parent.width - 48 : 620)
        )
        height: Math.min(
            500,
            Math.max(300, parent ? parent.height - 48 : 500)
        )
        closePolicy: Popup.CloseOnEscape

        onOpened: {
            requestSubmitted = false;
            Qt.callLater(function() {
                if (appearanceRecoveryDialog.opened)
                    cancelAppearanceRecoveryButton.forceActiveFocus();
            });
        }

        onVisibleChanged: {
            if (opened && (!root.recoveryAvailable || root.busy))
                close();
        }

        contentItem: ScrollView {
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 16

                Label {
                    Layout.fillWidth: true
                    text: qsTr("This recovery affects the whole compositor")
                    color: root.palette.text
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Frame {
                    Layout.fillWidth: true
                    padding: 14

                    background: Rectangle {
                        color: "#382125"
                        radius: 10
                        border.color: "#8bfb7185"
                    }

                    Label {
                        objectName: "appearanceRecoveryWarning"
                        anchors.fill: parent
                        text: qsTr("Recovery is not limited to Appearance. It replaces every pending compositor setting, including display and future settings, with the last verified working snapshot.")
                        color: "#ffb8c3"
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("HyprShelld creates a new monotonic desired-state revision from the last working snapshot, reloads Hyprland, and verifies that revision. Canceling leaves desired files and the running compositor unchanged.")
                    color: root.palette.placeholderText
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    Accessible.name: text
                }
            }
        }

        footer: DialogButtonBox {
            Button {
                id: cancelAppearanceRecoveryButton

                objectName: "cancelAppearanceRecoveryButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Cancel")
                enabled: !root.busy
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                Accessible.name: qsTr("Cancel without changing compositor settings")

                onClicked: appearanceRecoveryDialog.reject()
            }

            Button {
                objectName: "confirmAppearanceRecoveryButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: root.busyOperation === "recover"
                    ? qsTr("Restoring…")
                    : qsTr("Restore whole configuration")
                enabled: appearanceRecoveryDialog.opened
                    && root.recoveryAvailable && !root.busy
                    && !appearanceRecoveryDialog.requestSubmitted
                highlighted: true
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                Accessible.name: qsTr("Restore the last working whole-compositor configuration")

                onClicked: {
                    if (!appearanceRecoveryDialog.opened
                            || !root.recoveryAvailable || root.busy
                            || appearanceRecoveryDialog.requestSubmitted) {
                        return;
                    }
                    appearanceRecoveryDialog.requestSubmitted = true;
                    root.recoveryRequested();
                    appearanceRecoveryDialog.close();
                }
            }
        }
    }
}
