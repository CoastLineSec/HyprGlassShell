pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    property bool serviceAvailable: false
    property bool writable: false
    property bool catalogAvailable: false
    property bool environmentAvailable: false
    property bool environmentProjectionAvailable: false
    property bool uwsmIntegrationAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property var environmentVariables: []
    property string revisionToken: "0"
    property string loadState: "unavailable"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property string environmentErrorName: ""
    property string environmentErrorMessage: ""
    property string sharedErrorName: ""
    property string sharedErrorMessage: ""
    property bool retryApplyAvailable: false
    property bool recoveryAvailable: false
    property bool sharedMutationBusy: false
    property bool sharedApplySafe: false
    property real contentTopMargin: 28

    property var draftEnvironmentVariables: []
    property var synchronizedEnvironmentVariables: []
    property var submittedEnvironmentVariables: []
    property string synchronizedRevisionToken: ""
    property string submittedRevisionToken: ""
    property bool projectionInitialized: false
    property bool externalChangeWhileEditing: false
    property bool saveSubmitted: false
    property string editingVariableId: ""

    signal refreshRequested
    signal backRequested
    signal openDisplaysRequested
    signal saveRequested(var environmentVariables)
    signal retryApplyRequested
    signal recoveryRequested

    readonly property real minimumTargetSize: 44
    readonly property bool compactPage: root.width < 620
    readonly property int maximumVariables: 512
    readonly property int maximumIdLength: 128
    readonly property int maximumNameLength: 128
    readonly property int maximumValueLength: 4096
    readonly property var scopeValues: ["hyprland", "uwsm"]
    readonly property var scopeLabels: [qsTr("Hyprland startup"), qsTr("UWSM session environment")]
    readonly property bool revisionTokenValid: /^(0|[1-9][0-9]*)$/.test(root.revisionToken)
    readonly property bool synchronizedRevisionTokenValid: /^(0|[1-9][0-9]*)$/.test(root.synchronizedRevisionToken)
    readonly property bool trustedValuesValid: root.environmentProjectionAvailable && root.validateEnvironmentCollection(root.environmentVariables)
    readonly property bool draftValid: root.validateEnvironmentCollection(root.draftEnvironmentVariables)
    readonly property bool draftHasUnavailableUwsm: {
        if (root.uwsmIntegrationAvailable || !Array.isArray(root.draftEnvironmentVariables)) {
            return false;
        }
        return root.draftEnvironmentVariables.some(record => record && record.scope === "uwsm");
    }
    readonly property bool draftDirty: root.projectionInitialized && !root.valueEqual(root.draftEnvironmentVariables, root.synchronizedEnvironmentVariables)
    readonly property bool displayTestActive: root.confirmationState !== "idle" || root.managementState === "preview"
    readonly property bool controlsEnabled: root.serviceAvailable && root.writable && root.catalogAvailable && root.environmentAvailable && root.environmentProjectionAvailable && root.revisionTokenValid && root.trustedValuesValid && root.managementState === "managed" && root.loadState !== "unsupported" && !root.busy && !root.saveSubmitted && !root.sharedMutationBusy && root.sharedApplySafe && !root.externalChangeWhileEditing && !root.displayTestActive
    readonly property bool saveEnabled: root.controlsEnabled && root.draftDirty && root.draftValid && !root.draftHasUnavailableUwsm
    readonly property bool discardEnabled: root.serviceAvailable && root.environmentProjectionAvailable && root.trustedValuesValid && root.projectionInitialized && !root.busy && !root.saveSubmitted && !root.sharedMutationBusy && !root.externalChangeWhileEditing
    readonly property bool resetEnabled: root.controlsEnabled && root.draftEnvironmentVariables.length > 0
    readonly property var editingVariable: root.variableById(root.editingVariableId)
    readonly property bool editorActive: root.editingVariableId.length > 0 && root.editingVariable !== null
    readonly property string editorIssue: root.variableIssue(root.editingVariableId)
    readonly property bool statusVisible: !root.serviceAvailable || !root.writable || !root.catalogAvailable || !root.environmentAvailable || !root.environmentProjectionAvailable || !root.revisionTokenValid || !root.trustedValuesValid || root.loadState === "recovered" || root.loadState === "defaulted" || root.loadState === "unsupported" || root.managementState !== "managed" || root.applyState !== "current" || root.requiredActivation !== "none" || root.displayTestActive || root.externalChangeWhileEditing || root.environmentErrorMessage.length > 0 || root.sharedErrorMessage.length > 0 || root.draftHasUnavailableUwsm || root.busy || root.sharedMutationBusy || !root.sharedApplySafe
    readonly property bool statusIsDanger: root.managementState === "conflict" || root.applyState === "failed" || root.loadState === "unsupported" || (root.serviceAvailable && root.catalogAvailable && root.environmentAvailable && !root.environmentProjectionAvailable && root.environmentErrorMessage.length > 0) || (root.environmentProjectionAvailable && !root.trustedValuesValid) || root.externalChangeWhileEditing
    readonly property string statusMessage: {
        const environmentDetail = root.environmentErrorMessage.length > 0 ? " " + root.environmentErrorMessage : "";
        const sharedDetail = root.sharedErrorMessage.length > 0 ? " " + root.sharedErrorMessage : "";
        if (!root.serviceAvailable)
            return qsTr("Environment settings are unavailable. The compositor settings service may be restarting.%1").arg(sharedDetail);
        if (root.displayTestActive)
            return qsTr("A display test is active. Environment changes stay locked until that test is kept or reverted.");
        if (root.managementState === "unmanaged")
            return qsTr("HyprShelld is not managing the Hyprland entrypoint yet. Review takeover from Displays before changing startup variables.");
        if (root.managementState === "conflict")
            return qsTr("The managed compositor entrypoint changed unexpectedly. Environment changes are locked to preserve it.%1").arg(sharedDetail);
        if (!root.writable)
            return qsTr("This compositor configuration is read-only and has been preserved.");
        if (!root.catalogAvailable)
            return qsTr("The trusted Hyprland catalog is unavailable or does not match the compositor authority. Environment changes are disabled.%1").arg(environmentDetail);
        if (!root.environmentAvailable)
            return qsTr("The environment-variable transaction is unavailable. Existing desired state remains preserved.%1").arg(environmentDetail);
        if (!root.environmentProjectionAvailable) {
            return root.environmentErrorMessage.length > 0 ? qsTr("Environment authority verification failed. Variables cannot be trusted or changed until this check succeeds.%1").arg(environmentDetail) : qsTr("Environment variables are waiting for a current, verified full compositor projection.");
        }
        if (!root.trustedValuesValid)
            return qsTr("The current environment projection is not an exact managed-v1 collection. No compositor values will be written.%1").arg(environmentDetail);
        if (!root.revisionTokenValid)
            return qsTr("The exact compositor revision token is unavailable. Environment changes are disabled to prevent overwriting another revision.");
        if (root.externalChangeWhileEditing)
            return qsTr("Compositor settings changed outside this draft. Your complete environment draft is preserved, but it cannot be saved over the newer revision. Load the current settings to continue.");
        if (root.draftHasUnavailableUwsm)
            return qsTr("This draft contains UWSM-owned variables, but UWSM environment publishing is not available yet. The records are preserved; change their owner to Hyprland or wait for UWSM integration before saving.");
        if (root.busy) {
            if (root.busyOperation === "environment-save")
                return qsTr("Saving the validated environment-variable collection…");
            if (root.busyOperation === "compositor-apply" || root.busyOperation === "environment-apply") {
                return qsTr("Applying and verifying the saved compositor revision…");
            }
            if (root.busyOperation === "recover")
                return qsTr("Restoring and verifying the last working compositor configuration…");
            return qsTr("Another compositor operation is in progress. Environment changes are temporarily locked.");
        }
        if (root.sharedMutationBusy)
            return qsTr("A shared compositor setting is changing. Environment changes remain locked until that transition is verified.");
        if (root.applyState === "retained" || root.applyState === "failed" || root.applyState === "inactive") {
            if (root.requiredActivation === "session")
                return qsTr("The variables were saved, but startup and UWSM environment changes take effect only in a verified new session. The current session was not mutated.%1").arg(sharedDetail);
            if (root.requiredActivation === "restart")
                return qsTr("The variables were saved, but this revision requires a verified compositor restart before it becomes active.%1").arg(sharedDetail);
            return root.retryApplyAvailable ? qsTr("The desired compositor settings were saved but are not active. Retry the exact revision or restore the last working configuration.%1").arg(sharedDetail) : qsTr("The desired compositor state is not active. Review recovery before making another change.%1").arg(sharedDetail);
        }
        if (root.loadState === "recovered")
            return qsTr("Compositor settings were restored from the last known good desired-state copy. Review the variables before changing them.");
        if (root.loadState === "defaulted")
            return qsTr("Compositor settings could not be recovered, so safe empty environment defaults are in use.");
        if (root.loadState === "unsupported")
            return qsTr("The compositor settings use a newer format. They were preserved and remain read-only.");
        if (root.environmentErrorMessage.length > 0)
            return qsTr("The Environment operation failed. Your draft was preserved.%1").arg(environmentDetail);
        if (root.sharedErrorMessage.length > 0)
            return qsTr("The compositor operation failed.%1").arg(sharedDetail);
        if (!root.sharedApplySafe)
            return qsTr("Environment changes are waiting for the shared compositor apply path to become safe.");
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
        return JSON.stringify(left) === JSON.stringify(right);
    }

    function hasExactKeys(record, expected) {
        if (!record || typeof record !== "object" || Array.isArray(record))
            return false;
        const actual = Object.keys(record).sort();
        const wanted = expected.slice().sort();
        return JSON.stringify(actual) === JSON.stringify(wanted);
    }

    function validStableId(value) {
        return typeof value === "string" && value.length >= 1 && value.length <= root.maximumIdLength && /^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/.test(value);
    }

    function validEnvironmentName(value) {
        return typeof value === "string" && value.length >= 1 && value.length <= root.maximumNameLength && /^[A-Za-z_][A-Za-z0-9_]*$/.test(value);
    }

    function validEnvironmentValue(value) {
        return typeof value === "string" && value.length <= root.maximumValueLength && value.indexOf(String.fromCharCode(0)) < 0;
    }

    function validateEnvironmentRecord(record) {
        return root.hasExactKeys(record, ["id", "name", "value", "scope"]) && root.validStableId(record.id) && root.validEnvironmentName(record.name) && root.validEnvironmentValue(record.value) && root.scopeValues.includes(record.scope);
    }

    function validateEnvironmentCollection(records) {
        if (!Array.isArray(records) || records.length > root.maximumVariables)
            return false;
        const ids = new Set();
        const names = new Set();
        for (const record of records) {
            if (!root.validateEnvironmentRecord(record) || ids.has(record.id) || names.has(record.name)) {
                return false;
            }
            ids.add(record.id);
            names.add(record.name);
        }
        return true;
    }

    function variableIndex(id) {
        if (!Array.isArray(root.draftEnvironmentVariables))
            return -1;
        return root.draftEnvironmentVariables.findIndex(record => record && record.id === id);
    }

    function variableById(id) {
        const index = root.variableIndex(id);
        return index >= 0 ? root.draftEnvironmentVariables[index] : null;
    }

    function variableIssue(id) {
        const record = root.variableById(id);
        if (!record)
            return "";
        if (!root.validStableId(record.id))
            return qsTr("The record identity is not a canonical stable ID.");
        if (!root.validEnvironmentName(record.name))
            return qsTr("Use a variable name beginning with a letter or underscore and containing only letters, numbers, and underscores.");
        if (!root.validEnvironmentValue(record.value))
            return qsTr("The value must be at most 4096 characters and cannot contain a NUL character.");
        if (!root.scopeValues.includes(record.scope))
            return qsTr("Choose Hyprland startup or UWSM ownership.");
        for (const candidate of root.draftEnvironmentVariables) {
            if (candidate !== record && candidate && candidate.name === record.name)
                return qsTr("Every variable name must be unique.");
        }
        return "";
    }

    function nextRecordIdentity(prefix) {
        const used = new Set();
        for (const record of root.draftEnvironmentVariables) {
            if (record && typeof record.id === "string")
                used.add(record.id);
        }
        for (let suffix = 1; suffix <= root.maximumVariables + 1; ++suffix) {
            const candidate = prefix + suffix;
            if (!used.has(candidate))
                return candidate;
        }
        return "";
    }

    function nextVariableName() {
        const used = new Set();
        for (const record of root.draftEnvironmentVariables) {
            if (record && typeof record.name === "string")
                used.add(record.name);
        }
        if (!used.has("NEW_VARIABLE"))
            return "NEW_VARIABLE";
        for (let suffix = 2; suffix <= root.maximumVariables + 1; ++suffix) {
            const candidate = "NEW_VARIABLE_" + suffix;
            if (!used.has(candidate))
                return candidate;
        }
        return "VARIABLE";
    }

    function addVariable() {
        if (!root.controlsEnabled || root.draftEnvironmentVariables.length >= root.maximumVariables) {
            return;
        }
        const records = root.clone(root.draftEnvironmentVariables);
        const id = root.nextRecordIdentity("environment-");
        if (!records || id.length === 0)
            return;
        records.push({
            id: id,
            name: root.nextVariableName(),
            value: "",
            scope: "hyprland"
        });
        root.draftEnvironmentVariables = records;
        root.editingVariableId = id;
    }

    function setVariableField(id, field, value) {
        if (!root.controlsEnabled || !["name", "value", "scope"].includes(field)) {
            return;
        }
        const records = root.clone(root.draftEnvironmentVariables);
        const index = root.variableIndex(id);
        if (!records || index < 0)
            return;
        records[index][field] = value;
        root.draftEnvironmentVariables = records;
    }

    function moveVariable(id, offset) {
        if (!root.controlsEnabled || (offset !== -1 && offset !== 1))
            return;
        const records = root.clone(root.draftEnvironmentVariables);
        const index = root.variableIndex(id);
        const target = index + offset;
        if (!records || index < 0 || target < 0 || target >= records.length)
            return;
        const record = records[index];
        records[index] = records[target];
        records[target] = record;
        root.draftEnvironmentVariables = records;
    }

    function removeVariable(id) {
        if (!root.controlsEnabled)
            return;
        const records = root.clone(root.draftEnvironmentVariables);
        const index = root.variableIndex(id);
        if (!records || index < 0)
            return;
        records.splice(index, 1);
        root.draftEnvironmentVariables = records;
        if (root.editingVariableId === id)
            root.editingVariableId = "";
    }

    function openVariable(id) {
        if (root.controlsEnabled && root.variableIndex(id) >= 0)
            root.editingVariableId = id;
    }

    function closeEditor() {
        root.editingVariableId = "";
    }

    function resetDraftToDefaults() {
        if (!root.controlsEnabled)
            return;
        root.draftEnvironmentVariables = [];
        root.closeEditor();
    }

    function synchronizeDraft() {
        if (!root.serviceAvailable || !root.environmentProjectionAvailable || !root.revisionTokenValid || !root.trustedValuesValid || root.busy || root.sharedMutationBusy) {
            return;
        }
        const records = root.clone(root.environmentVariables);
        if (!records)
            return;
        root.synchronizedEnvironmentVariables = root.clone(records);
        root.draftEnvironmentVariables = records;
        root.synchronizedRevisionToken = root.revisionToken;
        root.projectionInitialized = true;
        root.externalChangeWhileEditing = false;
        root.saveSubmitted = false;
        root.submittedEnvironmentVariables = [];
        root.submittedRevisionToken = "";
        root.closeEditor();
    }

    function submitDraft() {
        if (!root.saveEnabled)
            return;
        const records = root.clone(root.draftEnvironmentVariables);
        if (!records)
            return;
        root.saveSubmitted = true;
        root.submittedEnvironmentVariables = root.clone(records);
        root.submittedRevisionToken = root.revisionToken;
        root.saveRequested(records);
        root.scheduleProjectionReview();
    }

    function reviewProjection() {
        if (!root.serviceAvailable || !root.trustedValuesValid) {
            if (root.serviceAvailable && !root.saveSubmitted && root.projectionInitialized && root.draftDirty && root.revisionTokenValid && root.synchronizedRevisionTokenValid && root.revisionToken !== root.synchronizedRevisionToken) {
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
            if (root.valueEqual(root.environmentVariables, root.submittedEnvironmentVariables)) {
                root.synchronizeDraft();
                return;
            }
            if (root.revisionToken === root.submittedRevisionToken) {
                root.saveSubmitted = false;
                root.submittedEnvironmentVariables = [];
                root.submittedRevisionToken = "";
                return;
            }
            root.saveSubmitted = false;
            root.externalChangeWhileEditing = true;
            return;
        }
        const projectionChanged = root.revisionToken !== root.synchronizedRevisionToken || !root.valueEqual(root.environmentVariables, root.synchronizedEnvironmentVariables);
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

    function scopeIndex(scope) {
        return Math.max(0, root.scopeValues.indexOf(scope));
    }

    function scopeSummary(scope) {
        return scope === "uwsm" ? qsTr("UWSM-owned; written to the session environment") : qsTr("Hyprland-owned; emitted with hl.env at startup");
    }

    function environmentExample(record) {
        const name = record && typeof record.name === "string" ? record.name : "XCURSOR_SIZE";
        const value = record && typeof record.value === "string" ? record.value : "24";
        if (record && record.scope === "uwsm")
            return "export " + name + "=\"" + value + "\"";
        return "hl.env(\"" + name + "\", \"" + value + "\")";
    }

    onEnvironmentVariablesChanged: root.scheduleProjectionReview()
    onEnvironmentProjectionAvailableChanged: root.scheduleProjectionReview()
    onRevisionTokenChanged: root.scheduleProjectionReview()
    onServiceAvailableChanged: root.scheduleProjectionReview()
    onBusyChanged: root.scheduleProjectionReview()
    onSharedMutationBusyChanged: root.scheduleProjectionReview()
    onEnvironmentErrorNameChanged: root.scheduleProjectionReview()
    onEnvironmentErrorMessageChanged: root.scheduleProjectionReview()
    onSharedErrorNameChanged: root.scheduleProjectionReview()
    onSharedErrorMessageChanged: root.scheduleProjectionReview()
    onApplyStateChanged: root.scheduleProjectionReview()
    Component.onCompleted: root.scheduleProjectionReview()

    background: Rectangle {
        color: root.palette.window
    }

    ScrollView {
        id: environmentScrollView

        objectName: "environmentScrollView"
        anchors.fill: parent
        anchors.topMargin: root.compactPage ? Math.min(root.contentTopMargin, 12) : root.contentTopMargin
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            objectName: "environmentContent"
            x: Math.max(24, (root.width - width) / 2)
            width: Math.max(0, Math.min(root.width - 48, 980))
            spacing: root.compactPage ? 14 : 18

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ToolButton {
                    objectName: "environmentBackButton"
                    implicitWidth: root.minimumTargetSize
                    implicitHeight: root.minimumTargetSize
                    text: "‹"
                    font.pixelSize: 28
                    Accessible.name: qsTr("Back to Hyprland categories")
                    onClicked: root.backRequested()
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 3

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Environment Variables")
                        color: root.palette.text
                        font.pixelSize: root.compactPage ? 24 : 28
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Define ordered startup variables and choose whether Hyprland or UWSM owns each value.")
                        color: root.palette.placeholderText
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }

                Button {
                    objectName: "refreshEnvironmentButton"
                    implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                    text: qsTr("Refresh")
                    enabled: !root.busy && !root.displayTestActive
                    icon.name: "view-refresh-symbolic"
                    Accessible.name: qsTr("Refresh compositor environment variables")

                    onClicked: root.refreshRequested()
                }
            }

            Frame {
                objectName: "environmentStatusCard"
                Layout.fillWidth: true
                visible: root.statusVisible
                padding: root.compactPage ? 12 : 16

                background: Rectangle {
                    color: root.statusIsDanger ? "#382125" : "#33251a"
                    radius: 12
                    border.color: root.statusIsDanger ? "#8bfb7185" : "#8bf6ad55"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    Label {
                        objectName: "environmentStatusMessage"
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
                        spacing: 8

                        Button {
                            objectName: "environmentOpenDisplaysButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            visible: root.serviceAvailable && root.managementState === "unmanaged"
                            text: qsTr("Review takeover in Displays")
                            enabled: !root.busy
                            Accessible.name: qsTr("Open Displays to review compositor takeover")

                            onClicked: root.openDisplaysRequested()
                        }

                        Button {
                            objectName: "loadCurrentEnvironmentButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            visible: root.externalChangeWhileEditing
                            text: qsTr("Load current settings")
                            enabled: !root.busy && !root.sharedMutationBusy && !root.saveSubmitted && root.environmentProjectionAvailable && root.trustedValuesValid
                            Accessible.name: qsTr("Discard the complete Environment draft and load current settings")

                            onClicked: root.synchronizeDraft()
                        }

                        Button {
                            objectName: "retryApplyEnvironmentButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            visible: root.retryApplyAvailable
                            text: root.busyOperation === "compositor-apply" || root.busyOperation === "environment-apply" ? qsTr("Retrying apply…") : qsTr("Retry apply")
                            enabled: root.retryApplyAvailable && !root.busy && !root.sharedMutationBusy && root.sharedApplySafe
                            Accessible.name: qsTr("Retry applying the exact saved compositor revision")

                            onClicked: root.retryApplyRequested()
                        }

                        Button {
                            objectName: "recoverEnvironmentButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            visible: root.recoveryAvailable
                            text: qsTr("Restore last working configuration")
                            enabled: root.recoveryAvailable && !root.busy && !root.sharedMutationBusy
                            Accessible.name: qsTr("Review whole-compositor recovery")

                            onClicked: environmentRecoveryDialog.open()
                        }
                    }
                }
            }

            Frame {
                objectName: "environmentOwnershipPreview"
                Layout.fillWidth: true
                padding: root.compactPage ? 14 : 18

                background: Rectangle {
                    color: root.palette.base
                    radius: 16
                    border.color: root.palette.mid
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: root.compactPage ? 12 : 18

                    Rectangle {
                        Layout.preferredWidth: root.compactPage ? 112 : 154
                        Layout.preferredHeight: root.compactPage ? 104 : 118
                        radius: 14
                        color: "#192530"
                        border.color: "#6aa9d8"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 7

                            Label {
                                Layout.fillWidth: true
                                text: "LUA"
                                color: "#8fd4ff"
                                font.bold: true
                                font.pixelSize: 11
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 7
                                radius: 4
                                color: "#8fd4ff"
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.rightMargin: 24
                                Layout.preferredHeight: 7
                                radius: 4
                                color: "#6e8292"
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.rightMargin: 9
                                Layout.preferredHeight: 7
                                radius: 4
                                color: "#78c995"
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "SESSION"
                                color: "#9fb0bd"
                                font.bold: true
                                font.pixelSize: 10
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 6

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("One value, one startup owner")
                            color: root.palette.text
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Hyprland scope emits hl.env before compositor startup. UWSM scope belongs in its session environment. Neither choice mutates the already-running process environment.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }

                        Label {
                            objectName: "environmentUwsmAvailability"
                            Layout.fillWidth: true
                            visible: !root.uwsmIntegrationAvailable
                            text: qsTr("UWSM publishing is currently unavailable. UWSM records remain visible and editable, but cannot be saved through this page yet.")
                            color: "#ffd5a1"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }

                        Label {
                            objectName: "environmentCodeExample"
                            Layout.fillWidth: true
                            text: root.environmentExample(root.editingVariable)
                            color: "#9cd7ff"
                            font.family: "monospace"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                            Accessible.name: qsTr("Environment configuration example: %1").arg(text)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Ordered variables")
                        color: root.palette.text
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Names are unique. Reordering is preserved in the generated configuration.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }

                Button {
                    objectName: "addEnvironmentVariableButton"
                    implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                    text: qsTr("Add variable")
                    enabled: root.controlsEnabled && root.draftEnvironmentVariables.length < root.maximumVariables
                    Accessible.name: qsTr("Add an environment variable to the draft")

                    onClicked: root.addVariable()
                }
            }

            Label {
                objectName: "emptyEnvironmentVariablesMessage"
                Layout.fillWidth: true
                visible: root.draftEnvironmentVariables.length === 0
                text: qsTr("No managed startup variables are saved. Applications inherit their normal session environment.")
                color: root.palette.placeholderText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Repeater {
                model: Array.isArray(root.draftEnvironmentVariables) ? root.draftEnvironmentVariables : []

                delegate: Frame {
                    id: variableCard

                    required property int index
                    required property var modelData

                    objectName: "environmentVariableCard" + variableCard.index
                    Layout.fillWidth: true
                    padding: root.compactPage ? 12 : 16

                    background: Rectangle {
                        color: root.palette.base
                        radius: 14
                        border.color: root.editingVariableId === String(variableCard.modelData.id) ? root.palette.highlight : root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: String(variableCard.modelData.name)
                                    color: root.palette.text
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.scopeSummary(variableCard.modelData.scope)
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                            }

                            Label {
                                text: variableCard.modelData.scope === "uwsm" ? qsTr("UWSM") : qsTr("LUA")
                                color: variableCard.modelData.scope === "uwsm" ? "#a9e6bc" : "#9cd7ff"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                textFormat: Text.PlainText
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.environmentExample(variableCard.modelData)
                            color: "#9fb0bd"
                            font.family: "monospace"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: childrenRect.height
                            spacing: 8

                            Button {
                                objectName: "editEnvironmentVariableButton" + variableCard.index
                                implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                text: qsTr("Edit")
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Edit environment variable %1").arg(variableCard.modelData.name)

                                onClicked: root.openVariable(String(variableCard.modelData.id))
                            }

                            Button {
                                objectName: "moveEnvironmentVariableUpButton" + variableCard.index
                                implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                text: qsTr("Move up")
                                enabled: root.controlsEnabled && variableCard.index > 0
                                Accessible.name: qsTr("Move %1 up").arg(variableCard.modelData.name)

                                onClicked: root.moveVariable(String(variableCard.modelData.id), -1)
                            }

                            Button {
                                objectName: "moveEnvironmentVariableDownButton" + variableCard.index
                                implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                text: qsTr("Move down")
                                enabled: root.controlsEnabled && variableCard.index + 1 < root.draftEnvironmentVariables.length
                                Accessible.name: qsTr("Move %1 down").arg(variableCard.modelData.name)

                                onClicked: root.moveVariable(String(variableCard.modelData.id), 1)
                            }

                            Button {
                                objectName: "removeEnvironmentVariableButton" + variableCard.index
                                implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                text: qsTr("Remove")
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Remove %1 from the environment draft").arg(variableCard.modelData.name)

                                onClicked: root.removeVariable(String(variableCard.modelData.id))
                            }
                        }

                        Frame {
                            objectName: "environmentVariableEditor" + variableCard.index
                            Layout.fillWidth: true
                            visible: root.editingVariableId === String(variableCard.modelData.id)
                            padding: root.compactPage ? 12 : 16

                            background: Rectangle {
                                color: "#18232b"
                                radius: 12
                                border.color: root.editorIssue.length === 0 ? "#598cb0" : "#a8606a"
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 10

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Variable name")
                                    color: "#eef5f8"
                                    font.weight: Font.Medium
                                    textFormat: Text.PlainText
                                }

                                TextField {
                                    objectName: "environmentVariableNameField" + variableCard.index
                                    Layout.fillWidth: true
                                    text: String(variableCard.modelData.name)
                                    enabled: root.controlsEnabled
                                    maximumLength: root.maximumNameLength
                                    selectByMouse: true
                                    placeholderText: qsTr("XCURSOR_SIZE")
                                    Accessible.name: qsTr("Environment variable name")

                                    onTextEdited: root.setVariableField(String(variableCard.modelData.id), "name", text)
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Value")
                                    color: "#eef5f8"
                                    font.weight: Font.Medium
                                    textFormat: Text.PlainText
                                }

                                TextField {
                                    objectName: "environmentVariableValueField" + variableCard.index
                                    Layout.fillWidth: true
                                    text: String(variableCard.modelData.value)
                                    enabled: root.controlsEnabled
                                    maximumLength: root.maximumValueLength
                                    selectByMouse: true
                                    placeholderText: qsTr("24")
                                    Accessible.name: qsTr("Environment variable value")

                                    onTextEdited: root.setVariableField(String(variableCard.modelData.id), "value", text)
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Startup owner")
                                    color: "#eef5f8"
                                    font.weight: Font.Medium
                                    textFormat: Text.PlainText
                                }

                                ComboBox {
                                    objectName: "environmentVariableScopeSelect" + variableCard.index
                                    Layout.fillWidth: true
                                    implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                    model: root.scopeLabels
                                    currentIndex: root.scopeIndex(variableCard.modelData.scope)
                                    enabled: root.controlsEnabled
                                    Accessible.name: qsTr("Environment variable startup owner")

                                    onActivated: index => root.setVariableField(String(variableCard.modelData.id), "scope", root.scopeValues[index])
                                }

                                Label {
                                    objectName: "environmentVariableEditorIssue" + variableCard.index
                                    Layout.fillWidth: true
                                    visible: root.editorIssue.length > 0
                                    text: root.editorIssue
                                    color: "#ffb8c3"
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                    Accessible.role: Accessible.AlertMessage
                                    Accessible.name: text
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Values are stored literally. Existing shell variables are not expanded by this editor, and a new session may be required before applications inherit the change.")
                                    color: "#b9c7cf"
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                }

                                Button {
                                    objectName: "doneEditingEnvironmentVariableButton" + variableCard.index
                                    implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                    text: qsTr("Done")
                                    Accessible.name: qsTr("Close the environment variable editor")

                                    onClicked: root.closeEditor()
                                }
                            }
                        }
                    }
                }
            }

            Frame {
                objectName: "environmentDraftActionsCard"
                Layout.fillWidth: true
                padding: root.compactPage ? 14 : 18

                background: Rectangle {
                    color: root.palette.base
                    radius: 16
                    border.color: root.draftValid && !root.draftHasUnavailableUwsm ? root.palette.mid : "#a8606a"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        text: !root.draftValid ? qsTr("Finish every variable and remove duplicate names before saving.") : root.draftHasUnavailableUwsm ? qsTr("UWSM-owned records are preserved, but cannot be saved until UWSM publishing is available.") : qsTr("The complete ordered collection is validated before it replaces desired state.")
                        color: root.draftValid && !root.draftHasUnavailableUwsm ? root.palette.placeholderText : "#ffb8c3"
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: childrenRect.height
                        spacing: 10

                        Button {
                            objectName: "discardEnvironmentDraftButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            text: qsTr("Discard draft")
                            visible: root.draftDirty && !root.externalChangeWhileEditing
                            enabled: root.discardEnabled
                            Accessible.name: qsTr("Discard the complete Environment draft")

                            onClicked: root.synchronizeDraft()
                        }

                        Button {
                            objectName: "resetEnvironmentDefaultsButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            text: qsTr("Reset to defaults")
                            enabled: root.resetEnabled
                            Accessible.name: qsTr("Clear every managed environment variable")

                            onClicked: root.resetDraftToDefaults()
                        }

                        Button {
                            objectName: "saveEnvironmentButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            text: root.busyOperation === "environment-save" ? qsTr("Saving…") : (root.busyOperation === "compositor-apply" || root.busyOperation === "environment-apply") ? qsTr("Applying…") : qsTr("Save & apply")
                            highlighted: true
                            enabled: root.saveEnabled
                            Accessible.name: qsTr("Save the validated environment-variable collection")

                            onClicked: root.submitDraft()
                        }
                    }
                }
            }

            Item {
                Layout.preferredHeight: 12
            }
        }
    }

    CompositorRecoveryDialog {
        id: environmentRecoveryDialog

        objectName: "environmentRecoveryDialog"
        recoveryAvailable: root.recoveryAvailable
        operationBusy: root.busy || root.sharedMutationBusy
        busyOperation: root.busyOperation
        settingsAreaName: qsTr("Environment Variables")
        warningObjectName: "environmentRecoveryWarning"
        cancelObjectName: "cancelEnvironmentRecoveryButton"
        confirmObjectName: "confirmEnvironmentRecoveryButton"
        minimumTargetSize: root.minimumTargetSize

        onRecoveryRequested: root.recoveryRequested()
    }
}
