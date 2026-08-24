pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    property bool serviceAvailable: false
    property bool writable: false
    property bool busy: false
    property bool sharedMutationBusy: false
    property bool sharedApplySafe: false
    property var snapshot: ({})
    property var connectedDisplays: []
    property string topologyDigest: ""
    property double observedAtMs: 0
    property double revision: 0
    property double appliedRevision: 0
    property string loadState: "unavailable"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property double confirmationRevision: 0
    property double confirmationDeadlineMs: 0
    property string confirmationGeneration: ""
    property bool confirmationOwned: false
    property string errorName: ""
    property string errorMessage: ""
    property real contentTopMargin: 28

    property var draftOutputs: []
    property var synchronizedOutputs: []
    property var synchronizedMonitorRecords: []
    property var offlineRecords: []
    property string selectedOutputId: ""
    property string synchronizedTopologyDigest: ""
    property bool draftDirty: false
    property bool inventoryChangedWhileEditing: false
    property double countdownNowMs: Date.now()
    property string previousConfirmationState: "idle"

    readonly property real minimumTargetSize: 44

    signal refreshRequested
    signal adoptionRequested
    signal applyRequested
    signal previewRequested(var outputs, int timeoutSeconds)
    signal confirmRequested
    signal revertRequested

    readonly property bool baselineCurrent: root.managementState === "managed" && root.applyState === "current" && root.appliedRevision === root.revision && root.requiredActivation === "none"
    readonly property bool confirmationActive: root.confirmationState !== "idle" || (root.serviceAvailable && root.managementState === "preview")
    readonly property bool adoptionAvailable: root.serviceAvailable && root.writable && root.managementState === "unmanaged"
    readonly property bool adoptionEligible: root.adoptionAvailable && !root.busy && !root.sharedMutationBusy && root.sharedApplySafe && !root.confirmationActive
    readonly property bool controlsEnabled: root.serviceAvailable && root.writable && !root.busy && !root.sharedMutationBusy && root.sharedApplySafe && root.baselineCurrent && !root.confirmationActive && root.connectedDisplays.length > 0
    readonly property bool anyDraftEnabled: {
        if (!Array.isArray(root.draftOutputs))
            return false;
        for (const output of root.draftOutputs) {
            if (output && output.enabled === true)
                return true;
        }
        return false;
    }
    readonly property string draftValidationMessage: root.validateDraftGraph()
    readonly property bool previewEnabled: root.controlsEnabled && root.draftDirty && root.draftValidationMessage.length === 0 && root.draftOutputs.length === root.connectedDisplays.length && /^[0-9a-f]{64}$/.test(root.topologyDigest)
    readonly property int confirmationSecondsRemaining: Math.max(0, Math.ceil((root.confirmationDeadlineMs - root.countdownNowMs) / 1000))
    readonly property var selectedOutput: root.outputById(root.selectedOutputId)
    readonly property var selectedObservedOutput: root.observedFor(root.selectedOutput ? root.selectedOutput.selector : "")

    function clone(value) {
        try {
            return JSON.parse(JSON.stringify(value));
        } catch (error) {
            return null;
        }
    }

    function outputsEqual(left, right) {
        if (!Array.isArray(left) || !Array.isArray(right))
            return false;
        try {
            return JSON.stringify(left) === JSON.stringify(right);
        } catch (error) {
            return false;
        }
    }

    function observedFor(selector) {
        if (!Array.isArray(root.connectedDisplays))
            return null;
        for (const output of root.connectedDisplays) {
            if (output && output.selector === selector)
                return output;
        }
        return null;
    }

    function outputById(id) {
        if (!Array.isArray(root.draftOutputs))
            return null;
        for (const output of root.draftOutputs) {
            if (output && String(output.id) === id)
                return output;
        }
        return null;
    }

    function staticMonitorSelectorValid(value) {
        if (typeof value !== "string")
            return false;
        if (/^[A-Za-z][A-Za-z0-9_.-]{0,127}$/.test(value))
            return !["current", "left", "right", "up", "down"].includes(value);
        return value.indexOf("desc:") === 0 && value.length >= 6 && value.length <= 261 && root.canonicalStringValid(value.substring(5), 256, false);
    }

    function canonicalStringValid(value, maximumLength, allowEmpty) {
        return typeof value === "string" && (allowEmpty || value.length > 0) && value.length <= maximumLength && value.normalize("NFC") === value && !root.hasDisallowedCharacter(value);
    }

    function hasDisallowedCharacter(value) {
        for (let index = 0; index < value.length; ++index) {
            const code = value.codePointAt(index);
            if (code > 0xffff)
                ++index;
            if (code <= 0x001f || (code >= 0x007f && code <= 0x009f) || code === 0x00ad || (code >= 0x0600 && code <= 0x0605) || code === 0x061c || code === 0x06dd || code === 0x070f || (code >= 0x0890 && code <= 0x0891) || code === 0x08e2 || code === 0x180e || (code >= 0x200b && code <= 0x200f) || (code >= 0x202a && code <= 0x202e) || (code >= 0x2060 && code <= 0x2064) || (code >= 0x2066 && code <= 0x206f) || code === 0xfeff || (code >= 0xfff9 && code <= 0xfffb) || code === 0x110bd || code === 0x110cd || (code >= 0x13430 && code <= 0x1343f) || (code >= 0x1bca0 && code <= 0x1bca3) || (code >= 0x1d173 && code <= 0x1d17a) || code === 0xe0001 || (code >= 0xe0020 && code <= 0xe007f)) {
                return true;
            }
        }
        return false;
    }

    function boundedNumber(value, minimum, maximum) {
        return typeof value === "number" && Number.isFinite(value) && value >= minimum && value <= maximum;
    }

    function boundedInteger(value, minimum, maximum) {
        return Number.isSafeInteger(value) && value >= minimum && value <= maximum;
    }

    function outputValidationMessage(output) {
        const selector = output && typeof output.selector === "string" ? output.selector : qsTr("Unknown display");
        const prefix = qsTr("%1: ").arg(selector);
        if (!output || typeof output !== "object")
            return prefix + qsTr("the monitor record is missing.");
        if (typeof output.id !== "string" || !/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/.test(output.id)) {
            return prefix + qsTr("the stable monitor identity is invalid.");
        }
        if (!root.staticMonitorSelectorValid(output.selector))
            return prefix + qsTr("the output selector is invalid.");
        if (typeof output.enabled !== "boolean")
            return prefix + qsTr("the enabled state is invalid.");
        const automaticModes = ["preferred", "highrr", "highres", "maxwidth"];
        const explicitMode = /^[1-9][0-9]{0,4}x[1-9][0-9]{0,4}(?:@(?:[1-9][0-9]{0,3}(?:\.[0-9]{1,3})?|0\.[0-9]{0,2}[1-9]))?$/;
        if (typeof output.mode !== "string" || (!automaticModes.includes(output.mode) && !explicitMode.test(output.mode))) {
            return prefix + qsTr("choose a supported resolution and refresh rate.");
        }
        const automaticPositions = ["auto", "auto-right", "auto-left", "auto-up", "auto-down", "auto-center-right", "auto-center-left", "auto-center-up", "auto-center-down"];
        const explicitPosition = /^(?:0|[+-]?(?:[1-9][0-9]{0,5}|1000000))x(?:0|[+-]?(?:[1-9][0-9]{0,5}|1000000))$/;
        if (typeof output.position !== "string" || (!automaticPositions.includes(output.position) && !explicitPosition.test(output.position))) {
            return prefix + qsTr("choose a valid automatic or explicit position.");
        }
        if (!(output.scale === "auto" || root.boundedNumber(output.scale, 0.25, 3.4028234663852886e+38))) {
            return prefix + qsTr("enter a display scale of at least 0.25.");
        }
        if (!Array.isArray(output.reserved) || output.reserved.length !== 4 || !output.reserved.every(value => Number.isSafeInteger(value))) {
            return prefix + qsTr("reserved edges must be four whole numbers.");
        }
        if (!root.boundedInteger(output.transform, 0, 7))
            return prefix + qsTr("choose a supported orientation.");
        if (typeof output.mirror !== "string" || (output.mirror.length > 0 && !root.staticMonitorSelectorValid(output.mirror))) {
            return prefix + qsTr("choose a valid mirror target.");
        }
        if (output.bitdepth !== 8 && output.bitdepth !== 10)
            return prefix + qsTr("choose 8-bit or 10-bit output.");
        if (!["auto", "srgb", "wide", "edid", "hdr", "hdredid", "dcip3", "dp3", "adobe"].includes(output.cm)) {
            return prefix + qsTr("choose a supported color-management mode.");
        }
        if (!["default", "auto", "srgb", "gamma22", "gamma22force"].includes(output.sdrEotf)) {
            return prefix + qsTr("choose a supported SDR transfer function.");
        }
        if (!root.boundedNumber(output.sdrBrightness, 0, 10))
            return prefix + qsTr("SDR brightness must be from 0 through 10.");
        if (!root.boundedNumber(output.sdrSaturation, 0, 10))
            return prefix + qsTr("SDR saturation must be from 0 through 10.");
        if (!root.boundedInteger(output.vrr, -1, 3))
            return prefix + qsTr("choose a supported variable-refresh mode.");
        if (!root.canonicalStringValid(output.icc, 256, true)) {
            return prefix + qsTr("the ICC profile path is invalid.");
        }
        if (!root.boundedInteger(output.supportsWideColor, -1, 1) || !root.boundedInteger(output.supportsHdr, -1, 1)) {
            return prefix + qsTr("wide-color and HDR support must be automatic, unsupported, or supported.");
        }
        if (!root.boundedNumber(output.sdrMinLuminance, 0, 10000))
            return prefix + qsTr("SDR minimum luminance is invalid.");
        if (!root.boundedInteger(output.sdrMaxLuminance, -1, 2147483647)) {
            return prefix + qsTr("SDR maximum luminance is invalid.");
        }
        if (!root.boundedNumber(output.minLuminance, -1, 10000))
            return prefix + qsTr("HDR minimum luminance is invalid.");
        if (!root.boundedInteger(output.maxLuminance, -1, 2147483647) || !root.boundedInteger(output.maxAvgLuminance, -1, 2147483647)) {
            return prefix + qsTr("HDR maximum luminance metadata is invalid.");
        }
        return "";
    }

    function validateDraftGraph() {
        if (!Array.isArray(root.draftOutputs) || root.draftOutputs.length === 0)
            return qsTr("No connected display is available to test.");
        const bySelector = Object.create(null);
        let usableOutput = false;
        for (const output of root.draftOutputs) {
            if (!output || typeof output.selector !== "string" || bySelector[output.selector])
                return qsTr("The display draft contains a duplicate output.");
            const fieldIssue = root.outputValidationMessage(output);
            if (fieldIssue.length > 0)
                return fieldIssue;
            bySelector[output.selector] = output;
            usableOutput = usableOutput || (output.enabled === true && !output.mirror);
        }
        if (!usableOutput)
            return qsTr("Keep at least one enabled display that is not a mirror.");
        for (const output of root.draftOutputs) {
            if (!output || !output.mirror)
                continue;
            const target = bySelector[String(output.mirror)];
            if (String(output.mirror) === String(output.selector))
                return qsTr("A display cannot mirror itself.");
            if (!target)
                return qsTr("%1 mirrors a display that is no longer connected.").arg(output.selector);
            if (target.enabled !== true)
                return qsTr("%1 cannot mirror a disabled display.").arg(output.selector);
            if (target.mirror)
                return qsTr("Mirror chains are not supported. Choose a direct display for %1.").arg(output.selector);
        }
        return "";
    }

    function currentMode(observed) {
        if (!observed || !(Number(observed.width) > 0) || !(Number(observed.height) > 0))
            return "preferred";
        const advertisedModes = Array.isArray(observed.modes) ? observed.modes : [];
        for (const mode of advertisedModes) {
            if (mode && Number(mode.width) === Number(observed.width) && Number(mode.height) === Number(observed.height) && Math.abs(Number(mode.refreshRate) - Number(observed.refreshRate)) <= 0.1 && typeof mode.managedMode === "string" && mode.managedMode.length > 0) {
                return mode.managedMode;
            }
        }
        let refresh = Number(observed.refreshRate);
        if (!isFinite(refresh) || refresh <= 0)
            return "%1x%2".arg(observed.width).arg(observed.height);
        let refreshText = refresh.toFixed(3).replace(/0+$/, "").replace(/\.$/, "");
        return "%1x%2@%3".arg(observed.width).arg(observed.height).arg(refreshText);
    }

    function uniqueId(selector, reservedIds) {
        let safe = String(selector || "display").replace(/[^A-Za-z0-9._:-]/g, "-");
        if (safe.length === 0)
            safe = "display";
        let base = ("display-" + safe).slice(0, 118);
        let candidate = base;
        let suffix = 2;
        while (reservedIds[candidate]) {
            candidate = (base.slice(0, 118 - String(suffix).length) + "-" + suffix);
            ++suffix;
        }
        reservedIds[candidate] = true;
        return candidate;
    }

    function defaultRecord(observed, id) {
        const x = observed && isFinite(Number(observed.x)) ? Math.max(-1000000, Math.min(1000000, Math.round(observed.x))) : 0;
        const y = observed && isFinite(Number(observed.y)) ? Math.max(-1000000, Math.min(1000000, Math.round(observed.y))) : 0;
        const scale = observed && Number(observed.scale) >= 0.25 ? Number(observed.scale) : 1;
        const transform = observed && Number.isInteger(Number(observed.transform)) && Number(observed.transform) >= 0 && Number(observed.transform) <= 7 ? Number(observed.transform) : 0;
        const managedColorModes = ["auto", "srgb", "wide", "edid", "hdr", "hdredid", "dcip3", "dp3", "adobe"];
        const observedColorMode = observed ? String(observed.colorManagement || "").toLowerCase() : "";
        const currentFormat = observed ? String(observed.currentFormat || "").toUpperCase() : "";
        function bounded(value, minimum, maximum, fallback) {
            const number = Number(value);
            return isFinite(number) && number >= minimum && number <= maximum ? number : fallback;
        }
        function boundedInteger(value, minimum, maximum, fallback) {
            const number = Number(value);
            return Number.isInteger(number) && number >= minimum && number <= maximum ? number : fallback;
        }
        return {
            id: id,
            selector: String(observed.selector),
            enabled: observed.enabled !== false,
            mode: root.currentMode(observed),
            position: "%1x%2".arg(x).arg(y),
            scale: scale,
            reserved: [0, 0, 0, 0],
            transform: transform,
            mirror: observed && observed.mirrorOf ? String(observed.mirrorOf) : "",
            bitdepth: ["XRGB2101010", "XBGR2101010"].includes(currentFormat) ? 10 : 8,
            cm: managedColorModes.includes(observedColorMode) ? observedColorMode : "auto",
            sdrEotf: "default",
            sdrBrightness: bounded(observed ? observed.sdrBrightness : undefined, 0, 10, 1),
            sdrSaturation: bounded(observed ? observed.sdrSaturation : undefined, 0, 10, 1),
            vrr: -1,
            icc: "",
            supportsWideColor: boundedInteger(observed ? observed.supportsWideColor : undefined, -1, 1, -1),
            supportsHdr: boundedInteger(observed ? observed.supportsHdr : undefined, -1, 1, -1),
            sdrMinLuminance: bounded(observed ? observed.sdrMinLuminance : undefined, 0, 10000, 0.2),
            sdrMaxLuminance: boundedInteger(observed ? observed.sdrMaxLuminance : undefined, -1, 2147483647, 80),
            minLuminance: bounded(observed ? observed.minLuminance : undefined, -1, 10000, -1),
            maxLuminance: boundedInteger(observed ? observed.maxLuminance : undefined, -1, 2147483647, -1),
            maxAvgLuminance: boundedInteger(observed ? observed.maxAvgLuminance : undefined, -1, 2147483647, -1)
        };
    }

    function selectedRecordExisted() {
        const desired = root.snapshot && Array.isArray(root.snapshot.monitors) ? root.snapshot.monitors : [];
        if (!root.selectedOutput)
            return false;
        return desired.some(record => record && record.selector === root.selectedOutput.selector);
    }

    function synchronizeDraft(markInventoryChange) {
        const desired = root.snapshot && Array.isArray(root.snapshot.monitors) ? root.snapshot.monitors : [];
        const observed = Array.isArray(root.connectedDisplays) ? root.connectedDisplays : [];
        const previouslyDirty = root.draftDirty;
        const priorSelection = root.selectedOutputId;
        const ids = {};
        for (const record of desired) {
            if (record && record.id)
                ids[String(record.id)] = true;
        }
        const connectedSelectors = {};
        const online = [];
        for (const display of observed) {
            if (!display || typeof display.selector !== "string" || display.selector.length === 0)
                continue;
            connectedSelectors[display.selector] = true;
            let matched = null;
            // Hyprland applies monitor rules in order, so the last exact
            // connector rule is the effective baseline for this output.
            for (let index = desired.length - 1; index >= 0; --index) {
                const record = desired[index];
                if (record && record.selector === display.selector) {
                    matched = root.clone(record);
                    break;
                }
            }
            if (!matched) {
                matched = root.defaultRecord(display, root.uniqueId(display.selector, ids));
            }
            online.push(matched);
        }
        const offline = [];
        for (const record of desired) {
            if (record && !connectedSelectors[String(record.selector)])
                offline.push(root.clone(record));
        }
        root.draftOutputs = online;
        root.synchronizedOutputs = root.clone(online);
        root.synchronizedMonitorRecords = root.clone(desired);
        root.offlineRecords = offline;
        root.synchronizedTopologyDigest = root.topologyDigest;
        root.draftDirty = false;
        root.inventoryChangedWhileEditing = markInventoryChange ? root.inventoryChangedWhileEditing || previouslyDirty : false;
        root.selectedOutputId = online.some(output => String(output.id) === priorSelection) ? priorSelection : online.length > 0 ? String(online[0].id) : "";
    }

    function reviewSnapshot() {
        const desired = root.snapshot && Array.isArray(root.snapshot.monitors) ? root.snapshot.monitors : [];
        if (root.draftDirty && root.outputsEqual(desired, root.synchronizedMonitorRecords)) {
            root.synchronizedMonitorRecords = root.clone(desired);
            return;
        }
        root.synchronizeDraft(false);
    }

    function normalizeMirrorPositions(outputs) {
        const bySelector = {};
        for (const output of outputs) {
            if (output && typeof output.selector === "string")
                bySelector[String(output.selector)] = output;
        }
        for (const output of outputs) {
            if (!output || !output.mirror)
                continue;
            const target = bySelector[String(output.mirror)];
            if (target && typeof target.position === "string")
                output.position = target.position;
        }
        return outputs;
    }

    function replaceOutput(replacement) {
        if (!replacement || !Array.isArray(root.draftOutputs))
            return;
        const outputs = root.clone(root.draftOutputs);
        for (let index = 0; index < outputs.length; ++index) {
            if (String(outputs[index].id) === String(replacement.id)) {
                outputs[index] = replacement;
                root.normalizeMirrorPositions(outputs);
                root.draftOutputs = outputs;
                root.draftDirty = !root.outputsEqual(outputs, root.synchronizedOutputs);
                root.inventoryChangedWhileEditing = false;
                return;
            }
        }
    }

    function setOutputPosition(id, x, y) {
        const output = root.outputById(id);
        if (!output)
            return;
        const replacement = root.clone(output);
        replacement.position = "%1x%2".arg(Math.max(-1000000, Math.min(1000000, Math.round(x)))).arg(Math.max(-1000000, Math.min(1000000, Math.round(y))));
        root.replaceOutput(replacement);
    }

    function resetOutput(id) {
        const output = root.outputById(id);
        const observed = output ? root.observedFor(output.selector) : null;
        if (!output || !observed)
            return;
        root.replaceOutput(root.defaultRecord(observed, String(output.id)));
    }

    onSnapshotChanged: root.reviewSnapshot()
    onConnectedDisplaysChanged: root.synchronizeDraft(true)
    onTopologyDigestChanged: {
        if (root.topologyDigest !== root.synchronizedTopologyDigest)
            root.synchronizeDraft(true);
    }
    onConfirmationStateChanged: {
        if (root.confirmationState === "idle" && root.previousConfirmationState !== "idle") {
            root.synchronizeDraft(false);
        }
        root.previousConfirmationState = root.confirmationState;
    }

    Component.onCompleted: root.synchronizeDraft(false)

    Timer {
        interval: 250
        repeat: true
        running: root.confirmationState === "awaiting-confirmation"

        onTriggered: root.countdownNowMs = Date.now()
    }

    background: Rectangle {
        color: root.palette.window
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: Math.max(24, (root.width - width) / 2)
            y: root.contentTopMargin
            width: Math.max(0, Math.min(root.width - 48, 980))
            spacing: 20

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Label {
                        text: qsTr("Displays")
                        color: root.palette.text
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Arrange connected outputs and test changes before keeping them.")
                        color: root.palette.placeholderText
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }
                }

                Button {
                    objectName: "refreshDisplaysButton"
                    text: qsTr("Refresh")
                    enabled: !root.busy && !root.confirmationActive
                    icon.name: "view-refresh-symbolic"

                    onClicked: root.refreshRequested()
                }
            }

            Frame {
                objectName: "displayStatusCard"
                Layout.fillWidth: true
                visible: !root.serviceAvailable || !root.writable || root.loadState === "recovered" || root.loadState === "defaulted" || root.managementState !== "managed" || !root.baselineCurrent || root.sharedMutationBusy || !root.sharedApplySafe || root.inventoryChangedWhileEditing || root.errorMessage.length > 0
                padding: 16

                background: Rectangle {
                    color: root.managementState === "conflict" || root.applyState === "failed" || root.confirmationState === "failed" ? "#382125" : "#33251a"
                    radius: 12
                    border.color: root.managementState === "conflict" || root.applyState === "failed" || root.confirmationState === "failed" ? "#8bfb7185" : "#8bf6ad55"
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 14

                    Label {
                        objectName: "displayStatusMessage"
                        Layout.fillWidth: true
                        text: {
                            if (!root.serviceAvailable)
                                return qsTr("Display settings are unavailable. The compositor settings service may be restarting.");
                            if (!root.writable)
                                return qsTr("This compositor configuration is read-only and has been preserved.");
                            if (root.loadState === "recovered")
                                return qsTr("Compositor settings were restored from the last known good copy. Review the display layout before changing it.");
                            if (root.loadState === "defaulted")
                                return qsTr("Compositor settings could not be recovered, so safe defaults are in use. Review them before continuing.");
                            if (root.managementState === "conflict")
                                return qsTr("The managed compositor entrypoint or its ownership state changed unexpectedly. Display changes are locked to preserve it. If the desktop health warning reports Compositor settings, you can restart it there; otherwise preserve the unexpected files and seek recovery guidance.");
                            if (root.managementState === "unmanaged" && root.busy)
                                return qsTr("HyprShelld is preparing to manage your compositor entrypoint. Display changes remain locked until the takeover is verified.");
                            if (root.sharedMutationBusy)
                                return qsTr("Shared visual settings are changing. Display takeover, Apply, and layout testing stay locked until that transition finishes.");
                            if (!root.sharedApplySafe)
                                return qsTr("Shared visual settings are waiting for an exact verified compositor baseline. Display takeover, Apply, and layout testing remain locked.");
                            if (root.errorMessage.length > 0)
                                return qsTr("The display operation failed. %1").arg(root.errorMessage);
                            if (root.managementState === "unmanaged")
                                return qsTr("HyprShelld is not managing your compositor entrypoint yet. Takeover does not import its settings. If hyprland.lua exists, the exact original is preserved privately for recovery when you confirm; if it does not exist, that absence is recorded. Nothing changes until you confirm.");
                            if (!root.baselineCurrent)
                                return qsTr("Other compositor settings are waiting to be applied. Apply that exact baseline before testing a display layout.");
                            if (root.inventoryChangedWhileEditing)
                                return qsTr("The connected display inventory changed. Your draft was refreshed so you can review it safely.");
                            return "";
                        }
                        color: root.managementState === "conflict" || root.applyState === "failed" ? "#ffb8c3" : "#ffd5a1"
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }

                    Button {
                        objectName: "adoptCompositorButton"
                        implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                        visible: root.adoptionAvailable
                        text: root.busy ? qsTr("Starting management…") : qsTr("Take control")
                        enabled: root.adoptionEligible
                        Accessible.name: qsTr("Review compositor management takeover")

                        onClicked: displayAdoptionDialog.open()
                    }

                    Button {
                        objectName: "applyCompositorBaselineButton"
                        visible: root.serviceAvailable && root.writable && root.managementState === "managed" && !root.baselineCurrent
                        text: qsTr("Apply pending changes")
                        enabled: !root.busy && !root.sharedMutationBusy && root.sharedApplySafe && root.confirmationState === "idle"

                        onClicked: root.applyRequested()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true

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
                        objectName: "savedDisplayRulesLabel"
                        visible: root.offlineRecords.length > 0
                        text: root.offlineRecords.length === 1 ? qsTr("1 other saved display rule preserved") : qsTr("%1 other saved display rules preserved").arg(root.offlineRecords.length)
                        color: root.palette.placeholderText
                        font.pixelSize: 11
                    }
                }

                DisplayTopologyPreview {
                    objectName: "displayTopologyPreview"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 310
                    outputs: root.draftOutputs
                    topology: root.connectedDisplays
                    selectedId: root.selectedOutputId
                    interactive: root.controlsEnabled

                    onOutputSelected: id => root.selectedOutputId = id
                    onPositionRequested: (id, x, y) => root.setOutputPosition(id, x, y)
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: root.draftOutputs

                    ItemDelegate {
                        required property var modelData

                        objectName: "displaySelector-" + modelData.id
                        text: modelData.selector
                        checkable: true
                        checked: String(modelData.id) === root.selectedOutputId
                        enabled: !root.confirmationActive
                        Accessible.role: Accessible.PageTab
                        Accessible.name: qsTr("Configure %1").arg(text)
                        Accessible.checked: checked

                        onClicked: root.selectedOutputId = String(modelData.id)
                    }
                }
            }

            DisplaySettingsCard {
                objectName: "displaySettingsCard"
                Layout.fillWidth: true
                output: root.selectedOutput
                observedOutput: root.selectedObservedOutput
                allOutputs: root.draftOutputs
                existingRecord: root.selectedRecordExisted()
                controlsEnabled: root.controlsEnabled

                onOutputRequested: output => root.replaceOutput(output)
                onResetRequested: id => root.resetOutput(id)
            }

            Frame {
                Layout.fillWidth: true
                padding: 18

                background: Rectangle {
                    color: root.palette.base
                    radius: 16
                    border.color: root.previewEnabled ? root.palette.highlight : root.palette.mid
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Label {
                            text: root.draftDirty ? qsTr("Ready to test") : qsTr("No display changes")
                            color: root.palette.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.draftValidationMessage.length > 0 ? root.draftValidationMessage : qsTr("The test lasts 15 seconds. If you cannot see the confirmation, HyprShelld reverts automatically.")
                            color: root.draftValidationMessage.length > 0 ? "#fb7185" : root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }

                    Button {
                        objectName: "discardDisplayDraftButton"
                        text: qsTr("Discard draft")
                        visible: root.draftDirty
                        enabled: !root.busy && !root.confirmationActive

                        onClicked: root.synchronizeDraft(false)
                    }

                    Button {
                        objectName: "previewDisplayConfigurationButton"
                        text: root.busy ? qsTr("Applying…") : qsTr("Test changes")
                        highlighted: true
                        enabled: root.previewEnabled

                        onClicked: root.previewRequested(root.clone(root.draftOutputs), 15)
                    }
                }
            }

            Item {
                Layout.preferredHeight: 12
            }
        }
    }

    DisplayAdoptionDialog {
        id: displayAdoptionDialog

        eligible: root.adoptionEligible
        operationBusy: root.busy || root.sharedMutationBusy

        onAdoptionConfirmed: {
            // Recheck the live projection at the final boundary. The dialog
            // also checks this before emitting, but this keeps the mutation
            // signal safe if the page state changes in the same event turn.
            if (root.adoptionEligible)
                root.adoptionRequested();
        }
    }

    Rectangle {
        id: confirmationScrim

        objectName: "displayConfirmationOverlay"
        anchors.fill: parent
        z: 100
        visible: root.confirmationActive
        color: "#b8101319"

        MouseArea {
            anchors.fill: parent
        }

        Frame {
            anchors.centerIn: parent
            width: Math.min(520, parent.width - 48)
            padding: 24

            background: Rectangle {
                color: root.palette.base
                radius: 18
                border.width: 2
                border.color: root.confirmationState === "failed" ? "#fb7185" : root.palette.highlight
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 16

                Label {
                    objectName: "displayConfirmationTitle"
                    Layout.fillWidth: true
                    text: {
                        if (!root.serviceAvailable)
                            return qsTr("Display confirmation is unavailable");
                        if (root.confirmationState === "reverting")
                            return qsTr("Restoring your displays…");
                        if (root.confirmationState === "committing")
                            return qsTr("Saving your display settings…");
                        if (root.confirmationState === "failed")
                            return qsTr("Display recovery needs attention");
                        if (!root.confirmationOwned)
                            return qsTr("Another display test is active");
                        return qsTr("Keep these display settings?");
                    }
                    color: root.palette.text
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    objectName: "displayConfirmationMessage"
                    Layout.fillWidth: true
                    text: {
                        if (!root.serviceAvailable) {
                            const detail = root.errorMessage.length > 0 ? " " + root.errorMessage : "";
                            return qsTr("This window could not recover the private confirmation capability. The daemon's automatic timeout remains in control.%1").arg(detail);
                        }
                        if (root.confirmationState === "reverting")
                            return qsTr("HyprShelld is returning to the last confirmed layout. Please wait.");
                        if (root.confirmationState === "committing")
                            return qsTr("The display layout is confirmed and is being committed. It can no longer be reverted from this prompt.");
                        if (root.confirmationState === "failed")
                            return qsTr("The automatic revert could not be verified. Avoid changing more compositor settings. If the desktop health warning reports Compositor settings, you can restart it there; otherwise preserve the current state and seek recovery guidance.");
                        if (!root.confirmationOwned)
                            return qsTr("This window does not own the test, so it cannot keep or revert it. The initiating session or the daemon timeout remains in control.");
                        if (root.confirmationSecondsRemaining <= 0)
                            return qsTr("The displayed countdown has elapsed. The daemon rejects late confirmation and reverts automatically.");
                        return root.confirmationSecondsRemaining === 1 ? qsTr("Reverting automatically in 1 second.") : qsTr("Reverting automatically in %1 seconds.").arg(root.confirmationSecondsRemaining);
                    }
                    color: root.confirmationState === "failed" ? "#ffb8c3" : root.palette.placeholderText
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                    Accessible.role: root.confirmationState === "failed" ? Accessible.AlertMessage : Accessible.StaticText
                    Accessible.name: text
                }

                ProgressBar {
                    Layout.fillWidth: true
                    visible: root.confirmationState === "awaiting-confirmation" && root.confirmationOwned
                    from: 0
                    to: 15
                    value: root.confirmationSecondsRemaining
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.confirmationState === "awaiting-confirmation" && root.confirmationOwned
                    spacing: 12

                    Button {
                        objectName: "revertDisplayConfigurationButton"
                        Layout.fillWidth: true
                        text: qsTr("Revert now")
                        enabled: !root.busy

                        onClicked: root.revertRequested()
                    }

                    Button {
                        objectName: "confirmDisplayConfigurationButton"
                        Layout.fillWidth: true
                        text: qsTr("Keep changes")
                        highlighted: true
                        enabled: !root.busy

                        onClicked: root.confirmRequested()
                    }
                }
            }
        }
    }
}
