pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import HyprShelld.UI
import "../../src/settings" as Settings

ApplicationWindow {
    id: root

    property string sceneName: argumentValue("scene", "overview")
    property string outputPath: argumentValue("output", "/tmp/hyprshelld-settings.png")
    property string themeMode: argumentValue("theme", "dark")
    property string selectorMode: argumentValue("selector-mode", themeMode)
    property bool autoCapture: true
    property bool fixtureReady: false
    property alias renderedItem: captureSurface
    property alias pageItem: pageLoader.item
    property var pinnedCatalog: null

    width: Number(argumentValue("width", "1080"))
    height: Number(argumentValue("height", "720"))
    visible: true
    color: palette.window
    title: "HyprShelld Settings visual fixture · " + sceneName

    palette.window: ShellTheme.colorFor(themeMode, "canvas")
    palette.windowText: ShellTheme.colorFor(themeMode, "onSurface")
    palette.base: ShellTheme.colorFor(themeMode, "card")
    palette.alternateBase: ShellTheme.colorFor(themeMode, "floating")
    palette.text: ShellTheme.colorFor(themeMode, "onSurface")
    palette.brightText: ShellTheme.colorFor(themeMode, "onSurface")
    palette.button: ShellTheme.colorFor(themeMode, "floating")
    palette.buttonText: ShellTheme.colorFor(themeMode, "onSurface")
    palette.highlight: ShellTheme.colorFor(themeMode, "primary")
    palette.highlightedText: ShellTheme.colorFor(themeMode, "onPrimary")
    palette.placeholderText: ShellTheme.colorFor(themeMode, "onSurfaceMuted")
    palette.light: ShellTheme.colorFor(themeMode, "floating")
    palette.midlight: ShellTheme.colorFor(themeMode, "track")
    palette.mid: ShellTheme.colorFor(themeMode, "outline")
    palette.dark: ShellTheme.colorFor(themeMode, "outlineStrong")
    palette.shadow: ShellTheme.colorFor(themeMode, "shadow")
    palette.link: ShellTheme.colorFor(themeMode, "primary")
    palette.linkVisited: ShellTheme.colorFor(themeMode, "primary")
    palette.toolTipBase: ShellTheme.colorFor(themeMode, "floating")
    palette.toolTipText: ShellTheme.colorFor(themeMode, "onSurface")
    palette.disabled.window: ShellTheme.colorFor(themeMode, "canvas")
    palette.disabled.windowText: ShellTheme.colorFor(themeMode, "onSurfaceDisabled")
    palette.disabled.base: ShellTheme.colorFor(themeMode, "card")
    palette.disabled.alternateBase: ShellTheme.colorFor(themeMode, "floating")
    palette.disabled.text: ShellTheme.colorFor(themeMode, "onSurfaceDisabled")
    palette.disabled.brightText: ShellTheme.colorFor(themeMode, "onSurfaceDisabled")
    palette.disabled.button: ShellTheme.colorFor(themeMode, "floating")
    palette.disabled.buttonText: ShellTheme.colorFor(themeMode, "onSurfaceDisabled")
    palette.disabled.highlight: ShellTheme.colorFor(themeMode, "track")
    palette.disabled.highlightedText: ShellTheme.colorFor(themeMode, "onSurfaceDisabled")
    palette.disabled.placeholderText: ShellTheme.colorFor(themeMode, "onSurfaceDisabled")
    palette.disabled.light: ShellTheme.colorFor(themeMode, "floating")
    palette.disabled.midlight: ShellTheme.colorFor(themeMode, "track")
    palette.disabled.mid: ShellTheme.colorFor(themeMode, "outline")
    palette.disabled.dark: ShellTheme.colorFor(themeMode, "outlineStrong")
    palette.disabled.shadow: ShellTheme.colorFor(themeMode, "shadow")
    palette.disabled.link: ShellTheme.colorFor(themeMode, "onSurfaceDisabled")
    palette.disabled.linkVisited: ShellTheme.colorFor(themeMode, "onSurfaceDisabled")
    palette.disabled.toolTipBase: ShellTheme.colorFor(themeMode, "floating")
    palette.disabled.toolTipText: ShellTheme.colorFor(themeMode, "onSurfaceDisabled")

    Component.onCompleted: {
        ShellTheme.previewMode = themeMode;
        console.log("VISUAL_HARNESS_READY", sceneName, themeMode, width + "x" + height);
    }

    function argumentValue(name, fallback) {
        const prefix = name + "=";
        for (const argument of Qt.application.arguments) {
            if (String(argument).startsWith(prefix))
                return String(argument).slice(prefix.length);
        }
        return fallback;
    }

    function clone(value) {
        return JSON.parse(JSON.stringify(value));
    }

    function loadPinnedCatalog() {
        if (root.pinnedCatalog)
            return root.pinnedCatalog;
        const request = new XMLHttpRequest();
        request.open("GET", Qt.resolvedUrl("../../data/hyprland/config-catalog-v2.json"), false);
        request.send();
        if (request.status !== 0 && request.status !== 200)
            throw new Error("Unable to read the pinned Hyprland catalog");
        root.pinnedCatalog = JSON.parse(request.responseText);
        return root.pinnedCatalog;
    }

    function rawCatalogOption(id) {
        const catalog = root.loadPinnedCatalog();
        return catalog.options.find(option => option.id === id) || null;
    }

    function resolvedCatalogDefault(option, seen) {
        if (!option)
            return undefined;
        const value = option.default;
        if (!value || typeof value !== "object" || Array.isArray(value) || value.kind !== "inherit")
            return root.clone(value);
        const visited = seen || new Set();
        if (visited.has(option.id))
            return undefined;
        visited.add(option.id);
        const catalog = root.loadPinnedCatalog();
        const inherited = catalog.options.find(candidate => candidate.path === value.from);
        return root.resolvedCatalogDefault(inherited, visited);
    }

    function guidedProjection(page) {
        const definitions = [];
        const values = {};
        for (const id of page.expectedOptionIds) {
            const source = root.rawCatalogOption(id);
            if (!source)
                throw new Error("Missing guided option " + id);
            const definition = {
                id: source.id,
                type: source.type,
                control: source.control,
                defaultValue: root.resolvedCatalogDefault(source),
                risk: source.risk,
                description: source.description,
                documentation: source.documentation
            };
            const constraints = source.constraints || {};
            for (const key of Object.keys(constraints))
                definition[key] = root.clone(constraints[key]);
            definitions.push(definition);
            values[id] = root.clone(definition.defaultValue);
        }
        return {
            definitions: definitions,
            values: values
        };
    }

    function coverageOptions() {
        const modules = [["animations", 2], ["binds", 14], ["cursor", 22], ["debug", 22], ["decoration", 43], ["dwindle", 11], ["ecosystem", 3], ["experimental", 1], ["general", 23], ["gestures", 14], ["group", 47], ["input", 62], ["layout", 2], ["master", 14], ["misc", 40], ["opengl", 1], ["quirks", 2], ["render", 17], ["scrolling", 9], ["xwayland", 4]];
        const options = [];
        for (const pair of modules) {
            for (let index = 0; index < pair[1]; ++index) {
                options.push({
                    id: "hyprland." + pair[0] + ".fixture_" + index,
                    module: pair[0]
                });
            }
        }
        return options;
    }

    function option(moduleName, name, type, value, tier, risk) {
        return {
            id: "hyprland." + moduleName + "." + name,
            path: moduleName + ":" + name,
            module: moduleName,
            luaPath: [moduleName, name],
            type: type,
            defaultPolicy: "hyprland",
            writable: true,
            defaultValue: value,
            uiTier: tier || "common",
            control: type === "boolean" ? "toggle" : type === "enum" ? "select" : "text",
            choices: type === "enum" ? [
                {
                    label: "Automatic",
                    value: "auto"
                },
                {
                    label: "Enabled",
                    value: "enabled"
                },
                {
                    label: "Disabled",
                    value: "disabled"
                }
            ] : [],
            min: type === "number" ? -1 : type === "integer" ? 0 : undefined,
            max: type === "number" ? 10 : type === "integer" ? 1000 : undefined,
            maxLength: type === "string" ? 256 : undefined,
            applyMode: "reload",
            risk: risk || "safe",
            since: "0.56.2",
            description: "Controls " + name.replace(/_/g, " ") + " in the generated Hyprland Lua configuration."
        };
    }

    function catalogFixture(category) {
        if (category === "appearance") {
            return [option("animations", "enabled", "boolean", true), option("animations", "first_launch_animation", "boolean", true, "advanced"), option("decoration", "rounding", "integer", 10), option("decoration", "active_opacity", "number", 1), option("decoration", "col_active_border", "color", "0xFF9F8CFF", "advanced"), option("cursor", "zoom_factor", "number", 1)];
        }
        if (category === "input") {
            const profile = option("input", "accel_profile", "enum", "auto");
            return [option("input", "kb_layout", "string", "us"), option("input", "sensitivity", "number", 0), profile, option("input", "follow_mouse", "integer", 1, "advanced"), option("input", "natural_scroll", "boolean", false), option("gestures", "workspace_swipe_distance", "integer", 300)];
        }
        if (category === "windows") {
            const layout = option("general", "layout", "enum", "auto");
            return [layout, option("general", "gaps_in", "cssGap", [5, 5, 5, 5]), option("dwindle", "preserve_split", "boolean", true), option("master", "mfact", "number", 0.55), option("scrolling", "column_width", "number", 0.5, "advanced"), option("group", "auto_group", "boolean", true)];
        }
        if (category === "shortcuts") {
            return [option("binds", "drag_threshold", "integer", 10), option("binds", "scroll_event_delay", "integer", 300), option("binds", "movefocus_cycles_fullscreen", "boolean", true), option("binds", "workspace_center_on", "integer", 0, "advanced")];
        }
        if (category === "system") {
            return [option("misc", "disable_hyprland_logo", "boolean", true), option("render", "direct_scanout", "boolean", false, "expert", "caution"), option("xwayland", "enabled", "boolean", true), option("opengl", "nvidia_anti_flicker", "boolean", true, "advanced"), option("quirks", "prefer_hdr", "boolean", false, "expert"), option("debug", "disable_logs", "boolean", true, "expert", "caution"), option("experimental", "xx_color_management_v4", "boolean", false, "expert", "dangerous")];
        }
        return [option("ecosystem", "no_update_news", "boolean", false), option("ecosystem", "no_donation_nag", "boolean", false), option("ecosystem", "enforce_permissions", "boolean", true, "advanced")];
    }

    function defaultBindingOptions() {
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

    function bindingRecord(id, modifiers, key, action, description, submap) {
        return {
            id: id,
            modifiers: modifiers,
            key: key,
            actionType: "dispatcher",
            action: action,
            arguments: {},
            description: description,
            enabled: true,
            submap: submap || "",
            options: defaultBindingOptions()
        };
    }

    function configureBindings(page) {
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
        page.bindingActions = [
            {
                id: "window.close",
                kind: "dispatcher",
                actionType: "dispatcher"
            },
            {
                id: "fullscreen",
                kind: "dispatcher",
                actionType: "dispatcher"
            },
            {
                id: "workspace",
                kind: "dispatcher",
                actionType: "dispatcher"
            },
            {
                id: "terminal",
                kind: "defaultApp",
                actionType: "defaultApp"
            }
        ];
        page.defaultBindings = [
            bindingRecord("hyprshelld.default.window.close", ["super"], "q", "window.close", "Close the focused window"),
            bindingRecord("hyprshelld.default.window.fullscreen", ["super", "shift"], "f", "fullscreen", "Toggle fullscreen"),
            bindingRecord("hyprshelld.default.focus.window.up.vim", ["super"], "k", "window.close", "Focus the window up"),
            bindingRecord("hyprshelld.default.focus.workspace.3", ["super"], "3", "workspace", "Focus workspace three")
        ];
        const closeOverride = bindingRecord("hyprshelld.default.window.close", ["super"], "z", "window.close", "Close the focused window with my shortcut");
        const disabledFullscreen = bindingRecord("hyprshelld.default.window.fullscreen", ["super", "shift"], "f", "fullscreen", "Toggle fullscreen");
        disabledFullscreen.enabled = false;
        page.bindings = [
            closeOverride,
            disabledFullscreen,
            bindingRecord("binding.move", ["super", "shift"], "3", "workspace", "Move to workspace three", "move")
        ];
        page.submaps = [
            {
                id: "submap.resize",
                name: "resize",
                reset: "",
                enabled: true
            },
            {
                id: "submap.move",
                name: "move",
                reset: "resize",
                enabled: true
            }
        ];
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.bindingsProjectionAvailable = true;
        page.synchronizeProjection(true);
        if (root.sceneName === "bindings-submaps") {
            page.currentTab = 1;
        } else if (root.sceneName === "bindings-list" && root.width < 760) {
            page.selectedBindingId = "";
        } else {
            page.currentTab = 0;
            page.selectedBindingId = "hyprshelld.default.window.close";
        }
    }

    function deviceRecord() {
        return {
            id: "device.keyboard",
            selector: "at-translated-set-2-keyboard",
            kind: "keyboard",
            enabled: true,
            overrides: {
                kb_layout: "us,de",
                kb_options: "caps:escape",
                repeat_rate: 30,
                repeat_delay: 450,
                numlock_by_default: true
            }
        };
    }

    function configureInputDevices(page) {
        page.inputDevicesProjectionAvailable = false;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.inputDevicesAvailable = true;
        page.busy = false;
        page.inputDevices = [deviceRecord(),
            {
                id: "device.touchpad",
                selector: "precision-touchpad",
                kind: "touchpad",
                enabled: true,
                overrides: {
                    natural_scroll: true,
                    tap_to_click: true,
                    sensitivity: 0.2
                }
            }
        ];
        page.revisionToken = "7";
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.inputDevicesProjectionAvailable = true;
        page.reviewProjection();
        if (root.sceneName === "input-editor")
            page.openInputDevice("device.keyboard");
    }

    function configureEnvironment(page) {
        page.environmentProjectionAvailable = false;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.environmentAvailable = true;
        page.uwsmIntegrationAvailable = false;
        page.busy = false;
        page.environmentVariables = [
            {
                id: "environment.cursor",
                name: "XCURSOR_SIZE",
                value: "24",
                scope: "hyprland"
            },
            {
                id: "environment.toolkit",
                name: "NIXOS_OZONE_WL",
                value: "1",
                scope: "hyprland"
            },
            {
                id: "environment.backend",
                name: "GDK_BACKEND",
                value: "wayland,x11,*",
                scope: "hyprland"
            }
        ];
        page.revisionToken = "7";
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.environmentProjectionAvailable = true;
        page.reviewProjection();
        if (root.sceneName === "environment-editor")
            page.openVariable("environment.cursor");
    }

    function configurePermissions(page) {
        page.permissionsProjectionAvailable = false;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.permissionsAvailable = true;
        page.busy = false;
        page.permissions = [
            {
                id: "permission.portal",
                binary: "^/usr/lib/xdg-desktop-portal(?:-[a-z]+)?$",
                type: "screencopy",
                mode: "ask"
            },
            {
                id: "permission.obs",
                binary: "^/usr/bin/obs$",
                type: "screencopy",
                mode: "allow"
            },
            {
                id: "permission.unknown-plugin",
                binary: ".*",
                type: "plugin",
                mode: "deny"
            }
        ];
        page.revisionToken = "7";
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.permissionsProjectionAvailable = true;
        page.reviewProjection();
        if (root.sceneName === "permissions-editor")
            page.openPermission("permission.portal");
    }

    function monitorRecord() {
        return {
            id: "display-DP-1",
            selector: "DP-1",
            enabled: true,
            mode: "maxwidth",
            position: "auto-center-down",
            scale: 1.333333,
            reserved: [1, 2, 3, 4],
            transform: 7,
            mirror: "",
            bitdepth: 10,
            cm: "hdr",
            sdrEotf: "gamma22force",
            sdrBrightness: 1.25,
            sdrSaturation: 0.9,
            vrr: 3,
            icc: "/profiles/display-p3.icc",
            supportsWideColor: 1,
            supportsHdr: 1,
            sdrMinLuminance: 0.1,
            sdrMaxLuminance: 203,
            minLuminance: 0.005,
            maxLuminance: 1000,
            maxAvgLuminance: 400
        };
    }

    function connectedDisplay() {
        return {
            selector: "DP-1",
            description: "27-inch HDR display",
            make: "Example",
            model: "Panel",
            serial: "DP-1-serial",
            enabled: true,
            width: 2560,
            height: 1440,
            physicalWidthMm: 600,
            physicalHeightMm: 340,
            refreshRate: 165,
            x: 0,
            y: 0,
            scale: 1,
            transform: 0,
            focused: true,
            dpms: true,
            vrrActive: true,
            mirrorOf: "",
            modes: [
                {
                    width: 2560,
                    height: 1440,
                    refreshRate: 165,
                    managedMode: "2560x1440@165"
                }
            ],
            colorManagement: "hdr",
            currentFormat: "XRGB2101010",
            sdrBrightness: 1,
            sdrSaturation: 1,
            sdrMinLuminance: 0.2,
            sdrMaxLuminance: 203
        };
    }

    function findByObjectName(node, name) {
        if (!node)
            return null;
        if (node.objectName === name)
            return node;
        const descendants = node.children || [];
        for (const descendant of descendants) {
            const found = findByObjectName(descendant, name);
            if (found)
                return found;
        }
        return null;
    }

    function scrollLargestFlickable(node, fraction) {
        let best = null;
        let bestOverflow = 0;
        function inspect(item) {
            if (!item)
                return;
            if (typeof item.contentY === "number" && typeof item.contentHeight === "number" && typeof item.height === "number") {
                const overflow = item.contentHeight - item.height;
                if (overflow > bestOverflow) {
                    bestOverflow = overflow;
                    best = item;
                }
            }
            const descendants = item.children || [];
            for (const descendant of descendants)
                inspect(descendant);
        }
        inspect(node);
        if (best && bestOverflow > 0)
            best.contentY = Math.max(0, Math.min(bestOverflow, bestOverflow * fraction));
    }

    function configureDisplay(page) {
        page.serviceAvailable = true;
        page.writable = true;
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.revision = 7;
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.snapshot = {
            monitors: [monitorRecord()]
        };
        page.connectedDisplays = [connectedDisplay()];
        page.topologyDigest = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.synchronizeDraft(false);
        Qt.callLater(function () {
            const advanced = findByObjectName(page, "displayAdvancedButton");
            if (advanced)
                advanced.checked = true;
            Qt.callLater(function () {
                scrollLargestFlickable(page, root.sceneName === "displays-luminance" ? 0.78 : 0.43);
                root.fixtureReady = true;
            });
        });
    }

    function configureAppearance(page) {
        const projection = root.guidedProjection(page);
        page.shellAppearanceMode = root.themeMode;
        page.shellEffectiveAppearanceMode = root.themeMode;
        page.shellAppearanceServiceAvailable = true;
        page.shellAppearanceBusy = false;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.appearanceOptions = projection.definitions;
        page.appearanceValues = projection.values;
        page.appearanceProjectionAvailable = true;
        page.appearanceAnimationProjectionAvailable = true;
        page.appearanceCurves = [
            {
                id: "curve-smooth",
                name: "smooth",
                type: "bezier",
                points: [[0.12, 0.72], [0.22, 0.98]]
            },
            {
                id: "curve-spring",
                name: "springy",
                type: "spring",
                stiffness: 275.5,
                dampening: 27.5,
                mass: 1.25
            }
        ];
        page.appearanceAnimations = [
            {
                id: "animation-windows",
                name: "windows",
                enabled: true,
                speed: 6,
                curve: "smooth",
                style: "slide"
            }
        ];
        page.sharedBorderAvailable = true;
        page.sharedBorderBusy = false;
        page.windowBorderSynced = true;
        page.sharedBorderSyncState = "current";
        page.sharedBorderConfigRevisionToken = "11";
        page.sharedBorderVerifiedRevisionToken = "11";
        page.sharedSpacingAvailable = true;
        page.sharedSpacingBusy = false;
        page.windowSpacingSynced = true;
        page.sharedSpacingSyncState = "current";
        page.sharedSpacingConfigRevisionToken = "11";
        page.sharedSpacingVerifiedRevisionToken = "11";
        page.revisionToken = "7";
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.appearanceAvailable = true;
        page.reviewProjection();
        page.appearanceTabIndex = root.sceneName === "appearance-animations" ? 1 : 0;
    }

    function gestureActions() {
        return [
            {
                id: "close",
                label: "Close",
                description: "Close a window."
            },
            {
                id: "cursorZoom",
                label: "Cursor zoom",
                description: "Control cursor-centered zoom."
            },
            {
                id: "float",
                label: "Float",
                description: "Change floating state."
            },
            {
                id: "fullscreen",
                label: "Fullscreen",
                description: "Change fullscreen state."
            },
            {
                id: "move",
                label: "Move",
                description: "Move a window."
            },
            {
                id: "resize",
                label: "Resize",
                description: "Resize a window."
            },
            {
                id: "scrollMove",
                label: "Scroll move",
                description: "Move in the gesture direction."
            },
            {
                id: "special",
                label: "Special workspace",
                description: "Open a named special workspace."
            },
            {
                id: "workspace",
                label: "Workspace",
                description: "Change workspace."
            }
        ];
    }

    function configureGuidedInput(page) {
        const projection = root.guidedProjection(page);
        const gestures = [
            {
                id: "gesture-workspace",
                fingers: 3,
                direction: "left",
                modifiers: ["super"],
                scale: 1,
                disableInhibit: false,
                action: {
                    type: "workspace"
                }
            },
            {
                id: "gesture-resize",
                fingers: 3,
                direction: "pinch",
                modifiers: [],
                scale: 1,
                disableInhibit: false,
                action: {
                    type: "resize"
                }
            }
        ];
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.inputProjectionAvailable = true;
        page.inputAvailable = true;
        page.inputGesturesProjectionAvailable = true;
        page.inputGestures = gestures;
        page.inputGestureCompatibility = gestures.map(gesture => ({
                    id: gesture.id,
                    editable: true,
                    reason: ""
                }));
        page.inputGestureActions = root.gestureActions();
        page.busy = false;
        page.inputOptions = projection.definitions;
        page.inputValues = projection.values;
        page.revisionToken = "7";
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.reviewProjection();
        page.inputTabIndex = root.sceneName === "guided-input-gestures" ? 2 : 0;
    }

    function configureWindows(page) {
        const projection = root.guidedProjection(page);
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.windowsProjectionAvailable = true;
        page.windowsAvailable = true;
        page.busy = false;
        page.windowsOptions = projection.definitions;
        page.windowsValues = projection.values;
        page.revisionToken = "7";
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.reviewProjection();
    }

    function configureWorkspaces(page) {
        const projection = root.guidedProjection(page);
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.workspacesProjectionAvailable = true;
        page.workspaceRulesProjectionAvailable = true;
        page.workspacesAvailable = true;
        page.busy = false;
        page.workspacesOptions = projection.definitions;
        page.workspacesValues = projection.values;
        page.workspaceRules = [];
        page.revisionToken = "7";
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.reviewProjection();
        page.workspacesTabIndex = 0;
    }

    function configureRules(page) {
        page.rulesProjectionAvailable = false;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.rulesAvailable = true;
        page.busy = false;
        page.windowRules = [
            {
                id: "window-browser",
                name: "Browser picture-in-picture",
                enabled: true,
                match: {
                    class: "^firefox$",
                    title: "^Picture-in-Picture$"
                },
                effects: {
                    float: true,
                    pin: true,
                    rounding: 12
                }
            },
            {
                id: "window-terminal",
                name: "Terminal workspace",
                enabled: true,
                match: {
                    class: "^(kitty|foot)$"
                },
                effects: {
                    workspace: {
                        target: "2",
                        silent: true
                    }
                }
            }
        ];
        page.layerRules = [
            {
                id: "layer-launcher",
                name: "Launcher blur",
                enabled: true,
                match: {
                    namespace: "^(launcher|rofi)$"
                },
                effects: {
                    blur: true,
                    ignore_alpha: 0.2
                }
            },
            {
                id: "layer-bar",
                name: "Bar above windows",
                enabled: true,
                match: {
                    namespace: "^waybar$"
                },
                effects: {
                    order: 10
                }
            }
        ];
        page.revisionToken = "7";
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.rulesProjectionAvailable = true;
        page.reviewProjection();
        page.rulesTabIndex = root.sceneName === "rules-layer" ? 1 : 0;
    }

    function configurePage(page) {
        if (!page)
            return;
        if (sceneName === "theme-selector") {
            fixtureReady = true;
            return;
        } else if (sceneName === "overview") {
            page.allOptions = coverageOptions();
        } else if (sceneName.startsWith("catalog-")) {
            const category = sceneName.slice("catalog-".length);
            const options = catalogFixture(category);
            const values = {};
            for (const definition of options)
                values[definition.id] = definition.defaultValue;
            page.allOptionsAvailable = false;
            page.categoryId = category;
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
        } else if (sceneName.startsWith("bindings-")) {
            configureBindings(page);
        } else if (sceneName.startsWith("input-")) {
            configureInputDevices(page);
        } else if (sceneName.startsWith("environment-")) {
            configureEnvironment(page);
        } else if (sceneName.startsWith("permissions-")) {
            configurePermissions(page);
        } else if (sceneName.startsWith("displays-")) {
            configureDisplay(page);
            return;
        } else if (sceneName.startsWith("appearance-")) {
            configureAppearance(page);
        } else if (sceneName.startsWith("guided-input-")) {
            configureGuidedInput(page);
        } else if (sceneName === "windows-layout") {
            configureWindows(page);
        } else if (sceneName === "workspaces") {
            configureWorkspaces(page);
        } else if (sceneName.startsWith("rules-")) {
            configureRules(page);
        }
        fixtureReady = true;
    }

    function capture() {
        renderedItem.grabToImage(function (result) {
            const saved = result.saveToFile(outputPath);
            console.log("VISUAL_CAPTURE", sceneName, width + "x" + height, outputPath, saved);
            Qt.quit();
        });
    }

    Item {
        id: captureSurface
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            color: root.palette.window
        }

        Loader {
            id: pageLoader

            anchors.fill: parent
            sourceComponent: {
                if (root.sceneName === "overview")
                    return overviewComponent;
                if (root.sceneName === "theme-selector")
                    return themeSelectorComponent;
                if (root.sceneName.startsWith("catalog-"))
                    return catalogComponent;
                if (root.sceneName.startsWith("bindings-"))
                    return bindingsComponent;
                if (root.sceneName.startsWith("input-"))
                    return inputDevicesComponent;
                if (root.sceneName.startsWith("environment-"))
                    return environmentComponent;
                if (root.sceneName.startsWith("permissions-"))
                    return permissionsComponent;
                if (root.sceneName.startsWith("displays-"))
                    return displaysComponent;
                if (root.sceneName.startsWith("appearance-"))
                    return appearanceComponent;
                if (root.sceneName.startsWith("guided-input-"))
                    return guidedInputComponent;
                if (root.sceneName === "windows-layout")
                    return windowsComponent;
                if (root.sceneName === "workspaces")
                    return workspacesComponent;
                if (root.sceneName.startsWith("rules-"))
                    return rulesComponent;
                return overviewComponent;
            }

            onLoaded: root.configurePage(item)
        }
    }

    Timer {
        interval: 700
        running: root.autoCapture && root.fixtureReady && pageLoader.status === Loader.Ready
        repeat: false
        onTriggered: root.capture()
    }

    Timer {
        interval: 1800
        running: root.autoCapture
        repeat: false
        onTriggered: root.capture()
    }

    Component {
        id: themeSelectorComponent

        Page {
            background: Rectangle { color: palette.window }

            Settings.ThemeModeSelector {
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: 28
                }
                mode: root.selectorMode
                effectiveMode: root.themeMode
                serviceAvailable: true
            }
        }
    }

    Component {
        id: overviewComponent
        Settings.HyprlandOverviewPage {
            contentItem.focus: true
        }
    }

    Component {
        id: catalogComponent
        Settings.HyprlandCatalogPage {}
    }

    Component {
        id: bindingsComponent
        Settings.BindingsPage {}
    }

    Component {
        id: inputDevicesComponent
        Settings.InputDevicesPage {}
    }

    Component {
        id: environmentComponent
        Settings.EnvironmentVariablesPage {}
    }

    Component {
        id: permissionsComponent
        Settings.PermissionsPage {}
    }

    Component {
        id: displaysComponent
        Settings.DisplaysPage {}
    }

    Component {
        id: appearanceComponent
        Settings.AppearancePage {}
    }

    Component {
        id: guidedInputComponent
        Settings.InputPage {}
    }

    Component {
        id: windowsComponent
        Settings.WindowsLayoutPage {}
    }

    Component {
        id: workspacesComponent
        Settings.WorkspacesPage {}
    }

    Component {
        id: rulesComponent
        Settings.RulesPage {}
    }
}
