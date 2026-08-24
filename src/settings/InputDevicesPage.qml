pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    property bool serviceAvailable: false
    property bool writable: false
    property bool catalogAvailable: false
    property bool inputDevicesAvailable: false
    property bool inputDevicesProjectionAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property var inputDevices: []
    property string revisionToken: "0"
    property string loadState: "unavailable"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property string inputDevicesErrorName: ""
    property string inputDevicesErrorMessage: ""
    property string sharedErrorName: ""
    property string sharedErrorMessage: ""
    property bool retryApplyAvailable: false
    property bool recoveryAvailable: false
    property bool sharedMutationBusy: false
    property bool sharedApplySafe: false
    property real contentTopMargin: 28

    property var draftInputDevices: []
    property var synchronizedInputDevices: []
    property var submittedInputDevices: []
    property string synchronizedRevisionToken: ""
    property string submittedRevisionToken: ""
    property bool projectionInitialized: false
    property bool externalChangeWhileEditing: false
    property bool saveSubmitted: false
    property string editingDeviceId: ""

    signal refreshRequested()
    signal backRequested()
    signal openDisplaysRequested()
    signal saveRequested(var inputDevices)
    signal retryApplyRequested()
    signal recoveryRequested()

    readonly property real minimumTargetSize: 44
    readonly property bool compactPage: root.width < 620
    readonly property int maximumDevices: 256
    readonly property int maximumIdLength: 128
    readonly property int maximumSelectorLength: 256
    readonly property var deviceKinds: [
        "keyboard", "pointer", "touchpad", "touch", "tablet",
        "tabletTool", "switch", "other"
    ]
    readonly property var deviceKindLabels: [
        qsTr("Keyboard"), qsTr("Pointing device"), qsTr("Touchpad"),
        qsTr("Touch device"), qsTr("Drawing tablet"),
        qsTr("Tablet tool"), qsTr("Switch"), qsTr("Other")
    ]
    readonly property var overrideGroups: [
        {
            key: "keyboard",
            title: qsTr("Keyboard and keymap"),
            description: qsTr("Tune the keymap source, repeat timing, Num Lock, keybind symbol resolution, and virtual-keyboard behavior for this exact selector.")
        },
        {
            key: "pointer",
            title: qsTr("Pointer, touchpad, and scrolling"),
            description: qsTr("Set acceleration, scrolling, tapping, handedness, rotation, and click or drag behavior. Unsupported hardware capabilities are ignored by libinput.")
        },
        {
            key: "tablet",
            title: qsTr("Touch and tablet mapping"),
            description: qsTr("Control transforms, mapped regions, active areas, relative motion, and axis reversal for touch and tablet-capable devices.")
        },
        {
            key: "compatibility",
            title: qsTr("Virtual input and compatibility"),
            description: qsTr("Manage per-device keybind participation, shared virtual-keyboard state, release cleanup, and other compatibility switches.")
        }
    ]
    readonly property var overrideDefinitions: [
        {
            key: "kb_file", group: "keyboard", type: "string",
            title: qsTr("Keymap file"),
            description: qsTr("Path to a compiled keymap file. An empty string uses the ordinary XKB fields."),
            defaultValue: "", placeholder: qsTr("/path/to/keymap.xkb")
        },
        {
            key: "kb_layout", group: "keyboard", type: "string",
            title: qsTr("Keyboard layout"),
            description: qsTr("Comma-separated XKB layout names, such as us or us,de."),
            defaultValue: "us", placeholder: qsTr("us")
        },
        {
            key: "kb_variant", group: "keyboard", type: "string",
            title: qsTr("Keyboard variant"),
            description: qsTr("Comma-separated XKB variants aligned with the layout list."),
            defaultValue: "", placeholder: qsTr("intl")
        },
        {
            key: "kb_options", group: "keyboard", type: "string",
            title: qsTr("Keyboard options"),
            description: qsTr("Comma-separated XKB options for this device."),
            defaultValue: "", placeholder: qsTr("caps:escape")
        },
        {
            key: "kb_rules", group: "keyboard", type: "string",
            title: qsTr("Keyboard rules"),
            description: qsTr("Override the XKB rules file name."),
            defaultValue: "", placeholder: qsTr("evdev")
        },
        {
            key: "kb_model", group: "keyboard", type: "string",
            title: qsTr("Keyboard model"),
            description: qsTr("Override the XKB keyboard model."),
            defaultValue: "", placeholder: qsTr("pc105")
        },
        {
            key: "repeat_rate", group: "keyboard", type: "integer",
            title: qsTr("Repeat rate"),
            description: qsTr("Repeated key events per second, from 0 through 200."),
            defaultValue: 25, minimum: 0, maximum: 200
        },
        {
            key: "repeat_delay", group: "keyboard", type: "integer",
            title: qsTr("Repeat delay"),
            description: qsTr("Milliseconds before a held key begins repeating, from 0 through 2000."),
            defaultValue: 600, minimum: 0, maximum: 2000
        },
        {
            key: "numlock_by_default", group: "keyboard", type: "boolean",
            title: qsTr("Num Lock by default"),
            description: qsTr("Request Num Lock when this keyboard is initialized."),
            defaultValue: false
        },
        {
            key: "resolve_binds_by_sym", group: "keyboard",
            type: "boolean", title: qsTr("Resolve keybinds by symbol"),
            description: qsTr("Resolve shortcut keys against the active layout for this device."),
            defaultValue: false
        },
        {
            key: "sensitivity", group: "pointer", type: "number",
            title: qsTr("Pointer sensitivity"),
            description: qsTr("libinput sensitivity from −1 through 1."),
            defaultValue: 0, minimum: -1, maximum: 1
        },
        {
            key: "accel_profile", group: "pointer", type: "enum",
            title: qsTr("Acceleration profile"),
            description: qsTr("Use the device default, adaptive acceleration, or flat unaccelerated motion."),
            values: ["", "adaptive", "flat"],
            labels: [qsTr("Device default"), qsTr("Adaptive"), qsTr("Flat")],
            defaultValue: ""
        },
        {
            key: "rotation", group: "pointer", type: "integer",
            title: qsTr("Device rotation"),
            description: qsTr("Clockwise rotation in degrees, from 0 through 359."),
            defaultValue: 0, minimum: 0, maximum: 359
        },
        {
            key: "natural_scroll", group: "pointer", type: "boolean",
            title: qsTr("Natural scrolling"),
            description: qsTr("Move content in the same direction as the physical gesture."),
            defaultValue: false
        },
        {
            key: "tap_button_map", group: "pointer", type: "enum",
            title: qsTr("Tap button map"),
            description: qsTr("Choose left/right/middle or left/middle/right three-finger tap mapping."),
            values: ["", "lrm", "lmr"],
            labels: [qsTr("Device default"), qsTr("Left · Right · Middle"), qsTr("Left · Middle · Right")],
            defaultValue: ""
        },
        {
            key: "disable_while_typing", group: "pointer",
            type: "boolean", title: qsTr("Disable while typing"),
            description: qsTr("Temporarily suppress touchpad motion while keys are typed."),
            defaultValue: true
        },
        {
            key: "clickfinger_behavior", group: "pointer",
            type: "boolean", title: qsTr("Clickfinger behavior"),
            description: qsTr("Choose physical click actions by the number of fingers on the touchpad."),
            defaultValue: false
        },
        {
            key: "middle_button_emulation", group: "pointer",
            type: "boolean", title: qsTr("Middle-button emulation"),
            description: qsTr("Press left and right together to emulate a middle click where supported."),
            defaultValue: false
        },
        {
            key: "tap_to_click", group: "pointer", type: "boolean",
            title: qsTr("Tap to click"),
            description: qsTr("Turn supported touchpad taps into button clicks."),
            defaultValue: true
        },
        {
            key: "tap_and_drag", group: "pointer", type: "boolean",
            title: qsTr("Tap and drag"),
            description: qsTr("Continue a tap gesture as a pointer drag."),
            defaultValue: true
        },
        {
            key: "drag_lock", group: "pointer", type: "enum",
            title: qsTr("Drag lock"),
            description: qsTr("Control whether tap-and-drag remains held after the finger lifts."),
            values: [0, 1, 2],
            labels: [qsTr("Off"), qsTr("On"), qsTr("Sticky")],
            defaultValue: 0
        },
        {
            key: "left_handed", group: "pointer", type: "boolean",
            title: qsTr("Left-handed buttons"),
            description: qsTr("Swap primary and secondary pointer buttons."),
            defaultValue: false
        },
        {
            key: "scroll_method", group: "pointer", type: "enum",
            title: qsTr("Scroll method"),
            description: qsTr("Select two-finger, edge, button-held, disabled, or device-default scrolling."),
            values: ["", "2fg", "edge", "on_button_down", "no_scroll"],
            labels: [qsTr("Device default"), qsTr("Two-finger"), qsTr("Edge"), qsTr("Hold button"), qsTr("No scrolling")],
            defaultValue: ""
        },
        {
            key: "scroll_button", group: "pointer", type: "integer",
            title: qsTr("Scroll button"),
            description: qsTr("Linux input button code used by the hold-button scroll method, from 0 through 300."),
            defaultValue: 0, minimum: 0, maximum: 300
        },
        {
            key: "scroll_button_lock", group: "pointer",
            type: "boolean", title: qsTr("Scroll-button lock"),
            description: qsTr("Keep button scrolling active after the selected button is released."),
            defaultValue: false
        },
        {
            key: "scroll_factor", group: "pointer", type: "number",
            title: qsTr("Scroll factor"),
            description: qsTr("Scale scroll deltas from 0 through 100."),
            defaultValue: 1, minimum: 0, maximum: 100
        },
        {
            key: "flip_x", group: "pointer", type: "boolean",
            title: qsTr("Reverse horizontal axis"),
            description: qsTr("Reverse horizontal scrolling or motion for compatible devices."),
            defaultValue: false
        },
        {
            key: "flip_y", group: "pointer", type: "boolean",
            title: qsTr("Reverse vertical axis"),
            description: qsTr("Reverse vertical scrolling or motion for compatible devices."),
            defaultValue: false
        },
        {
            key: "drag_3fg", group: "pointer", type: "enum",
            title: qsTr("Three-finger drag"),
            description: qsTr("Control the compatible touchpad three-finger drag mode."),
            values: [0, 1, 2],
            labels: [qsTr("Off"), qsTr("On"), qsTr("Locking")],
            defaultValue: 0
        },
        {
            key: "transform", group: "tablet", type: "enum",
            title: qsTr("Input transform"),
            description: qsTr("Apply the libinput transform code, or −1 for the device default."),
            values: [-1, 0, 1, 2, 3, 4, 5, 6, 7],
            labels: [qsTr("Device default (−1)"), qsTr("Normal"), qsTr("90°"), qsTr("180°"), qsTr("270°"), qsTr("Flipped"), qsTr("Flipped 90°"), qsTr("Flipped 180°"), qsTr("Flipped 270°")],
            defaultValue: -1
        },
        {
            key: "region_position", group: "tablet", type: "vector2",
            title: qsTr("Mapped-region position"),
            description: qsTr("Relative X and Y position of the mapped tablet region."),
            defaultValue: [0, 0]
        },
        {
            key: "absolute_region_position", group: "tablet",
            type: "boolean", title: qsTr("Absolute region position"),
            description: qsTr("Interpret the mapped-region position as absolute compositor coordinates."),
            defaultValue: false
        },
        {
            key: "region_size", group: "tablet", type: "vector2",
            title: qsTr("Mapped-region size"),
            description: qsTr("Width and height of the mapped tablet region."),
            defaultValue: [0, 0]
        },
        {
            key: "relative_input", group: "tablet", type: "boolean",
            title: qsTr("Relative tablet input"),
            description: qsTr("Use tablet deltas instead of absolute pointer placement."),
            defaultValue: false
        },
        {
            key: "active_area_position", group: "tablet", type: "vector2",
            title: qsTr("Active-area position"),
            description: qsTr("Physical X and Y offset of the active tablet area."),
            defaultValue: [0, 0]
        },
        {
            key: "active_area_size", group: "tablet", type: "vector2",
            title: qsTr("Active-area size"),
            description: qsTr("Physical width and height of the active tablet area."),
            defaultValue: [0, 0]
        },
        {
            key: "keybinds", group: "compatibility", type: "boolean",
            title: qsTr("Participate in keybinds"),
            description: qsTr("Allow key events from this device to trigger Hyprland bindings."),
            defaultValue: true
        },
        {
            key: "share_states", group: "compatibility", type: "enum",
            title: qsTr("Share virtual-keyboard states"),
            description: qsTr("Choose whether a virtual keyboard shares lock and modifier state."),
            values: [0, 1, 2],
            labels: [qsTr("Do not share"), qsTr("Share lock state"), qsTr("Share all state")],
            defaultValue: 0
        },
        {
            key: "release_pressed_on_close", group: "compatibility",
            type: "boolean", title: qsTr("Release keys when closed"),
            description: qsTr("Release pressed keys when a virtual keyboard disconnects."),
            defaultValue: true
        }
    ]
    readonly property bool revisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.revisionToken)
    readonly property bool synchronizedRevisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.synchronizedRevisionToken)
    readonly property bool trustedValuesValid:
        root.inputDevicesProjectionAvailable
        && root.validateInputDevicesCollection(root.inputDevices)
    readonly property bool draftValid:
        root.validateInputDevicesCollection(root.draftInputDevices)
    readonly property bool draftDirty:
        root.projectionInitialized
        && !root.valueEqual(
            root.draftInputDevices, root.synchronizedInputDevices
        )
    readonly property bool displayTestActive:
        root.confirmationState !== "idle"
        || root.managementState === "preview"
    readonly property bool controlsEnabled:
        root.serviceAvailable
        && root.writable
        && root.catalogAvailable
        && root.inputDevicesAvailable
        && root.inputDevicesProjectionAvailable
        && root.revisionTokenValid
        && root.trustedValuesValid
        && root.managementState === "managed"
        && root.loadState !== "unsupported"
        && !root.busy
        && !root.saveSubmitted
        && !root.sharedMutationBusy
        && root.sharedApplySafe
        && !root.externalChangeWhileEditing
        && !root.displayTestActive
    readonly property bool saveEnabled:
        root.controlsEnabled && root.draftDirty && root.draftValid
    readonly property bool discardEnabled:
        root.serviceAvailable
        && root.inputDevicesProjectionAvailable
        && root.trustedValuesValid
        && root.projectionInitialized
        && !root.busy
        && !root.saveSubmitted
        && !root.sharedMutationBusy
        && !root.externalChangeWhileEditing
    readonly property bool resetEnabled:
        root.controlsEnabled && root.draftInputDevices.length > 0
    readonly property var editingDevice:
        root.deviceById(root.editingDeviceId)
    readonly property bool editorActive:
        root.editingDeviceId.length > 0 && root.editingDevice !== null
    readonly property string editorIssue:
        root.deviceIssue(root.editingDeviceId)
    readonly property bool statusVisible:
        !root.serviceAvailable
        || !root.writable
        || !root.catalogAvailable
        || !root.inputDevicesAvailable
        || !root.inputDevicesProjectionAvailable
        || !root.revisionTokenValid
        || !root.trustedValuesValid
        || root.loadState === "recovered"
        || root.loadState === "defaulted"
        || root.loadState === "unsupported"
        || root.managementState !== "managed"
        || root.applyState !== "current"
        || root.requiredActivation !== "none"
        || root.displayTestActive
        || root.externalChangeWhileEditing
        || root.inputDevicesErrorMessage.length > 0
        || root.sharedErrorMessage.length > 0
        || root.busy
        || root.sharedMutationBusy
        || !root.sharedApplySafe
    readonly property bool statusIsDanger:
        root.managementState === "conflict"
        || root.applyState === "failed"
        || root.loadState === "unsupported"
        || (root.inputDevicesProjectionAvailable && !root.trustedValuesValid)
        || root.externalChangeWhileEditing
    readonly property string statusMessage: {
        const detail = root.inputDevicesErrorMessage.length > 0
            ? " " + root.inputDevicesErrorMessage : "";
        const sharedDetail = root.sharedErrorMessage.length > 0
            ? " " + root.sharedErrorMessage : "";
        if (!root.serviceAvailable)
            return qsTr("Per-device Input settings are unavailable. The compositor settings service may be restarting.%1").arg(sharedDetail);
        if (root.displayTestActive)
            return qsTr("A display test is active. Device changes stay locked until that test is kept or reverted.");
        if (root.managementState === "unmanaged")
            return qsTr("HyprShelld is not managing the Hyprland entrypoint yet. Review takeover from Displays before changing device records.");
        if (root.managementState === "conflict")
            return qsTr("The managed compositor entrypoint changed unexpectedly. Device changes are locked to preserve it.%1").arg(sharedDetail);
        if (!root.writable)
            return qsTr("This compositor configuration is read-only and has been preserved.");
        if (!root.catalogAvailable)
            return qsTr("The trusted Hyprland 0.56.2 catalog is unavailable or does not match the compositor authority. Device changes are disabled.%1").arg(detail);
        if (!root.inputDevicesAvailable)
            return qsTr("The per-device transaction is unavailable. Existing desired state remains preserved.%1").arg(detail);
        if (!root.inputDevicesProjectionAvailable) {
            return root.inputDevicesErrorMessage.length > 0
                ? qsTr("Per-device authority verification failed. Records cannot be trusted or changed until this check succeeds.%1").arg(detail)
                : qsTr("Per-device settings are waiting for a current, verified full compositor projection.");
        }
        if (!root.trustedValuesValid)
            return qsTr("The current device projection is not an exact managed-v1 collection. No compositor values will be written.%1").arg(detail);
        if (!root.revisionTokenValid)
            return qsTr("The exact compositor revision token is unavailable. Device changes are disabled to prevent overwriting another revision.");
        if (root.externalChangeWhileEditing)
            return qsTr("Compositor settings changed outside this draft. Your complete device draft is preserved, but it cannot be saved over the newer revision. Load the current settings to continue.");
        if (root.busy) {
            if (root.busyOperation === "input-devices-save")
                return qsTr("Saving the validated per-device collection…");
            if (root.busyOperation === "compositor-apply"
                    || root.busyOperation === "input-devices-apply") {
                return qsTr("Applying and verifying the saved compositor revision…");
            }
            if (root.busyOperation === "recover")
                return qsTr("Restoring and verifying the last working compositor configuration…");
            return qsTr("Another compositor operation is in progress. Device changes are temporarily locked.");
        }
        if (root.sharedMutationBusy)
            return qsTr("A shared compositor setting is changing. Device changes remain locked until that transition is verified.");
        if (root.applyState === "retained"
                || root.applyState === "failed"
                || root.applyState === "inactive") {
            if (root.requiredActivation === "restart")
                return qsTr("The device records were saved, but Hyprland must be restarted and verified before they become active. The running input configuration was not partially changed.%1").arg(sharedDetail);
            if (root.requiredActivation === "session")
                return qsTr("The device records were saved, but they require a verified new session before they become active.%1").arg(sharedDetail);
            return root.retryApplyAvailable
                ? qsTr("The desired compositor settings were saved but are not active. Retry the exact revision or restore the last working configuration.%1").arg(sharedDetail)
                : qsTr("The desired compositor state is not active. Review recovery before making another change.%1").arg(sharedDetail);
        }
        if (root.loadState === "recovered")
            return qsTr("Compositor settings were restored from the last known good desired-state copy. Review the device records before changing them.");
        if (root.loadState === "defaulted")
            return qsTr("Compositor settings could not be recovered, so the safe empty device defaults are in use.");
        if (root.loadState === "unsupported")
            return qsTr("The compositor settings use a newer format. They were preserved and remain read-only.");
        if (root.inputDevicesErrorMessage.length > 0)
            return qsTr("The per-device Input operation failed. Your draft was preserved.%1").arg(detail);
        if (root.sharedErrorMessage.length > 0)
            return qsTr("The compositor operation failed.%1").arg(sharedDetail);
        if (!root.sharedApplySafe)
            return qsTr("Device changes are waiting for the shared compositor apply path to become safe.");
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

    function validCanonicalText(value, maximumLength, allowEmpty) {
        if (typeof value !== "string"
                || value.length > maximumLength
                || (!allowEmpty && value.length === 0)
                || /[\u0000-\u001f\u007f-\u009f\u200b-\u200f\u202a-\u202e\u2060-\u206f\ufeff]/.test(value)) {
            return false;
        }
        return typeof value.normalize !== "function"
            || value === value.normalize("NFC");
    }

    function validStableId(value) {
        return typeof value === "string"
            && value.length >= 1 && value.length <= root.maximumIdLength
            && /^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/.test(value);
    }

    function overrideDefinition(key) {
        return root.overrideDefinitions.find(item => item.key === key) || null;
    }

    function overrideValueValid(definition, value) {
        if (!definition)
            return false;
        if (definition.type === "boolean")
            return typeof value === "boolean";
        if (definition.type === "string")
            return root.validCanonicalText(value, 256, true);
        if (definition.type === "enum")
            return Array.isArray(definition.values)
                && definition.values.includes(value);
        if (definition.type === "integer") {
            return typeof value === "number" && Number.isFinite(value)
                && Number.isInteger(value)
                && value >= definition.minimum
                && value <= definition.maximum;
        }
        if (definition.type === "number") {
            return typeof value === "number" && Number.isFinite(value)
                && value >= definition.minimum
                && value <= definition.maximum;
        }
        if (definition.type === "vector2") {
            return Array.isArray(value) && value.length === 2
                && value.every(item => typeof item === "number"
                    && Number.isFinite(item));
        }
        return false;
    }

    function validateOverrides(overrides) {
        if (!overrides || typeof overrides !== "object"
                || Array.isArray(overrides)) {
            return false;
        }
        for (const key of Object.keys(overrides)) {
            const definition = root.overrideDefinition(key);
            if (!root.overrideValueValid(definition, overrides[key]))
                return false;
        }
        return true;
    }

    function validateInputDeviceRecord(record) {
        return root.hasExactKeys(
                record, ["id", "selector", "kind", "enabled", "overrides"]
            )
            && root.validStableId(record.id)
            && root.validCanonicalText(
                record.selector, root.maximumSelectorLength, false
            )
            && root.deviceKinds.includes(record.kind)
            && typeof record.enabled === "boolean"
            && root.validateOverrides(record.overrides);
    }

    function naturalSelectorIdentity(selector) {
        return String(selector).replace(/ /g, "-");
    }

    function validateInputDevicesCollection(records) {
        if (!Array.isArray(records) || records.length > root.maximumDevices)
            return false;
        const ids = new Set();
        const selectors = new Set();
        for (const record of records) {
            if (!root.validateInputDeviceRecord(record))
                return false;
            const selector = root.naturalSelectorIdentity(record.selector);
            if (ids.has(record.id) || selectors.has(selector))
                return false;
            ids.add(record.id);
            selectors.add(selector);
        }
        return true;
    }

    function deviceIndex(id) {
        if (!Array.isArray(root.draftInputDevices))
            return -1;
        return root.draftInputDevices.findIndex(
            record => record && record.id === id
        );
    }

    function deviceById(id) {
        const index = root.deviceIndex(id);
        return index >= 0 ? root.draftInputDevices[index] : null;
    }

    function invalidOverrideKeysFor(record) {
        if (!record || !record.overrides
                || typeof record.overrides !== "object"
                || Array.isArray(record.overrides)) {
            return [];
        }
        return Object.keys(record.overrides).filter(key =>
            !root.overrideValueValid(
                root.overrideDefinition(key), record.overrides[key]
            )
        );
    }

    function deviceIssue(id) {
        const record = root.deviceById(id);
        if (!record)
            return "";
        if (!root.validStableId(record.id))
            return qsTr("The device record identity is not a canonical stable ID.");
        if (!root.validCanonicalText(
                record.selector, root.maximumSelectorLength, false)) {
            return qsTr("Enter a non-empty canonical Hyprland selector of at most 256 characters without control or formatting characters.");
        }
        if (!root.deviceKinds.includes(record.kind))
            return qsTr("Choose a supported pinned device category.");
        if (typeof record.enabled !== "boolean")
            return qsTr("The device enabled state must be true or false.");
        if (!record.overrides || typeof record.overrides !== "object"
                || Array.isArray(record.overrides)) {
            return qsTr("Device overrides must be a typed object.");
        }
        const invalid = root.invalidOverrideKeysFor(record);
        if (invalid.length > 0)
            return qsTr("Fix the invalid or unsupported override: %1.").arg(invalid[0]);
        for (const candidate of root.draftInputDevices) {
            if (!candidate || candidate === record)
                continue;
            if (candidate.id === record.id)
                return qsTr("Every managed device record ID must be unique.");
            if (root.naturalSelectorIdentity(candidate.selector)
                    === root.naturalSelectorIdentity(record.selector)) {
                return qsTr("Every Hyprland device selector must be unique after spaces are normalized to hyphens.");
            }
        }
        return "";
    }

    function nextRecordIdentity(prefix) {
        const used = new Set();
        for (const record of root.draftInputDevices) {
            if (record && typeof record.id === "string")
                used.add(record.id);
        }
        for (let suffix = 1; suffix <= root.maximumDevices + 1; ++suffix) {
            const candidate = prefix + suffix;
            if (!used.has(candidate))
                return candidate;
        }
        return "";
    }

    function nextSelector() {
        const used = new Set();
        for (const record of root.draftInputDevices) {
            if (record && typeof record.selector === "string") {
                used.add(root.naturalSelectorIdentity(record.selector));
            }
        }
        for (let suffix = 1; suffix <= root.maximumDevices + 1; ++suffix) {
            const candidate = "new-device-" + suffix;
            if (!used.has(candidate))
                return candidate;
        }
        return "new-device";
    }

    function addInputDevice() {
        if (!root.controlsEnabled
                || root.draftInputDevices.length >= root.maximumDevices)
            return;
        const records = root.clone(root.draftInputDevices);
        const id = root.nextRecordIdentity("device-");
        if (!records || id.length === 0)
            return;
        records.push({
            id: id,
            selector: root.nextSelector(),
            kind: "other",
            enabled: true,
            overrides: {}
        });
        root.draftInputDevices = records;
        root.editingDeviceId = id;
    }

    function setInputDeviceField(id, field, value) {
        if (!root.controlsEnabled
                || !["selector", "kind", "enabled"].includes(field))
            return;
        const records = root.clone(root.draftInputDevices);
        const index = root.deviceIndex(id);
        if (!records || index < 0)
            return;
        records[index][field] = value;
        root.draftInputDevices = records;
    }

    function setInputDeviceOverride(id, key, included, value) {
        if (!root.controlsEnabled || !root.overrideDefinition(key))
            return;
        const records = root.clone(root.draftInputDevices);
        const index = root.deviceIndex(id);
        if (!records || index < 0)
            return;
        const overrides = records[index].overrides;
        if (!overrides || typeof overrides !== "object"
                || Array.isArray(overrides))
            return;
        if (included)
            overrides[key] = root.clone(value);
        else
            delete overrides[key];
        root.draftInputDevices = records;
    }

    function moveInputDevice(id, offset) {
        if (!root.controlsEnabled || (offset !== -1 && offset !== 1))
            return;
        const records = root.clone(root.draftInputDevices);
        const index = root.deviceIndex(id);
        const target = index + offset;
        if (!records || index < 0 || target < 0 || target >= records.length)
            return;
        const record = records[index];
        records[index] = records[target];
        records[target] = record;
        root.draftInputDevices = records;
    }

    function removeInputDevice(id) {
        if (!root.controlsEnabled)
            return;
        const records = root.clone(root.draftInputDevices);
        const index = root.deviceIndex(id);
        if (!records || index < 0)
            return;
        records.splice(index, 1);
        root.draftInputDevices = records;
        if (root.editingDeviceId === id)
            root.editingDeviceId = "";
    }

    function openInputDevice(id) {
        if (root.controlsEnabled && root.deviceIndex(id) >= 0)
            root.editingDeviceId = id;
    }

    function closeEditor() {
        root.editingDeviceId = "";
    }

    function resetDraftToDefaults() {
        if (!root.controlsEnabled)
            return;
        root.draftInputDevices = [];
        root.closeEditor();
    }

    function synchronizeDraft() {
        if (!root.serviceAvailable || !root.inputDevicesProjectionAvailable
                || !root.revisionTokenValid || !root.trustedValuesValid
                || root.busy || root.sharedMutationBusy)
            return;
        const records = root.clone(root.inputDevices);
        if (!records)
            return;
        root.synchronizedInputDevices = root.clone(records);
        root.draftInputDevices = records;
        root.synchronizedRevisionToken = root.revisionToken;
        root.projectionInitialized = true;
        root.externalChangeWhileEditing = false;
        root.saveSubmitted = false;
        root.submittedInputDevices = [];
        root.submittedRevisionToken = "";
        root.closeEditor();
    }

    function submitDraft() {
        if (!root.saveEnabled)
            return;
        const records = root.clone(root.draftInputDevices);
        if (!records)
            return;
        root.saveSubmitted = true;
        root.submittedInputDevices = root.clone(records);
        root.submittedRevisionToken = root.revisionToken;
        root.saveRequested(records);
        root.scheduleProjectionReview();
    }

    function reviewProjection() {
        if (!root.serviceAvailable || !root.trustedValuesValid) {
            if (root.serviceAvailable && !root.saveSubmitted
                    && root.projectionInitialized && root.draftDirty
                    && root.revisionTokenValid
                    && root.synchronizedRevisionTokenValid
                    && root.revisionToken !== root.synchronizedRevisionToken) {
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
            if (root.valueEqual(root.inputDevices, root.submittedInputDevices)) {
                root.synchronizeDraft();
                return;
            }
            if (root.revisionToken === root.submittedRevisionToken) {
                root.saveSubmitted = false;
                root.submittedInputDevices = [];
                root.submittedRevisionToken = "";
                return;
            }
            root.saveSubmitted = false;
            root.externalChangeWhileEditing = true;
            return;
        }
        const projectionChanged = root.revisionToken
                !== root.synchronizedRevisionToken
            || !root.valueEqual(
                root.inputDevices, root.synchronizedInputDevices
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

    function kindLabel(kind) {
        const index = root.deviceKinds.indexOf(kind);
        return index >= 0 ? root.deviceKindLabels[index] : String(kind);
    }

    function luaQuoted(value) {
        return "\"" + String(value)
            .replace(/\\/g, "\\\\")
            .replace(/\"/g, "\\\"")
            .replace(/\n/g, "\\n")
            .replace(/\r/g, "\\r") + "\"";
    }

    function luaValue(value) {
        if (typeof value === "boolean")
            return value ? "true" : "false";
        if (typeof value === "number")
            return String(value);
        if (typeof value === "string")
            return root.luaQuoted(value);
        if (Array.isArray(value))
            return "{" + value.map(item => root.luaValue(item)).join(", ") + "}";
        return "nil";
    }

    function deviceExample(record) {
        if (!record)
            return "hl.device({name = \"device-name\", enabled = true})";
        const fields = [
            "name = " + root.luaQuoted(record.selector),
            "enabled = " + (record.enabled ? "true" : "false")
        ];
        const overrides = record.overrides && typeof record.overrides === "object"
            ? record.overrides : {};
        for (const key of Object.keys(overrides).sort())
            fields.push(key + " = " + root.luaValue(overrides[key]));
        return "hl.device({" + fields.join(", ") + "})";
    }

    onInputDevicesChanged: root.scheduleProjectionReview()
    onInputDevicesProjectionAvailableChanged: root.scheduleProjectionReview()
    onRevisionTokenChanged: root.scheduleProjectionReview()
    onServiceAvailableChanged: root.scheduleProjectionReview()
    onBusyChanged: root.scheduleProjectionReview()
    onSharedMutationBusyChanged: root.scheduleProjectionReview()
    onInputDevicesErrorNameChanged: root.scheduleProjectionReview()
    onInputDevicesErrorMessageChanged: root.scheduleProjectionReview()
    onSharedErrorNameChanged: root.scheduleProjectionReview()
    onSharedErrorMessageChanged: root.scheduleProjectionReview()
    onApplyStateChanged: root.scheduleProjectionReview()
    Component.onCompleted: root.scheduleProjectionReview()

    background: Rectangle { color: root.palette.window }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.editorActive ? 1 : 0

        ScrollView {
            id: inputDevicesScrollView

            objectName: "inputDevicesScrollView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                objectName: "inputDevicesContent"
                x: Math.max(24, (inputDevicesScrollView.width - width) / 2)
                width: Math.max(
                    0, Math.min(inputDevicesScrollView.width - 48, 980)
                )
                spacing: root.compactPage ? 14 : 18

                Item {
                    Layout.preferredHeight: root.compactPage
                        ? Math.min(root.contentTopMargin, 12)
                        : root.contentTopMargin
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ToolButton {
                        objectName: "inputDevicesBackButton"
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
                            text: qsTr("Input devices")
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
                            text: qsTr("Author the complete ordered Hyprland per-device collection with typed 0.56.2 overrides.")
                            color: root.palette.placeholderText
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }

                    Button {
                        objectName: "refreshInputDevicesButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Refresh")
                        enabled: !root.busy && !root.displayTestActive
                        icon.name: "view-refresh-symbolic"
                        Accessible.name: qsTr("Refresh per-device compositor settings")

                        onClicked: root.refreshRequested()
                    }
                }

                Frame {
                    objectName: "inputDevicesStatusCard"
                    Layout.fillWidth: true
                    visible: root.statusVisible
                    padding: root.compactPage ? 12 : 16

                    background: Rectangle {
                        color: root.statusIsDanger ? "#382125" : "#33251a"
                        radius: 12
                        border.color: root.statusIsDanger
                            ? "#8bfb7185" : "#8bf6ad55"
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        Label {
                            objectName: "inputDevicesStatusMessage"
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
                            spacing: 8

                            Button {
                                objectName: "inputDevicesOpenDisplaysButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                visible: root.serviceAvailable
                                    && root.managementState === "unmanaged"
                                text: qsTr("Review takeover in Displays")
                                enabled: !root.busy

                                onClicked: root.openDisplaysRequested()
                            }

                            Button {
                                objectName: "loadCurrentInputDevicesButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                visible: root.externalChangeWhileEditing
                                text: qsTr("Load current settings")
                                enabled: !root.busy && !root.sharedMutationBusy
                                    && !root.saveSubmitted
                                    && root.inputDevicesProjectionAvailable
                                    && root.trustedValuesValid

                                onClicked: root.synchronizeDraft()
                            }

                            Button {
                                objectName: "retryApplyInputDevicesButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                visible: root.retryApplyAvailable
                                text: root.busyOperation === "compositor-apply"
                                    || root.busyOperation === "input-devices-apply"
                                    ? qsTr("Retrying apply…") : qsTr("Retry apply")
                                enabled: root.retryApplyAvailable && !root.busy
                                    && !root.sharedMutationBusy
                                    && root.sharedApplySafe

                                onClicked: root.retryApplyRequested()
                            }

                            Button {
                                objectName: "recoverInputDevicesButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                visible: root.recoveryAvailable
                                text: qsTr("Restore last working configuration")
                                enabled: root.recoveryAvailable && !root.busy
                                    && !root.sharedMutationBusy

                                onClicked: inputDevicesRecoveryDialog.open()
                            }
                        }
                    }
                }

                Frame {
                    objectName: "inputDevicePipelinePreview"
                    Layout.fillWidth: true
                    padding: root.compactPage ? 14 : 18

                    background: Rectangle {
                        color: "#211e2e"
                        radius: 18
                        border.color: "#665282"
                    }

                    GridLayout {
                        anchors.fill: parent
                        columns: root.compactPage ? 1 : 5
                        rowSpacing: 10
                        columnSpacing: 10

                        Repeater {
                            model: [
                                {
                                    title: qsTr("1 · Selector"),
                                    copy: qsTr("Exact session device name")
                                },
                                {
                                    title: qsTr("2 · Typed overrides"),
                                    copy: qsTr("Only checked fields")
                                },
                                {
                                    title: qsTr("3 · Managed Lua"),
                                    copy: qsTr("One hl.device record")
                                }
                            ]

                            Rectangle {
                                id: pipelineStep

                                required property int index
                                required property var modelData

                                Layout.fillWidth: true
                                Layout.row: root.compactPage ? index : 0
                                Layout.column: root.compactPage ? 0 : index * 2
                                Layout.preferredHeight: 78
                                radius: 12
                                color: index === 2 ? "#493b60" : "#302a41"
                                border.color: index === 2
                                    ? "#b398dd" : "#625578"

                                Column {
                                    anchors.centerIn: parent
                                    width: parent.width - 20
                                    spacing: 3

                                    Label {
                                        width: parent.width
                                        horizontalAlignment: Text.AlignHCenter
                                        text: String(pipelineStep.modelData.title)
                                        color: "#efe6ff"
                                        font.weight: Font.DemiBold
                                        wrapMode: Text.Wrap
                                        textFormat: Text.PlainText
                                    }

                                    Label {
                                        width: parent.width
                                        horizontalAlignment: Text.AlignHCenter
                                        text: String(pipelineStep.modelData.copy)
                                        color: "#c4b6d7"
                                        font.pixelSize: 11
                                        wrapMode: Text.Wrap
                                        textFormat: Text.PlainText
                                    }
                                }
                            }
                        }

                        Repeater {
                            model: root.compactPage ? 0 : 2

                            Label {
                                required property int index

                                Layout.row: 0
                                Layout.column: index === 0 ? 1 : 3
                                text: "→"
                                color: "#c7afea"
                                font.pixelSize: 24
                                Accessible.ignored: true
                            }
                        }
                    }
                }

                Frame {
                    objectName: "inputDeviceRestartSafetyCard"
                    Layout.fillWidth: true
                    padding: root.compactPage ? 14 : 18

                    background: Rectangle {
                        color: "#33251a"
                        radius: 14
                        border.color: "#8bf6ad55"
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 7

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Restart-required input boundary")
                            color: "#ffd3a9"
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Adding, removing, reordering, retargeting, enabling, or changing any override saves a Restart-required desired revision. The editor does not claim stable hardware identity or mutate the running input stack in place.")
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
                            text: qsTr("Ordered managed devices")
                            color: root.palette.text
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Ordering is preserved exactly. Every selector and record ID must be unique.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }

                    Button {
                        objectName: "addInputDeviceButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Add device")
                        enabled: root.controlsEnabled
                            && root.draftInputDevices.length < root.maximumDevices
                        Accessible.name: qsTr("Add a managed input-device record")

                        onClicked: root.addInputDevice()
                    }
                }

                Label {
                    objectName: "emptyInputDevicesMessage"
                    Layout.fillWidth: true
                    visible: root.draftInputDevices.length === 0
                    text: qsTr("No per-device records are saved. Global Input values continue to apply.")
                    color: root.palette.placeholderText
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Repeater {
                    model: Array.isArray(root.draftInputDevices)
                        ? root.draftInputDevices : []

                    delegate: Frame {
                        id: deviceCard

                        required property int index
                        required property var modelData

                        objectName: "inputDeviceCard" + deviceCard.index
                        Layout.fillWidth: true
                        padding: root.compactPage ? 12 : 16

                        background: Rectangle {
                            color: root.palette.base
                            radius: 14
                            border.color: root.deviceIssue(
                                    String(deviceCard.modelData.id)
                                ).length > 0
                                ? "#a8606a" : root.palette.mid
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 42
                                    Layout.preferredHeight: 42
                                    radius: 12
                                    color: deviceCard.modelData.enabled
                                        ? "#4b3e61" : "#34323a"
                                    border.color: deviceCard.modelData.enabled
                                        ? "#9e83c8" : "#62606a"

                                    Label {
                                        anchors.centerIn: parent
                                        text: root.kindLabel(
                                            deviceCard.modelData.kind
                                        ).slice(0, 1).toUpperCase()
                                        color: "#f0e7ff"
                                        font.pixelSize: 17
                                        font.weight: Font.Bold
                                        textFormat: Text.PlainText
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: String(deviceCard.modelData.selector)
                                        color: root.palette.text
                                        font.family: "monospace"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                        textFormat: Text.PlainText
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.kindLabel(
                                            deviceCard.modelData.kind
                                        ) + " · " + Object.keys(
                                            deviceCard.modelData.overrides
                                        ).length + qsTr(" overrides")
                                        color: root.palette.placeholderText
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        textFormat: Text.PlainText
                                    }
                                }

                                Label {
                                    text: deviceCard.modelData.enabled
                                        ? qsTr("ENABLED") : qsTr("DISABLED")
                                    color: deviceCard.modelData.enabled
                                        ? "#9ce4b5" : "#c3c0ca"
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                    textFormat: Text.PlainText
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.deviceExample(deviceCard.modelData)
                                color: "#baa7d7"
                                font.family: "monospace"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                textFormat: Text.PlainText
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: root.deviceIssue(
                                    String(deviceCard.modelData.id)
                                ).length > 0
                                text: root.deviceIssue(
                                    String(deviceCard.modelData.id)
                                )
                                color: "#ffb8c3"
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
                                    objectName: "editInputDeviceButton"
                                        + deviceCard.index
                                    implicitHeight: Math.max(
                                        root.minimumTargetSize,
                                        implicitBackgroundHeight,
                                        implicitContentHeight + topPadding + bottomPadding
                                    )
                                    text: qsTr("Edit")
                                    enabled: root.controlsEnabled

                                    onClicked: root.openInputDevice(
                                        String(deviceCard.modelData.id)
                                    )
                                }

                                Button {
                                    objectName: "moveInputDeviceUpButton"
                                        + deviceCard.index
                                    implicitHeight: Math.max(
                                        root.minimumTargetSize,
                                        implicitBackgroundHeight,
                                        implicitContentHeight + topPadding + bottomPadding
                                    )
                                    text: qsTr("Move up")
                                    enabled: root.controlsEnabled
                                        && deviceCard.index > 0

                                    onClicked: root.moveInputDevice(
                                        String(deviceCard.modelData.id), -1
                                    )
                                }

                                Button {
                                    objectName: "moveInputDeviceDownButton"
                                        + deviceCard.index
                                    implicitHeight: Math.max(
                                        root.minimumTargetSize,
                                        implicitBackgroundHeight,
                                        implicitContentHeight + topPadding + bottomPadding
                                    )
                                    text: qsTr("Move down")
                                    enabled: root.controlsEnabled
                                        && deviceCard.index + 1
                                            < root.draftInputDevices.length

                                    onClicked: root.moveInputDevice(
                                        String(deviceCard.modelData.id), 1
                                    )
                                }

                                Button {
                                    objectName: "removeInputDeviceButton"
                                        + deviceCard.index
                                    implicitHeight: Math.max(
                                        root.minimumTargetSize,
                                        implicitBackgroundHeight,
                                        implicitContentHeight + topPadding + bottomPadding
                                    )
                                    text: qsTr("Remove")
                                    enabled: root.controlsEnabled

                                    onClicked: root.removeInputDevice(
                                        String(deviceCard.modelData.id)
                                    )
                                }
                            }
                        }
                    }
                }

                Frame {
                    objectName: "inputDevicesDraftActionsCard"
                    Layout.fillWidth: true
                    padding: root.compactPage ? 14 : 18

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.draftValid
                            ? root.palette.mid : "#a8606a"
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        Label {
                            Layout.fillWidth: true
                            text: root.draftValid
                                ? qsTr("All 39 supported override fields and every record are validated before the complete ordered collection replaces desired state.")
                                : qsTr("Finish every device, remove duplicate selectors, and fix invalid overrides before saving.")
                            color: root.draftValid
                                ? root.palette.placeholderText : "#ffb8c3"
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: childrenRect.height
                            spacing: 10

                            Button {
                                objectName: "discardInputDevicesDraftButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                text: qsTr("Discard draft")
                                visible: root.draftDirty
                                    && !root.externalChangeWhileEditing
                                enabled: root.discardEnabled

                                onClicked: root.synchronizeDraft()
                            }

                            Button {
                                objectName: "resetInputDevicesDefaultsButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                text: qsTr("Reset to defaults")
                                enabled: root.resetEnabled

                                onClicked: root.resetDraftToDefaults()
                            }

                            Button {
                                objectName: "saveInputDevicesButton"
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                text: root.busyOperation === "input-devices-save"
                                    ? qsTr("Saving…")
                                    : (root.busyOperation === "compositor-apply"
                                        || root.busyOperation === "input-devices-apply")
                                        ? qsTr("Applying…")
                                        : qsTr("Save for restart")
                                highlighted: true
                                enabled: root.saveEnabled
                                Accessible.name: qsTr("Save the validated per-device collection")

                                onClicked: root.submitDraft()
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 12 }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            InputDeviceEditor {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                anchors.topMargin: root.compactPage
                    ? Math.min(root.contentTopMargin, 12)
                    : root.contentTopMargin
                device: root.editingDevice
                overrideDefinitions: root.overrideDefinitions
                groups: root.overrideGroups
                invalidOverrideKeys: root.invalidOverrideKeysFor(
                    root.editingDevice
                )
                controlsEnabled: root.controlsEnabled
                deviceIssue: root.editorIssue
                minimumTargetSize: root.minimumTargetSize
                compact: root.compactPage

                onCloseRequested: root.closeEditor()
                onRemoveRequested: id => root.removeInputDevice(id)
                onPropertyModified: (id, propertyName, value) =>
                    root.setInputDeviceField(id, propertyName, value)
                onOverrideModified: (id, key, included, value) =>
                    root.setInputDeviceOverride(id, key, included, value)
            }
        }
    }

    CompositorRecoveryDialog {
        id: inputDevicesRecoveryDialog

        objectName: "inputDevicesRecoveryDialog"
        recoveryAvailable: root.recoveryAvailable
        operationBusy: root.busy || root.sharedMutationBusy
        busyOperation: root.busyOperation
        settingsAreaName: qsTr("Input devices")
        warningObjectName: "inputDevicesRecoveryWarning"
        cancelObjectName: "cancelInputDevicesRecoveryButton"
        confirmObjectName: "confirmInputDevicesRecoveryButton"
        minimumTargetSize: root.minimumTargetSize

        onRecoveryRequested: root.recoveryRequested()
    }
}
