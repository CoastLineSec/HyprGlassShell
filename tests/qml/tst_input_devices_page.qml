import "../../src/settings" as Settings
import QtQuick
import QtQuick.Window
import QtTest

TestCase {
    function json(value) {
        return JSON.stringify(value);
    }

    function minimalDevice(id, selector, kind) {
        return {
            "id": id,
            "selector": selector,
            "kind": kind || "other",
            "enabled": true,
            "overrides": {
            }
        };
    }

    function completeOverrides() {
        return {
            "sensitivity": 0.25,
            "accel_profile": "adaptive",
            "rotation": 90,
            "kb_file": "",
            "kb_layout": "us,de",
            "kb_variant": "intl,",
            "kb_options": "caps:escape",
            "kb_rules": "evdev",
            "kb_model": "pc105",
            "repeat_rate": 30,
            "repeat_delay": 450,
            "natural_scroll": true,
            "tap_button_map": "lrm",
            "numlock_by_default": true,
            "resolve_binds_by_sym": true,
            "disable_while_typing": true,
            "clickfinger_behavior": true,
            "middle_button_emulation": true,
            "tap_to_click": true,
            "tap_and_drag": true,
            "drag_lock": 2,
            "left_handed": true,
            "scroll_method": "on_button_down",
            "scroll_button": 274,
            "scroll_button_lock": true,
            "scroll_factor": 1.25,
            "transform": 6,
            "region_position": [-12.5, 4],
            "absolute_region_position": true,
            "region_size": [1920, 1080],
            "relative_input": true,
            "active_area_position": [2.5, 3.5],
            "active_area_size": [200, 120],
            "flip_x": true,
            "flip_y": false,
            "drag_3fg": 1,
            "keybinds": false,
            "share_states": 2,
            "release_pressed_on_close": true
        };
    }

    function configure(page, records) {
        page.inputDevicesProjectionAvailable = false;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.inputDevicesAvailable = true;
        page.busy = false;
        page.busyOperation = "";
        page.inputDevices = records || [];
        page.revisionToken = "7";
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.inputDevicesErrorName = "";
        page.inputDevicesErrorMessage = "";
        page.sharedErrorName = "";
        page.sharedErrorMessage = "";
        page.retryApplyAvailable = false;
        page.recoveryAvailable = false;
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.inputDevicesProjectionAvailable = true;
        page.reviewProjection();
    }

    function compareMinimumTarget(item, name) {
        verify(item !== null, name + " must exist");
        verify(item.implicitHeight >= 44, name + " must be at least 44px");
    }

    function test_allPinnedDeviceOverridesAreExposedAndTyped() {
        const window = createTemporaryObject(pageComponent, this);
        verify(window !== null);
        const page = window.page;
        const expected = ["sensitivity", "accel_profile", "rotation", "kb_file", "kb_layout", "kb_variant", "kb_options", "kb_rules", "kb_model", "repeat_rate", "repeat_delay", "natural_scroll", "tap_button_map", "numlock_by_default", "resolve_binds_by_sym", "disable_while_typing", "clickfinger_behavior", "middle_button_emulation", "tap_to_click", "tap_and_drag", "drag_lock", "left_handed", "scroll_method", "scroll_button", "scroll_button_lock", "scroll_factor", "transform", "region_position", "absolute_region_position", "region_size", "relative_input", "active_area_position", "active_area_size", "flip_x", "flip_y", "drag_3fg", "keybinds", "share_states", "release_pressed_on_close"].sort();
        const actual = page.overrideDefinitions.map((item) => {
            return item.key;
        }).sort();
        compare(actual.length, 39);
        compare(json(actual), json(expected));
        compare(new Set(actual).size, actual.length);
        for (const definition of page.overrideDefinitions) {
            verify(["keyboard", "pointer", "tablet", "compatibility"].includes(definition.group), definition.key);
            verify(["boolean", "integer", "number", "string", "enum", "vector2"].includes(definition.type), definition.key);
            verify(definition.title.length > 0, definition.key);
            verify(definition.description.length > 0, definition.key);
            verify(Object.prototype.hasOwnProperty.call(definition, "defaultValue"), definition.key);
        }
    }

    function test_exactPinnedShapeRangesAndNaturalIdentity() {
        const window = createTemporaryObject(pageComponent, this);
        verify(window !== null);
        const page = window.page;
        const complete = minimalDevice("device.complete", "Main Keyboard", "keyboard");
        complete.overrides = completeOverrides();
        compare(page.validateInputDeviceRecord(complete), true);
        compare(page.validateInputDevicesCollection([complete]), true);
        compare(Object.keys(complete.overrides).length, 39);
        const extraRecord = JSON.parse(json(complete));
        extraRecord.extra = true;
        compare(page.validateInputDeviceRecord(extraRecord), false);
        const unknownOverride = JSON.parse(json(complete));
        unknownOverride.overrides.unknown = true;
        compare(page.validateInputDeviceRecord(unknownOverride), false);
        const invalidCases = [["sensitivity", 1.01], ["rotation", 360], ["repeat_rate", 201], ["repeat_delay", 2001], ["drag_lock", 3], ["scroll_button", 301], ["scroll_factor", -0.01], ["transform", 8], ["drag_3fg", -1], ["share_states", 3], ["region_position", [1]], ["active_area_size", [1, "2"]], ["accel_profile", "linear"], ["tap_button_map", "rlm"], ["scroll_method", "wheel"]];
        for (const invalid of invalidCases) {
            const record = JSON.parse(json(complete));
            record.overrides[invalid[0]] = invalid[1];
            compare(page.validateInputDeviceRecord(record), false, invalid[0]);
        }
        const control = JSON.parse(json(complete));
        control.selector = "bad\u0000selector";
        compare(page.validateInputDeviceRecord(control), false);
        const decomposed = JSON.parse(json(complete));
        decomposed.selector = "Cafe\u0301";
        compare(page.validateInputDeviceRecord(decomposed), false);
        compare(page.validateInputDevicesCollection([minimalDevice("device.a", "Main Keyboard", "keyboard"), minimalDevice("device.b", "Main-Keyboard", "keyboard")]), false);
        compare(page.validateInputDevicesCollection([minimalDevice("device.same", "one"), minimalDevice("device.same", "two")]), false);
    }

    function test_orderedCrudOverrideRemovalAndAtomicSave() {
        const window = createTemporaryObject(pageComponent, this);
        verify(window !== null);
        const page = window.page;
        configure(page, [minimalDevice("device.a", "Keyboard One", "keyboard"), minimalDevice("device.b", "Mouse One", "pointer")]);
        tryCompare(page, "projectionInitialized", true);
        compare(page.controlsEnabled, true);
        page.moveInputDevice("device.b", -1);
        compare(page.draftInputDevices[0].id, "device.b");
        page.setInputDeviceField("device.b", "selector", "Mouse Primary");
        page.setInputDeviceField("device.b", "kind", "touchpad");
        page.setInputDeviceField("device.b", "enabled", false);
        page.setInputDeviceOverride("device.b", "sensitivity", true, 0.4);
        page.setInputDeviceOverride("device.b", "region_size", true, [1000, 800]);
        compare(page.draftInputDevices[0].selector, "Mouse Primary");
        compare(page.draftInputDevices[0].overrides.sensitivity, 0.4);
        compare(page.draftInputDevices[0].overrides.region_size[1], 800);
        compare(page.deviceExample(page.draftInputDevices[0]), "hl.device({name = \"Mouse Primary\", enabled = false, " + "region_size = {1000, 800}, sensitivity = 0.4})");
        page.setInputDeviceOverride("device.b", "sensitivity", false, undefined);
        compare(Object.prototype.hasOwnProperty.call(page.draftInputDevices[0].overrides, "sensitivity"), false);
        page.addInputDevice();
        verify(/^device-[1-9][0-9]*$/.test(page.editingDeviceId));
        compare(page.draftInputDevices.length, 3);
        compare(page.validateInputDevicesCollection(page.draftInputDevices), true);
        const addedId = page.editingDeviceId;
        page.closeEditor();
        page.removeInputDevice("device.a");
        compare(page.draftInputDevices.length, 2);
        compare(page.deviceIndex(addedId) >= 0, true);
        compare(page.saveEnabled, true);
        page.submitDraft();
        compare(window.saveSpy.count, 1);
        compare(json(window.saveSpy.signalArguments[0][0]), json(page.submittedInputDevices));
        page.saveSubmitted = false;
        page.externalChangeWhileEditing = false;
        page.resetDraftToDefaults();
        compare(page.draftInputDevices.length, 0);
        compare(page.draftValid, true);
        compare(page.saveEnabled, true);
    }

    function test_externalRevisionConflictPreservesDeviceDraft() {
        const window = createTemporaryObject(pageComponent, this);
        verify(window !== null);
        const page = window.page;
        configure(page, [minimalDevice("device.a", "Original Device", "keyboard")]);
        tryCompare(page, "projectionInitialized", true);
        page.setInputDeviceField("device.a", "selector", "Draft Device");
        page.inputDevices = [minimalDevice("device.a", "External Device", "keyboard")];
        page.revisionToken = "8";
        page.reviewProjection();
        compare(page.externalChangeWhileEditing, true);
        compare(page.draftInputDevices[0].selector, "Draft Device");
        compare(page.controlsEnabled, false);
        verify(page.statusMessage.indexOf("outside this draft") >= 0);
        page.synchronizeDraft();
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftInputDevices[0].selector, "External Device");
    }

    function test_visualPreviewEditorsAndAccessibleTargets() {
        const window = createTemporaryObject(pageComponent, this);
        verify(window !== null);
        const page = window.page;
        const record = minimalDevice("device.keyboard", "Main Keyboard", "keyboard");
        record.overrides = {
            "kb_layout": "us",
            "repeat_rate": 30
        };
        configure(page, [record]);
        tryCompare(page, "projectionInitialized", true);
        wait(0);
        for (const name of ["inputDevicePipelinePreview", "inputDeviceRestartSafetyCard", "inputDeviceCard0"]) {
            verify(findChild(page, name) !== null, name);
        }
        for (const name of ["inputDevicesBackButton", "refreshInputDevicesButton", "addInputDeviceButton", "editInputDeviceButton0", "moveInputDeviceDownButton0", "removeInputDeviceButton0", "saveInputDevicesButton"]) {
            compareMinimumTarget(findChild(page, name), name);
        }
        page.openInputDevice("device.keyboard");
        wait(0);
        for (const name of ["inputDeviceVisualPreview", "inputDeviceIdentityCard", "inputDeviceOverrideExplanationCard", "inputDeviceOverridesGroup_keyboard", "inputDeviceOverridesGroup_pointer", "inputDeviceOverridesGroup_tablet", "inputDeviceOverridesGroup_compatibility", "inputDeviceOverrideRow_kb_layout", "inputDeviceOverrideRow_active_area_size"]) {
            verify(findChild(page, name) !== null, name);
        }
        for (const name of ["closeInputDeviceEditorButton", "inputDeviceSelectorField", "inputDeviceKindSelect", "inputDeviceEnabledSwitch", "inputDeviceOverrideToggle_kb_layout", "inputDeviceScalarControl_kb_layout", "doneEditingInputDeviceButton", "removeEditedInputDeviceButton"]) {
            compareMinimumTarget(findChild(page, name), name);
        }
        page.applyState = "retained";
        page.requiredActivation = "restart";
        verify(page.statusMessage.indexOf("restarted") >= 0);
        page.busyOperation = "input-devices-save";
        page.busy = true;
        verify(page.statusMessage.indexOf("Saving") >= 0);
    }

    name: "InputDevicesPage"
    when: windowShown

    Component {
        id: pageComponent

        Window {
            property alias page: inputDevicesPage
            property alias saveSpy: inputDevicesSaveSpy

            width: 900
            height: 1000
            visible: true

            Settings.InputDevicesPage {
                id: inputDevicesPage

                anchors.fill: parent
            }

            SignalSpy {
                id: inputDevicesSaveSpy

                target: inputDevicesPage
                signalName: "saveRequested"
            }

        }

    }

}
