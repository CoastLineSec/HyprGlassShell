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

    function configureBindings(page, records, submaps) {
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
