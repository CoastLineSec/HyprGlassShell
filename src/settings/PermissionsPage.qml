pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    property bool serviceAvailable: false
    property bool writable: false
    property bool catalogAvailable: false
    property bool permissionsAvailable: false
    property bool permissionsProjectionAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property var permissions: []
    property string revisionToken: "0"
    property string loadState: "unavailable"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property string permissionErrorName: ""
    property string permissionErrorMessage: ""
    property string sharedErrorName: ""
    property string sharedErrorMessage: ""
    property bool retryApplyAvailable: false
    property bool recoveryAvailable: false
    property bool sharedMutationBusy: false
    property bool sharedApplySafe: false
    property real contentTopMargin: 28

    property var draftPermissions: []
    property var synchronizedPermissions: []
    property var submittedPermissions: []
    property string synchronizedRevisionToken: ""
    property string submittedRevisionToken: ""
    property bool projectionInitialized: false
    property bool externalChangeWhileEditing: false
    property bool saveSubmitted: false
    property string editingPermissionId: ""

    signal refreshRequested
    signal backRequested
    signal openDisplaysRequested
    signal saveRequested(var permissions)
    signal retryApplyRequested
    signal recoveryRequested

    readonly property real minimumTargetSize: 44
    readonly property bool compactPage: root.width < 620
    readonly property int maximumPermissions: 256
    readonly property int maximumIdLength: 128
    readonly property int maximumBinaryLength: 512
    readonly property var permissionTypes: ["screencopy", "cursorpos", "plugin", "keyboard", "input-capture"]
    readonly property var permissionTypeLabels: [qsTr("Screen capture"), qsTr("Cursor position"), qsTr("Plugin loading"), qsTr("New keyboards"), qsTr("Input capture")]
    readonly property var permissionModes: ["ask", "allow", "deny"]
    readonly property var permissionModeLabels: [qsTr("Ask"), qsTr("Allow"), qsTr("Deny")]
    readonly property bool revisionTokenValid: /^(0|[1-9][0-9]*)$/.test(root.revisionToken)
    readonly property bool synchronizedRevisionTokenValid: /^(0|[1-9][0-9]*)$/.test(root.synchronizedRevisionToken)
    readonly property bool trustedValuesValid: root.permissionsProjectionAvailable && root.validatePermissionCollection(root.permissions)
    readonly property bool draftValid: root.validatePermissionCollection(root.draftPermissions)
    readonly property bool draftDirty: root.projectionInitialized && !root.valueEqual(root.draftPermissions, root.synchronizedPermissions)
    readonly property bool displayTestActive: root.confirmationState !== "idle" || root.managementState === "preview"
    readonly property bool controlsEnabled: root.serviceAvailable && root.writable && root.catalogAvailable && root.permissionsAvailable && root.permissionsProjectionAvailable && root.revisionTokenValid && root.trustedValuesValid && root.managementState === "managed" && root.loadState !== "unsupported" && !root.busy && !root.saveSubmitted && !root.sharedMutationBusy && root.sharedApplySafe && !root.externalChangeWhileEditing && !root.displayTestActive
    readonly property bool saveEnabled: root.controlsEnabled && root.draftDirty && root.draftValid
    readonly property bool discardEnabled: root.serviceAvailable && root.permissionsProjectionAvailable && root.trustedValuesValid && root.projectionInitialized && !root.busy && !root.saveSubmitted && !root.sharedMutationBusy && !root.externalChangeWhileEditing
    readonly property bool resetEnabled: root.controlsEnabled && root.draftPermissions.length > 0
    readonly property var editingPermission: root.permissionById(root.editingPermissionId)
    readonly property bool editorActive: root.editingPermissionId.length > 0 && root.editingPermission !== null
    readonly property string editorIssue: root.permissionIssue(root.editingPermissionId)
    readonly property bool statusVisible: !root.serviceAvailable || !root.writable || !root.catalogAvailable || !root.permissionsAvailable || !root.permissionsProjectionAvailable || !root.revisionTokenValid || !root.trustedValuesValid || root.loadState === "recovered" || root.loadState === "defaulted" || root.loadState === "unsupported" || root.managementState !== "managed" || root.applyState !== "current" || root.requiredActivation !== "none" || root.displayTestActive || root.externalChangeWhileEditing || root.permissionErrorMessage.length > 0 || root.sharedErrorMessage.length > 0 || root.busy || root.sharedMutationBusy || !root.sharedApplySafe
    readonly property bool statusIsDanger: root.managementState === "conflict" || root.applyState === "failed" || root.loadState === "unsupported" || (root.permissionsProjectionAvailable && !root.trustedValuesValid) || root.externalChangeWhileEditing
    readonly property string statusMessage: {
        const detail = root.permissionErrorMessage.length > 0 ? " " + root.permissionErrorMessage : "";
        const sharedDetail = root.sharedErrorMessage.length > 0 ? " " + root.sharedErrorMessage : "";
        if (!root.serviceAvailable)
            return qsTr("Permission settings are unavailable. The compositor settings service may be restarting.%1").arg(sharedDetail);
        if (root.displayTestActive)
            return qsTr("A display test is active. Permission changes stay locked until that test is kept or reverted.");
        if (root.managementState === "unmanaged")
            return qsTr("HyprShelld is not managing the Hyprland entrypoint yet. Review takeover from Displays before changing permissions.");
        if (root.managementState === "conflict")
            return qsTr("The managed compositor entrypoint changed unexpectedly. Permission changes are locked to preserve it.%1").arg(sharedDetail);
        if (!root.writable)
            return qsTr("This compositor configuration is read-only and has been preserved.");
        if (!root.catalogAvailable)
            return qsTr("The trusted Hyprland catalog is unavailable or does not match the compositor authority. Permission changes are disabled.%1").arg(detail);
        if (!root.permissionsAvailable)
            return qsTr("The permission transaction is unavailable. Existing desired state remains preserved.%1").arg(detail);
        if (!root.permissionsProjectionAvailable) {
            return root.permissionErrorMessage.length > 0 ? qsTr("Permission authority verification failed. Rules cannot be trusted or changed until this check succeeds.%1").arg(detail) : qsTr("Permissions are waiting for a current, verified full compositor projection.");
        }
        if (!root.trustedValuesValid)
            return qsTr("The current permission projection is not an exact managed-v1 collection. No compositor values will be written.%1").arg(detail);
        if (!root.revisionTokenValid)
            return qsTr("The exact compositor revision token is unavailable. Permission changes are disabled to prevent overwriting another revision.");
        if (root.externalChangeWhileEditing)
            return qsTr("Compositor settings changed outside this draft. Your complete permission draft is preserved, but it cannot be saved over the newer revision. Load the current settings to continue.");
        if (root.busy) {
            if (root.busyOperation === "permissions-save")
                return qsTr("Saving the validated permission collection…");
            if (root.busyOperation === "compositor-apply" || root.busyOperation === "permissions-apply")
                return qsTr("Applying and verifying the saved compositor revision…");
            if (root.busyOperation === "recover")
                return qsTr("Restoring and verifying the last working compositor configuration…");
            return qsTr("Another compositor operation is in progress. Permission changes are temporarily locked.");
        }
        if (root.sharedMutationBusy)
            return qsTr("A shared compositor setting is changing. Permission changes remain locked until that transition is verified.");
        if (root.applyState === "retained" || root.applyState === "failed" || root.applyState === "inactive") {
            if (root.requiredActivation === "restart")
                return qsTr("The permissions were saved, but Hyprland must be restarted and verified before this security policy becomes active. The running policy was not partially changed.%1").arg(sharedDetail);
            return root.retryApplyAvailable ? qsTr("The desired compositor settings were saved but are not active. Retry the exact revision or restore the last working configuration.%1").arg(sharedDetail) : qsTr("The desired compositor state is not active. Review recovery before making another change.%1").arg(sharedDetail);
        }
        if (root.loadState === "recovered")
            return qsTr("Compositor settings were restored from the last known good desired-state copy. Review the permissions before changing them.");
        if (root.loadState === "defaulted")
            return qsTr("Compositor settings could not be recovered, so the safe empty permission defaults are in use.");
        if (root.loadState === "unsupported")
            return qsTr("The compositor settings use a newer format. They were preserved and remain read-only.");
        if (root.permissionErrorMessage.length > 0)
            return qsTr("The Permissions operation failed. Your draft was preserved.%1").arg(detail);
        if (root.sharedErrorMessage.length > 0)
            return qsTr("The compositor operation failed.%1").arg(sharedDetail);
        if (!root.sharedApplySafe)
            return qsTr("Permission changes are waiting for the shared compositor apply path to become safe.");
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

    function validPermissionBinary(value) {
        return typeof value === "string" && value.length >= 1 && value.length <= root.maximumBinaryLength && /^[^\u0000-\u001f\u007f]+$/.test(value);
    }

    function validatePermissionRecord(record) {
        return root.hasExactKeys(record, ["id", "binary", "type", "mode"]) && root.validStableId(record.id) && root.validPermissionBinary(record.binary) && root.permissionTypes.includes(record.type) && root.permissionModes.includes(record.mode);
    }

    function validatePermissionCollection(records) {
        if (!Array.isArray(records) || records.length > root.maximumPermissions)
            return false;
        const ids = new Set();
        const identities = new Set();
        for (const record of records) {
            if (!root.validatePermissionRecord(record))
                return false;
            const identity = record.binary + "\u0000" + record.type;
            if (ids.has(record.id) || identities.has(identity))
                return false;
            ids.add(record.id);
            identities.add(identity);
        }
        return true;
    }

    function permissionIndex(id) {
        if (!Array.isArray(root.draftPermissions))
            return -1;
        return root.draftPermissions.findIndex(record => record && record.id === id);
    }

    function permissionById(id) {
        const index = root.permissionIndex(id);
        return index >= 0 ? root.draftPermissions[index] : null;
    }

    function permissionIssue(id) {
        const record = root.permissionById(id);
        if (!record)
            return "";
        if (!root.validStableId(record.id))
            return qsTr("The record identity is not a canonical stable ID.");
        if (!root.validPermissionBinary(record.binary))
            return qsTr("Enter a non-empty RE2 binary pattern of at most 512 characters without control characters.");
        if (!root.permissionTypes.includes(record.type))
            return qsTr("Choose a supported Hyprland permission type.");
        if (!root.permissionModes.includes(record.mode))
            return qsTr("Choose Ask, Allow, or Deny.");
        for (const candidate of root.draftPermissions) {
            if (candidate !== record && candidate && candidate.binary === record.binary && candidate.type === record.type) {
                return qsTr("Every binary and permission-type pair must be unique.");
            }
        }
        return "";
    }

    function nextRecordIdentity(prefix) {
        const used = new Set();
        for (const record of root.draftPermissions) {
            if (record && typeof record.id === "string")
                used.add(record.id);
        }
        for (let suffix = 1; suffix <= root.maximumPermissions + 1; ++suffix) {
            const candidate = prefix + suffix;
            if (!used.has(candidate))
                return candidate;
        }
        return "";
    }

    function nextBinaryPattern() {
        const used = new Set();
        for (const record of root.draftPermissions) {
            if (record && typeof record.binary === "string" && record.type === "screencopy") {
                used.add(record.binary);
            }
        }
        if (!used.has("^/usr/bin/example$"))
            return "^/usr/bin/example$";
        for (let suffix = 2; suffix <= root.maximumPermissions + 1; ++suffix) {
            const candidate = "^/usr/bin/example-" + suffix + "$";
            if (!used.has(candidate))
                return candidate;
        }
        return ".*";
    }

    function addPermission() {
        if (!root.controlsEnabled || root.draftPermissions.length >= root.maximumPermissions)
            return;
        const records = root.clone(root.draftPermissions);
        const id = root.nextRecordIdentity("permission-");
        if (!records || id.length === 0)
            return;
        records.push({
            id: id,
            binary: root.nextBinaryPattern(),
            type: "screencopy",
            mode: "ask"
        });
        root.draftPermissions = records;
        root.editingPermissionId = id;
    }

    function setPermissionField(id, field, value) {
        if (!root.controlsEnabled || !["binary", "type", "mode"].includes(field))
            return;
        const records = root.clone(root.draftPermissions);
        const index = root.permissionIndex(id);
        if (!records || index < 0)
            return;
        records[index][field] = value;
        root.draftPermissions = records;
    }

    function movePermission(id, offset) {
        if (!root.controlsEnabled || (offset !== -1 && offset !== 1))
            return;
        const records = root.clone(root.draftPermissions);
        const index = root.permissionIndex(id);
        const target = index + offset;
        if (!records || index < 0 || target < 0 || target >= records.length)
            return;
        const record = records[index];
        records[index] = records[target];
        records[target] = record;
        root.draftPermissions = records;
    }

    function removePermission(id) {
        if (!root.controlsEnabled)
            return;
        const records = root.clone(root.draftPermissions);
        const index = root.permissionIndex(id);
        if (!records || index < 0)
            return;
        records.splice(index, 1);
        root.draftPermissions = records;
        if (root.editingPermissionId === id)
            root.editingPermissionId = "";
    }

    function openPermission(id) {
        if (root.controlsEnabled && root.permissionIndex(id) >= 0)
            root.editingPermissionId = id;
    }

    function closeEditor() {
        root.editingPermissionId = "";
    }

    function resetDraftToDefaults() {
        if (!root.controlsEnabled)
            return;
        root.draftPermissions = [];
        root.closeEditor();
    }

    function synchronizeDraft() {
        if (!root.serviceAvailable || !root.permissionsProjectionAvailable || !root.revisionTokenValid || !root.trustedValuesValid || root.busy || root.sharedMutationBusy)
            return;
        const records = root.clone(root.permissions);
        if (!records)
            return;
        root.synchronizedPermissions = root.clone(records);
        root.draftPermissions = records;
        root.synchronizedRevisionToken = root.revisionToken;
        root.projectionInitialized = true;
        root.externalChangeWhileEditing = false;
        root.saveSubmitted = false;
        root.submittedPermissions = [];
        root.submittedRevisionToken = "";
        root.closeEditor();
    }

    function submitDraft() {
        if (!root.saveEnabled)
            return;
        const records = root.clone(root.draftPermissions);
        if (!records)
            return;
        root.saveSubmitted = true;
        root.submittedPermissions = root.clone(records);
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
            if (root.valueEqual(root.permissions, root.submittedPermissions)) {
                root.synchronizeDraft();
                return;
            }
            if (root.revisionToken === root.submittedRevisionToken) {
                root.saveSubmitted = false;
                root.submittedPermissions = [];
                root.submittedRevisionToken = "";
                return;
            }
            root.saveSubmitted = false;
            root.externalChangeWhileEditing = true;
            return;
        }
        const projectionChanged = root.revisionToken !== root.synchronizedRevisionToken || !root.valueEqual(root.permissions, root.synchronizedPermissions);
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

    function typeIndex(type) {
        return Math.max(0, root.permissionTypes.indexOf(type));
    }

    function modeIndex(mode) {
        return Math.max(0, root.permissionModes.indexOf(mode));
    }

    function typeLabel(type) {
        const index = root.permissionTypes.indexOf(type);
        return index >= 0 ? root.permissionTypeLabels[index] : String(type);
    }

    function modeLabel(mode) {
        const index = root.permissionModes.indexOf(mode);
        return index >= 0 ? root.permissionModeLabels[index] : String(mode);
    }

    function permissionRisk(record) {
        if (!record)
            return qsTr("Permission policy controls security-sensitive compositor capabilities.");
        if (record.mode === "deny")
            return qsTr("Deny blocks matching binaries. Keep a deliberate final fallback and check first-match ordering.");
        if (record.mode === "allow" && ["plugin", "keyboard", "input-capture"].includes(record.type)) {
            return qsTr("High risk: Allow grants a matching binary a privileged input or code-loading capability without prompting.");
        }
        if (record.mode === "allow")
            return qsTr("Allow grants this capability without prompting whenever the binary pattern matches.");
        return qsTr("Ask requires an interactive decision for matching requests.");
    }

    function luaQuoted(value) {
        return "\"" + String(value).replace(/\\/g, "\\\\").replace(/\"/g, "\\\"").replace(/\n/g, "\\n").replace(/\r/g, "\\r") + "\"";
    }

    function permissionExample(record) {
        const binary = record ? record.binary : "^/usr/bin/portal$";
        const type = record ? record.type : "screencopy";
        const mode = record ? record.mode : "ask";
        return "hl.permission(" + root.luaQuoted(binary) + ", " + root.luaQuoted(type) + ", " + root.luaQuoted(mode) + ")";
    }

    onPermissionsChanged: root.scheduleProjectionReview()
    onPermissionsProjectionAvailableChanged: root.scheduleProjectionReview()
    onRevisionTokenChanged: root.scheduleProjectionReview()
    onServiceAvailableChanged: root.scheduleProjectionReview()
    onBusyChanged: root.scheduleProjectionReview()
    onSharedMutationBusyChanged: root.scheduleProjectionReview()
    onPermissionErrorNameChanged: root.scheduleProjectionReview()
    onPermissionErrorMessageChanged: root.scheduleProjectionReview()
    onSharedErrorNameChanged: root.scheduleProjectionReview()
    onSharedErrorMessageChanged: root.scheduleProjectionReview()
    onApplyStateChanged: root.scheduleProjectionReview()
    Component.onCompleted: root.scheduleProjectionReview()

    background: Rectangle {
        color: root.palette.window
    }

    ScrollView {
        id: permissionsScrollView

        objectName: "permissionsScrollView"
        anchors.fill: parent
        anchors.topMargin: root.compactPage ? Math.min(root.contentTopMargin, 12) : root.contentTopMargin
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            objectName: "permissionsContent"
            x: Math.max(24, (root.width - width) / 2)
            width: Math.max(0, Math.min(root.width - 48, 980))
            spacing: root.compactPage ? 14 : 18

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ToolButton {
                    objectName: "permissionsBackButton"
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
                        text: qsTr("Permissions")
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
                        text: qsTr("Build the ordered Hyprland capability policy evaluated for matching application binaries.")
                        color: root.palette.placeholderText
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }

                Button {
                    objectName: "refreshPermissionsButton"
                    implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                    text: qsTr("Refresh")
                    enabled: !root.busy && !root.displayTestActive
                    icon.name: "view-refresh-symbolic"
                    Accessible.name: qsTr("Refresh compositor permissions")

                    onClicked: root.refreshRequested()
                }
            }

            Frame {
                objectName: "permissionsStatusCard"
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
                        objectName: "permissionsStatusMessage"
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
                            objectName: "permissionsOpenDisplaysButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            visible: root.serviceAvailable && root.managementState === "unmanaged"
                            text: qsTr("Review takeover in Displays")
                            enabled: !root.busy
                            onClicked: root.openDisplaysRequested()
                        }

                        Button {
                            objectName: "loadCurrentPermissionsButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            visible: root.externalChangeWhileEditing
                            text: qsTr("Load current settings")
                            enabled: !root.busy && !root.sharedMutationBusy && !root.saveSubmitted && root.permissionsProjectionAvailable && root.trustedValuesValid
                            onClicked: root.synchronizeDraft()
                        }

                        Button {
                            objectName: "retryApplyPermissionsButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            visible: root.retryApplyAvailable
                            text: root.busyOperation === "compositor-apply" || root.busyOperation === "permissions-apply" ? qsTr("Retrying apply…") : qsTr("Retry apply")
                            enabled: root.retryApplyAvailable && !root.busy && !root.sharedMutationBusy && root.sharedApplySafe
                            onClicked: root.retryApplyRequested()
                        }

                        Button {
                            objectName: "recoverPermissionsButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            visible: root.recoveryAvailable
                            text: qsTr("Restore last working configuration")
                            enabled: root.recoveryAvailable && !root.busy && !root.sharedMutationBusy
                            onClicked: permissionsRecoveryDialog.open()
                        }
                    }
                }
            }

            Frame {
                objectName: "permissionPolicyPreview"
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
                        Layout.preferredHeight: root.compactPage ? 112 : 126
                        radius: 14
                        color: "#201f2c"
                        border.color: "#ab91dc"

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: parent.width - 24
                            spacing: 5

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: root.editingPermission ? root.modeLabel(root.editingPermission.mode).toUpperCase() : qsTr("POLICY")
                                color: root.editingPermission && root.editingPermission.mode === "deny" ? "#ffacb8" : "#c9b4f4"
                                font.pixelSize: 15
                                font.weight: Font.Bold
                            }

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 58
                                Layout.preferredHeight: 47
                                radius: 13
                                color: "#332b47"
                                border.color: "#c9b4f4"

                                Label {
                                    anchors.centerIn: parent
                                    text: root.editingPermission && root.editingPermission.mode === "deny" ? "×" : "✓"
                                    color: root.editingPermission && root.editingPermission.mode === "deny" ? "#ffacb8" : "#b9efcb"
                                    font.pixelSize: 26
                                    font.weight: Font.Bold
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("FIRST MATCH")
                                color: "#a69db9"
                                font.pixelSize: 10
                                font.weight: Font.Bold
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 6

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Security policy, in evaluation order")
                            color: root.palette.text
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }

                        Label {
                            objectName: "permissionRiskExplanation"
                            Layout.fillWidth: true
                            text: root.permissionRisk(root.editingPermission)
                            color: root.editingPermission && root.editingPermission.mode === "allow" && ["plugin", "keyboard", "input-capture"].includes(root.editingPermission.type) ? "#ffb8c3" : root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }

                        Label {
                            objectName: "permissionCodeExample"
                            Layout.fillWidth: true
                            text: root.permissionExample(root.editingPermission)
                            color: "#c9b4f4"
                            font.family: "monospace"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                            Accessible.name: qsTr("Permission configuration example: %1").arg(text)
                        }
                    }
                }
            }

            Frame {
                objectName: "permissionSafetyCard"
                Layout.fillWidth: true
                padding: 14

                background: Rectangle {
                    color: "#30231f"
                    radius: 13
                    border.color: "#8bedad63"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 5

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Restart and enforcement safety")
                        color: "#ffd3a9"
                        font.weight: Font.DemiBold
                        textFormat: Text.PlainText
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Permission changes require a verified Hyprland restart. They are effective only when ecosystem.enforce_permissions is enabled. This editor does not enable enforcement, and the trusted backend performs final RE2 compilation before saving.")
                        color: "#e6c8ad"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
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
                        text: qsTr("Ordered permission rules")
                        color: root.palette.text
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Binary/type pairs are unique. Hyprland uses the first matching rule, so place narrow patterns before broad fallbacks.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }

                Button {
                    objectName: "addPermissionButton"
                    implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                    text: qsTr("Add permission")
                    enabled: root.controlsEnabled && root.draftPermissions.length < root.maximumPermissions
                    Accessible.name: qsTr("Add a permission rule to the draft")
                    onClicked: root.addPermission()
                }
            }

            Label {
                objectName: "emptyPermissionsMessage"
                Layout.fillWidth: true
                visible: root.draftPermissions.length === 0
                text: qsTr("No managed permission rules are saved. This does not enable or disable Hyprland permission enforcement.")
                color: root.palette.placeholderText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Repeater {
                model: Array.isArray(root.draftPermissions) ? root.draftPermissions : []

                delegate: Frame {
                    id: permissionCard

                    required property int index
                    required property var modelData

                    objectName: "permissionCard" + permissionCard.index
                    Layout.fillWidth: true
                    padding: root.compactPage ? 12 : 16

                    background: Rectangle {
                        color: root.palette.base
                        radius: 14
                        border.color: root.editingPermissionId === String(permissionCard.modelData.id) ? root.palette.highlight : root.palette.mid
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
                                    text: String(permissionCard.modelData.binary)
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.family: "monospace"
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.typeLabel(permissionCard.modelData.type)
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                            }

                            Label {
                                text: root.modeLabel(permissionCard.modelData.mode).toUpperCase()
                                color: permissionCard.modelData.mode === "deny" ? "#ffacb8" : permissionCard.modelData.mode === "allow" ? "#b9efcb" : "#ffd3a9"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                textFormat: Text.PlainText
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: childrenRect.height
                            spacing: 8

                            Button {
                                objectName: "editPermissionButton" + permissionCard.index
                                implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                text: qsTr("Edit")
                                enabled: root.controlsEnabled
                                onClicked: root.openPermission(String(permissionCard.modelData.id))
                            }

                            Button {
                                objectName: "movePermissionUpButton" + permissionCard.index
                                implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                text: qsTr("Move up")
                                enabled: root.controlsEnabled && permissionCard.index > 0
                                onClicked: root.movePermission(String(permissionCard.modelData.id), -1)
                            }

                            Button {
                                objectName: "movePermissionDownButton" + permissionCard.index
                                implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                text: qsTr("Move down")
                                enabled: root.controlsEnabled && permissionCard.index + 1 < root.draftPermissions.length
                                onClicked: root.movePermission(String(permissionCard.modelData.id), 1)
                            }

                            Button {
                                objectName: "removePermissionButton" + permissionCard.index
                                implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                text: qsTr("Remove")
                                enabled: root.controlsEnabled
                                onClicked: root.removePermission(String(permissionCard.modelData.id))
                            }
                        }

                        Frame {
                            objectName: "permissionEditor" + permissionCard.index
                            Layout.fillWidth: true
                            visible: root.editingPermissionId === String(permissionCard.modelData.id)
                            padding: root.compactPage ? 12 : 16

                            background: Rectangle {
                                color: "#201f2c"
                                radius: 12
                                border.color: root.editorIssue.length === 0 ? "#8a74b0" : "#a8606a"
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 10

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Binary pattern (RE2)")
                                    color: "#f3edf9"
                                    font.weight: Font.Medium
                                    textFormat: Text.PlainText
                                }

                                TextField {
                                    objectName: "permissionBinaryField" + permissionCard.index
                                    Layout.fillWidth: true
                                    text: String(permissionCard.modelData.binary)
                                    enabled: root.controlsEnabled
                                    maximumLength: root.maximumBinaryLength
                                    selectByMouse: true
                                    placeholderText: qsTr("^/usr/bin/xdg-desktop-portal$")
                                    Accessible.name: qsTr("Permission binary RE2 pattern")

                                    onTextEdited: root.setPermissionField(String(permissionCard.modelData.id), "binary", text)
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Capability")
                                    color: "#f3edf9"
                                    font.weight: Font.Medium
                                    textFormat: Text.PlainText
                                }

                                ComboBox {
                                    objectName: "permissionTypeSelect" + permissionCard.index
                                    Layout.fillWidth: true
                                    implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                    model: root.permissionTypeLabels
                                    currentIndex: root.typeIndex(permissionCard.modelData.type)
                                    enabled: root.controlsEnabled
                                    Accessible.name: qsTr("Hyprland permission capability")

                                    onActivated: index => root.setPermissionField(String(permissionCard.modelData.id), "type", root.permissionTypes[index])
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Decision")
                                    color: "#f3edf9"
                                    font.weight: Font.Medium
                                    textFormat: Text.PlainText
                                }

                                ComboBox {
                                    objectName: "permissionModeSelect" + permissionCard.index
                                    Layout.fillWidth: true
                                    implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                    model: root.permissionModeLabels
                                    currentIndex: root.modeIndex(permissionCard.modelData.mode)
                                    enabled: root.controlsEnabled
                                    Accessible.name: qsTr("Permission decision")

                                    onActivated: index => root.setPermissionField(String(permissionCard.modelData.id), "mode", root.permissionModes[index])
                                }

                                Label {
                                    objectName: "permissionEditorIssue" + permissionCard.index
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
                                    text: root.permissionRisk(permissionCard.modelData)
                                    color: permissionCard.modelData.mode === "allow" && ["plugin", "keyboard", "input-capture"].includes(permissionCard.modelData.type) ? "#ffb8c3" : "#c1b9cc"
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                }

                                Button {
                                    objectName: "doneEditingPermissionButton" + permissionCard.index
                                    implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                                    text: qsTr("Done")
                                    onClicked: root.closeEditor()
                                }
                            }
                        }
                    }
                }
            }

            Frame {
                objectName: "permissionsDraftActionsCard"
                Layout.fillWidth: true
                padding: root.compactPage ? 14 : 18

                background: Rectangle {
                    color: root.palette.base
                    radius: 16
                    border.color: root.draftValid ? root.palette.mid : "#a8606a"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        text: root.draftValid ? qsTr("The complete ordered collection is validated before it replaces desired state.") : qsTr("Finish every rule and remove duplicate binary/type pairs before saving.")
                        color: root.draftValid ? root.palette.placeholderText : "#ffb8c3"
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: childrenRect.height
                        spacing: 10

                        Button {
                            objectName: "discardPermissionsDraftButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            text: qsTr("Discard draft")
                            visible: root.draftDirty && !root.externalChangeWhileEditing
                            enabled: root.discardEnabled
                            onClicked: root.synchronizeDraft()
                        }

                        Button {
                            objectName: "resetPermissionsDefaultsButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            text: qsTr("Reset to defaults")
                            enabled: root.resetEnabled
                            onClicked: root.resetDraftToDefaults()
                        }

                        Button {
                            objectName: "savePermissionsButton"
                            implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                            text: root.busyOperation === "permissions-save" ? qsTr("Saving…") : (root.busyOperation === "compositor-apply" || root.busyOperation === "permissions-apply") ? qsTr("Applying…") : qsTr("Save & restart")
                            highlighted: true
                            enabled: root.saveEnabled
                            Accessible.name: qsTr("Save the validated permission collection")
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
        id: permissionsRecoveryDialog

        objectName: "permissionsRecoveryDialog"
        recoveryAvailable: root.recoveryAvailable
        operationBusy: root.busy || root.sharedMutationBusy
        busyOperation: root.busyOperation
        settingsAreaName: qsTr("Permissions")
        warningObjectName: "permissionsRecoveryWarning"
        cancelObjectName: "cancelPermissionsRecoveryButton"
        confirmObjectName: "confirmPermissionsRecoveryButton"
        minimumTargetSize: root.minimumTargetSize

        onRecoveryRequested: root.recoveryRequested()
    }
}
