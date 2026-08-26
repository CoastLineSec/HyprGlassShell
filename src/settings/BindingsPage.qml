pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    property bool serviceAvailable: false
    property bool writable: false
    property bool catalogAvailable: false
    property bool bindingsAvailable: false
    property bool bindingsProjectionAvailable: false
    property bool actionCatalogAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property string revisionToken: "0"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property var defaultBindings: []
    property var bindings: []
    property var submaps: []
    property var bindingActions: []
    property string bindingsErrorName: ""
    property string bindingsErrorMessage: ""
    property string sharedErrorName: ""
    property string sharedErrorMessage: ""
    property bool retryApplyAvailable: false
    property bool recoveryAvailable: false
    property bool sharedMutationBusy: false
    property bool sharedApplySafe: false
    property real contentTopMargin: 28

    property var synchronizedBindings: []
    property var synchronizedSubmaps: []
    property var draftBindings: []
    property var draftSubmaps: []
    property bool projectionInitialized: false
    property bool externalChangeWhileEditing: false
    property string selectedBindingId: ""
    property int currentTab: 0
    property string bindingSearchText: ""
    property int bindingFilterIndex: 0
    property int idCounter: 0

    signal refreshRequested
    signal openDisplaysRequested
    signal saveRequested(var bindings, var submaps)
    signal retryApplyRequested
    signal recoveryRequested
    signal backRequested

    readonly property bool revisionTokenValid: /^(0|[1-9][0-9]*)$/.test(revisionToken)
    readonly property bool displayTestActive: confirmationState !== "idle" || managementState === "preview"
    readonly property bool draftDirty: projectionInitialized && (!root.valuesEqual(draftBindings, synchronizedBindings) || !root.valuesEqual(draftSubmaps, synchronizedSubmaps))
    readonly property string draftIssue: root.validateDraft()
    readonly property bool controlsEnabled: serviceAvailable && writable && catalogAvailable && bindingsAvailable && bindingsProjectionAvailable && actionCatalogAvailable && revisionTokenValid && !busy && !sharedMutationBusy && sharedApplySafe && !externalChangeWhileEditing && !displayTestActive && managementState === "managed"
    readonly property bool saveEnabled: controlsEnabled && draftDirty && draftIssue.length === 0
    readonly property bool compactPage: width < 760
    readonly property var selectedBinding: root.bindingById(selectedBindingId)
    readonly property string selectedBindingOrigin: root.bindingOrigin(root.selectedBinding)
    readonly property bool selectedBindingCanReset: root.isDefaultBinding(root.selectedBinding) && selectedBindingOrigin !== "default"
    readonly property var visibleBindings: root.filteredBindings()
    readonly property bool bindingViewFiltered: bindingSearchText.trim().length > 0 || bindingFilterIndex !== 0

    function listValue(value) {
        if (Array.isArray(value))
            return value.slice();
        if (!value || typeof value.length !== "number")
            return [];
        const result = [];
        for (let index = 0; index < value.length; ++index)
            result.push(value[index]);
        return result;
    }

    function clone(value) {
        try {
            return JSON.parse(JSON.stringify(value));
        } catch (error) {
            return null;
        }
    }

    function valuesEqual(left, right) {
        try {
            return JSON.stringify(root.canonicalValue(left)) === JSON.stringify(root.canonicalValue(right));
        } catch (error) {
            return false;
        }
    }

    function canonicalValue(value) {
        if (Array.isArray(value))
            return value.map(item => root.canonicalValue(item));
        if (value && typeof value === "object") {
            const result = {};
            for (const key of Object.keys(value).sort())
                result[key] = root.canonicalValue(value[key]);
            return result;
        }
        return value;
    }

    function stableId(prefix) {
        root.idCounter += 1;
        return String(prefix) + "-" + String(Date.now()) + "-" + String(root.idCounter);
    }

    function normalizedBindingChord(record) {
        if (!record || typeof record !== "object")
            return "";
        const order = ["super", "ctrl", "alt", "shift", "caps", "mod2", "mod3", "mod5"];
        const selected = new Set(root.listValue(record.modifiers).map(value => String(value).toLowerCase()));
        const modifiers = order.filter(value => selected.has(value));
        return String(record.submap || "") + "|" + modifiers.join("+") + "|" + String(record.key || "").toLowerCase();
    }

    function plainBinding(record) {
        const result = root.clone(record);
        if (!result)
            return null;
        delete result._bindingOrigin;
        delete result._defaultId;
        return result;
    }

    function defaultBindingById(id) {
        return root.listValue(root.defaultBindings).find(record => record && record.id === id) || null;
    }

    function bindingOrigin(record) {
        return record && typeof record._bindingOrigin === "string" ? record._bindingOrigin : "custom";
    }

    function isDefaultBinding(record) {
        return record && typeof record._defaultId === "string" && record._defaultId.length > 0;
    }

    function bindingMatchesDefault(record) {
        if (!root.isDefaultBinding(record))
            return false;
        const baseline = root.defaultBindingById(record._defaultId);
        const authored = root.plainBinding(record);
        return baseline !== null && authored !== null && root.valuesEqual(authored, baseline);
    }

    function effectiveBindings(userBindings) {
        const defaults = root.clone(root.listValue(root.defaultBindings));
        const users = root.clone(root.listValue(userBindings));
        if (!defaults || !users)
            return null;
        const consumed = new Set();
        const result = [];
        for (const baseline of defaults) {
            let index = users.findIndex((record, candidateIndex) => !consumed.has(candidateIndex) && record && record.id === baseline.id);
            if (index < 0) {
                const chord = root.normalizedBindingChord(baseline);
                index = users.findIndex((record, candidateIndex) => !consumed.has(candidateIndex) && root.normalizedBindingChord(record) === chord);
            }
            let effective = root.clone(baseline);
            if (index >= 0) {
                consumed.add(index);
                effective = root.clone(users[index]);
                effective.id = baseline.id;
                effective._bindingOrigin = effective.enabled === false ? "disabled" : "override";
            } else {
                effective._bindingOrigin = "default";
            }
            effective._defaultId = baseline.id;
            result.push(effective);
        }
        for (let index = 0; index < users.length; ++index) {
            if (consumed.has(index))
                continue;
            const custom = root.clone(users[index]);
            custom._bindingOrigin = "custom";
            result.push(custom);
        }
        return result;
    }

    function persistedBindings(records) {
        const result = [];
        for (const record of root.listValue(records)) {
            const authored = root.plainBinding(record);
            if (!authored)
                continue;
            if (root.isDefaultBinding(record)) {
                const baseline = root.defaultBindingById(record._defaultId);
                authored.id = record._defaultId;
                if (baseline !== null && root.valuesEqual(authored, baseline))
                    continue;
            }
            result.push(authored);
        }
        return result;
    }

    function filteredBindings() {
        const query = root.bindingSearchText.trim().toLowerCase();
        return root.listValue(root.draftBindings).filter(record => {
            const origin = root.bindingOrigin(record);
            if (root.bindingFilterIndex === 1 && origin === "default")
                return false;
            if (root.bindingFilterIndex === 2 && !root.isDefaultBinding(record))
                return false;
            if (root.bindingFilterIndex === 3 && root.isDefaultBinding(record))
                return false;
            if (query.length === 0)
                return true;
            const searchable = [
                record.description || "",
                record.key || "",
                record.action || "",
                record.submap || "",
                root.listValue(record.modifiers).join(" "),
                origin
            ].join(" ").toLowerCase();
            return searchable.includes(query);
        });
    }

    function bindingById(id) {
        for (const record of root.listValue(root.draftBindings)) {
            if (record && record.id === id)
                return record;
        }
        return null;
    }

    function bindingIndex(id) {
        return root.listValue(root.draftBindings).findIndex(record => record && record.id === id);
    }

    function submapIndex(id) {
        return root.listValue(root.draftSubmaps).findIndex(record => record && record.id === id);
    }

    function actionTypeOf(action) {
        if (!action || typeof action !== "object")
            return "dispatcher";
        if (typeof action.actionType === "string")
            return action.actionType;
        if (action.kind === "defaultApp" || action.kind === "hyprshelld")
            return action.kind;
        return "dispatcher";
    }

    function actionIdOf(action) {
        return action && typeof action.id === "string" ? action.id : "";
    }

    function actionExists(type, id) {
        return root.listValue(root.bindingActions).some(action => root.actionTypeOf(action) === type && root.actionIdOf(action) === id);
    }

    function defaultAction() {
        const actions = root.listValue(root.bindingActions);
        const preferred = ["window.close", "no_op"];
        for (const id of preferred) {
            const match = actions.find(action => root.actionTypeOf(action) === "dispatcher" && root.actionIdOf(action) === id);
            if (match)
                return match;
        }
        return actions.length > 0 ? actions[0] : null;
    }

    function defaultOptions() {
        return {
            repeating: false,
            locked: false,
            release: false,
            nonConsuming: false,
            autoConsuming: false,
            transparent: false,
            ignoreMods: false,
            dontInhibit: false,
            longPress: false,
            submapUniversal: false,
            click: false,
            drag: false,
            allowInputCapture: false
        };
    }

    function availableNewBindingChord() {
        const used = new Set(root.listValue(root.draftBindings).map(record => root.normalizedBindingChord(record)));
        const modifierCandidates = [
            ["super"],
            ["super", "shift"],
            ["super", "ctrl"],
            ["super", "alt"],
            []
        ];
        const modifierIdentities = new Set(modifierCandidates.map(modifiers => modifiers.join("+")));
        const modifierOrder = ["super", "ctrl", "alt", "shift", "caps", "mod2", "mod3", "mod5"];
        for (let mask = 0; mask < 256; ++mask) {
            const modifiers = [];
            for (let bit = 0; bit < modifierOrder.length; ++bit) {
                if ((mask & (1 << bit)) !== 0)
                    modifiers.push(modifierOrder[bit]);
            }
            const identity = modifiers.join("+");
            if (!modifierIdentities.has(identity)) {
                modifierIdentities.add(identity);
                modifierCandidates.push(modifiers);
            }
        }
        for (const modifiers of modifierCandidates) {
            for (let functionKey = 13; functionKey <= 35; ++functionKey) {
                const key = "F" + String(functionKey);
                const candidate = { modifiers: modifiers, key: key, submap: "" };
                if (!used.has(root.normalizedBindingChord(candidate)))
                    return { modifiers: modifiers.slice(), key: key };
            }
        }
        return null;
    }

    function addBinding() {
        if (!root.controlsEnabled || root.persistedBindings(root.draftBindings).length >= 2048)
            return;
        const action = root.defaultAction();
        const chord = root.availableNewBindingChord();
        if (!action || !chord)
            return;
        const record = {
            id: root.stableId("binding"),
            modifiers: chord.modifiers,
            key: chord.key,
            actionType: root.actionTypeOf(action),
            action: root.actionIdOf(action),
            arguments: {},
            description: qsTr("New shortcut"),
            enabled: true,
            submap: "",
            options: root.defaultOptions(),
            _bindingOrigin: "custom"
        };
        const next = root.clone(root.draftBindings);
        next.push(record);
        root.draftBindings = next;
        root.selectedBindingId = record.id;
        root.currentTab = 0;
    }

    function replaceBinding(record) {
        if (!root.controlsEnabled || !record || typeof record.id !== "string")
            return;
        const index = root.bindingIndex(record.id);
        if (index < 0)
            return;
        const next = root.clone(root.draftBindings);
        const replacement = root.clone(record);
        if (root.isDefaultBinding(replacement)) {
            replacement.id = replacement._defaultId;
            replacement._bindingOrigin = root.bindingMatchesDefault(replacement)
                ? "default" : replacement.enabled === false ? "disabled" : "override";
        }
        next[index] = replacement;
        root.draftBindings = next;
    }

    function removeBinding(id) {
        if (!root.controlsEnabled)
            return;
        const index = root.bindingIndex(id);
        if (index < 0)
            return;
        const next = root.clone(root.draftBindings);
        if (root.isDefaultBinding(next[index])) {
            next[index].enabled = false;
            next[index]._bindingOrigin = "disabled";
            root.draftBindings = next;
            return;
        }
        next.splice(index, 1);
        root.draftBindings = next;
        root.selectedBindingId = next.length > 0 ? next[Math.min(index, next.length - 1)].id : "";
    }

    function resetBinding(id) {
        if (!root.controlsEnabled)
            return;
        const index = root.bindingIndex(id);
        if (index < 0 || !root.isDefaultBinding(root.draftBindings[index]))
            return;
        const baseline = root.clone(root.defaultBindingById(root.draftBindings[index]._defaultId));
        if (!baseline)
            return;
        baseline._bindingOrigin = "default";
        baseline._defaultId = baseline.id;
        const next = root.clone(root.draftBindings);
        next[index] = baseline;
        root.draftBindings = next;
    }

    function canMoveBinding(id) {
        const record = root.bindingById(id);
        return record !== null && !root.isDefaultBinding(record);
    }

    function moveBinding(id, delta) {
        if (!root.controlsEnabled)
            return;
        const index = root.bindingIndex(id);
        const destination = index + delta;
        if (index < 0 || destination < 0 || destination >= root.draftBindings.length)
            return;
        const next = root.clone(root.draftBindings);
        const moved = next.splice(index, 1)[0];
        next.splice(destination, 0, moved);
        root.draftBindings = next;
    }

    function addSubmap() {
        if (!root.controlsEnabled || root.draftSubmaps.length >= 256)
            return;
        const taken = new Set(root.draftSubmaps.map(record => record.name));
        let suffix = 1;
        let name = "mode";
        while (taken.has(name)) {
            suffix += 1;
            name = "mode" + suffix;
        }
        const next = root.clone(root.draftSubmaps);
        next.push({
            id: root.stableId("submap"),
            name: name,
            reset: "",
            enabled: true
        });
        root.draftSubmaps = next;
        root.currentTab = 1;
    }

    function modifySubmap(id, field, value) {
        if (!root.controlsEnabled)
            return;
        const index = root.submapIndex(id);
        if (index < 0)
            return;
        const nextSubmaps = root.clone(root.draftSubmaps);
        const previousName = nextSubmaps[index].name;
        nextSubmaps[index][field] = value;
        let nextBindings = root.draftBindings;
        if (field === "name" && previousName !== value) {
            nextBindings = root.clone(root.draftBindings);
            for (const binding of nextBindings) {
                if (binding.submap === previousName)
                    binding.submap = value;
            }
            for (const submap of nextSubmaps) {
                if (submap.reset === previousName)
                    submap.reset = value;
            }
        }
        root.draftSubmaps = nextSubmaps;
        if (nextBindings !== root.draftBindings)
            root.draftBindings = nextBindings;
    }

    function removeSubmap(id) {
        if (!root.controlsEnabled)
            return;
        const index = root.submapIndex(id);
        if (index < 0)
            return;
        const nextSubmaps = root.clone(root.draftSubmaps);
        const removed = nextSubmaps.splice(index, 1)[0];
        for (const submap of nextSubmaps) {
            if (submap.reset === removed.name)
                submap.reset = "";
        }
        const nextBindings = root.clone(root.draftBindings);
        for (const binding of nextBindings) {
            if (binding.submap === removed.name)
                binding.submap = "";
        }
        root.draftSubmaps = nextSubmaps;
        root.draftBindings = nextBindings;
    }

    function moveSubmap(id, delta) {
        if (!root.controlsEnabled)
            return;
        const index = root.submapIndex(id);
        const destination = index + delta;
        if (index < 0 || destination < 0 || destination >= root.draftSubmaps.length)
            return;
        const next = root.clone(root.draftSubmaps);
        const moved = next.splice(index, 1)[0];
        next.splice(destination, 0, moved);
        root.draftSubmaps = next;
    }

    function synchronizeProjection(force) {
        const nextBindings = root.effectiveBindings(root.bindings);
        const nextSubmaps = root.clone(root.listValue(root.submaps));
        if (!nextBindings || !nextSubmaps)
            return;
        if (!root.projectionInitialized || force) {
            root.synchronizedBindings = root.clone(nextBindings);
            root.synchronizedSubmaps = root.clone(nextSubmaps);
            root.draftBindings = root.clone(nextBindings);
            root.draftSubmaps = root.clone(nextSubmaps);
            root.projectionInitialized = true;
            root.externalChangeWhileEditing = false;
            if (root.bindingIndex(root.selectedBindingId) < 0) {
                root.selectedBindingId = !root.compactPage && root.draftBindings.length > 0 ? root.draftBindings[0].id : "";
            }
            return;
        }
        if (root.valuesEqual(nextBindings, root.synchronizedBindings) && root.valuesEqual(nextSubmaps, root.synchronizedSubmaps)) {
            return;
        }
        if (root.valuesEqual(nextBindings, root.draftBindings) && root.valuesEqual(nextSubmaps, root.draftSubmaps)) {
            root.synchronizedBindings = root.clone(nextBindings);
            root.synchronizedSubmaps = root.clone(nextSubmaps);
            root.externalChangeWhileEditing = false;
            return;
        }
        if (root.draftDirty) {
            root.externalChangeWhileEditing = true;
            return;
        }
        root.synchronizedBindings = root.clone(nextBindings);
        root.synchronizedSubmaps = root.clone(nextSubmaps);
        root.draftBindings = root.clone(nextBindings);
        root.draftSubmaps = root.clone(nextSubmaps);
        root.externalChangeWhileEditing = false;
    }

    function keyValid(key) {
        const value = String(key || "");
        if (value.startsWith("code:")) {
            const code = value.slice(5);
            return /^(?:0|[1-9][0-9]{0,9})$/.test(code) && Number(code) <= 4294967294;
        }
        return ["catchall", "mouse_down", "mouse_up", "mouse_left", "mouse_right"].includes(value) || /^mouse:(?:27[2-9]|2[89][0-9]|[3-6][0-9]{2}|7[0-5][0-9]|76[0-7])$/.test(value) || /^(?!(?:catchall|mouse_down|mouse_up|mouse_left|mouse_right)$)[A-Za-z0-9_]{1,64}$/.test(value);
    }

    function bindingIssue(record, seenIds, seenChords, submapNames) {
        if (!record || typeof record !== "object" || Array.isArray(record))
            return qsTr("A shortcut record is invalid.");
        if (typeof record.id !== "string" || !/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/.test(record.id) || seenIds.has(record.id)) {
            return qsTr("Shortcut IDs must be unique stable identifiers.");
        }
        seenIds.add(record.id);
        const modifiers = root.listValue(record.modifiers);
        const allowedModifiers = ["shift", "caps", "ctrl", "alt", "mod2", "mod3", "super", "mod5"];
        if (modifiers.length > 8 || new Set(modifiers).size !== modifiers.length || modifiers.some(value => !allowedModifiers.includes(value))) {
            return qsTr("%1 has invalid modifiers.").arg(record.id);
        }
        if (!root.keyValid(record.key))
            return qsTr("%1 has an invalid key token.").arg(record.id);
        if (!["dispatcher", "defaultApp", "hyprshelld"].includes(record.actionType) || !root.actionExists(record.actionType, record.action)) {
            return qsTr("%1 does not select a reviewed action.").arg(record.id);
        }
        if (!record.arguments || typeof record.arguments !== "object" || Array.isArray(record.arguments) || Object.keys(record.arguments).length > 16) {
            return qsTr("%1 has invalid action arguments.").arg(record.id);
        }
        if (typeof record.description !== "string" || record.description.length < 1 || record.description.length > 512 || record.description.includes("\0")) {
            return qsTr("%1 needs a description of 1–512 characters.").arg(record.id);
        }
        if (typeof record.enabled !== "boolean" || typeof record.submap !== "string" || record.submap.length > 256 || (record.submap.length > 0 && !submapNames.has(record.submap))) {
            return qsTr("%1 references an invalid submap.").arg(record.id);
        }
        if (record.key === "catchall" && record.submap.length === 0)
            return qsTr("Catch-all shortcuts must belong to a submap.");
        const options = record.options;
        const optionKeys = ["repeating", "locked", "release", "nonConsuming", "autoConsuming", "transparent", "ignoreMods", "dontInhibit", "longPress", "submapUniversal", "click", "drag", "allowInputCapture"];
        if (!options || typeof options !== "object" || Array.isArray(options) || optionKeys.some(key => typeof options[key] !== "boolean")) {
            return qsTr("%1 has incomplete binding behavior.").arg(record.id);
        }
        const actualOptionKeys = Object.keys(options);
        if (actualOptionKeys.some(key => !optionKeys.includes(key) && key !== "device")) {
            return qsTr("%1 has an unknown binding option.").arg(record.id);
        }
        if (options.click && options.drag)
            return qsTr("A shortcut cannot be both click and drag.");
        if ((options.click || options.drag) && !options.release)
            return qsTr("Click and drag shortcuts must trigger on release.");
        if (options.repeating && (options.release || options.click || options.drag || options.longPress)) {
            return qsTr("Repeating conflicts with release, click, drag, or long press.");
        }
        if (options.device !== undefined) {
            const device = options.device;
            if (!device || typeof device !== "object" || Array.isArray(device) || typeof device.inclusive !== "boolean" || !Array.isArray(device.list) || device.list.length > 64 || device.list.length !== new Set(device.list).size || device.list.some(value => typeof value !== "string" || value.length < 1 || value.length > 512)) {
                return qsTr("%1 has an invalid device filter.").arg(record.id);
            }
        }
        const chord = root.normalizedBindingChord(record);
        if (seenChords.has(chord))
            return qsTr("Two shortcuts use the same chord in one submap.");
        seenChords.add(chord);
        return "";
    }

    function validateDraft() {
        if (!root.projectionInitialized)
            return qsTr("Waiting for the shortcut projection.");
        if (root.persistedBindings(root.draftBindings).length > 2048)
            return qsTr("At most 2048 shortcuts can be managed.");
        if (root.draftSubmaps.length > 256)
            return qsTr("At most 256 submaps can be managed.");
        const submapIds = new Set();
        const submapNames = new Set();
        for (const record of root.draftSubmaps) {
            if (!record || typeof record !== "object" || Array.isArray(record) || typeof record.id !== "string" || !/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/.test(record.id) || submapIds.has(record.id)) {
                return qsTr("Submap IDs must be unique stable identifiers.");
            }
            submapIds.add(record.id);
            if (typeof record.name !== "string" || record.name.length < 1 || record.name.length > 256 || submapNames.has(record.name)) {
                return qsTr("Submap names must be non-empty and unique.");
            }
            submapNames.add(record.name);
            if (typeof record.reset !== "string" || record.reset.length > 256 || typeof record.enabled !== "boolean") {
                return qsTr("A submap has an invalid reset target or state.");
            }
        }
        for (const record of root.draftSubmaps) {
            if (record.reset.length > 0 && !submapNames.has(record.reset))
                return qsTr("Submap %1 has an unknown reset target.").arg(record.name);
        }
        const bindingIds = new Set();
        const chords = new Set();
        for (const record of root.draftBindings) {
            const issue = root.bindingIssue(record, bindingIds, chords, submapNames);
            if (issue.length > 0)
                return issue;
        }
        return "";
    }

    function selectedBindingIssue() {
        if (!root.selectedBinding)
            return "";
        const ids = new Set();
        const chords = new Set();
        const names = new Set(root.draftSubmaps.map(record => record.name));
        for (const record of root.draftBindings) {
            const issue = root.bindingIssue(record, ids, chords, names);
            if (record.id === root.selectedBinding.id)
                return issue;
            if (issue.length > 0)
                return "";
        }
        return "";
    }

    function statusMessage() {
        const detail = root.bindingsErrorMessage.length > 0 ? " " + root.bindingsErrorMessage : "";
        if (!root.serviceAvailable)
            return qsTr("Shortcut settings are unavailable.%1").arg(detail);
        if (root.displayTestActive)
            return qsTr("A display test is active. Shortcut changes are locked.");
        if (root.managementState === "unmanaged") {
            return qsTr("HyprShelld is not managing the Hyprland entrypoint yet. Review takeover from Displays before editing shortcuts.");
        }
        if (root.managementState === "conflict")
            return qsTr("The managed compositor entrypoint changed unexpectedly.%1").arg(detail);
        if (!root.writable)
            return qsTr("The current shortcut state is read-only.");
        if (!root.catalogAvailable || !root.actionCatalogAvailable)
            return qsTr("The reviewed option or action catalog is unavailable.%1").arg(detail);
        if (!root.bindingsProjectionAvailable || !root.bindingsAvailable)
            return qsTr("The active bindings and submaps could not be projected safely.%1").arg(detail);
        if (!root.revisionTokenValid)
            return qsTr("The exact desired-state revision is unavailable.");
        if (root.externalChangeWhileEditing)
            return qsTr("Shortcuts changed outside this draft. Load Current before saving.");
        if (root.busy)
            return qsTr("Saving and verifying ordered Lua shortcuts…");
        if (root.applyState !== "current") {
            return root.requiredActivation === "restart" ? qsTr("The saved shortcuts require a compositor restart to activate.") : qsTr("The saved shortcuts are waiting for activation.");
        }
        if (root.sharedErrorMessage.length > 0)
            return root.sharedErrorMessage;
        return "";
    }

    onBindingsChanged: root.synchronizeProjection(false)
    onDefaultBindingsChanged: root.synchronizeProjection(false)
    onSubmapsChanged: root.synchronizeProjection(false)
    Component.onCompleted: root.synchronizeProjection(true)

    background: Rectangle {
        color: "transparent"
    }

    ColumnLayout {
        anchors {
            fill: parent
            topMargin: root.contentTopMargin
            leftMargin: 28
            rightMargin: 28
            bottomMargin: 20
        }
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ToolButton {
                text: "‹"
                font.pixelSize: 28
                implicitWidth: 44
                implicitHeight: 44
                Accessible.name: qsTr("Back to Hyprland categories")
                onClicked: root.backRequested()
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Shortcuts & Submaps")
                    color: root.palette.text
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Build ordered Lua keybindings from reviewed Hyprland dispatchers and HyprShelld actions, then organize modal shortcut layers with submaps.")
                    color: root.palette.placeholderText
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            Rectangle {
                Layout.preferredWidth: 180
                Layout.preferredHeight: 74
                visible: root.width >= 760
                radius: 16
                color: Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.10)
                border.width: 1
                border.color: Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.32)

                Row {
                    anchors.centerIn: parent
                    spacing: 6

                    Repeater {
                        model: ["Super", "K"]
                        delegate: Rectangle {
                            id: headerKey
                            required property string modelData
                            width: headerKeyLabel.implicitWidth + 18
                            height: 34
                            radius: 8
                            color: root.palette.button
                            border.width: 1
                            border.color: root.palette.mid
                            Label {
                                id: headerKeyLabel
                                anchors.centerIn: parent
                                text: headerKey.modelData
                                color: root.palette.buttonText
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "→"
                        color: root.palette.highlight
                        font.pixelSize: 20
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 30
                        height: 30
                        radius: 15
                        color: root.palette.highlight
                        Label {
                            anchors.centerIn: parent
                            text: "✓"
                            color: root.palette.highlightedText
                            font.weight: Font.Bold
                        }
                    }
                }
            }
        }

        Rectangle {
            objectName: "bindingsStatusCard"
            Layout.fillWidth: true
            implicitHeight: statusRow.implicitHeight + 18
            visible: root.statusMessage().length > 0 || root.draftIssue.length > 0
            radius: 12
            color: root.externalChangeWhileEditing || root.draftIssue.length > 0 ? "#4c232c" : "#40351f"
            border.width: 1
            border.color: root.externalChangeWhileEditing || root.draftIssue.length > 0 ? "#b55268" : "#9c7934"

            RowLayout {
                id: statusRow
                anchors {
                    fill: parent
                    margins: 9
                }
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: root.draftIssue.length > 0 && root.projectionInitialized ? root.draftIssue : root.statusMessage()
                    color: root.externalChangeWhileEditing || root.draftIssue.length > 0 ? "#ffb8c3" : "#ffe0a6"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }

                Button {
                    visible: root.externalChangeWhileEditing
                    text: qsTr("Load Current")
                    onClicked: root.synchronizeProjection(true)
                }

                Button {
                    visible: !root.externalChangeWhileEditing && root.statusMessage().length > 0 && !root.busy
                    text: qsTr("Refresh")
                    onClicked: root.refreshRequested()
                }
            }
        }

        TabBar {
            objectName: "bindingsTabBar"
            Layout.fillWidth: true
            currentIndex: root.currentTab
            onCurrentIndexChanged: root.currentTab = currentIndex

            TabButton {
                objectName: "shortcutsTabButton"
                implicitHeight: 44
                text: qsTr("Shortcuts  %1").arg(root.draftBindings.length)
            }
            TabButton {
                objectName: "submapsTabButton"
                implicitHeight: 44
                text: qsTr("Submaps  %1").arg(root.draftSubmaps.length)
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentTab

            Item {
                RowLayout {
                    anchors.fill: parent
                    spacing: 14

                    ColumnLayout {
                        Layout.fillHeight: true
                        Layout.preferredWidth: root.compactPage ? parent.width : Math.min(390, parent.width * 0.43)
                        Layout.maximumWidth: root.compactPage ? parent.width : 430
                        visible: !root.compactPage || root.selectedBindingId.length === 0
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Ordered shortcuts")
                                color: root.palette.text
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }
                            Button {
                                objectName: "addBindingButton"
                                implicitHeight: 44
                                text: qsTr("Add Shortcut")
                                enabled: root.controlsEnabled && root.bindingActions.length > 0 && root.persistedBindings(root.draftBindings).length < 2048 && root.availableNewBindingChord() !== null
                                Accessible.description: enabled ? qsTr("Create a custom shortcut with the first unused safe placeholder chord") : qsTr("No additional placeholder chord is available")
                                onClicked: root.addBinding()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            TextField {
                                objectName: "bindingSearchField"
                                Layout.fillWidth: true
                                implicitHeight: 42
                                text: root.bindingSearchText
                                placeholderText: qsTr("Search shortcuts…")
                                Accessible.name: qsTr("Search shortcuts")
                                onTextChanged: root.bindingSearchText = text
                            }

                            ComboBox {
                                objectName: "bindingOriginFilter"
                                Layout.preferredWidth: 142
                                implicitHeight: 42
                                model: [
                                    qsTr("All"),
                                    qsTr("Changed"),
                                    qsTr("Defaults"),
                                    qsTr("Custom")
                                ]
                                currentIndex: root.bindingFilterIndex
                                Accessible.name: qsTr("Shortcut origin filter")
                                onActivated: index => root.bindingFilterIndex = index
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.bindingViewFiltered
                            text: qsTr("Showing %1 of %2 shortcuts").arg(root.visibleBindings.length).arg(root.draftBindings.length)
                            color: root.palette.placeholderText
                            font.pixelSize: 10
                            textFormat: Text.PlainText
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentWidth: availableWidth
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                width: parent.width
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    visible: root.visibleBindings.length === 0
                                    text: root.draftBindings.length === 0 ? qsTr("No shipped defaults or custom shortcuts are available.") : qsTr("No shortcuts match this search and filter.")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 13
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignHCenter
                                    topPadding: 30
                                }

                                Repeater {
                                    model: root.visibleBindings

                                    delegate: Rectangle {
                                        id: bindingCard
                                        required property int index
                                        required property var modelData
                                        readonly property int draftIndex: root.bindingIndex(modelData.id)
                                        objectName: "bindingCard" + bindingCard.index
                                        activeFocusOnTab: true
                                        Accessible.role: Accessible.Button
                                        Accessible.name: qsTr("%1, %2 plus %3").arg(bindingCard.modelData.description).arg(root.listValue(bindingCard.modelData.modifiers).join(" plus ")).arg(bindingCard.modelData.key)
                                        Accessible.description: root.bindingOrigin(bindingCard.modelData) === "default" ? qsTr("Shipped default shortcut") : root.bindingOrigin(bindingCard.modelData) === "override" ? qsTr("User override shortcut") : root.bindingOrigin(bindingCard.modelData) === "disabled" ? qsTr("Default shortcut disabled by the user") : qsTr("Custom shortcut")
                                        Accessible.onPressAction: root.selectedBindingId = bindingCard.modelData.id
                                        Keys.onReturnPressed: event => {
                                            root.selectedBindingId = bindingCard.modelData.id;
                                            event.accepted = true;
                                        }
                                        Keys.onEnterPressed: event => {
                                            root.selectedBindingId = bindingCard.modelData.id;
                                            event.accepted = true;
                                        }

                                        Layout.fillWidth: true
                                        implicitHeight: cardRow.implicitHeight + 18
                                        radius: 12
                                        color: root.selectedBindingId === modelData.id ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.14) : root.palette.base
                                        border.width: 1
                                        border.color: root.selectedBindingId === modelData.id || bindingCard.activeFocus ? root.palette.highlight : root.palette.mid

                                        RowLayout {
                                            id: cardRow
                                            anchors {
                                                fill: parent
                                                margins: 9
                                            }
                                            spacing: 8

                                            Rectangle {
                                                Layout.preferredWidth: 54
                                                Layout.preferredHeight: 34
                                                radius: 8
                                                color: root.palette.button
                                                border.width: 1
                                                border.color: root.palette.mid
                                                Label {
                                                    anchors.centerIn: parent
                                                    width: parent.width - 8
                                                    text: bindingCard.modelData.key
                                                    color: root.palette.buttonText
                                                    font.pixelSize: 10
                                                    font.weight: Font.DemiBold
                                                    horizontalAlignment: Text.AlignHCenter
                                                    elide: Text.ElideRight
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                spacing: 2
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: bindingCard.modelData.description
                                                    color: root.palette.text
                                                    font.pixelSize: 13
                                                    font.weight: Font.Medium
                                                    elide: Text.ElideRight
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: root.listValue(bindingCard.modelData.modifiers).join(" + ") + (bindingCard.modelData.submap ? " · " + bindingCard.modelData.submap : "")
                                                    color: root.palette.placeholderText
                                                    font.pixelSize: 10
                                                    elide: Text.ElideRight
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: {
                                                        const origin = root.bindingOrigin(bindingCard.modelData);
                                                        if (origin === "default")
                                                            return qsTr("Shipped default");
                                                        if (origin === "override")
                                                            return qsTr("User override");
                                                        if (origin === "disabled")
                                                            return qsTr("Default disabled by user");
                                                        return qsTr("Custom shortcut");
                                                    }
                                                    color: root.bindingOrigin(bindingCard.modelData) === "default" ? root.palette.placeholderText : root.palette.highlight
                                                    font.pixelSize: 9
                                                    font.weight: Font.DemiBold
                                                    elide: Text.ElideRight
                                                }
                                            }

                                            ToolButton {
                                                text: "↑"
                                                enabled: root.controlsEnabled && !root.bindingViewFiltered && root.canMoveBinding(bindingCard.modelData.id) && bindingCard.draftIndex > 0 && root.canMoveBinding(root.draftBindings[bindingCard.draftIndex - 1].id)
                                                Accessible.name: qsTr("Move shortcut earlier")
                                                onClicked: root.moveBinding(bindingCard.modelData.id, -1)
                                            }
                                            ToolButton {
                                                text: "↓"
                                                enabled: root.controlsEnabled && !root.bindingViewFiltered && root.canMoveBinding(bindingCard.modelData.id) && bindingCard.draftIndex < root.draftBindings.length - 1 && root.canMoveBinding(root.draftBindings[bindingCard.draftIndex + 1].id)
                                                Accessible.name: qsTr("Move shortcut later")
                                                onClicked: root.moveBinding(bindingCard.modelData.id, 1)
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            anchors.rightMargin: 82
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.selectedBindingId = bindingCard.modelData.id
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.selectedBinding !== null
                        contentWidth: availableWidth
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        BindingEditor {
                            objectName: "bindingEditor"
                            width: parent.width
                            binding: root.selectedBinding
                            submaps: root.draftSubmaps
                            actions: root.bindingActions
                            controlsEnabled: root.controlsEnabled
                            issue: root.selectedBindingIssue()
                            bindingOrigin: root.selectedBindingOrigin
                            canReset: root.selectedBindingCanReset
                            onRecordModified: record => root.replaceBinding(record)
                            onCloseRequested: {
                                if (root.compactPage)
                                    root.selectedBindingId = "";
                            }
                            onRemoveRequested: id => root.removeBinding(id)
                            onResetRequested: id => root.resetBinding(id)
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: qsTr("Modal shortcut layers")
                                color: root.palette.text
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("A submap activates a named set of shortcuts. Reset returns to global bindings or another reviewed submap.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }
                        }
                        Button {
                            objectName: "addSubmapButton"
                            implicitHeight: 44
                            text: qsTr("Add Submap")
                            enabled: root.controlsEnabled
                            onClicked: root.addSubmap()
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: availableWidth
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        ColumnLayout {
                            width: parent.width
                            spacing: 10

                            Label {
                                Layout.fillWidth: true
                                visible: root.draftSubmaps.length === 0
                                text: qsTr("No submaps yet. Global shortcuts remain active without one.")
                                color: root.palette.placeholderText
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                topPadding: 30
                            }

                            Repeater {
                                model: root.draftSubmaps

                                delegate: Rectangle {
                                    id: submapCard
                                    required property int index
                                    required property var modelData
                                    objectName: "submapCard" + submapCard.index
                                    readonly property var resetNames: [qsTr("Global")].concat(root.draftSubmaps.filter(record => record.id !== submapCard.modelData.id).map(record => record.name))

                                    Layout.fillWidth: true
                                    implicitHeight: submapContent.implicitHeight + 22
                                    radius: 14
                                    color: root.palette.base
                                    border.width: 1
                                    border.color: root.palette.mid

                                    ColumnLayout {
                                        id: submapContent
                                        anchors {
                                            fill: parent
                                            margins: 11
                                        }
                                        spacing: 10

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 10
                                            Rectangle {
                                                Layout.preferredWidth: 42
                                                Layout.preferredHeight: 42
                                                radius: 12
                                                color: Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.14)
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: "⌘"
                                                    color: root.palette.highlight
                                                    font.pixelSize: 20
                                                }
                                            }
                                            TextField {
                                                Layout.fillWidth: true
                                                implicitHeight: 44
                                                text: submapCard.modelData.name
                                                enabled: root.controlsEnabled
                                                placeholderText: qsTr("Submap name")
                                                Accessible.name: qsTr("Submap name")
                                                onEditingFinished: root.modifySubmap(submapCard.modelData.id, "name", text.trim())
                                            }
                                            Switch {
                                                checked: submapCard.modelData.enabled === true
                                                enabled: root.controlsEnabled
                                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                                onClicked: root.modifySubmap(submapCard.modelData.id, "enabled", checked)
                                            }
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 10
                                            Label {
                                                text: qsTr("Reset target")
                                                color: root.palette.text
                                                font.pixelSize: 13
                                                font.weight: Font.Medium
                                            }
                                            ComboBox {
                                                Layout.fillWidth: true
                                                implicitHeight: 44
                                                model: submapCard.resetNames
                                                currentIndex: {
                                                    if (!submapCard.modelData.reset)
                                                        return 0;
                                                    const found = submapCard.resetNames.indexOf(submapCard.modelData.reset);
                                                    return found >= 0 ? found : 0;
                                                }
                                                enabled: root.controlsEnabled
                                                onActivated: index => root.modifySubmap(submapCard.modelData.id, "reset", index <= 0 ? "" : submapCard.resetNames[index])
                                            }
                                            ToolButton {
                                                text: "↑"
                                                enabled: root.controlsEnabled && submapCard.index > 0
                                                onClicked: root.moveSubmap(submapCard.modelData.id, -1)
                                            }
                                            ToolButton {
                                                text: "↓"
                                                enabled: root.controlsEnabled && submapCard.index < root.draftSubmaps.length - 1
                                                onClicked: root.moveSubmap(submapCard.modelData.id, 1)
                                            }
                                            Button {
                                                text: qsTr("Remove")
                                                enabled: root.controlsEnabled
                                                onClicked: root.removeSubmap(submapCard.modelData.id)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: saveRow.implicitHeight + 18
            radius: 14
            color: root.palette.base
            border.width: 1
            border.color: root.palette.mid

            RowLayout {
                id: saveRow
                anchors {
                    fill: parent
                    margins: 9
                }
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: root.draftDirty ? qsTr("Ordered shortcut draft has unsaved changes") : qsTr("Shortcuts match the saved desired state")
                    color: root.draftDirty ? root.palette.text : root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                Button {
                    objectName: "discardBindingsButton"
                    implicitHeight: 44
                    visible: root.draftDirty
                    text: qsTr("Discard")
                    enabled: !root.busy
                    onClicked: root.synchronizeProjection(true)
                }

                Button {
                    objectName: "saveBindingsButton"
                    implicitHeight: 44
                    text: root.busy && root.busyOperation === "bindings-save" ? qsTr("Saving…") : qsTr("Save Shortcuts")
                    highlighted: true
                    enabled: root.saveEnabled
                    onClicked: root.saveRequested(root.persistedBindings(root.draftBindings), root.clone(root.draftSubmaps))
                }
            }
        }
    }
}
