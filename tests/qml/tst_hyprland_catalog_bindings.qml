import QtQuick
import QtQuick.Window
import QtTest
import "../../src/settings" as Settings

TestCase {
    name: "HyprlandCatalogBindings"
    when: windowShown

    Component {
        id: catalogComponent

        Window {
            width: 900
            height: 820
            visible: true
            property alias page: catalogPage
            property alias saveSpy: catalogSaveSpy

            Settings.HyprlandCatalogPage {
                id: catalogPage
                anchors.fill: parent
            }

            SignalSpy {
                id: catalogSaveSpy
                target: catalogPage
                signalName: "saveRequested"
            }
        }
    }

    Component {
        id: bindingsComponent

        Window {
            width: 980
            height: 900
            visible: true
            property alias page: bindingsPage
            property alias saveSpy: bindingsSaveSpy

            Settings.BindingsPage {
                id: bindingsPage
                anchors.fill: parent
            }

            SignalSpy {
                id: bindingsSaveSpy
                target: bindingsPage
                signalName: "saveRequested"
            }
        }
    }

    function option(id, module, type, value, writable) {
        return {
            id: id,
            path: module + ":" + id.split(".").pop(),
            module: module,
            luaPath: [module, id.split(".").pop()],
            type: type,
            defaultPolicy: "hyprland",
            writable: writable === undefined ? true : writable,
            defaultValue: value,
            uiTier: "common",
            control: type === "boolean" ? "toggle" : "text",
            constraints: {},
            applyMode: "reload",
            risk: "safe",
            since: "0.55.0",
            description: "Fixture option"
        };
    }

    function configureCatalog(page, category) {
        const options = [option("hyprland.animations.enabled", "animations", "boolean", true), option("hyprland.decoration.rounding", "decoration", "integer", 10), option("hyprland.cursor.zoom_factor", "cursor", "number", 1), option("hyprland.input.sensitivity", "input", "number", 0), option("hyprland.binds.drag_threshold", "binds", "integer", 10), option("hyprland.debug.disable_logs", "debug", "boolean", true, false), option("hyprland.ecosystem.no_update_news", "ecosystem", "boolean", false)];
        const values = {};
        for (const definition of options)
            values[definition.id] = definition.defaultValue;
        page.allOptionsAvailable = false;
        page.categoryId = category || "appearance";
        page.serviceAvailable = true;
        page.writable = true;
        page.busy = false;
        page.revisionToken = "9";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.allOptions = options;
        page.allValues = values;
        page.allOptionsAvailable = true;
        page.synchronizeProjection(true);
    }

    function actions() {
        return [
            {
                id: "window.close",
                kind: "dispatcher",
                actionType: "dispatcher"
            },
            {
                id: "terminal",
                kind: "defaultApp",
                actionType: "defaultApp"
            },
            {
                id: "overview.toggle",
                kind: "hyprshelld",
                actionType: "hyprshelld"
            }
        ];
    }

    function bindingOptions() {
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

    function binding(id, key, submap) {
        return {
            id: id,
            modifiers: ["super"],
            key: key,
            actionType: "dispatcher",
            action: "window.close",
            arguments: {},
            description: "Close the focused window",
            enabled: true,
            submap: submap || "",
            options: bindingOptions()
        };
    }

    function configureBindings(page, records, submaps, defaults) {
        page.bindingsProjectionAvailable = false;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.bindingsAvailable = true;
        page.actionCatalogAvailable = true;
        page.busy = false;
        page.revisionToken = "12";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.bindingActions = actions();
        page.defaultBindings = defaults || [];
        page.bindings = records || [];
        page.submaps = submaps || [];
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.bindingsProjectionAvailable = true;
        page.synchronizeProjection(true);
    }

    function test_catalogTaxonomySearchSparseSaveAndReadOnlyPreservation() {
        const window = createTemporaryObject(catalogComponent, this);
        verify(window !== null);
        const page = window.page;
        configureCatalog(page, "appearance");

        compare(page.modules, ["animations", "decoration", "cursor"]);
        compare(page.categoryOptions.length, 3);
        compare(page.controlsEnabled, true);
        compare(page.projectionInitialized, true);

        page.searchText = "rounding";
        compare(page.filteredOptions.length, 1);
        compare(page.filteredOptions[0].id, "hyprland.decoration.rounding");
        page.searchText = "";
        page.selectedModule = "cursor";
        compare(page.filteredOptions.length, 1);
        page.selectedModule = "";

        page.editValue("hyprland.decoration.rounding", 18);
        compare(page.savePatchCount, 1);
        compare(page.savePatch["hyprland.decoration.rounding"], 18);
        verify(!Object.prototype.hasOwnProperty.call(page.savePatch, "hyprland.animations.enabled"));
        findChild(page, "hyprlandCatalogSaveButton").clicked();
        compare(window.saveSpy.count, 1);
        compare(window.saveSpy.signalArguments[0][0], {
            "hyprland.decoration.rounding": 18
        });

        configureCatalog(page, "system");
        compare(page.categoryOptions.length, 1);
        page.editValue("hyprland.debug.disable_logs", false);
        compare(page.savePatchCount, 0);
        compare(page.saveEnabled, false);
    }

    function test_catalogRoutesEveryScalarCategoryToItsGuidedSurface() {
        const window = createTemporaryObject(catalogComponent, this);
        verify(window !== null);
        const page = window.page;
        const expected = {
            appearance: "appearance",
            input: "input",
            windows: "windows",
            shortcuts: "bindings",
            system: "advanced",
            session: "environment"
        };
        for (const category of Object.keys(expected)) {
            page.categoryId = category;
            compare(page.guideTarget, expected[category], category);
            verify(page.modules.length > 0, category);
        }
    }

    function test_bindingSchemaChordLimitsAndOptionInvariants() {
        const window = createTemporaryObject(bindingsComponent, this);
        verify(window !== null);
        const page = window.page;
        const record = binding("binding.close", "Q");
        configureBindings(page, [record], []);

        compare(page.controlsEnabled, true);
        compare(page.draftIssue, "");
        compare(page.keyValid("code:4294967294"), true);
        compare(page.keyValid("code:4294967295"), false);
        compare(page.keyValid("mouse:272"), true);
        compare(page.keyValid("mouse:768"), false);
        compare(page.keyValid("catchall"), true);

        const invalid = page.clone(record);
        invalid.options.click = true;
        invalid.options.release = false;
        page.draftBindings = [invalid];
        verify(page.draftIssue.includes("trigger on release"));

        invalid.options.release = true;
        invalid.options.drag = true;
        page.draftBindings = [invalid];
        verify(page.draftIssue.includes("both click and drag"));

        invalid.options.click = false;
        invalid.options.drag = false;
        invalid.key = "code:4294967295";
        page.draftBindings = [invalid];
        verify(page.draftIssue.includes("invalid key token"));
    }

    function test_defaultBindingSparseOverrideDisableResetAndSaveAcknowledgement() {
        const window = createTemporaryObject(bindingsComponent, this);
        verify(window !== null);
        const page = window.page;
        const baseline = binding("hyprshelld.default.window.close", "q");
        baseline.description = "Close the active window";
        configureBindings(page, [], [], [baseline]);

        compare(page.draftBindings.length, 1);
        compare(page.draftBindings[0].id, baseline.id);
        compare(page.draftBindings[0]._defaultId, baseline.id);
        compare(page.bindingOrigin(page.draftBindings[0]), "default");
        compare(page.persistedBindings(page.draftBindings).length, 0);
        compare(page.draftDirty, false);
        compare(page.visibleBindings.length, 1);
        page.bindingSearchText = "active window";
        compare(page.visibleBindings.length, 1);
        page.bindingSearchText = "does not exist";
        compare(page.visibleBindings.length, 0);
        page.bindingSearchText = "";
        page.bindingFilterIndex = 1;
        compare(page.visibleBindings.length, 0);
        page.bindingFilterIndex = 2;
        compare(page.visibleBindings.length, 1);
        page.bindingFilterIndex = 0;

        const override = page.clone(page.draftBindings[0]);
        override.key = "w";
        override.description = "Close the active window with the user shortcut";
        page.replaceBinding(override);

        compare(page.bindingOrigin(page.draftBindings[0]), "override");
        page.bindingFilterIndex = 1;
        compare(page.visibleBindings.length, 1);
        page.bindingFilterIndex = 0;
        let sparseLayer = page.persistedBindings(page.draftBindings);
        compare(sparseLayer.length, 1);
        compare(sparseLayer[0].id, baseline.id);
        compare(sparseLayer[0].key, "w");
        verify(!Object.prototype.hasOwnProperty.call(sparseLayer[0], "_defaultId"));
        verify(!Object.prototype.hasOwnProperty.call(sparseLayer[0], "_bindingOrigin"));

        page.removeBinding(baseline.id);
        compare(page.bindingOrigin(page.draftBindings[0]), "disabled");
        sparseLayer = page.persistedBindings(page.draftBindings);
        compare(sparseLayer.length, 1);
        compare(sparseLayer[0].id, baseline.id);
        compare(sparseLayer[0].enabled, false);

        page.resetBinding(baseline.id);
        compare(page.bindingOrigin(page.draftBindings[0]), "default");
        compare(page.draftBindings[0].key, baseline.key);
        compare(page.persistedBindings(page.draftBindings).length, 0);
        compare(page.draftDirty, false);
        page.bindingFilterIndex = 1;
        compare(page.visibleBindings.length, 0);
        page.bindingFilterIndex = 0;

        const savedOverride = page.clone(page.draftBindings[0]);
        savedOverride.key = "w";
        page.replaceBinding(savedOverride);
        compare(page.draftDirty, true);
        sparseLayer = page.persistedBindings(page.draftBindings);
        findChild(page, "saveBindingsButton").clicked();
        compare(window.saveSpy.count, 1);
        compare(window.saveSpy.signalArguments[0][0].length, 1);
        compare(window.saveSpy.signalArguments[0][0][0].id, baseline.id);
        compare(window.saveSpy.signalArguments[0][0][0].key, "w");

        page.externalChangeWhileEditing = true;
        page.bindings = sparseLayer;
        page.synchronizeProjection(false);
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.bindingOrigin(page.draftBindings[0]), "override");
    }

    function test_legacyChordFallbackMigratesToTheStableDefaultId() {
        const window = createTemporaryObject(bindingsComponent, this);
        verify(window !== null);
        const page = window.page;
        const baseline = binding("hyprshelld.default.window.close", "q");
        const legacyOverride = binding("legacy.user.close", "q");
        legacyOverride.description = "Legacy user close shortcut";
        configureBindings(page, [legacyOverride], [], [baseline]);

        compare(page.draftBindings.length, 1);
        compare(page.draftBindings[0].id, baseline.id);
        compare(page.draftBindings[0]._defaultId, baseline.id);
        compare(page.bindingOrigin(page.draftBindings[0]), "override");
        const migrated = page.persistedBindings(page.draftBindings);
        compare(migrated.length, 1);
        compare(migrated[0].id, baseline.id);
        compare(migrated[0].key, baseline.key);
        compare(migrated[0].description, legacyOverride.description);
    }

    function test_customBindingUsesAnUnusedCanonicalChordAndAcknowledgesReorderedData() {
        const window = createTemporaryObject(bindingsComponent, this);
        verify(window !== null);
        const page = window.page;
        const closeDefault = binding("hyprshelld.default.window.close", "q");
        const focusDefault = binding("hyprshelld.default.focus.window.up.vim", "k");
        focusDefault.description = "Focus the window up";
        configureBindings(page, [], [], [closeDefault, focusDefault]);

        page.addBinding();
        compare(page.draftBindings.length, 3);
        const custom = page.draftBindings[2];
        compare(page.bindingOrigin(custom), "custom");
        compare(custom.modifiers, ["super"]);
        compare(custom.key, "F13");
        compare(page.draftIssue, "");
        compare(page.draftDirty, true);

        const sparseLayer = page.persistedBindings(page.draftBindings);
        compare(sparseLayer.length, 1);
        findChild(page, "saveBindingsButton").clicked();
        compare(window.saveSpy.count, 1);
        compare(window.saveSpy.signalArguments[0][0][0].key, "F13");

        const saved = sparseLayer[0];
        const reorderedAcknowledgement = {
            options: saved.options,
            submap: saved.submap,
            enabled: saved.enabled,
            description: saved.description,
            arguments: saved.arguments,
            action: saved.action,
            actionType: saved.actionType,
            key: saved.key,
            modifiers: saved.modifiers,
            id: saved.id
        };
        page.bindings = [reorderedAcknowledgement];
        page.synchronizeProjection(false);
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.bindingOrigin(page.draftBindings[2]), "custom");
    }

    function test_compactDefaultsOpenOnTheKeyboardAccessibleList() {
        const window = createTemporaryObject(bindingsComponent, this, {
            width: 620
        });
        verify(window !== null);
        const page = window.page;
        const baseline = binding("hyprshelld.default.window.close", "q");
        configureBindings(page, [], [], [baseline]);

        compare(page.compactPage, true);
        compare(page.selectedBindingId, "");
        const card = findChild(page, "bindingCard0");
        verify(card !== null);
        compare(card.activeFocusOnTab, true);
        window.requestActivate();
        tryCompare(window, "active", true);
        card.forceActiveFocus();
        tryCompare(card, "activeFocus", true);
        keyClick(Qt.Key_Return);
        tryCompare(page, "selectedBindingId", baseline.id);
    }

    function test_resetClearsTransientEditorErrors() {
        const window = createTemporaryObject(bindingsComponent, this);
        verify(window !== null);
        const page = window.page;
        const baseline = binding("hyprshelld.default.window.close", "q");
        configureBindings(page, [], [], [baseline]);
        page.selectedBindingId = baseline.id;

        const override = page.clone(page.draftBindings[0]);
        override.key = "w";
        page.replaceBinding(override);
        const editor = findChild(page, "bindingEditor");
        verify(editor !== null);
        editor.commitArguments("{");
        verify(editor.argumentsIssue.length > 0);
        const devices = [];
        for (let index = 0; index < 65; ++index)
            devices.push("device-" + String(index));
        editor.commitDeviceList(devices.join(","));
        verify(editor.deviceIssue.length > 0);

        const resetButton = findChild(editor, "resetBindingButton");
        verify(resetButton !== null);
        tryCompare(resetButton, "visible", true);
        resetButton.clicked();
        compare(editor.argumentsIssue, "");
        compare(editor.deviceIssue, "");
        compare(page.bindingOrigin(page.draftBindings[0]), "default");
        compare(page.draftDirty, false);
    }

    function test_bindingAndSubmapCrudOrderingConflictAndAtomicSave() {
        const window = createTemporaryObject(bindingsComponent, this);
        verify(window !== null);
        const page = window.page;
        configureBindings(page, [binding("binding.close", "Q")], []);

        page.addSubmap();
        compare(page.draftSubmaps.length, 1);
        const submapName = page.draftSubmaps[0].name;
        const moved = page.clone(page.draftBindings[0]);
        moved.submap = submapName;
        page.replaceBinding(moved);
        page.addBinding();
        compare(page.draftBindings.length, 2);
        page.moveBinding(page.draftBindings[1].id, -1);
        compare(page.draftBindings[1].id, "binding.close");
        compare(page.draftIssue, "");
        compare(page.saveEnabled, true);

        findChild(page, "saveBindingsButton").clicked();
        compare(window.saveSpy.count, 1);
        compare(window.saveSpy.signalArguments[0][0].length, 2);
        compare(window.saveSpy.signalArguments[0][1].length, 1);

        page.bindings = [binding("binding.external", "E")];
        page.synchronizeProjection(false);
        compare(page.externalChangeWhileEditing, true);
        compare(page.saveEnabled, false);
        page.synchronizeProjection(true);
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftBindings[0].id, "binding.external");
    }

    function test_bindingControlsAndGraphicsRemainAccessible() {
        const window = createTemporaryObject(bindingsComponent, this);
        verify(window !== null);
        const page = window.page;
        configureBindings(page, [], []);

        for (const name of ["shortcutsTabButton", "submapsTabButton", "addBindingButton", "addSubmapButton", "saveBindingsButton"]) {
            const control = findChild(page, name);
            verify(control !== null, name);
            verify(control.implicitHeight >= 44, name);
        }
        verify(findChild(page, "bindingsTabBar") !== null);
        verify(findChild(page, "bindingsStatusCard") !== null);
    }
}
