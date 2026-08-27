import QtQuick
import QtQuick.Window
import QtTest
import HyprShelld.Client
import "../../src/settings" as Settings

TestCase {
    name: "BarSettingsPage"
    when: windowShown

    Component {
        id: pageComponent

        Settings.BarSettingsPage {
            width: 720
            height: 520
            barHeight: 40
            minimumBarHeight: 24
            maximumBarHeight: 96
            defaultBarHeight: 40
            shellBorderEnabled: true
            shellBorderWidth: 1
            shellBorderRadius: 15
            syncHyprlandWindowBorders: true
            shellInnerSpacing: 8
            shellOuterSpacing: 12
            syncHyprlandWindowSpacing: true
            workspaceShowIdentifiers: true
            workspaceShowNames: false
            workspaceShowApplications: false
            workspaceMaximumApplications: 3
            workspaceOccupiedOnly: false
            workspaceScrollMode: "disabled"
            workspaceInstanceAvailable: true
        }
    }

    Component {
        id: mainComponent

        Settings.Main {
            visible: false
        }
    }

    Component {
        id: healthWarningComponent

        Window {
            width: 375
            height: 480
            visible: true

            property alias warning: healthWarning

            Settings.ShellHealthWarning {
                id: healthWarning

                width: parent.width
                coordinatorAvailable: true
                coordinatorHealthy: true
                coordinatorFailedUnits: []
            }
        }
    }

    function enableCoreSettings(page) {
        page.coreServiceAvailable = true;
    }

    function enableWorkspaceSettings(page) {
        page.componentServiceAvailable = true;
        page.componentCatalogAvailable = true;
        page.componentWritable = true;
        page.workspaceInstanceAvailable = true;
    }

    Component {
        id: workspaceSettingsComponent

        Window {
            width: 520
            height: 900
            visible: true

            property alias settings: workspaceSettings

            Settings.WorkspaceSwitcherSettings {
                id: workspaceSettings

                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: 12
                }
                showIdentifiers: true
                showNames: false
                showApplications: false
                maximumApplications: 3
                occupiedOnly: false
                scrollMode: "disabled"
                controlsEnabled: true
            }
        }
    }

    Component {
        id: componentsPageComponent

        Window {
            width: 760
            height: 720
            visible: true

            property alias page: componentsPage

            Settings.ComponentsPage {
                id: componentsPage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: displaysPageComponent

        Window {
            width: 820
            height: 900
            visible: true

            property alias page: displaysPage

            Settings.DisplaysPage {
                id: displaysPage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: appearancePageComponent

        Window {
            width: 820
            height: 900
            visible: true

            property alias page: appearancePage

            Settings.AppearancePage {
                id: appearancePage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: appearancePreviewComponent

        Window {
            width: 720
            height: 420
            visible: true

            property alias preview: appearancePreview

            Settings.AppearancePreview {
                id: appearancePreview

                anchors.fill: parent
            }
        }
    }

    Component {
        id: inputPageComponent

        Window {
            width: 820
            height: 900
            visible: true

            property alias page: inputPage

            Settings.InputPage {
                id: inputPage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: windowsLayoutPageComponent

        Window {
            width: 820
            height: 900
            visible: true

            property alias page: windowsLayoutPage

            Settings.WindowsLayoutPage {
                id: windowsLayoutPage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: workspacesPageComponent

        Window {
            width: 820
            height: 900
            visible: true

            property alias page: workspacesPage

            Settings.WorkspacesPage {
                id: workspacesPage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: rulesPageComponent

        Window {
            width: 820
            height: 900
            visible: true

            property alias page: rulesPage

            Settings.RulesPage {
                id: rulesPage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: advancedPageComponent

        Window {
            width: 820
            height: 900
            visible: true

            property alias page: advancedPage

            Settings.AdvancedPage {
                id: advancedPage

                anchors.fill: parent
            }
        }
    }

    function advancedDefinitions() {
        return [
            {
                id: "hyprland.misc.allow_session_lock_restore",
                type: "boolean",
                control: "toggle",
                defaultValue: false,
                risk: "safe"
            },
            {
                id: "hyprland.misc.lockdead_screen_delay",
                type: "integer",
                control: "spinBox",
                defaultValue: 1000,
                min: 0,
                max: 5000,
                risk: "safe"
            },
            {
                id: "hyprland.misc.disable_scale_notification",
                type: "boolean",
                control: "toggle",
                defaultValue: false,
                risk: "safe"
            },
            {
                id: "hyprland.misc.render_unfocused_fps",
                type: "integer",
                control: "spinBox",
                defaultValue: 15,
                min: 1,
                max: 120,
                risk: "safe"
            },
            {
                id: "hyprland.misc.screencopy_force_8b",
                type: "boolean",
                control: "toggle",
                defaultValue: true,
                risk: "safe"
            },
            {
                id: "hyprland.misc.disable_hyprland_logo",
                type: "boolean",
                control: "toggle",
                defaultValue: false,
                risk: "safe"
            },
            {
                id: "hyprland.misc.disable_splash_rendering",
                type: "boolean",
                control: "toggle",
                defaultValue: false,
                risk: "safe"
            },
            {
                id: "hyprland.misc.session_lock_xray",
                type: "boolean",
                control: "toggle",
                defaultValue: false,
                risk: "safe"
            },
            {
                id: "hyprland.misc.session_lock_blur",
                type: "boolean",
                control: "toggle",
                defaultValue: false,
                risk: "safe"
            },
            {
                id: "hyprland.xwayland.use_nearest_neighbor",
                type: "boolean",
                control: "toggle",
                defaultValue: true,
                risk: "caution"
            },
            {
                id: "hyprland.render.expand_undersized_textures",
                type: "boolean",
                control: "toggle",
                defaultValue: true,
                risk: "caution"
            },
            {
                id: "hyprland.render.direct_scanout",
                type: "enum",
                control: "select",
                defaultValue: 0,
                min: 0,
                max: 2,
                choices: [
                    { label: "disable", value: 0 },
                    { label: "enable", value: 1 },
                    { label: "auto", value: 2 }
                ],
                risk: "caution"
            },
            {
                id: "hyprland.render.fp16_sdr_tf",
                type: "enum",
                control: "select",
                defaultValue: 0,
                min: 0,
                max: 1,
                choices: [
                    { label: "monitor", value: 0 },
                    { label: "linear", value: 1 }
                ],
                risk: "caution"
            },
            {
                id: "hyprland.render.xp_mode",
                type: "boolean",
                control: "toggle",
                defaultValue: false,
                risk: "caution"
            },
            {
                id: "hyprland.input-capture.capture_modifiers",
                type: "boolean",
                control: "toggle",
                defaultValue: false,
                risk: "caution"
            },
            {
                id: "hyprland.input-capture.enforce_barriers",
                type: "boolean",
                control: "toggle",
                defaultValue: true,
                risk: "caution"
            }
        ];
    }

    function advancedDefaults() {
        return {
            "hyprland.misc.allow_session_lock_restore": false,
            "hyprland.misc.lockdead_screen_delay": 1000,
            "hyprland.misc.disable_scale_notification": false,
            "hyprland.misc.render_unfocused_fps": 15,
            "hyprland.misc.screencopy_force_8b": true,
            "hyprland.misc.disable_hyprland_logo": false,
            "hyprland.misc.disable_splash_rendering": false,
            "hyprland.misc.session_lock_xray": false,
            "hyprland.misc.session_lock_blur": false,
            "hyprland.xwayland.use_nearest_neighbor": true,
            "hyprland.render.expand_undersized_textures": true,
            "hyprland.render.direct_scanout": 0,
            "hyprland.render.fp16_sdr_tf": 0,
            "hyprland.render.xp_mode": false,
            "hyprland.input-capture.capture_modifiers": false,
            "hyprland.input-capture.enforce_barriers": true
        };
    }

    function configureAdvancedPage(page, values) {
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.advancedOptions = advancedDefinitions();
        page.advancedValues = values || advancedDefaults();
        page.revisionToken = "7";
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.advancedProjectionAvailable = true;
        page.advancedAvailable = true;
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.reviewProjection();
    }

    function appearanceDefinitions() {
        return [
            {
                id: "hyprland.general.border_size",
                type: "integer",
                control: "spinBox",
                defaultValue: 1,
                min: 0,
                max: 20
            },
            {
                id: "hyprland.decoration.rounding",
                type: "integer",
                control: "spinBox",
                defaultValue: 0,
                min: 0,
                max: 20
            },
            {
                id: "hyprland.general.gaps_in",
                type: "cssGap",
                control: "text",
                defaultValue: [5, 5, 5, 5]
            },
            {
                id: "hyprland.general.gaps_out",
                type: "cssGap",
                control: "text",
                defaultValue: [20, 20, 20, 20]
            },
            {
                id: "hyprland.decoration.blur.enabled",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.decoration.shadow.enabled",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.animations.enabled",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.decoration.dim_inactive",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.decoration.dim_strength",
                type: "number",
                control: "slider",
                defaultValue: 0.5,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.active_opacity",
                type: "number",
                control: "slider",
                defaultValue: 1,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.inactive_opacity",
                type: "number",
                control: "slider",
                defaultValue: 1,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.fullscreen_opacity",
                type: "number",
                control: "slider",
                defaultValue: 1,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.dim_modal",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.decoration.dim_special",
                type: "number",
                control: "slider",
                defaultValue: 0.2,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.dim_around",
                type: "number",
                control: "slider",
                defaultValue: 0.4,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.blur.size",
                type: "integer",
                control: "spinBox",
                defaultValue: 8,
                min: 0,
                max: 100
            },
            {
                id: "hyprland.decoration.blur.passes",
                type: "integer",
                control: "spinBox",
                defaultValue: 1,
                min: 0,
                max: 10
            },
            {
                id: "hyprland.decoration.blur.ignore_opacity",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.decoration.blur.new_optimizations",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.decoration.blur.xray",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.decoration.blur.special",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.decoration.blur.popups",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.decoration.blur.popups_ignorealpha",
                type: "number",
                control: "slider",
                defaultValue: 0.2,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.blur.input_methods",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.decoration.blur.input_methods_ignorealpha",
                type: "number",
                control: "slider",
                defaultValue: 0.2,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.blur.brightness",
                type: "number",
                control: "slider",
                defaultValue: 1,
                min: 0,
                max: 2
            },
            {
                id: "hyprland.decoration.blur.contrast",
                type: "number",
                control: "slider",
                defaultValue: 0.8916,
                min: 0,
                max: 2
            },
            {
                id: "hyprland.decoration.blur.noise",
                type: "number",
                control: "slider",
                defaultValue: 0.0117,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.blur.vibrancy",
                type: "number",
                control: "slider",
                defaultValue: 0.1696,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.blur.vibrancy_darkness",
                type: "number",
                control: "slider",
                defaultValue: 0,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.border_part_of_window",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.decoration.rounding_power",
                type: "number",
                control: "slider",
                defaultValue: 2,
                min: 2,
                max: 10
            },
            {
                id: "hyprland.decoration.shadow.range",
                type: "integer",
                control: "spinBox",
                defaultValue: 4,
                min: 0,
                max: 100
            },
            {
                id: "hyprland.decoration.shadow.render_power",
                type: "integer",
                control: "spinBox",
                defaultValue: 3,
                min: 1,
                max: 4
            },
            {
                id: "hyprland.decoration.shadow.sharp",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.decoration.shadow.offset",
                type: "vector2",
                control: "vector2",
                defaultValue: [0, 0],
                min: [-250, -250],
                max: [250, 250]
            },
            {
                id: "hyprland.decoration.shadow.scale",
                type: "number",
                control: "slider",
                defaultValue: 1,
                min: 0,
                max: 1
            },
            {
                id: "hyprland.decoration.glow.enabled",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.decoration.glow.range",
                type: "integer",
                control: "spinBox",
                defaultValue: 10,
                min: 0,
                max: 100
            },
            {
                id: "hyprland.decoration.glow.render_power",
                type: "integer",
                control: "spinBox",
                defaultValue: 3,
                min: 1,
                max: 4
            }
        ];
    }

    function appearanceDefaults() {
        return {
            "hyprland.general.border_size": 1,
            "hyprland.decoration.rounding": 0,
            "hyprland.general.gaps_in": [5, 5, 5, 5],
            "hyprland.general.gaps_out": [20, 20, 20, 20],
            "hyprland.decoration.blur.enabled": true,
            "hyprland.decoration.shadow.enabled": true,
            "hyprland.animations.enabled": true,
            "hyprland.decoration.dim_inactive": false,
            "hyprland.decoration.dim_strength": 0.5,
            "hyprland.decoration.active_opacity": 1,
            "hyprland.decoration.inactive_opacity": 1,
            "hyprland.decoration.fullscreen_opacity": 1,
            "hyprland.decoration.dim_modal": true,
            "hyprland.decoration.dim_special": 0.2,
            "hyprland.decoration.dim_around": 0.4,
            "hyprland.decoration.blur.size": 8,
            "hyprland.decoration.blur.passes": 1,
            "hyprland.decoration.blur.ignore_opacity": true,
            "hyprland.decoration.blur.new_optimizations": true,
            "hyprland.decoration.blur.xray": false,
            "hyprland.decoration.blur.special": false,
            "hyprland.decoration.blur.popups": false,
            "hyprland.decoration.blur.popups_ignorealpha": 0.2,
            "hyprland.decoration.blur.input_methods": false,
            "hyprland.decoration.blur.input_methods_ignorealpha": 0.2,
            "hyprland.decoration.blur.brightness": 1,
            "hyprland.decoration.blur.contrast": 0.8916,
            "hyprland.decoration.blur.noise": 0.0117,
            "hyprland.decoration.blur.vibrancy": 0.1696,
            "hyprland.decoration.blur.vibrancy_darkness": 0,
            "hyprland.decoration.border_part_of_window": true,
            "hyprland.decoration.rounding_power": 2,
            "hyprland.decoration.shadow.range": 4,
            "hyprland.decoration.shadow.render_power": 3,
            "hyprland.decoration.shadow.sharp": false,
            "hyprland.decoration.shadow.offset": [0, 0],
            "hyprland.decoration.shadow.scale": 1,
            "hyprland.decoration.glow.enabled": false,
            "hyprland.decoration.glow.range": 10,
            "hyprland.decoration.glow.render_power": 3
        };
    }

    function appearanceCurveFixtures() {
        return [
            {
                id: "curve-default",
                name: "default",
                type: "bezier",
                points: [[0.12, 0.72], [0.22, 0.98]]
            },
            {
                id: "curve-linear",
                name: "linear",
                type: "spring",
                stiffness: 275.5,
                dampening: 27.5,
                mass: 1.25
            }
        ];
    }

    function appearanceAnimationFixtures() {
        return [
            {
                id: "animation-windows",
                name: "windows",
                enabled: true,
                speed: 6,
                curve: "default",
                style: "slide"
            }
        ];
    }

    function configureAppearancePage(
        page,
        values,
        windowBorderSynced,
        windowSpacingSynced,
        curves,
        animations
    ) {
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.appearanceOptions = appearanceDefinitions();
        page.appearanceValues = values || appearanceDefaults();
        page.appearanceProjectionAvailable = true;
        page.appearanceAnimationProjectionAvailable = true;
        page.appearanceCurves = curves || [];
        page.appearanceAnimations = animations || [];
        page.sharedBorderAvailable = true;
        page.sharedBorderBusy = false;
        page.windowBorderSynced = windowBorderSynced === true;
        page.sharedBorderSyncState = windowBorderSynced === true
            ? "current" : "override";
        page.sharedBorderSyncError = "";
        page.sharedBorderClientError = "";
        page.sharedBorderConfigRevisionToken = "11";
        page.sharedBorderVerifiedRevisionToken = "11";
        page.sharedSpacingAvailable = true;
        page.sharedSpacingBusy = false;
        page.windowSpacingSynced = windowSpacingSynced === true;
        page.sharedSpacingSyncState = windowSpacingSynced === true
            ? "current" : "override";
        page.sharedSpacingSyncError = "";
        page.sharedSpacingClientError = "";
        page.sharedSpacingConfigRevisionToken = "11";
        page.sharedSpacingVerifiedRevisionToken = "11";
        page.revisionToken = "7";
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.appearanceProjectionAvailable = true;
        page.appearanceAvailable = true;
        page.reviewProjection();
    }

    function inputDefinitions() {
        return [
            {
                id: "hyprland.input.repeat_rate",
                type: "integer",
                control: "spinBox",
                defaultValue: 25,
                min: 0,
                max: 200
            },
            {
                id: "hyprland.input.repeat_delay",
                type: "integer",
                control: "spinBox",
                defaultValue: 600,
                min: 0,
                max: 2000
            },
            {
                id: "hyprland.input.sensitivity",
                type: "number",
                control: "slider",
                defaultValue: 0,
                min: -1,
                max: 1
            },
            {
                id: "hyprland.input.accel_profile",
                type: "enum",
                control: "select",
                defaultValue: "",
                choices: [
                    { label: "automatic", value: "" },
                    { label: "adaptive", value: "adaptive" },
                    { label: "flat", value: "flat" }
                ]
            },
            {
                id: "hyprland.input.natural_scroll",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.left_handed",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.scroll_factor",
                type: "number",
                control: "slider",
                defaultValue: 1,
                min: 0,
                max: 2
            },
            {
                id: "hyprland.input.touchpad.tap-to-click",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.input.touchpad.tap-and-drag",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.input.touchpad.natural_scroll",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.touchpad.disable_while_typing",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.input.touchpad.scroll_factor",
                type: "number",
                control: "slider",
                defaultValue: 1,
                min: 0,
                max: 2
            },
            {
                id: "hyprland.input.scroll_method",
                type: "enum",
                control: "select",
                defaultValue: "",
                choices: [
                    { label: "automatic", value: "" },
                    { label: "two-finger", value: "2fg" },
                    { label: "edge", value: "edge" },
                    { label: "button", value: "on_button_down" },
                    { label: "disabled", value: "no_scroll" }
                ]
            },
            {
                id: "hyprland.input.scroll_button",
                type: "integer",
                control: "spinBox",
                defaultValue: 0,
                min: 0,
                max: 300
            },
            {
                id: "hyprland.input.scroll_button_lock",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.off_window_axis_events",
                type: "enum",
                control: "select",
                defaultValue: 1,
                min: 0,
                max: 3,
                choices: [
                    { label: "ignore", value: 0 },
                    { label: "send", value: 1 },
                    { label: "clamp", value: 2 },
                    { label: "warp", value: 3 }
                ]
            },
            {
                id: "hyprland.input.emulate_discrete_scroll",
                type: "enum",
                control: "select",
                defaultValue: 1,
                min: 0,
                max: 2,
                choices: [
                    { label: "disable", value: 0 },
                    { label: "non_standard", value: 1 },
                    { label: "force_all", value: 2 }
                ]
            },
            {
                id: "hyprland.input.touchpad.clickfinger_behavior",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.touchpad.drag_3fg",
                type: "enum",
                control: "select",
                defaultValue: 0,
                min: 0,
                max: 2,
                choices: [
                    { label: "disable", value: 0 },
                    { label: "3_finger", value: 1 },
                    { label: "4_finger", value: 2 }
                ]
            },
            {
                id: "hyprland.input.touchpad.drag_lock",
                type: "enum",
                control: "select",
                defaultValue: 0,
                min: 0,
                max: 2,
                choices: [
                    { label: "disabled", value: 0 },
                    { label: "enabled with timeout", value: 1 },
                    { label: "sticky", value: 2 }
                ]
            },
            {
                id: "hyprland.input.touchpad.flip_x",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.touchpad.flip_y",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.touchpad.middle_button_emulation",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.touchpad.tap_button_map",
                type: "enum",
                control: "select",
                defaultValue: "",
                choices: [
                    { label: "automatic", value: "" },
                    { label: "left-right-middle", value: "lrm" },
                    { label: "left-middle-right", value: "lmr" }
                ]
            },
            {
                id: "hyprland.input.numlock_by_default",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.virtualkeyboard.share_states",
                type: "enum",
                control: "select",
                defaultValue: 2,
                min: 0,
                max: 2,
                choices: [
                    { label: "disable", value: 0 },
                    { label: "enable", value: 1 },
                    { label: "only_non_ime", value: 2 }
                ]
            },
            {
                id: "hyprland.input.virtualkeyboard.release_pressed_on_close",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.misc.name_vk_after_proc",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.input.force_no_accel",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.rotation",
                type: "integer",
                control: "spinBox",
                defaultValue: 0,
                min: 0,
                max: 359
            },
            {
                id: "hyprland.misc.middle_click_paste",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.gestures.close_max_timeout",
                type: "integer",
                control: "spinBox",
                defaultValue: 1000,
                min: 10,
                max: 2000
            },
            {
                id: "hyprland.input.touchdevice.enabled",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.input.touchdevice.transform",
                type: "integer",
                control: "spinBox",
                defaultValue: 0,
                min: 0,
                max: 6
            },
            {
                id: "hyprland.input.tablet.relative_input",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.tablet.left_handed",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.tablet.transform",
                type: "integer",
                control: "spinBox",
                defaultValue: 0,
                min: 0,
                max: 6
            },
            {
                id: "hyprland.cursor.hide_on_key_press",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.cursor.hide_on_touch",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.cursor.hide_on_tablet",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.cursor.inactive_timeout",
                type: "number",
                control: "slider",
                defaultValue: 0,
                min: 0,
                max: 20
            },
            {
                id: "hyprland.cursor.hotspot_padding",
                type: "integer",
                control: "spinBox",
                defaultValue: 0,
                min: 0,
                max: 20
            },
            {
                id: "hyprland.cursor.no_warps",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.cursor.persistent_warps",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.cursor.warp_back_after_non_mouse_input",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.tablet.region_position",
                type: "vector2",
                control: "vector2",
                defaultValue: [0, 0],
                min: [-20000, -20000],
                max: [20000, 20000]
            },
            {
                id: "hyprland.input.tablet.absolute_region_position",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.input.tablet.region_size",
                type: "vector2",
                control: "vector2",
                defaultValue: [0, 0],
                min: [-100, -100],
                max: [4000, 4000]
            },
            {
                id: "hyprland.input.resolve_binds_by_sym",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            }
        ];
    }

    function inputDefaults() {
        return {
            "hyprland.input.repeat_rate": 25,
            "hyprland.input.repeat_delay": 600,
            "hyprland.input.sensitivity": 0,
            "hyprland.input.accel_profile": "",
            "hyprland.input.natural_scroll": false,
            "hyprland.input.left_handed": false,
            "hyprland.input.scroll_factor": 1,
            "hyprland.input.touchpad.tap-to-click": true,
            "hyprland.input.touchpad.tap-and-drag": true,
            "hyprland.input.touchpad.natural_scroll": false,
            "hyprland.input.touchpad.disable_while_typing": true,
            "hyprland.input.touchpad.scroll_factor": 1,
            "hyprland.input.scroll_method": "",
            "hyprland.input.scroll_button": 0,
            "hyprland.input.scroll_button_lock": false,
            "hyprland.input.off_window_axis_events": 1,
            "hyprland.input.emulate_discrete_scroll": 1,
            "hyprland.input.touchpad.clickfinger_behavior": false,
            "hyprland.input.touchpad.drag_3fg": 0,
            "hyprland.input.touchpad.drag_lock": 0,
            "hyprland.input.touchpad.flip_x": false,
            "hyprland.input.touchpad.flip_y": false,
            "hyprland.input.touchpad.middle_button_emulation": false,
            "hyprland.input.touchpad.tap_button_map": "",
            "hyprland.input.numlock_by_default": false,
            "hyprland.input.virtualkeyboard.share_states": 2,
            "hyprland.input.virtualkeyboard.release_pressed_on_close": false,
            "hyprland.misc.name_vk_after_proc": true,
            "hyprland.input.force_no_accel": false,
            "hyprland.input.rotation": 0,
            "hyprland.misc.middle_click_paste": true,
            "hyprland.gestures.close_max_timeout": 1000,
            "hyprland.input.touchdevice.enabled": true,
            "hyprland.input.touchdevice.transform": 0,
            "hyprland.input.tablet.relative_input": false,
            "hyprland.input.tablet.left_handed": false,
            "hyprland.input.tablet.transform": 0,
            "hyprland.cursor.hide_on_key_press": false,
            "hyprland.cursor.hide_on_touch": true,
            "hyprland.cursor.hide_on_tablet": false,
            "hyprland.cursor.inactive_timeout": 0,
            "hyprland.cursor.hotspot_padding": 0,
            "hyprland.cursor.no_warps": false,
            "hyprland.cursor.persistent_warps": false,
            "hyprland.cursor.warp_back_after_non_mouse_input": false,
            "hyprland.input.tablet.region_position": [0, 0],
            "hyprland.input.tablet.absolute_region_position": false,
            "hyprland.input.tablet.region_size": [0, 0],
            "hyprland.input.resolve_binds_by_sym": false
        };
    }

    function inputGestureActions() {
        return [
            { id: "close", label: "Close", description: "Close a window." },
            {
                id: "cursorZoom",
                label: "Cursor zoom",
                description: "Control cursor-centered zoom."
            },
            { id: "float", label: "Float", description: "Change floating state." },
            {
                id: "fullscreen",
                label: "Fullscreen",
                description: "Change fullscreen state."
            },
            { id: "move", label: "Move", description: "Move a window." },
            { id: "resize", label: "Resize", description: "Resize a window." },
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

    function inputGestureCompatibilityFor(gestures, compatibilityById) {
        const rows = [];
        const overrides = compatibilityById || ({});
        for (const gesture of gestures) {
            const override = overrides[gesture.id];
            rows.push(override || {
                id: gesture.id,
                editable: true,
                reason: ""
            });
        }
        return rows;
    }

    function gestureRecord(id, fingers, direction, modifiers, scale,
                           disableInhibit, action) {
        return {
            id: id,
            fingers: fingers,
            direction: direction,
            modifiers: modifiers,
            scale: scale,
            disableInhibit: disableInhibit,
            action: action
        };
    }

    function compatibilityGestureFixture() {
        return [
            gestureRecord(
                "gesture-1", 4, "pinch", [], 1.5, false,
                { type: "workspace" }
            ),
            gestureRecord(
                "authored-row", 2, "left", ["super"], 1, false,
                { type: "move" }
            ),
            gestureRecord(
                "unset-base", 3, "right", [], 1, false,
                { type: "workspace" }
            ),
            gestureRecord(
                "unset-row", 3, "right", [], 1, false,
                { type: "unset" }
            ),
            gestureRecord(
                "scroll-pinch", 5, "pinch", [], 1, false,
                { type: "scrollMove" }
            ),
            gestureRecord(
                "live-swipe", 6, "left", [], 1, false,
                { type: "cursorZoom", zoomLevel: 2, mode: "live" }
            )
        ];
    }

    function compatibilityRowsForFixture(gestures) {
        return inputGestureCompatibilityFor(gestures, {
            "gesture-1": {
                id: "gesture-1",
                editable: false,
                reason: "Pinch scale is retained exactly for compatibility."
            },
            "unset-row": {
                id: "unset-row",
                editable: false,
                reason: "Unset records are retained exactly for compatibility."
            },
            "scroll-pinch": {
                id: "scroll-pinch",
                editable: false,
                reason: "Scroll Move with pinch is inert in this runtime."
            },
            "live-swipe": {
                id: "live-swipe",
                editable: false,
                reason: "Live cursor zoom requires a pinch direction."
            }
        });
    }

    function configureInputPage(page, values, gestures, compatibility) {
        const gestureRecords = gestures === undefined ? [] : gestures;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.inputProjectionAvailable = true;
        page.inputAvailable = true;
        page.inputGesturesProjectionAvailable = true;
        page.inputGestures = gestureRecords;
        page.inputGestureCompatibility = compatibility === undefined
            ? inputGestureCompatibilityFor(gestureRecords) : compatibility;
        page.inputGestureActions = inputGestureActions();
        page.busy = false;
        page.busyOperation = "";
        page.inputOptions = inputDefinitions();
        page.inputValues = values || inputDefaults();
        page.revisionToken = "7";
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.inputErrorName = "";
        page.inputErrorMessage = "";
        page.sharedErrorName = "";
        page.sharedErrorMessage = "";
        page.retryApplyAvailable = false;
        page.recoveryAvailable = false;
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.reviewProjection();
    }

    function configureInputDeviceInventory(page) {
        page.inputDeviceDiscoveryAvailable = true;
        page.inputDeviceDiscoveryBusy = false;
        page.inputDevicesObservedAtMs = 1800000000000;
        page.inputDeviceInventoryDigest = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.inputDeviceUnaddressableCounts = {
            switches: 1,
            tabletPads: 2,
            tabletTools: 3
        };
        page.inputDeviceDiscoveryErrorName = "";
        page.inputDeviceDiscoveryErrorMessage = "";
        page.inputDeviceProjectionAvailable = true;
        page.inputDeviceProjectionRevisionToken = "7";
        page.inputDeviceProjectionInventoryDigest = page.inputDeviceInventoryDigest;
        page.inputDeviceProjectionErrorName = "";
        page.inputDeviceProjectionErrorMessage = "";
        page.connectedInputDevices = [
            {
                sessionSelector: "exact-keyboard",
                observedKind: "keyboard",
                activeKeymap: "English (US)",
                savedSettingsState: "matched",
                savedDeviceId: "device:keyboard",
                configuredKind: "keyboard",
                configuredEnabled: true,
                overrideCount: 2
            },
            {
                sessionSelector: "pointing-device-with-a-very-long-session-selector-that-must-wrap-without-horizontal-overflow",
                observedKind: "pointer",
                activeKeymap: null,
                savedSettingsState: "not-saved",
                savedDeviceId: null,
                configuredKind: null,
                configuredEnabled: null,
                overrideCount: null
            },
            {
                sessionSelector: "touch-one",
                observedKind: "touch",
                activeKeymap: null,
                savedSettingsState: "unavailable",
                savedDeviceId: null,
                configuredKind: null,
                configuredEnabled: null,
                overrideCount: null
            },
            {
                sessionSelector: "tablet-one",
                observedKind: "tablet",
                activeKeymap: null,
                savedSettingsState: "kind-mismatch",
                savedDeviceId: "device:tablet",
                configuredKind: "keyboard",
                configuredEnabled: false,
                overrideCount: 1
            }
        ];
        page.savedInputDevices = [
            {
                id: "device:keyboard",
                selector: "exact keyboard",
                configuredKind: "keyboard",
                configuredEnabled: true,
                overrideCount: 2,
                matchState: "observed",
                observedKind: "keyboard"
            },
            {
                id: "device:offline",
                selector: "saved device with a very long exact selector that is preserved byte-for-byte",
                configuredKind: "touchpad",
                configuredEnabled: false,
                overrideCount: 4,
                matchState: "not-observed",
                observedKind: null
            }
        ];
        page.otherSavedInputDevices = [page.savedInputDevices[1]];
    }

    function booleanDefinition(id, defaultValue) {
        return {
            id: id,
            type: "boolean",
            control: "toggle",
            defaultValue: defaultValue
        };
    }

    function integerDefinition(id, defaultValue, minimum, maximum) {
        return {
            id: id,
            type: "integer",
            control: "spinBox",
            defaultValue: defaultValue,
            min: minimum,
            max: maximum
        };
    }

    function numberDefinition(id, defaultValue, minimum, maximum) {
        return {
            id: id,
            type: "number",
            control: "slider",
            defaultValue: defaultValue,
            min: minimum,
            max: maximum
        };
    }

    function stringDefinition(id, defaultValue, maximumLength) {
        return {
            id: id,
            type: "string",
            control: "text",
            defaultValue: defaultValue,
            maxLength: maximumLength
        };
    }

    function fontWeightDefinition(id, defaultValue) {
        return {
            id: id,
            type: "fontWeight",
            control: "spinBox",
            defaultValue: defaultValue,
            min: 0,
            max: 2147483647
        };
    }

    function enumDefinition(id, defaultValue, values, minimum, maximum) {
        const option = {
            id: id,
            type: "enum",
            control: "select",
            defaultValue: defaultValue,
            choices: values.map(function(value) {
                return { label: String(value), value: value };
            })
        };
        if (minimum !== undefined)
            option.min = minimum;
        if (maximum !== undefined)
            option.max = maximum;
        return option;
    }

    function cssGapDefinition(id, defaultValue) {
        return {
            id: id,
            type: "cssGap",
            control: "text",
            defaultValue: defaultValue
        };
    }

    function vectorDefinition(id, defaultValue, minimum, maximum) {
        return {
            id: id,
            type: "vector2",
            control: "vector2",
            defaultValue: defaultValue,
            min: minimum,
            max: maximum
        };
    }

    function windowsDefinitions() {
        return [
            {
                id: "hyprland.general.layout",
                type: "enum",
                control: "select",
                defaultValue: "dwindle",
                choices: [
                    { label: "dwindle", value: "dwindle" },
                    { label: "master", value: "master" },
                    { label: "scrolling", value: "scrolling" },
                    { label: "monocle", value: "monocle" }
                ]
            },
            {
                id: "hyprland.general.resize_on_border",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.general.extend_border_grab_area",
                type: "integer",
                control: "spinBox",
                defaultValue: 15,
                min: 0,
                max: 100
            },
            {
                id: "hyprland.general.hover_icon_on_border",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.general.resize_corner",
                type: "enum",
                control: "select",
                defaultValue: 0,
                min: 0,
                max: 4,
                choices: [
                    { label: "disable", value: 0 },
                    { label: "top_left", value: 1 },
                    { label: "top_right", value: 2 },
                    { label: "bottom_right", value: 3 },
                    { label: "bottom_left", value: 4 }
                ]
            },
            {
                id: "hyprland.general.snap.enabled",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.general.snap.border_overlap",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.general.snap.monitor_gap",
                type: "integer",
                control: "spinBox",
                defaultValue: 10,
                min: 0,
                max: 100
            },
            {
                id: "hyprland.general.snap.respect_gaps",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.general.snap.window_gap",
                type: "integer",
                control: "spinBox",
                defaultValue: 10,
                min: 0,
                max: 100
            },
            {
                id: "hyprland.input.follow_mouse",
                type: "enum",
                control: "select",
                defaultValue: 1,
                min: 0,
                max: 3,
                choices: [
                    { label: "disabled", value: 0 },
                    { label: "follow", value: 1 },
                    { label: "detached", value: 2 },
                    { label: "separate", value: 3 }
                ]
            },
            {
                id: "hyprland.input.mouse_refocus",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.input.follow_mouse_shrink",
                type: "integer",
                control: "spinBox",
                defaultValue: 0,
                min: 0,
                max: 300
            },
            {
                id: "hyprland.input.float_switch_override_focus",
                type: "enum",
                control: "select",
                defaultValue: 1,
                min: 0,
                max: 2,
                choices: [
                    { label: "disabled", value: 0 },
                    {
                        label: "tiled/floating transitions",
                        value: 1
                    },
                    { label: "all floating transitions", value: 2 }
                ]
            },
            {
                id: "hyprland.input.focus_on_close",
                type: "enum",
                control: "select",
                defaultValue: 0,
                min: 0,
                max: 2,
                choices: [
                    { label: "next", value: 0 },
                    { label: "cursor", value: 1 },
                    { label: "mru", value: 2 }
                ]
            },
            {
                id: "hyprland.input.special_fallthrough",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.general.no_focus_fallback",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.general.modal_parent_blocking",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            cssGapDefinition("hyprland.general.float_gaps", [0, 0, 0, 0]),
            integerDefinition("hyprland.general.gaps_workspaces", 0, 0, 100),
            vectorDefinition(
                "hyprland.layout.single_window_aspect_ratio",
                [0, 0], [0, 0], [1000, 1000]
            ),
            numberDefinition(
                "hyprland.layout.single_window_aspect_ratio_tolerance",
                0.1, 0, 1
            ),
            numberDefinition(
                "hyprland.dwindle.default_split_ratio", 1, 0.1, 1.9
            ),
            enumDefinition(
                "hyprland.dwindle.force_split", 0, [0, 1, 2], 0, 2
            ),
            booleanDefinition(
                "hyprland.dwindle.permanent_direction_override", false
            ),
            booleanDefinition("hyprland.dwindle.precise_mouse_move", false),
            booleanDefinition("hyprland.dwindle.preserve_split", false),
            booleanDefinition("hyprland.dwindle.smart_resizing", true),
            booleanDefinition("hyprland.dwindle.smart_split", false),
            numberDefinition(
                "hyprland.dwindle.special_scale_factor", 1, 0, 1
            ),
            enumDefinition(
                "hyprland.dwindle.split_bias", 0, [0, 1], 0, 1
            ),
            numberDefinition(
                "hyprland.dwindle.split_width_multiplier", 1, 0.1, 3
            ),
            booleanDefinition(
                "hyprland.dwindle.use_active_for_splits", true
            ),
            booleanDefinition("hyprland.master.allow_small_split", false),
            booleanDefinition("hyprland.master.always_keep_position", false),
            booleanDefinition("hyprland.master.center_ignores_reserved", false),
            enumDefinition(
                "hyprland.master.center_master_fallback", "left",
                ["left", "right", "top", "bottom"]
            ),
            booleanDefinition("hyprland.master.drop_at_cursor", true),
            booleanDefinition("hyprland.master.focus_master_on_close", false),
            numberDefinition("hyprland.master.mfact", 0.55, 0, 1),
            enumDefinition(
                "hyprland.master.new_on_active", "none",
                ["none", "before", "after"]
            ),
            booleanDefinition("hyprland.master.new_on_top", false),
            enumDefinition(
                "hyprland.master.new_status", "slave",
                ["master", "slave", "inherit"]
            ),
            enumDefinition(
                "hyprland.master.orientation", "left",
                ["left", "right", "top", "bottom", "center"]
            ),
            integerDefinition(
                "hyprland.master.slave_count_for_center_master", 2, 0, 10
            ),
            booleanDefinition("hyprland.master.smart_resizing", true),
            numberDefinition(
                "hyprland.master.special_scale_factor", 1, 0, 1
            ),
            numberDefinition("hyprland.scrolling.column_width", 0.5, 0.1, 1),
            enumDefinition(
                "hyprland.scrolling.direction", "right",
                ["left", "right", "up", "down"]
            ),
            enumDefinition(
                "hyprland.scrolling.focus_fit_method", 1, [0, 1], 0, 1
            ),
            booleanDefinition("hyprland.scrolling.follow_focus", true),
            numberDefinition(
                "hyprland.scrolling.follow_min_visible", 0.4, 0, 1
            ),
            booleanDefinition(
                "hyprland.scrolling.fullscreen_on_one_column", true
            ),
            booleanDefinition("hyprland.scrolling.wrap_focus", true),
            booleanDefinition("hyprland.scrolling.wrap_swapcol", true),
            booleanDefinition(
                "hyprland.gestures.scrolling.move_snap_cursor", true
            ),
            booleanDefinition(
                "hyprland.gestures.scrolling.move_snap_to_grid", true
            ),
            booleanDefinition("hyprland.group.auto_group", true, "advanced"),
            booleanDefinition(
                "hyprland.group.insert_after_current", true, "advanced"
            ),
            booleanDefinition(
                "hyprland.group.focus_removed_window", true, "advanced"
            ),
            enumDefinition(
                "hyprland.group.drag_into_group", 1, [0, 1, 2], 0, 2,
                "advanced"
            ),
            booleanDefinition(
                "hyprland.group.merge_groups_on_drag", true, "advanced"
            ),
            booleanDefinition(
                "hyprland.group.merge_groups_on_groupbar", true, "advanced"
            ),
            booleanDefinition(
                "hyprland.group.merge_floated_into_tiled_on_groupbar",
                false, "advanced"
            ),
            booleanDefinition(
                "hyprland.group.group_on_movetoworkspace", false,
                "advanced"
            ),
            booleanDefinition(
                "hyprland.group.groupbar.enabled", true
            ),
            booleanDefinition(
                "hyprland.group.groupbar.disable_when_only", false
            ),
            stringDefinition(
                "hyprland.group.groupbar.font_family", "", 4096
            ),
            fontWeightDefinition(
                "hyprland.group.groupbar.font_weight_active", 400
            ),
            fontWeightDefinition(
                "hyprland.group.groupbar.font_weight_inactive", 400
            ),
            integerDefinition(
                "hyprland.group.groupbar.font_size", 8, 2, 64
            ),
            booleanDefinition(
                "hyprland.group.groupbar.gradients", false
            ),
            integerDefinition(
                "hyprland.group.groupbar.height", 14, 1, 64
            ),
            integerDefinition(
                "hyprland.group.groupbar.indicator_gap", 0, 0, 64
            ),
            integerDefinition(
                "hyprland.group.groupbar.indicator_height", 3, 1, 64
            ),
            booleanDefinition(
                "hyprland.group.groupbar.stacked", false
            ),
            integerDefinition(
                "hyprland.group.groupbar.priority", 3, 0, 6
            ),
            booleanDefinition(
                "hyprland.group.groupbar.render_titles", true
            ),
            booleanDefinition(
                "hyprland.group.groupbar.scrolling", true
            ),
            booleanDefinition(
                "hyprland.group.groupbar.middle_click_close", true
            ),
            integerDefinition(
                "hyprland.group.groupbar.rounding", 1, 0, 20
            ),
            numberDefinition(
                "hyprland.group.groupbar.rounding_power", 2, 2, 10
            ),
            integerDefinition(
                "hyprland.group.groupbar.gradient_rounding", 2, 0, 20
            ),
            numberDefinition(
                "hyprland.group.groupbar.gradient_rounding_power", 2,
                2, 10
            ),
            booleanDefinition(
                "hyprland.group.groupbar.round_only_edges", true
            ),
            booleanDefinition(
                "hyprland.group.groupbar.gradient_round_only_edges", true
            ),
            integerDefinition(
                "hyprland.group.groupbar.gaps_out", 2, 0, 20
            ),
            integerDefinition(
                "hyprland.group.groupbar.gaps_in", 2, 0, 20
            ),
            booleanDefinition(
                "hyprland.group.groupbar.keep_upper_gap", true
            ),
            integerDefinition(
                "hyprland.group.groupbar.text_offset", 0, -20, 20
            ),
            integerDefinition(
                "hyprland.group.groupbar.text_padding", 0, 0, 22
            ),
            booleanDefinition(
                "hyprland.group.groupbar.blur", false
            ),
            booleanDefinition(
                "hyprland.binds.allow_pin_fullscreen", false, "advanced"
            ),
            enumDefinition(
                "hyprland.binds.focus_preferred_method", 0,
                [0, 1], 0, 1
            ),
            booleanDefinition(
                "hyprland.binds.ignore_group_lock", false, "advanced"
            ),
            booleanDefinition(
                "hyprland.binds.movefocus_cycles_fullscreen", false,
                "advanced"
            ),
            booleanDefinition(
                "hyprland.binds.movefocus_cycles_groupfirst", false,
                "advanced"
            ),
            booleanDefinition(
                "hyprland.binds.window_direction_monitor_fallback", true,
                "advanced"
            ),
            booleanDefinition(
                "hyprland.misc.enable_anr_dialog", true, "advanced"
            ),
            integerDefinition(
                "hyprland.misc.anr_missed_pings", 5, 1, 20, "advanced"
            ),
            booleanDefinition(
                "hyprland.misc.size_limits_tiled", false, "advanced"
            ),
            booleanDefinition(
                "hyprland.misc.always_follow_on_dnd", true, "advanced"
            ),
            booleanDefinition(
                "hyprland.misc.focus_on_activate", false, "advanced"
            ),
            booleanDefinition(
                "hyprland.misc.mouse_move_focuses_monitor", true,
                "advanced"
            ),
            enumDefinition(
                "hyprland.misc.on_focus_under_fullscreen", 2,
                [0, 1, 2], 0, 2, "advanced"
            ),
            booleanDefinition(
                "hyprland.misc.exit_window_retains_fullscreen", false,
                "advanced"
            ),
            booleanDefinition(
                "hyprland.misc.enable_swallow", false, "advanced"
            ),
            stringDefinition(
                "hyprland.misc.swallow_regex", "", 4096, "advanced"
            ),
            stringDefinition(
                "hyprland.misc.swallow_exception_regex", "", 4096,
                "advanced"
            ),
            {
                id: "hyprland.input.follow_mouse_threshold",
                type: "number",
                control: "slider",
                defaultValue: 0,
                min: 0,
                max: 1000000,
                risk: "safe"
            }
        ];
    }

    function windowsDefaults() {
        return {
            "hyprland.general.layout": "dwindle",
            "hyprland.general.resize_on_border": false,
            "hyprland.general.extend_border_grab_area": 15,
            "hyprland.general.hover_icon_on_border": true,
            "hyprland.general.resize_corner": 0,
            "hyprland.general.snap.enabled": false,
            "hyprland.general.snap.border_overlap": false,
            "hyprland.general.snap.monitor_gap": 10,
            "hyprland.general.snap.respect_gaps": false,
            "hyprland.general.snap.window_gap": 10,
            "hyprland.input.follow_mouse": 1,
            "hyprland.input.mouse_refocus": true,
            "hyprland.input.follow_mouse_shrink": 0,
            "hyprland.input.float_switch_override_focus": 1,
            "hyprland.input.focus_on_close": 0,
            "hyprland.input.special_fallthrough": false,
            "hyprland.general.no_focus_fallback": false,
            "hyprland.general.modal_parent_blocking": true,
            "hyprland.general.float_gaps": [0, 0, 0, 0],
            "hyprland.general.gaps_workspaces": 0,
            "hyprland.layout.single_window_aspect_ratio": [0, 0],
            "hyprland.layout.single_window_aspect_ratio_tolerance": 0.1,
            "hyprland.dwindle.default_split_ratio": 1,
            "hyprland.dwindle.force_split": 0,
            "hyprland.dwindle.permanent_direction_override": false,
            "hyprland.dwindle.precise_mouse_move": false,
            "hyprland.dwindle.preserve_split": false,
            "hyprland.dwindle.smart_resizing": true,
            "hyprland.dwindle.smart_split": false,
            "hyprland.dwindle.special_scale_factor": 1,
            "hyprland.dwindle.split_bias": 0,
            "hyprland.dwindle.split_width_multiplier": 1,
            "hyprland.dwindle.use_active_for_splits": true,
            "hyprland.master.allow_small_split": false,
            "hyprland.master.always_keep_position": false,
            "hyprland.master.center_ignores_reserved": false,
            "hyprland.master.center_master_fallback": "left",
            "hyprland.master.drop_at_cursor": true,
            "hyprland.master.focus_master_on_close": false,
            "hyprland.master.mfact": 0.55,
            "hyprland.master.new_on_active": "none",
            "hyprland.master.new_on_top": false,
            "hyprland.master.new_status": "slave",
            "hyprland.master.orientation": "left",
            "hyprland.master.slave_count_for_center_master": 2,
            "hyprland.master.smart_resizing": true,
            "hyprland.master.special_scale_factor": 1,
            "hyprland.scrolling.column_width": 0.5,
            "hyprland.scrolling.direction": "right",
            "hyprland.scrolling.focus_fit_method": 1,
            "hyprland.scrolling.follow_focus": true,
            "hyprland.scrolling.follow_min_visible": 0.4,
            "hyprland.scrolling.fullscreen_on_one_column": true,
            "hyprland.scrolling.wrap_focus": true,
            "hyprland.scrolling.wrap_swapcol": true,
            "hyprland.gestures.scrolling.move_snap_cursor": true,
            "hyprland.gestures.scrolling.move_snap_to_grid": true,
            "hyprland.group.auto_group": true,
            "hyprland.group.insert_after_current": true,
            "hyprland.group.focus_removed_window": true,
            "hyprland.group.drag_into_group": 1,
            "hyprland.group.merge_groups_on_drag": true,
            "hyprland.group.merge_groups_on_groupbar": true,
            "hyprland.group.merge_floated_into_tiled_on_groupbar": false,
            "hyprland.group.group_on_movetoworkspace": false,
            "hyprland.group.groupbar.enabled": true,
            "hyprland.group.groupbar.disable_when_only": false,
            "hyprland.group.groupbar.font_family": "",
            "hyprland.group.groupbar.font_weight_active": 400,
            "hyprland.group.groupbar.font_weight_inactive": 400,
            "hyprland.group.groupbar.font_size": 8,
            "hyprland.group.groupbar.gradients": false,
            "hyprland.group.groupbar.height": 14,
            "hyprland.group.groupbar.indicator_gap": 0,
            "hyprland.group.groupbar.indicator_height": 3,
            "hyprland.group.groupbar.stacked": false,
            "hyprland.group.groupbar.priority": 3,
            "hyprland.group.groupbar.render_titles": true,
            "hyprland.group.groupbar.scrolling": true,
            "hyprland.group.groupbar.middle_click_close": true,
            "hyprland.group.groupbar.rounding": 1,
            "hyprland.group.groupbar.rounding_power": 2,
            "hyprland.group.groupbar.gradient_rounding": 2,
            "hyprland.group.groupbar.gradient_rounding_power": 2,
            "hyprland.group.groupbar.round_only_edges": true,
            "hyprland.group.groupbar.gradient_round_only_edges": true,
            "hyprland.group.groupbar.gaps_out": 2,
            "hyprland.group.groupbar.gaps_in": 2,
            "hyprland.group.groupbar.keep_upper_gap": true,
            "hyprland.group.groupbar.text_offset": 0,
            "hyprland.group.groupbar.text_padding": 0,
            "hyprland.group.groupbar.blur": false,
            "hyprland.binds.allow_pin_fullscreen": false,
            "hyprland.binds.focus_preferred_method": 0,
            "hyprland.binds.ignore_group_lock": false,
            "hyprland.binds.movefocus_cycles_fullscreen": false,
            "hyprland.binds.movefocus_cycles_groupfirst": false,
            "hyprland.binds.window_direction_monitor_fallback": true,
            "hyprland.misc.enable_anr_dialog": true,
            "hyprland.misc.anr_missed_pings": 5,
            "hyprland.misc.size_limits_tiled": false,
            "hyprland.misc.always_follow_on_dnd": true,
            "hyprland.misc.focus_on_activate": false,
            "hyprland.misc.mouse_move_focuses_monitor": true,
            "hyprland.misc.on_focus_under_fullscreen": 2,
            "hyprland.misc.exit_window_retains_fullscreen": false,
            "hyprland.misc.enable_swallow": false,
            "hyprland.misc.swallow_regex": "",
            "hyprland.misc.swallow_exception_regex": "",
            "hyprland.input.follow_mouse_threshold": 0
        };
    }

    function configureWindowsPage(page, values) {
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.windowsProjectionAvailable = true;
        page.windowsAvailable = true;
        page.busy = false;
        page.busyOperation = "";
        page.windowsOptions = windowsDefinitions();
        page.windowsValues = values || windowsDefaults();
        page.revisionToken = "7";
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.windowsErrorName = "";
        page.windowsErrorMessage = "";
        page.sharedErrorName = "";
        page.sharedErrorMessage = "";
        page.retryApplyAvailable = false;
        page.recoveryAvailable = false;
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.reviewProjection();
    }

    function workspacesDefinitions() {
        return [
            booleanDefinition(
                "hyprland.animations.workspace_wraparound", false
            ),
            numberDefinition(
                "hyprland.gestures.workspace_swipe_cancel_ratio",
                0.5, 0, 1
            ),
            booleanDefinition(
                "hyprland.gestures.workspace_swipe_create_new", true
            ),
            booleanDefinition(
                "hyprland.gestures.workspace_swipe_direction_lock", true
            ),
            integerDefinition(
                "hyprland.gestures.workspace_swipe_direction_lock_threshold",
                10, 0, 200
            ),
            integerDefinition(
                "hyprland.gestures.workspace_swipe_distance", 300, 0, 2000
            ),
            booleanDefinition(
                "hyprland.gestures.workspace_swipe_forever", false
            ),
            booleanDefinition(
                "hyprland.gestures.workspace_swipe_invert", true
            ),
            integerDefinition(
                "hyprland.gestures.workspace_swipe_min_speed_to_force",
                30, 0, 200
            ),
            booleanDefinition(
                "hyprland.gestures.workspace_swipe_touch", false
            ),
            booleanDefinition(
                "hyprland.gestures.workspace_swipe_touch_invert", false
            ),
            booleanDefinition(
                "hyprland.gestures.workspace_swipe_use_r", false
            ),
            booleanDefinition("hyprland.misc.close_special_on_empty", true),
            enumDefinition(
                "hyprland.misc.initial_workspace_tracking",
                1, [0, 1, 2], 0, 2
            ),
            integerDefinition(
                "hyprland.misc.initial_workspace_token_timeout", 10, 1, 3600
            ),
            booleanDefinition(
                "hyprland.binds.allow_workspace_cycles", false
            ),
            booleanDefinition(
                "hyprland.binds.hide_special_on_workspace_change", false
            ),
            booleanDefinition(
                "hyprland.binds.workspace_back_and_forth", false
            ),
            enumDefinition(
                "hyprland.binds.workspace_center_on",
                1, [0, 1], 0, 1
            ),
            enumDefinition(
                "hyprland.cursor.warp_on_change_workspace",
                0, [0, 1, 2], 0, 2
            ),
            enumDefinition(
                "hyprland.cursor.warp_on_toggle_special",
                0, [0, 1, 2], 0, 2
            )
        ];
    }

    function workspacesDefaults() {
        return {
            "hyprland.animations.workspace_wraparound": false,
            "hyprland.gestures.workspace_swipe_cancel_ratio": 0.5,
            "hyprland.gestures.workspace_swipe_create_new": true,
            "hyprland.gestures.workspace_swipe_direction_lock": true,
            "hyprland.gestures.workspace_swipe_direction_lock_threshold": 10,
            "hyprland.gestures.workspace_swipe_distance": 300,
            "hyprland.gestures.workspace_swipe_forever": false,
            "hyprland.gestures.workspace_swipe_invert": true,
            "hyprland.gestures.workspace_swipe_min_speed_to_force": 30,
            "hyprland.gestures.workspace_swipe_touch": false,
            "hyprland.gestures.workspace_swipe_touch_invert": false,
            "hyprland.gestures.workspace_swipe_use_r": false,
            "hyprland.misc.close_special_on_empty": true,
            "hyprland.misc.initial_workspace_tracking": 1,
            "hyprland.misc.initial_workspace_token_timeout": 10,
            "hyprland.binds.allow_workspace_cycles": false,
            "hyprland.binds.hide_special_on_workspace_change": false,
            "hyprland.binds.workspace_back_and_forth": false,
            "hyprland.binds.workspace_center_on": 1,
            "hyprland.cursor.warp_on_change_workspace": 0,
            "hyprland.cursor.warp_on_toggle_special": 0
        };
    }

    function configureWorkspacesPage(page, values) {
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.workspacesProjectionAvailable = true;
        page.workspaceRulesProjectionAvailable = true;
        page.workspacesAvailable = true;
        page.busy = false;
        page.busyOperation = "";
        page.workspacesOptions = workspacesDefinitions();
        page.workspacesValues = values || workspacesDefaults();
        page.workspaceRules = [];
        page.revisionToken = "7";
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.workspacesErrorName = "";
        page.workspacesErrorMessage = "";
        page.sharedErrorName = "";
        page.sharedErrorMessage = "";
        page.retryApplyAvailable = false;
        page.recoveryAvailable = false;
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.reviewProjection();
    }

    function configureRulesStatusPage(page) {
        page.rulesProjectionAvailable = false;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.rulesAvailable = true;
        page.busy = false;
        page.busyOperation = "";
        page.windowRules = [];
        page.layerRules = [];
        page.revisionToken = "7";
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.rulesErrorName = "";
        page.rulesErrorMessage = "";
        page.sharedErrorName = "";
        page.sharedErrorMessage = "";
        page.retryApplyAvailable = false;
        page.recoveryAvailable = false;
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.rulesProjectionAvailable = true;
        page.reviewProjection();
    }

    function workspaceRule(id, selector, overrides) {
        return {
            id: id,
            selector: selector,
            enabled: false,
            monitor: "desc:Workspace display",
            persistent: true,
            isDefault: false,
            layout: "scrolling",
            overrides: overrides || {}
        };
    }

    function displayRecord(id, selector, enabled, mirror, vrr) {
        return {
            id: id,
            selector: selector,
            enabled: enabled,
            mode: "1920x1080@60",
            position: "0x0",
            scale: 1,
            reserved: [0, 0, 0, 0],
            transform: 0,
            mirror: mirror || "",
            bitdepth: 8,
            cm: "auto",
            sdrEotf: "default",
            sdrBrightness: 1,
            sdrSaturation: 1,
            vrr: vrr === undefined ? -1 : vrr,
            icc: "",
            supportsWideColor: -1,
            supportsHdr: -1,
            sdrMinLuminance: 0.2,
            sdrMaxLuminance: 80,
            minLuminance: -1,
            maxLuminance: -1,
            maxAvgLuminance: -1
        };
    }

    function connectedDisplay(selector, enabled, x, format, sdrMinimum) {
        return {
            selector: selector,
            description: selector + " test display",
            make: "Example",
            model: "Panel",
            serial: selector + "-serial",
            enabled: enabled,
            width: 1920,
            height: 1080,
            physicalWidthMm: 520,
            physicalHeightMm: 290,
            refreshRate: 60,
            x: x || 0,
            y: 0,
            scale: 1,
            transform: 0,
            focused: x === 0,
            dpms: true,
            vrrActive: false,
            mirrorOf: "",
            modes: [{
                width: 1920,
                height: 1080,
                refreshRate: 60,
                managedMode: "1920x1080@60"
            }],
            colorManagement: "srgb",
            currentFormat: format || "XRGB8888",
            sdrBrightness: 1,
            sdrSaturation: 1,
            sdrMinLuminance: sdrMinimum === undefined ? 0.2 : sdrMinimum,
            sdrMaxLuminance: 80
        };
    }

    function configureDisplaysPage(page, records, topology) {
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
        page.snapshot = { monitors: records };
        page.connectedDisplays = topology;
        page.topologyDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.synchronizeDraft(false);
    }

    function workspaceCatalogRecord() {
        return {
            id: "io.github.coastlinesec.hyprshelld.workspace-switcher",
            type: "bar-widget",
            version: "0.2.0",
            name: "Workspace Switcher",
            description: "Shows and activates workspaces on each display.",
            authors: [{ name: "CoastLineSec", email: "", homepage: "" }],
            license: "LicenseRef-HyprShelld",
            packageDigest:
                "4887e8c9e981ce892d39382e696de83d5b2dee4236e83db6da84780064aeaf54",
            origin: "system",
            removable: false,
            hasSettings: true,
            activationSupported: true,
            compatibilityReason: "",
            settingsDefinitions: [],
            requestedCapabilities: []
        };
    }

    function thirdPartyServiceRecord() {
        return {
            id: "org.example.local-service",
            type: "shell-service",
            version: "1.2.3",
            name: "Local Service",
            description: "A locally installed test service.",
            authors: [{ name: "Example Author" }],
            license: "MIT",
            packageDigest:
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            origin: "user",
            removable: true,
            hasSettings: true,
            activationSupported: false,
            compatibilityReason:
                "Runtime activation for third-party components is not available yet.",
            settingsDefinitions: [
                {
                    key: "logging",
                    scope: "component",
                    type: "boolean",
                    label: "Enable logging",
                    description: "Write diagnostic messages.",
                    group: "behavior",
                    order: 10,
                    defaultValue: false,
                    options: []
                },
                {
                    key: "mode",
                    scope: "component",
                    type: "enum",
                    label: "Logging mode",
                    description: "Choose how much information is recorded.",
                    group: "behavior",
                    order: 20,
                    defaultValue: "quiet",
                    options: [
                        { value: "quiet", label: "Quiet" },
                        {
                            value: "verbose",
                            label: "Verbose <img src=https://example.invalid/x>"
                        }
                    ],
                    visibleWhen: { key: "logging", equals: true }
                },
                {
                    key: "instanceTitle",
                    scope: "instance",
                    type: "string",
                    label: "Instance title",
                    description: "A title for one placement.",
                    group: "appearance",
                    order: 10,
                    defaultValue: "Widget",
                    minimumLength: 1,
                    maximumLength: 64,
                    options: []
                }
            ],
            requestedCapabilities: [
                { id: "example.read", reason: "Read example state." }
            ],
            dependencies: []
        };
    }

    function thirdPartyApplicationRecord() {
        const record = thirdPartyServiceRecord();
        record.id = "org.example.local-application";
        record.type = "shell-application";
        record.name = "Local Application";
        record.packageDigest =
            "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
        return record;
    }

    function thirdPartyDeclarativeWidgetRecord() {
        return {
            id: "org.example.clock-widget",
            type: "bar-widget",
            version: "1.0.0",
            name: "Clock Widget",
            description: "A data-only local clock label.",
            authors: [{ name: "Example Author" }],
            license: "MIT",
            packageDigest:
                "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
            origin: "user",
            removable: true,
            hasSettings: true,
            activationSupported: true,
            compatibilityReason: "",
            runtime: {
                kind: "declarative-v1",
                entrypoint: "payload/widget.json"
            },
            settingsDefinitions: [{
                key: "label",
                scope: "component",
                type: "string",
                label: "Label",
                description: "Text shown in the bar.",
                group: "appearance",
                order: 10,
                defaultValue: "Clock",
                minimumLength: 1,
                maximumLength: 32,
                options: []
            }],
            requestedCapabilities: [],
            dependencies: []
        };
    }

    function configureSnapshotForComponent(component, enabled) {
        const components = {};
        components[component.id] = {
            packageDigest: component.packageDigest,
            enabled: enabled,
            grantedCapabilities: [],
            settings: {}
        };
        return {
            formatVersion: 1,
            revision: "4",
            components: components,
            instances: {},
            layouts: { bars: {}, desktops: {} }
        };
    }

    function configureSnapshotForComponents(components, enabled) {
        const records = {};
        for (const component of components) {
            records[component.id] = {
                packageDigest: component.packageDigest,
                enabled: enabled,
                grantedCapabilities: [],
                settings: component.origin === "user"
                    ? { logging: false, mode: "quiet" } : {}
            };
        }
        return {
            formatVersion: 1,
            revision: "4",
            components: records,
            instances: {},
            layouts: { bars: {}, desktops: {} }
        };
    }

    function configureComponentsPage(page) {
        const component = workspaceCatalogRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [component];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        page.configSnapshot = configureSnapshotForComponent(component, true);
    }

    function test_serviceAvailability() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);

        const coreWarning = findChild(page, "coreServiceWarning");
        const componentWarning = findChild(
            page,
            "componentServiceWarning"
        );
        const control = findChild(page, "barHeightControl");
        const preview = findChild(page, "barPreview");
        const workspaceSettings = findChild(
            page,
            "workspaceSwitcherSettingsCard"
        );
        verify(coreWarning !== null);
        verify(componentWarning !== null);
        verify(control !== null);
        verify(preview !== null);
        verify(workspaceSettings !== null);
        compare(page.coreServiceWarningVisible, true);
        compare(page.componentServiceWarningVisible, true);
        compare(page.coreControlsEnabled, false);
        compare(page.workspaceControlsEnabled, false);
        compare(control.busy, true);
        compare(preview.barHeight, 40);
        compare(preview.configurationAvailable, false);
        compare(workspaceSettings.controlsEnabled, false);

        enableCoreSettings(page);
        compare(page.coreServiceWarningVisible, false);
        compare(page.componentServiceWarningVisible, true);
        compare(page.coreControlsEnabled, true);
        compare(page.workspaceControlsEnabled, false);
        compare(control.busy, false);
        compare(preview.configurationAvailable, true);

        enableWorkspaceSettings(page);
        compare(page.componentServiceWarningVisible, false);
        compare(page.workspaceControlsEnabled, true);
        compare(workspaceSettings.controlsEnabled, true);

        page.coreServiceAvailable = false;
        compare(page.coreControlsEnabled, false);
        compare(page.workspaceControlsEnabled, true);
        compare(control.enabled, false);
        compare(workspaceSettings.controlsEnabled, true);

        page.coreServiceAvailable = true;
        page.componentCatalogAvailable = false;
        compare(page.coreControlsEnabled, true);
        compare(page.workspaceControlsEnabled, false);
        compare(control.enabled, true);
        compare(workspaceSettings.controlsEnabled, false);
        verify(page.componentWarningMessage.includes("catalog"));

        page.componentRecoveryState = "unsupported";
        page.componentServiceAvailable = false;
        verify(page.componentWarningMessage.includes("cannot read"));
        compare(page.coreControlsEnabled, true);
        page.componentServiceAvailable = true;
        verify(page.componentWarningMessage.includes("recovery copy"));
        compare(page.workspaceControlsEnabled, false);
    }

    function test_recoveryMessages() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);

        const coreWarning = findChild(page, "coreRecoveryWarning");
        const componentWarning = findChild(
            page,
            "componentRecoveryWarning"
        );
        verify(coreWarning !== null);
        verify(componentWarning !== null);
        compare(page.coreRecoveryWarningVisible, false);
        compare(page.componentRecoveryWarningVisible, false);

        page.coreRecoveryState = "recovered";
        compare(page.coreRecoveryWarningVisible, true);
        verify(page.coreRecoveryMessage.includes("last known good"));
        compare(page.componentRecoveryWarningVisible, false);

        page.coreRecoveryState = "normal";
        page.componentRecoveryState = "defaulted";
        compare(page.coreRecoveryWarningVisible, false);
        compare(page.componentRecoveryWarningVisible, true);
        verify(page.componentRecoveryMessage.includes("safe defaults"));

        page.componentRecoveryState = "normal";
        compare(page.componentRecoveryWarningVisible, false);
    }

    function test_componentsPageUsesFixedCategoriesAndProtectedBuiltinRow() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureComponentsPage(page);
        waitForRendering(page);
        wait(0);

        const categories = [
            findChild(page, "componentCategory-bar-widget"),
            findChild(page, "componentCategory-desktop-widget"),
            findChild(page, "componentCategory-shell-service"),
            findChild(page, "componentCategory-shell-application")
        ];
        for (const category of categories)
            verify(category !== null);
        compare(categories.map(category => category.text), [
            "Bar Widgets",
            "Desktop Widgets",
            "Services",
            "Shell Applications"
        ]);
        for (let index = 1; index < categories.length; ++index) {
            verify(categories[index].mapToItem(page, 0, 0).y
                > categories[index - 1].mapToItem(page, 0, 0).y);
        }

        const componentId = workspaceCatalogRecord().id;
        const pill = findChild(page, "componentPill-" + componentId);
        const origin = findChild(page, "componentOrigin-" + componentId);
        const toggle = findChild(page, "componentEnabled-" + componentId);
        verify(pill !== null);
        verify(origin !== null);
        verify(toggle !== null);
        compare(origin.text, "Built-in");
        compare(toggle.checked, true);
        compare(toggle.enabled, true);
        compare(
            findChild(page, "componentSettings-" + componentId).visible,
            false
        );
        compare(
            findChild(page, "componentRemove-" + componentId).visible,
            false
        );
        const install = findChild(page, "installComponent");
        verify(install !== null);
        compare(install.enabled, true);

        let requested = [];
        let requestCount = 0;
        page.componentEnabledRequested.connect(function(
            id,
            packageDigest,
            enabled
        ) {
            ++requestCount;
            requested = [id, packageDigest, enabled];
        });
        toggle.checked = false;
        toggle.toggled();
        compare(requested, [
            componentId,
            workspaceCatalogRecord().packageDigest,
            false
        ]);
        compare(requestCount, 1);

        page.lastErrorComponentId = componentId;
        page.configError = "The package digest no longer matches.";
        wait(0);
        compare(toggle.checked, true);
        compare(requestCount, 1);

        page.lastErrorComponentId = "";
        page.configError = "";
        toggle.checked = false;
        toggle.toggled();
        compare(requestCount, 2);
        page.pendingComponentId = componentId;
        page.configBusy = true;
        wait(0);
        compare(toggle.checked, false);
        page.lastErrorComponentId = componentId;
        page.configError = "The change could not be saved.";
        page.pendingComponentId = "";
        page.configBusy = false;
        wait(0);
        compare(toggle.checked, true);
        compare(requestCount, 2);
    }

    function test_componentsToggleRequiresLiveDigestBoundState() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureComponentsPage(page);
        waitForRendering(page);
        wait(0);

        const component = workspaceCatalogRecord();
        const toggle = findChild(page, "componentEnabled-" + component.id);
        const status = findChild(page, "componentStatus-" + component.id);
        const warning = findChild(page, "componentsAvailabilityWarning");
        verify(toggle !== null);
        verify(status !== null);
        verify(warning !== null);
        compare(toggle.enabled, true);
        compare(warning.visible, false);

        page.managerBusy = true;
        compare(toggle.enabled, false);
        page.managerBusy = false;
        page.configBusy = true;
        compare(toggle.enabled, false);
        page.configBusy = false;

        page.configCatalogDigest =
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        compare(toggle.enabled, false);
        compare(warning.visible, true);
        verify(page.availabilityMessage.includes("both services agree"));

        page.configCatalogDigest = page.managerCatalogDigest;
        const mismatched = JSON.parse(JSON.stringify(page.configSnapshot));
        mismatched.components[component.id].packageDigest =
            "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
        page.configSnapshot = mismatched;
        compare(toggle.enabled, false);
        compare(status.text,
            "Configuration does not match the installed package.");

        page.configSnapshot = configureSnapshotForComponent(component, true);
        page.lastErrorComponentId = component.id;
        page.configError = "The enablement change could not be saved.";
        compare(status.text, "The enablement change could not be saved.");
        compare(status.Accessible.role, Accessible.AlertMessage);

        page.lastErrorComponentId = "org.example.some-other-component";
        compare(status.text, "Enabled");
    }

    function test_localPackagePickerAndReviewAreExplicit() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureComponentsPage(page);
        waitForRendering(page);
        wait(0);

        const install = findChild(page, "installComponent");
        verify(install !== null);

        let selectedUrl = "";
        page.inspectPackageRequested.connect(function(packageUrl) {
            selectedUrl = String(packageUrl);
        });
        page.inspectSelectedPackage(
            "file:///tmp/example.hyprshelld-component"
        );
        compare(
            selectedUrl,
            "file:///tmp/example.hyprshelld-component"
        );

        page.inspectionToken = "0123456789abcdef0123456789abcdef";
        wait(0);

        const review = findChild(page, "componentReviewDialog");
        verify(review !== null);
        compare(review.opened, false);

        page.inspectionReview = {
            operation: "install",
            id: "org.example.local-service",
            name: "Local Service",
            description: "A locally selected component.",
            version: "1.2.3",
            type: "shell-service",
            authors: [{ name: "Example Author" }],
            license: "MIT",
            runtime: { kind: "process-v1" },
            packageDigest:
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            activationSupported: false,
            compatibilityReason: "Activation is not available yet.",
            requestedCapabilities: [
                { id: "example.read", reason: "Read example state." }
            ],
            dependencies: []
        };
        wait(0);

        const warning = findChild(review, "componentUnverifiedWarning");
        const name = findChild(review, "componentReviewName");
        const capabilities = findChild(
            review,
            "componentReviewCapabilities"
        );
        const activation = findChild(
            review,
            "componentActivationNoticeText"
        );
        const confirm = findChild(
            review,
            "confirmComponentInstallation"
        );
        const cancel = findChild(
            review,
            "cancelComponentInstallation"
        );
        verify(review !== null);
        verify(warning !== null);
        verify(name !== null);
        verify(capabilities !== null);
        verify(activation !== null);
        verify(confirm !== null);
        verify(cancel !== null);
        compare(review.opened, true);
        compare(name.text, "Local Service");
        verify(capabilities.text.includes("example.read"));
        verify(activation.text.includes("cannot activate"));
        verify(activation.text.includes("does not change saved state"));
        const supportedReview = Object.assign({}, page.inspectionReview, {
            activationSupported: true,
            compatibilityReason: ""
        });
        page.inspectionReview = supportedReview;
        wait(0);
        verify(activation.text.includes("does not change saved enablement"));
        verify(activation.text.includes("exact version"));

        let installRequests = 0;
        page.installInspectedPackageRequested.connect(function() {
            ++installRequests;
            page.packageOperationBusy = true;
        });
        confirm.clicked();
        compare(installRequests, 1);
        compare(review.opened, true);
        compare(confirm.enabled, false);

        page.packageOperationBusy = false;
        page.inspectionToken = "";
        wait(0);
        compare(review.opened, false);

        page.inspectionToken = "0123456789abcdef0123456789abcdef";
        wait(0);
        compare(review.opened, true);

        let cancelRequests = 0;
        page.cancelInspectionRequested.connect(function() {
            ++cancelRequests;
        });
        cancel.clicked();
        compare(cancelRequests, 1);
    }

    function test_thirdPartyRowsExposeTrustedActionsOnly() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const service = thirdPartyServiceRecord();
        const application = thirdPartyApplicationRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [workspaceCatalogRecord(), service, application];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        page.configSnapshot = configureSnapshotForComponents(
            page.components,
            false
        );
        waitForRendering(page);
        wait(0);

        const trust = findChild(page, "componentTrust-" + service.id);
        const status = findChild(page, "componentStatus-" + service.id);
        const toggle = findChild(page, "componentEnabled-" + service.id);
        const configure = findChild(
            page,
            "componentSettings-" + service.id
        );
        const remove = findChild(page, "componentRemove-" + service.id);
        verify(trust !== null);
        verify(status !== null);
        verify(toggle !== null);
        verify(configure !== null);
        verify(remove !== null);
        compare(trust.text, "Unverified third-party code");
        verify(status.text.includes("Installed disabled"));
        verify(status.text.includes(service.compatibilityReason));
        compare(page.toggleAvailable(service), false);
        compare(toggle.enabled, false);
        compare(configure.visible, true);
        compare(remove.visible, true);
        compare(remove.enabled, true);
        page.managerAvailable = false;
        wait(0);
        compare(remove.enabled, false);
        page.managerAvailable = true;
        wait(0);
        compare(remove.enabled, true);
        compare(
            findChild(page, "componentSettings-" + application.id).visible,
            false
        );

        let removed = [];
        page.packageRemovalRequested.connect(function(
            componentId,
            packageDigest,
            catalogDigest
        ) {
            removed = [componentId, packageDigest, catalogDigest];
        });
        remove.clicked();
        wait(0);
        const removalDialog = findChild(page, "componentRemovalDialog");
        verify(removalDialog !== null);
        const confirmRemoval = findChild(
            removalDialog,
            "confirmComponentRemoval"
        );
        verify(confirmRemoval !== null);
        compare(removalDialog.opened, true);
        compare(removed.length, 0);
        confirmRemoval.clicked();
        compare(removed, [
            service.id,
            service.packageDigest,
            page.managerCatalogDigest
        ]);
        page.packageRemovalCompleted(service.id);
        wait(0);
        compare(removalDialog.opened, false);
    }

    function test_unsupportedComponentStaysDisabledAfterSettingsRecord() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const service = thirdPartyServiceRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [service];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        page.configSnapshot = {
            formatVersion: 1,
            revision: "4",
            components: {},
            instances: {},
            layouts: { bars: {}, desktops: {} }
        };
        waitForRendering(page);
        wait(0);

        const configure = findChild(
            page,
            "componentSettings-" + service.id
        );
        const toggle = findChild(
            page,
            "componentEnabled-" + service.id
        );
        const status = findChild(
            page,
            "componentStatus-" + service.id
        );
        verify(configure !== null);
        verify(toggle !== null);
        verify(status !== null);
        compare(configure.enabled, true);
        compare(page.configRecord(service), null);
        compare(page.toggleAvailable(service), false);
        compare(toggle.enabled, false);

        configure.clicked();
        wait(0);
        const form = findChild(page, "genericComponentSettings");
        verify(form !== null);
        const save = findChild(form, "saveGenericComponentSettings");
        const logging = findChild(
            form,
            "componentSettingBoolean-logging"
        );
        verify(save !== null);
        verify(logging !== null);
        logging.checked = true;
        logging.clicked();
        wait(0);
        compare(save.enabled, true);
        save.clicked();

        page.configSnapshot = configureSnapshotForComponent(service, false);
        wait(0);
        verify(page.configRecord(service) !== null);
        compare(page.toggleAvailable(service), false);
        compare(toggle.enabled, false);
        verify(status.text.includes("Installed disabled"));
        verify(status.text.includes(service.compatibilityReason));
    }

    function test_genericSettingsRenderTrustedComponentScope() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const service = thirdPartyServiceRecord();
        service.activationSupported = true;
        service.compatibilityReason = "";
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [service];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        page.configSnapshot = configureSnapshotForComponents([service], false);
        waitForRendering(page);
        wait(0);

        const configure = findChild(
            page,
            "componentSettings-" + service.id
        );
        verify(configure !== null);
        configure.clicked();
        wait(0);

        const dialog = findChild(page, "componentSettingsDialog");
        const form = findChild(page, "genericComponentSettings");
        verify(dialog !== null);
        verify(form !== null);
        const logging = findChild(
            form,
            "componentSettingBoolean-logging"
        );
        const modeRow = findChild(form, "componentSetting-mode");
        const mode = findChild(form, "componentSettingEnum-mode");
        const instanceNotice = findChild(
            form,
            "componentInstanceSettingsNotice"
        );
        const save = findChild(form, "saveGenericComponentSettings");
        verify(logging !== null);
        verify(modeRow !== null);
        verify(mode !== null);
        verify(instanceNotice !== null);
        verify(save !== null);
        compare(dialog.opened, true);
        compare(logging.checked, false);
        compare(modeRow.visible, false);
        compare(instanceNotice.visible, true);
        compare(save.enabled, false);
        compare(mode.contentItem.textFormat, Text.PlainText);

        logging.checked = true;
        logging.clicked();
        wait(0);
        compare(modeRow.visible, true);
        mode.popup.open();
        wait(0);
        const unsafeOption = mode.popup.contentItem.itemAtIndex(1);
        verify(unsafeOption !== null);
        compare(
            unsafeOption.text,
            "Verbose <img src=https://example.invalid/x>"
        );
        compare(unsafeOption.Accessible.name, unsafeOption.text);
        compare(unsafeOption.contentItem.textFormat, Text.PlainText);
        compare(unsafeOption.contentItem.Accessible.ignored, true);
        compare(
            unsafeOption.contentItem.text,
            "Verbose <img src=https://example.invalid/x>"
        );
        mode.popup.close();
        mode.activated(1);
        compare(save.enabled, true);

        let saved = [];
        page.componentSettingsRequested.connect(function(
            componentId,
            packageDigest,
            settings
        ) {
            saved = [componentId, packageDigest, settings];
        });
        save.clicked();
        compare(saved[0], service.id);
        compare(saved[1], service.packageDigest);
        compare(saved[2].logging, true);
        compare(saved[2].mode, "verbose");
        verify(!Object.prototype.hasOwnProperty.call(
            saved[2],
            "instanceTitle"
        ));
    }

    function test_compatibleWidgetUsesAtomicAddToBarThenGlobalToggle() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const widget = thirdPartyDeclarativeWidgetRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [widget];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        const instanceId = "d9a61b25-670b-44cb-a824-9e52772e79f1";
        page.configSnapshot = {
            formatVersion: 1,
            revision: "4",
            components: {},
            instances: {},
            layouts: { bars: {}, desktops: {} }
        };
        waitForRendering(page);
        wait(0);

        const add = findChild(page, "componentAddToBar-" + widget.id);
        const toggle = findChild(page, "componentEnabled-" + widget.id);
        const status = findChild(page, "componentStatus-" + widget.id);
        verify(add !== null);
        verify(toggle !== null);
        verify(status !== null);
        compare(add.visible, true);
        compare(add.enabled, true);
        compare(toggle.visible, false);
        verify(status.text.includes("Add it to the bar"));

        const configuredComponents = {};
        configuredComponents[widget.id] = {
            packageDigest: widget.packageDigest,
            enabled: false,
            grantedCapabilities: [],
            settings: { label: "Clock" }
        };
        const unplacedInstances = {};
        unplacedInstances[instanceId] = {
            componentId: widget.id,
            enabled: false,
            settings: {}
        };
        page.configSnapshot = {
            formatVersion: 1,
            revision: "5",
            components: configuredComponents,
            instances: unplacedInstances,
            layouts: { bars: {}, desktops: {} }
        };
        wait(0);
        compare(add.visible, true);
        compare(toggle.visible, false);
        compare(status.text, "Configured but not on the bar.");

        page.configSnapshot = {
            formatVersion: 1,
            revision: "6",
            components: configuredComponents,
            instances: unplacedInstances,
            layouts: {
                bars: {
                    secondary: {
                        outputs: { mode: "all" },
                        regions: {
                            start: [],
                            center: [],
                            end: [instanceId]
                        }
                    }
                },
                desktops: {}
            }
        };
        wait(0);
        compare(add.visible, true);
        compare(toggle.visible, false);
        compare(status.text, "Configured but not on the bar.");

        page.configSnapshot = {
            formatVersion: 1,
            revision: "7",
            components: configuredComponents,
            instances: unplacedInstances,
            layouts: {
                bars: {
                    main: {
                        outputs: { mode: "all" },
                        regions: {
                            start: [], center: [], end: [instanceId]
                        }
                    }
                },
                desktops: {}
            }
        };
        wait(0);
        compare(add.visible, true);
        compare(toggle.visible, false);
        compare(status.text, "Configured but not on the bar.");

        let request = [];
        page.componentAddToBarRequested.connect(function(
            componentId,
            packageDigest,
            settings
        ) {
            request = [componentId, packageDigest, settings];
        });
        add.clicked();
        compare(request[0], widget.id);
        compare(request[1], widget.packageDigest);
        compare(request[2].label, "Clock");

        const components = {};
        components[widget.id] = {
            packageDigest: widget.packageDigest,
            enabled: true,
            grantedCapabilities: [],
            settings: { label: "Clock" }
        };
        const instances = {};
        instances[instanceId] = {
            componentId: widget.id,
            enabled: true,
            settings: {}
        };
        page.configSnapshot = {
            formatVersion: 1,
            revision: "5",
            components: components,
            instances: instances,
            layouts: {
                bars: {
                    main: {
                        outputs: { mode: "all" },
                        regions: { start: [], center: [], end: [instanceId] }
                    }
                },
                desktops: {}
            }
        };
        wait(0);
        compare(add.visible, false);
        compare(toggle.visible, true);
        compare(toggle.checked, true);
        compare(status.text, "Enabled");

        const disabled = JSON.parse(JSON.stringify(page.configSnapshot));
        disabled.components[widget.id].enabled = false;
        page.configSnapshot = disabled;
        wait(0);
        compare(add.visible, false);
        compare(toggle.visible, true);
        compare(toggle.checked, false);
        compare(status.text, "Installed disabled. Review it before enabling.");
    }

    function test_updatedSchemaFreeWidgetRequiresExplicitInertAdoption() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const widget = thirdPartyDeclarativeWidgetRecord();
        widget.settingsDefinitions = [];
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [widget];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        const instanceId = "d9a61b25-670b-44cb-a824-9e52772e79f1";
        const oldDigest =
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        const components = {};
        components[widget.id] = {
            packageDigest: oldDigest,
            enabled: true,
            grantedCapabilities: ["old.permission"],
            settings: { oldValue: "preserved only until adoption" }
        };
        const instances = {};
        instances[instanceId] = {
            componentId: widget.id,
            enabled: true,
            settings: {}
        };
        page.configSnapshot = {
            formatVersion: 1,
            revision: "7",
            components: components,
            instances: instances,
            layouts: {
                bars: {
                    main: {
                        outputs: { mode: "all" },
                        regions: {
                            start: [], center: [], end: [instanceId]
                        }
                    }
                },
                desktops: {}
            }
        };
        waitForRendering(page);
        wait(0);

        const adopt = findChild(
            page,
            "componentAdoptPackage-" + widget.id
        );
        const add = findChild(page, "componentAddToBar-" + widget.id);
        const toggle = findChild(page, "componentEnabled-" + widget.id);
        const configure = findChild(
            page,
            "componentSettings-" + widget.id
        );
        const status = findChild(page, "componentStatus-" + widget.id);
        verify(adopt !== null);
        verify(add !== null);
        verify(toggle !== null);
        verify(configure !== null);
        verify(status !== null);
        compare(adopt.visible, true);
        compare(adopt.enabled, true);
        compare(adopt.text, "Use Installed Version");
        compare(add.visible, false);
        compare(toggle.visible, false);
        compare(configure.visible, false);
        verify(status.text.includes("different package version"));

        let adoption = [];
        page.componentAdoptionRequested.connect(function(
            componentId,
            packageDigest,
            settings
        ) {
            adoption = [componentId, packageDigest, settings];
        });
        adopt.clicked();
        compare(adoption[0], widget.id);
        compare(adoption[1], widget.packageDigest);
        compare(Object.keys(adoption[2]).length, 0);

        const adopted = JSON.parse(JSON.stringify(page.configSnapshot));
        adopted.revision = "8";
        adopted.components[widget.id] = {
            packageDigest: widget.packageDigest,
            enabled: false,
            grantedCapabilities: [],
            settings: {}
        };
        page.configSnapshot = adopted;
        wait(0);
        compare(adopt.visible, false);
        compare(add.visible, false);
        compare(toggle.visible, true);
        compare(toggle.checked, false);
        compare(status.text, "Installed disabled. Review it before enabling.");
    }

    function test_quarantinedWidgetOffersDigestBoundRetry() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const widget = thirdPartyDeclarativeWidgetRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [widget];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        const instanceId = "d9a61b25-670b-44cb-a824-9e52772e79f1";
        const placed = configureSnapshotForComponent(widget, true);
        placed.instances[instanceId] = {
            componentId: widget.id,
            enabled: true,
            settings: {}
        };
        placed.layouts.bars.main = {
            outputs: { mode: "all" },
            regions: { start: [], center: [], end: [instanceId] }
        };
        page.configSnapshot = placed;
        page.runtimeAvailable = true;
        page.runtimeStates = [{
            componentId: widget.id,
            packageDigest: widget.packageDigest,
            state: "quarantined",
            reason: "timeout",
            failureCount: 1
        }];
        waitForRendering(page);
        wait(0);

        const retry = findChild(page, "componentRetry-" + widget.id);
        const add = findChild(page, "componentAddToBar-" + widget.id);
        const status = findChild(page, "componentStatus-" + widget.id);
        verify(retry !== null);
        verify(add !== null);
        verify(status !== null);
        compare(add.visible, false);
        compare(retry.visible, true);
        compare(retry.enabled, true);
        verify(status.text.includes("activation did not complete"));
        verify(status.text.includes("did not stabilize"));

        let request = [];
        page.componentRetryRequested.connect(function(
            componentId,
            packageDigest
        ) {
            request = [componentId, packageDigest];
        });
        retry.clicked();
        compare(request, [widget.id, widget.packageDigest]);
        page.runtimeRetryBusyComponentId = widget.id;
        compare(retry.enabled, false);
    }

    function test_thirdPartyRuntimeSafeModeWarnsAndLocksActivation() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const widget = thirdPartyDeclarativeWidgetRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [widget];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        page.configSnapshot = configureSnapshotForComponent(widget, true);
        page.runtimeAvailable = true;
        page.thirdPartySafeMode = true;
        waitForRendering(page);
        wait(0);

        const warning = findChild(page, "componentRuntimeSafeModeWarning");
        const add = findChild(page, "componentAddToBar-" + widget.id);
        const toggle = findChild(page, "componentEnabled-" + widget.id);
        const retry = findChild(page, "componentRetry-" + widget.id);
        const status = findChild(page, "componentStatus-" + widget.id);
        verify(warning !== null);
        verify(add !== null);
        verify(toggle !== null);
        verify(retry !== null);
        verify(status !== null);
        compare(warning.visible, true);
        compare(add.visible, true);
        compare(add.enabled, false);
        compare(toggle.visible, false);
        compare(retry.visible, false);
        verify(status.text.includes("runtime safe mode"));

        const instanceId = "d9a61b25-670b-44cb-a824-9e52772e79f1";
        const placed = configureSnapshotForComponent(widget, true);
        placed.instances[instanceId] = {
            componentId: widget.id,
            enabled: true,
            settings: {}
        };
        placed.layouts.bars.main = {
            outputs: { mode: "all" },
            regions: { start: [], center: [], end: [instanceId] }
        };
        page.configSnapshot = placed;
        page.runtimeStates = [{
            componentId: widget.id,
            packageDigest: widget.packageDigest,
            state: "quarantined",
            reason: "Recovery data could not be trusted.",
            failureCount: 1
        }];
        wait(0);
        compare(add.visible, false);
        compare(toggle.visible, true);
        compare(toggle.enabled, false);
        compare(retry.visible, true);
        compare(retry.enabled, false);
        verify(status.text.includes("runtime safe mode"));

        page.thirdPartySafeMode = false;
        compare(warning.visible, false);
        compare(toggle.enabled, true);
        compare(retry.enabled, true);
    }

    function test_requestsAreForwardedOnce() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableCoreSettings(page);

        const control = findChild(page, "barHeightControl");
        verify(control !== null);

        let requestedHeight = 0;
        let heightRequestCount = 0;
        let resetRequestCount = 0;
        page.barHeightRequested.connect(function(height) {
            requestedHeight = height;
            ++heightRequestCount;
        });
        page.resetBarHeightRequested.connect(function() {
            ++resetRequestCount;
        });

        control.valueRequested(64);
        compare(requestedHeight, 64);
        compare(heightRequestCount, 1);

        control.resetRequested();
        compare(resetRequestCount, 1);
    }

    function test_sharedBorderControlsUseOneAtomicRequestAndRealPreview() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableCoreSettings(page);

        const enabledControl = findChild(page, "shellBorderEnabled");
        const widthControl = findChild(page, "shellBorderWidth");
        const radiusControl = findChild(page, "shellBorderRadius");
        const syncControl = findChild(
            page,
            "syncHyprlandWindowBorders"
        );
        const authorityMessage = findChild(
            page,
            "sharedBorderAuthorityMessage"
        );
        const enabledDescription = findChild(
            page,
            "shellBorderEnabledDescription"
        );
        const reset = findChild(page, "resetSharedBorder");
        const previewBar = findChild(page, "previewBarVisual");
        verify(enabledControl !== null);
        verify(widthControl !== null);
        verify(radiusControl !== null);
        verify(syncControl !== null);
        verify(authorityMessage !== null);
        verify(enabledDescription !== null);
        verify(reset !== null);
        verify(previewBar !== null);

        compare(enabledControl.checked, true);
        compare(widthControl.from, 0);
        compare(widthControl.to, 20);
        compare(widthControl.value, 1);
        compare(radiusControl.from, 0);
        compare(radiusControl.to, 20);
        compare(radiusControl.value, 15);
        compare(syncControl.checked, true);
        compare(reset.enabled, false);
        compare(
            enabledControl.Accessible.name,
            "Show shared border on the bar and synchronized windows"
        );
        verify(enabledDescription.text.includes(
            "while synced"
        ));
        verify(enabledDescription.text.includes(
            "kept when hidden"
        ));
        verify(authorityMessage.text.includes("HyprShelld controls"));
        verify(authorityMessage.text.includes("read-only"));
        compare(previewBar.shellBorderEnabled, true);
        compare(previewBar.shellBorderWidth, 1);
        compare(previewBar.shellBorderRadius, 15);

        let requestCount = 0;
        let request = [];
        let resetCount = 0;
        page.sharedBorderRequested.connect(function(
            enabled,
            width,
            radius,
            sync
        ) {
            ++requestCount;
            request = [enabled, width, radius, sync];
        });
        page.resetSharedBorderRequested.connect(function() {
            ++resetCount;
        });

        page.requestSharedBorder(true, 7, 15, true);
        compare(requestCount, 1);
        compare(request, [true, 7, 15, true]);
        compare(previewBar.shellBorderWidth, 7);

        page.requestSharedBorder(true, 7, 11, true);
        compare(requestCount, 2);
        compare(request, [true, 7, 11, true]);
        compare(previewBar.shellBorderRadius, 11);

        page.requestSharedBorder(false, 7, 11, true);
        compare(requestCount, 3);
        compare(request, [false, 7, 11, true]);
        compare(previewBar.shellBorderEnabled, false);
        compare(previewBar.renderedBorderWidth, 0);

        page.requestSharedBorder(false, 7, 11, false);
        compare(requestCount, 4);
        compare(request, [false, 7, 11, false]);
        verify(authorityMessage.text.includes("own window border override"));
        compare(reset.enabled, true);

        reset.clicked();
        compare(resetCount, 1);
        compare(requestCount, 4);
        compare(previewBar.shellBorderEnabled, true);
        compare(previewBar.shellBorderWidth, 1);
        compare(previewBar.shellBorderRadius, 15);
        compare(syncControl.checked, true);
        compare(reset.enabled, false);
    }

    function test_sharedBorderControlsFollowCoreAvailability() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);

        const enabledControl = findChild(page, "shellBorderEnabled");
        const widthControl = findChild(page, "shellBorderWidth");
        const radiusControl = findChild(page, "shellBorderRadius");
        const syncControl = findChild(
            page,
            "syncHyprlandWindowBorders"
        );
        verify(enabledControl !== null);
        verify(widthControl !== null);
        verify(radiusControl !== null);
        verify(syncControl !== null);

        compare(enabledControl.enabled, false);
        compare(widthControl.enabled, false);
        compare(radiusControl.enabled, false);
        compare(syncControl.enabled, false);

        enableCoreSettings(page);
        compare(enabledControl.enabled, true);
        compare(widthControl.enabled, true);
        compare(radiusControl.enabled, true);
        compare(syncControl.enabled, true);

        page.coreBusy = true;
        compare(enabledControl.enabled, false);
        compare(widthControl.enabled, false);
        compare(radiusControl.enabled, false);
        compare(syncControl.enabled, false);
    }

    function test_sharedSpacingControlsUseOneAtomicRequestAndAttachedPreview() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableCoreSettings(page);

        const inner = findChild(page, "shellInnerSpacing");
        const outer = findChild(page, "shellOuterSpacing");
        const sync = findChild(page, "syncHyprlandWindowSpacing");
        const authority = findChild(page, "sharedSpacingAuthorityMessage");
        const reset = findChild(page, "resetSharedSpacing");
        const maximize = findChild(page, "previewMaximizedWindow");
        const preview = findChild(page, "barPreview");
        const frame = findChild(page, "previewBarFrame");
        const bar = findChild(page, "previewBarVisual");
        const reservedLabel = findChild(page, "reservedWorkspaceLabel");
        for (const item of [
            inner,
            outer,
            sync,
            authority,
            reset,
            maximize,
            preview,
            frame,
            bar,
            reservedLabel
        ]) {
            verify(item !== null);
        }

        compare(inner.from, 0);
        compare(inner.to, 32);
        compare(inner.value, 8);
        compare(outer.from, 0);
        compare(outer.to, 32);
        compare(outer.value, 12);
        compare(sync.checked, true);
        compare(reset.enabled, false);
        verify(String(authority.text).includes("HyprShelld controls"));
        verify(String(authority.text).includes("read-only"));
        compare(preview.renderedInnerSpacing, 8);
        compare(preview.renderedOuterSpacing, 12);
        compare(preview.attachedToTopEdge, false);
        compare(frame.x, 12);
        compare(frame.y, 12);

        let requestCount = 0;
        let request = [];
        let resetCount = 0;
        page.sharedSpacingRequested.connect(function(
            innerSpacing,
            outerSpacing,
            synced
        ) {
            ++requestCount;
            request = [innerSpacing, outerSpacing, synced];
        });
        page.resetSharedSpacingRequested.connect(function() {
            ++resetCount;
        });

        page.requestSharedSpacing(0, 32, false);
        compare(requestCount, 1);
        compare(request, [0, 32, false]);
        compare(inner.value, 0);
        compare(outer.value, 32);
        compare(sync.checked, false);
        compare(preview.renderedInnerSpacing, 0);
        compare(preview.renderedOuterSpacing, 32);
        compare(frame.x, 32);
        compare(frame.y, 32);
        verify(String(authority.text).includes("own normal window gaps"));
        compare(reset.enabled, true);

        page.previewAttachedToTopEdge = true;
        wait(0);
        compare(maximize.checked, true);
        compare(page.previewAttachedToTopEdge, true);
        compare(preview.attachedToTopEdge, true);
        compare(frame.x, 0);
        compare(frame.y, 0);
        compare(bar.attachedToTopEdge, true);
        compare(bar.renderedTopLeftCornerRadius, 0);
        compare(bar.renderedTopRightCornerRadius, 0);
        compare(bar.renderedBottomLeftCornerRadius, 15);
        compare(bar.renderedBottomRightCornerRadius, 15);
        compare(reservedLabel.text, "Maximized workspace");

        reset.clicked();
        compare(resetCount, 1);
        compare(requestCount, 1);
        compare(inner.value, 8);
        compare(outer.value, 12);
        compare(sync.checked, true);
        compare(reset.enabled, false);

        page.coreBusy = true;
        compare(inner.enabled, false);
        compare(outer.enabled, false);
        compare(sync.enabled, false);
        compare(reset.enabled, false);
    }

    function test_busyAndErrorsAreWired() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableCoreSettings(page);
        enableWorkspaceSettings(page);

        const control = findChild(page, "barHeightControl");
        const workspaceSettings = findChild(
            page,
            "workspaceSwitcherSettingsCard"
        );
        const coreError = findChild(page, "coreConfigurationError");
        const coreErrorLabel = findChild(
            page,
            "coreConfigurationErrorLabel"
        );
        const componentError = findChild(
            page,
            "componentConfigurationError"
        );
        const componentErrorLabel = findChild(
            page,
            "componentConfigurationErrorLabel"
        );
        verify(control !== null);
        verify(workspaceSettings !== null);
        verify(coreError !== null);
        verify(coreErrorLabel !== null);
        verify(componentError !== null);
        verify(componentErrorLabel !== null);
        compare(control.busy, false);
        compare(control.errorText, "");
        compare(workspaceSettings.controlsEnabled, true);

        page.coreBusy = true;
        page.coreErrorText = "Could not save the bar size.";
        wait(0);
        compare(control.busy, true);
        compare(control.errorText, "");
        compare(workspaceSettings.controlsEnabled, true);
        compare(page.coreConfigurationErrorVisible, true);
        compare(page.componentConfigurationErrorVisible, false);
        verify(coreErrorLabel.text.includes(
            "Could not save the bar size."
        ));
        compare(
            coreErrorLabel.Accessible.role,
            Accessible.AlertMessage
        );

        page.coreBusy = false;
        page.componentBusy = true;
        page.componentErrorText = "Could not save workspace settings.";
        wait(0);
        compare(control.busy, false);
        compare(workspaceSettings.controlsEnabled, false);
        compare(page.coreConfigurationErrorVisible, true);
        compare(page.componentConfigurationErrorVisible, true);
        verify(componentErrorLabel.text.includes(
            "Could not save workspace settings."
        ));
        compare(
            componentErrorLabel.Accessible.role,
            Accessible.AlertMessage
        );
    }

    function test_workspaceControlsExposeCurrentSettings() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);

        const showIdentifiers = findChild(
            page,
            "workspaceShowIdentifiers"
        );
        const showNames = findChild(page, "workspaceShowNames");
        const showApplications = findChild(
            page,
            "workspaceShowApplications"
        );
        const maximumRow = findChild(
            page,
            "workspaceMaximumApplicationsRow"
        );
        const maximumApplications = findChild(
            page,
            "workspaceMaximumApplications"
        );
        const occupiedOnly = findChild(page, "workspaceOccupiedOnly");
        const scrollMode = findChild(page, "workspaceScrollMode");
        const reset = findChild(page, "resetWorkspaceSwitcher");
        verify(showIdentifiers !== null);
        verify(showNames !== null);
        verify(showApplications !== null);
        verify(maximumRow !== null);
        verify(maximumApplications !== null);
        verify(occupiedOnly !== null);
        verify(scrollMode !== null);
        verify(reset !== null);

        compare(showIdentifiers.checked, true);
        compare(showNames.checked, false);
        compare(
            showIdentifiers.Accessible.name,
            "Show workspace identifiers"
        );
        compare(showNames.Accessible.name, "Show workspace names");
        verify(showIdentifiers.mapToItem(page, 0, 0).y
            < showNames.mapToItem(page, 0, 0).y);
        verify(showNames.mapToItem(page, 0, 0).y
            < showApplications.mapToItem(page, 0, 0).y);
        compare(showApplications.checked, false);
        compare(maximumRow.visible, false);
        compare(maximumApplications.from, 1);
        compare(maximumApplications.to, 5);
        compare(maximumApplications.value, 3);
        compare(occupiedOnly.checked, false);
        compare(scrollMode.currentText, "Off");
        compare(reset.enabled, false);

        enableWorkspaceSettings(page);
        page.workspaceShowIdentifiers = false;
        page.workspaceShowNames = true;
        page.workspaceShowApplications = true;
        page.workspaceMaximumApplications = 5;
        page.workspaceOccupiedOnly = true;
        page.workspaceScrollMode = "reversed";
        const scrollView = findChild(page, "barOptionsScrollView");
        verify(scrollView !== null);
        scrollView.contentItem.contentY = scrollView.contentItem.contentHeight
            - scrollView.contentItem.height;
        wait(0);
        compare(showIdentifiers.checked, false);
        compare(showNames.checked, true);
        compare(showApplications.checked, true);
        compare(maximumApplications.value, 5);
        compare(occupiedOnly.checked, true);
        compare(scrollMode.currentText, "Reversed");
        compare(reset.enabled, true);

        page.componentBusy = true;
        compare(showIdentifiers.enabled, false);
        compare(showNames.enabled, false);
        compare(showApplications.enabled, false);
        compare(maximumApplications.enabled, false);
        compare(occupiedOnly.enabled, false);
        compare(scrollMode.enabled, false);
        compare(reset.enabled, false);
    }

    function test_disabledWorkspaceGreysNaturalSettingsAndOmitsPreview() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableCoreSettings(page);
        enableWorkspaceSettings(page);

        const message = findChild(page, "workspaceFeatureDisabledMessage");
        const messageLabel = findChild(
            page,
            "workspaceFeatureDisabledMessageLabel"
        );
        const controls = findChild(page, "workspaceSettingsControls");
        const showIdentifiers = findChild(
            page,
            "workspaceShowIdentifiers"
        );
        const showNames = findChild(page, "workspaceShowNames");
        const reset = findChild(page, "resetWorkspaceSwitcher");
        const heightControl = findChild(page, "barHeightControl");
        verify(message !== null);
        verify(messageLabel !== null);
        verify(controls !== null);
        verify(showIdentifiers !== null);
        verify(showNames !== null);
        verify(reset !== null);
        verify(heightControl !== null);

        page.workspaceFeatureAvailable = true;
        page.workspaceFeatureEnabled = false;
        page.workspacePreviewEnabled = false;
        wait(0);
        const settingsCard = findChild(
            page,
            "workspaceSwitcherSettingsCard"
        );
        verify(settingsCard !== null);
        compare(page.workspaceFeatureAvailable, true);
        compare(page.workspaceFeatureEnabled, false);
        compare(settingsCard.featureAvailable, true);
        compare(settingsCard.featureEnabled, false);
        compare(settingsCard.disabledMessageVisible, true);
        compare(
            messageLabel.text,
            "This feature has been disabled. Enable it from Components → Bar Widgets to change these settings."
        );
        compare(messageLabel.opacity, 1);
        compare(controls.opacity, 0.42);
        compare(showIdentifiers.enabled, false);
        compare(showNames.enabled, false);
        compare(reset.enabled, false);
        compare(heightControl.enabled, true);
        compare(findChild(page, "workspaceSwitcher"), null);

        page.workspaceShowIdentifiers = false;
        page.workspaceShowNames = true;
        page.workspaceShowApplications = true;
        page.workspaceMaximumApplications = 5;
        page.workspaceOccupiedOnly = true;
        page.workspaceScrollMode = "reversed";
        page.workspaceFeatureEnabled = true;
        page.workspacePreviewEnabled = true;
        wait(0);
        const switcher = findChild(page, "workspaceSwitcher");
        verify(switcher !== null);
        compare(settingsCard.disabledMessageVisible, false);
        compare(controls.opacity, 1);
        compare(showIdentifiers.checked, false);
        compare(showNames.checked, true);
        compare(switcher.showIdentifiers, false);
        compare(switcher.showNames, true);
        compare(switcher.showApplications, true);
        compare(switcher.maximumApplications, 5);
        compare(switcher.scrollMode, "reversed");
    }

    function test_unavailableWorkspaceIsNotPresentedAsDisabled() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableCoreSettings(page);
        enableWorkspaceSettings(page);

        page.workspaceFeatureAvailable = false;
        page.workspaceFeatureEnabled = true;
        page.workspacePreviewEnabled = true;
        wait(0);
        const message = findChild(page, "workspaceFeatureDisabledMessage");
        const switcher = findChild(page, "workspaceSwitcher");
        verify(message !== null);
        verify(switcher !== null);
        compare(message.visible, false);
        compare(page.componentServiceWarningVisible, true);
        verify(page.componentWarningMessage.includes("placement"));
        compare(page.workspaceControlsEnabled, false);
        compare(switcher.showIdentifiers, true);
        compare(switcher.showNames, false);
    }

    function test_workspaceAuthorityDistinguishesGlobalAndInstanceState() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const definition = workspaceCatalogRecord();
        const digest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        const instanceId = application.workspaceInstanceId;
        const instances = {};
        instances[instanceId] = {
            componentId: definition.id,
            enabled: true,
            settings: {
                showIdentifiers: true,
                showNames: false,
                showApplications: false,
                maximumApplications: 3,
                occupiedOnly: false,
                scrollMode: "disabled"
            }
        };
        const snapshot = configureSnapshotForComponent(definition, true);
        snapshot.instances = instances;

        let state = application.workspaceComponentStateFromServices(
            true,
            digest,
            [definition],
            true,
            true,
            digest,
            snapshot
        );
        compare(state.available, true);
        compare(state.desiredEnabled, true);
        compare(state.instanceEnabled, true);
        compare(state.previewEnabled, true);

        state = application.workspaceComponentStateFromServices(
            false,
            digest,
            [definition],
            true,
            true,
            digest,
            snapshot
        );
        compare(state.available, false);
        compare(state.desiredEnabled, true);
        compare(state.instanceEnabled, true);
        compare(state.previewEnabled, true);

        snapshot.components[definition.id].enabled = false;
        snapshot.instances[instanceId].enabled = false;
        state = application.workspaceComponentStateFromServices(
            true, digest, [definition], true, true, digest, snapshot
        );
        compare(state.available, true);
        compare(state.desiredEnabled, false);
        compare(state.instanceEnabled, false);
        compare(state.previewEnabled, false);
        compare(application.workspaceNaturalSettingsAvailable(state), true);

        snapshot.components[definition.id].enabled = true;
        state = application.workspaceComponentStateFromServices(
            true, digest, [definition], true, true, digest, snapshot
        );
        compare(state.available, true);
        compare(state.desiredEnabled, true);
        compare(state.instanceEnabled, false);
        compare(state.previewEnabled, false);
        compare(application.workspaceNaturalSettingsAvailable(state), false);

        state = application.workspaceComponentStateFromServices(
            true,
            digest,
            [definition],
            true,
            true,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            snapshot
        );
        compare(state.available, false);
        compare(state.previewEnabled, false);
    }

    function test_workspaceConditionalRowIsVisible() {
        const testWindow = createTemporaryObject(
            workspaceSettingsComponent,
            this
        );
        verify(testWindow !== null);
        const settings = testWindow.settings;
        verify(settings !== null);
        const maximumRow = findChild(
            settings,
            "workspaceMaximumApplicationsRow"
        );
        verify(maximumRow !== null);
        compare(maximumRow.visible, false);

        settings.showApplications = true;
        waitForRendering(settings);
        compare(maximumRow.visible, true);

        settings.featureEnabled = false;
        waitForRendering(settings);
        const disabledMessage = findChild(
            settings,
            "workspaceFeatureDisabledMessage"
        );
        verify(disabledMessage !== null);
        compare(settings.disabledMessageVisible, true);
        compare(disabledMessage.visible, true);
    }

    function test_workspaceRequestsCarryOneAtomicSnapshot() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableWorkspaceSettings(page);

        const settings = findChild(
            page,
            "workspaceSwitcherSettingsCard"
        );
        const reset = findChild(page, "resetWorkspaceSwitcher");
        verify(settings !== null);
        verify(reset !== null);

        let requestCount = 0;
        let request = [];
        let resetCount = 0;
        page.workspaceSwitcherRequested.connect(function(
            showIdentifiers,
            showNames,
            showApplications,
            maximumApplications,
            occupiedOnly,
            scrollMode
        ) {
            ++requestCount;
            request = [
                showIdentifiers,
                showNames,
                showApplications,
                maximumApplications,
                occupiedOnly,
                scrollMode
            ];
        });
        page.resetWorkspaceSwitcherRequested.connect(function() {
            ++resetCount;
        });

        settings.requestSnapshot(
            true, false, false, 3, false, "disabled"
        );
        compare(requestCount, 0);

        settings.requestSnapshot(
            false, false, false, 5, false, "disabled"
        );
        compare(requestCount, 1);
        compare(request, [
            false, false, false, 5, false, "disabled"
        ]);

        page.workspaceShowIdentifiers = false;
        page.workspaceMaximumApplications = 5;
        page.workspaceOccupiedOnly = true;
        settings.requestSnapshot(
            false, true, false, 5, true, "reversed"
        );
        compare(requestCount, 2);
        compare(request, [
            false, true, false, 5, true, "reversed"
        ]);

        page.componentBusy = true;
        settings.requestSnapshot(
            true, true, false, 5, true, "reversed"
        );
        compare(requestCount, 2);

        page.componentBusy = false;
        reset.clicked();
        compare(resetCount, 1);
    }

    function test_workspaceControlsReconcileAfterRejectedAsyncSave() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableWorkspaceSettings(page);

        const showIdentifiers = findChild(
            page,
            "workspaceShowIdentifiers"
        );
        const maximumApplications = findChild(
            page,
            "workspaceMaximumApplications"
        );
        const scrollMode = findChild(page, "workspaceScrollMode");
        verify(showIdentifiers !== null);
        verify(maximumApplications !== null);
        verify(scrollMode !== null);

        let requestCount = 0;
        page.workspaceSwitcherRequested.connect(function() {
            ++requestCount;
            page.componentBusy = true;
        });

        showIdentifiers.checked = false;
        showIdentifiers.toggled();
        compare(requestCount, 1);
        compare(page.componentBusy, true);
        compare(showIdentifiers.checked, false);

        page.componentErrorText = "The change could not be saved.";
        page.componentBusy = false;
        wait(0);
        compare(showIdentifiers.checked, true);

        page.componentErrorText = "";
        page.workspaceShowApplications = true;
        wait(0);
        maximumApplications.value = 5;
        maximumApplications.valueModified();
        compare(requestCount, 2);
        compare(page.componentBusy, true);
        compare(maximumApplications.value, 5);

        page.componentErrorText = "The change could not be saved.";
        page.componentBusy = false;
        wait(0);
        compare(maximumApplications.value, 3);

        page.componentErrorText = "";
        scrollMode.currentIndex = 2;
        scrollMode.activated(2);
        compare(requestCount, 3);
        compare(page.componentBusy, true);
        compare(scrollMode.currentIndex, 2);

        page.componentErrorText = "The change could not be saved.";
        page.componentBusy = false;
        wait(0);
        compare(scrollMode.currentIndex, 0);
        compare(scrollMode.currentText, "Off");
    }

    function test_workspaceSnapshotReplacementIsWholeAndAtomic() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);

        const workspaceId = application.workspaceInstanceId;
        const workspaceComponentId = application.workspaceComponentId;
        const dormantId = "c89b6683-33a8-4d63-a573-b89b99fd0dd0";
        const instances = {};
        instances[workspaceId] = {
            componentId: workspaceComponentId,
            enabled: true,
            settings: {
                showIdentifiers: true,
                showNames: false,
                showApplications: false,
                maximumApplications: 3,
                occupiedOnly: false,
                scrollMode: "disabled"
            }
        };
        instances[dormantId] = {
            componentId: "org.example.dormant-widget",
            enabled: false,
            settings: { preserved: "exactly" }
        };
        const snapshot = {
            formatVersion: 1,
            revision: "42",
            components: {
                "org.example.dormant-widget": {
                    packageDigest:
                        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                    enabled: false,
                    grantedCapabilities: ["example.read"],
                    settings: { retained: true }
                }
            },
            instances: instances,
            layouts: {
                bars: {
                    main: {
                        outputs: { mode: "all" },
                        regions: {
                            start: [workspaceId],
                            center: [],
                            end: [dormantId]
                        }
                    }
                },
                desktops: {}
            }
        };

        const extracted = application.workspaceSettingsFromSnapshot(
            snapshot
        );
        compare(extracted.valid, true);
        compare(extracted.showIdentifiers, true);
        compare(extracted.showNames, false);

        const replacement = application.workspaceSnapshotWithSettings(
            snapshot,
            false,
            true,
            true,
            5,
            true,
            "reversed"
        );
        verify(replacement !== null);
        verify(replacement !== snapshot);
        compare(
            snapshot.instances[workspaceId].settings.showIdentifiers,
            true
        );
        compare(snapshot.instances[workspaceId].settings.showNames, false);
        compare(replacement.instances[workspaceId].settings, {
            showIdentifiers: false,
            showNames: true,
            showApplications: true,
            maximumApplications: 5,
            occupiedOnly: true,
            scrollMode: "reversed"
        });
        compare(
            replacement.instances[dormantId],
            snapshot.instances[dormantId]
        );
        compare(replacement.components, snapshot.components);
        compare(replacement.layouts, snapshot.layouts);
        compare(replacement.revision, "42");

        const reset = application.workspaceSnapshotWithSettings(
            replacement,
            true,
            false,
            false,
            3,
            false,
            "disabled"
        );
        compare(
            reset.instances[workspaceId].settings,
            application.workspaceDefaults
        );

        compare(application.workspaceSnapshotWithSettings(
            snapshot,
            true,
            false,
            false,
            6,
            false,
            "disabled"
        ), null);
        snapshot.instances[workspaceId].componentId =
            "org.example.wrong-component";
        compare(
            application.workspaceSettingsFromSnapshot(snapshot).valid,
            false
        );
        compare(application.workspaceSnapshotWithSettings(
            snapshot,
            true,
            false,
            false,
            3,
            false,
            "disabled"
        ), null);
    }

    function test_previewDemonstratesWorkspaceSettings() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        const preview = findChild(page, "barPreview");
        const bar = findChild(page, "previewBarVisual");
        const startSlot = findChild(page, "barStartComponentSlot");
        const switcher = findChild(page, "workspaceSwitcher");
        verify(preview !== null);
        verify(bar !== null);
        verify(startSlot !== null);
        verify(switcher !== null);

        compare(bar.currentTime.getFullYear(), 1991);
        compare(bar.currentTime.getMonth(), 8);
        compare(bar.currentTime.getDate(), 17);
        compare(bar.currentTime.getDay(), 2);
        compare(bar.currentTime.getHours(), 15);
        compare(bar.currentTime.getMinutes(), 42);

        compare(preview.previewWorkspaces.length, 3);
        compare(switcher.showIdentifiers, true);
        compare(switcher.showNames, false);
        compare(Boolean(preview.previewWorkspaces[0].placeholder), false);
        compare(preview.previewWorkspaces[0].occupied, false);
        compare(preview.previewWorkspaces[0].active, false);
        compare(preview.previewWorkspaces[1].active, true);
        compare(preview.previewWorkspaces[1].occupied, true);
        compare(preview.previewWorkspaces[1].applications.length, 5);
        compare(preview.previewWorkspaces[1].applications[0].iconSource, "");
        compare(preview.previewWorkspaces[1].applications[0].fallbackInitial, "E");
        compare(preview.previewWorkspaces[1].applications[0].active, true);
        compare(preview.previewWorkspaces[1].applications[0].count, 1);
        compare(preview.previewWorkspaces[1].applications[0].activatable, false);
        verify(preview.previewWorkspaces[1].applications[1].iconSource.length > 0);
        compare(preview.previewWorkspaces[2].urgent, true);
        compare(preview.previewWorkspaces[2].occupied, true);
        compare(preview.previewWorkspaces[2].applications.length, 1);
        compare(preview.previewWorkspaces[2].applications[0].active, false);
        compare(preview.previewWorkspaces[2].applications[0].count, 3);
        compare(preview.previewWorkspaces[2].applications[0].activatable, false);
        compare(findChild(page, "workspacePlaceholder-0"), null);

        const numericIndicator = findChild(page, "workspaceIndicator-1");
        const namedIndicator = findChild(page, "workspaceIndicator-2");
        verify(numericIndicator !== null);
        verify(namedIndicator !== null);
        compare(numericIndicator.circleIdentifier, "1");
        compare(numericIndicator.nameLabel, "");
        compare(namedIndicator.circleIdentifier, "2");
        compare(namedIndicator.nameLabel, "");

        page.workspaceShowIdentifiers = false;
        wait(0);
        compare(namedIndicator.circleIdentifier, "");
        compare(namedIndicator.nameLabel, "");

        page.workspaceShowIdentifiers = true;
        page.workspaceShowNames = true;
        wait(0);
        compare(numericIndicator.circleIdentifier, "1");
        compare(numericIndicator.nameLabel, "");
        compare(namedIndicator.circleIdentifier, "2");
        compare(namedIndicator.nameLabel, "writing");

        page.workspaceShowIdentifiers = false;
        wait(0);
        compare(numericIndicator.circleIdentifier, "");
        compare(numericIndicator.nameLabel, "");
        compare(namedIndicator.circleIdentifier, "");
        compare(namedIndicator.nameLabel, "writing");

        page.workspaceShowApplications = true;
        page.workspaceMaximumApplications = 1;
        wait(0);
        const currentIndicator = findChild(page, "workspaceIndicator-2");
        verify(currentIndicator !== null);
        compare(currentIndicator.visibleApplications.length, 1);
        compare(currentIndicator.visibleApplications[0].active, true);
        compare(currentIndicator.applicationOverflow, 4);

        page.workspaceMaximumApplications = 3;
        page.workspaceScrollMode = "normal";
        compare(switcher.showIdentifiers, false);
        compare(switcher.showNames, true);
        compare(switcher.showApplications, true);
        compare(switcher.maximumApplications, 3);
        compare(switcher.scrollMode, "normal");

        page.workspaceOccupiedOnly = true;
        compare(preview.previewWorkspaces.length, 2);
        compare(preview.previewWorkspaces[0].active, true);
        compare(preview.previewWorkspaces[1].occupied, true);

        page.workspaceOccupiedOnly = false;
        compare(preview.previewWorkspaces.length, 3);
        compare(preview.previewWorkspaces[0].workspaceId, 1);
        compare(preview.previewWorkspaces[2].workspaceId, 4);
    }

    function test_barPreviewStaysStickyWhileOptionsScroll() {
        const page = createTemporaryObject(pageComponent, this, {
            width: 820,
            height: 900
        });
        verify(page !== null);
        enableCoreSettings(page);
        enableWorkspaceSettings(page);
        waitForRendering(page);
        wait(0);

        const sticky = findChild(page, "barStickyPreview");
        const scroll = findChild(page, "barOptionsScrollView");
        const content = findChild(page, "barOptionsContent");
        const preview = findChild(page, "barPreview");
        const heightSlider = findChild(page, "barHeightSlider");
        verify(sticky !== null);
        verify(scroll !== null);
        verify(content !== null);
        verify(preview !== null);
        verify(heightSlider !== null);
        compare(page.compactPreview, false);
        compare(preview.height, 286);
        compare(preview.implicitHeight, 286);
        compare(preview.scale, 1);
        verify(Math.abs(preview.width - sticky.width) <= 0.01);
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width <= scroll.contentWidth + 0.01);

        const maximumContentY = scroll.contentItem.contentHeight
            - scroll.contentItem.height;
        verify(maximumContentY > 0);
        const stickyBefore = sticky.mapToItem(page, 0, 0);
        const previewBefore = preview.mapToItem(page, 0, 0);
        const contentBefore = content.mapToItem(page, 0, 0);
        const targetContentY = Math.min(180, maximumContentY);
        verify(targetContentY > 0);

        scroll.contentItem.contentY = targetContentY;
        tryCompare(scroll.contentItem, "contentY", targetContentY);
        const stickyAfter = sticky.mapToItem(page, 0, 0);
        const previewAfter = preview.mapToItem(page, 0, 0);
        const contentAfter = content.mapToItem(page, 0, 0);
        verify(Math.abs(stickyAfter.x - stickyBefore.x) <= 0.01);
        verify(Math.abs(stickyAfter.y - stickyBefore.y) <= 0.01);
        verify(Math.abs(previewAfter.x - previewBefore.x) <= 0.01);
        verify(Math.abs(previewAfter.y - previewBefore.y) <= 0.01);
        verify(contentAfter.y < contentBefore.y);

        heightSlider.value = 64;
        wait(0);
        compare(preview.barHeight, 64);
    }

    function test_minimumSizeCanReachWorkspaceReset() {
        // Main's 620-pixel minimum width leaves 423 pixels for this page
        // after the fixed sidebar and separator.
        const page = createTemporaryObject(pageComponent, this, {
            width: 423,
            height: 480
        });
        verify(page !== null);
        const sticky = findChild(page, "barStickyPreview");
        const scrollView = findChild(page, "barOptionsScrollView");
        const content = findChild(page, "barOptionsContent");
        const preview = findChild(page, "barPreview");
        const maximizePreview = findChild(page, "previewMaximizedWindow");
        const spacingCard = findChild(page, "sharedSpacingSettingsCard");
        const innerSpacing = findChild(page, "shellInnerSpacing");
        const outerSpacing = findChild(page, "shellOuterSpacing");
        const spacingSync = findChild(page, "syncHyprlandWindowSpacing");
        const spacingReset = findChild(page, "resetSharedSpacing");
        const borderCard = findChild(page, "sharedBorderSettingsCard");
        const borderEnabled = findChild(page, "shellBorderEnabled");
        const borderWidth = findChild(page, "shellBorderWidth");
        const borderRadius = findChild(page, "shellBorderRadius");
        const borderSync = findChild(page, "syncHyprlandWindowBorders");
        const borderReset = findChild(page, "resetSharedBorder");
        const reset = findChild(page, "resetWorkspaceSwitcher");
        verify(sticky !== null);
        verify(scrollView !== null);
        verify(content !== null);
        verify(preview !== null);
        verify(maximizePreview !== null);
        verify(spacingCard !== null);
        verify(innerSpacing !== null);
        verify(outerSpacing !== null);
        verify(spacingSync !== null);
        verify(spacingReset !== null);
        verify(borderCard !== null);
        verify(borderEnabled !== null);
        verify(borderWidth !== null);
        verify(borderRadius !== null);
        verify(borderSync !== null);
        verify(borderReset !== null);
        verify(reset !== null);
        waitForRendering(page);
        wait(0);
        compare(page.compactPreview, true);
        compare(preview.height, 286);
        compare(preview.implicitHeight, 286);
        compare(preview.scale, 1);
        verify(Math.abs(preview.width - sticky.width) <= 0.01);
        verify(scrollView.height >= 100);
        verify(scrollView.contentItem.contentHeight > scrollView.height);
        verify(scrollView.contentWidth
            <= scrollView.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width
            <= scrollView.contentWidth + 0.01);
        for (const control of [
            maximizePreview,
            innerSpacing,
            outerSpacing,
            spacingSync,
            spacingReset,
            borderEnabled,
            borderWidth,
            borderRadius,
            borderSync,
            borderReset
        ]) {
            verify(control.height >= 44);
            const position = control.mapToItem(page, 0, 0);
            verify(position.x >= 0);
            verify(position.x + control.width <= page.width + 0.01);
        }
        const spacingCardPosition = spacingCard.mapToItem(content, 0, 0);
        verify(spacingCardPosition.x >= 0);
        verify(spacingCardPosition.x + spacingCard.width
            <= content.width + 0.01);
        const borderCardPosition = borderCard.mapToItem(content, 0, 0);
        verify(borderCardPosition.x >= 0);
        verify(borderCardPosition.x + borderCard.width
            <= content.width + 0.01);

        const stickyBefore = sticky.mapToItem(page, 0, 0);
        const previewBefore = preview.mapToItem(page, 0, 0);
        const contentBefore = content.mapToItem(page, 0, 0);
        verify(stickyBefore.x >= 0);
        verify(stickyBefore.x + sticky.width <= page.width + 0.01);
        verify(stickyBefore.y >= 0);
        verify(stickyBefore.y + sticky.height <= page.height + 0.01);

        const maximumContentY = Math.max(
            0,
            scrollView.contentItem.contentHeight
                - scrollView.contentItem.height
        );
        verify(maximumContentY > 0);
        scrollView.contentItem.contentY = maximumContentY;
        tryCompare(scrollView.contentItem, "contentY", maximumContentY);
        const stickyAfter = sticky.mapToItem(page, 0, 0);
        const previewAfter = preview.mapToItem(page, 0, 0);
        const contentAfter = content.mapToItem(page, 0, 0);
        verify(Math.abs(stickyAfter.x - stickyBefore.x) <= 0.01);
        verify(Math.abs(stickyAfter.y - stickyBefore.y) <= 0.01);
        verify(Math.abs(previewAfter.x - previewBefore.x) <= 0.01);
        verify(Math.abs(previewAfter.y - previewBefore.y) <= 0.01);
        verify(contentAfter.y < contentBefore.y);
        const resetPosition = reset.mapToItem(page, 0, 0);
        verify(resetPosition.x >= 0);
        verify(resetPosition.x + reset.width <= page.width);
        verify(resetPosition.y >= 0);
        verify(resetPosition.y + reset.height <= page.height);
    }

    function test_narrowPreviewUsesDesktopScale() {
        const page = createTemporaryObject(pageComponent, this, {
            width: 423,
            height: 480
        });
        verify(page !== null);

        const preview = findChild(page, "barPreview");
        const frame = findChild(page, "previewBarFrame");
        const bar = findChild(page, "previewBarVisual");
        const reservedLabel = findChild(page, "reservedWorkspaceLabel");
        const switcher = findChild(page, "workspaceSwitcher");
        const firstWorkspace = findChild(page, "workspaceIndicator-1");
        const activeWorkspace = findChild(page, "workspaceIndicator-2");
        const urgentWorkspace = findChild(page, "workspaceIndicator-4");
        verify(preview !== null);
        verify(frame !== null);
        verify(bar !== null);
        verify(reservedLabel !== null);
        verify(switcher !== null);
        verify(firstWorkspace !== null);
        verify(activeWorkspace !== null);
        verify(urgentWorkspace !== null);
        verify(preview.previewScale < 1);
        verify(frame.width * frame.scale <= preview.width);
        verify(reservedLabel.y >= frame.y + bar.height * frame.scale);
        compare(switcher.interactive, false);
        compare(firstWorkspace.workspaceActive, false);
        compare(activeWorkspace.workspaceActive, true);
        compare(urgentWorkspace.workspaceUrgent, true);
        compare(findChild(page, "workspaceIndicator-3"), null);
    }

    function test_displayDraftRequiresAChangeAndSafeMirrorGraph() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const outputs = [
            displayRecord("display-a", "DP-1", true, "", -1),
            displayRecord("display-b", "DP-2", false, "", -1),
            displayRecord("display-c", "DP-3", true, "DP-1", -1),
            displayRecord("display-d", "DP-4", true, "", -1)
        ];
        const topology = [
            connectedDisplay("DP-1", true, 0),
            connectedDisplay("DP-2", false, 1920),
            connectedDisplay("DP-3", true, 3840),
            connectedDisplay("DP-4", true, 5760)
        ];
        configureDisplaysPage(page, outputs, topology);
        waitForRendering(page);
        wait(0);

        compare(page.draftDirty, false);
        compare(page.previewEnabled, false);

        const card = findChild(page, "displaySettingsCard");
        verify(card !== null);
        compare(card.availableMirrors.length, 2);
        compare(card.availableMirrors[0].value, "");
        compare(card.availableMirrors[1].value, "DP-4");

        let changed = page.clone(page.outputById("display-a"));
        changed.scale = 1.25;
        page.replaceOutput(changed);
        compare(page.draftDirty, true);
        compare(page.draftValidationMessage, "");
        compare(page.previewEnabled, true);

        changed = page.clone(page.outputById("display-a"));
        changed.scale = 1;
        page.replaceOutput(changed);
        compare(page.draftDirty, false);
        compare(page.previewEnabled, false);

        changed = page.clone(page.outputById("display-a"));
        changed.mirror = "DP-2";
        page.replaceOutput(changed);
        verify(page.draftValidationMessage.includes("disabled"));
        compare(page.previewEnabled, false);

        changed = page.clone(page.outputById("display-a"));
        changed.mirror = "DP-1";
        page.replaceOutput(changed);
        verify(page.draftValidationMessage.includes("itself"));
        compare(page.previewEnabled, false);

        changed = page.clone(page.outputById("display-a"));
        changed.mirror = "DP-4";
        page.replaceOutput(changed);
        verify(page.draftValidationMessage.includes("chains"));
        compare(page.previewEnabled, false);

        let directTarget = page.clone(page.outputById("display-d"));
        directTarget.mirror = "DP-1";
        page.replaceOutput(directTarget);
        verify(page.draftValidationMessage.includes("not a mirror"));
        compare(page.previewEnabled, false);
    }

    function test_displayMirrorTracksTargetPosition() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const primary = displayRecord("display-a", "DP-1", true, "", -1);
        const secondary = displayRecord("display-b", "DP-2", true, "", -1);
        primary.position = "0x0";
        secondary.position = "1920x0";
        configureDisplaysPage(
            page,
            [primary, secondary],
            [
                connectedDisplay("DP-1", true, 0),
                connectedDisplay("DP-2", true, 1920)
            ]
        );
        waitForRendering(page);
        wait(0);

        let mirrored = page.clone(page.outputById("display-b"));
        mirrored.mirror = "DP-1";
        page.replaceOutput(mirrored);
        compare(page.outputById("display-b").position, "0x0");
        compare(page.draftValidationMessage, "");

        let movedTarget = page.clone(page.outputById("display-a"));
        movedTarget.position = "320x180";
        page.replaceOutput(movedTarget);
        compare(page.outputById("display-a").position, "320x180");
        compare(page.outputById("display-b").position, "320x180");
        compare(page.draftValidationMessage, "");
        compare(page.previewEnabled, true);
    }

    function test_displayAdvancedValuesAndNewConnectorSeeds() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", 3)],
            [
                connectedDisplay("DP-1", true, 0),
                connectedDisplay(
                    "DP-2",
                    true,
                    1920,
                    "XRGB2101010",
                    0.2
                ),
                connectedDisplay("DP-3", true, 3840, "XRGB8888", -20)
            ]
        );
        waitForRendering(page);
        wait(0);

        const vrr = findChild(page, "displayVrrComboBox");
        verify(vrr !== null);
        compare(vrr.currentValue, 3);

        const tenBit = page.outputById("display-DP-2");
        verify(tenBit !== null);
        compare(tenBit.bitdepth, 10);
        compare(tenBit.cm, "srgb");
        compare(tenBit.sdrMinLuminance, 0.2);

        const invalidMinimum = page.outputById("display-DP-3");
        verify(invalidMinimum !== null);
        compare(invalidMinimum.bitdepth, 8);
        compare(invalidMinimum.sdrMinLuminance, 0.2);
    }

    function test_displayDraftUsesTheWinningExactConnectorRule() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const first = displayRecord(
            "older-display-rule",
            "DP-1",
            true,
            "",
            0
        );
        const winning = displayRecord(
            "winning-display-rule",
            "DP-1",
            true,
            "",
            3
        );
        winning.icc = "/profiles/winning.icc";
        configureDisplaysPage(
            page,
            [first, winning],
            [connectedDisplay("DP-1", true, 0)]
        );
        waitForRendering(page);
        wait(0);

        compare(page.draftOutputs.length, 1);
        compare(page.draftOutputs[0].id, "winning-display-rule");
        compare(page.draftOutputs[0].vrr, 3);
        compare(page.draftOutputs[0].icc, "/profiles/winning.icc");
    }

    function test_displayHotplugRefreshesAndInvalidatesTheDraft() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        waitForRendering(page);
        wait(0);

        const changed = page.clone(page.outputById("display-a"));
        changed.scale = 1.25;
        page.replaceOutput(changed);
        compare(page.draftDirty, true);
        compare(page.previewEnabled, true);

        page.connectedDisplays = [
            connectedDisplay("DP-1", true, 0),
            connectedDisplay("DP-2", true, 1920)
        ];
        page.topologyDigest =
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        wait(0);

        compare(page.draftDirty, false);
        compare(page.previewEnabled, false);
        compare(page.inventoryChangedWhileEditing, true);
        compare(page.draftOutputs.length, 2);
        compare(page.synchronizedTopologyDigest, page.topologyDigest);
    }

    function test_displayDraftSurvivesUnrelatedSharedVisualSnapshotRevision() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const original = displayRecord(
            "display-a",
            "DP-1",
            true,
            "",
            -1
        );
        configureDisplaysPage(
            page,
            [original],
            [connectedDisplay("DP-1", true, 0)]
        );
        waitForRendering(page);
        wait(0);

        let changed = page.clone(page.outputById("display-a"));
        changed.scale = 1.25;
        page.replaceOutput(changed);
        compare(page.draftDirty, true);
        compare(page.outputById("display-a").scale, 1.25);

        page.snapshot = {
            revision: "8",
            overrides: {
                "hyprland.general.gaps_in": [8, 8, 8, 8]
            },
            monitors: page.clone([original])
        };
        wait(0);
        compare(page.draftDirty, true);
        compare(page.outputById("display-a").scale, 1.25);
        compare(page.inventoryChangedWhileEditing, false);
        compare(page.synchronizedMonitorRecords, [original]);

        const authoritativeChange = page.clone(original);
        authoritativeChange.scale = 1.5;
        page.snapshot = {
            revision: "9",
            overrides: {
                "hyprland.general.gaps_in": [9, 9, 9, 9]
            },
            monitors: [authoritativeChange]
        };
        wait(0);
        compare(page.draftDirty, false);
        compare(page.outputById("display-a").scale, 1.5);
        compare(page.synchronizedMonitorRecords, [authoritativeChange]);
    }

    function test_displaySavedRulesArePreservedWithoutOfflineClaim() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const descriptionRule = displayRecord(
            "saved-projector",
            "desc:Example Projector",
            true,
            "",
            -1
        );
        configureDisplaysPage(
            page,
            [
                displayRecord("display-a", "DP-1", true, "", -1),
                descriptionRule
            ],
            [connectedDisplay("DP-1", true, 0)]
        );
        waitForRendering(page);
        wait(0);

        compare(page.offlineRecords.length, 1);
        compare(page.offlineRecords[0].selector, "desc:Example Projector");
        const label = findChild(page, "savedDisplayRulesLabel");
        verify(label !== null);
        compare(label.visible, true);
        verify(String(label.text).includes("saved display rule"));
        verify(!/offline/i.test(String(label.text)));
    }

    function test_displayOwnerLossProjectionUnlocksUnavailablePage() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        page.managementState = "preview";
        page.confirmationState = "awaiting-confirmation";
        page.confirmationRevision = 8;
        // DeadlineMs is an informational UTC-epoch projection. The daemon's
        // monotonic timer, rather than this countdown, owns automatic revert.
        page.countdownNowMs = 100000;
        page.confirmationDeadlineMs = 115000;
        page.confirmationGeneration =
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        page.confirmationOwned = true;
        waitForRendering(page);
        wait(0);

        const overlay = findChild(page, "displayConfirmationOverlay");
        const keep = findChild(page, "confirmDisplayConfigurationButton");
        const title = findChild(page, "displayConfirmationTitle");
        const message = findChild(page, "displayConfirmationMessage");
        verify(overlay !== null);
        verify(keep !== null);
        verify(title !== null);
        verify(message !== null);
        compare(overlay.visible, true);
        compare(keep.visible, true);
        compare(keep.enabled, true);
        compare(page.confirmationSecondsRemaining, 15);

        // The UTC projection drives presentation only. A wall-clock jump must
        // not locally decide whether the daemon still accepts confirmation.
        page.countdownNowMs = 120000;
        compare(page.confirmationSecondsRemaining, 0);
        compare(keep.enabled, true);

        page.confirmationOwned = false;
        wait(0);
        compare(keep.visible, false);

        page.confirmationState = "failed";
        wait(0);
        compare(title.text, "Display recovery needs attention");
        verify(String(message.text).includes(
            "If the desktop health warning reports Compositor settings"
        ));
        verify(String(message.text).includes(
            "preserve the current state and seek recovery guidance"
        ));
        compare(message.Accessible.role, Accessible.AlertMessage);

        page.serviceAvailable = false;
        page.errorMessage = "Injected private recovery failure";
        wait(0);
        compare(title.text, "Display confirmation is unavailable");

        // This is the projection CompositorClient publishes when its service
        // owner disappears; stale global preview state must not pin the page.
        page.managementState = "unmanaged";
        page.confirmationState = "idle";
        page.confirmationRevision = 0;
        page.confirmationDeadlineMs = 0;
        page.confirmationGeneration = "";
        wait(0);
        compare(overlay.visible, false);
        compare(page.confirmationRevision, 0);
        compare(page.confirmationDeadlineMs, 0);
        compare(page.confirmationGeneration, "");
    }

    function test_displayAdoptionRequiresExplicitConfirmation() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        page.managementState = "unmanaged";
        page.applyState = "unavailable";
        page.errorName = "org.hyprshelld.Error.OperationFailed";
        page.errorMessage = "The previous takeover attempt was rejected.";
        waitForRendering(page);
        wait(0);

        const takeControl = findChild(page, "adoptCompositorButton");
        const status = findChild(page, "displayStatusMessage");
        const dialog = findChild(page, "displayAdoptionDialog");
        const explanation = findChild(
            dialog,
            "displayAdoptionExplanation"
        );
        const cancel = findChild(
            dialog,
            "cancelDisplayAdoptionButton"
        );
        const confirm = findChild(
            dialog,
            "confirmDisplayAdoptionButton"
        );
        verify(takeControl !== null);
        verify(status !== null);
        verify(dialog !== null);
        verify(explanation !== null);
        verify(cancel !== null);
        verify(confirm !== null);
        compare(page.serviceAvailable, true);
        compare(page.writable, true);
        compare(page.busy, false);
        compare(page.confirmationState, "idle");
        compare(dialog.opened, false);
        compare(dialog.modal, true);
        compare(takeControl.visible, true);
        compare(takeControl.enabled, true);
        verify(String(status.text).includes("display operation failed"));
        verify(String(status.text).includes(
            "previous takeover attempt was rejected"
        ));
        compare(takeControl.Accessible.name,
            "Review compositor management takeover");
        compare(cancel.Accessible.name,
            "Cancel without changing Hyprland");
        compare(confirm.Accessible.name,
            "Confirm and let HyprShelld manage Hyprland");
        compare(explanation.Accessible.name, explanation.text);
        verify(takeControl.implicitHeight >= 44);
        verify(cancel.implicitHeight >= 44);
        verify(confirm.implicitHeight >= 44);

        let adoptionRequests = 0;
        page.adoptionRequested.connect(function() {
            ++adoptionRequests;
        });

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        compare(adoptionRequests, 0);
        tryCompare(cancel, "activeFocus", true);

        const copy = String(explanation.text).toLowerCase();
        verify(copy.includes("replace the active hyprland entrypoint"));
        verify(copy.includes("reload hyprland"));
        verify(copy.includes("not imported"));
        verify(copy.includes("if hyprland.lua exists"));
        verify(copy.includes("exact original"));
        verify(copy.includes("preserved privately for recovery"));
        verify(copy.includes("if it does not exist"));
        verify(copy.includes("that absence is recorded"));
        verify(copy.includes("legacy configuration files stay where they are"));
        verify(copy.includes("no longer selects them"));
        verify(copy.includes("preserves an existing user-custom.lua"));
        verify(copy.includes("if that file is absent"));
        verify(copy.includes("creates it"));
        verify(copy.includes("loaded last"));
        verify(copy.includes("no user-facing action to stop managing"));
        verify(copy.includes("canceling leaves your files"));
        verify(copy.includes("running compositor unchanged"));

        cancel.clicked();
        tryCompare(dialog, "opened", false);
        compare(adoptionRequests, 0);

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        keyClick(Qt.Key_Escape);
        tryCompare(dialog, "opened", false);
        compare(adoptionRequests, 0);

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        confirm.clicked();
        tryCompare(dialog, "opened", false);
        compare(adoptionRequests, 1);
        compare(confirm.enabled, false);

        // A stale callback after the dialog closes cannot submit twice.
        dialog.confirmAdoption();
        compare(adoptionRequests, 1);
    }

    function test_displayPendingBaselineActionSurvivesTransientError() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.appliedRevision = 6;
        page.errorName = "org.hyprshelld.Error.OperationFailed";
        page.errorMessage = "The previous apply attempt was interrupted.";
        waitForRendering(page);
        wait(0);

        const apply = findChild(
            page,
            "applyCompositorBaselineButton"
        );
        const status = findChild(page, "displayStatusMessage");
        verify(apply !== null);
        verify(status !== null);
        compare(page.managementState, "managed");
        compare(page.baselineCurrent, false);
        compare(page.busy, false);
        compare(page.confirmationState, "idle");
        compare(apply.visible, true);
        compare(apply.enabled, true);
        verify(String(status.text).includes("display operation failed"));
        verify(String(status.text).includes(
            "previous apply attempt was interrupted"
        ));

        let applyRequests = 0;
        page.applyRequested.connect(function() { ++applyRequests; });
        apply.clicked();
        compare(applyRequests, 1);

        page.busy = true;
        compare(apply.visible, true);
        compare(apply.enabled, false);
        compare(applyRequests, 1);

        page.busy = false;
        page.confirmationState = "awaiting-confirmation";
        compare(apply.visible, true);
        compare(apply.enabled, false);
        compare(applyRequests, 1);
    }

    function test_displayMutationsWaitForSharedVisualAuthority() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        waitForRendering(page);
        wait(0);

        let changed = page.clone(page.outputById("display-a"));
        changed.scale = 1.25;
        page.replaceOutput(changed);
        const preview = findChild(
            page,
            "previewDisplayConfigurationButton"
        );
        const status = findChild(page, "displayStatusMessage");
        verify(preview !== null);
        verify(status !== null);
        compare(page.controlsEnabled, true);
        compare(preview.enabled, true);

        page.sharedMutationBusy = true;
        compare(page.controlsEnabled, false);
        compare(preview.enabled, false);
        verify(String(status.text).includes("Shared visual"));

        page.sharedMutationBusy = false;
        page.sharedApplySafe = false;
        compare(page.controlsEnabled, false);
        compare(preview.enabled, false);
        verify(String(status.text).includes("exact verified"));

        page.managementState = "unmanaged";
        page.applyState = "unavailable";
        page.sharedApplySafe = true;
        wait(0);
        const adopt = findChild(page, "adoptCompositorButton");
        verify(adopt !== null);
        compare(adopt.visible, true);
        compare(adopt.enabled, true);
        page.sharedMutationBusy = true;
        compare(adopt.enabled, false);
        page.sharedMutationBusy = false;
        page.sharedApplySafe = false;
        compare(adopt.enabled, false);

        page.managementState = "managed";
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.appliedRevision = 6;
        page.sharedApplySafe = true;
        wait(0);
        const apply = findChild(page, "applyCompositorBaselineButton");
        verify(apply !== null);
        compare(apply.visible, true);
        compare(apply.enabled, true);
        page.sharedMutationBusy = true;
        compare(apply.enabled, false);
        page.sharedMutationBusy = false;
        page.sharedApplySafe = false;
        compare(apply.enabled, false);
    }

    function test_displayAuthorityOverridesStaleOperationError() {
        const rows = [
            {
                serviceAvailable: false,
                writable: true,
                managementState: "unmanaged",
                expectedStatus: "settings are unavailable"
            },
            {
                serviceAvailable: true,
                writable: false,
                managementState: "unmanaged",
                expectedStatus: "read-only"
            },
            {
                serviceAvailable: true,
                writable: true,
                managementState: "conflict",
                expectedStatus: "changed unexpectedly"
            }
        ];
        const staleDetail = "Stale transient operation detail.";

        for (const row of rows) {
            const testWindow = createTemporaryObject(
                displaysPageComponent,
                this
            );
            verify(testWindow !== null);
            const page = testWindow.page;
            configureDisplaysPage(
                page,
                [displayRecord("display-a", "DP-1", true, "", -1)],
                [connectedDisplay("DP-1", true, 0)]
            );
            page.serviceAvailable = row.serviceAvailable;
            page.writable = row.writable;
            page.managementState = row.managementState;
            page.applyState = "retained";
            page.requiredActivation = "reload";
            page.errorName = "org.hyprshelld.Error.OperationFailed";
            page.errorMessage = staleDetail;
            waitForRendering(page);
            wait(0);

            const takeControl = findChild(
                page,
                "adoptCompositorButton"
            );
            const apply = findChild(
                page,
                "applyCompositorBaselineButton"
            );
            const card = findChild(page, "displayStatusCard");
            const status = findChild(page, "displayStatusMessage");
            verify(takeControl !== null);
            verify(apply !== null);
            verify(card !== null);
            verify(status !== null);
            compare(takeControl.visible, false);
            compare(apply.visible, false);
            compare(card.visible, true);
            verify(String(status.text).includes(row.expectedStatus));
            verify(!String(status.text).includes(staleDetail));
        }
    }

    function test_displayAdoptionDialogFitsTheMinimumWindow() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this,
            { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        page.managementState = "unmanaged";
        page.applyState = "unavailable";
        waitForRendering(page);
        wait(0);

        const takeControl = findChild(page, "adoptCompositorButton");
        const dialog = findChild(page, "displayAdoptionDialog");
        const scroll = findChild(page, "displayAdoptionScrollView");
        const content = findChild(page, "displayAdoptionContent");
        const explanation = findChild(
            page,
            "displayAdoptionExplanation"
        );
        const cancel = findChild(
            page,
            "cancelDisplayAdoptionButton"
        );
        const confirm = findChild(
            page,
            "confirmDisplayAdoptionButton"
        );
        verify(takeControl !== null);
        verify(dialog !== null);
        verify(scroll !== null);
        verify(content !== null);
        verify(explanation !== null);
        verify(cancel !== null);
        verify(confirm !== null);

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        verify(dialog.width <= page.width);
        verify(dialog.height <= page.height);
        verify(scroll.width > 0);
        verify(scroll.height > 0);
        verify(content.width <= scroll.availableWidth + 1);

        // A shorter temporary viewport forces the bounded dialog onto its
        // scrolling path without moving the confirmation actions offscreen.
        testWindow.height = 360;
        wait(0);
        verify(dialog.height <= page.height);
        verify(scroll.contentItem.contentHeight
            > scroll.contentItem.height);

        const maximumContentY = scroll.contentItem.contentHeight
            - scroll.contentItem.height;
        verify(maximumContentY > 0);
        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);

        const explanationPosition = explanation.mapToItem(scroll, 0, 0);
        verify(explanationPosition.y + explanation.height
            <= scroll.height + 1);

        const cancelPosition = cancel.mapToItem(page, 0, 0);
        const confirmPosition = confirm.mapToItem(page, 0, 0);
        verify(cancelPosition.x >= 0);
        verify(cancelPosition.x + cancel.width <= page.width);
        verify(cancelPosition.y >= 0);
        verify(cancelPosition.y + cancel.height <= page.height);
        verify(confirmPosition.x >= 0);
        verify(confirmPosition.x + confirm.width <= page.width);
        verify(confirmPosition.y >= 0);
        verify(confirmPosition.y + confirm.height <= page.height);
    }

    function test_displayAdoptionDialogClosesWhenEligibilityIsLost() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        page.managementState = "unmanaged";
        page.applyState = "unavailable";
        waitForRendering(page);
        wait(0);

        const takeControl = findChild(page, "adoptCompositorButton");
        const dialog = findChild(page, "displayAdoptionDialog");
        const confirm = findChild(
            dialog,
            "confirmDisplayAdoptionButton"
        );
        verify(takeControl !== null);
        verify(dialog !== null);
        verify(confirm !== null);

        let adoptionRequests = 0;
        page.adoptionRequested.connect(function() {
            ++adoptionRequests;
        });

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        page.busy = true;
        tryCompare(dialog, "opened", false);
        compare(confirm.enabled, false);
        compare(adoptionRequests, 0);
        page.busy = false;

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        page.serviceAvailable = false;
        tryCompare(dialog, "opened", false);
        compare(confirm.enabled, false);
        compare(adoptionRequests, 0);
        page.serviceAvailable = true;

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        page.writable = false;
        tryCompare(dialog, "opened", false);
        compare(confirm.enabled, false);
        compare(adoptionRequests, 0);
        page.writable = true;

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        page.managementState = "conflict";
        tryCompare(dialog, "opened", false);
        compare(confirm.enabled, false);
        compare(adoptionRequests, 0);
        page.managementState = "unmanaged";

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        page.confirmationState = "awaiting-confirmation";
        tryCompare(dialog, "opened", false);
        compare(confirm.enabled, false);
        compare(adoptionRequests, 0);
    }

    function test_displayStatusMessagesDescribeAuthoritativeState() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        waitForRendering(page);
        wait(0);

        const card = findChild(page, "displayStatusCard");
        const status = findChild(page, "displayStatusMessage");
        verify(card !== null);
        verify(status !== null);
        compare(status.Accessible.role, Accessible.AlertMessage);
        compare(card.visible, false);

        page.loadState = "recovered";
        wait(0);
        compare(card.visible, true);
        verify(String(status.text).includes("last known good"));
        verify(String(status.text).includes("Review"));
        compare(status.Accessible.name, status.text);

        page.loadState = "defaulted";
        wait(0);
        verify(String(status.text).includes("safe defaults"));
        verify(String(status.text).includes("Review"));
        compare(status.Accessible.name, status.text);

        page.loadState = "normal";
        page.managementState = "unmanaged";
        wait(0);
        verify(String(status.text).includes("not managing"));
        verify(String(status.text).includes("does not import"));
        verify(String(status.text).includes("If hyprland.lua exists"));
        verify(String(status.text).includes("exact original"));
        verify(String(status.text).includes("preserved privately for recovery"));
        verify(String(status.text).includes("absence is recorded"));
        compare(status.Accessible.name, status.text);

        page.managementState = "conflict";
        wait(0);
        verify(String(status.text).includes("changed unexpectedly"));
        verify(String(status.text).includes("locked"));
        verify(String(status.text).includes(
            "ownership state"
        ));
        verify(String(status.text).includes(
            "If the desktop health warning reports Compositor settings"
        ));
        verify(String(status.text).includes(
            "preserve the unexpected files and seek recovery guidance"
        ));
        compare(status.Accessible.name, status.text);
    }

    function test_healthWarningIsQuietWhenHealthy() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        compare(warning.warningVisible, false);
        compare(warning.failedComponentCount, 0);
        compare(warning.visible, false);
    }

    function test_coordinatorFailureOffersOneRestart() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorHealthy = false;
        warning.coordinatorFailedUnits = ["hyprshelld-configd.service"];
        waitForRendering(warning);
        compare(warning.warningVisible, true);
        compare(warning.failedComponentCount, 1);
        compare(warning.friendlyName("hyprshelld-configd.service"), "Settings service");
        verify(!warning.friendlyName("hyprshelld-configd.service").includes(".service"));

        const restartButton = findChild(
            warning,
            "restartButton-hyprshelld-configd.service"
        );
        verify(restartButton !== null);
        compare(restartButton.visible, true);
        compare(restartButton.enabled, true);

        let requestedUnit = "";
        let requestCount = 0;
        warning.restartRequested.connect(function(unitName) {
            requestedUnit = unitName;
            ++requestCount;
        });
        restartButton.clicked();
        compare(requestedUnit, "hyprshelld-configd.service");
        compare(requestCount, 1);

        warning.restartBusy = true;
        warning.restartingUnit = "hyprshelld-configd.service";
        compare(restartButton.enabled, false);
        compare(restartButton.text, "Restarting…");

        warning.restartErrorUnit = "hyprshelld-configd.service";
        warning.restartError = "The restart request was rejected.";
        const error = findChild(warning, "restartError");
        verify(error !== null);
        compare(error.visible, true);
        verify(error.text.includes("Settings service"));

        warning.coordinatorFailedUnits = ["hyprshelld-surfaced.service"];
        compare(error.visible, false);
    }

    function test_coordinatorRestartAcceptsEnterKeys() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorHealthy = false;
        warning.coordinatorFailedUnits = ["hyprshelld-configd.service"];
        waitForRendering(warning);

        const restartButton = findChild(
            warning,
            "restartButton-hyprshelld-configd.service"
        );
        verify(restartButton !== null);

        let requestedUnit = "";
        let requestCount = 0;
        warning.restartRequested.connect(function(unitName) {
            requestedUnit = unitName;
            ++requestCount;
        });

        testWindow.requestActivate();
        restartButton.forceActiveFocus();
        tryCompare(restartButton, "activeFocus", true);

        keyClick(Qt.Key_Return);
        compare(requestedUnit, "hyprshelld-configd.service");
        compare(requestCount, 1);

        keyClick(Qt.Key_Enter);
        compare(requestCount, 2);
    }

    function test_componentManagerFailureHasDedicatedRow() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorHealthy = false;
        warning.coordinatorFailedUnits = [
            "hyprshelld-componentd.service"
        ];
        waitForRendering(warning);

        compare(warning.failedComponentCount, 1);
        compare(
            warning.friendlyName("hyprshelld-componentd.service"),
            "Component manager"
        );
        verify(findChild(
            warning,
            "restartButton-hyprshelld-componentd.service"
        ) !== null);
    }

    function test_compositorAuthorityFailureHasDedicatedRow() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorHealthy = false;
        warning.coordinatorFailedUnits = [
            "hyprshelld-compositord.service"
        ];
        waitForRendering(warning);

        compare(warning.failedComponentCount, 1);
        compare(
            warning.friendlyName("hyprshelld-compositord.service"),
            "Compositor settings"
        );
        verify(findChild(
            warning,
            "restartButton-hyprshelld-compositord.service"
        ) !== null);
    }

    function test_systemdFallbackIsReadOnly() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorAvailable = false;
        warning.fallbackActive = true;
        warning.fallbackAvailable = true;
        warning.targetState = "active";
        warning.coordinatorState = "failed";
        warning.configurationState = "active";
        warning.componentManagerState = "failed";
        warning.compositorState = "failed";
        warning.surfaceState = "active";
        waitForRendering(warning);
        compare(warning.warningVisible, true);
        compare(warning.failedComponentCount, 3);
        compare(warning.friendlyName("hyprshelld.service"), "Shell health");
        compare(
            warning.friendlyName("hyprshelld-componentd.service"),
            "Component manager"
        );
        compare(
            warning.friendlyName("hyprshelld-compositord.service"),
            "Compositor settings"
        );

        const restartButton = findChild(
            warning,
            "restartButton-hyprshelld.service"
        );
        verify(restartButton !== null);
        compare(restartButton.visible, false);
        const componentRestartButton = findChild(
            warning,
            "restartButton-hyprshelld-componentd.service"
        );
        verify(componentRestartButton !== null);
        compare(componentRestartButton.visible, false);
        const compositorRestartButton = findChild(
            warning,
            "restartButton-hyprshelld-compositord.service"
        );
        verify(compositorRestartButton !== null);
        compare(compositorRestartButton.visible, false);
        verify(warning.warningDescription.includes("directly from systemd"));
    }

    function test_appearanceUsesExactTrustedDefinitions() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        compare(page.trustedDefinitionsValid, true);
        compare(page.trustedValuesValid, true);
        compare(page.controlsEnabled, true);
        compare(page.expectedOptionIds.length, 40);
        compare(page.expectedOptionIds[7], page.dimInactiveId);
        compare(page.expectedOptionIds[8], page.dimStrengthId);
        compare(page.expectedOptionIds[9], page.activeOpacityId);
        compare(page.expectedOptionIds[10], page.inactiveOpacityId);
        compare(page.expectedOptionIds[11], page.fullscreenOpacityId);
        compare(page.expectedOptionIds[12], page.dimModalId);
        compare(page.expectedOptionIds[13], page.dimSpecialId);
        compare(page.expectedOptionIds[14], page.dimAroundId);
        compare(page.expectedOptionIds[15], page.blurSizeId);
        compare(page.expectedOptionIds[16], page.blurPassesId);
        compare(page.expectedOptionIds[17], page.blurIgnoreOpacityId);
        compare(page.expectedOptionIds[18], page.blurOptimizationsId);
        compare(page.expectedOptionIds[19], page.blurXrayId);
        compare(page.expectedOptionIds[20], page.blurSpecialId);
        compare(page.expectedOptionIds[21], page.blurPopupsId);
        compare(
            page.expectedOptionIds[22], page.blurPopupsIgnoreAlphaId
        );
        compare(page.expectedOptionIds[23], page.blurInputMethodsId);
        compare(
            page.expectedOptionIds[24],
            page.blurInputMethodsIgnoreAlphaId
        );
        compare(page.expectedOptionIds[25], page.blurBrightnessId);
        compare(page.expectedOptionIds[26], page.blurContrastId);
        compare(page.expectedOptionIds[27], page.blurNoiseId);
        compare(page.expectedOptionIds[28], page.blurVibrancyId);
        compare(
            page.expectedOptionIds[29], page.blurVibrancyDarknessId
        );
        compare(page.expectedOptionIds[30], page.borderPartOfWindowId);
        compare(page.expectedOptionIds[31], page.roundingPowerId);
        compare(page.expectedOptionIds[32], page.shadowRangeId);
        compare(page.expectedOptionIds[33], page.shadowRenderPowerId);
        compare(page.expectedOptionIds[34], page.shadowSharpId);
        compare(page.expectedOptionIds[35], page.shadowOffsetId);
        compare(page.expectedOptionIds[36], page.shadowScaleId);
        compare(page.expectedOptionIds[37], page.glowEnabledId);
        compare(page.expectedOptionIds[38], page.glowRangeId);
        compare(page.expectedOptionIds[39], page.glowRenderPowerId);

        const border = findChild(page, "appearanceBorderSize");
        const rounding = findChild(page, "appearanceRounding");
        const roundingPower = findChild(
            page, "appearanceRoundingPower"
        );
        const blur = findChild(page, "appearanceBlurEnabled");
        const shadow = findChild(page, "appearanceShadowEnabled");
        const shadowRange = findChild(page, "appearanceShadowRange");
        const shadowRenderPower = findChild(
            page, "appearanceShadowRenderPower"
        );
        const shadowSharp = findChild(page, "appearanceShadowSharp");
        const shadowOffsetX = findChild(page, "appearanceShadowOffsetX");
        const shadowOffsetY = findChild(page, "appearanceShadowOffsetY");
        const shadowScale = findChild(page, "appearanceShadowScale");
        const glowEnabled = findChild(page, "appearanceGlowEnabled");
        const glowRange = findChild(page, "appearanceGlowRange");
        const glowRenderPower = findChild(
            page, "appearanceGlowRenderPower"
        );
        const glowSafety = findChild(
            page, "appearanceGlowSafetyMessage"
        );
        const borderPartOfWindow = findChild(
            page, "appearanceBorderPartOfWindow"
        );
        const dimInactive = findChild(page, "appearanceDimInactive");
        const dimStrength = findChild(page, "appearanceDimStrength");
        const activeOpacity = findChild(page, "appearanceActiveOpacity");
        const inactiveOpacity = findChild(
            page, "appearanceInactiveOpacity"
        );
        const fullscreenOpacity = findChild(
            page, "appearanceFullscreenOpacity"
        );
        const dimModal = findChild(page, "appearanceDimModal");
        const dimSpecial = findChild(page, "appearanceDimSpecial");
        const dimAround = findChild(page, "appearanceDimAround");
        const blurSize = findChild(page, "appearanceBlurSize");
        const blurPasses = findChild(page, "appearanceBlurPasses");
        const blurIgnoreOpacity = findChild(
            page, "appearanceBlurIgnoreOpacity"
        );
        const blurOptimizations = findChild(
            page, "appearanceBlurOptimizations"
        );
        const blurXray = findChild(page, "appearanceBlurXray");
        const blurSpecial = findChild(page, "appearanceBlurSpecial");
        const blurPopups = findChild(page, "appearanceBlurPopups");
        const blurPopupsIgnoreAlpha = findChild(
            page, "appearanceBlurPopupsIgnoreAlpha"
        );
        const blurInputMethods = findChild(
            page, "appearanceBlurInputMethods"
        );
        const blurInputMethodsIgnoreAlpha = findChild(
            page, "appearanceBlurInputMethodsIgnoreAlpha"
        );
        const blurBrightness = findChild(
            page, "appearanceBlurBrightness"
        );
        const blurContrast = findChild(page, "appearanceBlurContrast");
        const blurNoise = findChild(page, "appearanceBlurNoise");
        const blurVibrancy = findChild(page, "appearanceBlurVibrancy");
        const blurVibrancyDarkness = findChild(
            page, "appearanceBlurVibrancyDarkness"
        );
        const animations = findChild(page, "appearanceAnimationsEnabled");
        const preview = findChild(page, "appearancePreview");
        const previewStage = findChild(page, "appearancePreviewStage");
        const summary = findChild(page, "appearancePreviewSummary");
        const motionToggle = findChild(
            page,
            "toggleAppearanceMotionButton"
        );
        verify(border !== null);
        verify(rounding !== null);
        verify(roundingPower !== null);
        verify(blur !== null);
        verify(shadow !== null);
        verify(shadowRange !== null);
        verify(shadowRenderPower !== null);
        verify(shadowSharp !== null);
        verify(shadowOffsetX !== null);
        verify(shadowOffsetY !== null);
        verify(shadowScale !== null);
        verify(glowEnabled !== null);
        verify(glowRange !== null);
        verify(glowRenderPower !== null);
        verify(glowSafety !== null);
        verify(borderPartOfWindow !== null);
        verify(dimInactive !== null);
        verify(dimStrength !== null);
        verify(activeOpacity !== null);
        verify(inactiveOpacity !== null);
        verify(fullscreenOpacity !== null);
        verify(dimModal !== null);
        verify(dimSpecial !== null);
        verify(dimAround !== null);
        verify(blurSize !== null);
        verify(blurPasses !== null);
        verify(blurIgnoreOpacity !== null);
        verify(blurOptimizations !== null);
        verify(blurXray !== null);
        verify(blurSpecial !== null);
        verify(blurPopups !== null);
        verify(blurPopupsIgnoreAlpha !== null);
        verify(blurInputMethods !== null);
        verify(blurInputMethodsIgnoreAlpha !== null);
        verify(blurBrightness !== null);
        verify(blurContrast !== null);
        verify(blurNoise !== null);
        verify(blurVibrancy !== null);
        verify(blurVibrancyDarkness !== null);
        verify(animations !== null);
        compare(findChild(page, "appearanceLayout"), null);
        compare(findChild(page, "appearanceResizeOnBorder"), null);
        compare(findChild(page, "appearanceSnapEnabled"), null);
        verify(preview !== null);
        verify(previewStage !== null);
        verify(summary !== null);
        verify(motionToggle !== null);

        compare(border.from, 0);
        compare(border.to, 20);
        compare(border.value, 1);
        compare(rounding.from, 0);
        compare(rounding.to, 20);
        compare(rounding.value, 0);
        compare(roundingPower.text, "2");
        compare(roundingPower.inputValid, true);
        compare(roundingPower.enabled, true);
        compare(roundingPower.Accessible.name, "Window corner power");
        compare(blur.checked, true);
        compare(shadow.checked, true);
        compare(shadowRange.from, 0);
        compare(shadowRange.to, 100);
        compare(shadowRange.value, 4);
        compare(shadowRange.enabled, true);
        compare(shadowRenderPower.from, 1);
        compare(shadowRenderPower.to, 4);
        compare(shadowRenderPower.value, 3);
        compare(shadowRenderPower.enabled, true);
        compare(shadowSharp.checked, false);
        compare(shadowSharp.enabled, true);
        compare(shadowOffsetX.text, "0");
        compare(shadowOffsetY.text, "0");
        compare(shadowOffsetX.minimumValue, -250);
        compare(shadowOffsetX.maximumValue, 250);
        compare(shadowOffsetY.minimumValue, -250);
        compare(shadowOffsetY.maximumValue, 250);
        compare(shadowOffsetX.inputValid, true);
        compare(shadowOffsetY.inputValid, true);
        compare(shadowOffsetX.enabled, true);
        compare(shadowOffsetY.enabled, true);
        compare(shadowScale.text, "1");
        compare(shadowScale.minimumValue, 0);
        compare(shadowScale.maximumValue, 1);
        compare(shadowScale.inputValid, true);
        compare(shadowScale.enabled, true);
        compare(
            shadowOffsetX.Accessible.name, "Horizontal shadow offset"
        );
        compare(shadowOffsetY.Accessible.name, "Vertical shadow offset");
        compare(shadowScale.Accessible.name, "Shadow scale");
        compare(glowEnabled.checked, false);
        compare(glowEnabled.enabled, true);
        compare(glowEnabled.Accessible.name, "Inner window glow");
        compare(glowRange.from, 0);
        compare(glowRange.to, 100);
        compare(glowRange.value, 10);
        compare(glowRange.enabled, true);
        compare(glowRange.Accessible.name, "Glow range");
        compare(glowRenderPower.from, 1);
        compare(glowRenderPower.to, 4);
        compare(glowRenderPower.value, 3);
        compare(glowRenderPower.enabled, false);
        compare(glowRenderPower.Accessible.name, "Glow falloff");
        compare(glowSafety.visible, false);
        compare(glowSafety.Accessible.role, Accessible.AlertMessage);
        compare(borderPartOfWindow.checked, true);
        compare(borderPartOfWindow.enabled, true);
        compare(
            borderPartOfWindow.Accessible.name,
            "Include borders in window shadows"
        );
        compare(dimInactive.checked, false);
        compare(dimStrength.from, 0);
        compare(dimStrength.to, 1);
        compare(dimStrength.value, 0.5);
        compare(dimStrength.stepSize, 0.05);
        compare(dimStrength.enabled, false);
        for (const slider of [activeOpacity, inactiveOpacity,
                             fullscreenOpacity, dimSpecial, dimAround]) {
            compare(slider.from, 0);
            compare(slider.to, 1);
            compare(slider.stepSize, 0.05);
            compare(slider.enabled, true);
        }
        compare(activeOpacity.value, 1);
        compare(inactiveOpacity.value, 1);
        compare(fullscreenOpacity.value, 1);
        compare(dimModal.checked, true);
        compare(dimSpecial.value, 0.2);
        compare(dimAround.value, 0.4);
        compare(blurSize.from, 0);
        compare(blurSize.to, 100);
        compare(blurSize.value, 8);
        compare(blurSize.enabled, true);
        compare(blurPasses.from, 0);
        compare(blurPasses.to, 10);
        compare(blurPasses.value, 1);
        compare(blurPasses.enabled, true);
        compare(blurIgnoreOpacity.checked, true);
        compare(blurIgnoreOpacity.enabled, true);
        compare(blurOptimizations.checked, true);
        compare(blurOptimizations.enabled, true);
        compare(blurXray.checked, false);
        compare(blurXray.enabled, true);
        compare(blurSpecial.checked, false);
        compare(blurSpecial.enabled, true);
        compare(blurPopups.checked, false);
        compare(blurPopups.enabled, true);
        compare(blurPopupsIgnoreAlpha.from, 0);
        compare(blurPopupsIgnoreAlpha.to, 1);
        compare(blurPopupsIgnoreAlpha.value, 0.2);
        compare(blurPopupsIgnoreAlpha.stepSize, 0.05);
        compare(blurPopupsIgnoreAlpha.enabled, false);
        compare(blurInputMethods.checked, false);
        compare(blurInputMethods.enabled, true);
        compare(blurInputMethodsIgnoreAlpha.from, 0);
        compare(blurInputMethodsIgnoreAlpha.to, 1);
        compare(blurInputMethodsIgnoreAlpha.value, 0.2);
        compare(blurInputMethodsIgnoreAlpha.stepSize, 0.05);
        compare(blurInputMethodsIgnoreAlpha.enabled, false);
        compare(blurBrightness.text, "1");
        compare(blurContrast.text, "0.8916");
        compare(blurNoise.text, "0.0117");
        compare(blurVibrancy.text, "0.1696");
        compare(blurVibrancyDarkness.text, "0");
        for (const field of [blurBrightness, blurContrast, blurNoise,
                             blurVibrancy, blurVibrancyDarkness]) {
            compare(field.inputValid, true);
            compare(field.enabled, true);
        }
        compare(animations.checked, true);
        compare(previewStage.Accessible.ignored, true);
        compare(summary.Accessible.name, summary.text);
        verify(String(summary.text).includes("Dwindle layout"));
        verify(String(summary.text).includes(
            "Shadows on. Shadow range 4. Soft-shadow falloff power 3. Sharp shadow edges off."
        ));
        verify(String(summary.text).includes(
            "Horizontal shadow offset 0. Vertical shadow offset 0."
        ));
        verify(String(summary.text).includes(
            "Visible borders included in window-shadow bounds on"
        ));
        verify(String(summary.text).includes("Window corner power 2"));
        verify(String(summary.text).includes("Active-window opacity 1.00"));
        verify(String(summary.text).includes(
            "Inactive-window opacity 1.00"
        ));
        verify(String(summary.text).includes(
            "True-fullscreen opacity 1.00"
        ));
        verify(String(summary.text).includes("Modal-parent dimming on"));
        verify(String(summary.text).includes(
            "Special-workspace dimming 0.20"
        ));
        verify(String(summary.text).includes("Dim-around strength 0.40"));
        verify(String(summary.text).includes("Blur size 8"));
        verify(String(summary.text).includes("Blur passes 1"));
        verify(String(summary.text).includes("Blur ignores opacity on"));
        verify(String(summary.text).includes("Optimized blur path on"));
        verify(String(summary.text).includes("X-ray blur off"));
        verify(String(summary.text).includes("Blur brightness 1"));
        verify(String(summary.text).includes("Blur contrast 0.8916"));
        verify(String(summary.text).includes("Blur noise 0.0117"));
        verify(String(summary.text).includes("Blur vibrancy 0.1696"));
        verify(String(summary.text).includes("Dark-area vibrancy 0"));
        verify(String(summary.text).includes("Special-workspace blur off"));
        verify(String(summary.text).includes("Popup blur off"));
        verify(String(summary.text).includes(
            "Popup ignore-alpha threshold 0.20"
        ));
        verify(String(summary.text).includes("Input-method blur off"));
        verify(String(summary.text).includes(
            "Input-method ignore-alpha threshold 0.20"
        ));
        verify(String(summary.text).includes(
            "Inner window glow off. Glow range 10. Glow falloff power 3."
        ));
        compare(motionToggle.enabled, true);
        compare(motionToggle.text, "Pause motion");
        compare(
            motionToggle.Accessible.name,
            "Pause the illustrative window motion"
        );
        compare(preview.motionRunning, true);
        compare(preview.motionPaused, false);
        compare(preview.motionStory, "dwindle-split");
        verify(String(summary.text).includes("Motion playing"));

        const targetIds = [
            "refreshAppearanceButton",
            "appearanceOpenDisplaysButton",
            "loadCurrentAppearanceButton",
            "retryApplyAppearanceButton",
            "recoverAppearanceButton",
            "windowBorderSourceButton",
            "appearanceBorderSize",
            "appearanceRounding",
            "appearanceRoundingPower",
            "appearanceBlurEnabled",
            "appearanceShadowEnabled",
            "appearanceShadowRange",
            "appearanceShadowRenderPower",
            "appearanceShadowSharp",
            "appearanceShadowOffsetX",
            "appearanceShadowOffsetY",
            "appearanceGlowEnabled",
            "appearanceGlowRange",
            "appearanceGlowRenderPower",
            "appearanceBorderPartOfWindow",
            "appearanceDimInactive",
            "appearanceDimStrength",
            "appearanceActiveOpacity",
            "appearanceInactiveOpacity",
            "appearanceFullscreenOpacity",
            "appearanceDimModal",
            "appearanceDimSpecial",
            "appearanceDimAround",
            "appearanceBlurSize",
            "appearanceBlurPasses",
            "appearanceBlurIgnoreOpacity",
            "appearanceBlurOptimizations",
            "appearanceBlurXray",
            "appearanceBlurSpecial",
            "appearanceBlurPopups",
            "appearanceBlurPopupsIgnoreAlpha",
            "appearanceBlurInputMethods",
            "appearanceBlurInputMethodsIgnoreAlpha",
            "appearanceBlurBrightness",
            "appearanceBlurContrast",
            "appearanceBlurNoise",
            "appearanceBlurVibrancy",
            "appearanceBlurVibrancyDarkness",
            "appearanceAnimationsEnabled",
            "discardAppearanceDraftButton",
            "resetAppearanceDefaultsButton",
            "saveAppearanceButton",
            "toggleAppearanceMotionButton",
            "cancelAppearanceRecoveryButton",
            "confirmAppearanceRecoveryButton"
        ];
        for (const objectName of targetIds) {
            const target = findChild(page, objectName);
            verify(target !== null, "Missing target " + objectName);
            verify(
                target.implicitHeight >= 44,
                objectName + " must provide a 44px interaction target"
            );
        }

        page.setDraftValue(page.animationsId, false);
        wait(0);
        compare(motionToggle.enabled, false);
        compare(motionToggle.text, "Motion off");
        compare(
            motionToggle.Accessible.name,
            "Illustrative window motion is off"
        );
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "off");
        verify(String(summary.text).includes("Dwindle layout"));
        verify(String(summary.text).includes("Animations off"));
        compare(
            findChild(page, "appearancePreviewSnapGuide").visible,
            false
        );
    }

    function test_appearanceGlowKeepsLegacyRepairAvailableAndPreviewSummaryOnly() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const baseline = appearanceDefaults();
        baseline["hyprland.decoration.glow.enabled"] = true;
        baseline["hyprland.decoration.glow.range"] = 9;
        baseline["hyprland.decoration.glow.render_power"] = 2;
        configureAppearancePage(page, baseline);
        waitForRendering(page);
        wait(0);

        const enabledRow = findChild(page, "appearanceGlowEnabledRow");
        const enabled = findChild(page, "appearanceGlowEnabled");
        const card = findChild(page, "appearanceGlowRenderingCard");
        const rangeRow = findChild(page, "appearanceGlowRangeRow");
        const range = findChild(page, "appearanceGlowRange");
        const powerRow = findChild(
            page, "appearanceGlowRenderPowerRow"
        );
        const power = findChild(page, "appearanceGlowRenderPower");
        const safety = findChild(page, "appearanceGlowSafetyMessage");
        const save = findChild(page, "saveAppearanceButton");
        const retry = findChild(page, "retryApplyAppearanceButton");
        const reset = findChild(page, "resetAppearanceDefaultsButton");
        const preview = findChild(page, "appearancePreview");
        const activeWindow = findChild(
            page, "appearancePreviewActiveWindow"
        );
        const summary = findChild(page, "appearancePreviewSummary");
        const disclaimer = findChild(
            page, "appearanceAnimationPreviewDisclaimer"
        );
        for (const item of [enabledRow, enabled, card, rangeRow, range,
                            powerRow, power, safety, save, retry, reset, preview,
                            activeWindow, summary, disclaimer]) {
            verify(item !== null);
        }
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        wait(0);

        compare(page.trustedValuesValid, true);
        compare(page.controlsEnabled, true);
        compare(page.draftValuesValid, true);
        compare(page.draftValid, false);
        compare(page.draftDirty, false);
        compare(enabled.checked, true);
        compare(enabled.enabled, true);
        verify(String(enabledRow.description).includes(
            "Glow colors are not edited on this page"
        ));
        compare(range.value, 9);
        compare(range.enabled, true);
        compare(power.value, 2);
        compare(power.enabled, true);
        compare(safety.visible, true);
        compare(safety.Accessible.role, Accessible.AlertMessage);
        compare(safety.Accessible.name, safety.text);
        verify(String(safety.text).includes("range below 10"));
        compare(save.enabled, false);
        compare(retry.visible, true);
        compare(retry.enabled, false);
        verify(String(summary.text).includes(
            "Inner window glow on. Glow range 9. Glow falloff power 2."
        ));
        verify(String(disclaimer.text).includes(
            "Inner glow size, falloff, color, opacity, blur, and motion are not simulated."
        ));
        verify(String(disclaimer.text).includes("not simulated"));

        const previewX = activeWindow.x;
        const previewY = activeWindow.y;
        const previewWidth = activeWindow.width;
        const previewHeight = activeWindow.height;

        enabled.checked = false;
        enabled.clicked();
        wait(0);
        compare(page.draftValue(page.glowEnabledId), false);
        compare(page.draftValue(page.glowRangeId), 9);
        compare(page.draftValue(page.glowRenderPowerId), 2);
        compare(page.draftValid, true);
        compare(page.draftDirty, true);
        compare(safety.visible, false);
        compare(range.enabled, true);
        compare(power.enabled, false);
        compare(enabled.enabled, false);
        compare(save.enabled, true);
        compare(retry.enabled, false);

        page.setDraftValue(page.glowRangeId, 10);
        wait(0);
        compare(range.value, 10);
        compare(enabled.enabled, true);
        compare(power.enabled, false);
        enabled.checked = true;
        enabled.clicked();
        wait(0);
        compare(page.draftValue(page.glowEnabledId), true);
        compare(page.draftValid, true);
        compare(power.enabled, true);

        page.setDraftValue(page.glowRangeId, 9);
        wait(0);
        compare(page.draftValuesValid, true);
        compare(page.draftValid, false);
        compare(page.controlsEnabled, true);
        compare(enabled.enabled, true);
        compare(range.enabled, true);
        compare(power.enabled, true);
        compare(safety.visible, true);
        compare(save.enabled, false);

        compare(activeWindow.x, previewX);
        compare(activeWindow.y, previewY);
        compare(activeWindow.width, previewWidth);
        compare(activeWindow.height, previewHeight);

        reset.clicked();
        wait(0);
        compare(page.draftValue(page.glowEnabledId), false);
        compare(page.draftValue(page.glowRangeId), 10);
        compare(page.draftValue(page.glowRenderPowerId), 3);
        compare(page.draftValid, true);
        compare(safety.visible, false);
        compare(enabled.enabled, true);
        compare(range.enabled, true);
        compare(power.enabled, false);
        compare(preview.glowEnabled, false);
        compare(preview.glowRange, 10);
        compare(preview.glowRenderPower, 3);
        compare(retry.enabled, false);

        const safeWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(safeWindow !== null);
        const safePage = safeWindow.page;
        configureAppearancePage(safePage);
        safePage.applyState = "retained";
        safePage.requiredActivation = "reload";
        safePage.retryApplyAvailable = true;
        waitForRendering(safePage);
        wait(0);
        const safeRetry = findChild(
            safePage, "retryApplyAppearanceButton"
        );
        verify(safeRetry !== null);
        compare(safeRetry.enabled, true);
        safePage.setDraftValue(safePage.glowEnabledId, true);
        safePage.setDraftValue(safePage.glowRangeId, 9);
        wait(0);
        compare(safePage.draftValid, false);
        compare(safePage.authoritativeGlowSafe, true);
        compare(safeRetry.enabled, true);
    }

    function test_appearanceBorderShadowBoundsAreRetainedAndPreviewHonest() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const baseline = appearanceDefaults();
        baseline["hyprland.decoration.border_part_of_window"] = false;
        configureAppearancePage(page, baseline);
        waitForRendering(page);
        wait(0);

        const row = findChild(page, "appearanceBorderPartOfWindowRow");
        const control = findChild(
            page, "appearanceBorderPartOfWindow"
        );
        const preview = findChild(page, "appearancePreview");
        const activeWindow = findChild(
            page, "appearancePreviewActiveWindow"
        );
        const summary = findChild(page, "appearancePreviewSummary");
        const disclaimer = findChild(
            page, "appearanceAnimationPreviewDisclaimer"
        );
        for (const item of [row, control, preview, activeWindow,
                            summary, disclaimer]) {
            verify(item !== null);
        }

        compare(row.title, "Include borders in window shadows");
        compare(
            row.description,
            "Size each window shadow from the outside edge of its visible border. The saved choice is retained while window shadows are off."
        );
        compare(
            control.Accessible.name,
            "Include borders in window shadows"
        );
        verify(control.implicitHeight >= page.minimumTargetSize);
        compare(page.draftValue(page.borderPartOfWindowId), false);
        compare(control.checked, false);
        compare(control.enabled, true);
        compare(preview.borderPartOfWindow, false);
        verify(String(summary.text).includes(
            "Visible borders included in window-shadow bounds off"
        ));
        verify(String(disclaimer.text).includes(
            "border-inclusive shadow bounds"
        ));

        preview.motionPaused = true;
        wait(0);
        const previewX = activeWindow.x;
        const previewWidth = activeWindow.width;
        control.checked = true;
        control.clicked();
        wait(0);
        compare(page.draftValue(page.borderPartOfWindowId), true);
        compare(preview.borderPartOfWindow, true);
        verify(String(summary.text).includes(
            "Visible borders included in window-shadow bounds on"
        ));
        compare(activeWindow.x, previewX);
        compare(activeWindow.width, previewWidth);

        page.setDraftValue(page.shadowId, false);
        wait(0);
        compare(control.enabled, false);
        compare(page.draftValue(page.borderPartOfWindowId), true);
        compare(preview.borderPartOfWindow, true);
        verify(String(summary.text).includes("Shadows off"));

        page.setDraftValue(page.shadowId, true);
        wait(0);
        compare(control.enabled, true);
        compare(page.draftValue(page.borderPartOfWindowId), true);

        findChild(page, "discardAppearanceDraftButton").clicked();
        compare(page.draftValue(page.borderPartOfWindowId), false);
        compare(page.draftDirty, false);

        findChild(page, "resetAppearanceDefaultsButton").clicked();
        compare(page.draftValue(page.borderPartOfWindowId), true);
        compare(page.draftDirty, true);
    }

    function test_appearanceShadowRenderingRetainsDependenciesAndIsSummaryOnly() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const baseline = appearanceDefaults();
        baseline["hyprland.decoration.shadow.range"] = 17;
        baseline["hyprland.decoration.shadow.render_power"] = 4;
        baseline["hyprland.decoration.shadow.offset"] = [12.5, -8.25];
        baseline["hyprland.decoration.shadow.scale"] = 0.75;
        configureAppearancePage(page, baseline);
        waitForRendering(page);
        wait(0);

        const card = findChild(page, "appearanceShadowRenderingCard");
        const rangeRow = findChild(page, "appearanceShadowRangeRow");
        const powerRow = findChild(
            page, "appearanceShadowRenderPowerRow"
        );
        const sharpRow = findChild(page, "appearanceShadowSharpRow");
        const scaleRow = findChild(page, "appearanceShadowScaleRow");
        const offsetXRow = findChild(
            page, "appearanceShadowOffsetXRow"
        );
        const offsetYRow = findChild(
            page, "appearanceShadowOffsetYRow"
        );
        const range = findChild(page, "appearanceShadowRange");
        const power = findChild(page, "appearanceShadowRenderPower");
        const sharp = findChild(page, "appearanceShadowSharp");
        const scale = findChild(page, "appearanceShadowScale");
        const offsetX = findChild(page, "appearanceShadowOffsetX");
        const offsetY = findChild(page, "appearanceShadowOffsetY");
        const visualLastRow = findChild(
            page, "appearanceDimStrengthRow"
        );
        const blurCard = findChild(page, "appearanceBlurRenderingCard");
        const preview = findChild(page, "appearancePreview");
        const activeWindow = findChild(
            page, "appearancePreviewActiveWindow"
        );
        const secondaryShadow = findChild(
            page, "appearancePreviewSecondaryShadow"
        );
        const summary = findChild(page, "appearancePreviewSummary");
        const disclaimer = findChild(
            page, "appearanceAnimationPreviewDisclaimer"
        );
        for (const item of [card, rangeRow, powerRow, sharpRow, scaleRow,
                            offsetXRow, offsetYRow, range, power, sharp,
                            scale, offsetX, offsetY, visualLastRow, blurCard,
                            preview, activeWindow, secondaryShadow,
                            summary, disclaimer]) {
            verify(item !== null);
        }

        compare(rangeRow.title, "Shadow range");
        compare(
            rangeRow.description,
            "Set how far each window shadow extends beyond its window in layout pixels. This value is retained while window shadows are off and still controls the extent of sharp shadows."
        );
        compare(powerRow.title, "Soft-shadow falloff");
        compare(
            powerRow.description,
            "Choose the soft-shadow falloff power from 1 through 4. Higher values fade more quickly. This value is retained while window shadows are off or sharp edges are enabled."
        );
        compare(sharpRow.title, "Sharp shadow edges");
        compare(
            sharpRow.description,
            "Draw a solid-edged shadow instead of a soft falloff. Shadow range still controls its extent; the saved soft falloff returns when this is off."
        );
        compare(scaleRow.title, "Shadow scale");
        compare(
            scaleRow.description,
            "Scale each window shadow around its center from 0 through 1. The default 1 keeps its full size; lower exact values shrink it, and 0 makes it invisible. The saved value is retained while window shadows are off and applies to both soft and sharp shadows."
        );
        compare(offsetXRow.title, "Horizontal shadow offset");
        compare(
            offsetXRow.description,
            "Move every window shadow horizontally in layout pixels. Positive values move right and negative values move left. The exact saved value is retained while window shadows are off."
        );
        compare(offsetYRow.title, "Vertical shadow offset");
        compare(
            offsetYRow.description,
            "Move every window shadow vertically in layout pixels. Positive values move down and negative values move up. The exact saved value is retained while window shadows are off."
        );
        compare(range.from, 0);
        compare(range.to, 100);
        compare(range.value, 17);
        compare(power.from, 1);
        compare(power.to, 4);
        compare(power.value, 4);
        compare(sharp.checked, false);
        compare(scale.text, "0.75");
        compare(scale.minimumValue, 0);
        compare(scale.maximumValue, 1);
        compare(scale.inputValid, true);
        compare(offsetX.text, "12.5");
        compare(offsetY.text, "-8.25");
        compare(offsetX.minimumValue, -250);
        compare(offsetX.maximumValue, 250);
        compare(offsetY.minimumValue, -250);
        compare(offsetY.maximumValue, 250);
        compare(range.Accessible.name, "Shadow range");
        compare(power.Accessible.name, "Soft-shadow falloff");
        compare(sharp.Accessible.name, "Sharp shadow edges");
        compare(scale.Accessible.name, "Shadow scale");
        compare(offsetX.Accessible.name, "Horizontal shadow offset");
        compare(offsetY.Accessible.name, "Vertical shadow offset");
        for (const control of [range, power, sharp, scale, offsetX, offsetY]) {
            verify(control.implicitHeight >= page.minimumTargetSize);
            compare(control.enabled, true);
        }

        const visualLastY = visualLastRow.mapToItem(page, 0, 0).y;
        const cardY = card.mapToItem(page, 0, 0).y;
        const blurCardY = blurCard.mapToItem(page, 0, 0).y;
        verify(cardY > visualLastY);
        verify(cardY < blurCardY);
        verify(rangeRow.mapToItem(page, 0, 0).y
            < powerRow.mapToItem(page, 0, 0).y);
        verify(powerRow.mapToItem(page, 0, 0).y
            < sharpRow.mapToItem(page, 0, 0).y);
        verify(sharpRow.mapToItem(page, 0, 0).y
            < scaleRow.mapToItem(page, 0, 0).y);
        verify(scaleRow.mapToItem(page, 0, 0).y
            < offsetXRow.mapToItem(page, 0, 0).y);
        verify(offsetXRow.mapToItem(page, 0, 0).y
            < offsetYRow.mapToItem(page, 0, 0).y);

        preview.motionPaused = true;
        preview.motionProgress = 1;
        wait(0);
        const previewGeometry = [
            activeWindow.x, activeWindow.y, activeWindow.width,
            activeWindow.height, activeWindow.radius,
            activeWindow.opacity, String(activeWindow.color),
            secondaryShadow.x, secondaryShadow.y,
            secondaryShadow.width, secondaryShadow.height,
            secondaryShadow.radius, secondaryShadow.opacity,
            String(secondaryShadow.color), secondaryShadow.visible
        ];

        page.setDraftValue(page.shadowRangeId, 91);
        page.setDraftValue(page.shadowRenderPowerId, 2);
        page.setDraftValue(page.shadowSharpId, true);
        page.setExactDecimalDraftValue(page.shadowScaleId, 0.625);
        page.setExactVectorComponentDraftValue(
            page.shadowOffsetId, 0, 125.5
        );
        compare(page.draftValue(page.shadowOffsetId), [125.5, -8.25]);
        compare(page.appearanceValues[page.shadowOffsetId], [12.5, -8.25]);
        page.setExactVectorComponentDraftValue(
            page.shadowOffsetId, 1, -0
        );
        compare(page.draftValue(page.shadowOffsetId), [125.5, 0]);
        verify(!Object.is(page.draftValue(page.shadowOffsetId)[1], -0));
        page.setExactVectorComponentDraftValue(
            page.shadowOffsetId, 1, -80.25
        );
        wait(0);
        compare(preview.shadowRange, 91);
        compare(preview.shadowRenderPower, 2);
        compare(preview.shadowSharp, true);
        compare(preview.shadowScale, 0.625);
        compare(preview.shadowOffsetX, 125.5);
        compare(preview.shadowOffsetY, -80.25);
        verify(String(summary.text).includes(
            "Shadows on. Shadow range 91. Soft-shadow falloff power 2. Sharp shadow edges on."
        ));
        verify(String(summary.text).includes(
            "Horizontal shadow offset 125.5. Vertical shadow offset -80.25."
        ));
        verify(String(summary.text).includes("Shadow scale 0.625."));
        compare(power.enabled, false);
        compare(range.enabled, true);
        compare(sharp.enabled, true);
        compare(scale.enabled, true);
        compare(offsetX.enabled, true);
        compare(offsetY.enabled, true);
        compare([
            activeWindow.x, activeWindow.y, activeWindow.width,
            activeWindow.height, activeWindow.radius,
            activeWindow.opacity, String(activeWindow.color),
            secondaryShadow.x, secondaryShadow.y,
            secondaryShadow.width, secondaryShadow.height,
            secondaryShadow.radius, secondaryShadow.opacity,
            String(secondaryShadow.color), secondaryShadow.visible
        ], previewGeometry);
        verify(String(disclaimer.text).includes(
            "Shadow range, falloff, sharp edges, shadow offset, and shadow scale are not simulated"
        ));

        page.setDraftValue(page.shadowId, false);
        wait(0);
        compare(range.enabled, false);
        compare(power.enabled, false);
        compare(sharp.enabled, false);
        compare(scale.enabled, false);
        compare(offsetX.enabled, false);
        compare(offsetY.enabled, false);
        compare(page.draftValue(page.shadowRangeId), 91);
        compare(page.draftValue(page.shadowRenderPowerId), 2);
        compare(page.draftValue(page.shadowSharpId), true);
        compare(page.draftValue(page.shadowScaleId), 0.625);
        page.setExactDecimalDraftValue(page.shadowScaleId, 0.5);
        compare(page.draftValue(page.shadowScaleId), 0.625);
        compare(page.draftValue(page.shadowOffsetId), [125.5, -80.25]);
        compare(preview.shadowRange, 91);
        compare(preview.shadowRenderPower, 2);
        compare(preview.shadowSharp, true);
        compare(preview.shadowScale, 0.625);
        compare(preview.shadowOffsetX, 125.5);
        compare(preview.shadowOffsetY, -80.25);

        page.setDraftValue(page.shadowId, true);
        wait(0);
        compare(range.enabled, true);
        compare(power.enabled, false);
        compare(sharp.enabled, true);
        compare(scale.enabled, true);
        compare(offsetX.enabled, true);
        compare(offsetY.enabled, true);
        page.setDraftValue(page.shadowSharpId, false);
        wait(0);
        compare(power.enabled, true);
        compare(scale.enabled, true);
        compare(page.draftValue(page.shadowRenderPowerId), 2);

        findChild(page, "resetAppearanceDefaultsButton").clicked();
        wait(0);
        compare(page.draftValue(page.shadowRangeId), 4);
        compare(page.draftValue(page.shadowRenderPowerId), 3);
        compare(page.draftValue(page.shadowSharpId), false);
        compare(page.draftValue(page.shadowScaleId), 1);
        compare(page.draftValue(page.shadowOffsetId), [0, 0]);
        compare(range.value, 4);
        compare(power.value, 3);
        compare(sharp.checked, false);
        compare(scale.text, "1");
        compare(offsetX.text, "0");
        compare(offsetY.text, "0");
    }

    function test_appearanceShadowScaleExactDraftValidationAndRecovery() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const row = findChild(page, "appearanceShadowScaleRow");
        verify(row !== null);
        let hydrationEditCount = 0;
        row.valueModified.connect(function() {
            ++hydrationEditCount;
        });

        const baseline = appearanceDefaults();
        baseline[page.shadowScaleId] = Number.MIN_VALUE;
        configureAppearancePage(page, baseline);
        page.reviewProjection();
        waitForRendering(page);
        wait(0);
        compare(hydrationEditCount, 0);

        const field = findChild(page, "appearanceShadowScale");
        const validation = findChild(
            page, "appearanceShadowScaleValidation"
        );
        const aggregateValidation = findChild(
            page, "appearanceDraftValidationMessage"
        );
        const save = findChild(page, "saveAppearanceButton");
        const discard = findChild(
            page, "discardAppearanceDraftButton"
        );
        const reset = findChild(page, "resetAppearanceDefaultsButton");
        const loadCurrent = findChild(
            page, "loadCurrentAppearanceButton"
        );
        const refresh = findChild(page, "refreshAppearanceButton");
        const preview = findChild(page, "appearancePreview");
        const activeWindow = findChild(
            page, "appearancePreviewActiveWindow"
        );
        const secondaryShadow = findChild(
            page, "appearancePreviewSecondaryShadow"
        );
        const summary = findChild(page, "appearancePreviewSummary");
        for (const item of [field, validation, aggregateValidation, save,
                            discard, reset, loadCurrent, refresh, preview,
                            activeWindow, secondaryShadow, summary]) {
            verify(item !== null);
        }

        const minimumPlainDecimal = "0." + "0".repeat(323) + "5";
        compare(field.text, minimumPlainDecimal);
        compare(field.maximumLength, 326);
        compare(field.inputValid, true);
        compare(page.valuesEqual(page.draftValues, baseline), true);
        compare(page.draftDirty, false);
        compare(preview.shadowScale, Number.MIN_VALUE);

        preview.motionPaused = true;
        preview.motionProgress = 1;
        wait(0);
        const previewState = [
            activeWindow.x, activeWindow.y, activeWindow.width,
            activeWindow.height, activeWindow.radius, activeWindow.opacity,
            String(activeWindow.color), secondaryShadow.x,
            secondaryShadow.y, secondaryShadow.width,
            secondaryShadow.height, secondaryShadow.radius,
            secondaryShadow.opacity, String(secondaryShadow.color),
            secondaryShadow.visible
        ];

        for (const rejected of [-0.0001, 1.0001, NaN,
                                Infinity, -Infinity]) {
            page.setExactDecimalDraftValue(page.shadowScaleId, rejected);
        }
        compare(page.draftValue(page.shadowScaleId), Number.MIN_VALUE);

        field.forceActiveFocus();
        field.text = "2e-1";
        field.textEdited();
        wait(0);
        compare(hydrationEditCount, 1);
        compare(page.draftValue(page.shadowScaleId), "2e-1");
        compare(page.draftValuesValid, false);
        compare(page.draftValid, false);
        compare(page.draftDirty, true);
        compare(save.enabled, false);
        compare(field.inputValid, false);
        compare(validation.visible, true);
        compare(validation.Accessible.role, Accessible.AlertMessage);
        compare(aggregateValidation.visible, true);
        verify(Number.isNaN(preview.shadowScale));
        verify(String(summary.text).includes("Shadow scale invalid draft."));
        compare([
            activeWindow.x, activeWindow.y, activeWindow.width,
            activeWindow.height, activeWindow.radius, activeWindow.opacity,
            String(activeWindow.color), secondaryShadow.x,
            secondaryShadow.y, secondaryShadow.width,
            secondaryShadow.height, secondaryShadow.radius,
            secondaryShadow.opacity, String(secondaryShadow.color),
            secondaryShadow.visible
        ], previewState);

        field.text = "0.731234567890123";
        field.textEdited();
        refresh.forceActiveFocus();
        wait(0);
        compare(page.draftValue(page.shadowScaleId), 0.731234567890123);
        compare(page.draftValuesValid, true);
        compare(page.draftValid, true);
        compare(validation.visible, false);
        compare(aggregateValidation.visible, false);
        compare(save.enabled, true);

        discard.clicked();
        wait(0);
        compare(page.draftValues, baseline);
        compare(page.draftValue(page.shadowScaleId), Number.MIN_VALUE);
        compare(page.draftDirty, false);

        page.setExactDecimalDraftValue(page.shadowScaleId, -0);
        compare(page.draftValue(page.shadowScaleId), 0);
        verify(!Object.is(page.draftValue(page.shadowScaleId), -0));
        page.setDraftValue(page.shadowSharpId, true);
        wait(0);
        compare(field.enabled, true);
        reset.clicked();
        wait(0);
        compare(page.draftValue(page.shadowScaleId), 1);
        compare(field.text, "1");

        page.setExactDecimalDraftValue(page.shadowScaleId, 0.375);
        const newer = appearanceDefaults();
        newer[page.shadowScaleId] = 0.625;
        page.appearanceValues = newer;
        page.revisionToken = "8";
        wait(0);
        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.shadowScaleId), 0.375);
        compare(loadCurrent.enabled, true);
        loadCurrent.clicked();
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftValues, newer);
        compare(page.draftValue(page.shadowScaleId), 0.625);
        compare(page.draftDirty, false);
        compare(validation.visible, false);
    }

    function test_appearanceShadowOffsetExactDraftValidationAndRecovery() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const xRow = findChild(page, "appearanceShadowOffsetXRow");
        const yRow = findChild(page, "appearanceShadowOffsetYRow");
        verify(xRow !== null);
        verify(yRow !== null);
        let hydrationEditCount = 0;
        xRow.valueModified.connect(function() {
            ++hydrationEditCount;
        });
        yRow.valueModified.connect(function() {
            ++hydrationEditCount;
        });

        const baseline = appearanceDefaults();
        baseline[page.shadowOffsetId] = [0.0000001, -250];
        configureAppearancePage(page, baseline);
        page.reviewProjection();
        waitForRendering(page);
        wait(0);
        compare(hydrationEditCount, 0);

        const offsetX = findChild(page, "appearanceShadowOffsetX");
        const offsetY = findChild(page, "appearanceShadowOffsetY");
        const xValidation = findChild(
            page, "appearanceShadowOffsetXValidation"
        );
        const yValidation = findChild(
            page, "appearanceShadowOffsetYValidation"
        );
        const aggregateValidation = findChild(
            page, "appearanceDraftValidationMessage"
        );
        const save = findChild(page, "saveAppearanceButton");
        const discard = findChild(
            page, "discardAppearanceDraftButton"
        );
        const reset = findChild(page, "resetAppearanceDefaultsButton");
        const loadCurrent = findChild(
            page, "loadCurrentAppearanceButton"
        );
        const refresh = findChild(page, "refreshAppearanceButton");
        const preview = findChild(page, "appearancePreview");
        const activeWindow = findChild(
            page, "appearancePreviewActiveWindow"
        );
        const secondaryShadow = findChild(
            page, "appearancePreviewSecondaryShadow"
        );
        const summary = findChild(page, "appearancePreviewSummary");
        for (const item of [offsetX, offsetY, xValidation, yValidation,
                            aggregateValidation, save, discard, reset,
                            loadCurrent, refresh, preview, activeWindow,
                            secondaryShadow, summary]) {
            verify(item !== null);
        }

        compare(offsetX.maximumLength, 326);
        compare(offsetY.maximumLength, 326);
        compare(offsetX.text, "0.0000001");
        compare(offsetY.text, "-250");
        compare(offsetX.inputValid, true);
        compare(offsetY.inputValid, true);
        compare(page.valuesEqual(page.draftValues, baseline), true);
        compare(page.draftDirty, false);
        compare(preview.shadowOffsetX, 0.0000001);
        compare(preview.shadowOffsetY, -250);

        preview.motionPaused = true;
        preview.motionProgress = 1;
        wait(0);
        const previewState = [
            activeWindow.x, activeWindow.y, activeWindow.width,
            activeWindow.height, activeWindow.radius, activeWindow.opacity,
            String(activeWindow.color), secondaryShadow.x,
            secondaryShadow.y, secondaryShadow.width,
            secondaryShadow.height, secondaryShadow.radius,
            secondaryShadow.opacity, String(secondaryShadow.color),
            secondaryShadow.visible, preview.motionPaused,
            preview.motionProgress, preview.motionPhase, preview.motionStory
        ];

        for (const rejected of [-250.0001, 250.0001, NaN,
                                Infinity, -Infinity]) {
            page.setExactVectorComponentDraftValue(
                page.shadowOffsetId, 0, rejected
            );
        }
        compare(page.draftValue(page.shadowOffsetId), [0.0000001, -250]);

        offsetX.forceActiveFocus();
        offsetX.text = "123.456789";
        offsetX.textEdited();
        wait(0);
        compare(hydrationEditCount, 1);
        compare(page.draftValue(page.shadowOffsetId), [123.456789, -250]);
        compare(page.appearanceValues[page.shadowOffsetId], [0.0000001, -250]);
        compare(page.draftValuesValid, true);
        compare(preview.shadowOffsetX, 123.456789);

        offsetY.forceActiveFocus();
        offsetY.text = "2e2";
        offsetY.textEdited();
        wait(0);
        compare(hydrationEditCount, 2);
        compare(page.draftValue(page.shadowOffsetId), [123.456789, "2e2"]);
        compare(page.draftValuesValid, false);
        compare(page.draftValid, false);
        compare(page.draftDirty, true);
        compare(save.enabled, false);
        compare(offsetY.inputValid, false);
        compare(yValidation.visible, true);
        compare(yValidation.Accessible.role, Accessible.AlertMessage);
        compare(yValidation.Accessible.name, yValidation.text);
        compare(aggregateValidation.visible, true);
        compare(aggregateValidation.Accessible.role,
            Accessible.AlertMessage);
        verify(Number.isNaN(preview.shadowOffsetY));
        verify(String(summary.text).includes(
            "Horizontal shadow offset 123.456789. Vertical shadow offset invalid draft."
        ));
        compare([
            activeWindow.x, activeWindow.y, activeWindow.width,
            activeWindow.height, activeWindow.radius, activeWindow.opacity,
            String(activeWindow.color), secondaryShadow.x,
            secondaryShadow.y, secondaryShadow.width,
            secondaryShadow.height, secondaryShadow.radius,
            secondaryShadow.opacity, String(secondaryShadow.color),
            secondaryShadow.visible, preview.motionPaused,
            preview.motionProgress, preview.motionPhase, preview.motionStory
        ], previewState);

        offsetY.text = "-249.999999";
        offsetY.textEdited();
        refresh.forceActiveFocus();
        wait(0);
        compare(page.draftValue(page.shadowOffsetId), [
            123.456789, -249.999999
        ]);
        compare(page.draftValuesValid, true);
        compare(page.draftValid, true);
        compare(offsetY.inputValid, true);
        compare(yValidation.visible, false);
        compare(aggregateValidation.visible, false);
        compare(save.enabled, true);

        discard.clicked();
        wait(0);
        compare(page.draftValues, baseline);
        compare(page.draftValue(page.shadowOffsetId), [0.0000001, -250]);
        compare(page.draftValuesValid, true);
        compare(page.draftDirty, false);

        page.setExactVectorComponentDraftValue(
            page.shadowOffsetId, 0, -0
        );
        compare(page.draftValue(page.shadowOffsetId), [0, -250]);
        verify(!Object.is(page.draftValue(page.shadowOffsetId)[0], -0));
        reset.clicked();
        wait(0);
        compare(page.draftValue(page.shadowOffsetId), [0, 0]);
        compare(offsetX.text, "0");
        compare(offsetY.text, "0");

        page.setExactVectorComponentDraftValue(
            page.shadowOffsetId, 0, 77.125
        );
        const newer = appearanceDefaults();
        newer[page.shadowOffsetId] = [-10.5, 20.25];
        page.appearanceValues = newer;
        page.revisionToken = "8";
        wait(0);
        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.shadowOffsetId), [77.125, 0]);
        compare(loadCurrent.enabled, true);
        loadCurrent.clicked();
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftValues, newer);
        compare(page.draftValue(page.shadowOffsetId), [-10.5, 20.25]);
        compare(page.draftDirty, false);
        compare(xValidation.visible, false);
        compare(yValidation.visible, false);
    }

    function test_appearanceInactiveDimmingRetainsDormantStrengthAndDarkensPreview() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const dimInactiveRow = findChild(
            page, "appearanceDimInactiveRow"
        );
        const dimStrengthRow = findChild(
            page, "appearanceDimStrengthRow"
        );
        const dimInactive = findChild(page, "appearanceDimInactive");
        const dimStrength = findChild(page, "appearanceDimStrength");
        const dimStrengthValue = findChild(
            page, "appearanceDimStrengthValue"
        );
        const preview = findChild(page, "appearancePreview");
        const activeWindow = findChild(
            page, "appearancePreviewActiveWindow"
        );
        const spawnedWindow = findChild(
            page, "appearancePreviewSpawnedWindow"
        );
        const overlay = findChild(
            page, "appearancePreviewInactiveDimOverlay"
        );
        const summary = findChild(page, "appearancePreviewSummary");
        verify(dimInactiveRow !== null);
        verify(dimStrengthRow !== null);
        verify(dimInactive !== null);
        verify(dimStrength !== null);
        verify(dimStrengthValue !== null);
        verify(preview !== null);
        verify(activeWindow !== null);
        verify(spawnedWindow !== null);
        verify(overlay !== null);
        verify(summary !== null);

        compare(dimInactiveRow.title, "Dim inactive windows");
        compare(
            dimInactiveRow.description,
            "Darken windows that do not have focus. A matching Window Rule can keep a window undimmed."
        );
        compare(
            dimStrengthRow.title,
            "Inactive-window dimming strength"
        );
        compare(
            dimStrengthRow.description,
            "Choose how strongly inactive windows are darkened. The saved strength is retained while inactive-window dimming is off."
        );
        compare(dimInactive.Accessible.name, "Dim inactive windows");
        compare(
            dimStrength.Accessible.name,
            "Inactive-window dimming strength"
        );
        compare(dimStrength.stepSize, 0.05);
        compare(dimStrengthValue.text, "0.50");
        compare(page.draftValue(page.dimInactiveId), false);
        compare(page.draftValue(page.dimStrengthId), 0.5);
        compare(dimStrength.enabled, false);

        preview.motionPaused = true;
        preview.motionProgress = 1;
        wait(0);
        compare(activeWindow.opacity, 1);
        compare(spawnedWindow.opacity, 1);
        compare(overlay.opacity, 0);
        compare(overlay.Accessible.ignored, true);
        verify(String(summary.text).startsWith("Illustrative preview"));
        verify(String(summary.text).includes(
            "Inactive-window dimming off"
        ));
        verify(String(summary.text).includes("Dimming strength 0.50"));

        dimInactive.checked = true;
        dimInactive.clicked();
        wait(0);
        compare(page.draftValue(page.dimInactiveId), true);
        compare(dimStrength.enabled, true);
        compare(preview.dimInactive, true);
        compare(overlay.opacity, 0.5);
        compare(spawnedWindow.opacity, 1);

        page.setDimStrength(0);
        wait(0);
        compare(page.draftValue(page.dimStrengthId), 0);
        compare(overlay.opacity, 0);
        compare(activeWindow.opacity, 1);
        compare(spawnedWindow.opacity, 1);
        verify(String(summary.text).includes("Dimming strength 0.00"));

        page.setDimStrength(1);
        wait(0);
        compare(page.draftValue(page.dimStrengthId), 1);
        compare(overlay.opacity, 1);
        compare(activeWindow.opacity, 1);
        compare(spawnedWindow.opacity, 1);
        verify(String(summary.text).includes("Dimming strength 1.00"));

        page.setDimStrength(0.37);
        wait(0);
        compare(page.draftValue(page.dimStrengthId), 0.35);
        compare(dimStrength.value, 0.35);
        compare(dimStrengthValue.text, "0.35");
        compare(preview.dimStrength, 0.35);
        compare(overlay.opacity, 0.35);
        compare(activeWindow.opacity, 1);
        compare(spawnedWindow.opacity, 1);
        verify(String(summary.text).includes(
            "Inactive-window dimming on"
        ));
        verify(String(summary.text).includes("Dimming strength 0.35"));

        dimInactive.checked = false;
        dimInactive.clicked();
        wait(0);
        compare(page.draftValue(page.dimInactiveId), false);
        compare(page.draftValue(page.dimStrengthId), 0.35);
        compare(dimStrength.enabled, false);
        compare(dimStrength.value, 0.35);
        compare(preview.dimStrength, 0.35);
        compare(overlay.opacity, 0);
        verify(String(summary.text).includes(
            "Inactive-window dimming off"
        ));
        verify(String(summary.text).includes("Dimming strength 0.35"));
    }

    function test_appearanceOpacityAndContextualDimmingAreExactAndHonest() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const activeRow = findChild(page, "appearanceActiveOpacityRow");
        const opacityHeading = findChild(
            page, "appearanceWindowOpacityHeading"
        );
        const inactiveRow = findChild(
            page, "appearanceInactiveOpacityRow"
        );
        const fullscreenRow = findChild(
            page, "appearanceFullscreenOpacityRow"
        );
        const modalRow = findChild(page, "appearanceDimModalRow");
        const contextualHeading = findChild(
            page, "appearanceContextualDimmingHeading"
        );
        const specialRow = findChild(page, "appearanceDimSpecialRow");
        const aroundRow = findChild(page, "appearanceDimAroundRow");
        const activeOpacity = findChild(page, "appearanceActiveOpacity");
        const inactiveOpacity = findChild(
            page, "appearanceInactiveOpacity"
        );
        const fullscreenOpacity = findChild(
            page, "appearanceFullscreenOpacity"
        );
        const dimModal = findChild(page, "appearanceDimModal");
        const dimSpecial = findChild(page, "appearanceDimSpecial");
        const dimAround = findChild(page, "appearanceDimAround");
        const preview = findChild(page, "appearancePreview");
        const activeWindow = findChild(
            page, "appearancePreviewActiveWindow"
        );
        const spawnedWindow = findChild(
            page, "appearancePreviewSpawnedWindow"
        );
        const secondaryWindow = findChild(
            page, "appearancePreviewSecondaryWindow"
        );
        const secondaryShadow = findChild(
            page, "appearancePreviewSecondaryShadow"
        );
        const overlay = findChild(
            page, "appearancePreviewInactiveDimOverlay"
        );
        const summary = findChild(page, "appearancePreviewSummary");
        const disclaimer = findChild(
            page, "appearanceAnimationPreviewDisclaimer"
        );
        for (const item of [opacityHeading, activeRow, inactiveRow,
                            fullscreenRow, contextualHeading, modalRow,
                            specialRow, aroundRow,
                            activeOpacity, inactiveOpacity,
                            fullscreenOpacity, dimModal, dimSpecial,
                            dimAround, preview, activeWindow,
                            spawnedWindow, secondaryWindow,
                            secondaryShadow, overlay, summary,
                            disclaimer]) {
            verify(item !== null);
        }

        compare(opacityHeading.text, "Window opacity");
        compare(opacityHeading.textFormat, Text.PlainText);
        compare(opacityHeading.Accessible.name, opacityHeading.text);
        compare(contextualHeading.text, "Contextual dimming");
        compare(contextualHeading.textFormat, Text.PlainText);
        compare(
            contextualHeading.Accessible.name,
            contextualHeading.text
        );
        compare(activeRow.title, "Active-window opacity");
        verify(activeRow.description.includes(
            "change the resulting per-window opacity"
        ));
        compare(inactiveRow.title, "Inactive-window opacity");
        verify(inactiveRow.description.includes(
            "inactive-window dimming remains separate"
        ));
        compare(fullscreenRow.title, "Fullscreen-window opacity");
        verify(fullscreenRow.description.includes("true fullscreen"));
        verify(fullscreenRow.description.includes(
            "Maximized windows use the focused or unfocused value"
        ));
        compare(modalRow.title, "Dim parents of modal dialogs");
        verify(modalRow.description.includes(
            "can combine with inactive-window dimming"
        ));
        compare(specialRow.title, "Special-workspace dimming");
        verify(specialRow.description.includes("Hyprland 0.56.1"));
        verify(specialRow.description.includes("closed and reopened"));
        compare(aroundRow.title, "Dim-around strength");
        verify(aroundRow.description.includes("Window or Layer Rule"));
        verify(aroundRow.description.includes("fade-out"));
        verify(String(disclaimer.text).includes(
            "true fullscreen, modal-dialog parent, special-workspace dimming, Dim around Rule"
        ));
        compare(disclaimer.textFormat, Text.PlainText);

        for (const slider of [activeOpacity, inactiveOpacity,
                             fullscreenOpacity, dimSpecial, dimAround]) {
            compare(slider.from, 0);
            compare(slider.to, 1);
            compare(slider.stepSize, 0.05);
            compare(slider.enabled, true);
        }
        compare(activeOpacity.Accessible.name, "Active-window opacity");
        compare(
            inactiveOpacity.Accessible.name,
            "Inactive-window opacity"
        );
        compare(
            fullscreenOpacity.Accessible.name,
            "Fullscreen-window opacity"
        );
        compare(
            dimModal.Accessible.name,
            "Dim parents of modal dialogs"
        );
        compare(dimSpecial.Accessible.name, "Special-workspace dimming");
        compare(dimAround.Accessible.name, "Dim-around strength");
        compare(dimModal.checked, true);
        compare(findChild(page, "appearanceActiveOpacityValue").text, "1.00");
        compare(
            findChild(page, "appearanceInactiveOpacityValue").text,
            "1.00"
        );
        compare(
            findChild(page, "appearanceFullscreenOpacityValue").text,
            "1.00"
        );
        compare(findChild(page, "appearanceDimSpecialValue").text, "0.20");
        compare(findChild(page, "appearanceDimAroundValue").text, "0.40");

        const offGridValues = appearanceDefaults();
        offGridValues[page.activeOpacityId] = 0.83;
        offGridValues[page.inactiveOpacityId] = 0.47;
        offGridValues[page.fullscreenOpacityId] = 0.62;
        offGridValues[page.dimSpecialId] = 0.23;
        offGridValues[page.dimAroundId] = 0.41;
        page.appearanceValues = offGridValues;
        page.revisionToken = "8";
        page.appliedRevision = 8;
        page.reviewProjection();
        wait(0);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.activeOpacityId), 0.83);
        compare(page.draftValue(page.inactiveOpacityId), 0.47);
        compare(page.draftValue(page.fullscreenOpacityId), 0.62);
        compare(page.draftValue(page.dimSpecialId), 0.23);
        compare(page.draftValue(page.dimAroundId), 0.41);
        compare(activeOpacity.value, 0.83);
        compare(inactiveOpacity.value, 0.47);
        compare(fullscreenOpacity.value, 0.62);
        compare(dimSpecial.value, 0.23);
        compare(dimAround.value, 0.41);
        compare(findChild(page, "appearanceActiveOpacityValue").text, "0.83");
        compare(
            findChild(page, "appearanceInactiveOpacityValue").text,
            "0.47"
        );
        compare(
            findChild(page, "appearanceFullscreenOpacityValue").text,
            "0.62"
        );
        compare(findChild(page, "appearanceDimSpecialValue").text, "0.23");
        compare(findChild(page, "appearanceDimAroundValue").text, "0.41");

        page.setUnitSliderValue(page.activeOpacityId, 0.82);
        page.setUnitSliderValue(page.inactiveOpacityId, 0.37);
        page.setUnitSliderValue(page.fullscreenOpacityId, 0.44);
        page.setDraftValue(page.dimModalId, false);
        page.setUnitSliderValue(page.dimSpecialId, 0.63);
        page.setUnitSliderValue(page.dimAroundId, 0.76);
        wait(0);
        compare(page.draftValue(page.activeOpacityId), 0.8);
        compare(page.draftValue(page.inactiveOpacityId), 0.35);
        compare(page.draftValue(page.fullscreenOpacityId), 0.45);
        compare(page.draftValue(page.dimModalId), false);
        compare(page.draftValue(page.dimSpecialId), 0.65);
        compare(page.draftValue(page.dimAroundId), 0.75);

        preview.motionPaused = true;
        preview.motionProgress = 0;
        wait(0);
        compare(activeWindow.opacity, 0.8);
        compare(spawnedWindow.opacity, 0);
        compare(overlay.opacity, 0);

        preview.motionProgress = 1;
        wait(0);
        compare(activeWindow.opacity, 0.35);
        compare(spawnedWindow.opacity, 0.8);
        compare(overlay.opacity, 0);
        verify(String(summary.text).includes("Active-window opacity 0.80"));
        verify(String(summary.text).includes(
            "Inactive-window opacity 0.35"
        ));
        verify(String(summary.text).includes(
            "True-fullscreen opacity 0.45"
        ));
        verify(String(summary.text).includes("Modal-parent dimming off"));
        verify(String(summary.text).includes(
            "Special-workspace dimming 0.65"
        ));
        verify(String(summary.text).includes("Dim-around strength 0.75"));

        // Contexts that this representative preview does not contain remain
        // summary-only and cannot change its Dwindle opacity or dim overlay.
        page.setUnitSliderValue(page.fullscreenOpacityId, 0);
        page.setDraftValue(page.dimModalId, true);
        page.setUnitSliderValue(page.dimSpecialId, 0);
        page.setUnitSliderValue(page.dimAroundId, 1);
        wait(0);
        compare(activeWindow.opacity, 0.35);
        compare(spawnedWindow.opacity, 0.8);
        compare(overlay.opacity, 0);
        verify(String(summary.text).includes(
            "True-fullscreen opacity 0.00"
        ));
        verify(String(summary.text).includes("Modal-parent dimming on"));
        verify(String(summary.text).includes(
            "Special-workspace dimming 0.00"
        ));
        verify(String(summary.text).includes("Dim-around strength 1.00"));

        page.setUnitSliderValue(page.activeOpacityId, 0);
        page.setUnitSliderValue(page.inactiveOpacityId, 1);
        preview.motionProgress = 0;
        wait(0);
        compare(activeWindow.opacity, 0);
        compare(spawnedWindow.opacity, 0);
        preview.motionProgress = 1;
        wait(0);
        compare(activeWindow.opacity, 1);
        compare(spawnedWindow.opacity, 0);

        page.setUnitSliderValue(page.activeOpacityId, 0.8);
        page.setUnitSliderValue(page.inactiveOpacityId, 0.35);
        preview.layoutMode = "master";
        preview.motionProgress = 1;
        wait(0);
        compare(secondaryWindow.opacity, 0.35);
        compare(secondaryShadow.opacity, 0.35);
    }

    function test_appearanceBlurDetailsAreExactRetainedAndPreviewHonest() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const renderingHeading = findChild(
            page, "appearanceBlurRenderingHeading"
        );
        const contextsHeading = findChild(
            page, "appearanceBlurContextsHeading"
        );
        const sizeRow = findChild(page, "appearanceBlurSizeRow");
        const passesRow = findChild(page, "appearanceBlurPassesRow");
        const ignoreRow = findChild(
            page, "appearanceBlurIgnoreOpacityRow"
        );
        const optimizationsRow = findChild(
            page, "appearanceBlurOptimizationsRow"
        );
        const xrayRow = findChild(page, "appearanceBlurXrayRow");
        const specialRow = findChild(page, "appearanceBlurSpecialRow");
        const popupsRow = findChild(page, "appearanceBlurPopupsRow");
        const popupsAlphaRow = findChild(
            page, "appearanceBlurPopupsIgnoreAlphaRow"
        );
        const inputMethodsRow = findChild(
            page, "appearanceBlurInputMethodsRow"
        );
        const inputMethodsAlphaRow = findChild(
            page, "appearanceBlurInputMethodsIgnoreAlphaRow"
        );
        const size = findChild(page, "appearanceBlurSize");
        const passes = findChild(page, "appearanceBlurPasses");
        const ignoreOpacity = findChild(
            page, "appearanceBlurIgnoreOpacity"
        );
        const optimizations = findChild(
            page, "appearanceBlurOptimizations"
        );
        const xray = findChild(page, "appearanceBlurXray");
        const special = findChild(page, "appearanceBlurSpecial");
        const popups = findChild(page, "appearanceBlurPopups");
        const popupsAlpha = findChild(
            page, "appearanceBlurPopupsIgnoreAlpha"
        );
        const inputMethods = findChild(
            page, "appearanceBlurInputMethods"
        );
        const inputMethodsAlpha = findChild(
            page, "appearanceBlurInputMethodsIgnoreAlpha"
        );
        const blur = findChild(page, "appearanceBlurEnabled");
        const preview = findChild(page, "appearancePreview");
        const activeWindow = findChild(
            page, "appearancePreviewActiveWindow"
        );
        const spawnedWindow = findChild(
            page, "appearancePreviewSpawnedWindow"
        );
        const summary = findChild(page, "appearancePreviewSummary");
        const disclaimer = findChild(
            page, "appearanceAnimationPreviewDisclaimer"
        );
        for (const item of [renderingHeading, contextsHeading, sizeRow,
                            passesRow, ignoreRow, optimizationsRow,
                            xrayRow, specialRow, popupsRow,
                            popupsAlphaRow, inputMethodsRow,
                            inputMethodsAlphaRow, size, passes,
                            ignoreOpacity, optimizations, xray, special,
                            popups, popupsAlpha, inputMethods,
                            inputMethodsAlpha, blur, preview,
                            activeWindow, spawnedWindow, summary,
                            disclaimer]) {
            verify(item !== null);
        }

        compare(renderingHeading.text, "Blur rendering");
        compare(renderingHeading.textFormat, Text.PlainText);
        compare(renderingHeading.Accessible.name, renderingHeading.text);
        compare(contextsHeading.text, "Blur contexts");
        compare(contextsHeading.textFormat, Text.PlainText);
        compare(contextsHeading.Accessible.name, contextsHeading.text);
        verify(sizeRow.description.includes("0 to 100"));
        verify(sizeRow.description.includes("stores and uses this full range"));
        verify(sizeRow.description.includes(
            "increase the blur distance and GPU work"
        ));
        verify(passesRow.description.includes("0 to 10"));
        verify(passesRow.description.includes("1–8"));
        verify(passesRow.description.includes(
            "within that effective range"
        ));
        verify(ignoreRow.description.includes("ignore window opacity"));
        verify(optimizationsRow.description.includes(
            "preserves the X-ray setting"
        ));
        verify(xrayRow.description.includes(
            "requires the optimized blur path"
        ));
        verify(xrayRow.description.includes("Window Rules"));
        verify(xrayRow.description.includes("Layer Rules"));
        verify(specialRow.description.includes("expensive"));
        verify(specialRow.description.includes("Hyprland 0.56.1"));
        verify(specialRow.description.includes("closed and reopened"));
        verify(popupsRow.description.includes("right-click menus"));
        verify(popupsAlphaRow.description.includes(
            "opacity is below this saved value"
        ));
        verify(popupsAlphaRow.description.includes(
            "Live mapped popups use the saved 0.00–1.00 value directly"
        ));
        verify(popupsAlphaRow.description.includes(
            "Only popup snapshot or fadeout capture applies a 0.01 minimum"
        ));
        verify(popupsAlphaRow.description.includes(
            "on that capture path, a layer owner's Rule ignore-alpha value can replace this global threshold"
        ));
        verify(inputMethodsRow.description.includes("fcitx5"));
        verify(inputMethodsAlphaRow.description.includes(
            "opacity is below this saved value"
        ));
        verify(inputMethodsAlphaRow.description.includes(
            "Live input-method rendering uses the saved 0.00–1.00 value directly"
        ));
        verify(inputMethodsAlphaRow.description.includes(
            "does not apply a 0.01 minimum"
        ));
        verify(String(disclaimer.text).includes(
            "Blur is illustrated only as on or off"
        ));
        verify(String(disclaimer.text).includes(
            "Kawase renderer tuning, brightness, contrast, noise, vibrancy, dark-area vibrancy, opacity handling, optimized and X-ray rendering"
        ));
        verify(String(disclaimer.text).includes(
            "special-workspace blur, popup blur, and input-method blur are not simulated"
        ));
        compare(disclaimer.textFormat, Text.PlainText);

        compare(size.Accessible.name, "Blur size");
        compare(passes.Accessible.name, "Blur passes");
        compare(ignoreOpacity.Accessible.name, "Ignore window opacity");
        compare(optimizations.Accessible.name, "Optimized blur path");
        compare(xray.Accessible.name, "X-ray blur");
        compare(special.Accessible.name, "Special-workspace blur");
        compare(popups.Accessible.name, "Popup blur");
        compare(
            popupsAlpha.Accessible.name,
            "Popup ignore-alpha threshold"
        );
        compare(inputMethods.Accessible.name, "Input-method blur");
        compare(
            inputMethodsAlpha.Accessible.name,
            "Input-method ignore-alpha threshold"
        );
        compare(popupsAlpha.stepSize, 0.05);
        compare(inputMethodsAlpha.stepSize, 0.05);
        compare(findChild(
            page, "appearanceBlurPopupsIgnoreAlphaValue"
        ).text, "0.20");
        compare(findChild(
            page, "appearanceBlurInputMethodsIgnoreAlphaValue"
        ).text, "0.20");

        preview.motionPaused = true;
        preview.motionProgress = 1;
        wait(0);
        const activeColor = String(activeWindow.color);
        const spawnedColor = String(spawnedWindow.color);
        const activeWidth = activeWindow.width;
        const spawnedWidth = spawnedWindow.width;
        const activeOpacity = activeWindow.opacity;
        const spawnedOpacity = spawnedWindow.opacity;

        page.setDraftValue(page.blurSizeId, 100);
        page.setDraftValue(page.blurPassesId, 10);
        page.setDraftValue(page.blurIgnoreOpacityId, false);
        page.setDraftValue(page.blurXrayId, true);
        page.setDraftValue(page.blurSpecialId, true);
        page.setDraftValue(page.blurPopupsId, true);
        page.setUnitSliderValue(page.blurPopupsIgnoreAlphaId, 0.37);
        page.setDraftValue(page.blurInputMethodsId, true);
        page.setUnitSliderValue(
            page.blurInputMethodsIgnoreAlphaId, 0.83
        );
        wait(0);

        compare(page.draftValue(page.blurSizeId), 100);
        compare(page.draftValue(page.blurPassesId), 10);
        compare(page.draftValue(page.blurIgnoreOpacityId), false);
        compare(page.draftValue(page.blurXrayId), true);
        compare(page.draftValue(page.blurSpecialId), true);
        compare(page.draftValue(page.blurPopupsId), true);
        compare(page.draftValue(page.blurPopupsIgnoreAlphaId), 0.35);
        compare(page.draftValue(page.blurInputMethodsId), true);
        compare(
            page.draftValue(page.blurInputMethodsIgnoreAlphaId), 0.85
        );
        compare(popupsAlpha.enabled, true);
        compare(inputMethodsAlpha.enabled, true);
        compare(findChild(
            page, "appearanceBlurPopupsIgnoreAlphaValue"
        ).text, "0.35");
        compare(findChild(
            page, "appearanceBlurInputMethodsIgnoreAlphaValue"
        ).text, "0.85");
        compare(String(activeWindow.color), activeColor);
        compare(String(spawnedWindow.color), spawnedColor);
        compare(activeWindow.width, activeWidth);
        compare(spawnedWindow.width, spawnedWidth);
        compare(activeWindow.opacity, activeOpacity);
        compare(spawnedWindow.opacity, spawnedOpacity);
        verify(String(summary.text).includes("Blur size 100"));
        verify(String(summary.text).includes("Blur passes 10"));
        verify(String(summary.text).includes("Blur ignores opacity off"));
        verify(String(summary.text).includes("Optimized blur path on"));
        verify(String(summary.text).includes("X-ray blur on"));
        verify(String(summary.text).includes("Special-workspace blur on"));
        verify(String(summary.text).includes("Popup blur on"));
        verify(String(summary.text).includes(
            "Popup ignore-alpha threshold 0.35"
        ));
        verify(String(summary.text).includes("Input-method blur on"));
        verify(String(summary.text).includes(
            "Input-method ignore-alpha threshold 0.85"
        ));

        page.setDraftValue(page.blurPopupsId, false);
        page.setDraftValue(page.blurInputMethodsId, false);
        wait(0);
        compare(popupsAlpha.enabled, false);
        compare(inputMethodsAlpha.enabled, false);
        compare(page.draftValue(page.blurPopupsIgnoreAlphaId), 0.35);
        compare(
            page.draftValue(page.blurInputMethodsIgnoreAlphaId), 0.85
        );

        page.setDraftValue(page.blurOptimizationsId, false);
        wait(0);
        compare(xray.enabled, false);
        compare(page.draftValue(page.blurXrayId), true);
        verify(String(summary.text).includes("Optimized blur path off"));
        verify(String(summary.text).includes("X-ray blur on"));

        page.setDraftValue(page.blurId, false);
        wait(0);
        compare(blur.checked, false);
        for (const control of [size, passes, ignoreOpacity,
                               optimizations, xray, special, popups,
                               popupsAlpha, inputMethods,
                               inputMethodsAlpha]) {
            compare(control.enabled, false);
        }
        compare(page.draftValue(page.blurSizeId), 100);
        compare(page.draftValue(page.blurPassesId), 10);
        compare(page.draftValue(page.blurIgnoreOpacityId), false);
        compare(page.draftValue(page.blurOptimizationsId), false);
        compare(page.draftValue(page.blurXrayId), true);
        compare(page.draftValue(page.blurSpecialId), true);
        compare(page.draftValue(page.blurPopupsId), false);
        compare(page.draftValue(page.blurPopupsIgnoreAlphaId), 0.35);
        compare(page.draftValue(page.blurInputMethodsId), false);
        compare(
            page.draftValue(page.blurInputMethodsIgnoreAlphaId), 0.85
        );
        verify(String(summary.text).includes("Blur off"));
        verify(String(activeWindow.color) !== activeColor);

        page.setDraftValue(page.blurId, true);
        page.setDraftValue(page.blurOptimizationsId, true);
        page.setDraftValue(page.blurPopupsId, true);
        page.setDraftValue(page.blurInputMethodsId, true);
        wait(0);
        compare(size.enabled, true);
        compare(passes.enabled, true);
        compare(ignoreOpacity.enabled, true);
        compare(optimizations.enabled, true);
        compare(xray.enabled, true);
        compare(special.enabled, true);
        compare(popups.enabled, true);
        compare(popupsAlpha.enabled, true);
        compare(inputMethods.enabled, true);
        compare(inputMethodsAlpha.enabled, true);
        compare(page.draftValue(page.blurXrayId), true);
        compare(page.draftValue(page.blurPopupsIgnoreAlphaId), 0.35);
        compare(
            page.draftValue(page.blurInputMethodsIgnoreAlphaId), 0.85
        );
        compare(String(activeWindow.color), activeColor);
        compare(String(spawnedWindow.color), spawnedColor);
        page.setUnitSliderValue(page.blurPopupsIgnoreAlphaId, 0);
        page.setUnitSliderValue(
            page.blurInputMethodsIgnoreAlphaId, 0
        );
        wait(0);
        compare(page.draftValue(page.blurPopupsIgnoreAlphaId), 0);
        compare(
            page.draftValue(page.blurInputMethodsIgnoreAlphaId), 0
        );
        compare(findChild(
            page, "appearanceBlurPopupsIgnoreAlphaValue"
        ).text, "0.00");
        compare(findChild(
            page, "appearanceBlurInputMethodsIgnoreAlphaValue"
        ).text, "0.00");
        verify(String(summary.text).includes(
            "Popup ignore-alpha threshold 0.00"
        ));
        verify(String(summary.text).includes(
            "Input-method ignore-alpha threshold 0.00"
        ));
        compare(String(activeWindow.color), activeColor);
        compare(String(spawnedWindow.color), spawnedColor);
    }

    function test_appearanceBlurModulationUsesExactRecoverableDecimalDrafts() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const hydrationRow = findChild(
            page, "appearanceBlurBrightnessRow"
        );
        const hydrationNoiseRow = findChild(
            page, "appearanceBlurNoiseRow"
        );
        verify(hydrationRow !== null);
        verify(hydrationNoiseRow !== null);
        let hydrationEditCount = 0;
        let hydrationNoiseEditCount = 0;
        hydrationRow.valueModified.connect(function() {
            ++hydrationEditCount;
        });
        hydrationNoiseRow.valueModified.connect(function() {
            ++hydrationNoiseEditCount;
        });
        const minimumPlainDecimal = "0." + "0".repeat(323) + "5";
        const authority = appearanceDefaults();
        authority[page.blurBrightnessId] = 1e-7;
        authority[page.blurNoiseId] = Number.MIN_VALUE;
        authority[page.blurVibrancyId] = 0.3333333333333333;
        authority[page.blurVibrancyDarknessId] = 0.875;
        configureAppearancePage(page, authority);
        waitForRendering(page);
        wait(0);
        compare(hydrationEditCount, 0);
        compare(hydrationNoiseEditCount, 0);

        const renderingCard = findChild(
            page, "appearanceBlurRenderingCard"
        );
        const modulationCard = findChild(
            page, "appearanceBlurModulationCard"
        );
        const contextsCard = findChild(page, "appearanceBlurContextsCard");
        const heading = findChild(page, "appearanceBlurModulationHeading");
        const rows = [
            findChild(page, "appearanceBlurBrightnessRow"),
            findChild(page, "appearanceBlurContrastRow"),
            findChild(page, "appearanceBlurNoiseRow"),
            findChild(page, "appearanceBlurVibrancyRow"),
            findChild(page, "appearanceBlurVibrancyDarknessRow")
        ];
        const fields = [
            findChild(page, "appearanceBlurBrightness"),
            findChild(page, "appearanceBlurContrast"),
            findChild(page, "appearanceBlurNoise"),
            findChild(page, "appearanceBlurVibrancy"),
            findChild(page, "appearanceBlurVibrancyDarkness")
        ];
        const validation = findChild(
            page, "appearanceBlurBrightnessValidation"
        );
        const aggregateValidation = findChild(
            page, "appearanceDraftValidationMessage"
        );
        const animationValidation = findChild(
            page, "appearanceAnimationDraftValidationMessage"
        );
        const preview = findChild(page, "appearancePreview");
        const summary = findChild(page, "appearancePreviewSummary");
        const disclaimer = findChild(
            page, "appearanceAnimationPreviewDisclaimer"
        );
        const activeWindow = findChild(
            page, "appearancePreviewActiveWindow"
        );
        const spawnedWindow = findChild(
            page, "appearancePreviewSpawnedWindow"
        );
        for (const item of [renderingCard, modulationCard, contextsCard,
                            heading, validation, aggregateValidation,
                            animationValidation, preview, summary,
                            disclaimer, activeWindow, spawnedWindow]) {
            verify(item !== null);
        }
        for (const row of rows)
            verify(row !== null);
        for (const field of fields)
            verify(field !== null);

        verify(renderingCard.y < modulationCard.y);
        verify(modulationCard.y < contextsCard.y);
        compare(heading.text, "Blur color modulation");
        compare(heading.textFormat, Text.PlainText);
        compare(heading.Accessible.role, Accessible.Heading);
        compare(heading.Accessible.name, heading.text);
        compare(rows[0].title, "Blur brightness");
        compare(rows[1].title, "Blur contrast");
        compare(rows[2].title, "Blur noise");
        compare(rows[3].title, "Blur vibrancy");
        compare(rows[4].title, "Dark-area vibrancy");
        verify(rows[0].description.includes("default is 1"));
        verify(rows[1].description.includes("default is 0.8916"));
        verify(rows[2].description.includes("default is 0.0117"));
        verify(rows[3].description.includes("default is 0.1696"));
        verify(rows[4].description.includes("default is 0"));
        compare(rows[0].validationExample, "0.5");

        compare(page.draftDirty, false);
        compare(page.draftValuesValid, true);
        compare(page.draftAnimationCollectionsValid, true);
        compare(rows[0].value, 1e-7);
        compare(rows[0].projectedValue, "0.0000001");
        compare(rows[0].localEditActive, false);
        compare(fields[0].text, "0.0000001");
        compare(fields[1].text, "0.8916");
        compare(fields[2].text, minimumPlainDecimal);
        compare(fields[3].text, "0.3333333333333333");
        compare(fields[4].text, "0.875");
        compare(rows[0].plainDecimalString(1.25e3), "1250");
        compare(rows[0].plainDecimalString(-1.25e3), "-1250");
        compare(rows[0].plainDecimalString(1.25e-3), "0.00125");
        compare(rows[0].plainDecimalString(-1.25e-3), "-0.00125");
        compare(rows[0].plainDecimalString(-0), "0");
        compare(
            rows[0].plainDecimalString(Number.MIN_VALUE),
            minimumPlainDecimal
        );
        compare(
            rows[0].plainDecimalString(-Number.MIN_VALUE),
            "-" + minimumPlainDecimal
        );
        compare(minimumPlainDecimal.length, 326);
        compare(("-" + minimumPlainDecimal).length, 327);
        compare(rows[0].maximumPlainDecimalLength, 326);
        compare(fields[0].maximumLength, 326);
        compare(fields[2].maximumLength, 326);
        compare(fields[2].parseDecimal(minimumPlainDecimal), Number.MIN_VALUE);
        fields[2].forceActiveFocus();
        fields[2].textEdited();
        wait(0);
        compare(hydrationNoiseEditCount, 1);
        compare(page.draftValue(page.blurNoiseId), Number.MIN_VALUE);
        compare(page.draftDirty, false);
        compare(fields[2].text, minimumPlainDecimal);

        const accessibleNames = [
            "Blur brightness", "Blur contrast", "Blur noise",
            "Blur vibrancy", "Dark-area vibrancy"
        ];
        for (let index = 0; index < fields.length; ++index) {
            compare(fields[index].Accessible.name, accessibleNames[index]);
            compare(fields[index].inputValid, true);
            verify(fields[index].implicitHeight >= page.minimumTargetSize);
        }
        compare(validation.visible, false);
        compare(aggregateValidation.visible, false);
        compare(summary.Accessible.name, summary.text);
        verify(String(summary.text).includes("Blur brightness 1e-7"));
        verify(String(summary.text).includes("Blur contrast 0.8916"));
        verify(String(summary.text).includes("Blur noise 5e-324"));
        verify(String(summary.text).includes(
            "Blur vibrancy 0.3333333333333333"
        ));
        verify(String(summary.text).includes("Dark-area vibrancy 0.875"));
        for (const phrase of ["brightness", "contrast", "noise",
                              "vibrancy", "dark-area vibrancy"]) {
            verify(String(disclaimer.text).includes(phrase));
        }

        preview.motionPaused = true;
        preview.motionProgress = 1;
        wait(0);
        const activeColor = String(activeWindow.color);
        const spawnedColor = String(spawnedWindow.color);
        const activeWidth = activeWindow.width;
        const spawnedWidth = spawnedWindow.width;
        const activeOpacity = activeWindow.opacity;
        const spawnedOpacity = spawnedWindow.opacity;

        fields[1].forceActiveFocus();
        fields[1].text = "1.234500";
        fields[1].textEdited();
        compare(page.draftValue(page.blurContrastId), 1.2345);
        compare(fields[1].text, "1.234500");
        fields[0].forceActiveFocus();
        wait(0);
        compare(fields[1].activeFocus, false);
        compare(rows[1].localEditActive, false);
        compare(fields[1].text, "1.2345");
        page.setExactDecimalDraftValue(page.blurBrightnessId, 0.125);
        page.setExactDecimalDraftValue(page.blurNoiseId, 0.25);
        page.setExactDecimalDraftValue(page.blurVibrancyId, 0);
        page.setExactDecimalDraftValue(
            page.blurVibrancyDarknessId, 0.75
        );
        wait(0);
        compare(fields[3].enabled, true);
        compare(fields[4].enabled, true);
        verify(String(summary.text).includes("Blur brightness 0.125"));
        verify(String(summary.text).includes("Blur contrast 1.2345"));
        verify(String(summary.text).includes("Blur noise 0.25"));
        verify(String(summary.text).includes("Blur vibrancy 0"));
        verify(String(summary.text).includes("Dark-area vibrancy 0.75"));
        compare(String(activeWindow.color), activeColor);
        compare(String(spawnedWindow.color), spawnedColor);
        compare(activeWindow.width, activeWidth);
        compare(spawnedWindow.width, spawnedWidth);
        compare(activeWindow.opacity, activeOpacity);
        compare(spawnedWindow.opacity, spawnedOpacity);

        page.setDraftValue(page.blurId, false);
        wait(0);
        for (const field of fields)
            compare(field.enabled, false);
        compare(page.draftValue(page.blurBrightnessId), 0.125);
        compare(page.draftValue(page.blurContrastId), 1.2345);
        compare(page.draftValue(page.blurNoiseId), 0.25);
        compare(page.draftValue(page.blurVibrancyId), 0);
        compare(page.draftValue(page.blurVibrancyDarknessId), 0.75);
        page.setDraftValue(page.blurId, true);
        wait(0);
        for (const field of fields)
            compare(field.enabled, true);

        const invalidTexts = [
            "", "+0.5", " 0.5", "01", ".5", "1.", "1e-1", "2.0001"
        ];
        for (const invalidText of invalidTexts) {
            fields[0].forceActiveFocus();
            fields[0].text = invalidText;
            fields[0].textEdited();
            wait(0);
            compare(page.draftValue(page.blurBrightnessId), invalidText);
            compare(fields[0].inputValid, false);
            compare(page.draftValuesValid, false);
            compare(page.draftAnimationCollectionsValid, true);
            compare(page.draftValid, false);
            compare(page.draftDirty, true);
            compare(findChild(page, "saveAppearanceButton").enabled, false);
            compare(validation.visible, true);
            compare(validation.Accessible.role, Accessible.AlertMessage);
            compare(validation.Accessible.name, validation.text);
            verify(String(fields[0].Accessible.description).includes(
                "Enter a plain decimal from 0 through 2"
            ));
            compare(aggregateValidation.visible, true);
            verify(String(aggregateValidation.text).includes(
                "Return to Visuals"
            ));
            compare(animationValidation.visible, false);

            fields[0].text = "1.25";
            fields[0].textEdited();
            wait(0);
            compare(page.draftValue(page.blurBrightnessId), 1.25);
            compare(page.draftValuesValid, true);
            compare(page.draftValid, true);
            compare(validation.visible, false);
        }

        fields[2].forceActiveFocus();
        fields[2].text = "-0";
        fields[2].textEdited();
        fields[1].forceActiveFocus();
        wait(0);
        compare(page.draftValue(page.blurNoiseId), 0);
        compare(fields[2].text, "0");

        fields[0].forceActiveFocus();
        fields[0].text = "1e0";
        fields[0].textEdited();
        wait(0);
        compare(page.draftValue(page.blurBrightnessId), "1e0");
        verify(String(summary.text).includes(
            "Blur brightness invalid draft"
        ));
        compare(String(activeWindow.color), activeColor);
        compare(String(spawnedWindow.color), spawnedColor);
        page.appearanceTabIndex = 1;
        wait(0);
        compare(page.draftValue(page.blurBrightnessId), "1e0");
        compare(animationValidation.visible, false);
        compare(aggregateValidation.visible, true);
        page.appearanceTabIndex = 0;
        wait(0);

        const newer = appearanceDefaults();
        newer[page.blurBrightnessId] = 0.75;
        page.appearanceValues = newer;
        page.revisionToken = "8";
        wait(0);
        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.blurBrightnessId), "1e0");
        verify(String(summary.text).includes(
            "Blur brightness invalid draft"
        ));
        const loadCurrent = findChild(
            page, "loadCurrentAppearanceButton"
        );
        verify(loadCurrent !== null);
        compare(loadCurrent.enabled, true);
        loadCurrent.clicked();
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.draftValuesValid, true);
        compare(page.draftValue(page.blurBrightnessId), 0.75);
        compare(fields[0].text, "0.75");

        page.setExactDecimalDraftValue(page.blurContrastId, 1.5);
        findChild(page, "discardAppearanceDraftButton").clicked();
        wait(0);
        compare(page.draftValue(page.blurContrastId), 0.8916);
        page.setExactDecimalDraftValue(page.blurBrightnessId, 0.25);
        page.setExactDecimalDraftValue(page.blurContrastId, 1.25);
        page.setExactDecimalDraftValue(page.blurNoiseId, 0.5);
        page.setExactDecimalDraftValue(page.blurVibrancyId, 0.75);
        page.setExactDecimalDraftValue(
            page.blurVibrancyDarknessId, 0.25
        );
        findChild(page, "resetAppearanceDefaultsButton").clicked();
        wait(0);
        compare(page.draftValue(page.blurBrightnessId), 1);
        compare(page.draftValue(page.blurContrastId), 0.8916);
        compare(page.draftValue(page.blurNoiseId), 0.0117);
        compare(page.draftValue(page.blurVibrancyId), 0.1696);
        compare(page.draftValue(page.blurVibrancyDarknessId), 0);
    }

    function test_appearanceRoundingPowerUsesExactIndependentDrafts() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const row = findChild(page, "appearanceRoundingPowerRow");
        verify(row !== null);
        let hydrationEditCount = 0;
        row.valueModified.connect(function() {
            ++hydrationEditCount;
        });

        const authority = appearanceDefaults();
        authority[page.borderSizeId] = 7;
        authority[page.roundingId] = 0;
        authority[page.roundingPowerId] = 7.421;
        configureAppearancePage(page, authority, true);
        page.reviewProjection();
        waitForRendering(page);
        wait(0);
        compare(hydrationEditCount, 0);

        const field = findChild(page, "appearanceRoundingPower");
        const validation = findChild(
            page, "appearanceRoundingPowerValidation"
        );
        const border = findChild(page, "appearanceBorderSize");
        const radius = findChild(page, "appearanceRounding");
        const blur = findChild(page, "appearanceBlurEnabled");
        const aggregateValidation = findChild(
            page, "appearanceDraftValidationMessage"
        );
        const preview = findChild(page, "appearancePreview");
        const activeWindow = findChild(
            page, "appearancePreviewActiveWindow"
        );
        const spawnedWindow = findChild(
            page, "appearancePreviewSpawnedWindow"
        );
        const secondaryWindow = findChild(
            page, "appearancePreviewSecondaryWindow"
        );
        const summary = findChild(page, "appearancePreviewSummary");
        const disclaimer = findChild(
            page, "appearanceAnimationPreviewDisclaimer"
        );
        const save = findChild(page, "saveAppearanceButton");
        const refresh = findChild(page, "refreshAppearanceButton");
        const discard = findChild(
            page, "discardAppearanceDraftButton"
        );
        const reset = findChild(page, "resetAppearanceDefaultsButton");
        const loadCurrent = findChild(
            page, "loadCurrentAppearanceButton"
        );
        for (const item of [field, validation, border, radius, blur,
                            aggregateValidation, preview, activeWindow,
                            spawnedWindow, secondaryWindow, summary,
                            disclaimer, save, refresh, discard, reset,
                            loadCurrent]) {
            verify(item !== null);
        }

        compare(row.title, "Window corner power");
        compare(
            row.description,
            "Set the corner power from 2 through 10. The default 2 is circular; higher values make corners squarer, and Hyprland adjusts the effective corner radius with the power. This direct compositor choice stays editable while shared borders are synced or the global radius is zero because a Window Rule may still use it."
        );
        compare(row.minimumValue, 2);
        compare(row.maximumValue, 10);
        compare(row.controlWidth, 190);
        compare(row.validationExample, "2.5");
        compare(field.text, "7.421");
        compare(field.inputValid, true);
        compare(field.enabled, true);
        compare(border.enabled, false);
        compare(radius.enabled, false);
        compare(field.Accessible.name, "Window corner power");
        compare(field.Accessible.description, row.description);
        verify(field.implicitHeight >= page.minimumTargetSize);
        compare(validation.visible, false);
        compare(preview.roundingPower, 7.421);
        verify(String(summary.text).includes(
            "Corner radius 0. Window corner power 7.421."
        ));
        verify(row.mapToItem(page, 0, 0).y
            > radius.mapToItem(page, 0, 0).y);
        verify(row.mapToItem(page, 0, 0).y
            < blur.mapToItem(page, 0, 0).y);
        verify(String(disclaimer.text).includes(
            "Window corner power"
        ));
        verify(String(disclaimer.text).includes("not simulated"));

        preview.motionPaused = true;
        preview.motionProgress = 1;
        wait(0);
        const activeGeometry = [activeWindow.x, activeWindow.y,
                                activeWindow.width, activeWindow.height,
                                activeWindow.radius, activeWindow.opacity,
                                String(activeWindow.color)];
        const spawnedGeometry = [spawnedWindow.x, spawnedWindow.y,
                                 spawnedWindow.width, spawnedWindow.height,
                                 spawnedWindow.radius, spawnedWindow.opacity,
                                 String(spawnedWindow.color)];
        const secondaryGeometry = [
            secondaryWindow.x, secondaryWindow.y, secondaryWindow.width,
            secondaryWindow.height, secondaryWindow.radius,
            secondaryWindow.opacity, String(secondaryWindow.color)
        ];
        const motionState = [preview.motionPaused, preview.motionProgress,
                             preview.motionPhase, preview.motionStory];

        for (const rejected of [1.9999, 10.0001, NaN,
                                Infinity, -Infinity]) {
            page.setExactDecimalDraftValue(page.roundingPowerId, rejected);
        }
        compare(page.draftValue(page.roundingPowerId), 7.421);

        field.forceActiveFocus();
        field.text = "2.5730";
        field.textEdited();
        wait(0);
        compare(hydrationEditCount, 1);
        compare(page.draftValue(page.roundingPowerId), 2.573);
        compare(field.text, "2.5730");
        compare(preview.roundingPower, 2.573);
        verify(String(summary.text).includes("Window corner power 2.573"));
        refresh.forceActiveFocus();
        wait(0);
        compare(field.activeFocus, false);
        compare(row.localEditActive, false);
        compare(field.text, "2.573");
        compare(
            [activeWindow.x, activeWindow.y, activeWindow.width,
             activeWindow.height, activeWindow.radius, activeWindow.opacity,
             String(activeWindow.color)],
            activeGeometry
        );
        compare(
            [spawnedWindow.x, spawnedWindow.y, spawnedWindow.width,
             spawnedWindow.height, spawnedWindow.radius,
             spawnedWindow.opacity, String(spawnedWindow.color)],
            spawnedGeometry
        );
        compare(
            [secondaryWindow.x, secondaryWindow.y, secondaryWindow.width,
             secondaryWindow.height, secondaryWindow.radius,
             secondaryWindow.opacity, String(secondaryWindow.color)],
            secondaryGeometry
        );
        compare(
            [preview.motionPaused, preview.motionProgress,
             preview.motionPhase, preview.motionStory],
            motionState
        );

        const invalidTexts = [
            "", "+2", " 2", "02", ".5", "2.", "2e0", "1.9999",
            "10.0001"
        ];
        for (const invalidText of invalidTexts) {
            field.forceActiveFocus();
            field.text = invalidText;
            field.textEdited();
            wait(0);
            compare(page.draftValue(page.roundingPowerId), invalidText);
            compare(field.inputValid, false);
            compare(page.draftValuesValid, false);
            compare(page.draftValid, false);
            compare(save.enabled, false);
            compare(field.enabled, true);
            compare(validation.visible, true);
            compare(validation.Accessible.role, Accessible.AlertMessage);
            compare(validation.Accessible.name, validation.text);
            verify(String(validation.text).includes(
                "plain decimal from 2 through 10"
            ));
            verify(String(validation.text).includes("for example, 2.5"));
            compare(field.Accessible.description, validation.text);
            compare(aggregateValidation.visible, true);
            verify(String(aggregateValidation.text).includes(
                "Return to Visuals"
            ));
            verify(Number.isNaN(preview.roundingPower));
            verify(String(summary.text).includes(
                "Corner radius 0. Window corner power invalid draft."
            ));
            compare(
                [activeWindow.x, activeWindow.y, activeWindow.width,
                 activeWindow.height, activeWindow.radius,
                 activeWindow.opacity, String(activeWindow.color)],
                activeGeometry
            );
            compare(
                [secondaryWindow.x, secondaryWindow.y,
                 secondaryWindow.width, secondaryWindow.height,
                 secondaryWindow.radius, secondaryWindow.opacity,
                 String(secondaryWindow.color)],
                secondaryGeometry
            );
            compare(
                [preview.motionPaused, preview.motionProgress,
                 preview.motionPhase, preview.motionStory],
                motionState
            );

            field.text = "7.421";
            field.textEdited();
            wait(0);
            compare(page.draftValue(page.roundingPowerId), 7.421);
            compare(field.inputValid, true);
            compare(page.draftValuesValid, true);
            compare(validation.visible, false);
            compare(aggregateValidation.visible, false);
            compare(preview.roundingPower, 7.421);
        }

        field.forceActiveFocus();
        field.text = "2e0";
        field.textEdited();
        refresh.forceActiveFocus();
        wait(0);
        discard.clicked();
        wait(0);
        compare(page.draftValue(page.roundingPowerId), 7.421);
        compare(field.inputValid, true);

        field.forceActiveFocus();
        field.text = "2e0";
        field.textEdited();
        refresh.forceActiveFocus();
        wait(0);
        reset.clicked();
        wait(0);
        compare(page.draftValue(page.borderSizeId), 7);
        compare(page.draftValue(page.roundingId), 0);
        compare(page.draftValue(page.roundingPowerId), 2);
        compare(field.inputValid, true);

        field.forceActiveFocus();
        field.text = "2e0";
        field.textEdited();
        refresh.forceActiveFocus();
        wait(0);
        const newer = appearanceDefaults();
        newer[page.borderSizeId] = 7;
        newer[page.roundingId] = 0;
        newer[page.roundingPowerId] = 3.257;
        page.appearanceValues = newer;
        page.revisionToken = "8";
        wait(0);
        compare(page.externalChangeWhileEditing, true);
        compare(loadCurrent.enabled, true);
        loadCurrent.clicked();
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftValue(page.roundingPowerId), 3.257);
        compare(field.text, "3.257");
        compare(field.inputValid, true);

        compare(field.enabled, true);
        compare(border.enabled, false);
        compare(radius.enabled, false);
        compare(page.draftValue(page.roundingId), 0);
    }

    function test_syncedWindowBorderPairIsReadOnlyAndChangesSourceAtomically() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 7;
        resolvedValues[page.roundingId] = 13;
        resolvedValues[page.roundingPowerId] = 7.421;
        configureAppearancePage(page, resolvedValues, true);
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        const border = findChild(page, "appearanceBorderSize");
        const rounding = findChild(page, "appearanceRounding");
        const roundingPower = findChild(
            page, "appearanceRoundingPower"
        );
        const blur = findChild(page, "appearanceBlurEnabled");
        const source = findChild(page, "windowBorderSourceButton");
        const message = findChild(page, "windowBorderAuthorityMessage");
        verify(border !== null);
        verify(rounding !== null);
        verify(roundingPower !== null);
        verify(blur !== null);
        verify(source !== null);
        verify(message !== null);

        compare(border.value, 7);
        compare(rounding.value, 13);
        compare(border.enabled, false);
        compare(rounding.enabled, false);
        compare(roundingPower.enabled, true);
        compare(page.draftValue(page.roundingPowerId), 7.421);
        compare(blur.enabled, true);
        compare(source.text, "Override window borders");
        compare(source.enabled, true);
        verify(source.implicitHeight >= 44);
        verify(String(message.text).includes("Controlled by HyprShelld"));
        verify(String(message.text).includes("Bar page"));

        let requestCount = 0;
        let requests = [];
        page.windowBorderSyncRequested.connect(function(sync) {
            ++requestCount;
            requests.push(sync);
        });

        source.clicked();
        compare(requestCount, 1);
        compare(requests, [false]);

        // Model the single authoritative Config1 update that follows the
        // request. Only the synchronized pair changes editability.
        page.sharedBorderBusy = true;
        page.sharedBorderConfigRevisionToken = "12";
        page.windowBorderSynced = false;
        page.sharedBorderSyncState = "override";
        page.sharedBorderVerifiedRevisionToken = "12";
        page.sharedBorderBusy = false;
        wait(0);
        compare(border.enabled, true);
        compare(rounding.enabled, true);
        compare(roundingPower.enabled, true);
        compare(page.draftValue(page.roundingPowerId), 7.421);
        compare(blur.enabled, true);
        compare(source.text, "Sync with HyprShelld");

        page.setDraftValue(page.roundingId, 0);
        compare(page.draftValue(page.roundingId), 0);
        compare(roundingPower.enabled, true);
        compare(page.draftValue(page.roundingPowerId), 7.421);
        page.setDraftValue(page.roundingId, 13);
        compare(page.draftValue(page.roundingId), 13);
        compare(page.draftValue(page.roundingPowerId), 7.421);

        source.clicked();
        compare(requestCount, 2);
        compare(requests, [false, true]);

        page.sharedBorderBusy = true;
        page.sharedBorderConfigRevisionToken = "13";
        page.windowBorderSynced = true;
        page.sharedBorderSyncState = "current";
        page.sharedBorderVerifiedRevisionToken = "13";
        page.sharedBorderBusy = false;
        wait(0);
        compare(border.enabled, false);
        compare(rounding.enabled, false);
        compare(roundingPower.enabled, true);
        compare(page.draftValue(page.roundingPowerId), 7.421);
    }

    function test_unavailableProjectionKeepsSourceActionReversible() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const overrideValues = appearanceDefaults();
        overrideValues[page.borderSizeId] = 4;
        overrideValues[page.roundingId] = 5;
        configureAppearancePage(page, overrideValues);
        waitForRendering(page);
        wait(0);

        const source = findChild(page, "windowBorderSourceButton");
        const save = findChild(page, "saveAppearanceButton");
        const retryApply = findChild(
            page,
            "retryApplyAppearanceButton"
        );
        verify(source !== null);
        verify(save !== null);
        verify(retryApply !== null);

        const requests = [];
        page.windowBorderSyncRequested.connect(function(sync) {
            requests.push(sync);
        });
        source.clicked();
        compare(requests, [true]);

        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 8;
        resolvedValues[page.roundingId] = 12;
        page.sharedBorderBusy = true;
        page.sharedBorderConfigRevisionToken = "12";
        page.windowBorderSynced = true;
        page.appearanceValues = resolvedValues;
        page.sharedBorderSyncState = "unavailable";
        page.sharedBorderBusy = false;
        wait(0);
        page.reviewProjection();

        compare(page.sharedBorderSourceRequestPending, false);
        compare(page.sharedBorderProjectionPending, true);
        compare(page.sharedBorderRevisionVerified, false);
        compare(page.draftValue(page.borderSizeId), 8);
        compare(page.draftValue(page.roundingId), 12);
        compare(page.draftDirty, false);
        compare(source.enabled, true);
        compare(save.enabled, false);
        page.retryApplyAvailable = true;
        compare(retryApply.enabled, false);

        source.clicked();
        compare(requests, [true, false]);
        page.sharedBorderBusy = true;
        page.sharedBorderConfigRevisionToken = "13";
        page.windowBorderSynced = false;
        page.appearanceValues = overrideValues;
        page.sharedBorderBusy = false;
        wait(0);
        page.reviewProjection();

        compare(page.sharedBorderSourceRequestPending, false);
        compare(page.sharedBorderProjectionPending, true);
        compare(page.sharedBorderRevisionVerified, false);
        compare(page.draftValue(page.borderSizeId), 4);
        compare(page.draftValue(page.roundingId), 5);
        compare(page.draftDirty, false);
        compare(source.enabled, true);
        compare(save.enabled, false);
        compare(retryApply.enabled, false);
        compare(requests, [true, false]);
    }

    function test_dirtyAppearanceDraftBlocksWindowBorderSourceChanges() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const source = findChild(page, "windowBorderSourceButton");
        verify(source !== null);
        compare(source.enabled, true);

        page.setDraftValue(page.blurId, false);
        compare(page.draftDirty, true);
        compare(source.enabled, false);

        page.synchronizeDraft();
        compare(page.draftDirty, false);
        compare(source.enabled, true);

        page.saveSubmitted = true;
        compare(source.enabled, false);
        page.saveSubmitted = false;
        compare(source.enabled, true);
    }

    function test_sharedBorderBusyPreservesAndLocksAppearanceDraft() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.blurId, false);
        const save = findChild(page, "saveAppearanceButton");
        const source = findChild(page, "windowBorderSourceButton");
        verify(save !== null);
        verify(source !== null);
        compare(page.draftDirty, true);
        compare(save.enabled, true);

        let saveCount = 0;
        page.saveRequested.connect(function() { ++saveCount; });
        page.sharedBorderBusy = true;
        wait(0);
        compare(page.controlsEnabled, false);
        compare(save.enabled, false);
        compare(source.enabled, false);

        page.setDraftValue(page.shadowId, false);
        page.resetDraftToDefaults();
        page.synchronizeDraft();
        page.submitDraft();
        compare(page.draftValue(page.blurId), false);
        compare(page.draftValue(page.shadowId), true);
        compare(page.draftDirty, true);
        compare(saveCount, 0);

        page.sharedBorderBusy = false;
        wait(0);
        compare(page.controlsEnabled, true);
        compare(page.draftValue(page.blurId), false);
        compare(save.enabled, true);
        page.setDraftValue(page.shadowId, false);
        compare(page.draftValue(page.shadowId), false);
    }

    function test_appearanceApplyActionsRequireSettledSharedBorderAuthority() {
        const rows = [
            {
                label: "synchronized current",
                synced: true,
                state: "current",
                sourceBusy: false,
                safe: true,
                controlsEnabled: true
            },
            {
                label: "synchronized saved",
                synced: true,
                state: "saved",
                sourceBusy: false,
                safe: true,
                controlsEnabled: true
            },
            {
                label: "synchronized pending",
                synced: true,
                state: "pending",
                sourceBusy: false,
                safe: false,
                controlsEnabled: false
            },
            {
                label: "synchronized unavailable",
                synced: true,
                state: "unavailable",
                sourceBusy: false,
                safe: false,
                controlsEnabled: true
            },
            {
                label: "synchronized failed",
                synced: true,
                state: "failed",
                sourceBusy: false,
                safe: false,
                controlsEnabled: true
            },
            {
                label: "explicit override",
                synced: false,
                state: "override",
                sourceBusy: false,
                safe: true,
                controlsEnabled: true
            },
            {
                label: "override source busy",
                synced: false,
                state: "override",
                sourceBusy: true,
                safe: false,
                controlsEnabled: false
            },
            {
                label: "incoherent override state",
                synced: false,
                state: "current",
                sourceBusy: false,
                safe: false,
                controlsEnabled: true
            }
        ];

        for (const row of rows) {
            const testWindow = createTemporaryObject(
                appearancePageComponent,
                this
            );
            verify(testWindow !== null, row.label);
            const page = testWindow.page;
            configureAppearancePage(page, undefined, row.synced);
            waitForRendering(page);
            wait(0);

            page.setDraftValue(page.blurId, false);
            compare(page.draftDirty, true, row.label);
            page.retryApplyAvailable = true;
            page.sharedBorderSyncState = row.state;
            page.sharedBorderBusy = row.sourceBusy;
            wait(0);

            const save = findChild(page, "saveAppearanceButton");
            const retry = findChild(page, "retryApplyAppearanceButton");
            verify(save !== null, row.label);
            verify(retry !== null, row.label);
            compare(page.sharedBorderApplySafe, row.safe, row.label);
            compare(page.controlsEnabled, row.controlsEnabled, row.label);
            compare(save.enabled, row.safe, row.label);
            compare(retry.enabled, row.safe, row.label);
            compare(page.draftValue(page.blurId), false, row.label);
            testWindow.destroy();
        }
    }

    function test_unsyncedSavedSpacingIsActionableButNotClaimedActive() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page, undefined, false, false);
        waitForRendering(page);
        wait(0);

        page.sharedSpacingSyncState = "saved";
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.setDraftValue(page.blurId, false);
        wait(0);

        const authority = findChild(
            page,
            "windowSpacingAuthorityMessage"
        );
        const save = findChild(page, "saveAppearanceButton");
        const retry = findChild(page, "retryApplyAppearanceButton");
        verify(authority !== null);
        verify(save !== null);
        verify(retry !== null);
        compare(page.windowSpacingSynced, false);
        compare(page.sharedSpacingProjectionVerified, true);
        compare(page.sharedSpacingApplyStateSettled, true);
        compare(page.sharedSpacingApplySafe, true);
        compare(page.sharedVisualApplySafe, true);
        compare(page.draftDirty, true);
        compare(save.enabled, true);
        compare(retry.visible, true);
        compare(retry.enabled, true);
        verify(String(authority.text).includes(
            "protected maximize rule is saved but not active"
        ));
        verify(String(authority.text).includes(
            "apply the exact pending compositor revision"
        ));

        // An unsynchronized `current` projection is incoherent: it must not
        // inherit the safe state from the previously verified saved revision.
        page.sharedSpacingSyncState = "current";
        wait(0);
        compare(page.sharedSpacingProjectionVerified, false);
        compare(page.sharedSpacingApplyStateSettled, false);
        compare(page.sharedSpacingApplySafe, false);
        compare(page.sharedVisualApplySafe, false);
        compare(save.enabled, false);
        compare(retry.enabled, false);

        // Config1 can disappear before compositord has published a new
        // status. Retained terminal states and matching tokens remain useful
        // projection data, but cannot authorize either activation path.
        page.sharedSpacingSyncState = "saved";
        page.sharedSpacingAvailable = false;
        page.sharedBorderAvailable = false;
        wait(0);
        compare(page.sharedSpacingProjectionVerified, true);
        compare(page.sharedSpacingApplyStateSettled, true);
        compare(page.sharedSpacingApplySafe, false);
        compare(page.sharedBorderApplySafe, false);
        compare(page.sharedVisualApplySafe, false);
        compare(page.draftValue(page.blurId), false);
        compare(save.enabled, false);
        compare(retry.enabled, false);
    }

    function test_retryApplyLocksForCompleteSourceTransition() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        page.retryApplyAvailable = true;
        waitForRendering(page);
        wait(0);

        const retry = findChild(page, "retryApplyAppearanceButton");
        const source = findChild(page, "windowBorderSourceButton");
        verify(retry !== null);
        verify(source !== null);
        compare(page.sharedBorderApplySafe, true);
        compare(retry.enabled, true);

        let sourceRequests = 0;
        page.windowBorderSyncRequested.connect(function(sync) {
            ++sourceRequests;
            compare(sync, true);
        });
        source.clicked();
        compare(sourceRequests, 1);
        compare(page.sharedBorderSourceRequestPending, true);
        compare(retry.enabled, false);

        page.sharedBorderBusy = true;
        page.sharedBorderConfigRevisionToken = "12";
        page.sharedBorderSyncState = "pending";
        page.windowBorderSynced = true;
        page.sharedBorderBusy = false;
        wait(0);
        compare(page.sharedBorderSourceRequestPending, false);
        compare(page.sharedBorderProjectionPending, true);
        compare(retry.enabled, false);

        page.sharedBorderVerifiedRevisionToken = "12";
        page.sharedBorderSyncState = "saved";
        wait(0);
        page.reviewProjection();
        compare(page.sharedBorderProjectionPending, false);
        compare(page.sharedBorderApplySafe, true);
        compare(retry.enabled, true);
        compare(sourceRequests, 1);
    }

    function test_sharedBorderRevisionCorrelationIsLosslessAndPolicyAware() {
        const terminalStates = ["current", "saved"];
        const configRevision = "9007199254740993";
        const olderRevision = "9007199254740992";

        for (const terminalState of terminalStates) {
            const testWindow = createTemporaryObject(
                appearancePageComponent,
                this
            );
            verify(testWindow !== null, terminalState);
            const page = testWindow.page;
            configureAppearancePage(page);
            page.retryApplyAvailable = true;
            waitForRendering(page);
            wait(0);

            const source = findChild(page, "windowBorderSourceButton");
            const retry = findChild(page, "retryApplyAppearanceButton");
            verify(source !== null, terminalState);
            verify(retry !== null, terminalState);

            const resolvedValues = appearanceDefaults();
            resolvedValues[page.borderSizeId] = 6;
            resolvedValues[page.roundingId] = 10;
            page.sharedBorderBusy = true;
            page.sharedBorderConfigRevisionToken = configRevision;
            page.sharedBorderVerifiedRevisionToken = olderRevision;
            page.windowBorderSynced = true;
            page.appearanceValues = resolvedValues;
            page.sharedBorderSyncState = terminalState;
            page.sharedBorderBusy = false;
            wait(0);
            page.reviewProjection();

            // No transient pending state was observed. The already-terminal
            // projection remains gated until the exact source revision lands.
            compare(
                page.sharedBorderConfigRevisionToken,
                configRevision,
                terminalState
            );
            compare(
                page.sharedBorderVerifiedRevisionToken,
                olderRevision,
                terminalState
            );
            compare(page.sharedBorderRevisionVerified, false, terminalState);
            compare(
                page.sharedBorderProjectionPending,
                true,
                terminalState
            );
            compare(page.sharedBorderApplySafe, false, terminalState);
            compare(retry.enabled, false, terminalState);
            compare(source.enabled, true, terminalState);

            page.sharedBorderVerifiedRevisionToken =
                "0" + configRevision;
            wait(0);
            compare(page.sharedBorderRevisionVerified, false, terminalState);
            compare(
                page.sharedBorderProjectionPending,
                true,
                terminalState
            );

            page.sharedBorderVerifiedRevisionToken = configRevision;
            wait(0);
            page.reviewProjection();
            compare(page.sharedBorderRevisionVerified, true, terminalState);
            compare(
                page.sharedBorderProjectionPending,
                false,
                terminalState
            );
            compare(page.sharedBorderApplySafe, true, terminalState);
            compare(retry.enabled, true, terminalState);
            compare(page.draftValue(page.borderSizeId), 6, terminalState);
            compare(page.draftValue(page.roundingId), 10, terminalState);
            testWindow.destroy();
        }

        const terminalFailureWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(terminalFailureWindow !== null);
        const failurePage = terminalFailureWindow.page;
        configureAppearancePage(failurePage);
        failurePage.sharedBorderBusy = true;
        failurePage.sharedBorderConfigRevisionToken = "12";
        failurePage.sharedBorderVerifiedRevisionToken = "12";
        failurePage.windowBorderSynced = true;
        failurePage.sharedBorderSyncState = "failed";
        failurePage.sharedBorderBusy = false;
        wait(0);
        failurePage.reviewProjection();
        compare(failurePage.sharedBorderProjectionPending, false);
        compare(failurePage.controlsEnabled, true);
        compare(failurePage.sharedBorderApplySafe, false);

        const unavailableWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(unavailableWindow !== null);
        const unavailablePage = unavailableWindow.page;
        configureAppearancePage(unavailablePage);
        unavailablePage.sharedBorderBusy = true;
        unavailablePage.sharedBorderConfigRevisionToken = "0";
        unavailablePage.sharedBorderVerifiedRevisionToken = "0";
        unavailablePage.windowBorderSynced = true;
        unavailablePage.sharedBorderSyncState = "unavailable";
        unavailablePage.sharedBorderBusy = false;
        wait(0);
        unavailablePage.reviewProjection();
        compare(unavailablePage.sharedBorderRevisionVerified, true);
        compare(unavailablePage.sharedBorderProjectionVerified, false);
        compare(unavailablePage.sharedBorderProjectionPending, true);
        compare(unavailablePage.sharedBorderApplySafe, false);

        unavailablePage.sharedBorderConfigRevisionToken = "12";
        unavailablePage.sharedBorderVerifiedRevisionToken = "12";
        wait(0);
        unavailablePage.reviewProjection();
        compare(unavailablePage.sharedBorderProjectionVerified, true);
        compare(unavailablePage.sharedBorderProjectionPending, false);
        compare(unavailablePage.controlsEnabled, true);
        compare(unavailablePage.sharedBorderApplySafe, false);
    }

    function test_settledSourceTransitionProjectsOneCoherentBorderPair() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const overrideValues = appearanceDefaults();
        overrideValues[page.borderSizeId] = 4;
        overrideValues[page.roundingId] = 5;
        configureAppearancePage(page, overrideValues);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.blurId, false);
        compare(page.draftDirty, true);

        page.sharedBorderBusy = true;
        page.sharedBorderConfigRevisionToken = "12";
        page.sharedBorderSyncState = "pending";
        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 9;
        resolvedValues[page.roundingId] = 14;
        page.appearanceValues = resolvedValues;
        page.revisionToken = "8";
        page.windowBorderSynced = true;
        wait(0);

        compare(page.controlsEnabled, false);
        compare(page.draftValue(page.borderSizeId), 4);
        compare(page.draftValue(page.roundingId), 5);
        compare(findChild(page, "saveAppearanceButton").enabled, false);

        page.sharedBorderBusy = false;
        wait(0);
        compare(page.controlsEnabled, false);
        page.sharedBorderVerifiedRevisionToken = "12";
        page.sharedBorderSyncState = "current";
        wait(0);
        page.reviewProjection();

        const border = findChild(page, "appearanceBorderSize");
        const rounding = findChild(page, "appearanceRounding");
        const source = findChild(page, "windowBorderSourceButton");
        const save = findChild(page, "saveAppearanceButton");
        verify(border !== null);
        verify(rounding !== null);
        verify(source !== null);
        verify(save !== null);
        compare(page.draftValue(page.borderSizeId), 9);
        compare(page.draftValue(page.roundingId), 14);
        compare(page.synchronizedValues[page.borderSizeId], 9);
        compare(page.synchronizedValues[page.roundingId], 14);
        compare(page.draftValue(page.blurId), false);
        compare(page.synchronizedValues[page.blurId], true);
        compare(page.synchronizedRevisionToken, "8");
        compare(page.externalChangeWhileEditing, false);
        compare(page.sharedBorderProjectionPending, false);
        compare(page.draftDirty, true);
        compare(border.value, 9);
        compare(rounding.value, 14);
        compare(border.enabled, false);
        compare(rounding.enabled, false);
        compare(source.enabled, false);
        compare(save.enabled, true);
    }

    function test_sharedBorderHydrationWaitsForAuthoritativeRevision() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const overrideValues = appearanceDefaults();
        overrideValues[page.borderSizeId] = 4;
        overrideValues[page.roundingId] = 5;
        configureAppearancePage(page, overrideValues);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.blurId, false);
        compare(page.draftDirty, true);
        compare(page.synchronizedRevisionToken, "7");

        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 9;
        resolvedValues[page.roundingId] = 14;

        // A reconnect can deliver the terminal shared-border tuple while the
        // compositor client still exposes its old, unavailable snapshot.
        page.serviceAvailable = false;
        page.sharedBorderConfigRevisionToken = "12";
        page.sharedBorderVerifiedRevisionToken = "12";
        page.windowBorderSynced = true;
        page.appearanceValues = resolvedValues;
        page.sharedBorderSyncState = "current";
        wait(0);
        page.reviewProjection();

        compare(page.sharedBorderProjectionPending, true);
        compare(page.draftValue(page.borderSizeId), 4);
        compare(page.draftValue(page.roundingId), 5);
        compare(page.draftValue(page.blurId), false);
        compare(page.synchronizedValues[page.blurId], true);
        compare(page.synchronizedRevisionToken, "7");
        compare(page.externalChangeWhileEditing, false);

        // The refreshed revision can arrive while availability is still
        // false. Raising availability must schedule the authoritative review.
        page.revisionToken = "8";
        wait(0);
        compare(page.synchronizedRevisionToken, "7");
        compare(page.externalChangeWhileEditing, false);

        page.serviceAvailable = true;
        wait(0);

        compare(page.sharedBorderProjectionPending, false);
        compare(page.draftValue(page.borderSizeId), 9);
        compare(page.draftValue(page.roundingId), 14);
        compare(page.draftValue(page.blurId), false);
        compare(page.synchronizedValues[page.borderSizeId], 9);
        compare(page.synchronizedValues[page.roundingId], 14);
        compare(page.synchronizedValues[page.blurId], true);
        compare(page.synchronizedRevisionToken, "8");
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, true);
        compare(findChild(page, "saveAppearanceButton").enabled, true);
    }

    function test_syncedAppearanceResetPreservesResolvedBorderPair() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 6;
        resolvedValues[page.roundingId] = 12;
        resolvedValues[page.roundingPowerId] = 7.421;
        configureAppearancePage(page, resolvedValues, true);
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        // The synchronized pair rejects Appearance draft writes, while an
        // unrelated setting remains independently editable.
        page.setDraftValue(page.borderSizeId, 3);
        page.setDraftValue(page.roundingId, 4);
        compare(page.draftValue(page.borderSizeId), 6);
        compare(page.draftValue(page.roundingId), 12);
        const roundingPower = findChild(
            page, "appearanceRoundingPower"
        );
        verify(roundingPower !== null);
        compare(roundingPower.enabled, true);
        compare(page.draftValue(page.roundingPowerId), 7.421);
        const reset = findChild(page, "resetAppearanceDefaultsButton");
        verify(reset !== null);
        compare(reset.enabled, true);

        page.setDraftValue(page.blurId, false);
        page.setExactDecimalDraftValue(page.roundingPowerId, 2.573);
        compare(page.draftValue(page.blurId), false);
        compare(page.draftValue(page.roundingPowerId), 2.573);
        compare(page.draftDirty, true);
        compare(reset.enabled, true);
        reset.clicked();
        compare(page.draftValue(page.borderSizeId), 6);
        compare(page.draftValue(page.roundingId), 12);
        compare(page.draftValue(page.roundingPowerId), 2);
        compare(page.draftValue(page.blurId), true);
        compare(page.draftValue(page.animationsId), true);
        compare(page.draftDirty, true);
        compare(reset.enabled, false);
    }

    function test_unavailableSharedBorderSourceFailsClosedLocally() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 5;
        resolvedValues[page.roundingId] = 9;
        configureAppearancePage(page, resolvedValues, true);
        page.sharedBorderAvailable = false;
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        const border = findChild(page, "appearanceBorderSize");
        const rounding = findChild(page, "appearanceRounding");
        const blur = findChild(page, "appearanceBlurEnabled");
        const source = findChild(page, "windowBorderSourceButton");
        const message = findChild(page, "windowBorderAuthorityMessage");
        verify(border !== null);
        verify(rounding !== null);
        verify(blur !== null);
        verify(source !== null);
        verify(message !== null);

        compare(border.enabled, false);
        compare(rounding.enabled, false);
        compare(source.enabled, false);
        verify(String(message.text).includes("service is unavailable"));
        verify(String(message.text).includes("read-only"));
        compare(blur.enabled, true);
        page.setDraftValue(page.blurId, false);
        compare(page.draftValue(page.blurId), false);
        compare(page.draftDirty, true);
    }

    function test_failedSharedBorderSyncOffersOneExplicitRetry() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page, undefined, true);
        page.sharedBorderSyncState = "failed";
        page.sharedBorderSyncError = "Injected synchronization failure.";
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        const message = findChild(page, "windowBorderAuthorityMessage");
        const retry = findChild(page, "retrySharedBorderSyncButton");
        verify(message !== null);
        verify(retry !== null);
        verify(String(message.text).includes("Controlled by HyprShelld"));
        verify(String(message.text).includes(
            "Injected synchronization failure."
        ));
        compare(retry.visible, true);
        compare(retry.enabled, true);
        verify(retry.implicitHeight >= 44);

        let retryCount = 0;
        page.retrySharedBorderSyncRequested.connect(function() {
            ++retryCount;
        });
        retry.clicked();
        compare(retryCount, 1);

        page.busy = true;
        compare(retry.enabled, false);
    }

    function test_unavailableSharedBorderSyncOffersBoundedRetry() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page, undefined, true);
        page.sharedBorderSyncState = "unavailable";
        page.sharedBorderSyncError = "Config1 GetAll failed transiently.";
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        const retry = findChild(page, "retrySharedBorderSyncButton");
        verify(retry !== null);
        compare(page.sharedBorderAvailable, true);
        compare(page.serviceAvailable, true);
        compare(retry.visible, true);
        compare(retry.enabled, true);
        verify(retry.implicitHeight >= 44);

        let retryCount = 0;
        page.retrySharedBorderSyncRequested.connect(function() {
            ++retryCount;
        });
        retry.clicked();
        compare(retryCount, 1);

        page.sharedBorderBusy = true;
        compare(retry.enabled, false);
        page.sharedBorderBusy = false;
        page.busy = true;
        compare(retry.enabled, false);
        page.busy = false;
        page.serviceAvailable = false;
        compare(retry.visible, false);
    }

    function test_sharedBorderMutationFailureIsDistinctAndRetryClearsIt() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const source = findChild(page, "windowBorderSourceButton");
        const error = findChild(
            page,
            "sharedBorderMutationErrorMessage"
        );
        verify(source !== null);
        verify(error !== null);
        compare(error.visible, false);

        // A retained error from another Config1 operation is not evidence
        // that this page's source action failed.
        page.sharedBorderClientError =
            "Retained Bar height mutation failure.";
        wait(0);
        compare(error.visible, false);

        let requestCount = 0;
        let requestedSync = false;
        page.windowBorderSyncRequested.connect(function(sync) {
            ++requestCount;
            requestedSync = sync;
        });
        source.clicked();
        compare(requestCount, 1);
        compare(requestedSync, true);

        // ConfigClient clears its retained error before beginning a mutation.
        page.sharedBorderClientError = "";
        page.sharedBorderBusy = true;
        const failurePrefix =
            "Injected Config1 source mutation failure.";
        const oversizedFailure = failurePrefix
            + new Array(2049).join("x");
        page.sharedBorderClientError = oversizedFailure;
        page.sharedBorderBusy = false;
        wait(0);
        compare(error.visible, true);
        verify(String(error.text).includes(
            "shared border source could not be changed"
        ));
        verify(String(error.text).includes(
            failurePrefix
        ));
        compare(
            page.sharedBorderSourceActionError,
            oversizedFailure.slice(
                0,
                page.maximumSharedBorderSourceErrorLength
            )
        );
        compare(
            page.sharedBorderSourceActionError.length,
            page.maximumSharedBorderSourceErrorLength
        );
        compare(error.textFormat, Text.PlainText);
        compare(error.Accessible.role, Accessible.AlertMessage);
        compare(error.Accessible.name, error.text);
        compare(source.enabled, true);

        source.clicked();
        compare(requestCount, 2);
        compare(requestedSync, true);
        compare(error.visible, false);
        page.sharedBorderClientError = "";
        page.sharedBorderBusy = true;
        compare(source.enabled, false);
        page.sharedBorderConfigRevisionToken = "12";
        page.sharedBorderSyncState = "pending";
        page.windowBorderSynced = true;
        page.sharedBorderVerifiedRevisionToken = "12";
        page.sharedBorderBusy = false;
        page.sharedBorderSyncState = "current";
        wait(0);
        compare(error.visible, false);
        compare(page.sharedBorderSourceRequestPending, false);
        compare(source.enabled, true);
    }

    function test_mainProjectsSharedBorderIntoAppearance() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const appearance = findChild(application, "appearancePage");
        verify(appearance !== null);

        const input = appearanceDefaults();
        input[appearance.borderSizeId] = 19;
        input[appearance.roundingId] = 2;
        const resolved = application.appearanceValuesWithSharedBorder(input);
        verify(resolved !== null);

        if (ConfigClient.syncHyprlandWindowBorders) {
            compare(
                resolved[appearance.borderSizeId],
                ConfigClient.shellBorderEnabled
                    ? ConfigClient.shellBorderWidth : 0
            );
            compare(
                resolved[appearance.roundingId],
                ConfigClient.shellBorderRadius
            );
            compare(input[appearance.borderSizeId], 19);
            compare(input[appearance.roundingId], 2);
        } else {
            compare(resolved, input);
        }
        compare(
            appearance.windowBorderSynced,
            ConfigClient.syncHyprlandWindowBorders
        );
        compare(
            appearance.sharedBorderClientError,
            ConfigClient.lastErrorMessage
        );
        compare(
            appearance.sharedBorderConfigRevisionToken,
            ConfigClient.revisionToken
        );
        compare(
            appearance.sharedBorderVerifiedRevisionToken,
            CompositorClient.sharedBorderSourceRevisionToken
        );
    }

    function test_mainOwnerLossClosesSharedVisualActivationGates() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const appearance = findChild(application, "appearancePage");
        const displays = findChild(application, "displaysPage");
        const input = findChild(application, "inputPage");
        verify(appearance !== null);
        verify(displays !== null);
        verify(input !== null);

        // The isolated test bus has no Config1 owner. Compositor state may
        // remain retained independently, but Main must fail closed before a
        // later compositord availability/status update arrives.
        compare(ConfigClient.available, false);
        compare(application.sharedCompositorApplySafe, false);
        compare(appearance.sharedBorderAvailable, false);
        compare(appearance.sharedSpacingAvailable, false);
        compare(appearance.sharedVisualApplySafe, false);
        compare(displays.sharedApplySafe, false);
        compare(input.sharedApplySafe, false);
    }

    function test_appearanceMotionAutoRunsAndTogglesDeterministically() {
        const testWindow = createTemporaryObject(
            appearancePreviewComponent,
            this
        );
        verify(testWindow !== null);
        const preview = testWindow.preview;
        waitForRendering(preview);
        wait(0);

        const toggle = findChild(
            preview,
            "toggleAppearanceMotionButton"
        );
        const summary = findChild(preview, "appearancePreviewSummary");
        verify(toggle !== null);
        verify(summary !== null);

        compare(preview.animationsEnabled, true);
        compare(preview.motionPaused, false);
        compare(preview.motionRunning, true);
        compare(preview.motionStatus, "playing");
        verify(preview.motionPhase !== "off");
        compare(toggle.enabled, true);
        compare(toggle.text, "Pause motion");
        compare(
            toggle.Accessible.name,
            "Pause the illustrative window motion"
        );
        verify(toggle.implicitHeight >= 44);
        compare(summary.Accessible.name, summary.text);
        verify(String(summary.text).includes("Motion playing"));

        toggle.clicked();
        compare(preview.motionPaused, true);
        compare(preview.motionRunning, false);
        compare(preview.motionStatus, "paused");
        compare(toggle.text, "Play motion");
        compare(
            toggle.Accessible.name,
            "Play the illustrative window motion"
        );
        verify(String(summary.text).includes("Motion paused"));

        const pausedPhase = preview.motionPhase;
        const pausedProgress = preview.motionProgress;
        preview.synchronizeMotion(false);
        wait(0);
        compare(preview.motionRunning, false);
        compare(preview.motionPhase, pausedPhase);
        compare(preview.motionProgress, pausedProgress);

        toggle.clicked();
        compare(preview.motionPaused, false);
        compare(preview.motionRunning, true);
        compare(preview.motionStatus, "playing");
        compare(toggle.text, "Pause motion");
        verify(preview.motionPhase !== "off");
        verify(String(summary.text).includes("Motion playing"));
    }

    function test_appearanceMotionStoriesUseDistinctGeometry() {
        const testWindow = createTemporaryObject(
            appearancePreviewComponent,
            this
        );
        verify(testWindow !== null);
        const preview = testWindow.preview;
        waitForRendering(preview);
        wait(0);

        const stage = findChild(preview, "appearancePreviewStage");
        const active = findChild(
            preview,
            "appearancePreviewActiveWindow"
        );
        const secondary = findChild(
            preview,
            "appearancePreviewSecondaryWindow"
        );
        const spawned = findChild(
            preview,
            "appearancePreviewSpawnedWindow"
        );
        verify(stage !== null);
        verify(active !== null);
        verify(secondary !== null);
        verify(spawned !== null);

        preview.motionPaused = true;
        const layouts = [
            { mode: "dwindle", story: "dwindle-split" },
            { mode: "master", story: "master-stack" },
            { mode: "scrolling", story: "scrolling-strip" },
            { mode: "monocle", story: "monocle-replace" }
        ];
        const stories = [];

        for (const layout of layouts) {
            preview.layoutMode = layout.mode;
            preview.synchronizeMotion(true);
            wait(0);

            compare(preview.motionRunning, false);
            compare(preview.motionPhase, "resting");
            compare(preview.motionProgress, 0);
            compare(preview.motionStory, layout.story);
            compare(stories.indexOf(preview.motionStory), -1);
            stories.push(preview.motionStory);

            const before = {
                activeX: active.x,
                activeY: active.y,
                activeWidth: active.width,
                activeHeight: active.height,
                activeOpacity: active.opacity,
                secondaryX: secondary.x,
                secondaryY: secondary.y,
                secondaryWidth: secondary.width,
                secondaryHeight: secondary.height,
                spawnedX: spawned.x,
                spawnedY: spawned.y,
                spawnedWidth: spawned.width,
                spawnedHeight: spawned.height,
                spawnedOpacity: spawned.opacity
            };

            preview.motionProgress = 1;
            wait(0);
            const after = {
                activeX: active.x,
                activeY: active.y,
                activeWidth: active.width,
                activeHeight: active.height,
                activeOpacity: active.opacity,
                secondaryX: secondary.x,
                secondaryY: secondary.y,
                secondaryWidth: secondary.width,
                secondaryHeight: secondary.height,
                spawnedX: spawned.x,
                spawnedY: spawned.y,
                spawnedWidth: spawned.width,
                spawnedHeight: spawned.height,
                spawnedOpacity: spawned.opacity,
                spawnedScale: spawned.scale
            };

            compare(before.spawnedOpacity, 0);
            compare(after.spawnedOpacity, 1);
            const tolerance = 0.01;

            if (layout.mode === "dwindle") {
                compare(secondary.visible, false);
                verify(Math.abs(
                    stage.dwindleAreaLeft - stage.width * 0.07
                ) <= tolerance);
                verify(Math.abs(
                    stage.dwindleAreaRight - stage.width * 0.93
                ) <= tolerance);
                compare(
                    stage.dwindleGap,
                    Math.max(8, Math.round(stage.width * 0.02))
                );
                verify(Math.abs(
                    before.activeX - stage.dwindleAreaLeft
                ) <= tolerance);
                verify(Math.abs(
                    before.activeWidth - stage.dwindleAreaWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.activeX + before.activeWidth
                        - stage.dwindleAreaRight
                ) <= tolerance);
                verify(Math.abs(
                    before.spawnedX - before.activeX
                        - before.activeWidth - stage.dwindleGap
                ) <= tolerance);

                verify(Math.abs(
                    after.activeX - before.activeX
                ) <= tolerance);
                verify(Math.abs(
                    after.activeY - before.activeY
                ) <= tolerance);
                verify(Math.abs(
                    after.activeHeight - before.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.activeWidth - stage.dwindleTileWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedWidth - stage.dwindleTileWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.activeWidth - after.spawnedWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedY - after.activeY
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedHeight - after.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX - after.activeX
                        - after.activeWidth - stage.dwindleGap
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX + after.spawnedWidth
                        - stage.dwindleAreaRight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX + after.spawnedWidth
                        - before.activeX - before.activeWidth
                ) <= tolerance);
            } else if (layout.mode === "master") {
                compare(secondary.visible, true);
                verify(Math.abs(
                    stage.masterWidth
                        - (stage.windowAreaWidth - stage.windowGap) * 0.55
                ) <= tolerance);
                verify(Math.abs(
                    stage.masterStackWidth
                        - (stage.windowAreaWidth - stage.windowGap) * 0.45
                ) <= tolerance);

                verify(Math.abs(
                    before.activeX - stage.windowAreaLeft
                ) <= tolerance);
                verify(Math.abs(
                    before.activeY - stage.windowAreaTop
                ) <= tolerance);
                verify(Math.abs(
                    before.activeWidth - stage.masterWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.activeHeight - stage.windowAreaHeight
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryX - before.activeX
                        - before.activeWidth - stage.windowGap
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryY - stage.windowAreaTop
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryWidth - stage.masterStackWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryHeight - stage.windowAreaHeight
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryX + before.secondaryWidth
                        - stage.windowAreaRight
                ) <= tolerance);

                verify(Math.abs(
                    after.activeX - before.activeX
                ) <= tolerance);
                verify(Math.abs(
                    after.activeY - before.activeY
                ) <= tolerance);
                verify(Math.abs(
                    after.activeWidth - before.activeWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.activeHeight - before.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryX - before.secondaryX
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryY - before.secondaryY
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryWidth - before.secondaryWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX - after.secondaryX
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedWidth - after.secondaryWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryHeight - stage.masterStackTileHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedHeight - after.secondaryHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedY - after.secondaryY
                        - after.secondaryHeight - stage.windowGap
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedY + after.spawnedHeight
                        - before.secondaryY - before.secondaryHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX + after.spawnedWidth
                        - before.secondaryX - before.secondaryWidth
                ) <= tolerance);
            } else if (layout.mode === "scrolling") {
                compare(secondary.visible, true);
                verify(Math.abs(
                    stage.scrollingTravel
                        - stage.halfWindowWidth - stage.windowGap
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryX - before.activeX
                        - stage.scrollingTravel
                ) <= tolerance);
                verify(Math.abs(
                    before.spawnedX - before.secondaryX
                        - stage.scrollingTravel
                ) <= tolerance);
                verify(Math.abs(
                    before.activeWidth - stage.halfWindowWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryWidth - before.activeWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.spawnedWidth - before.activeWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.activeY - stage.scrollingAreaTop
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryY - before.activeY
                ) <= tolerance);
                verify(Math.abs(
                    before.spawnedY - before.activeY
                ) <= tolerance);
                verify(Math.abs(
                    before.activeHeight - stage.scrollingAreaHeight
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryHeight - before.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    before.spawnedHeight - before.activeHeight
                ) <= tolerance);

                verify(Math.abs(
                    after.activeX
                        - (before.activeX - stage.scrollingTravel)
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryX - before.activeX
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX - before.secondaryX
                ) <= tolerance);
                verify(Math.abs(
                    after.activeWidth - stage.halfWindowWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryWidth - after.activeWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedWidth - after.activeWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryY - after.activeY
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedY - after.activeY
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryHeight - after.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedHeight - after.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX + after.spawnedWidth
                        - stage.windowAreaRight
                ) <= tolerance);
                verify(after.spawnedX >= stage.windowAreaLeft - tolerance);
                verify(after.spawnedX + after.spawnedWidth
                    <= stage.windowAreaRight + tolerance);
                compare(after.activeOpacity, 0);
            } else {
                compare(secondary.visible, false);
                verify(Math.abs(
                    before.activeX - before.spawnedX
                ) <= tolerance);
                verify(Math.abs(
                    before.activeY - before.spawnedY
                ) <= tolerance);
                verify(Math.abs(
                    before.activeWidth - before.spawnedWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.activeHeight - before.spawnedHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.activeX - after.spawnedX
                ) <= tolerance);
                verify(Math.abs(
                    after.activeY - after.spawnedY
                ) <= tolerance);
                verify(Math.abs(
                    after.activeWidth - after.spawnedWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.activeHeight - after.spawnedHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedScale - 1
                ) <= tolerance);
                compare(after.activeOpacity, 0);
                compare(after.spawnedOpacity, 1);
            }
        }

        compare(stories.length, 4);
    }

    function test_appearanceMotionResetsAcrossStateAndLifecycleChanges() {
        const testWindow = createTemporaryObject(
            appearancePreviewComponent,
            this
        );
        verify(testWindow !== null);
        const preview = testWindow.preview;
        waitForRendering(preview);
        wait(0);

        const toggle = findChild(
            preview,
            "toggleAppearanceMotionButton"
        );
        const summary = findChild(preview, "appearancePreviewSummary");
        verify(toggle !== null);
        verify(summary !== null);
        compare(preview.motionRunning, true);

        preview.motionProgress = 0.72;
        preview.motionPhase = "settled";
        preview.animationsEnabled = false;
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "off");
        compare(preview.motionStatus, "off");
        compare(toggle.enabled, false);
        compare(toggle.text, "Motion off");
        compare(
            toggle.Accessible.name,
            "Illustrative window motion is off"
        );
        verify(String(summary.text).includes("Animations off"));
        verify(String(summary.text).includes("Motion off"));

        preview.synchronizeMotion(false);
        wait(0);
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "off");

        preview.animationsEnabled = true;
        compare(preview.motionRunning, true);
        verify(preview.motionPhase !== "off");

        toggle.clicked();
        compare(preview.motionPaused, true);
        compare(preview.motionRunning, false);
        preview.motionProgress = 0.64;
        preview.motionPhase = "closing";
        preview.layoutMode = "master";
        compare(preview.motionStory, "master-stack");
        compare(preview.motionPaused, true);
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "resting");

        toggle.clicked();
        compare(preview.motionPaused, false);
        compare(preview.motionRunning, true);
        verify(preview.motionPhase !== "closing");

        preview.visible = false;
        compare(preview.motionPaused, false);
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "resting");

        preview.visible = true;
        compare(preview.motionPaused, false);
        compare(preview.motionRunning, true);
        verify(preview.motionPhase !== "off");

        toggle.clicked();
        compare(preview.motionPaused, true);
        preview.motionProgress = 0.48;
        preview.motionPhase = "settled";
        preview.visible = false;
        compare(preview.motionPaused, true);
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "resting");

        preview.visible = true;
        compare(preview.motionPaused, true);
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "resting");
        compare(toggle.text, "Play motion");
        verify(String(summary.text).includes("Motion paused"));
    }

    function test_appearanceCatalogMismatchFailsClosed() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const definitions = appearanceDefinitions();
        definitions[14].max = 2;
        page.appearanceOptions = definitions;
        wait(0);

        compare(page.catalogAvailable, true);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);
        compare(findChild(page, "appearanceBorderSize").enabled, false);
        compare(findChild(page, "saveAppearanceButton").enabled, false);
        const status = findChild(page, "appearanceStatusMessage");
        verify(status !== null);
        compare(status.visible, true);
        verify(String(status.text).includes(
            "trusted compositor appearance contract does not match"
        ));

        const invalidDefinitionCases = [
            { index: 15, key: "type", value: "number" },
            { index: 15, key: "control", value: "slider" },
            { index: 15, key: "defaultValue", value: 9 },
            { index: 15, key: "min", value: 1 },
            { index: 15, key: "max", value: 99 },
            { index: 16, key: "defaultValue", value: 2 },
            { index: 17, key: "defaultValue", value: false },
            { index: 19, key: "defaultValue", value: true },
            { index: 22, key: "defaultValue", value: 0.3 },
            { index: 22, key: "max", value: 0.9 },
            { index: 24, key: "type", value: "integer" },
            { index: 25, key: "type", value: "integer" },
            { index: 25, key: "control", value: "text" },
            { index: 25, key: "defaultValue", value: 0.9 },
            { index: 25, key: "min", value: 0.1 },
            { index: 25, key: "max", value: 1 },
            { index: 26, key: "defaultValue", value: 0.8917 },
            { index: 27, key: "defaultValue", value: 0.0118 },
            { index: 28, key: "defaultValue", value: 0.1697 },
            { index: 29, key: "defaultValue", value: 0.1 },
            { index: 30, key: "type", value: "integer" },
            { index: 30, key: "control", value: "spinBox" },
            { index: 30, key: "defaultValue", value: false },
            { index: 31, key: "type", value: "integer" },
            { index: 31, key: "control", value: "text" },
            { index: 31, key: "defaultValue", value: 3 },
            { index: 31, key: "min", value: 1 },
            { index: 31, key: "max", value: 11 },
            { index: 31, key: "step", value: 0.1 },
            { index: 31, key: "choices", value: [2, 3] },
            { index: 32, key: "type", value: "number" },
            { index: 32, key: "control", value: "slider" },
            { index: 32, key: "defaultValue", value: 5 },
            { index: 32, key: "min", value: 1 },
            { index: 32, key: "max", value: 99 },
            { index: 33, key: "type", value: "number" },
            { index: 33, key: "control", value: "slider" },
            { index: 33, key: "defaultValue", value: 2 },
            { index: 33, key: "min", value: 0 },
            { index: 33, key: "max", value: 5 },
            { index: 34, key: "type", value: "integer" },
            { index: 34, key: "control", value: "spinBox" },
            { index: 34, key: "defaultValue", value: true },
            { index: 35, key: "type", value: "number" },
            { index: 35, key: "control", value: "text" },
            { index: 35, key: "defaultValue", value: [1, 0] },
            { index: 35, key: "min", value: [-249, -250] },
            { index: 35, key: "max", value: [250, 249] },
            { index: 35, key: "step", value: 1 },
            { index: 35, key: "choices", value: [[0, 0]] },
            { index: 36, key: "type", value: "integer" },
            { index: 36, key: "control", value: "spinBox" },
            { index: 36, key: "defaultValue", value: 0.9 },
            { index: 36, key: "min", value: 0.1 },
            { index: 36, key: "max", value: 0.9 },
            { index: 36, key: "step", value: 0.1 },
            { index: 36, key: "choices", value: [0, 1] },
            { index: 37, key: "type", value: "integer" },
            { index: 37, key: "control", value: "spinBox" },
            { index: 37, key: "defaultValue", value: true },
            { index: 38, key: "type", value: "number" },
            { index: 38, key: "control", value: "slider" },
            { index: 38, key: "defaultValue", value: 9 },
            { index: 38, key: "min", value: 1 },
            { index: 38, key: "max", value: 99 },
            { index: 39, key: "type", value: "number" },
            { index: 39, key: "control", value: "slider" },
            { index: 39, key: "defaultValue", value: 2 },
            { index: 39, key: "min", value: 0 },
            { index: 39, key: "max", value: 5 }
        ];
        for (const row of invalidDefinitionCases) {
            const mismatched = appearanceDefinitions();
            mismatched[row.index][row.key] = row.value;
            page.appearanceOptions = mismatched;
            wait(0);
            compare(
                page.trustedDefinitionsValid,
                false,
                "Definition mismatch accepted at index " + row.index
                    + " for " + row.key
            );
        }

        const reordered = appearanceDefinitions();
        const firstBlurDetail = reordered[15];
        reordered[15] = reordered[16];
        reordered[16] = firstBlurDetail;
        page.appearanceOptions = reordered;
        wait(0);
        compare(page.trustedDefinitionsValid, false);

        const reorderedShadowScale = appearanceDefinitions();
        const shadowScale = reorderedShadowScale[36];
        reorderedShadowScale[36] = reorderedShadowScale[35];
        reorderedShadowScale[35] = shadowScale;
        page.appearanceOptions = reorderedShadowScale;
        wait(0);
        compare(page.trustedDefinitionsValid, false);

        const reorderedShadowOffset = appearanceDefinitions();
        const shadowOffset = reorderedShadowOffset[35];
        reorderedShadowOffset[35] = reorderedShadowOffset[34];
        reorderedShadowOffset[34] = shadowOffset;
        page.appearanceOptions = reorderedShadowOffset;
        wait(0);
        compare(page.trustedDefinitionsValid, false);

        const reorderedModulation = appearanceDefinitions();
        const brightness = reorderedModulation[25];
        reorderedModulation[25] = reorderedModulation[26];
        reorderedModulation[26] = brightness;
        page.appearanceOptions = reorderedModulation;
        wait(0);
        compare(page.trustedDefinitionsValid, false);

        const reorderedShadowBounds = appearanceDefinitions();
        const borderPartOfWindow = reorderedShadowBounds[30];
        reorderedShadowBounds[30] = reorderedShadowBounds[29];
        reorderedShadowBounds[29] = borderPartOfWindow;
        page.appearanceOptions = reorderedShadowBounds;
        wait(0);
        compare(page.trustedDefinitionsValid, false);

        const reorderedRoundingPower = appearanceDefinitions();
        const roundingPower = reorderedRoundingPower[31];
        reorderedRoundingPower[31] = reorderedRoundingPower[30];
        reorderedRoundingPower[30] = roundingPower;
        page.appearanceOptions = reorderedRoundingPower;
        wait(0);
        compare(page.trustedDefinitionsValid, false);

        const reorderedShadowRendering = appearanceDefinitions();
        const shadowRange = reorderedShadowRendering[32];
        reorderedShadowRendering[32] = reorderedShadowRendering[33];
        reorderedShadowRendering[33] = shadowRange;
        page.appearanceOptions = reorderedShadowRendering;
        wait(0);
        compare(page.trustedDefinitionsValid, false);

        const reorderedGlow = appearanceDefinitions();
        const glowRange = reorderedGlow[38];
        reorderedGlow[38] = reorderedGlow[39];
        reorderedGlow[39] = glowRange;
        page.appearanceOptions = reorderedGlow;
        wait(0);
        compare(page.trustedDefinitionsValid, false);

        page.appearanceOptions = appearanceDefinitions();
        compare(page.trustedDefinitionsValid, true);
        const invalidValueCases = [
            { id: page.blurSizeId, value: 8.5 },
            { id: page.blurSizeId, value: -1 },
            { id: page.blurSizeId, value: 101 },
            { id: page.blurPassesId, value: 1.5 },
            { id: page.blurPassesId, value: -1 },
            { id: page.blurPassesId, value: 11 },
            { id: page.blurIgnoreOpacityId, value: 1 },
            { id: page.blurOptimizationsId, value: "true" },
            { id: page.blurXrayId, value: 0 },
            { id: page.blurSpecialId, value: null },
            { id: page.blurPopupsId, value: "false" },
            { id: page.blurPopupsIgnoreAlphaId, value: -0.01 },
            { id: page.blurPopupsIgnoreAlphaId, value: 1.01 },
            { id: page.blurPopupsIgnoreAlphaId, value: "0.2" },
            { id: page.blurInputMethodsId, value: 1 },
            { id: page.blurInputMethodsIgnoreAlphaId, value: -0.01 },
            { id: page.blurInputMethodsIgnoreAlphaId, value: 1.01 },
            { id: page.blurInputMethodsIgnoreAlphaId, value: "0.2" },
            { id: page.blurBrightnessId, value: -0.01 },
            { id: page.blurBrightnessId, value: 2.01 },
            { id: page.blurBrightnessId, value: "1" },
            { id: page.blurContrastId, value: -0.01 },
            { id: page.blurContrastId, value: 2.01 },
            { id: page.blurNoiseId, value: 1.01 },
            { id: page.blurVibrancyId, value: -0.01 },
            { id: page.blurVibrancyDarknessId, value: "0" },
            { id: page.borderPartOfWindowId, value: 1 },
            { id: page.roundingPowerId, value: 1.999 },
            { id: page.roundingPowerId, value: 10.001 },
            { id: page.roundingPowerId, value: "2.5" },
            { id: page.roundingPowerId, value: true },
            { id: page.roundingPowerId, value: NaN },
            { id: page.roundingPowerId, value: Infinity },
            { id: page.roundingPowerId, value: -Infinity },
            { id: page.shadowRangeId, value: -1 },
            { id: page.shadowRangeId, value: 101 },
            { id: page.shadowRangeId, value: 4.5 },
            { id: page.shadowRangeId, value: "4" },
            { id: page.shadowRenderPowerId, value: 0 },
            { id: page.shadowRenderPowerId, value: 5 },
            { id: page.shadowRenderPowerId, value: 2.5 },
            { id: page.shadowRenderPowerId, value: "3" },
            { id: page.shadowSharpId, value: 0 },
            { id: page.shadowOffsetId, value: [] },
            { id: page.shadowOffsetId, value: [0] },
            { id: page.shadowOffsetId, value: [0, 0, 0] },
            { id: page.shadowOffsetId, value: ["0", 0] },
            { id: page.shadowOffsetId, value: [true, 0] },
            { id: page.shadowOffsetId, value: [NaN, 0] },
            { id: page.shadowOffsetId, value: [Infinity, 0] },
            { id: page.shadowOffsetId, value: [-Infinity, 0] },
            { id: page.shadowOffsetId, value: [-250.001, 0] },
            { id: page.shadowOffsetId, value: [250.001, 0] },
            { id: page.shadowOffsetId, value: [0, -250.001] },
            { id: page.shadowOffsetId, value: [0, 250.001] },
            { id: page.shadowOffsetId, value: 0 },
            { id: page.shadowOffsetId, value: {} },
            { id: page.shadowScaleId, value: -0.001 },
            { id: page.shadowScaleId, value: 1.001 },
            { id: page.shadowScaleId, value: "1" },
            { id: page.shadowScaleId, value: true },
            { id: page.shadowScaleId, value: NaN },
            { id: page.shadowScaleId, value: Infinity },
            { id: page.shadowScaleId, value: -Infinity },
            { id: page.glowEnabledId, value: 1 },
            { id: page.glowRangeId, value: -1 },
            { id: page.glowRangeId, value: 101 },
            { id: page.glowRangeId, value: 9.5 },
            { id: page.glowRangeId, value: "9" },
            { id: page.glowRenderPowerId, value: 0 },
            { id: page.glowRenderPowerId, value: 5 },
            { id: page.glowRenderPowerId, value: 2.5 },
            { id: page.glowRenderPowerId, value: "3" }
        ];
        for (const row of invalidValueCases) {
            const invalidValues = appearanceDefaults();
            invalidValues[row.id] = row.value;
            page.appearanceValues = invalidValues;
            wait(0);
            compare(
                page.trustedValuesValid,
                false,
                "Invalid projected value accepted for " + row.id
            );
            compare(page.controlsEnabled, false);
        }

        const invalidExistingValues = appearanceDefaults();
        invalidExistingValues[
            "hyprland.decoration.fullscreen_opacity"
        ] = 1.01;
        page.appearanceValues = invalidExistingValues;
        wait(0);
        compare(page.trustedValuesValid, false);

        page.appearanceValues = appearanceDefaults();
        page.catalogAvailable = false;
        wait(0);
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("catalog is unavailable"));
    }

    function test_appearanceDraftActionsAreExplicitAndExact() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const baseline = appearanceDefaults();
        baseline["hyprland.general.border_size"] = 3;
        baseline["hyprland.decoration.shadow.enabled"] = false;
        baseline["hyprland.decoration.shadow.range"] = 17;
        baseline["hyprland.decoration.shadow.render_power"] = 4;
        baseline["hyprland.decoration.shadow.sharp"] = true;
        baseline["hyprland.decoration.shadow.offset"] = [12.5, -8.25];
        baseline["hyprland.decoration.shadow.scale"] = 0.75;
        baseline["hyprland.decoration.border_part_of_window"] = false;
        baseline["hyprland.decoration.dim_inactive"] = true;
        baseline["hyprland.decoration.dim_strength"] = 0.65;
        baseline["hyprland.decoration.active_opacity"] = 0.9;
        baseline["hyprland.decoration.inactive_opacity"] = 0.7;
        baseline["hyprland.decoration.fullscreen_opacity"] = 0.8;
        baseline["hyprland.decoration.dim_modal"] = false;
        baseline["hyprland.decoration.dim_special"] = 0.3;
        baseline["hyprland.decoration.dim_around"] = 0.6;
        baseline["hyprland.decoration.blur.size"] = 21;
        baseline["hyprland.decoration.blur.passes"] = 4;
        baseline["hyprland.decoration.blur.ignore_opacity"] = false;
        baseline["hyprland.decoration.blur.new_optimizations"] = false;
        baseline["hyprland.decoration.blur.xray"] = true;
        baseline["hyprland.decoration.blur.special"] = true;
        baseline["hyprland.decoration.blur.popups"] = false;
        baseline["hyprland.decoration.blur.popups_ignorealpha"] = 0.3;
        baseline["hyprland.decoration.blur.input_methods"] = false;
        baseline[
            "hyprland.decoration.blur.input_methods_ignorealpha"
        ] = 0.6;
        baseline["hyprland.decoration.blur.brightness"] = 1.25;
        baseline["hyprland.decoration.blur.contrast"] = 0.75;
        baseline["hyprland.decoration.blur.noise"] = 0.25;
        baseline["hyprland.decoration.blur.vibrancy"] = 0.5;
        baseline["hyprland.decoration.blur.vibrancy_darkness"] = 0.75;
        baseline["hyprland.decoration.rounding_power"] = 7.421;
        configureAppearancePage(page, baseline);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.borderSizeId, 7);
        page.setDraftValue(page.roundingId, 5);
        page.setExactDecimalDraftValue(page.roundingPowerId, 2.573);
        page.setExactDecimalDraftValue(page.blurBrightnessId, 1.234567);
        page.setExactDecimalDraftValue(page.blurContrastId, 0.7654321);
        page.setExactDecimalDraftValue(page.blurNoiseId, 0.222222);
        page.setExactDecimalDraftValue(page.blurVibrancyId, 0.444444);
        page.setExactDecimalDraftValue(
            page.blurVibrancyDarknessId, 0.666666
        );
        page.setDraftValue(page.blurId, false);
        page.setDimStrength(0.37);
        page.setUnitSliderValue(page.activeOpacityId, 0.83);
        page.setUnitSliderValue(page.inactiveOpacityId, 0.52);
        page.setUnitSliderValue(page.fullscreenOpacityId, 0.46);
        page.setDraftValue(page.dimModalId, true);
        page.setUnitSliderValue(page.dimSpecialId, 0.68);
        page.setUnitSliderValue(page.dimAroundId, 0.74);
        page.setDraftValue(page.blurSizeId, 33);
        page.setDraftValue(page.blurPassesId, 5);
        page.setDraftValue(page.blurIgnoreOpacityId, true);
        page.setDraftValue(page.blurOptimizationsId, true);
        page.setDraftValue(page.blurXrayId, false);
        page.setDraftValue(page.blurSpecialId, false);
        page.setDraftValue(page.blurPopupsId, true);
        page.setUnitSliderValue(page.blurPopupsIgnoreAlphaId, 0.68);
        page.setDraftValue(page.blurInputMethodsId, true);
        page.setUnitSliderValue(
            page.blurInputMethodsIgnoreAlphaId, 0.74
        );
        compare(page.draftDirty, true);

        let saveCount = 0;
        let submitted = null;
        let submittedCurves = null;
        let submittedAnimations = null;
        page.saveRequested.connect(function(values, curves, animations) {
            ++saveCount;
            submitted = values;
            submittedCurves = curves;
            submittedAnimations = animations;
        });
        const save = findChild(page, "saveAppearanceButton");
        verify(save !== null);
        compare(save.enabled, true);
        save.clicked();
        compare(saveCount, 1);
        verify(submitted !== null);
        compare(Object.keys(submitted).length, 40);
        compare(submitted[page.borderSizeId], 7);
        compare(submitted[page.roundingId], 5);
        compare(submitted[page.roundingPowerId], 2.573);
        compare(submitted[page.gapsInId], [5, 5, 5, 5]);
        compare(submitted[page.gapsOutId], [20, 20, 20, 20]);
        compare(submitted[page.blurId], false);
        compare(submitted[page.shadowId], false);
        compare(submitted[page.shadowRangeId], 17);
        compare(submitted[page.shadowRenderPowerId], 4);
        compare(submitted[page.shadowSharpId], true);
        compare(submitted[page.shadowOffsetId], [12.5, -8.25]);
        compare(submitted[page.shadowScaleId], 0.75);
        compare(submitted[page.glowEnabledId], false);
        compare(submitted[page.glowRangeId], 10);
        compare(submitted[page.glowRenderPowerId], 3);
        compare(submitted[page.borderPartOfWindowId], false);
        compare(submitted[page.dimInactiveId], true);
        compare(submitted[page.dimStrengthId], 0.35);
        compare(submitted[page.activeOpacityId], 0.85);
        compare(submitted[page.inactiveOpacityId], 0.5);
        compare(submitted[page.fullscreenOpacityId], 0.45);
        compare(submitted[page.dimModalId], true);
        compare(submitted[page.dimSpecialId], 0.7);
        compare(submitted[page.dimAroundId], 0.75);
        compare(submitted[page.blurSizeId], 33);
        compare(submitted[page.blurPassesId], 5);
        compare(submitted[page.blurIgnoreOpacityId], true);
        compare(submitted[page.blurOptimizationsId], true);
        compare(submitted[page.blurXrayId], false);
        compare(submitted[page.blurSpecialId], false);
        compare(submitted[page.blurPopupsId], true);
        compare(submitted[page.blurPopupsIgnoreAlphaId], 0.7);
        compare(submitted[page.blurInputMethodsId], true);
        compare(
            submitted[page.blurInputMethodsIgnoreAlphaId], 0.75
        );
        compare(submitted[page.blurBrightnessId], 1.234567);
        compare(submitted[page.blurContrastId], 0.7654321);
        compare(submitted[page.blurNoiseId], 0.222222);
        compare(submitted[page.blurVibrancyId], 0.444444);
        compare(submitted[page.blurVibrancyDarknessId], 0.666666);
        compare(submittedCurves, []);
        compare(submittedAnimations, []);

        // A second submission is blocked until the first request resolves.
        save.clicked();
        compare(saveCount, 1);

        // A fresh page proves Discard and catalog-default reset are local.
        const secondWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(secondWindow !== null);
        const secondPage = secondWindow.page;
        configureAppearancePage(secondPage, baseline);
        waitForRendering(secondPage);
        wait(0);
        secondPage.setDraftValue(secondPage.borderSizeId, 9);
        secondPage.setExactDecimalDraftValue(
            secondPage.roundingPowerId, 3.257
        );
        secondPage.setDimStrength(0.75);
        secondPage.setUnitSliderValue(secondPage.activeOpacityId, 0.4);
        secondPage.setDraftValue(secondPage.dimModalId, true);
        secondPage.setUnitSliderValue(secondPage.dimSpecialId, 0.9);
        secondPage.setUnitSliderValue(secondPage.dimAroundId, 0.1);
        secondPage.setDraftValue(secondPage.blurSizeId, 90);
        secondPage.setDraftValue(secondPage.blurPassesId, 9);
        secondPage.setDraftValue(
            secondPage.blurOptimizationsId, true
        );
        secondPage.setDraftValue(secondPage.blurPopupsId, true);
        secondPage.setUnitSliderValue(
            secondPage.blurPopupsIgnoreAlphaId, 0.85
        );
        secondPage.setDraftValue(
            secondPage.blurInputMethodsId, true
        );
        secondPage.setUnitSliderValue(
            secondPage.blurInputMethodsIgnoreAlphaId, 0.15
        );
        secondPage.setExactDecimalDraftValue(
            secondPage.blurBrightnessId, 0.125
        );
        secondPage.setExactDecimalDraftValue(
            secondPage.blurContrastId, 1.125
        );
        secondPage.setExactDecimalDraftValue(
            secondPage.blurNoiseId, 0.125
        );
        secondPage.setExactDecimalDraftValue(
            secondPage.blurVibrancyId, 0.625
        );
        secondPage.setExactDecimalDraftValue(
            secondPage.blurVibrancyDarknessId, 0.375
        );
        compare(secondPage.draftDirty, true);
        findChild(secondPage, "discardAppearanceDraftButton").clicked();
        compare(secondPage.draftDirty, false);
        compare(secondPage.draftValue(secondPage.borderSizeId), 3);
        compare(
            secondPage.draftValue(secondPage.roundingPowerId), 7.421
        );
        compare(secondPage.draftValue(secondPage.dimInactiveId), true);
        compare(secondPage.draftValue(secondPage.dimStrengthId), 0.65);
        compare(secondPage.draftValue(secondPage.activeOpacityId), 0.9);
        compare(secondPage.draftValue(secondPage.inactiveOpacityId), 0.7);
        compare(secondPage.draftValue(secondPage.fullscreenOpacityId), 0.8);
        compare(secondPage.draftValue(secondPage.dimModalId), false);
        compare(secondPage.draftValue(secondPage.dimSpecialId), 0.3);
        compare(secondPage.draftValue(secondPage.dimAroundId), 0.6);
        compare(secondPage.draftValue(secondPage.blurSizeId), 21);
        compare(secondPage.draftValue(secondPage.blurPassesId), 4);
        compare(
            secondPage.draftValue(secondPage.blurIgnoreOpacityId), false
        );
        compare(
            secondPage.draftValue(secondPage.blurOptimizationsId), false
        );
        compare(secondPage.draftValue(secondPage.blurXrayId), true);
        compare(secondPage.draftValue(secondPage.blurSpecialId), true);
        compare(secondPage.draftValue(secondPage.blurPopupsId), false);
        compare(
            secondPage.draftValue(secondPage.blurPopupsIgnoreAlphaId),
            0.3
        );
        compare(
            secondPage.draftValue(secondPage.blurInputMethodsId), false
        );
        compare(
            secondPage.draftValue(
                secondPage.blurInputMethodsIgnoreAlphaId
            ),
            0.6
        );
        compare(secondPage.draftValue(secondPage.blurBrightnessId), 1.25);
        compare(secondPage.draftValue(secondPage.blurContrastId), 0.75);
        compare(secondPage.draftValue(secondPage.blurNoiseId), 0.25);
        compare(secondPage.draftValue(secondPage.blurVibrancyId), 0.5);
        compare(
            secondPage.draftValue(secondPage.blurVibrancyDarknessId),
            0.75
        );
        compare(
            secondPage.draftValue(secondPage.borderPartOfWindowId),
            false
        );
        compare(secondPage.draftValue(secondPage.shadowRangeId), 17);
        compare(
            secondPage.draftValue(secondPage.shadowRenderPowerId), 4
        );
        compare(secondPage.draftValue(secondPage.shadowSharpId), true);
        compare(
            secondPage.draftValue(secondPage.shadowOffsetId),
            [12.5, -8.25]
        );
        compare(secondPage.draftValue(secondPage.shadowScaleId), 0.75);

        findChild(secondPage, "resetAppearanceDefaultsButton").clicked();
        compare(secondPage.draftDirty, true);
        compare(secondPage.draftValue(secondPage.borderSizeId), 1);
        compare(secondPage.draftValue(secondPage.roundingPowerId), 2);
        compare(secondPage.draftValue(secondPage.gapsInId), [5, 5, 5, 5]);
        compare(
            secondPage.draftValue(secondPage.gapsOutId),
            [20, 20, 20, 20]
        );
        compare(secondPage.draftValue(secondPage.shadowId), true);
        compare(secondPage.draftValue(secondPage.shadowRangeId), 4);
        compare(
            secondPage.draftValue(secondPage.shadowRenderPowerId), 3
        );
        compare(secondPage.draftValue(secondPage.shadowSharpId), false);
        compare(secondPage.draftValue(secondPage.shadowOffsetId), [0, 0]);
        compare(secondPage.draftValue(secondPage.shadowScaleId), 1);
        compare(secondPage.draftValue(secondPage.glowEnabledId), false);
        compare(secondPage.draftValue(secondPage.glowRangeId), 10);
        compare(secondPage.draftValue(secondPage.glowRenderPowerId), 3);
        compare(secondPage.draftValue(secondPage.blurId), true);
        compare(secondPage.draftValue(secondPage.dimInactiveId), false);
        compare(secondPage.draftValue(secondPage.dimStrengthId), 0.5);
        compare(secondPage.draftValue(secondPage.activeOpacityId), 1);
        compare(secondPage.draftValue(secondPage.inactiveOpacityId), 1);
        compare(secondPage.draftValue(secondPage.fullscreenOpacityId), 1);
        compare(secondPage.draftValue(secondPage.dimModalId), true);
        compare(secondPage.draftValue(secondPage.dimSpecialId), 0.2);
        compare(secondPage.draftValue(secondPage.dimAroundId), 0.4);
        compare(secondPage.draftValue(secondPage.blurSizeId), 8);
        compare(secondPage.draftValue(secondPage.blurPassesId), 1);
        compare(
            secondPage.draftValue(secondPage.blurIgnoreOpacityId), true
        );
        compare(
            secondPage.draftValue(secondPage.blurOptimizationsId), true
        );
        compare(secondPage.draftValue(secondPage.blurXrayId), false);
        compare(secondPage.draftValue(secondPage.blurSpecialId), false);
        compare(secondPage.draftValue(secondPage.blurPopupsId), false);
        compare(
            secondPage.draftValue(secondPage.blurPopupsIgnoreAlphaId),
            0.2
        );
        compare(
            secondPage.draftValue(secondPage.blurInputMethodsId), false
        );
        compare(
            secondPage.draftValue(
                secondPage.blurInputMethodsIgnoreAlphaId
            ),
            0.2
        );
        compare(secondPage.draftValue(secondPage.blurBrightnessId), 1);
        compare(secondPage.draftValue(secondPage.blurContrastId), 0.8916);
        compare(secondPage.draftValue(secondPage.blurNoiseId), 0.0117);
        compare(secondPage.draftValue(secondPage.blurVibrancyId), 0.1696);
        compare(
            secondPage.draftValue(secondPage.blurVibrancyDarknessId),
            0
        );
        compare(
            secondPage.draftValue(secondPage.borderPartOfWindowId),
            true
        );
        compare(findChild(
            secondPage, "appearanceDimStrength"
        ).enabled, false);
        compare(secondPage.draftCurves, secondPage.synchronizedCurves);
        compare(secondPage.draftAnimations, []);
    }

    function test_appearanceCurvesAndAnimationRulesStayOneSafeDraft() {
        const testWindow = createTemporaryObject(
            appearancePageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const curves = appearanceCurveFixtures();
        const animations = appearanceAnimationFixtures();
        configureAppearancePage(
            page, undefined, false, false, curves, animations
        );
        waitForRendering(page);
        wait(0);

        const tabBar = findChild(page, "appearanceTabBar");
        const visualsTab = findChild(page, "appearanceVisualsTab");
        const animationsTab = findChild(page, "appearanceAnimationsTab");
        verify(tabBar !== null);
        verify(visualsTab !== null);
        verify(animationsTab !== null);
        verify(visualsTab.implicitHeight >= 44);
        verify(animationsTab.implicitHeight >= 44);
        tabBar.currentIndex = 1;
        wait(0);
        compare(page.appearanceTabIndex, 1);
        compare(findChild(page, "addAnimationCurveButton"), null);
        compare(findChild(page, "removeAnimationCurveButton"), null);
        verify(findChild(page, "animationCurvesCard") !== null);
        verify(findChild(page, "animationRulesCard") !== null);

        const choices = page.curveChoices();
        compare(choices.length, 2);
        compare(choices[0].value, "default");
        verify(String(choices[0].label).includes("custom override"));
        compare(choices[1].value, "linear");
        verify(String(choices[1].label).includes("custom override"));

        const moveCurveDown = findChild(
            page, "moveAnimationCurveDownButton0"
        );
        verify(moveCurveDown !== null);
        verify(moveCurveDown.implicitHeight >= 44);
        compare(moveCurveDown.enabled, true);
        moveCurveDown.clicked();
        compare(page.draftCurves[0].name, "linear");
        compare(page.draftCurves[1].name, "default");

        page.openCurve("curve-default");
        wait(0);
        const name = findChild(page, "animationCurveName");
        const type = findChild(page, "animationCurveType");
        const shadow = findChild(
            page, "animationCurveBuiltinShadowMessage"
        );
        verify(name !== null);
        verify(type !== null);
        verify(shadow !== null);
        compare(name.readOnly, true);
        compare(type.readOnly, true);
        compare(name.text, "default");
        compare(type.text, "Bezier");
        compare(shadow.visible, true);
        verify(String(shadow.text).includes("authored Bezier type"));
        page.setCurvePoint("curve-default", 0, 0, 0.317);
        compare(page.curveById("curve-default").points[0][0], 0.317);
        page.setCurveProperty("curve-default", "name", "renamed");
        compare(page.curveById("curve-default").name, "default");
        findChild(page, "closeAnimationCurveEditorButton").clicked();

        const master = findChild(page, "appearanceAnimationsEnabled");
        verify(master !== null);
        page.setDraftValue(page.animationsId, false);
        compare(page.draftValue(page.animationsId), false);
        compare(findChild(page, "addAnimationRuleButton").enabled, false);
        compare(findChild(page, "editAnimationCurveButton0").enabled, true);
        compare(page.draftCurves.length, 2);
        compare(page.draftAnimations.length, 1);
        page.setDraftValue(page.animationsId, true);

        const addRule = findChild(page, "addAnimationRuleButton");
        verify(addRule !== null);
        verify(addRule.implicitHeight >= 44);
        addRule.clicked();
        compare(page.draftAnimations.length, 2);
        const addedId = page.editingAnimationId;
        verify(addedId.length > 0);
        compare(page.animationById(addedId).name, "global");
        page.setAnimationProperty(addedId, "speed", 2.573);
        page.setAnimationProperty(addedId, "curve", "linear");
        page.setAnimationProperty(addedId, "enabled", false);
        compare(page.animationById(addedId).speed, 2.573);
        compare(page.animationById(addedId).curve, "linear");
        compare(page.animationById(addedId).enabled, false);
        page.setAnimationProperty(addedId, "name", "windows");
        compare(page.draftValid, false);
        compare(findChild(page, "saveAppearanceButton").enabled, false);
        verify(String(page.animationIssue(page.animationById(addedId))).includes(
            "only once"
        ));
        page.setAnimationProperty(addedId, "name", "fade");
        compare(page.draftValid, true);
        page.removeAnimation("animation-windows");
        compare(page.draftAnimations.length, 1);

        let submittedValues = null;
        let submittedCurves = null;
        let submittedAnimations = null;
        page.saveRequested.connect(function(values, savedCurves, savedRules) {
            submittedValues = values;
            submittedCurves = savedCurves;
            submittedAnimations = savedRules;
        });
        const save = findChild(page, "saveAppearanceButton");
        compare(save.enabled, true);
        save.clicked();
        verify(submittedValues !== null);
        compare(submittedCurves, page.draftCurves);
        compare(submittedAnimations, page.draftAnimations);

        const resetWindow = createTemporaryObject(
            appearancePageComponent, this
        );
        verify(resetWindow !== null);
        const resetPage = resetWindow.page;
        configureAppearancePage(
            resetPage, undefined, false, false, curves, animations
        );
        waitForRendering(resetPage);
        wait(0);
        resetPage.setDraftValue(resetPage.blurId, false);
        findChild(resetPage, "resetAppearanceDefaultsButton").clicked();
        compare(resetPage.draftCurves, curves);
        compare(resetPage.draftAnimations, []);

        resetPage.setCurvePoint("curve-default", 0, 0, 0.413);
        const retained = resetPage.curveById("curve-default").points[0][0];
        const discard = findChild(
            resetPage, "discardAppearanceDraftButton"
        );
        compare(discard.visible, true);
        resetPage.appearanceAnimationProjectionAvailable = false;
        wait(0);
        compare(discard.enabled, false);
        discard.clicked();
        compare(resetPage.curveById("curve-default").points[0][0], retained);
        resetPage.appearanceAnimationProjectionAvailable = true;
        wait(0);
        compare(discard.enabled, true);
        discard.clicked();
        compare(resetPage.draftCurves, curves);
    }

    function test_unpublishedRestartAndSessionStatesNeverPromiseActivation() {
        const appearanceWindow = createTemporaryObject(
            appearancePageComponent, this
        );
        const inputWindow = createTemporaryObject(inputPageComponent, this);
        const windowsWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        const workspacesWindow = createTemporaryObject(
            workspacesPageComponent, this
        );
        const advancedWindow = createTemporaryObject(
            advancedPageComponent, this
        );
        const rulesWindow = createTemporaryObject(rulesPageComponent, this);
        verify(appearanceWindow !== null);
        verify(inputWindow !== null);
        verify(windowsWindow !== null);
        verify(workspacesWindow !== null);
        verify(advancedWindow !== null);
        verify(rulesWindow !== null);
        configureAppearancePage(appearanceWindow.page);
        configureInputPage(inputWindow.page);
        configureWindowsPage(windowsWindow.page);
        configureWorkspacesPage(workspacesWindow.page);
        configureAdvancedPage(advancedWindow.page);
        configureRulesStatusPage(rulesWindow.page);
        const pages = [
            { page: appearanceWindow.page, status: "appearanceStatusMessage" },
            { page: inputWindow.page, status: "inputStatusMessage" },
            { page: windowsWindow.page, status: "windowsStatusMessage" },
            { page: workspacesWindow.page, status: "workspacesStatusMessage" },
            { page: advancedWindow.page, status: "advancedStatusMessage" },
            { page: rulesWindow.page, status: "rulesStatusMessage" }
        ];
        for (const mode of ["restart", "session"]) {
            for (const entry of pages) {
                entry.page.applyState = "retained";
                entry.page.requiredActivation = mode;
                entry.page.retryApplyAvailable = false;
                entry.page.recoveryAvailable = true;
            }
            wait(0);
            for (const entry of pages) {
                const status = findChild(entry.page, entry.status);
                verify(status !== null);
                const message = String(status.text);
                verify(message.includes(
                    mode === "restart"
                        ? "verified compositor-restart workflow"
                        : "verified new-session workflow"
                ), message);
                verify(message.includes("does not have yet"), message);
                verify(message.includes(
                    "cannot be activated from Settings"
                ), message);
                verify(message.includes(
                    "Restore the last working compositor configuration"
                ), message);
                verify(!message.includes("restart Hyprland manually"), message);
                verify(!message.includes("log out"), message);
            }
        }
    }

    function test_appearanceAnimationStrictValidationAndLeafCap() {
        const testWindow = createTemporaryObject(
            appearancePageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const curves = appearanceCurveFixtures();
        configureAppearancePage(
            page, undefined, false, false, curves, []
        );
        waitForRendering(page);
        wait(0);

        const bezier = {
            id: "curve-boundary",
            name: "boundary",
            type: "bezier",
            points: [[-1, 0], [1, 2]]
        };
        compare(page.validateCurveRecord(bezier, false), true);
        let invalid = page.clone(bezier);
        invalid.points[0][0] = -1.001;
        compare(page.validateCurveRecord(invalid, false), false);
        invalid = page.clone(bezier);
        invalid.points[1][1] = 2.001;
        compare(page.validateCurveRecord(invalid, false), false);
        invalid = page.clone(bezier);
        invalid.points[0][0] = Number.NaN;
        compare(page.validateCurveRecord(invalid, false), false);
        invalid = page.clone(bezier);
        invalid.points = [[0, 0]];
        compare(page.validateCurveRecord(invalid, false), false);

        const spring = {
            id: "curve-spring-boundary",
            name: "spring-boundary",
            type: "spring",
            stiffness: 0.500001,
            dampening: 1000000,
            mass: 1
        };
        compare(page.validateCurveRecord(spring, false), true);
        invalid = page.clone(spring);
        invalid.stiffness = 0.5;
        compare(page.validateCurveRecord(invalid, false), false);
        invalid = page.clone(spring);
        invalid.dampening = 1000000.001;
        compare(page.validateCurveRecord(invalid, false), false);
        invalid = page.clone(spring);
        invalid.mass = Number.POSITIVE_INFINITY;
        compare(page.validateCurveRecord(invalid, false), false);

        function animation(leaf, style) {
            return {
                id: "animation-test",
                name: leaf,
                enabled: true,
                speed: 100,
                curve: "default",
                style: style
            };
        }
        compare(page.validateAnimationRecord(
            animation("windows", "popin 37%"), curves, false
        ), true);
        compare(page.validateAnimationRecord(
            animation("workspaces", "slidefadevert left 37%"),
            curves, false
        ), true);
        compare(page.validateAnimationRecord(
            animation("borderangle", "loop"), curves, false
        ), true);
        compare(page.validateAnimationRecord(
            animation("layers", "popin 37%"), curves, false
        ), true);
        compare(page.validateAnimationRecord(
            animation("fade", "slide"), curves, false
        ), false);
        let rule = animation("windows", "slide");
        rule.speed = 0;
        compare(page.validateAnimationRecord(rule, curves, false), false);
        rule.speed = 100.001;
        compare(page.validateAnimationRecord(rule, curves, false), false);
        rule.speed = Number.NaN;
        compare(page.validateAnimationRecord(rule, curves, false), false);
        rule.speed = "6";
        compare(page.validateAnimationRecord(rule, curves, false), false);
        rule.speed = 6;
        rule.curve = "unknown";
        compare(page.validateAnimationRecord(rule, curves, false), false);

        const duplicateLeaf = animation("windows", "slide");
        const duplicateLeaf2 = page.clone(duplicateLeaf);
        duplicateLeaf2.id = "animation-test-two";
        compare(page.validateAnimationCollection(
            [duplicateLeaf, duplicateLeaf2], curves, false
        ), false);

        const allLeaves = [];
        for (let index = 0; index < page.animationLeaves.length; ++index) {
            allLeaves.push({
                id: "animation-leaf-" + index,
                name: page.animationLeaves[index],
                enabled: true,
                speed: 1,
                curve: "default",
                style: ""
            });
        }
        compare(allLeaves.length, 34);
        compare(page.validateAnimationCollection(
            allLeaves, curves, false
        ), true);
        page.appearanceAnimations = allLeaves;
        page.revisionToken = "8";
        page.reviewProjection();
        wait(0);
        findChild(page, "appearanceTabBar").currentIndex = 1;
        wait(0);
        compare(findChild(page, "addAnimationRuleButton").enabled, false);

        page.appearanceCurves = [{ malformed: true }];
        page.revisionToken = "9";
        page.reviewProjection();
        wait(0);
        compare(page.trustedAnimationProjectionValid, false);
        compare(page.controlsEnabled, false);
        verify(String(findChild(
            page, "appearanceStatusMessage"
        ).text).includes(
            "strict managed compositor appearance contract"
        ));
    }

    function test_appearanceExternalRevisionPreservesDraft() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.borderSizeId, 8);
        page.setDraftValue(page.dimInactiveId, true);
        page.setDimStrength(0.37);
        page.setUnitSliderValue(page.activeOpacityId, 0.57);
        page.setUnitSliderValue(page.inactiveOpacityId, 0.46);
        page.setUnitSliderValue(page.fullscreenOpacityId, 0.34);
        page.setDraftValue(page.dimModalId, false);
        page.setUnitSliderValue(page.dimSpecialId, 0.63);
        page.setUnitSliderValue(page.dimAroundId, 0.78);
        page.setDraftValue(page.blurSizeId, 66);
        page.setDraftValue(page.blurPassesId, 6);
        page.setDraftValue(page.blurIgnoreOpacityId, false);
        page.setDraftValue(page.blurOptimizationsId, false);
        page.setDraftValue(page.blurXrayId, true);
        page.setDraftValue(page.blurSpecialId, true);
        page.setDraftValue(page.blurPopupsId, true);
        page.setUnitSliderValue(page.blurPopupsIgnoreAlphaId, 0.52);
        page.setDraftValue(page.blurInputMethodsId, true);
        page.setUnitSliderValue(
            page.blurInputMethodsIgnoreAlphaId, 0.88
        );
        page.setExactVectorComponentDraftValue(
            page.shadowOffsetId, 0, 125.5
        );
        page.setExactVectorComponentDraftValue(
            page.shadowOffsetId, 1, -80.25
        );
        compare(page.draftDirty, true);
        compare(page.draftValue(page.borderSizeId), 8);
        compare(page.draftValue(page.dimStrengthId), 0.35);
        compare(page.draftValue(page.activeOpacityId), 0.55);
        compare(page.draftValue(page.inactiveOpacityId), 0.45);
        compare(page.draftValue(page.fullscreenOpacityId), 0.35);
        compare(page.draftValue(page.dimModalId), false);
        compare(page.draftValue(page.dimSpecialId), 0.65);
        compare(page.draftValue(page.dimAroundId), 0.8);
        compare(page.draftValue(page.blurSizeId), 66);
        compare(page.draftValue(page.blurPassesId), 6);
        compare(page.draftValue(page.blurIgnoreOpacityId), false);
        compare(page.draftValue(page.blurOptimizationsId), false);
        compare(page.draftValue(page.blurXrayId), true);
        compare(page.draftValue(page.blurSpecialId), true);
        compare(page.draftValue(page.blurPopupsId), true);
        compare(page.draftValue(page.blurPopupsIgnoreAlphaId), 0.5);
        compare(page.draftValue(page.blurInputMethodsId), true);
        compare(
            page.draftValue(page.blurInputMethodsIgnoreAlphaId), 0.9
        );
        compare(page.draftValue(page.shadowOffsetId), [125.5, -80.25]);

        const newerValues = appearanceDefaults();
        newerValues["hyprland.general.border_size"] = 2;
        newerValues["hyprland.decoration.rounding"] = 4;
        newerValues["hyprland.decoration.dim_inactive"] = true;
        newerValues["hyprland.decoration.dim_strength"] = 0.8;
        newerValues["hyprland.decoration.active_opacity"] = 0.95;
        newerValues["hyprland.decoration.inactive_opacity"] = 0.85;
        newerValues["hyprland.decoration.fullscreen_opacity"] = 0.75;
        newerValues["hyprland.decoration.dim_modal"] = true;
        newerValues["hyprland.decoration.dim_special"] = 0.25;
        newerValues["hyprland.decoration.dim_around"] = 0.55;
        newerValues["hyprland.decoration.blur.size"] = 12;
        newerValues["hyprland.decoration.blur.passes"] = 2;
        newerValues["hyprland.decoration.blur.ignore_opacity"] = false;
        newerValues[
            "hyprland.decoration.blur.new_optimizations"
        ] = true;
        newerValues["hyprland.decoration.blur.xray"] = false;
        newerValues["hyprland.decoration.blur.special"] = true;
        newerValues["hyprland.decoration.blur.popups"] = false;
        newerValues[
            "hyprland.decoration.blur.popups_ignorealpha"
        ] = 0.25;
        newerValues["hyprland.decoration.blur.input_methods"] = false;
        newerValues[
            "hyprland.decoration.blur.input_methods_ignorealpha"
        ] = 0.45;
        newerValues[page.shadowOffsetId] = [-10.5, 20.25];
        page.appearanceValues = newerValues;
        page.revisionToken = "8";
        page.appliedRevision = 8;
        wait(0);

        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.borderSizeId), 8);
        compare(page.draftValue(page.dimStrengthId), 0.35);
        compare(page.draftValue(page.activeOpacityId), 0.55);
        compare(page.draftValue(page.inactiveOpacityId), 0.45);
        compare(page.draftValue(page.fullscreenOpacityId), 0.35);
        compare(page.draftValue(page.dimModalId), false);
        compare(page.draftValue(page.dimSpecialId), 0.65);
        compare(page.draftValue(page.dimAroundId), 0.8);
        compare(page.draftValue(page.blurSizeId), 66);
        compare(page.draftValue(page.blurPassesId), 6);
        compare(page.draftValue(page.blurIgnoreOpacityId), false);
        compare(page.draftValue(page.blurOptimizationsId), false);
        compare(page.draftValue(page.blurXrayId), true);
        compare(page.draftValue(page.blurSpecialId), true);
        compare(page.draftValue(page.blurPopupsId), true);
        compare(page.draftValue(page.blurPopupsIgnoreAlphaId), 0.5);
        compare(page.draftValue(page.blurInputMethodsId), true);
        compare(
            page.draftValue(page.blurInputMethodsIgnoreAlphaId), 0.9
        );
        compare(page.draftValue(page.shadowOffsetId), [125.5, -80.25]);
        compare(page.synchronizedRevisionToken, "7");
        compare(findChild(page, "saveAppearanceButton").enabled, false);
        const status = findChild(page, "appearanceStatusMessage");
        verify(String(status.text).includes("draft is preserved"));

        const loadCurrent = findChild(
            page,
            "loadCurrentAppearanceButton"
        );
        verify(loadCurrent !== null);
        compare(loadCurrent.visible, true);
        loadCurrent.clicked();
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.borderSizeId), 2);
        compare(page.draftValue(page.roundingId), 4);
        compare(page.draftValue(page.dimInactiveId), true);
        compare(page.draftValue(page.dimStrengthId), 0.8);
        compare(page.draftValue(page.activeOpacityId), 0.95);
        compare(page.draftValue(page.inactiveOpacityId), 0.85);
        compare(page.draftValue(page.fullscreenOpacityId), 0.75);
        compare(page.draftValue(page.dimModalId), true);
        compare(page.draftValue(page.dimSpecialId), 0.25);
        compare(page.draftValue(page.dimAroundId), 0.55);
        compare(page.draftValue(page.blurSizeId), 12);
        compare(page.draftValue(page.blurPassesId), 2);
        compare(page.draftValue(page.blurIgnoreOpacityId), false);
        compare(page.draftValue(page.blurOptimizationsId), true);
        compare(page.draftValue(page.blurXrayId), false);
        compare(page.draftValue(page.blurSpecialId), true);
        compare(page.draftValue(page.blurPopupsId), false);
        compare(page.draftValue(page.blurPopupsIgnoreAlphaId), 0.25);
        compare(page.draftValue(page.blurInputMethodsId), false);
        compare(
            page.draftValue(page.blurInputMethodsIgnoreAlphaId), 0.45
        );
        compare(page.draftValue(page.shadowOffsetId), [-10.5, 20.25]);
        compare(page.synchronizedRevisionToken, "8");
    }

    function test_appearanceRevisionTokenRemainsExactAboveJsIntegerLimit() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        page.revisionToken = "9007199254740992";
        page.reviewProjection();
        waitForRendering(page);
        wait(0);
        compare(
            page.synchronizedRevisionToken,
            "9007199254740992"
        );

        page.setDraftValue(page.borderSizeId, 8);
        compare(page.draftDirty, true);

        // An unrelated option changes authority N -> N+1 while the thirty-six
        // projected Appearance values remain byte-for-byte equivalent. These
        // adjacent revisions collapse to one JavaScript Number, so conflict
        // detection must use the client's exact canonical string token.
        page.appearanceValues = appearanceDefaults();
        page.revisionToken = "9007199254740993";
        wait(0);

        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.borderSizeId), 8);
        compare(
            page.synchronizedRevisionToken,
            "9007199254740992"
        );
        compare(findChild(page, "saveAppearanceButton").enabled, false);
    }

    function test_appearanceSynchronousRejectionKeepsRetryableDraft() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.borderSizeId, 6);
        let requestCount = 0;
        page.saveRequested.connect(function() {
            ++requestCount;
            // Model a same-turn client authorization failure: no busy state
            // is entered and the authoritative projection does not change.
            page.errorName = "org.hyprshelld.Error.StaleRevision";
            page.errorMessage = "The compositor revision changed.";
        });

        const save = findChild(page, "saveAppearanceButton");
        verify(save !== null);
        save.clicked();
        compare(requestCount, 1);
        wait(0);
        compare(page.saveSubmitted, false);
        compare(page.draftDirty, true);
        compare(page.draftValue(page.borderSizeId), 6);
        compare(save.enabled, true);
    }

    function test_appearanceOwnSavedRevisionReconcilesAfterApplyFailure() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.borderSizeId, 6);
        page.setExactDecimalDraftValue(page.blurContrastId, 0.7777777);
        page.setExactVectorComponentDraftValue(
            page.shadowOffsetId, 0, 125.5
        );
        page.setExactVectorComponentDraftValue(
            page.shadowOffsetId, 1, -80.25
        );
        const submitted = page.clone(page.draftValues);
        let requestCount = 0;
        page.saveRequested.connect(function() { ++requestCount; });
        page.submitDraft();
        compare(requestCount, 1);
        compare(page.saveSubmitted, true);

        page.busyOperation = "appearance-save";
        page.busy = true;
        page.serviceAvailable = false;
        page.appearanceProjectionAvailable = false;
        page.appearanceAvailable = false;
        page.appearanceValues = ({});
        page.revisionToken = "8";
        page.busy = false;
        page.busyOperation = "";
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, true);
        compare(page.draftValue(page.borderSizeId), 6);
        compare(page.draftValue(page.blurContrastId), 0.7777777);
        compare(page.draftValue(page.shadowOffsetId), [125.5, -80.25]);

        page.serviceAvailable = true;
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, true);
        compare(page.draftValue(page.borderSizeId), 6);
        compare(page.draftValue(page.blurContrastId), 0.7777777);
        compare(page.draftValue(page.shadowOffsetId), [125.5, -80.25]);

        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.appearanceValues = submitted;
        page.appearanceProjectionAvailable = true;
        wait(0);
        compare(page.appearanceAvailable, false);
        compare(page.appearanceProjectionAvailable, true);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, false);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.borderSizeId), 6);
        compare(page.draftValue(page.blurContrastId), 0.7777777);
        compare(page.draftValue(page.shadowOffsetId), [125.5, -80.25]);
        compare(page.synchronizedRevisionToken, "8");
        const retry = findChild(page, "retryApplyAppearanceButton");
        verify(retry !== null);
        compare(retry.visible, true);
        compare(retry.enabled, true);
    }

    function test_appearanceRetainedRevisionHasBoundedActions() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        page.appearanceAvailable = false;
        page.appliedRevision = 6;
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.recoveryAvailable = true;
        wait(0);

        let retryCount = 0;
        let recoveryCount = 0;
        page.retryApplyRequested.connect(function() { ++retryCount; });
        page.recoveryRequested.connect(function() { ++recoveryCount; });

        const retry = findChild(page, "retryApplyAppearanceButton");
        const recover = findChild(page, "recoverAppearanceButton");
        const dialog = findChild(page, "appearanceRecoveryDialog");
        const cancel = findChild(page, "cancelAppearanceRecoveryButton");
        const confirm = findChild(page, "confirmAppearanceRecoveryButton");
        const warning = findChild(page, "appearanceRecoveryWarning");
        verify(retry !== null);
        verify(recover !== null);
        verify(dialog !== null);
        verify(cancel !== null);
        verify(confirm !== null);
        verify(warning !== null);
        compare(retry.visible, true);
        compare(recover.visible, true);
        page.busyOperation = "appearance-apply";
        page.busy = true;
        wait(0);
        verify(String(findChild(page, "appearanceStatusMessage").text)
            .includes("Applying and verifying"));
        compare(retry.enabled, false);
        compare(recover.enabled, false);
        page.busy = false;
        page.busyOperation = "";
        wait(0);
        retry.clicked();
        compare(retryCount, 1);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        tryCompare(cancel, "activeFocus", true);
        compare(recoveryCount, 0);
        verify(String(warning.text).includes(
            "not limited to Appearance"
        ));
        verify(String(warning.text).includes("every pending compositor"));
        keyClick(Qt.Key_Escape);
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 0);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        cancel.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 0);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        confirm.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 1);
        compare(confirm.enabled, false);

        // A stale signal cannot bypass the final live eligibility check.
        confirm.clicked();
        compare(recoveryCount, 1);
    }

    function test_appearanceStatusMatrixLocksUnsafeStates() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);
        const status = findChild(page, "appearanceStatusMessage");
        verify(status !== null);

        page.appearanceAvailable = false;
        wait(0);
        compare(status.visible, true);
        verify(String(status.text).includes(
            "waiting for a current, verified compositor baseline"
        ));

        page.managementState = "unmanaged";
        wait(0);
        verify(String(status.text).includes("takeover from Displays"));
        compare(page.controlsEnabled, false);
        const openDisplays = findChild(
            page,
            "appearanceOpenDisplaysButton"
        );
        verify(openDisplays !== null);
        compare(openDisplays.visible, true);
        let routeCount = 0;
        page.openDisplaysRequested.connect(function() { ++routeCount; });
        openDisplays.clicked();
        compare(routeCount, 1);

        page.managementState = "managed";
        page.confirmationState = "awaiting-confirmation";
        wait(0);
        verify(String(status.text).includes("display test is active"));

        page.confirmationState = "idle";
        page.loadState = "recovered";
        page.appearanceAvailable = true;
        wait(0);
        verify(String(status.text).includes("last known good"));
        compare(page.controlsEnabled, true);

        page.loadState = "defaulted";
        wait(0);
        verify(String(status.text).includes("safe desired-state defaults"));

        page.loadState = "normal";
        page.revisionToken = "";
        page.appearanceAvailable = true;
        wait(0);
        verify(String(status.text).includes("exact compositor revision token"));
        compare(page.controlsEnabled, false);

        page.revisionToken = "7";
        page.managementState = "conflict";
        page.appearanceAvailable = false;
        wait(0);
        verify(String(status.text).includes("changed unexpectedly"));
        compare(page.statusIsDanger, true);
    }

    function test_appearanceDangerRevealsWithoutResettingOrdinaryScroll() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const sticky = findChild(page, "appearanceStickyPreview");
        const scroll = findChild(page, "appearanceOptionsScrollView");
        const statusCard = findChild(page, "appearanceStatusCard");
        const status = findChild(page, "appearanceStatusMessage");
        verify(sticky !== null);
        verify(scroll !== null);
        verify(statusCard !== null);
        verify(status !== null);
        compare(page.statusIsDanger, false);

        const maximumContentY = scroll.contentItem.contentHeight
            - scroll.contentItem.height;
        verify(maximumContentY > 0);
        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        const stickyBefore = sticky.mapToItem(page, 0, 0);

        page.setDraftValue(page.borderSizeId, 4);
        wait(0);
        compare(page.draftDirty, true);
        compare(page.statusIsDanger, false);
        compare(scroll.contentItem.contentY, maximumContentY);

        page.loadState = "recovered";
        wait(0);
        compare(page.statusVisible, true);
        compare(page.statusIsDanger, false);
        compare(scroll.contentItem.contentY, maximumContentY);

        page.loadState = "normal";
        wait(0);
        compare(page.statusIsDanger, false);
        compare(scroll.contentItem.contentY, maximumContentY);

        page.managementState = "conflict";
        tryCompare(scroll.contentItem, "contentY", 0);
        compare(page.statusIsDanger, true);
        compare(statusCard.visible, true);
        verify(String(status.text).includes("changed unexpectedly"));
        const statusPosition = statusCard.mapToItem(scroll, 0, 0);
        verify(statusPosition.y >= 0);
        verify(statusPosition.y + statusCard.height <= scroll.height);
        const stickyAfter = sticky.mapToItem(page, 0, 0);
        verify(Math.abs(stickyAfter.x - stickyBefore.x) <= 0.01);
        verify(Math.abs(stickyAfter.y - stickyBefore.y) <= 0.01);

        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        page.managementState = "managed";
        wait(0);
        compare(page.statusIsDanger, false);
        compare(scroll.contentItem.contentY, maximumContentY);

        page.managementState = "conflict";
        page.managementState = "managed";
        wait(0);
        compare(page.statusIsDanger, false);
        compare(scroll.contentItem.contentY, maximumContentY);
    }

    function test_appearancePreviewStaysStickyWhileOptionsScroll() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const sticky = findChild(page, "appearanceStickyPreview");
        const scroll = findChild(page, "appearanceOptionsScrollView");
        const content = findChild(page, "appearanceOptionsContent");
        const preview = findChild(page, "appearancePreview");
        const motionToggle = findChild(
            page,
            "toggleAppearanceMotionButton"
        );
        verify(sticky !== null);
        verify(scroll !== null);
        verify(content !== null);
        verify(preview !== null);
        verify(motionToggle !== null);
        compare(page.compactPreview, false);
        compare(preview.scale, 1);
        verify(Math.abs(
            preview.width - sticky.availableWidth
        ) <= 0.01);
        verify(Math.abs(
            preview.height - preview.implicitHeight
        ) <= 0.01);
        verify(motionToggle.height >= page.minimumTargetSize);

        const maximumContentY = scroll.contentItem.contentHeight
            - scroll.contentItem.height;
        verify(maximumContentY > 0);
        const stickyBefore = sticky.mapToItem(page, 0, 0);
        const contentBefore = content.mapToItem(page, 0, 0);
        const targetContentY = Math.min(180, maximumContentY);
        verify(targetContentY > 0);

        scroll.contentItem.contentY = targetContentY;
        tryCompare(scroll.contentItem, "contentY", targetContentY);
        const stickyAfter = sticky.mapToItem(page, 0, 0);
        const contentAfter = content.mapToItem(page, 0, 0);
        verify(Math.abs(stickyAfter.x - stickyBefore.x) <= 0.01);
        verify(Math.abs(stickyAfter.y - stickyBefore.y) <= 0.01);
        verify(contentAfter.y < contentBefore.y);
    }

    function test_appearanceActionsReachableAtMinimumWindow() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this,
            { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const compactAuthority = appearanceDefaults();
        compactAuthority[page.blurBrightnessId] = Number.MIN_VALUE;
        compactAuthority[page.roundingPowerId] = 2.573;
        compactAuthority[page.shadowOffsetId] = [Number.MIN_VALUE, -250];
        compactAuthority[page.shadowScaleId] = Number.MIN_VALUE;
        configureAppearancePage(
            page, compactAuthority, true, false,
            appearanceCurveFixtures(), appearanceAnimationFixtures()
        );
        page.setDraftValue(page.blurId, false);
        page.setDraftValue(page.dimInactiveId, true);
        waitForRendering(page);
        wait(0);

        const sticky = findChild(page, "appearanceStickyPreview");
        const scroll = findChild(page, "appearanceOptionsScrollView");
        const content = findChild(page, "appearanceOptionsContent");
        const save = findChild(page, "saveAppearanceButton");
        const preview = findChild(page, "appearancePreview");
        const refresh = findChild(page, "refreshAppearanceButton");
        const source = findChild(page, "windowBorderSourceButton");
        const roundingPower = findChild(
            page, "appearanceRoundingPower"
        );
        const roundingPowerRow = findChild(
            page, "appearanceRoundingPowerRow"
        );
        const roundingPowerValidation = findChild(
            page, "appearanceRoundingPowerValidation"
        );
        const borderPartOfWindow = findChild(
            page, "appearanceBorderPartOfWindow"
        );
        const borderPartOfWindowRow = findChild(
            page, "appearanceBorderPartOfWindowRow"
        );
        const shadowRenderingCard = findChild(
            page, "appearanceShadowRenderingCard"
        );
        const shadowRange = findChild(page, "appearanceShadowRange");
        const shadowRangeRow = findChild(
            page, "appearanceShadowRangeRow"
        );
        const shadowRenderPower = findChild(
            page, "appearanceShadowRenderPower"
        );
        const shadowRenderPowerRow = findChild(
            page, "appearanceShadowRenderPowerRow"
        );
        const shadowSharp = findChild(page, "appearanceShadowSharp");
        const shadowSharpRow = findChild(
            page, "appearanceShadowSharpRow"
        );
        const shadowScale = findChild(page, "appearanceShadowScale");
        const shadowScaleRow = findChild(
            page, "appearanceShadowScaleRow"
        );
        const shadowScaleValidation = findChild(
            page, "appearanceShadowScaleValidation"
        );
        const shadowOffsetX = findChild(
            page, "appearanceShadowOffsetX"
        );
        const shadowOffsetY = findChild(
            page, "appearanceShadowOffsetY"
        );
        const shadowOffsetXRow = findChild(
            page, "appearanceShadowOffsetXRow"
        );
        const shadowOffsetYRow = findChild(
            page, "appearanceShadowOffsetYRow"
        );
        const shadowOffsetXValidation = findChild(
            page, "appearanceShadowOffsetXValidation"
        );
        const shadowOffsetYValidation = findChild(
            page, "appearanceShadowOffsetYValidation"
        );
        const glowRenderingCard = findChild(
            page, "appearanceGlowRenderingCard"
        );
        const glowEnabled = findChild(page, "appearanceGlowEnabled");
        const glowEnabledRow = findChild(
            page, "appearanceGlowEnabledRow"
        );
        const glowRange = findChild(page, "appearanceGlowRange");
        const glowRangeRow = findChild(page, "appearanceGlowRangeRow");
        const glowRenderPower = findChild(
            page, "appearanceGlowRenderPower"
        );
        const glowRenderPowerRow = findChild(
            page, "appearanceGlowRenderPowerRow"
        );
        const glowSafety = findChild(
            page, "appearanceGlowSafetyMessage"
        );
        const dimInactive = findChild(page, "appearanceDimInactive");
        const dimStrength = findChild(page, "appearanceDimStrength");
        const dimStrengthRow = findChild(
            page, "appearanceDimStrengthRow"
        );
        const compactDetailControls = [
            findChild(page, "appearanceActiveOpacity"),
            findChild(page, "appearanceInactiveOpacity"),
            findChild(page, "appearanceFullscreenOpacity"),
            findChild(page, "appearanceDimModal"),
            findChild(page, "appearanceDimSpecial"),
            findChild(page, "appearanceDimAround")
        ];
        const compactDetailRows = [
            findChild(page, "appearanceActiveOpacityRow"),
            findChild(page, "appearanceInactiveOpacityRow"),
            findChild(page, "appearanceFullscreenOpacityRow"),
            findChild(page, "appearanceDimModalRow"),
            findChild(page, "appearanceDimSpecialRow"),
            findChild(page, "appearanceDimAroundRow")
        ];
        const compactBlurDetailControls = [
            findChild(page, "appearanceBlurSize"),
            findChild(page, "appearanceBlurPasses"),
            findChild(page, "appearanceBlurIgnoreOpacity"),
            findChild(page, "appearanceBlurOptimizations"),
            findChild(page, "appearanceBlurXray"),
            findChild(page, "appearanceBlurSpecial"),
            findChild(page, "appearanceBlurPopups"),
            findChild(page, "appearanceBlurPopupsIgnoreAlpha"),
            findChild(page, "appearanceBlurInputMethods"),
            findChild(page, "appearanceBlurInputMethodsIgnoreAlpha"),
            findChild(page, "appearanceBlurBrightness"),
            findChild(page, "appearanceBlurContrast"),
            findChild(page, "appearanceBlurNoise"),
            findChild(page, "appearanceBlurVibrancy"),
            findChild(page, "appearanceBlurVibrancyDarkness")
        ];
        const compactBlurDetailRows = [
            findChild(page, "appearanceBlurSizeRow"),
            findChild(page, "appearanceBlurPassesRow"),
            findChild(page, "appearanceBlurIgnoreOpacityRow"),
            findChild(page, "appearanceBlurOptimizationsRow"),
            findChild(page, "appearanceBlurXrayRow"),
            findChild(page, "appearanceBlurSpecialRow"),
            findChild(page, "appearanceBlurPopupsRow"),
            findChild(page, "appearanceBlurPopupsIgnoreAlphaRow"),
            findChild(page, "appearanceBlurInputMethodsRow"),
            findChild(
                page, "appearanceBlurInputMethodsIgnoreAlphaRow"
            ),
            findChild(page, "appearanceBlurBrightnessRow"),
            findChild(page, "appearanceBlurContrastRow"),
            findChild(page, "appearanceBlurNoiseRow"),
            findChild(page, "appearanceBlurVibrancyRow"),
            findChild(page, "appearanceBlurVibrancyDarknessRow")
        ];
        const resetDefaults = findChild(
            page,
            "resetAppearanceDefaultsButton"
        );
        const motionToggle = findChild(
            page,
            "toggleAppearanceMotionButton"
        );
        const disclaimer = findChild(
            page, "appearanceAnimationPreviewDisclaimer"
        );
        verify(sticky !== null);
        verify(scroll !== null);
        verify(content !== null);
        verify(save !== null);
        verify(preview !== null);
        verify(refresh !== null);
        verify(source !== null);
        verify(roundingPower !== null);
        verify(roundingPowerRow !== null);
        verify(roundingPowerValidation !== null);
        verify(borderPartOfWindow !== null);
        verify(borderPartOfWindowRow !== null);
        verify(shadowRenderingCard !== null);
        verify(shadowRange !== null);
        verify(shadowRangeRow !== null);
        verify(shadowRenderPower !== null);
        verify(shadowRenderPowerRow !== null);
        verify(shadowSharp !== null);
        verify(shadowSharpRow !== null);
        verify(shadowScale !== null);
        verify(shadowScaleRow !== null);
        verify(shadowScaleValidation !== null);
        verify(shadowOffsetX !== null);
        verify(shadowOffsetY !== null);
        verify(shadowOffsetXRow !== null);
        verify(shadowOffsetYRow !== null);
        verify(shadowOffsetXValidation !== null);
        verify(shadowOffsetYValidation !== null);
        verify(glowRenderingCard !== null);
        verify(glowEnabled !== null);
        verify(glowEnabledRow !== null);
        verify(glowRange !== null);
        verify(glowRangeRow !== null);
        verify(glowRenderPower !== null);
        verify(glowRenderPowerRow !== null);
        verify(glowSafety !== null);
        verify(dimInactive !== null);
        verify(dimStrength !== null);
        verify(dimStrengthRow !== null);
        for (const control of compactDetailControls)
            verify(control !== null);
        for (const row of compactDetailRows)
            verify(row !== null);
        for (const control of compactBlurDetailControls)
            verify(control !== null);
        for (const row of compactBlurDetailRows)
            verify(row !== null);
        verify(resetDefaults !== null);
        verify(motionToggle !== null);
        verify(disclaimer !== null);
        compare(page.compactPreview, true);
        verify(scroll.contentItem.contentHeight > scroll.height);
        verify(scroll.height > 0);
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width <= scroll.contentWidth + 0.01);
        verify(preview.width > 0);
        compare(preview.scale, 1);
        verify(Math.abs(
            preview.width - sticky.availableWidth
        ) <= 0.01);
        verify(Math.abs(
            preview.height - preview.implicitHeight
        ) <= 0.01);
        verify(motionToggle.height >= page.minimumTargetSize);
        verify(source.height >= page.minimumTargetSize);
        verify(roundingPower.implicitHeight >= page.minimumTargetSize);
        compare(roundingPower.enabled, true);
        compare(roundingPower.text, "2.573");
        compare(roundingPowerRow.controlWidth, 160);
        verify(roundingPowerRow.width <= content.width + 0.01);
        verify(String(disclaimer.text).includes(
            "window corner power"
        ));
        verify(String(disclaimer.text).includes("not simulated"));
        verify(String(disclaimer.text).includes(
            "Inner glow size, falloff, color, opacity, blur, and motion are not simulated."
        ));
        verify(
            borderPartOfWindow.implicitHeight >= page.minimumTargetSize
        );
        verify(borderPartOfWindowRow.width <= content.width + 0.01);
        compare(borderPartOfWindow.enabled, true);
        compare(borderPartOfWindow.checked, true);
        verify(shadowRenderingCard.width <= content.width + 0.01);
        for (const control of [shadowRange, shadowRenderPower,
                             shadowSharp, shadowScale,
                             shadowOffsetX, shadowOffsetY]) {
            verify(control.implicitHeight >= page.minimumTargetSize);
            compare(control.enabled, true);
            const position = control.mapToItem(content, 0, 0);
            verify(position.x >= 0);
            verify(position.x + control.width <= content.width + 0.01);
        }
        for (const row of [shadowRangeRow, shadowRenderPowerRow,
                           shadowSharpRow, shadowScaleRow, shadowOffsetXRow,
                           shadowOffsetYRow]) {
            verify(row.width <= content.width + 0.01);
        }
        verify(glowRenderingCard.width <= content.width + 0.01);
        for (const control of [glowEnabled, glowRange, glowRenderPower]) {
            verify(control.implicitHeight >= page.minimumTargetSize);
            const position = control.mapToItem(content, 0, 0);
            verify(position.x >= 0);
            verify(position.x + control.width <= content.width + 0.01);
        }
        for (const row of [glowEnabledRow, glowRangeRow,
                           glowRenderPowerRow]) {
            verify(row.width <= content.width + 0.01);
        }
        compare(glowEnabled.enabled, true);
        compare(glowRange.enabled, true);
        compare(glowRenderPower.enabled, false);
        compare(glowSafety.visible, false);
        compare(shadowOffsetXRow.controlWidth, 160);
        compare(shadowOffsetYRow.controlWidth, 160);
        compare(shadowScaleRow.controlWidth, 160);
        compare(shadowOffsetX.maximumLength, 326);
        compare(shadowOffsetY.maximumLength, 326);
        compare(shadowScale.maximumLength, 326);
        compare(shadowScale.text,
            "0." + "0".repeat(323) + "5");
        verify(dimInactive.implicitHeight >= page.minimumTargetSize);
        verify(dimStrength.implicitHeight >= page.minimumTargetSize);
        verify(dimStrengthRow.width <= content.width + 0.01);
        compare(dimStrength.enabled, true);
        for (const control of compactDetailControls) {
            verify(control.implicitHeight >= page.minimumTargetSize);
            compare(control.enabled, true);
        }
        for (const row of compactDetailRows)
            verify(row.width <= content.width + 0.01);
        for (const control of compactBlurDetailControls) {
            verify(control.implicitHeight >= page.minimumTargetSize);
            compare(control.enabled, false);
        }
        for (const row of compactBlurDetailRows)
            verify(row.width <= content.width + 0.01);
        const roundingPowerPosition = roundingPower.mapToItem(
            content, 0, 0
        );
        verify(roundingPowerPosition.x >= 0);
        verify(
            roundingPowerPosition.x + roundingPower.width
                <= content.width + 0.01
        );
        roundingPower.forceActiveFocus();
        roundingPower.text = "2e0";
        roundingPower.textEdited();
        wait(0);
        compare(roundingPowerValidation.visible, true);
        const roundingValidationPosition = roundingPowerValidation.mapToItem(
            content, 0, 0
        );
        verify(roundingValidationPosition.x >= 0);
        verify(
            roundingValidationPosition.x + roundingPowerValidation.width
                <= content.width + 0.01
        );
        const roundingValidationMaximumY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        const roundingValidationY = Math.min(
            roundingValidationMaximumY,
            Math.max(0, roundingValidationPosition.y - 8)
        );
        scroll.contentItem.contentY = roundingValidationY;
        tryCompare(scroll.contentItem, "contentY", roundingValidationY);
        const roundingValidationInScroll = roundingPowerValidation.mapToItem(
            scroll, 0, 0
        );
        verify(roundingValidationInScroll.y >= 0);
        const roundingValidationOverflow = Math.max(
            0,
            roundingValidationInScroll.y + roundingPowerValidation.height
                - scroll.height
        );
        const roundingValidationBottomY = Math.min(
            roundingValidationMaximumY,
            roundingValidationY + roundingValidationOverflow
        );
        scroll.contentItem.contentY = roundingValidationBottomY;
        tryCompare(
            scroll.contentItem, "contentY", roundingValidationBottomY
        );
        const roundingValidationBottomInScroll =
            roundingPowerValidation.mapToItem(scroll, 0, 0);
        verify(
            roundingValidationBottomInScroll.y
                + roundingPowerValidation.height
                <= scroll.height + 0.01
        );
        roundingPower.text = "2.573";
        roundingPower.textEdited();
        refresh.forceActiveFocus();
        wait(0);
        compare(roundingPowerValidation.visible, false);
        page.setDraftValue(page.shadowId, false);
        wait(0);
        compare(borderPartOfWindow.enabled, false);
        compare(shadowRange.enabled, false);
        compare(shadowRenderPower.enabled, false);
        compare(shadowSharp.enabled, false);
        compare(shadowScale.enabled, false);
        compare(shadowOffsetX.enabled, false);
        compare(shadowOffsetY.enabled, false);
        compare(page.draftValue(page.borderPartOfWindowId), true);
        compare(page.draftValue(page.shadowRangeId), 4);
        compare(page.draftValue(page.shadowRenderPowerId), 3);
        compare(page.draftValue(page.shadowSharpId), false);
        compare(page.draftValue(page.shadowScaleId), Number.MIN_VALUE);
        compare(
            page.draftValue(page.shadowOffsetId),
            [Number.MIN_VALUE, -250]
        );
        page.setDraftValue(page.shadowId, true);
        wait(0);
        compare(borderPartOfWindow.enabled, true);
        compare(shadowRange.enabled, true);
        compare(shadowRenderPower.enabled, true);
        compare(shadowSharp.enabled, true);
        compare(shadowScale.enabled, true);
        compare(shadowOffsetX.enabled, true);
        compare(shadowOffsetY.enabled, true);
        compare(page.draftValue(page.borderPartOfWindowId), true);
        page.setDraftValue(page.blurId, true);
        wait(0);
        compare(findChild(page, "appearanceBlurSize").enabled, true);
        compare(findChild(page, "appearanceBlurPasses").enabled, true);
        compare(findChild(
            page, "appearanceBlurIgnoreOpacity"
        ).enabled, true);
        compare(findChild(
            page, "appearanceBlurOptimizations"
        ).enabled, true);
        compare(findChild(page, "appearanceBlurXray").enabled, true);
        compare(findChild(page, "appearanceBlurSpecial").enabled, true);
        compare(findChild(page, "appearanceBlurPopups").enabled, true);
        compare(findChild(
            page, "appearanceBlurPopupsIgnoreAlpha"
        ).enabled, false);
        compare(findChild(
            page, "appearanceBlurInputMethods"
        ).enabled, true);
        compare(findChild(
            page, "appearanceBlurInputMethodsIgnoreAlpha"
        ).enabled, false);
        page.setDraftValue(page.blurPopupsId, true);
        page.setDraftValue(page.blurInputMethodsId, true);
        wait(0);
        for (const control of compactBlurDetailControls)
            compare(control.enabled, true);
        const compactBrightness = findChild(
            page, "appearanceBlurBrightness"
        );
        const compactValidation = findChild(
            page, "appearanceBlurBrightnessValidation"
        );
        const compactMinimumPlainDecimal =
            "0." + "0".repeat(323) + "5";
        compare(shadowOffsetX.text, compactMinimumPlainDecimal);
        compare(shadowOffsetY.text, "-250");
        compare(compactBrightness.text, compactMinimumPlainDecimal);
        compare(compactBrightness.maximumLength, 326);
        const brightnessPosition = compactBrightness.mapToItem(
            content, 0, 0
        );
        verify(brightnessPosition.x >= 0);
        verify(
            brightnessPosition.x + compactBrightness.width
                <= content.width + 0.01
        );
        compactBrightness.forceActiveFocus();
        compactBrightness.text = "1e0";
        compactBrightness.textEdited();
        wait(0);
        compare(compactValidation.visible, true);
        const validationPosition = compactValidation.mapToItem(
            content, 0, 0
        );
        verify(validationPosition.x >= 0);
        verify(
            validationPosition.x + compactValidation.width
                <= content.width + 0.01
        );
        const validationMaximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        const validationContentY = Math.min(
            validationMaximumContentY,
            Math.max(0, validationPosition.y - 8)
        );
        scroll.contentItem.contentY = validationContentY;
        tryCompare(scroll.contentItem, "contentY", validationContentY);
        const validationInScroll = compactValidation.mapToItem(
            scroll, 0, 0
        );
        verify(validationInScroll.y >= 0);
        verify(validationInScroll.y < scroll.height);
        const validationOverflow = Math.max(
            0,
            validationInScroll.y + compactValidation.height - scroll.height
        );
        const validationBottomContentY = Math.min(
            validationMaximumContentY,
            validationContentY + validationOverflow
        );
        scroll.contentItem.contentY = validationBottomContentY;
        tryCompare(
            scroll.contentItem, "contentY", validationBottomContentY
        );
        const validationBottomInScroll = compactValidation.mapToItem(
            scroll, 0, 0
        );
        verify(
            validationBottomInScroll.y + compactValidation.height
                <= scroll.height + 0.01
        );
        verify(
            validationBottomInScroll.y + compactValidation.height > 0
        );
        compactBrightness.text = "1";
        compactBrightness.textEdited();
        refresh.forceActiveFocus();
        wait(0);
        compare(compactBrightness.activeFocus, false);
        compare(compactValidation.visible, false);
        verify(resetDefaults.height >= page.minimumTargetSize);

        const animationsTab = findChild(page, "appearanceAnimationsTab");
        const appearanceTabs = findChild(page, "appearanceTabBar");
        verify(animationsTab !== null);
        verify(appearanceTabs !== null);
        verify(animationsTab.implicitHeight >= page.minimumTargetSize);
        appearanceTabs.currentIndex = 1;
        wait(0);
        compare(page.appearanceTabIndex, 1);
        verify(findChild(page, "animationCurvesCard") !== null);
        verify(findChild(page, "animationRulesCard") !== null);
        for (const objectName of [
                 "editAnimationCurveButton0",
                 "moveAnimationCurveDownButton0",
                 "addAnimationRuleButton",
                 "animationRuleEnabled0",
                 "editAnimationRuleButton0",
                 "removeAnimationRuleButton0"
             ]) {
            const control = findChild(page, objectName);
            verify(control !== null, "Missing compact control " + objectName);
            verify(
                control.implicitHeight >= page.minimumTargetSize,
                objectName + " must provide a 44px interaction target"
            );
        }
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width <= scroll.contentWidth + 0.01);
        appearanceTabs.currentIndex = 0;
        wait(0);

        const stickyBefore = sticky.mapToItem(page, 0, 0);
        const contentBefore = content.mapToItem(page, 0, 0);
        verify(stickyBefore.x >= 0);
        verify(stickyBefore.x + sticky.width <= page.width + 0.01);
        verify(stickyBefore.y >= 0);
        verify(stickyBefore.y + sticky.height <= page.height + 0.01);
        const refreshPosition = refresh.mapToItem(page, 0, 0);
        verify(refreshPosition.x >= 0);
        verify(refreshPosition.x + refresh.width <= page.width);

        const maximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        verify(maximumContentY > 0);
        const sourceInContent = source.mapToItem(content, 0, 0);
        const sourceContentY = Math.min(
            maximumContentY,
            Math.max(0, sourceInContent.y)
        );
        scroll.contentItem.contentY = sourceContentY;
        tryCompare(scroll.contentItem, "contentY", sourceContentY);
        const scrollPosition = scroll.mapToItem(page, 0, 0);
        const sourcePosition = source.mapToItem(page, 0, 0);
        verify(sourcePosition.x >= 0);
        verify(sourcePosition.x + source.width <= page.width);
        verify(sourcePosition.y >= scrollPosition.y);
        verify(sourcePosition.y + source.height
            <= scrollPosition.y + scroll.height + 0.01,
            "source bottom " + (sourcePosition.y + source.height)
                + " exceeds scroll bottom "
                + (scrollPosition.y + scroll.height));

        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        const stickyAfter = sticky.mapToItem(page, 0, 0);
        const contentAfter = content.mapToItem(page, 0, 0);
        verify(Math.abs(stickyAfter.x - stickyBefore.x) <= 0.01);
        verify(Math.abs(stickyAfter.y - stickyBefore.y) <= 0.01);
        verify(contentAfter.y < contentBefore.y);
        const savePosition = save.mapToItem(page, 0, 0);
        verify(savePosition.x >= 0);
        verify(savePosition.x + save.width <= page.width);
        verify(savePosition.y >= 0);
        verify(savePosition.y + save.height <= page.height);

        appearanceTabs.currentIndex = 1;
        wait(0);
        const animationMaximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        verify(animationMaximumContentY > 0);
        scroll.contentItem.contentY = animationMaximumContentY;
        tryCompare(
            scroll.contentItem, "contentY", animationMaximumContentY
        );
        const animationSavePosition = save.mapToItem(page, 0, 0);
        verify(animationSavePosition.x >= 0);
        verify(animationSavePosition.x + save.width <= page.width);
        verify(animationSavePosition.y >= 0);
        verify(animationSavePosition.y + save.height <= page.height);
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
    }

    function test_inputUsesExactAuthoredControlsStepsAndTargets() {
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page);
        waitForRendering(page);
        wait(0);

        compare(page.trustedDefinitionsValid, true);
        compare(page.trustedValuesValid, true);
        compare(page.controlsEnabled, true);
        compare(page.expectedOptionIds.length, 49);
        compare(
            page.expectedOptionIds[48],
            "hyprland.input.resolve_binds_by_sym"
        );
        compare(page.expectedOptionIds.slice(37), [
            "hyprland.cursor.hide_on_key_press",
            "hyprland.cursor.hide_on_touch",
            "hyprland.cursor.hide_on_tablet",
            "hyprland.cursor.inactive_timeout",
            "hyprland.cursor.hotspot_padding",
            "hyprland.cursor.no_warps",
            "hyprland.cursor.persistent_warps",
            "hyprland.cursor.warp_back_after_non_mouse_input",
            "hyprland.input.tablet.region_position",
            "hyprland.input.tablet.absolute_region_position",
            "hyprland.input.tablet.region_size",
            "hyprland.input.resolve_binds_by_sym"
        ]);

        const keyboard = findChild(page, "inputKeyboardCard");
        const introduction = findChild(page, "inputIntroduction");
        const virtualKeyboard = findChild(
            page, "inputVirtualKeyboardCard"
        );
        const virtualKeyboardCopy = findChild(
            page, "inputVirtualKeyboardLifecycleCopy"
        );
        const mouse = findChild(page, "inputMouseCard");
        const pointerBehavior = findChild(
            page, "inputPointerBehaviorCard"
        );
        const cursorVisibility = findChild(
            page, "inputCursorVisibilityCard"
        );
        const cursorPlacement = findChild(
            page, "inputCursorPlacementCard"
        );
        const touchpad = findChild(page, "inputTouchpadCard");
        const advancedScrolling = findChild(
            page, "inputAdvancedScrollingCard"
        );
        const touchpadButtons = findChild(
            page, "inputTouchpadButtonsGesturesCard"
        );
        const touchpadButtonsCopy = findChild(
            page, "inputTouchpadButtonsGesturesAvailabilityCopy"
        );
        const touchDevice = findChild(page, "inputTouchDeviceCard");
        const touchDeviceCopy = findChild(
            page, "inputTouchDeviceAvailabilityCopy"
        );
        const drawingTablet = findChild(page, "inputDrawingTabletCard");
        const drawingTabletCopy = findChild(
            page, "inputDrawingTabletAvailabilityCopy"
        );
        const tabletMappedRegion = findChild(
            page, "inputTabletMappedRegionCard"
        );
        const tabletMappedRegionCopy = findChild(
            page, "inputTabletMappedRegionAvailabilityCopy"
        );
        const touchpadCopy = findChild(
            page, "inputTouchpadAvailabilityCopy"
        );
        verify(keyboard !== null);
        verify(introduction !== null);
        verify(String(introduction.text).includes(
            "cursor visibility and placement"
        ));
        verify(virtualKeyboard !== null);
        verify(virtualKeyboardCopy !== null);
        verify(mouse !== null);
        verify(pointerBehavior !== null);
        verify(cursorVisibility !== null);
        verify(cursorPlacement !== null);
        verify(touchpad !== null);
        verify(touchpadButtons !== null);
        verify(touchpadButtonsCopy !== null);
        verify(touchDevice !== null);
        verify(touchDeviceCopy !== null);
        verify(drawingTablet !== null);
        verify(tabletMappedRegion !== null);
        verify(drawingTabletCopy !== null);
        verify(tabletMappedRegionCopy !== null);
        verify(advancedScrolling !== null);
        verify(touchpadCopy !== null);
        compare(touchpad.visible, true);
        verify(String(touchpadCopy.text).includes(
            "apply when a touchpad is present"
        ));
        verify(String(touchpadButtonsCopy.text).includes(
            "without hardware detection"
        ));
        for (const copy of [touchDeviceCopy, drawingTabletCopy]) {
            verify(String(copy.text).includes("global fallbacks"));
            verify(String(copy.text).includes(
                "exact saved per-device override wins"
            ));
            verify(String(copy.text).includes(
                "Devices tab remains read-only"
            ));
            verify(String(copy.text).includes("does not prove"));
        }
        verify(String(touchDeviceCopy.text).includes(
            "compatible libinput touch devices"
        ));
        verify(String(drawingTabletCopy.text).includes(
            "compatible libinput drawing tablets"
        ));
        verify(String(tabletMappedRegionCopy.text).includes(
            "global fallbacks"
        ));
        verify(String(tabletMappedRegionCopy.text).includes(
            "Exact saved per-device values win"
        ));
        verify(String(tabletMappedRegionCopy.text).includes(
            "Relative tablet motion keeps these values"
        ));
        verify(String(virtualKeyboardCopy.text).includes(
            "when a virtual keyboard next connects"
        ));
        verify(String(virtualKeyboardCopy.text).includes(
            "releasing held keys is checked when it closes"
        ));
        verify(keyboard.mapToItem(page, 0, 0).y
            < virtualKeyboard.mapToItem(page, 0, 0).y);
        verify(virtualKeyboard.mapToItem(page, 0, 0).y
            < mouse.mapToItem(page, 0, 0).y);
        verify(mouse.mapToItem(page, 0, 0).y
            < pointerBehavior.mapToItem(page, 0, 0).y);
        verify(pointerBehavior.mapToItem(page, 0, 0).y
            < cursorVisibility.mapToItem(page, 0, 0).y);
        verify(cursorVisibility.mapToItem(page, 0, 0).y
            < cursorPlacement.mapToItem(page, 0, 0).y);
        verify(cursorPlacement.mapToItem(page, 0, 0).y
            < touchpad.mapToItem(page, 0, 0).y);
        verify(touchpad.mapToItem(page, 0, 0).y
            < touchpadButtons.mapToItem(page, 0, 0).y);
        verify(touchpadButtons.mapToItem(page, 0, 0).y
            < touchDevice.mapToItem(page, 0, 0).y);
        verify(touchDevice.mapToItem(page, 0, 0).y
            < drawingTablet.mapToItem(page, 0, 0).y);
        verify(drawingTablet.mapToItem(page, 0, 0).y
            < tabletMappedRegion.mapToItem(page, 0, 0).y);
        verify(tabletMappedRegion.mapToItem(page, 0, 0).y
            < advancedScrolling.mapToItem(page, 0, 0).y);

        const repeatRate = findChild(page, "inputRepeatRate");
        const repeatDelay = findChild(page, "inputRepeatDelay");
        const numLock = findChild(page, "inputNumLockByDefault");
        const resolveBindsBySymbol = findChild(
            page, "inputResolveBindsBySymbol"
        );
        const virtualKeyboardShareStates = findChild(
            page, "inputVirtualKeyboardShareStates"
        );
        const virtualKeyboardRelease = findChild(
            page, "inputVirtualKeyboardReleasePressedOnClose"
        );
        const virtualKeyboardName = findChild(
            page, "inputVirtualKeyboardNameAfterProcess"
        );
        const sensitivity = findChild(page, "inputSensitivity");
        const acceleration = findChild(
            page, "inputAccelerationProfile"
        );
        const forceNoAccel = findChild(page, "inputForceNoAccel");
        const rotation = findChild(page, "inputRotation");
        const middleClickPaste = findChild(
            page, "inputMiddleClickPaste"
        );
        const cursorHideOnKeyPress = findChild(
            page, "inputCursorHideOnKeyPress"
        );
        const cursorHideOnTouch = findChild(
            page, "inputCursorHideOnTouch"
        );
        const cursorHideOnTablet = findChild(
            page, "inputCursorHideOnTablet"
        );
        const cursorInactiveTimeout = findChild(
            page, "inputCursorInactiveTimeout"
        );
        const cursorInactiveTimeoutValue = findChild(
            page, "inputCursorInactiveTimeoutValue"
        );
        const cursorHotspotPadding = findChild(
            page, "inputCursorHotspotPadding"
        );
        const cursorNoWarps = findChild(page, "inputCursorNoWarps");
        const cursorPersistentWarps = findChild(
            page, "inputCursorPersistentWarps"
        );
        const cursorWarpBack = findChild(
            page, "inputCursorWarpBackAfterNonMouseInput"
        );
        const naturalScroll = findChild(page, "inputNaturalScroll");
        const leftHanded = findChild(page, "inputLeftHanded");
        const scrollFactor = findChild(page, "inputScrollFactor");
        const tapToClick = findChild(
            page, "inputTouchpadTapToClick"
        );
        const tapAndDrag = findChild(
            page, "inputTouchpadTapAndDrag"
        );
        const touchpadNatural = findChild(
            page, "inputTouchpadNaturalScroll"
        );
        const disableWhileTyping = findChild(
            page, "inputTouchpadDisableWhileTyping"
        );
        const touchpadScroll = findChild(
            page, "inputTouchpadScrollFactor"
        );
        const scrollMethod = findChild(page, "inputScrollMethod");
        const scrollButton = findChild(page, "inputScrollButton");
        const scrollButtonLock = findChild(page, "inputScrollButtonLock");
        const offWindowAxis = findChild(page, "inputOffWindowAxisEvents");
        const discreteScroll = findChild(page, "inputEmulateDiscreteScroll");
        const clickfinger = findChild(
            page, "inputTouchpadClickfingerBehavior"
        );
        const tapButtonMap = findChild(page, "inputTouchpadTapButtonMap");
        const middleButton = findChild(
            page, "inputTouchpadMiddleButtonEmulation"
        );
        const dragLock = findChild(page, "inputTouchpadDragLock");
        const multiFingerDrag = findChild(
            page, "inputTouchpadMultiFingerDrag"
        );
        const flipHorizontal = findChild(
            page, "inputTouchpadFlipHorizontal"
        );
        const flipVertical = findChild(page, "inputTouchpadFlipVertical");
        const touchDeviceEnabled = findChild(
            page, "inputTouchDeviceEnabled"
        );
        const touchDeviceTransform = findChild(
            page, "inputTouchDeviceTransform"
        );
        const tabletRelativeInput = findChild(
            page, "inputTabletRelativeInput"
        );
        const tabletLeftHanded = findChild(
            page, "inputTabletLeftHanded"
        );
        const tabletTransform = findChild(page, "inputTabletTransform");
        const tabletRegionPositionX = findChild(
            page, "inputTabletRegionPositionX"
        );
        const tabletRegionPositionY = findChild(
            page, "inputTabletRegionPositionY"
        );
        const tabletAbsoluteRegionPosition = findChild(
            page, "inputTabletAbsoluteRegionPosition"
        );
        const tabletRegionSizeWidth = findChild(
            page, "inputTabletRegionSizeWidth"
        );
        const tabletRegionSizeHeight = findChild(
            page, "inputTabletRegionSizeHeight"
        );
        const controls = [
            repeatRate,
            repeatDelay,
            numLock,
            resolveBindsBySymbol,
            virtualKeyboardShareStates,
            virtualKeyboardRelease,
            virtualKeyboardName,
            sensitivity,
            acceleration,
            forceNoAccel,
            rotation,
            middleClickPaste,
            cursorHideOnKeyPress,
            cursorHideOnTouch,
            cursorHideOnTablet,
            cursorInactiveTimeout,
            cursorHotspotPadding,
            cursorNoWarps,
            cursorPersistentWarps,
            cursorWarpBack,
            naturalScroll,
            leftHanded,
            scrollFactor,
            tapToClick,
            tapAndDrag,
            touchpadNatural,
            disableWhileTyping,
            touchpadScroll,
            scrollMethod,
            scrollButton,
            scrollButtonLock,
            offWindowAxis,
            discreteScroll,
            clickfinger,
            tapButtonMap,
            middleButton,
            dragLock,
            multiFingerDrag,
            flipHorizontal,
            flipVertical,
            touchDeviceEnabled,
            touchDeviceTransform,
            tabletRelativeInput,
            tabletLeftHanded,
            tabletTransform,
            tabletRegionPositionX,
            tabletRegionPositionY,
            tabletAbsoluteRegionPosition,
            tabletRegionSizeWidth,
            tabletRegionSizeHeight
        ];
        for (const control of controls) {
            verify(control !== null);
            verify(control.implicitHeight >= 44);
            verify(String(control.Accessible.name).length > 0);
        }

        compare(repeatRate.from, 0);
        compare(repeatRate.to, 200);
        compare(repeatRate.value, 25);
        compare(repeatRate.stepSize, 1);
        compare(repeatDelay.from, 0);
        compare(repeatDelay.to, 2000);
        compare(repeatDelay.value, 600);
        compare(repeatDelay.stepSize, 1);
        compare(numLock.checked, false);
        compare(resolveBindsBySymbol.checked, false);
        compare(
            String(resolveBindsBySymbol.parent.title),
            "Shortcuts follow the active layout"
        );
        compare(
            String(resolveBindsBySymbol.parent.description),
            "Resolve symbol-based shortcuts from each keyboard's active layout instead of the primary globally configured layout. Exact saved per-device values win. Keycode-based shortcuts are unchanged."
        );
        compare(
            String(resolveBindsBySymbol.Accessible.name),
            "Resolve symbol-based shortcuts from each keyboard's active layout"
        );
        compare(resolveBindsBySymbol.parent.parent, numLock.parent.parent);
        verify(resolveBindsBySymbol.parent.y > numLock.parent.y);
        verify(Math.abs(
            resolveBindsBySymbol.parent.y
                - numLock.parent.y - numLock.parent.height - 18
        ) <= 0.01);
        let resolveHydrationEditCount = 0;
        resolveBindsBySymbol.parent.valueModified.connect(function() {
            ++resolveHydrationEditCount;
        });
        const hydratedValues = inputDefaults();
        hydratedValues[page.resolveBindsBySymbolId] = true;
        page.inputValues = hydratedValues;
        page.revisionToken = "8";
        page.reviewProjection();
        wait(0);
        compare(resolveBindsBySymbol.checked, true);
        compare(resolveHydrationEditCount, 0);
        compare(page.draftDirty, false);
        compare(virtualKeyboardShareStates.currentText,
            "Except input methods");
        compare(virtualKeyboardShareStates.model.length, 3);
        compare(virtualKeyboardRelease.checked, false);
        compare(virtualKeyboardName.checked, true);
        for (const control of [
                 numLock,
                 resolveBindsBySymbol,
                 virtualKeyboardShareStates,
                 virtualKeyboardRelease,
                 virtualKeyboardName
             ])
            compare(control.enabled, true);
        page.inputAvailable = false;
        for (const control of [
                 numLock,
                 resolveBindsBySymbol,
                 virtualKeyboardShareStates,
                 virtualKeyboardRelease,
                 virtualKeyboardName
             ])
            compare(control.enabled, false);
        page.inputAvailable = true;
        for (const control of [
                 numLock,
                 resolveBindsBySymbol,
                 virtualKeyboardShareStates,
                 virtualKeyboardRelease,
                 virtualKeyboardName
             ])
            compare(control.enabled, true);
        compare(sensitivity.from, -1);
        compare(sensitivity.to, 1);
        compare(sensitivity.value, 0);
        compare(sensitivity.stepSize, 0.05);
        compare(acceleration.currentText, "Automatic");
        compare(acceleration.model.length, 3);
        compare(forceNoAccel.checked, false);
        compare(rotation.from, 0);
        compare(rotation.to, 359);
        compare(rotation.value, 0);
        compare(rotation.stepSize, 1);
        compare(middleClickPaste.checked, true);
        compare(cursorHideOnKeyPress.checked, false);
        compare(cursorHideOnTouch.checked, true);
        compare(cursorHideOnTablet.checked, false);
        compare(cursorInactiveTimeout.from, 0);
        compare(cursorInactiveTimeout.to, 20);
        compare(cursorInactiveTimeout.value, 0);
        compare(cursorInactiveTimeout.stepSize, 0.1);
        compare(cursorInactiveTimeoutValue.text, "0.0 s");
        compare(cursorHotspotPadding.from, 0);
        compare(cursorHotspotPadding.to, 20);
        compare(cursorHotspotPadding.value, 0);
        compare(cursorHotspotPadding.stepSize, 1);
        compare(cursorNoWarps.checked, false);
        compare(cursorPersistentWarps.checked, false);
        compare(cursorWarpBack.checked, false);
        compare(
            String(cursorHideOnKeyPress.parent.title),
            "Hide after keyboard input"
        );
        compare(
            String(cursorHideOnKeyPress.parent.description),
            "Hide the cursor after a keyboard key event until physical mouse movement."
        );
        compare(
            String(cursorHideOnTouch.parent.description),
            "Hide the cursor after touch input. Hyprland can keep it hidden through the first following mouse movement; a subsequent movement reveals it."
        );
        compare(
            String(cursorHideOnTablet.parent.description),
            "Hide the cursor after drawing-tablet input. Hyprland can keep it hidden through the first following mouse movement; a subsequent movement reveals it."
        );
        compare(
            String(cursorInactiveTimeout.parent.parent.description),
            "Hide the cursor after this many seconds without pointer, touch, or tablet movement or a pointer-button event. Zero disables only inactivity-based hiding. Hyprland checks every 500 ms; wheel scrolling alone does not restart the timer."
        );
        compare(
            String(cursorHotspotPadding.parent.description),
            "Clamp a hotspot-centered square by checking its corners against the active display layout on the next pointer move or warp. At a display seam the square may span adjacent displays. This does not change cursor artwork."
        );
        compare(
            String(cursorNoWarps.parent.description),
            "Prevent ordinary compositor actions from moving the pointer automatically. Explicitly forced moves can still occur."
        );
        compare(
            String(cursorPersistentWarps.parent.description),
            "When an action moves the pointer to a refocused window, return to its last remembered position there instead of the center. This does not force a move when ordinary jumps are suppressed."
        );
        compare(
            String(cursorWarpBack.parent.description),
            "When the pointer was last moved by non-mouse input, return to the last physical-mouse position when the mouse is next used. This return is not blocked by Suppress ordinary pointer jumps."
        );
        verify(String(forceNoAccel.parent.description).includes(
            "unaccelerated device motion"
        ));
        verify(String(forceNoAccel.parent.description).includes(
            "remain saved"
        ));
        verify(String(rotation.parent.description).includes(
            "Unsupported devices ignore"
        ));
        verify(String(middleClickPaste.parent.description).includes(
            "next tries to set that selection"
        ));
        compare(naturalScroll.checked, false);
        compare(leftHanded.checked, false);
        compare(scrollFactor.from, 0);
        compare(scrollFactor.to, 2);
        compare(scrollFactor.value, 1);
        compare(scrollFactor.stepSize, 0.1);
        compare(tapToClick.checked, true);
        compare(tapAndDrag.checked, true);
        compare(touchpadNatural.checked, false);
        compare(disableWhileTyping.checked, true);
        compare(touchpadScroll.from, 0);
        compare(touchpadScroll.to, 2);
        compare(touchpadScroll.value, 1);
        compare(touchpadScroll.stepSize, 0.1);
        compare(scrollMethod.currentText, "Automatic");
        compare(scrollMethod.model.length, 5);
        compare(scrollButton.from, 0);
        compare(scrollButton.to, 300);
        compare(scrollButton.value, 0);
        compare(scrollButton.enabled, false);
        compare(scrollButtonLock.checked, false);
        compare(scrollButtonLock.enabled, false);
        compare(offWindowAxis.currentText, "Send to window");
        compare(offWindowAxis.model.length, 4);
        compare(discreteScroll.currentText, "When needed");
        compare(discreteScroll.model.length, 3);
        compare(clickfinger.checked, false);
        compare(tapButtonMap.currentText, "Automatic");
        compare(tapButtonMap.model.length, 3);
        compare(tapButtonMap.enabled, true);
        compare(middleButton.checked, false);
        compare(dragLock.currentText, "Off");
        compare(dragLock.model.length, 3);
        compare(dragLock.enabled, true);
        compare(multiFingerDrag.currentText, "Off");
        compare(multiFingerDrag.model.length, 3);
        compare(flipHorizontal.checked, false);
        compare(flipVertical.checked, false);
        compare(touchDeviceEnabled.checked, true);
        compare(touchDeviceTransform.from, 0);
        compare(touchDeviceTransform.to, 6);
        compare(touchDeviceTransform.value, 0);
        compare(touchDeviceTransform.stepSize, 1);
        compare(tabletRelativeInput.checked, false);
        compare(tabletLeftHanded.checked, false);
        compare(tabletTransform.from, 0);
        compare(tabletTransform.to, 6);
        compare(tabletTransform.value, 0);
        compare(tabletTransform.stepSize, 1);
        compare(tabletRegionPositionX.text, "0");
        compare(tabletRegionPositionX.minimumValue, -20000);
        compare(tabletRegionPositionX.maximumValue, 20000);
        compare(tabletRegionPositionY.text, "0");
        compare(tabletRegionPositionY.minimumValue, -20000);
        compare(tabletRegionPositionY.maximumValue, 20000);
        compare(tabletAbsoluteRegionPosition.checked, false);
        compare(tabletRegionSizeWidth.text, "0");
        compare(tabletRegionSizeWidth.minimumValue, -100);
        compare(tabletRegionSizeWidth.maximumValue, 4000);
        compare(tabletRegionSizeHeight.text, "0");
        compare(tabletRegionSizeHeight.minimumValue, -100);
        compare(tabletRegionSizeHeight.maximumValue, 4000);
        for (const control of [
                 tabletRegionPositionX,
                 tabletRegionPositionY,
                 tabletAbsoluteRegionPosition,
                 tabletRegionSizeWidth,
                 tabletRegionSizeHeight
             ]) {
            compare(control.enabled, true);
        }
        verify(String(touchDeviceTransform.parent.description).includes(
            "0 for identity (normal)"
        ));
        verify(String(touchDeviceTransform.parent.description).includes(
            "6 for flipped + 180°"
        ));
        verify(String(touchDeviceTransform.parent.description).includes(
            "calibration-matrix support"
        ));
        verify(String(tabletRelativeInput.parent.description).includes(
            "pen-motion deltas"
        ));
        verify(String(tabletRelativeInput.parent.description).includes(
            "absolute pen placement"
        ));
        verify(String(tabletLeftHanded.parent.description).includes(
            "rotate a compatible drawing tablet by 180°"
        ));
        verify(String(tabletTransform.parent.description).includes(
            "0 for identity (normal)"
        ));
        verify(String(tabletTransform.parent.description).includes(
            "6 for flipped + 180°"
        ));
        verify(String(tabletRegionPositionX.parent.parent.description).includes(
            "exact compositor-space X position"
        ));
        verify(String(tabletAbsoluteRegionPosition.parent.description).includes(
            "saved per-device output binding"
        ));
        verify(String(tabletRegionSizeWidth.parent.parent.description).includes(
            "magnitude at least 0.000000001"
        ));
        verify(String(tabletRegionSizeHeight.parent.parent.description).includes(
            "negative height reverses the vertical axis"
        ));

        page.setExactVectorComponentDraftValue(
            page.tabletRegionPositionId, 0, 125.5
        );
        page.setExactVectorComponentDraftValue(
            page.tabletRegionPositionId, 1, -80.25
        );
        page.setDraftValue(page.tabletAbsoluteRegionPositionId, true);
        page.setExactVectorComponentDraftValue(
            page.tabletRegionSizeId, 0, -50.5
        );
        page.setExactVectorComponentDraftValue(
            page.tabletRegionSizeId, 1, 1080.125
        );
        compare(page.draftValue(page.tabletRegionPositionId), [125.5, -80.25]);
        compare(page.draftValue(page.tabletAbsoluteRegionPositionId), true);
        compare(page.draftValue(page.tabletRegionSizeId), [-50.5, 1080.125]);
        page.setDraftValue(page.tabletRelativeInputId, true);
        for (const control of [
                 tabletRegionPositionX,
                 tabletRegionPositionY,
                 tabletAbsoluteRegionPosition,
                 tabletRegionSizeWidth,
                 tabletRegionSizeHeight
             ]) {
            compare(control.enabled, false);
        }
        compare(page.draftValue(page.tabletRegionPositionId), [125.5, -80.25]);
        compare(page.draftValue(page.tabletAbsoluteRegionPositionId), true);
        compare(page.draftValue(page.tabletRegionSizeId), [-50.5, 1080.125]);
        page.setDraftValue(page.tabletRelativeInputId, false);
        compare(tabletRegionPositionX.enabled, true);
        compare(tabletRegionSizeHeight.enabled, true);

        page.setDraftValue(page.sensitivityId, 0.35);
        page.setDraftValue(page.accelerationProfileId, "flat");
        page.setDraftValue(page.forceNoAccelId, true);
        compare(forceNoAccel.checked, true);
        compare(sensitivity.enabled, false);
        compare(acceleration.enabled, false);
        compare(rotation.enabled, true);
        compare(middleClickPaste.enabled, true);
        compare(page.draftValue(page.sensitivityId), 0.35);
        compare(page.draftValue(page.accelerationProfileId), "flat");
        page.setDraftValue(page.forceNoAccelId, false);
        compare(sensitivity.enabled, true);
        compare(acceleration.enabled, true);
        compare(page.draftValue(page.sensitivityId), 0.35);
        compare(page.draftValue(page.accelerationProfileId), "flat");

        page.setDraftValue(page.cursorPersistentWarpsId, true);
        page.setDraftValue(
            page.cursorWarpBackAfterNonMouseInputId, true
        );
        page.setDraftValue(page.cursorNoWarpsId, true);
        compare(cursorNoWarps.checked, true);
        compare(cursorPersistentWarps.checked, true);
        compare(cursorWarpBack.checked, true);
        compare(cursorPersistentWarps.enabled, true);
        compare(cursorWarpBack.enabled, true);
        compare(page.draftValue(page.cursorPersistentWarpsId), true);
        compare(page.draftValue(
            page.cursorWarpBackAfterNonMouseInputId
        ), true);

        page.setDraftValue(page.touchpadTapToClickId, false);
        compare(tapButtonMap.enabled, false);
        compare(dragLock.enabled, false);
        page.setDraftValue(page.touchpadTapToClickId, true);
        compare(tapButtonMap.enabled, true);
        compare(dragLock.enabled, true);
        page.setDraftValue(page.touchpadTapAndDragId, false);
        compare(tapButtonMap.enabled, true);
        compare(dragLock.enabled, false);
        compare(multiFingerDrag.enabled, true);
        page.setDraftValue(page.touchpadTapAndDragId, true);

        const actionTargets = [
            "refreshInputButton",
            "inputOpenDisplaysButton",
            "loadCurrentInputButton",
            "retryApplyInputButton",
            "recoverInputButton",
            "discardInputDraftButton",
            "resetInputDefaultsButton",
            "saveInputButton",
            "cancelInputRecoveryButton",
            "confirmInputRecoveryButton"
        ];
        for (const objectName of actionTargets) {
            const target = findChild(page, objectName);
            verify(target !== null, "Missing target " + objectName);
            verify(
                target.implicitHeight >= 44,
                objectName + " must provide a 44px interaction target"
            );
        }
    }

    function test_inputDevicesTabIsReadOnlyQualifiedAndAccessible() {
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page);
        configureInputDeviceInventory(page);
        waitForRendering(page);
        wait(0);

        const tabs = findChild(page, "inputTabBar");
        const globalTab = findChild(page, "inputGlobalTab");
        const devicesTab = findChild(page, "inputDevicesTab");
        const pane = findChild(page, "inputDevicesPane");
        const refresh = findChild(page, "refreshConnectedInputDevicesButton");
        const manageProfiles = findChild(
            page, "manageInputDeviceProfilesButton"
        );
        verify(tabs !== null);
        verify(globalTab !== null);
        verify(devicesTab !== null);
        verify(pane !== null);
        verify(refresh !== null);
        verify(manageProfiles !== null);
        compare(page.inputTabIndex, 0);
        compare(globalTab.checked, true);
        compare(pane.visible, false);
        verify(globalTab.implicitHeight >= 44);
        verify(devicesTab.implicitHeight >= 44);
        verify(refresh.implicitHeight >= 44);
        verify(manageProfiles.implicitHeight >= 44);

        let refreshCount = 0;
        let manageProfilesCount = 0;
        page.refreshConnectedInputDevicesRequested.connect(function() {
            ++refreshCount;
        });
        page.manageInputDeviceProfilesRequested.connect(function() {
            ++manageProfilesCount;
        });
        tabs.currentIndex = 1;
        tryCompare(page, "inputTabIndex", 1);
        compare(pane.visible, true);
        refresh.clicked();
        compare(refreshCount, 1);
        manageProfiles.clicked();
        compare(manageProfilesCount, 1);

        const sections = [
            findChild(page, "inputKeyboardsSection"),
            findChild(page, "inputPointingDevicesSection"),
            findChild(page, "inputTouchDevicesSection"),
            findChild(page, "inputTabletsSection")
        ];
        for (const section of sections) {
            verify(section !== null);
            compare(section.visible, true);
        }
        const sectionY = sections.map(function(section) {
            return section.mapToItem(page, 0, 0).y;
        });
        const sortedSectionY = sectionY.slice().sort(function(left, right) {
            return left - right;
        });
        compare(sectionY, sortedSectionY);

        const connectedRows = [
            findChild(page, "inputConnectedDeviceRow0"),
            findChild(page, "inputConnectedDeviceRow1"),
            findChild(page, "inputConnectedDeviceRow2"),
            findChild(page, "inputConnectedDeviceRow3")
        ];
        for (const row of connectedRows) {
            verify(row !== null);
            verify(row.implicitHeight >= 52);
            verify(String(row.Accessible.name).length > 0);
        }
        verify(String(connectedRows[0].Accessible.name).includes(
            "exact-keyboard"
        ));
        const savedRow = findChild(page, "inputSavedDeviceRow0");
        verify(savedRow !== null);
        verify(savedRow.implicitHeight >= 52);
        verify(String(savedRow.Accessible.name).includes(
            "saved device with a very long exact selector"
        ));
        verify(String(findChild(page, "inputDevicesObservationCopy").text)
            .includes("Observed at"));
        verify(String(findChild(page, "inputDeviceSessionIdentityCopy").text)
            .includes("connection order"));
        verify(String(findChild(page, "inputUnaddressableDevicesSummary").text)
            .includes("3 tablet tools"));

        const forbiddenNames = [
            "addInputDeviceButton",
            "editInputDeviceButton",
            "forgetInputDeviceButton",
            "reassignInputDeviceButton",
            "resetInputDeviceButton",
            "testInputDeviceButton",
            "confirmInputDeviceButton"
        ];
        for (const name of forbiddenNames)
            compare(findChild(page, name), null);

        page.setDraftValue(page.repeatRateId, 31);
        page.setDraftValue(page.touchDeviceEnabledId, false);
        page.setDraftValue(page.touchDeviceTransformId, 6);
        page.setDraftValue(page.tabletRelativeInputId, true);
        page.setDraftValue(page.tabletLeftHandedId, true);
        page.setDraftValue(page.tabletTransformId, 5);
        page.setDraftValue(page.cursorInactiveTimeoutId, 2.37);
        page.setDraftValue(page.cursorNoWarpsId, true);
        page.setDraftValue(page.cursorPersistentWarpsId, true);
        compare(findChild(page, "inputGlobalDraftStatus").visible, true);
        verify(String(findChild(page, "inputGlobalTab").text).includes("•"));

        page.inputDeviceDiscoveryAvailable = false;
        page.inputDeviceDiscoveryErrorMessage = "Independent discovery failure";
        wait(0);
        compare(page.controlsEnabled, true);
        compare(Object.keys(page.draftValues).length, 49);
        compare(page.draftValue(page.touchDeviceEnabledId), false);
        compare(page.draftValue(page.touchDeviceTransformId), 6);
        compare(page.draftValue(page.tabletRelativeInputId), true);
        compare(page.draftValue(page.tabletLeftHandedId), true);
        compare(page.draftValue(page.tabletTransformId), 5);
        compare(page.draftValue(page.cursorInactiveTimeoutId), 2.37);
        compare(page.draftValue(page.cursorNoWarpsId), true);
        compare(page.draftValue(page.cursorPersistentWarpsId), true);
        page.inputDeviceDiscoveryAvailable = true;
        page.inputDeviceDiscoveryErrorMessage = "";

        page.inputAvailable = false;
        compare(pane.visible, true);
        compare(refresh.enabled, true);
        page.inputDeviceDiscoveryAvailable = false;
        page.inputDeviceDiscoveryErrorMessage = "Injected discovery failure";
        page.inputDeviceProjectionAvailable = true;
        page.otherSavedInputDevices = [{
            id: "device:offline",
            selector: "offline exact selector",
            configuredKind: "keyboard",
            configuredEnabled: true,
            overrideCount: 0,
            matchState: "inventory-unavailable",
            observedKind: null
        }];
        wait(0);
        verify(String(findChild(page, "inputDeviceDiscoveryStatusMessage").text)
            .includes("Connection status unavailable"));
        verify(findChild(page, "inputSavedDeviceRow0") !== null);
        page.inputDeviceDiscoveryAvailable = true;
        page.inputDeviceDiscoveryErrorMessage = "";
        page.inputDeviceProjectionAvailable = false;
        page.inputDeviceProjectionErrorMessage = "Injected projection failure";
        wait(0);
        verify(String(findChild(page, "inputDeviceProjectionStatusMessage").text)
            .includes("Saved device settings are unavailable"));
        verify(findChild(page, "inputConnectedDeviceRow0") !== null);
    }

    function test_inputGesturesTabsActionsCrudAndAuthenticatedFilters() {
        const occupiedDefault = gestureRecord(
            "occupied-default", 3, "swipe", [], 1, false,
            { type: "workspace" }
        );
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page, inputDefaults(), [occupiedDefault]);
        waitForRendering(page);
        wait(0);

        const tabs = findChild(page, "inputTabBar");
        const globalTab = findChild(page, "inputGlobalTab");
        const devicesTab = findChild(page, "inputDevicesTab");
        const gesturesTab = findChild(page, "inputGesturesTab");
        for (const tab of [globalTab, devicesTab, gesturesTab]) {
            verify(tab !== null);
            verify(tab.implicitHeight >= 44);
        }
        verify(tabs !== null);
        compare(tabs.count, 3);
        compare(page.inputTabIndex, 0);
        compare(globalTab.checked, true);
        compare(globalTab.text, "Global");
        compare(devicesTab.text, "Devices");
        compare(gesturesTab.text, "Gestures");
        tabs.currentIndex = 2;
        tryCompare(page, "inputTabIndex", 2);

        const closeTimeout = findChild(page, "inputGestureCloseTimeout");
        const add = findChild(page, "addGestureButton");
        verify(closeTimeout !== null);
        verify(add !== null);
        verify(closeTimeout.implicitHeight >= 44);
        verify(add.implicitHeight >= 44);
        compare(closeTimeout.value, 1000);
        compare(add.enabled, true);
        add.clicked();
        compare(page.draftGestures.length, 2);
        compare(page.draftGestures[1].id, "gesture-1");
        compare(page.draftGestures[1].fingers, 3);
        compare(page.draftGestures[1].modifiers, ["shift"]);
        compare(page.editingGestureId, "gesture-1");

        const editor = findChild(page, "inputGestureEditor");
        const fingers = findChild(page, "gestureFingers");
        const direction = findChild(page, "gestureDirection");
        const action = findChild(page, "gestureAction");
        const modifier = findChild(page, "gestureModifier6");
        const inhibit = findChild(page, "gestureDisableInhibit");
        const done = findChild(page, "closeGestureEditorButton");
        const removeFromEditor = findChild(
            page, "removeGestureFromEditorButton"
        );
        for (const control of [
                fingers, direction, action, modifier, inhibit, done,
                removeFromEditor
            ]) {
            verify(control !== null);
            verify(control.implicitHeight >= 44);
        }
        compare(editor.visible, true);
        compare(action.model.length, 9);
        for (let index = 0; index < action.model.length; ++index)
            verify(String(action.model[index]).toLowerCase() !== "unset");

        fingers.value = 4;
        fingers.valueModified();
        modifier.checked = true;
        modifier.toggled();
        inhibit.checked = true;
        inhibit.toggled();
        compare(page.editingGesture().fingers, 4);
        compare(page.editingGesture().modifiers, ["shift", "super"]);
        compare(page.editingGesture().disableInhibit, true);

        direction.currentIndex = 7;
        direction.activated(7);
        compare(page.editingGesture().direction, "pinch");
        compare(page.editingGesture().scale, 1);
        compare(findChild(page, "gestureScale").visible, false);

        action.currentIndex = 6;
        action.activated(6);
        compare(page.editingGesture().action.type, "scrollMove");
        compare(page.editingGesture().direction, "swipe");
        compare(direction.model.length, 7);

        action.currentIndex = 1;
        action.activated(1);
        const zoomMode = findChild(page, "gestureCursorZoomMode");
        verify(zoomMode !== null);
        verify(zoomMode.implicitHeight >= 44);
        compare(zoomMode.model.length, 3);
        zoomMode.currentIndex = 2;
        zoomMode.activated(2);
        compare(page.editingGesture().action.mode, "live");
        compare(page.editingGesture().direction, "pinch");
        compare(page.editingGesture().scale, 1);
        compare(direction.model.length, 3);

        done.clicked();
        compare(page.editingGestureId, "");
        const editedCard = findChild(page, "gestureCard1");
        const moveUp = findChild(page, "moveGestureUpButton1");
        verify(editedCard !== null);
        verify(moveUp !== null);
        verify(moveUp.implicitHeight >= 44);
        moveUp.clicked();
        compare(page.draftGestures[0].id, "gesture-1");
        const remove = findChild(page, "removeGestureButton0");
        verify(remove !== null);
        verify(remove.implicitHeight >= 44);
        remove.clicked();
        compare(page.draftGestures.length, 1);
        compare(page.draftGestures[0].id, "occupied-default");

        compare(findChild(page, "inputLayoutPreview"), null);
        compare(findChild(page, "gesturePreview"), null);
        compare(findChild(page, "testGestureButton"), null);
        compare(findChild(page, "confirmGestureButton"), null);
    }

    function test_inputGestureCompatibilityIsExactReadOnlyAndBounded() {
        const gestures = compatibilityGestureFixture();
        const compatibility = compatibilityRowsForFixture(gestures);
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(
            page, inputDefaults(), gestures, compatibility
        );
        page.inputTabIndex = 2;
        waitForRendering(page);
        wait(0);

        compare(page.trustedGesturesValid, true);
        compare(page.draftGestures, gestures);
        compare(findChild(page, "editGestureButton0").visible, false);
        compare(findChild(page, "editGestureButton1").visible, true);
        compare(findChild(page, "editGestureButton3").visible, false);
        compare(findChild(page, "editGestureButton4").visible, false);
        compare(findChild(page, "editGestureButton5").visible, false);
        const changedCompatibility = page.clone(gestures[0]);
        changedCompatibility.scale = 1;
        compare(page.replaceGesture("gesture-1", changedCompatibility), false);
        compare(page.draftGestures[0], gestures[0]);
        verify(String(page.gestureCompatibilityForId("gesture-1").reason)
            .includes("retained exactly"));
        for (const index of [0, 3, 4, 5]) {
            const remove = findChild(page, "removeGestureButton" + index);
            verify(remove !== null);
            verify(remove.enabled);
            verify(remove.implicitHeight >= 44);
        }
        const unsetUp = findChild(page, "moveGestureUpButton3");
        const unsetDown = findChild(page, "moveGestureDownButton3");
        verify(unsetUp !== null);
        verify(unsetDown !== null);
        compare(unsetUp.enabled, false);
        compare(unsetDown.enabled, true);
        unsetDown.clicked();
        compare(page.draftGestures[4].id, "unset-row");
        compare(page.draftGestures[3].id, "scroll-pinch");
        compare(page.draftGesturesValid, true);

        const removeCompatibility = findChild(page, "removeGestureButton0");
        removeCompatibility.clicked();
        compare(page.draftGestures.some(record => record.id === "gesture-1"), false);
        const add = findChild(page, "addGestureButton");
        verify(add.enabled);
        add.clicked();
        verify(page.editingGestureId !== "gesture-1");
        compare(page.editingGestureId, "gesture-2");
        compare(page.gestureCompatibilityForId(page.editingGestureId), null);
    }

    function test_inputGestureMax64AndAggregateSaveDiscardResetConflict() {
        const baselineGestures = [
            gestureRecord(
                "baseline-a", 2, "left", [], 1.25, false,
                { type: "workspace" }
            ),
            gestureRecord(
                "baseline-b", 3, "right", ["alt"], 1, true,
                { type: "close" }
            )
        ];
        const baseline = inputDefaults();
        baseline["hyprland.input.repeat_rate"] = 73;
        baseline["hyprland.gestures.close_max_timeout"] = 1444;
        baseline["hyprland.input.touchdevice.transform"] = 4;
        baseline["hyprland.cursor.inactive_timeout"] = 2.37;
        baseline["hyprland.cursor.no_warps"] = true;
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page, baseline, baselineGestures);
        page.inputTabIndex = 2;
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.leftHandedId, true);
        page.setDraftValue(page.tabletRelativeInputId, true);
        page.setDraftValue(page.cursorInactiveTimeoutId, 2.37);
        page.setDraftValue(page.cursorNoWarpsId, true);
        page.setDraftValue(page.cursorPersistentWarpsId, true);
        page.setDraftValue(
            page.cursorWarpBackAfterNonMouseInputId, true
        );
        page.setDraftValue(page.closeGestureTimeoutId, 1555);
        page.removeGesture("baseline-b");
        compare(page.globalDraftDirty, true);
        compare(page.gesturesDraftDirty, true);
        compare(page.draftDirty, true);
        let submittedValues = null;
        let submittedGestures = null;
        let saveCount = 0;
        page.saveRequested.connect(function(values, gestures) {
            ++saveCount;
            submittedValues = values;
            submittedGestures = gestures;
        });
        findChild(page, "saveInputButton").clicked();
        compare(saveCount, 1);
        compare(Object.keys(submittedValues).length, 49);
        compare(submittedValues[page.repeatRateId], 73);
        compare(submittedValues[page.leftHandedId], true);
        compare(submittedValues[page.tabletRelativeInputId], true);
        compare(submittedValues[page.closeGestureTimeoutId], 1555);
        compare(submittedValues[page.touchDeviceTransformId], 4);
        compare(submittedValues[page.cursorInactiveTimeoutId], 2.37);
        compare(submittedValues[page.cursorNoWarpsId], true);
        compare(submittedValues[page.cursorPersistentWarpsId], true);
        compare(submittedValues[
            page.cursorWarpBackAfterNonMouseInputId
        ], true);
        compare(submittedGestures.length, 1);
        compare(submittedGestures[0], baselineGestures[0]);

        const discardWindow = createTemporaryObject(
            inputPageComponent, this
        );
        verify(discardWindow !== null);
        const discardPage = discardWindow.page;
        configureInputPage(discardPage, baseline, baselineGestures);
        discardPage.setDraftValue(discardPage.repeatRateId, 74);
        discardPage.removeGesture("baseline-a");
        findChild(discardPage, "discardInputDraftButton").clicked();
        compare(discardPage.draftValues, baseline);
        compare(discardPage.draftGestures, baselineGestures);
        compare(discardPage.draftDirty, false);

        discardPage.setDraftValue(discardPage.repeatRateId, 74);
        discardPage.removeGesture("baseline-a");
        findChild(discardPage, "resetInputDefaultsButton").clicked();
        compare(discardPage.draftValues, inputDefaults());
        compare(discardPage.draftGestures, []);
        compare(discardPage.draftDirty, true);

        const conflictWindow = createTemporaryObject(
            inputPageComponent, this
        );
        verify(conflictWindow !== null);
        const conflictPage = conflictWindow.page;
        configureInputPage(conflictPage, baseline, baselineGestures);
        conflictPage.setDraftValue(
            conflictPage.cursorPersistentWarpsId, true
        );
        conflictPage.removeGesture("baseline-b");
        const preservedValues = conflictPage.clone(conflictPage.draftValues);
        const preservedDraft = conflictPage.clone(conflictPage.draftGestures);
        const newer = inputDefaults();
        newer[conflictPage.repeatRateId] = 41;
        conflictPage.inputValues = newer;
        conflictPage.inputGestures = [];
        conflictPage.inputGestureCompatibility = [];
        conflictPage.revisionToken = "8";
        wait(0);
        compare(conflictPage.externalChangeWhileEditing, true);
        compare(conflictPage.draftValues, preservedValues);
        compare(conflictPage.draftValue(
            conflictPage.cursorInactiveTimeoutId
        ), 2.37);
        compare(conflictPage.draftValue(
            conflictPage.cursorPersistentWarpsId
        ), true);
        compare(conflictPage.draftGestures, preservedDraft);
        compare(findChild(conflictPage, "saveInputButton").enabled, false);
        compare(findChild(conflictPage, "loadCurrentInputButton").visible, true);

        const maxGestures = [];
        for (let index = 0; index < 64; ++index) {
            const fingers = 2 + Math.floor(index / 8);
            const modifier = [
                "shift", "caps", "ctrl", "alt",
                "mod2", "mod3", "super", "mod5"
            ][index % 8];
            maxGestures.push(gestureRecord(
                "gesture-" + (index + 1), fingers, "left", [modifier],
                1, false, { type: "workspace" }
            ));
        }
        const maxWindow = createTemporaryObject(inputPageComponent, this);
        verify(maxWindow !== null);
        const maxPage = maxWindow.page;
        configureInputPage(maxPage, inputDefaults(), maxGestures);
        maxPage.inputTabIndex = 2;
        waitForRendering(maxPage);
        wait(0);
        const add = findChild(maxPage, "addGestureButton");
        verify(add !== null);
        compare(maxPage.draftGestures.length, 64);
        compare(add.enabled, false);
        add.clicked();
        compare(maxPage.draftGestures.length, 64);
    }

    function test_inputGestureAuthorityFailsClosedWithoutHidingGlobals() {
        const baseline = inputDefaults();
        baseline["hyprland.cursor.inactive_timeout"] = 2.37;
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page, baseline);
        waitForRendering(page);
        wait(0);

        const globalControl = findChild(page, "inputRepeatRate");
        const resolveBindsBySymbol = findChild(
            page, "inputResolveBindsBySymbol"
        );
        const cursorControl = findChild(
            page, "inputCursorInactiveTimeout"
        );
        const status = findChild(page, "inputStatusMessage");
        const save = findChild(page, "saveInputButton");
        verify(globalControl !== null);
        verify(resolveBindsBySymbol !== null);
        verify(cursorControl !== null);
        verify(status !== null);
        verify(save !== null);
        compare(page.trustedValuesValid, true);
        compare(globalControl.visible, true);
        compare(resolveBindsBySymbol.visible, true);
        compare(resolveBindsBySymbol.enabled, true);
        compare(cursorControl.visible, true);
        compare(cursorControl.value, 2.37);

        page.inputGesturesProjectionAvailable = false;
        page.inputGestureActions = [];
        page.inputGestures = [];
        page.inputGestureCompatibility = [];
        page.inputErrorName = "org.example.GestureAuthority";
        page.inputErrorMessage = "Injected gesture authority failure.";
        wait(0);
        compare(page.trustedValuesValid, true);
        compare(page.trustedGesturesValid, false);
        compare(page.controlsEnabled, false);
        compare(globalControl.visible, true);
        compare(globalControl.enabled, false);
        compare(resolveBindsBySymbol.visible, true);
        compare(resolveBindsBySymbol.enabled, false);
        compare(cursorControl.visible, true);
        compare(cursorControl.enabled, false);
        compare(cursorControl.value, 2.37);
        compare(save.enabled, false);
        verify(String(status.text).includes(
            "Gesture authority verification failed"
        ));
        verify(String(status.text).includes(
            "Global values remain readable"
        ));
        verify(String(status.text).includes(
            "Injected gesture authority failure"
        ));

        page.inputGesturesProjectionAvailable = true;
        page.inputGestureActions = inputGestureActions().concat([{
            id: "unset",
            label: "Unset",
            description: "Must never be offered."
        }]);
        page.inputErrorName = "";
        page.inputErrorMessage = "";
        wait(0);
        compare(page.trustedGestureActionsValid, false);
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("trusted Gestures contract"));

        page.inputGestureActions = inputGestureActions();
        page.inputGestureCompatibility = [{
            id: "orphaned",
            editable: false,
            reason: "No exact gesture exists."
        }];
        wait(0);
        compare(page.trustedGesturesValid, false);
        compare(page.controlsEnabled, false);
        page.inputGestureCompatibility = [];
        wait(0);
        compare(page.trustedGesturesValid, true);
        compare(page.controlsEnabled, true);
    }

    function test_inputAllControlsWriteTheExactDraft() {
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page);
        waitForRendering(page);
        wait(0);

        const repeatRate = findChild(page, "inputRepeatRate");
        const repeatDelay = findChild(page, "inputRepeatDelay");
        const numLock = findChild(page, "inputNumLockByDefault");
        const resolveBindsBySymbol = findChild(
            page, "inputResolveBindsBySymbol"
        );
        const virtualKeyboardShareStates = findChild(
            page, "inputVirtualKeyboardShareStates"
        );
        const virtualKeyboardRelease = findChild(
            page, "inputVirtualKeyboardReleasePressedOnClose"
        );
        const virtualKeyboardName = findChild(
            page, "inputVirtualKeyboardNameAfterProcess"
        );
        const sensitivity = findChild(page, "inputSensitivity");
        const acceleration = findChild(
            page, "inputAccelerationProfile"
        );
        const forceNoAccel = findChild(page, "inputForceNoAccel");
        const rotation = findChild(page, "inputRotation");
        const middleClickPaste = findChild(
            page, "inputMiddleClickPaste"
        );
        const cursorHideOnKeyPress = findChild(
            page, "inputCursorHideOnKeyPress"
        );
        const cursorHideOnTouch = findChild(
            page, "inputCursorHideOnTouch"
        );
        const cursorHideOnTablet = findChild(
            page, "inputCursorHideOnTablet"
        );
        const cursorInactiveTimeout = findChild(
            page, "inputCursorInactiveTimeout"
        );
        const cursorHotspotPadding = findChild(
            page, "inputCursorHotspotPadding"
        );
        const cursorNoWarps = findChild(page, "inputCursorNoWarps");
        const cursorPersistentWarps = findChild(
            page, "inputCursorPersistentWarps"
        );
        const cursorWarpBack = findChild(
            page, "inputCursorWarpBackAfterNonMouseInput"
        );
        const naturalScroll = findChild(page, "inputNaturalScroll");
        const leftHanded = findChild(page, "inputLeftHanded");
        const scrollFactor = findChild(page, "inputScrollFactor");
        const tapToClick = findChild(
            page, "inputTouchpadTapToClick"
        );
        const tapAndDrag = findChild(
            page, "inputTouchpadTapAndDrag"
        );
        const touchpadNatural = findChild(
            page, "inputTouchpadNaturalScroll"
        );
        const disableWhileTyping = findChild(
            page, "inputTouchpadDisableWhileTyping"
        );
        const touchpadScroll = findChild(
            page, "inputTouchpadScrollFactor"
        );
        const scrollMethod = findChild(page, "inputScrollMethod");
        const scrollButton = findChild(page, "inputScrollButton");
        const scrollButtonLock = findChild(page, "inputScrollButtonLock");
        const offWindowAxis = findChild(page, "inputOffWindowAxisEvents");
        const discreteScroll = findChild(page, "inputEmulateDiscreteScroll");
        const clickfinger = findChild(
            page, "inputTouchpadClickfingerBehavior"
        );
        const tapButtonMap = findChild(page, "inputTouchpadTapButtonMap");
        const middleButton = findChild(
            page, "inputTouchpadMiddleButtonEmulation"
        );
        const dragLock = findChild(page, "inputTouchpadDragLock");
        const multiFingerDrag = findChild(
            page, "inputTouchpadMultiFingerDrag"
        );
        const flipHorizontal = findChild(
            page, "inputTouchpadFlipHorizontal"
        );
        const flipVertical = findChild(page, "inputTouchpadFlipVertical");
        const touchDeviceEnabled = findChild(
            page, "inputTouchDeviceEnabled"
        );
        const touchDeviceTransform = findChild(
            page, "inputTouchDeviceTransform"
        );
        const tabletRelativeInput = findChild(
            page, "inputTabletRelativeInput"
        );
        const tabletLeftHanded = findChild(
            page, "inputTabletLeftHanded"
        );
        const tabletTransform = findChild(page, "inputTabletTransform");

        const tabletRegionPositionX = findChild(
            page, "inputTabletRegionPositionX"
        );
        const tabletRegionPositionY = findChild(
            page, "inputTabletRegionPositionY"
        );
        const tabletAbsoluteRegionPosition = findChild(
            page, "inputTabletAbsoluteRegionPosition"
        );
        const tabletRegionSizeWidth = findChild(
            page, "inputTabletRegionSizeWidth"
        );
        const tabletRegionSizeHeight = findChild(
            page, "inputTabletRegionSizeHeight"
        );

        clickfinger.checked = true;
        clickfinger.clicked();
        tapButtonMap.currentIndex = 2;
        tapButtonMap.activated(2);
        middleButton.checked = true;
        middleButton.clicked();
        dragLock.currentIndex = 2;
        dragLock.activated(2);
        multiFingerDrag.currentIndex = 2;
        multiFingerDrag.activated(2);
        flipHorizontal.checked = true;
        flipHorizontal.clicked();
        flipVertical.checked = true;
        flipVertical.clicked();
        numLock.checked = true;
        numLock.clicked();
        resolveBindsBySymbol.checked = true;
        resolveBindsBySymbol.clicked();
        virtualKeyboardShareStates.currentIndex = 0;
        virtualKeyboardShareStates.activated(0);
        virtualKeyboardRelease.checked = true;
        virtualKeyboardRelease.clicked();
        virtualKeyboardName.checked = false;
        virtualKeyboardName.clicked();
        touchDeviceEnabled.checked = false;
        touchDeviceEnabled.clicked();
        touchDeviceTransform.value = 6;
        touchDeviceTransform.valueModified();
        tabletRegionPositionX.text = "123.125";
        tabletRegionPositionX.textEdited();
        tabletRegionPositionY.text = "-456.875";
        tabletRegionPositionY.textEdited();
        tabletAbsoluteRegionPosition.checked = true;
        tabletAbsoluteRegionPosition.clicked();
        tabletRegionSizeWidth.text = "-99.5";
        tabletRegionSizeWidth.textEdited();
        tabletRegionSizeHeight.text = "0";
        tabletRegionSizeHeight.textEdited();
        tabletRelativeInput.checked = true;
        tabletRelativeInput.clicked();
        tabletLeftHanded.checked = true;
        tabletLeftHanded.clicked();
        tabletTransform.value = 5;
        tabletTransform.valueModified();

        repeatRate.value = 30;
        repeatRate.valueModified();
        repeatDelay.value = 650;
        repeatDelay.valueModified();
        sensitivity.value = 0.13;
        sensitivity.moved();
        acceleration.currentIndex = 2;
        acceleration.activated(2);
        naturalScroll.checked = true;
        naturalScroll.clicked();
        leftHanded.checked = true;
        leftHanded.clicked();
        scrollFactor.value = 1.14;
        scrollFactor.moved();
        tapToClick.checked = false;
        tapToClick.clicked();
        tapAndDrag.checked = false;
        tapAndDrag.clicked();
        touchpadNatural.checked = true;
        touchpadNatural.clicked();
        disableWhileTyping.checked = false;
        disableWhileTyping.clicked();
        touchpadScroll.value = 0.86;
        touchpadScroll.moved();
        scrollMethod.currentIndex = 3;
        scrollMethod.activated(3);
        compare(scrollButton.enabled, true);
        compare(scrollButtonLock.enabled, true);
        scrollButton.value = 274;
        scrollButton.valueModified();
        scrollButtonLock.checked = true;
        scrollButtonLock.clicked();
        offWindowAxis.currentIndex = 3;
        offWindowAxis.activated(3);
        discreteScroll.currentIndex = 2;
        discreteScroll.activated(2);
        rotation.value = 137;
        rotation.valueModified();
        middleClickPaste.checked = false;
        middleClickPaste.clicked();
        forceNoAccel.checked = true;
        forceNoAccel.clicked();
        cursorHideOnKeyPress.checked = true;
        cursorHideOnKeyPress.clicked();
        cursorHideOnTouch.checked = false;
        cursorHideOnTouch.clicked();
        cursorHideOnTablet.checked = true;
        cursorHideOnTablet.clicked();
        cursorInactiveTimeout.value = 2.36;
        cursorInactiveTimeout.moved();
        cursorHotspotPadding.value = 13;
        cursorHotspotPadding.valueModified();
        cursorNoWarps.checked = true;
        cursorNoWarps.clicked();
        cursorPersistentWarps.checked = true;
        cursorPersistentWarps.clicked();
        cursorWarpBack.checked = true;
        cursorWarpBack.clicked();

        compare(page.draftValue(page.repeatRateId), 30);
        compare(page.draftValue(page.repeatDelayId), 650);
        compare(page.draftValue(page.sensitivityId), 0.15);
        compare(page.draftValue(page.accelerationProfileId), "flat");
        compare(page.draftValue(page.naturalScrollId), true);
        compare(page.draftValue(page.leftHandedId), true);
        compare(page.draftValue(page.tabletRelativeInputId), true);
        compare(page.draftValue(page.scrollFactorId), 1.1);
        compare(page.draftValue(page.touchpadTapToClickId), false);
        compare(page.draftValue(page.touchpadTapAndDragId), false);
        compare(page.draftValue(page.touchpadNaturalScrollId), true);
        compare(page.draftValue(
            page.touchpadDisableWhileTypingId
        ), false);
        compare(page.draftValue(page.touchpadScrollFactorId), 0.9);
        compare(page.draftValue(page.scrollMethodId), "on_button_down");
        compare(page.draftValue(page.scrollButtonId), 274);
        compare(page.draftValue(page.scrollButtonLockId), true);
        compare(page.draftValue(page.offWindowAxisEventsId), 3);
        compare(page.draftValue(page.emulateDiscreteScrollId), 2);
        compare(page.draftValue(page.touchpadClickfingerBehaviorId), true);
        compare(page.draftValue(page.touchpadTapButtonMapId), "lmr");
        compare(page.draftValue(page.touchpadMiddleButtonEmulationId), true);
        compare(page.draftValue(page.touchpadDragLockId), 2);
        compare(page.draftValue(page.touchpadMultiFingerDragId), 2);
        compare(page.draftValue(page.touchpadFlipHorizontalId), true);
        compare(page.draftValue(page.touchpadFlipVerticalId), true);
        compare(page.draftValue(page.numLockByDefaultId), true);
        compare(page.draftValue(page.resolveBindsBySymbolId), true);
        compare(page.draftValue(page.virtualKeyboardShareStatesId), 0);
        compare(page.draftValue(
            page.virtualKeyboardReleasePressedOnCloseId
        ), true);
        compare(page.draftValue(
            page.virtualKeyboardNameAfterProcessId
        ), false);
        compare(page.draftValue(page.forceNoAccelId), true);
        compare(page.draftValue(page.rotationId), 137);
        compare(page.draftValue(page.middleClickPasteId), false);
        compare(page.draftValue(page.cursorHideOnKeyPressId), true);
        compare(page.draftValue(page.cursorHideOnTouchId), false);
        compare(page.draftValue(page.cursorHideOnTabletId), true);
        compare(page.draftValue(page.cursorInactiveTimeoutId), 2.4);
        compare(page.draftValue(page.cursorHotspotPaddingId), 13);
        compare(page.draftValue(page.cursorNoWarpsId), true);
        compare(page.draftValue(page.cursorPersistentWarpsId), true);
        compare(page.draftValue(
            page.cursorWarpBackAfterNonMouseInputId
        ), true);
        compare(cursorPersistentWarps.enabled, true);
        compare(cursorWarpBack.enabled, true);
        compare(page.draftValue(page.touchDeviceEnabledId), false);
        compare(page.draftValue(page.touchDeviceTransformId), 6);
        compare(page.draftValue(page.tabletRelativeInputId), true);
        compare(page.draftValue(page.tabletLeftHandedId), true);
        compare(page.draftValue(page.tabletTransformId), 5);
        compare(page.draftValue(
            page.tabletRegionPositionId
        ), [123.125, -456.875]);
        compare(page.draftValue(
            page.tabletAbsoluteRegionPositionId
        ), true);
        compare(page.draftValue(page.tabletRegionSizeId), [-99.5, 0]);
        compare(sensitivity.enabled, false);
        compare(acceleration.enabled, false);
        compare(page.draftValue(page.sensitivityId), 0.15);
        compare(page.draftValue(page.accelerationProfileId), "flat");
        compare(tapButtonMap.enabled, false);
        compare(dragLock.enabled, false);
        compare(page.draftValue(page.touchpadTapButtonMapId), "lmr");
        compare(page.draftValue(page.touchpadDragLockId), 2);
        scrollMethod.currentIndex = 2;
        scrollMethod.activated(2);
        compare(scrollButton.enabled, false);
        compare(scrollButtonLock.enabled, false);
        compare(page.draftValue(page.scrollButtonId), 274);
        compare(page.draftValue(page.scrollButtonLockId), true);
        compare(page.draftDirty, true);
        compare(page.draftValid, true);
    }

    function test_inputCursorOffGridTimeoutRemainsExactUntilMoved() {
        const baseline = inputDefaults();
        baseline["hyprland.cursor.inactive_timeout"] = 2.37;
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page, baseline);
        waitForRendering(page);
        wait(0);

        const slider = findChild(page, "inputCursorInactiveTimeout");
        const value = findChild(
            page, "inputCursorInactiveTimeoutValue"
        );
        const tabs = findChild(page, "inputTabBar");
        verify(slider !== null);
        verify(value !== null);
        verify(tabs !== null);
        compare(slider.value, 2.37);
        compare(value.text, "2.37 s");

        page.setDraftValue(page.repeatRateId, 30);
        compare(page.draftValue(page.cursorInactiveTimeoutId), 2.37);
        tabs.currentIndex = 1;
        tryCompare(page, "inputTabIndex", 1);
        compare(page.draftValue(page.cursorInactiveTimeoutId), 2.37);
        tabs.currentIndex = 0;
        tryCompare(page, "inputTabIndex", 0);
        compare(slider.value, 2.37);
        compare(value.text, "2.37 s");

        slider.value = 2.36;
        slider.moved();
        compare(page.draftValue(page.cursorInactiveTimeoutId), 2.4);
        compare(value.text, "2.4 s");
    }

    function test_inputTabletMappedRegionExactDraftValidationAndRecovery() {
        const baseline = inputDefaults();
        baseline["hyprland.input.tablet.region_position"] = [1e-7, -2500];
        baseline["hyprland.input.tablet.absolute_region_position"] = false;
        baseline["hyprland.input.tablet.region_size"] = [-50.25, 1e-8];
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page, baseline);
        waitForRendering(page);
        wait(0);

        const positionX = findChild(page, "inputTabletRegionPositionX");
        const positionY = findChild(page, "inputTabletRegionPositionY");
        const positionYValidation = findChild(
            page, "inputTabletRegionPositionYValidation"
        );
        const absolute = findChild(
            page, "inputTabletAbsoluteRegionPosition"
        );
        const width = findChild(page, "inputTabletRegionSizeWidth");
        const height = findChild(page, "inputTabletRegionSizeHeight");
        const aggregateValidation = findChild(
            page, "inputGlobalValuesValidationMessage"
        );
        const save = findChild(page, "saveInputButton");
        const tabs = findChild(page, "inputTabBar");
        for (const control of [positionX, positionY, absolute, width, height]) {
            verify(control !== null);
            verify(control.implicitHeight >= 44);
        }
        verify(positionYValidation !== null);
        verify(aggregateValidation !== null);
        verify(save !== null);
        verify(tabs !== null);
        compare(positionX.maximumLength, 326);
        compare(positionX.text, "0.0000001");
        compare(positionY.text, "-2500");
        compare(width.text, "-50.25");
        compare(height.text, "0.00000001");
        compare(page.valuesEqual(page.draftValues, baseline), true);
        compare(page.globalDraftDirty, false);
        compare(page.draftDirty, false);

        positionX.forceActiveFocus();
        positionX.text = "123.456789";
        positionX.textEdited();
        compare(page.draftValue(page.tabletRegionPositionId), [
            123.456789, -2500
        ]);
        compare(page.inputValues[page.tabletRegionPositionId], [1e-7, -2500]);
        compare(page.globalDraftDirty, true);
        compare(page.draftValuesValid, true);

        positionY.forceActiveFocus();
        positionY.text = "2e3";
        positionY.textEdited();
        compare(page.draftValue(page.tabletRegionPositionId), [
            123.456789, "2e3"
        ]);
        compare(page.draftValuesValid, false);
        compare(page.draftValid, false);
        compare(save.enabled, false);
        compare(positionYValidation.visible, true);
        compare(aggregateValidation.visible, true);
        verify(String(positionYValidation.Accessible.name).includes(
            "plain decimal"
        ));
        compare(aggregateValidation.Accessible.role,
            Accessible.AlertMessage);
        tabs.currentIndex = 1;
        tryCompare(page, "inputTabIndex", 1);
        tabs.currentIndex = 0;
        tryCompare(page, "inputTabIndex", 0);
        compare(positionY.text, "2e3");

        positionY.forceActiveFocus();
        positionY.text = "-19999.999999";
        positionY.textEdited();
        width.forceActiveFocus();
        width.text = "-99.999999";
        width.textEdited();
        height.forceActiveFocus();
        height.text = "0";
        height.textEdited();
        absolute.checked = true;
        absolute.clicked();
        compare(page.draftValue(page.tabletRegionPositionId), [
            123.456789, -19999.999999
        ]);
        compare(page.draftValue(page.tabletRegionSizeId), [-99.999999, 0]);
        compare(page.draftValue(page.tabletAbsoluteRegionPositionId), true);
        compare(page.draftValuesValid, true);
        compare(positionYValidation.visible, false);
        compare(aggregateValidation.visible, false);
        compare(save.enabled, true);

        let submitted = null;
        page.saveRequested.connect(function(values) {
            submitted = values;
        });
        save.clicked();
        verify(submitted !== null);
        compare(submitted[page.tabletRegionPositionId], [
            123.456789, -19999.999999
        ]);
        compare(submitted[page.tabletAbsoluteRegionPositionId], true);
        compare(submitted[page.tabletRegionSizeId], [-99.999999, 0]);

        const discardWindow = createTemporaryObject(inputPageComponent, this);
        verify(discardWindow !== null);
        const discardPage = discardWindow.page;
        configureInputPage(discardPage, baseline);
        wait(0);
        const discardX = findChild(
            discardPage, "inputTabletRegionPositionX"
        );
        discardX.text = "not-a-decimal";
        discardX.textEdited();
        compare(discardPage.draftValuesValid, false);
        findChild(discardPage, "discardInputDraftButton").clicked();
        compare(discardPage.draftValues, baseline);
        compare(discardPage.draftValuesValid, true);
        compare(discardPage.draftDirty, false);

        discardPage.setExactVectorComponentDraftValue(
            discardPage.tabletRegionSizeId, 1, 2048.125
        );
        findChild(discardPage, "resetInputDefaultsButton").clicked();
        compare(discardPage.draftValues, inputDefaults());
        compare(discardPage.draftValue(
            discardPage.tabletRegionPositionId
        ), [0, 0]);
        compare(discardPage.draftValue(discardPage.tabletRegionSizeId), [0, 0]);
        compare(discardPage.draftValue(
            discardPage.tabletAbsoluteRegionPositionId
        ), false);

        const conflictWindow = createTemporaryObject(
            inputPageComponent, this
        );
        verify(conflictWindow !== null);
        const conflictPage = conflictWindow.page;
        configureInputPage(conflictPage, baseline);
        conflictPage.setExactVectorComponentDraftValue(
            conflictPage.tabletRegionPositionId, 0, 777.125
        );
        const newer = inputDefaults();
        newer[conflictPage.tabletRegionPositionId] = [-10.5, 20.25];
        newer[conflictPage.tabletAbsoluteRegionPositionId] = true;
        newer[conflictPage.tabletRegionSizeId] = [1920.5, 1080.25];
        conflictPage.inputValues = newer;
        conflictPage.revisionToken = "8";
        wait(0);
        compare(conflictPage.externalChangeWhileEditing, true);
        compare(conflictPage.draftValue(
            conflictPage.tabletRegionPositionId
        ), [777.125, -2500]);
        const load = findChild(conflictPage, "loadCurrentInputButton");
        verify(load.visible);
        load.clicked();
        compare(conflictPage.externalChangeWhileEditing, false);
        compare(conflictPage.draftValues, newer);
        compare(conflictPage.draftDirty, false);
    }

    function test_inputDraftActionsPreserveExactValues() {
        const baseline = inputDefaults();
        baseline["hyprland.input.repeat_rate"] = 72;
        baseline["hyprland.input.sensitivity"] = 0.07;
        baseline["hyprland.input.scroll_factor"] = 1.03;
        baseline["hyprland.input.touchpad.scroll_factor"] = 0.97;
        baseline["hyprland.input.scroll_method"] = "on_button_down";
        baseline["hyprland.input.scroll_button"] = 274;
        baseline["hyprland.input.scroll_button_lock"] = true;
        baseline["hyprland.input.off_window_axis_events"] = 3;
        baseline["hyprland.input.emulate_discrete_scroll"] = 2;
        baseline["hyprland.input.touchpad.clickfinger_behavior"] = true;
        baseline["hyprland.input.touchpad.drag_3fg"] = 2;
        baseline["hyprland.input.touchpad.drag_lock"] = 2;
        baseline["hyprland.input.touchpad.flip_x"] = true;
        baseline["hyprland.input.touchpad.flip_y"] = true;
        baseline["hyprland.input.touchpad.middle_button_emulation"] = true;
        baseline["hyprland.input.touchpad.tap_button_map"] = "lmr";
        baseline["hyprland.input.numlock_by_default"] = true;
        baseline["hyprland.input.virtualkeyboard.share_states"] = 0;
        baseline["hyprland.input.virtualkeyboard.release_pressed_on_close"] = true;
        baseline["hyprland.misc.name_vk_after_proc"] = false;
        baseline["hyprland.input.force_no_accel"] = true;
        baseline["hyprland.input.rotation"] = 137;
        baseline["hyprland.misc.middle_click_paste"] = false;
        baseline["hyprland.gestures.close_max_timeout"] = 1450;
        baseline["hyprland.input.touchdevice.enabled"] = false;
        baseline["hyprland.input.touchdevice.transform"] = 6;
        baseline["hyprland.input.tablet.relative_input"] = true;
        baseline["hyprland.input.tablet.left_handed"] = true;
        baseline["hyprland.input.tablet.transform"] = 5;
        baseline["hyprland.cursor.hide_on_key_press"] = true;
        baseline["hyprland.cursor.hide_on_touch"] = false;
        baseline["hyprland.cursor.hide_on_tablet"] = true;
        baseline["hyprland.cursor.inactive_timeout"] = 2.37;
        baseline["hyprland.cursor.hotspot_padding"] = 13;
        baseline["hyprland.cursor.no_warps"] = true;
        baseline["hyprland.cursor.persistent_warps"] = true;
        baseline["hyprland.cursor.warp_back_after_non_mouse_input"] = true;
        baseline["hyprland.input.tablet.region_position"] = [
            123.456789, -987.654321
        ];
        baseline["hyprland.input.tablet.absolute_region_position"] = true;
        baseline["hyprland.input.tablet.region_size"] = [-99.999999, 0];
        baseline["hyprland.input.resolve_binds_by_sym"] = true;
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page, baseline);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.leftHandedId, true);
        compare(page.draftDirty, true);
        let saveCount = 0;
        let submitted = null;
        page.saveRequested.connect(function(values) {
            ++saveCount;
            submitted = values;
        });
        const save = findChild(page, "saveInputButton");
        verify(save !== null);
        save.clicked();
        compare(saveCount, 1);
        compare(Object.keys(submitted).length, 49);
        compare(submitted[page.repeatRateId], 72);
        compare(submitted[page.sensitivityId], 0.07);
        compare(submitted[page.scrollFactorId], 1.03);
        compare(submitted[page.touchpadScrollFactorId], 0.97);
        compare(submitted[page.scrollMethodId], "on_button_down");
        compare(submitted[page.scrollButtonId], 274);
        compare(submitted[page.scrollButtonLockId], true);
        compare(submitted[page.offWindowAxisEventsId], 3);
        compare(submitted[page.emulateDiscreteScrollId], 2);
        compare(submitted[page.touchpadClickfingerBehaviorId], true);
        compare(submitted[page.touchpadMultiFingerDragId], 2);
        compare(submitted[page.touchpadDragLockId], 2);
        compare(submitted[page.touchpadFlipHorizontalId], true);
        compare(submitted[page.touchpadFlipVerticalId], true);
        compare(submitted[page.touchpadMiddleButtonEmulationId], true);
        compare(submitted[page.touchpadTapButtonMapId], "lmr");
        compare(submitted[page.numLockByDefaultId], true);
        compare(submitted[page.virtualKeyboardShareStatesId], 0);
        compare(submitted[
            page.virtualKeyboardReleasePressedOnCloseId
        ], true);
        compare(submitted[page.virtualKeyboardNameAfterProcessId], false);
        compare(submitted[page.forceNoAccelId], true);
        compare(submitted[page.rotationId], 137);
        compare(submitted[page.middleClickPasteId], false);
        compare(submitted[page.closeGestureTimeoutId], 1450);
        compare(submitted[page.touchDeviceEnabledId], false);
        compare(submitted[page.touchDeviceTransformId], 6);
        compare(submitted[page.tabletRelativeInputId], true);
        compare(submitted[page.tabletLeftHandedId], true);
        compare(submitted[page.tabletTransformId], 5);
        compare(submitted[page.cursorHideOnKeyPressId], true);
        compare(submitted[page.cursorHideOnTouchId], false);
        compare(submitted[page.cursorHideOnTabletId], true);
        compare(submitted[page.cursorInactiveTimeoutId], 2.37);
        compare(submitted[page.cursorHotspotPaddingId], 13);
        compare(submitted[page.cursorNoWarpsId], true);
        compare(submitted[page.cursorPersistentWarpsId], true);
        compare(submitted[
            page.cursorWarpBackAfterNonMouseInputId
        ], true);
        compare(submitted[page.tabletRegionPositionId], [
            123.456789, -987.654321
        ]);
        compare(submitted[page.tabletAbsoluteRegionPositionId], true);
        compare(submitted[page.tabletRegionSizeId], [-99.999999, 0]);
        compare(submitted[page.resolveBindsBySymbolId], true);
        compare(submitted[page.leftHandedId], true);
        save.clicked();
        compare(saveCount, 1);

        const secondWindow = createTemporaryObject(
            inputPageComponent, this
        );
        verify(secondWindow !== null);
        const secondPage = secondWindow.page;
        configureInputPage(secondPage, baseline);
        waitForRendering(secondPage);
        wait(0);
        secondPage.setDraftValue(secondPage.repeatRateId, 90);
        secondPage.setDraftValue(
            secondPage.resolveBindsBySymbolId, false
        );
        compare(secondPage.draftDirty, true);
        findChild(secondPage, "discardInputDraftButton").clicked();
        compare(secondPage.draftDirty, false);
        compare(secondPage.draftValue(secondPage.repeatRateId), 72);
        compare(secondPage.draftValue(secondPage.sensitivityId), 0.07);
        compare(secondPage.draftValue(secondPage.forceNoAccelId), true);
        compare(secondPage.draftValue(secondPage.rotationId), 137);
        compare(secondPage.draftValue(secondPage.middleClickPasteId), false);
        compare(secondPage.draftValue(secondPage.touchDeviceEnabledId), false);
        compare(secondPage.draftValue(secondPage.touchDeviceTransformId), 6);
        compare(secondPage.draftValue(secondPage.tabletRelativeInputId), true);
        compare(secondPage.draftValue(secondPage.tabletLeftHandedId), true);
        compare(secondPage.draftValue(secondPage.tabletTransformId), 5);
        compare(secondPage.draftValue(
            secondPage.cursorHideOnKeyPressId
        ), true);
        compare(secondPage.draftValue(
            secondPage.cursorHideOnTouchId
        ), false);
        compare(secondPage.draftValue(
            secondPage.cursorHideOnTabletId
        ), true);
        compare(secondPage.draftValue(
            secondPage.cursorInactiveTimeoutId
        ), 2.37);
        compare(secondPage.draftValue(
            secondPage.cursorHotspotPaddingId
        ), 13);
        compare(secondPage.draftValue(secondPage.cursorNoWarpsId), true);
        compare(secondPage.draftValue(
            secondPage.cursorPersistentWarpsId
        ), true);
        compare(secondPage.draftValue(
            secondPage.cursorWarpBackAfterNonMouseInputId
        ), true);
        compare(secondPage.draftValue(
            secondPage.resolveBindsBySymbolId
        ), true);

        findChild(secondPage, "resetInputDefaultsButton").clicked();
        compare(secondPage.draftDirty, true);
        compare(secondPage.draftValue(secondPage.repeatRateId), 25);
        compare(secondPage.draftValue(secondPage.repeatDelayId), 600);
        compare(secondPage.draftValue(secondPage.sensitivityId), 0);
        compare(secondPage.draftValue(
            secondPage.accelerationProfileId
        ), "");
        compare(secondPage.draftValue(secondPage.scrollFactorId), 1);
        compare(secondPage.draftValue(
            secondPage.touchpadTapToClickId
        ), true);
        compare(secondPage.draftValue(
            secondPage.touchpadDisableWhileTypingId
        ), true);
        compare(secondPage.draftValue(
            secondPage.touchpadScrollFactorId
        ), 1);
        compare(secondPage.draftValue(secondPage.scrollMethodId), "");
        compare(secondPage.draftValue(secondPage.scrollButtonId), 0);
        compare(secondPage.draftValue(secondPage.scrollButtonLockId), false);
        compare(secondPage.draftValue(secondPage.offWindowAxisEventsId), 1);
        compare(secondPage.draftValue(secondPage.emulateDiscreteScrollId), 1);
        compare(secondPage.draftValue(
            secondPage.touchpadClickfingerBehaviorId
        ), false);
        compare(secondPage.draftValue(
            secondPage.touchpadMultiFingerDragId
        ), 0);
        compare(secondPage.draftValue(secondPage.touchpadDragLockId), 0);
        compare(secondPage.draftValue(
            secondPage.touchpadFlipHorizontalId
        ), false);
        compare(secondPage.draftValue(
            secondPage.touchpadFlipVerticalId
        ), false);
        compare(secondPage.draftValue(
            secondPage.touchpadMiddleButtonEmulationId
        ), false);
        compare(secondPage.draftValue(secondPage.touchpadTapButtonMapId), "");
        compare(secondPage.draftValue(secondPage.numLockByDefaultId), false);
        compare(secondPage.draftValue(
            secondPage.virtualKeyboardShareStatesId
        ), 2);
        compare(secondPage.draftValue(
            secondPage.virtualKeyboardReleasePressedOnCloseId
        ), false);
        compare(secondPage.draftValue(
            secondPage.virtualKeyboardNameAfterProcessId
        ), true);
        compare(secondPage.draftValue(secondPage.forceNoAccelId), false);
        compare(secondPage.draftValue(secondPage.rotationId), 0);
        compare(secondPage.draftValue(secondPage.middleClickPasteId), true);
        compare(secondPage.draftValue(secondPage.touchDeviceEnabledId), true);
        compare(secondPage.draftValue(secondPage.touchDeviceTransformId), 0);
        compare(secondPage.draftValue(secondPage.tabletRelativeInputId), false);
        compare(secondPage.draftValue(secondPage.tabletLeftHandedId), false);
        compare(secondPage.draftValue(secondPage.tabletTransformId), 0);
        compare(secondPage.draftValue(
            secondPage.cursorHideOnKeyPressId
        ), false);
        compare(secondPage.draftValue(
            secondPage.cursorHideOnTouchId
        ), true);
        compare(secondPage.draftValue(
            secondPage.cursorHideOnTabletId
        ), false);
        compare(secondPage.draftValue(
            secondPage.cursorInactiveTimeoutId
        ), 0);
        compare(secondPage.draftValue(
            secondPage.cursorHotspotPaddingId
        ), 0);
        compare(secondPage.draftValue(secondPage.cursorNoWarpsId), false);
        compare(secondPage.draftValue(
            secondPage.cursorPersistentWarpsId
        ), false);
        compare(secondPage.draftValue(
            secondPage.cursorWarpBackAfterNonMouseInputId
        ), false);
        compare(secondPage.draftValue(
            secondPage.tabletRegionPositionId
        ), [0, 0]);
        compare(secondPage.draftValue(
            secondPage.tabletAbsoluteRegionPositionId
        ), false);
        compare(secondPage.draftValue(
            secondPage.tabletRegionSizeId
        ), [0, 0]);
        compare(secondPage.draftValue(
            secondPage.resolveBindsBySymbolId
        ), false);
    }

    function test_inputRejectsBadDefinitionsValuesAndNonfiniteNumbers() {
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page);
        waitForRendering(page);
        wait(0);

        const definitions = inputDefinitions();
        definitions[0].max = 201;
        page.inputOptions = definitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const advancedDefinitions = inputDefinitions();
        advancedDefinitions[12].choices[3].value = "button";
        page.inputOptions = advancedDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const touchpadDefinitions = inputDefinitions();
        touchpadDefinitions[18].choices[1].value = 2;
        page.inputOptions = touchpadDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const virtualKeyboardDefinitions = inputDefinitions();
        virtualKeyboardDefinitions[25].choices[1].value = 2;
        page.inputOptions = virtualKeyboardDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const pointerDefinitions = inputDefinitions();
        pointerDefinitions[29].max = 360;
        page.inputOptions = pointerDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const touchEnabledDefinitions = inputDefinitions();
        touchEnabledDefinitions[32].defaultValue = false;
        page.inputOptions = touchEnabledDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const touchTransformDefinitions = inputDefinitions();
        touchTransformDefinitions[33].max = 7;
        page.inputOptions = touchTransformDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const tabletRelativeDefinitions = inputDefinitions();
        tabletRelativeDefinitions[34].type = "integer";
        page.inputOptions = tabletRelativeDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const tabletLeftDefinitions = inputDefinitions();
        tabletLeftDefinitions[35].defaultValue = true;
        page.inputOptions = tabletLeftDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const tabletTransformDefinitions = inputDefinitions();
        tabletTransformDefinitions[36].min = -1;
        page.inputOptions = tabletTransformDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const cursorDefinitionMutations = [
            { index: 37, field: "defaultValue", value: true },
            { index: 38, field: "defaultValue", value: false },
            { index: 39, field: "type", value: "integer" },
            { index: 40, field: "max", value: 21 },
            { index: 41, field: "min", value: -1 },
            { index: 42, field: "control", value: "select" },
            { index: 43, field: "defaultValue", value: true },
            { index: 44, field: "id", value: "hyprland.cursor.unknown" }
        ];
        for (const mutation of cursorDefinitionMutations) {
            const mutated = inputDefinitions();
            mutated[mutation.index][mutation.field] = mutation.value;
            page.inputOptions = mutated;
            wait(0);
            compare(page.trustedDefinitionsValid, false);
            compare(page.controlsEnabled, false);
        }
        const reorderedCursorDefinitions = inputDefinitions();
        const reorderedCursor = reorderedCursorDefinitions[37];
        reorderedCursorDefinitions[37] = reorderedCursorDefinitions[38];
        reorderedCursorDefinitions[38] = reorderedCursor;
        page.inputOptions = reorderedCursorDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const mappedRegionDefinitionMutations = [
            { index: 45, field: "type", value: "number" },
            { index: 45, field: "control", value: "slider" },
            { index: 45, field: "defaultValue", value: [0, 1] },
            { index: 45, field: "min", value: [-19999, -20000] },
            { index: 45, field: "max", value: [20000, 19999] },
            { index: 46, field: "defaultValue", value: true },
            { index: 47, field: "defaultValue", value: [1, 0] },
            { index: 47, field: "min", value: [-99, -100] },
            { index: 47, field: "max", value: [4000, 3999] }
        ];
        for (const mutation of mappedRegionDefinitionMutations) {
            const mutated = inputDefinitions();
            mutated[mutation.index][mutation.field] = mutation.value;
            page.inputOptions = mutated;
            wait(0);
            compare(page.trustedDefinitionsValid, false);
            compare(page.controlsEnabled, false);
        }
        const reorderedMappedRegionDefinitions = inputDefinitions();
        const reorderedMappedRegion = reorderedMappedRegionDefinitions[45];
        reorderedMappedRegionDefinitions[45] =
            reorderedMappedRegionDefinitions[46];
        reorderedMappedRegionDefinitions[46] = reorderedMappedRegion;
        page.inputOptions = reorderedMappedRegionDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const resolveBindsDefinition = inputDefinitions();
        resolveBindsDefinition[48].defaultValue = true;
        page.inputOptions = resolveBindsDefinition;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        page.inputOptions = inputDefinitions();
        const badMaps = [];
        let invalid = inputDefaults();
        delete invalid[page.repeatRateId];
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid["hyprland.input.unknown"] = true;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.repeatRateId] = 25.5;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.sensitivityId] = NaN;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.sensitivityId] = Infinity;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.sensitivityId] = -Infinity;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.sensitivityId] = 1.01;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.scrollFactorId] = -0.01;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.accelerationProfileId] = "automatic";
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.naturalScrollId] = 1;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.scrollMethodId] = "button";
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.scrollButtonId] = 301;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.scrollButtonId] = 1.5;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.scrollButtonLockId] = 1;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.offWindowAxisEventsId] = 4;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.offWindowAxisEventsId] = 1.5;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.emulateDiscreteScrollId] = "1";
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.emulateDiscreteScrollId] = NaN;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.touchpadMultiFingerDragId] = 3;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.touchpadDragLockId] = 1.5;
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.touchpadTapButtonMapId] = "automatic";
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.touchpadClickfingerBehaviorId] = 1;
        badMaps.push(invalid);
        for (const invalidValue of [-1, 3, 1.5, "2", true, NaN, Infinity]) {
            invalid = inputDefaults();
            invalid[page.virtualKeyboardShareStatesId] = invalidValue;
            badMaps.push(invalid);
        }
        for (const id of [
                 page.numLockByDefaultId,
                 page.virtualKeyboardReleasePressedOnCloseId,
                 page.virtualKeyboardNameAfterProcessId,
                 page.forceNoAccelId,
                 page.middleClickPasteId,
                 page.touchDeviceEnabledId,
                 page.tabletRelativeInputId,
                 page.tabletLeftHandedId,
                 page.cursorHideOnKeyPressId,
                 page.cursorHideOnTouchId,
                 page.cursorHideOnTabletId,
                 page.cursorNoWarpsId,
                 page.cursorPersistentWarpsId,
                 page.cursorWarpBackAfterNonMouseInputId,
                 page.tabletAbsoluteRegionPositionId,
                 page.resolveBindsBySymbolId
             ]) {
            invalid = inputDefaults();
            invalid[id] = 1;
            badMaps.push(invalid);
            invalid = inputDefaults();
            invalid[id] = "false";
            badMaps.push(invalid);
        }
        for (const invalidValue of [
                 -1, 360, 137.5, "137", true, NaN, Infinity, -Infinity
             ]) {
            invalid = inputDefaults();
            invalid[page.rotationId] = invalidValue;
            badMaps.push(invalid);
        }
        for (const invalidValue of [
                 9, 2001, 10.5, "1000", true, NaN, Infinity
             ]) {
            invalid = inputDefaults();
            invalid[page.closeGestureTimeoutId] = invalidValue;
            badMaps.push(invalid);
        }
        for (const id of [
                 page.touchDeviceTransformId,
                 page.tabletTransformId,
                 page.cursorHotspotPaddingId
             ]) {
            for (const invalidValue of [
                     -1,
                     id === page.cursorHotspotPaddingId ? 21 : 7,
                     1.5, "2", true, NaN, Infinity, -Infinity
                 ]) {
                invalid = inputDefaults();
                invalid[id] = invalidValue;
                badMaps.push(invalid);
            }
        }
        for (const invalidValue of [
                 -0.1, 20.1, "2.37", true, NaN, Infinity, -Infinity
             ]) {
            invalid = inputDefaults();
            invalid[page.cursorInactiveTimeoutId] = invalidValue;
            badMaps.push(invalid);
        }
        for (const id of [
                 page.tabletRegionPositionId,
                 page.tabletRegionSizeId
             ]) {
            for (const invalidValue of [
                     true,
                     "0 0",
                     [],
                     [0],
                     [0, 0, 0],
                     ["0", 0],
                     [NaN, 0],
                     [0, Infinity],
                     [0, -Infinity]
                 ]) {
                invalid = inputDefaults();
                invalid[id] = invalidValue;
                badMaps.push(invalid);
            }
        }
        invalid = inputDefaults();
        invalid[page.tabletRegionPositionId] = [-20000.000001, 0];
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.tabletRegionPositionId] = [0, 20000.000001];
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.tabletRegionSizeId] = [-100.000001, 0];
        badMaps.push(invalid);
        invalid = inputDefaults();
        invalid[page.tabletRegionSizeId] = [0, 4000.000001];
        badMaps.push(invalid);
        for (const values of badMaps)
            compare(page.validateValues(values), false);

        const arbitrary = inputDefaults();
        arbitrary[page.sensitivityId] = 0.07;
        arbitrary[page.scrollFactorId] = 1.03;
        arbitrary[page.touchpadScrollFactorId] = 0.97;
        arbitrary[page.rotationId] = 137;
        arbitrary[page.cursorInactiveTimeoutId] = 2.37;
        arbitrary[page.cursorHotspotPaddingId] = 13;
        arbitrary[page.tabletRegionPositionId] = [
            123.456789, -987.654321
        ];
        arbitrary[page.tabletAbsoluteRegionPositionId] = true;
        arbitrary[page.tabletRegionSizeId] = [-99.999999, 0];
        compare(page.validateValues(arbitrary), true);
        page.inputValues = arbitrary;
        wait(0);
        page.reviewProjection();
        compare(page.trustedValuesValid, true);

        const invalidValues = inputDefaults();
        invalidValues[page.touchpadScrollFactorId] = 2.01;
        page.inputValues = invalidValues;
        wait(0);
        compare(page.trustedValuesValid, false);
        compare(page.controlsEnabled, false);
        compare(findChild(page, "saveInputButton").enabled, false);
        verify(String(findChild(page, "inputStatusMessage").text)
            .includes("trusted Input contract"));
    }

    function test_inputExternalRevisionPreservesDraftLosslessly() {
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page);
        page.revisionToken = "9007199254740992";
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.repeatRateId, 88);
        page.setDraftValue(page.sensitivityId, 0.07);
        page.setDraftValue(page.tabletTransformId, 6);
        page.setExactVectorComponentDraftValue(
            page.tabletRegionPositionId, 0, 123.456789
        );
        page.setExactVectorComponentDraftValue(
            page.tabletRegionSizeId, 1, 1080.25
        );
        page.setDraftValue(page.tabletAbsoluteRegionPositionId, true);
        page.setDraftValue(page.cursorInactiveTimeoutId, 2.37);
        page.setDraftValue(page.cursorPersistentWarpsId, true);
        page.setDraftValue(page.resolveBindsBySymbolId, true);
        compare(page.draftDirty, true);
        compare(page.synchronizedRevisionToken, "9007199254740992");

        const newer = inputDefaults();
        newer[page.repeatRateId] = 40;
        newer[page.scrollFactorId] = 1.03;
        newer[page.touchDeviceEnabledId] = false;
        newer[page.tabletRegionPositionId] = [-10.5, 20.25];
        newer[page.tabletAbsoluteRegionPositionId] = false;
        newer[page.tabletRegionSizeId] = [1920.5, 1080.25];
        newer[page.cursorInactiveTimeoutId] = 4.2;
        newer[page.cursorNoWarpsId] = true;
        newer[page.resolveBindsBySymbolId] = false;
        page.inputValues = newer;
        page.revisionToken = "9007199254740993";
        wait(0);

        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.repeatRateId), 88);
        compare(page.draftValue(page.sensitivityId), 0.07);
        compare(page.draftValue(page.tabletTransformId), 6);
        compare(page.draftValue(page.tabletRegionPositionId), [
            123.456789, 0
        ]);
        compare(page.draftValue(page.tabletAbsoluteRegionPositionId), true);
        compare(page.draftValue(page.tabletRegionSizeId), [0, 1080.25]);
        compare(page.draftValue(page.cursorInactiveTimeoutId), 2.37);
        compare(page.draftValue(page.cursorPersistentWarpsId), true);
        compare(page.draftValue(page.resolveBindsBySymbolId), true);
        compare(page.synchronizedRevisionToken, "9007199254740992");
        compare(findChild(page, "saveInputButton").enabled, false);
        verify(String(findChild(page, "inputStatusMessage").text)
            .includes("draft is preserved"));

        const loadCurrent = findChild(page, "loadCurrentInputButton");
        compare(loadCurrent.visible, true);
        loadCurrent.clicked();
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.repeatRateId), 40);
        compare(page.draftValue(page.scrollFactorId), 1.03);
        compare(page.draftValue(page.touchDeviceEnabledId), false);
        compare(page.draftValue(page.tabletTransformId), 0);
        compare(page.draftValue(page.tabletRegionPositionId), [-10.5, 20.25]);
        compare(page.draftValue(page.tabletAbsoluteRegionPositionId), false);
        compare(page.draftValue(page.tabletRegionSizeId), [1920.5, 1080.25]);
        compare(page.draftValue(page.cursorInactiveTimeoutId), 4.2);
        compare(page.draftValue(page.cursorNoWarpsId), true);
        compare(page.draftValue(page.cursorPersistentWarpsId), false);
        compare(page.draftValue(page.resolveBindsBySymbolId), false);
        compare(page.synchronizedRevisionToken, "9007199254740993");
    }

    function test_inputOwnSavedRevisionReconcilesAfterApplyFailure() {
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.leftHandedId, true);
        page.setDraftValue(page.tabletRelativeInputId, true);
        page.setDraftValue(page.cursorInactiveTimeoutId, 2.37);
        page.setDraftValue(page.cursorNoWarpsId, true);
        page.setDraftValue(page.cursorPersistentWarpsId, true);
        page.setDraftValue(page.resolveBindsBySymbolId, true);
        const submitted = page.clone(page.draftValues);
        let requestCount = 0;
        page.saveRequested.connect(function() { ++requestCount; });
        page.submitDraft();
        compare(requestCount, 1);
        compare(page.saveSubmitted, true);

        page.busyOperation = "input-save";
        page.busy = true;
        page.serviceAvailable = false;
        page.inputProjectionAvailable = false;
        page.inputAvailable = false;
        page.inputValues = ({});
        page.revisionToken = "8";
        page.busy = false;
        page.busyOperation = "";
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, true);
        compare(page.draftValue(page.leftHandedId), true);
        compare(page.draftValue(page.tabletRelativeInputId), true);
        compare(page.draftValue(page.cursorInactiveTimeoutId), 2.37);
        compare(page.draftValue(page.cursorNoWarpsId), true);
        compare(page.draftValue(page.cursorPersistentWarpsId), true);
        compare(page.draftValue(page.resolveBindsBySymbolId), true);

        page.serviceAvailable = true;
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, true);
        compare(page.draftValue(page.leftHandedId), true);
        compare(page.draftValue(page.tabletRelativeInputId), true);
        compare(page.draftValue(page.cursorInactiveTimeoutId), 2.37);
        compare(page.draftValue(page.cursorNoWarpsId), true);
        compare(page.draftValue(page.cursorPersistentWarpsId), true);
        compare(page.draftValue(page.resolveBindsBySymbolId), true);

        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.inputValues = submitted;
        page.inputProjectionAvailable = true;
        wait(0);
        compare(page.inputAvailable, false);
        compare(page.inputProjectionAvailable, true);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, false);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.leftHandedId), true);
        compare(page.draftValue(page.tabletRelativeInputId), true);
        compare(page.draftValue(page.cursorInactiveTimeoutId), 2.37);
        compare(page.draftValue(page.cursorNoWarpsId), true);
        compare(page.draftValue(page.cursorPersistentWarpsId), true);
        compare(page.draftValue(page.resolveBindsBySymbolId), true);
        compare(page.synchronizedRevisionToken, "8");
        const retry = findChild(page, "retryApplyInputButton");
        verify(retry !== null);
        compare(retry.visible, true);
        compare(retry.enabled, true);
    }

    function test_untrustedStaleValidMapsNeverInitializeDrafts() {
        const appearanceWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        const inputWindow = createTemporaryObject(inputPageComponent, this);
        const windowsWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(appearanceWindow !== null);
        verify(inputWindow !== null);
        verify(windowsWindow !== null);
        const appearance = appearanceWindow.page;
        const input = inputWindow.page;
        const windows = windowsWindow.page;
        configureAppearancePage(appearance);
        configureInputPage(input);
        configureWindowsPage(windows);

        appearance.appearanceProjectionAvailable = false;
        appearance.appearanceAvailable = false;
        appearance.projectionInitialized = false;
        appearance.draftValues = ({});
        appearance.synchronizedValues = ({});
        appearance.synchronizedRevisionToken = "";
        input.inputProjectionAvailable = false;
        input.inputAvailable = false;
        input.projectionInitialized = false;
        input.draftValues = ({});
        input.synchronizedValues = ({});
        input.synchronizedRevisionToken = "";
        windows.windowsProjectionAvailable = false;
        windows.windowsAvailable = false;
        windows.projectionInitialized = false;
        windows.draftValues = ({});
        windows.synchronizedValues = ({});
        windows.synchronizedRevisionToken = "";
        appearance.reviewProjection();
        input.reviewProjection();
        windows.reviewProjection();
        wait(0);
        compare(appearance.trustedValuesValid, false);
        compare(input.trustedValuesValid, false);
        compare(windows.trustedValuesValid, false);
        compare(appearance.projectionInitialized, false);
        compare(input.projectionInitialized, false);
        compare(windows.projectionInitialized, false);
        compare(Object.keys(appearance.draftValues).length, 0);
        compare(Object.keys(input.draftValues).length, 0);
        compare(Object.keys(windows.draftValues).length, 0);

        appearance.appearanceProjectionAvailable = true;
        input.inputProjectionAvailable = true;
        windows.windowsProjectionAvailable = true;
        appearance.reviewProjection();
        input.reviewProjection();
        windows.reviewProjection();
        compare(appearance.appearanceAvailable, false);
        compare(input.inputAvailable, false);
        compare(windows.windowsAvailable, false);
        compare(appearance.trustedValuesValid, true);
        compare(input.trustedValuesValid, true);
        compare(windows.trustedValuesValid, true);
        compare(appearance.projectionInitialized, true);
        compare(input.projectionInitialized, true);
        compare(windows.projectionInitialized, true);
        compare(Object.keys(appearance.draftValues).length, 40);
        compare(Object.keys(input.draftValues).length, 49);
        compare(Object.keys(windows.draftValues).length, 110);
    }

    function test_inputGatesUnsafeStatesAndScopesErrors() {
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page);
        waitForRendering(page);
        wait(0);
        const control = findChild(page, "inputRepeatRate");
        const save = findChild(page, "saveInputButton");
        const status = findChild(page, "inputStatusMessage");
        verify(control !== null);
        verify(save !== null);
        verify(status !== null);
        compare(page.controlsEnabled, true);

        page.busyOperation = "input-save";
        page.busy = true;
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("Saving"));
        page.busy = false;
        page.busyOperation = "";
        page.sharedMutationBusy = true;
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("shared compositor setting"));
        page.sharedMutationBusy = false;
        page.sharedApplySafe = false;
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("verified activation point"));
        page.sharedApplySafe = true;
        page.confirmationState = "awaiting-confirmation";
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("display test is active"));
        page.confirmationState = "idle";
        page.writable = false;
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("read-only"));
        page.writable = true;
        page.managementState = "unmanaged";
        page.inputAvailable = false;
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("takeover from Displays"));
        compare(findChild(page, "inputOpenDisplaysButton").visible, true);
        page.managementState = "managed";
        page.inputAvailable = true;
        page.inputErrorName = "org.example.Input";
        page.inputErrorMessage = "Injected Input-only failure.";
        verify(String(status.text).includes("Injected Input-only failure."));
        page.inputErrorName = "";
        page.inputErrorMessage = "";
        page.sharedErrorName = "org.example.Apply";
        page.sharedErrorMessage = "Injected shared Apply failure.";
        verify(String(status.text).includes("Injected shared Apply failure."));
        page.sharedErrorName = "";
        page.sharedErrorMessage = "";
        page.revisionToken = "";
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("exact compositor revision token"));
        page.revisionToken = "7";
        page.catalogAvailable = false;
        compare(page.controlsEnabled, false);
        page.catalogAvailable = true;
        page.inputAvailable = false;
        compare(page.controlsEnabled, false);
        compare(control.enabled, false);
        compare(save.enabled, false);
    }

    function test_inputRetainedRevisionUsesCancelFirstWholeRecovery() {
        const baseline = inputDefaults();
        baseline["hyprland.cursor.inactive_timeout"] = 2.37;
        baseline["hyprland.cursor.no_warps"] = true;
        baseline["hyprland.cursor.persistent_warps"] = true;
        baseline["hyprland.input.resolve_binds_by_sym"] = true;
        const testWindow = createTemporaryObject(inputPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page, baseline);
        page.inputAvailable = false;
        page.appliedRevision = 6;
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.recoveryAvailable = true;
        waitForRendering(page);
        wait(0);

        let retryCount = 0;
        let recoveryCount = 0;
        page.retryApplyRequested.connect(function() { ++retryCount; });
        page.recoveryRequested.connect(function() { ++recoveryCount; });
        const retry = findChild(page, "retryApplyInputButton");
        const recover = findChild(page, "recoverInputButton");
        const dialog = findChild(page, "inputRecoveryDialog");
        const warning = findChild(page, "inputRecoveryWarning");
        const cancel = findChild(page, "cancelInputRecoveryButton");
        const confirm = findChild(page, "confirmInputRecoveryButton");
        verify(retry !== null);
        verify(recover !== null);
        verify(dialog !== null);
        verify(warning !== null);
        verify(cancel !== null);
        verify(confirm !== null);
        compare(retry.visible, true);
        compare(recover.visible, true);
        retry.clicked();
        compare(retryCount, 1);
        compare(page.draftValue(page.resolveBindsBySymbolId), true);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        tryCompare(cancel, "activeFocus", true);
        compare(recoveryCount, 0);
        verify(String(warning.text).includes("not limited to Input"));
        verify(String(warning.text).includes("every pending compositor"));
        keyClick(Qt.Key_Escape);
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 0);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        cancel.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 0);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        confirm.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 1);
        compare(confirm.enabled, false);
        confirm.clicked();
        compare(recoveryCount, 1);
        compare(page.draftValue(page.cursorInactiveTimeoutId), 2.37);
        compare(page.draftValue(page.cursorNoWarpsId), true);
        compare(page.draftValue(page.cursorPersistentWarpsId), true);
        compare(page.draftValue(page.resolveBindsBySymbolId), true);
    }

    function test_inputActionsReachableAtMinimumWindow() {
        const testWindow = createTemporaryObject(
            inputPageComponent,
            this,
            { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page);
        page.setDraftValue(page.repeatRateId, 30);
        waitForRendering(page);
        wait(0);

        const scroll = findChild(page, "inputOptionsScrollView");
        const content = findChild(page, "inputOptionsContent");
        const save = findChild(page, "saveInputButton");
        const keyboard = findChild(page, "inputKeyboardCard");
        const virtualKeyboard = findChild(
            page, "inputVirtualKeyboardCard"
        );
        const mouse = findChild(page, "inputMouseCard");
        const pointerBehavior = findChild(
            page, "inputPointerBehaviorCard"
        );
        const cursorVisibility = findChild(
            page, "inputCursorVisibilityCard"
        );
        const cursorPlacement = findChild(
            page, "inputCursorPlacementCard"
        );
        const touchpad = findChild(page, "inputTouchpadCard");
        const advancedScrolling = findChild(
            page, "inputAdvancedScrollingCard"
        );
        const touchpadButtons = findChild(
            page, "inputTouchpadButtonsGesturesCard"
        );
        const touchDevice = findChild(page, "inputTouchDeviceCard");
        const drawingTablet = findChild(page, "inputDrawingTabletCard");
        const tabletMappedRegion = findChild(
            page, "inputTabletMappedRegionCard"
        );
        verify(scroll !== null);
        verify(content !== null);
        verify(save !== null);
        verify(keyboard !== null);
        verify(virtualKeyboard !== null);
        verify(mouse !== null);
        verify(pointerBehavior !== null);
        verify(cursorVisibility !== null);
        verify(cursorPlacement !== null);
        verify(touchpad !== null);
        verify(touchpadButtons !== null);
        verify(touchDevice !== null);
        verify(drawingTablet !== null);
        verify(tabletMappedRegion !== null);
        verify(advancedScrolling !== null);
        compare(page.compactPage, true);
        verify(scroll.contentItem.contentHeight > scroll.height);
        verify(scroll.height >= 100);
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width <= scroll.contentWidth + 0.01);
        for (const card of [
                 keyboard, virtualKeyboard, mouse, pointerBehavior,
                 cursorVisibility, cursorPlacement, touchpad, touchpadButtons,
                 touchDevice, drawingTablet, tabletMappedRegion,
                 advancedScrolling
             ]) {
            const position = card.mapToItem(page, 0, 0);
            verify(position.x >= 0);
            verify(position.x + card.width <= page.width + 0.01);
        }

        const maximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        verify(maximumContentY > 0);
        const scrollPosition = scroll.mapToItem(page, 0, 0);
        const addedControls = [
            findChild(page, "inputResolveBindsBySymbol"),
            findChild(page, "inputCursorHideOnKeyPress"),
            findChild(page, "inputCursorHideOnTouch"),
            findChild(page, "inputCursorHideOnTablet"),
            findChild(page, "inputCursorInactiveTimeout"),
            findChild(page, "inputCursorHotspotPadding"),
            findChild(page, "inputCursorNoWarps"),
            findChild(page, "inputCursorPersistentWarps"),
            findChild(page, "inputCursorWarpBackAfterNonMouseInput"),
            findChild(page, "inputTouchDeviceEnabled"),
            findChild(page, "inputTouchDeviceTransform"),
            findChild(page, "inputTabletRelativeInput"),
            findChild(page, "inputTabletLeftHanded"),
            findChild(page, "inputTabletTransform"),
            findChild(page, "inputTabletRegionPositionX"),
            findChild(page, "inputTabletRegionPositionY"),
            findChild(page, "inputTabletAbsoluteRegionPosition"),
            findChild(page, "inputTabletRegionSizeWidth"),
            findChild(page, "inputTabletRegionSizeHeight")
        ];
        for (const control of addedControls) {
            verify(control !== null);
            verify(control.implicitHeight >= 44);
            const contentPosition = control.mapToItem(content, 0, 0);
            const targetY = Math.max(0, Math.min(
                maximumContentY,
                contentPosition.y + control.height / 2 - scroll.height / 2
            ));
            scroll.contentItem.contentY = targetY;
            tryCompare(scroll.contentItem, "contentY", targetY);
            const pagePosition = control.mapToItem(page, 0, 0);
            verify(pagePosition.x >= 0);
            verify(pagePosition.x + control.width <= page.width + 0.01);
            verify(pagePosition.y + control.height > scrollPosition.y);
            verify(pagePosition.y < scrollPosition.y + scroll.height);
        }
        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        const savePosition = save.mapToItem(page, 0, 0);
        verify(savePosition.x >= 0);
        verify(savePosition.x + save.width <= page.width + 0.01);
        verify(savePosition.y >= 0);
        verify(savePosition.y + save.height <= page.height + 0.01);
        compare(save.enabled, true);
    }

    function test_inputDevicesReachableWithoutHorizontalOverflowAtMinimumWindow() {
        const testWindow = createTemporaryObject(
            inputPageComponent,
            this,
            { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page);
        configureInputDeviceInventory(page);
        waitForRendering(page);
        findChild(page, "inputTabBar").currentIndex = 1;
        tryCompare(page, "inputTabIndex", 1);
        wait(0);

        const scroll = findChild(page, "inputOptionsScrollView");
        const content = findChild(page, "inputOptionsContent");
        const pane = findChild(page, "inputDevicesPane");
        const lastRow = findChild(page, "inputSavedDeviceRow0");
        verify(scroll !== null);
        verify(content !== null);
        verify(pane !== null);
        verify(lastRow !== null);
        compare(page.compactPage, true);
        verify(scroll.contentItem.contentHeight > scroll.height);
        verify(scroll.height >= 100);
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width <= scroll.contentWidth + 0.01);
        const panePosition = pane.mapToItem(content, 0, 0);
        verify(panePosition.x >= 0);
        verify(panePosition.x + pane.width <= content.width + 0.01);

        const maximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        const rowPosition = lastRow.mapToItem(page, 0, 0);
        verify(rowPosition.x >= 0);
        verify(rowPosition.x + lastRow.width <= page.width + 0.01);
        verify(rowPosition.y >= 0);
        verify(rowPosition.y + lastRow.height <= page.height + 0.01);
    }

    function test_inputGesturesUseOneReachableScrollAtMinimumWindow() {
        const gestures = [gestureRecord(
            "compact-gesture", 4, "left", ["super"], 1.2, false,
            { type: "special", workspace: "long-special-workspace-name" }
        )];
        const testWindow = createTemporaryObject(
            inputPageComponent,
            this,
            { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureInputPage(page, inputDefaults(), gestures);
        page.setDraftValue(page.closeGestureTimeoutId, 1200);
        page.inputTabIndex = 2;
        waitForRendering(page);
        wait(0);

        const scroll = findChild(page, "inputOptionsScrollView");
        const content = findChild(page, "inputOptionsContent");
        const tabs = findChild(page, "inputTabBar");
        const behavior = findChild(page, "inputGestureBehaviorCard");
        const list = findChild(page, "inputGestureListCard");
        const edit = findChild(page, "editGestureButton0");
        const save = findChild(page, "saveInputButton");
        for (const control of [tabs, behavior, list, edit, save])
            verify(control !== null);
        compare(page.compactPage, true);
        compare(findChild(page, "inputGestureEditorScrollView"), null);
        compare(findChild(page, "gestureListScrollView"), null);
        verify(scroll.contentItem.contentHeight > scroll.height);
        verify(scroll.height >= 100);
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width <= scroll.contentWidth + 0.01);
        for (const item of [tabs, behavior, list]) {
            const position = item.mapToItem(content, 0, 0);
            verify(position.x >= 0);
            verify(position.x + item.width <= content.width + 0.01);
        }
        const listControls = [
                 findChild(page, "inputGlobalTab"),
                 findChild(page, "inputDevicesTab"),
                 findChild(page, "inputGesturesTab"),
                 findChild(page, "inputGestureCloseTimeout"),
                 findChild(page, "addGestureButton"), edit,
                 findChild(page, "moveGestureUpButton0"),
                 findChild(page, "moveGestureDownButton0"),
                 findChild(page, "removeGestureButton0"), save
             ];
        for (const control of listControls) {
            verify(control !== null);
            verify(control.implicitHeight >= 44);
            const position = control.mapToItem(content, 0, 0);
            verify(position.x >= 0);
            verify(position.x + control.width <= content.width + 0.01);
        }

        let maximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        verify(maximumContentY > 0);
        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        let savePosition = save.mapToItem(page, 0, 0);
        verify(savePosition.x >= 0);
        verify(savePosition.x + save.width <= page.width + 0.01);
        verify(savePosition.y >= 0);
        verify(savePosition.y + save.height <= page.height + 0.01);
        compare(save.enabled, true);

        edit.clicked();
        wait(0);
        const editor = findChild(page, "inputGestureEditor");
        const done = findChild(page, "closeGestureEditorButton");
        const remove = findChild(page, "removeGestureFromEditorButton");
        verify(editor !== null);
        verify(done !== null);
        verify(remove !== null);
        verify(done.implicitHeight >= 44);
        verify(remove.implicitHeight >= 44);
        const editorPosition = editor.mapToItem(content, 0, 0);
        verify(editorPosition.x >= 0);
        verify(
            editorPosition.x + editor.width <= content.width + 0.01,
            "Gesture editor width " + editor.width
                + " exceeds compact content width " + content.width
        );
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        const editorControls = [
            done,
            findChild(page, "gestureFingers"),
            findChild(page, "gestureDirection"),
            findChild(page, "gestureScale"),
            findChild(page, "gestureAction"),
            findChild(page, "gestureSpecialWorkspace"),
            findChild(page, "gestureDisableInhibit"),
            remove
        ];
        for (let index = 0; index < 8; ++index)
            editorControls.push(findChild(page, "gestureModifier" + index));
        for (const control of editorControls) {
            verify(control !== null);
            verify(control.visible);
            verify(control.implicitHeight >= 44);
            const position = control.mapToItem(content, 0, 0);
            verify(position.x >= 0);
            verify(
                position.x + control.width <= content.width + 0.01,
                control.objectName + " exceeds compact content width"
            );
        }
        const actionControl = findChild(page, "gestureAction");
        actionControl.currentIndex = 1;
        actionControl.activated(1);
        compare(page.editingGesture().action.type, "cursorZoom");
        verify(
            waitForPolish(testWindow, 1000),
            "Cursor zoom controls did not finish layout polishing"
        );
        const zoomMode = findChild(page, "gestureCursorZoomMode");
        const zoomLevel = findChild(page, "gestureCursorZoomLevel");
        for (const control of [zoomMode, zoomLevel]) {
            verify(control !== null);
            verify(control.visible);
            verify(control.implicitHeight >= 44);
            const position = control.mapToItem(content, 0, 0);
            verify(position.x >= 0);
            verify(
                position.x + control.width <= content.width + 0.01,
                control.objectName + " exceeds compact content width"
            );
        }
        maximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        savePosition = save.mapToItem(page, 0, 0);
        verify(savePosition.x >= 0);
        verify(savePosition.x + save.width <= page.width + 0.01);
        verify(savePosition.y >= 0);
        verify(savePosition.y + save.height <= page.height + 0.01);
    }

    function test_windowsUsesExactAuthoredControlsDependenciesAndTargets() {
        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWindowsPage(page);
        waitForRendering(page);
        wait(0);

        compare(page.trustedDefinitionsValid, true);
        compare(page.trustedValuesValid, true);
        compare(page.controlsEnabled, true);
        compare(page.expectedOptionIds.length, 110);
        const definitions = windowsDefinitions();
        compare(definitions.length, 110);
        compare(page.expectedOptionIds[109], page.followMouseThresholdId);
        compare(definitions[109].id, page.followMouseThresholdId);
        compare(definitions[109].type, "number");
        compare(definitions[109].control, "slider");
        compare(definitions[109].defaultValue, 0);
        compare(definitions[109].min, 0);
        compare(definitions[109].max, 1000000);
        compare(definitions[109].risk, "safe");
        verify(definitions[109].step === undefined);
        compare(
            JSON.stringify(definitions.map(option => option.id)),
            JSON.stringify(page.expectedOptionIds)
        );
        compare(JSON.stringify(definitions.slice(92)), JSON.stringify([
            {
                id: page.allowPinFullscreenId,
                type: "boolean", control: "toggle", defaultValue: false
            },
            {
                id: page.focusPreferredMethodId,
                type: "enum", control: "select", defaultValue: 0,
                choices: [
                    { label: "0", value: 0 },
                    { label: "1", value: 1 }
                ],
                min: 0, max: 1
            },
            {
                id: page.ignoreGroupLockId,
                type: "boolean", control: "toggle", defaultValue: false
            },
            {
                id: page.movefocusCyclesFullscreenId,
                type: "boolean", control: "toggle", defaultValue: false
            },
            {
                id: page.movefocusCyclesGroupfirstId,
                type: "boolean", control: "toggle", defaultValue: false
            },
            {
                id: page.windowDirectionMonitorFallbackId,
                type: "boolean", control: "toggle", defaultValue: true
            },
            {
                id: page.anrDialogEnabledId,
                type: "boolean", control: "toggle", defaultValue: true
            },
            {
                id: page.anrMissedPingsId,
                type: "integer", control: "spinBox", defaultValue: 5,
                min: 1, max: 20
            },
            {
                id: page.sizeLimitsTiledId,
                type: "boolean", control: "toggle", defaultValue: false
            },
            {
                id: page.alwaysFollowOnDndId,
                type: "boolean", control: "toggle", defaultValue: true
            },
            {
                id: page.focusOnActivateId,
                type: "boolean", control: "toggle", defaultValue: false
            },
            {
                id: page.mouseMoveFocusesMonitorId,
                type: "boolean", control: "toggle", defaultValue: true
            },
            {
                id: page.onFocusUnderFullscreenId,
                type: "enum", control: "select", defaultValue: 2,
                choices: [
                    { label: "0", value: 0 },
                    { label: "1", value: 1 },
                    { label: "2", value: 2 }
                ],
                min: 0, max: 2
            },
            {
                id: page.exitWindowRetainsFullscreenId,
                type: "boolean", control: "toggle", defaultValue: false
            },
            {
                id: page.enableSwallowId,
                type: "boolean", control: "toggle", defaultValue: false
            },
            {
                id: page.swallowRegexId,
                type: "string", control: "text", defaultValue: "",
                maxLength: 4096
            },
            {
                id: page.swallowExceptionRegexId,
                type: "string", control: "text", defaultValue: "",
                maxLength: 4096
            },
            {
                id: page.followMouseThresholdId,
                type: "number", control: "slider", defaultValue: 0,
                min: 0, max: 1000000, risk: "safe"
            }
        ]));

        const cards = [
            findChild(page, "windowsLayoutCard"),
            findChild(page, "windowsSpacingCard"),
            findChild(page, "windowsEngineCard"),
            findChild(page, "windowsGroupsCard"),
            findChild(page, "windowsGroupbarBehaviorCard"),
            findChild(page, "windowsGroupbarLayoutCard"),
            findChild(page, "windowsGroupbarTitlesCard"),
            findChild(page, "windowsGroupbarBackgroundCard"),
            findChild(page, "windowsResizeCard"),
            findChild(page, "windowsSnapCard"),
            findChild(page, "windowsFocusCard"),
            findChild(page, "windowsSwallowingCard"),
            findChild(page, "windowsUnresponsiveApplicationsCard")
        ];
        for (const card of cards)
            verify(card !== null);
        for (let index = 1; index < cards.length; ++index) {
            verify(cards[index].mapToItem(page, 0, 0).y
                > cards[index - 1].mapToItem(page, 0, 0).y);
        }

        const names = [
            "windowsDefaultLayout",
            "windowsResizeOnBorder",
            "windowsExtendBorderGrabArea",
            "windowsHoverIconOnBorder",
            "windowsResizeCorner",
            "windowsSnapEnabled",
            "windowsSnapBorderOverlap",
            "windowsSnapMonitorGap",
            "windowsSnapRespectGaps",
            "windowsSnapWindowGap",
            "windowsFollowMouse",
            "windowsFollowMouseThreshold",
            "windowsMouseRefocus",
            "windowsFollowMouseShrink",
            "windowsFloatSwitchOverrideFocus",
            "windowsFocusOnClose",
            "windowsSpecialFallthrough",
            "windowsNoFocusFallback",
            "windowsModalParentBlocking",
            "windowsAspectRatioWidth",
            "windowsAspectRatioHeight",
            "windowsAspectRatioTolerance",
            "windowsFloatGapTop",
            "windowsFloatGapRight",
            "windowsFloatGapBottom",
            "windowsFloatGapLeft",
            "windowsWorkspaceGaps",
            "windowsDwindleDefaultSplitRatio",
            "windowsDwindleForceSplit",
            "windowsDwindlePermanentDirectionOverride",
            "windowsDwindlePreciseMouseMove",
            "windowsDwindlePreserveSplit",
            "windowsDwindleSmartResizing",
            "windowsDwindleSmartSplit",
            "windowsDwindleSpecialScaleFactor",
            "windowsDwindleSplitBias",
            "windowsDwindleSplitWidthMultiplier",
            "windowsDwindleUseActiveForSplits",
            "windowsMasterAllowSmallSplit",
            "windowsMasterAlwaysKeepPosition",
            "windowsMasterCenterIgnoresReserved",
            "windowsMasterCenterFallback",
            "windowsMasterDropAtCursor",
            "windowsMasterFocusOnClose",
            "windowsMasterFactor",
            "windowsMasterNewOnActive",
            "windowsMasterNewOnTop",
            "windowsMasterNewStatus",
            "windowsMasterOrientation",
            "windowsMasterCenterSlaveCount",
            "windowsMasterSmartResizing",
            "windowsMasterSpecialScaleFactor",
            "windowsScrollingColumnWidth",
            "windowsScrollingDirection",
            "windowsScrollingFocusFitMethod",
            "windowsScrollingFollowFocus",
            "windowsScrollingFollowMinimumVisible",
            "windowsScrollingFullscreenOneColumn",
            "windowsScrollingWrapFocus",
            "windowsScrollingWrapSwapColumn",
            "windowsScrollingMoveSnapCursor",
            "windowsScrollingMoveSnapGrid",
            "windowsGroupAutoGroup",
            "windowsGroupInsertAfterCurrent",
            "windowsGroupFocusRemovedWindow",
            "windowsGroupDragIntoGroup",
            "windowsGroupMergeGroupsOnDrag",
            "windowsGroupMergeGroupsOnGroupbar",
            "windowsGroupMergeFloatedIntoTiledOnGroupbar",
            "windowsGroupOnMoveToWorkspace",
            "windowsGroupbarEnabled",
            "windowsGroupbarDisableWhenOnly",
            "windowsGroupbarScrolling",
            "windowsGroupbarMiddleClickClose",
            "windowsGroupbarStacked",
            "windowsGroupbarHeight",
            "windowsGroupbarIndicatorHeight",
            "windowsGroupbarIndicatorGap",
            "windowsGroupbarGapsIn",
            "windowsGroupbarGapsOut",
            "windowsGroupbarKeepUpperGap",
            "windowsGroupbarRounding",
            "windowsGroupbarRoundingPower",
            "windowsGroupbarRoundOnlyEdges",
            "windowsGroupbarPriority",
            "windowsGroupbarRenderTitles",
            "windowsGroupbarFontFamily",
            "windowsGroupbarFontSize",
            "windowsGroupbarFontWeightActive",
            "windowsGroupbarFontWeightInactive",
            "windowsGroupbarTextPadding",
            "windowsGroupbarTextOffset",
            "windowsGroupbarBlur",
            "windowsGroupbarGradients",
            "windowsGroupbarGradientRounding",
            "windowsGroupbarGradientRoundingPower",
            "windowsGroupbarGradientRoundOnlyEdges",
            "windowsAllowPinFullscreen",
            "windowsFocusPreferredMethod",
            "windowsIgnoreGroupLock",
            "windowsMovefocusCyclesFullscreen",
            "windowsMovefocusCyclesGroupfirst",
            "windowsWindowDirectionMonitorFallback",
            "windowsAnrDialogEnabled",
            "windowsAnrMissedPings",
            "windowsSizeLimitsTiled",
            "windowsAlwaysFollowOnDnd",
            "windowsFocusOnActivate",
            "windowsMouseMoveFocusesMonitor",
            "windowsOnFocusUnderFullscreen",
            "windowsExitWindowRetainsFullscreen",
            "windowsEnableSwallow",
            "windowsSwallowRegex",
            "windowsSwallowExceptionRegex"
        ];
        const controls = [];
        for (const name of names) {
            const control = findChild(page, name);
            verify(control !== null, "Missing control " + name);
            verify(control.implicitHeight >= 44,
                   name + " must provide a 44px interaction target");
            verify(String(control.Accessible.name).length > 0);
            controls.push(control);
        }

        compare(controls[0].currentText, "Dwindle");
        compare(controls[0].model.length, 4);
        compare(controls[1].checked, false);
        compare(controls[2].from, 0);
        compare(controls[2].to, 100);
        compare(controls[2].value, 15);
        compare(controls[3].checked, true);
        compare(controls[4].currentIndex, 0);
        compare(controls[4].model.length, 5);
        compare(controls[5].checked, false);
        compare(controls[7].from, 0);
        compare(controls[7].to, 100);
        compare(controls[7].value, 10);
        compare(controls[9].from, 0);
        compare(controls[9].to, 100);
        compare(controls[9].value, 10);
        compare(controls[10].currentIndex, 1);
        compare(controls[10].model.length, 4);
        compare(controls[11].text, "0");
        compare(controls[11].inputValid, true);
        compare(controls[11].Accessible.name,
                "Pointer focus movement threshold in logical pixels");
        compare(controls[13].from, 0);
        compare(controls[13].to, 300);
        compare(controls[13].value, 0);
        compare(controls[14].currentIndex, 1);
        compare(controls[15].currentIndex, 0);

        const allowPinFullscreen = findChild(
            page, "windowsAllowPinFullscreen"
        );
        const focusPreferredMethod = findChild(
            page, "windowsFocusPreferredMethod"
        );
        const ignoreGroupLock = findChild(
            page, "windowsIgnoreGroupLock"
        );
        const cyclesFullscreen = findChild(
            page, "windowsMovefocusCyclesFullscreen"
        );
        const cyclesGroupfirst = findChild(
            page, "windowsMovefocusCyclesGroupfirst"
        );
        const monitorFallback = findChild(
            page, "windowsWindowDirectionMonitorFallback"
        );
        const anrEnabled = findChild(page, "windowsAnrDialogEnabled");
        const anrThreshold = findChild(page, "windowsAnrMissedPings");
        const sizeLimitsTiled = findChild(page, "windowsSizeLimitsTiled");
        const alwaysFollowOnDnd = findChild(
            page, "windowsAlwaysFollowOnDnd"
        );
        const focusOnActivate = findChild(page, "windowsFocusOnActivate");
        const mouseMoveFocusesMonitor = findChild(
            page, "windowsMouseMoveFocusesMonitor"
        );
        const onFocusUnderFullscreen = findChild(
            page, "windowsOnFocusUnderFullscreen"
        );
        const exitWindowRetainsFullscreen = findChild(
            page, "windowsExitWindowRetainsFullscreen"
        );
        const enableSwallow = findChild(page, "windowsEnableSwallow");
        const swallowRegex = findChild(page, "windowsSwallowRegex");
        const swallowExceptionRegex = findChild(
            page, "windowsSwallowExceptionRegex"
        );
        compare(allowPinFullscreen.checked, false);
        compare(focusPreferredMethod.currentIndex, 0);
        compare(focusPreferredMethod.currentText, "Recent focus");
        compare(focusPreferredMethod.model.length, 2);
        verify(String(focusPreferredMethod.Accessible.name)
            .includes("tiled-window"));
        compare(ignoreGroupLock.checked, false);
        compare(cyclesFullscreen.checked, false);
        compare(cyclesGroupfirst.checked, false);
        compare(monitorFallback.checked, true);
        compare(anrEnabled.checked, true);
        compare(anrThreshold.from, 1);
        compare(anrThreshold.to, 20);
        compare(anrThreshold.value, 5);
        compare(sizeLimitsTiled.checked, false);
        compare(alwaysFollowOnDnd.checked, true);
        compare(alwaysFollowOnDnd.enabled, false);
        const followThreshold = findChild(
            page, "windowsFollowMouseThreshold"
        );
        const followThresholdRow = findChild(
            page, "windowsFollowMouseThresholdRow"
        );
        const followThresholdValidation = findChild(
            page, "windowsFollowMouseThresholdValidation"
        );
        verify(followThreshold !== null);
        verify(followThresholdRow !== null);
        verify(followThresholdValidation !== null);
        compare(followThreshold.text, "0");
        compare(followThreshold.inputValid, true);
        compare(followThreshold.enabled, true);
        verify(followThreshold.implicitHeight >= 44);
        compare(
            followThreshold.Accessible.name,
            "Pointer focus movement threshold in logical pixels"
        );
        compare(followThresholdValidation.visible, false);
        verify(followThresholdRow.description.includes("strictly exceed"));
        verify(followThresholdRow.description.includes(
            "protocol drag-and-drop override"
        ));
        verify(followThresholdRow.description.includes(
            "0.5 seconds or more"
        ));
        verify(followThresholdRow.description.includes(
            "explicit refocus bypasses"
        ));
        verify(followThresholdRow.description.includes(
            "No follow mouse Window Rule"
        ));
        verify(followThresholdRow.description.includes(
            "0 still requires positive movement"
        ));
        compare(focusOnActivate.checked, false);
        compare(mouseMoveFocusesMonitor.checked, true);
        compare(onFocusUnderFullscreen.currentIndex, 2);
        compare(onFocusUnderFullscreen.currentText, "Exit current mode");
        compare(onFocusUnderFullscreen.model.length, 3);
        compare(exitWindowRetainsFullscreen.checked, false);
        compare(enableSwallow.checked, false);
        compare(swallowRegex.text, "");
        compare(swallowExceptionRegex.text, "");
        compare(swallowRegex.maximumLength, 4096);
        compare(swallowExceptionRegex.maximumLength, 4096);
        compare(swallowRegex.enabled, false);
        compare(swallowExceptionRegex.enabled, false);
        exitWindowRetainsFullscreen.checked = true;
        exitWindowRetainsFullscreen.clicked();
        for (let index = 0; index < 3; ++index) {
            onFocusUnderFullscreen.currentIndex = index;
            onFocusUnderFullscreen.activated(index);
            compare(page.draftValue(page.onFocusUnderFullscreenId), index);
            compare(exitWindowRetainsFullscreen.checked, true);
            compare(page.draftValue(
                page.exitWindowRetainsFullscreenId
            ), true);
        }
        for (const control of [
                 allowPinFullscreen, focusPreferredMethod,
                 ignoreGroupLock, cyclesFullscreen,
                 cyclesGroupfirst, monitorFallback, anrEnabled, anrThreshold,
                 sizeLimitsTiled, focusOnActivate,
                 mouseMoveFocusesMonitor, onFocusUnderFullscreen,
                 exitWindowRetainsFullscreen, enableSwallow
             ]) {
            compare(control.enabled, true);
        }
        enableSwallow.checked = true;
        enableSwallow.clicked();
        compare(page.draftValue(page.enableSwallowId), true);
        compare(swallowRegex.enabled, true);
        compare(swallowExceptionRegex.enabled, true);
        swallowRegex.text = "^(kitty|Alacritty)$";
        swallowRegex.editingFinished();
        swallowExceptionRegex.text = "^scratch$";
        swallowExceptionRegex.editingFinished();
        compare(
            page.draftValue(page.swallowRegexId),
            "^(kitty|Alacritty)$"
        );
        compare(
            page.draftValue(page.swallowExceptionRegexId), "^scratch$"
        );
        enableSwallow.checked = false;
        enableSwallow.clicked();
        compare(page.draftValue(page.enableSwallowId), false);
        compare(swallowRegex.enabled, false);
        compare(swallowExceptionRegex.enabled, false);
        compare(
            page.draftValue(page.swallowRegexId),
            "^(kitty|Alacritty)$"
        );
        compare(
            page.draftValue(page.swallowExceptionRegexId), "^scratch$"
        );
        const follow = findChild(page, "windowsFollowMouse");
        follow.currentIndex = 0;
        follow.activated(0);
        compare(alwaysFollowOnDnd.enabled, true);
        compare(alwaysFollowOnDnd.checked, true);
        compare(followThreshold.enabled, true);
        alwaysFollowOnDnd.checked = false;
        alwaysFollowOnDnd.clicked();
        compare(page.draftValue(page.alwaysFollowOnDndId), false);
        compare(followThreshold.enabled, false);
        compare(page.draftValue(page.followMouseThresholdId), 0);
        alwaysFollowOnDnd.checked = true;
        alwaysFollowOnDnd.clicked();
        compare(followThreshold.enabled, true);
        follow.currentIndex = 1;
        follow.activated(1);
        compare(alwaysFollowOnDnd.enabled, false);
        compare(alwaysFollowOnDnd.checked, true);
        compare(followThreshold.enabled, true);
        verify(String(sizeLimitsTiled.parent.description)
            .includes("centered inside its assigned tile"));
        verify(String(alwaysFollowOnDnd.parent.description)
            .includes("saved choice is dormant"));
        verify(String(mouseMoveFocusesMonitor.parent.description)
            .includes("Clicks, explicit refocus"));
        verify(String(focusOnActivate.parent.description)
            .includes("normal cursor-warp policy"));
        verify(String(onFocusUnderFullscreen.parent.description)
            .includes("fullscreen or maximized"));
        verify(String(enableSwallow.parent.description)
            .includes("future mapped windows"));
        verify(String(enableSwallow.parent.description)
            .includes("preserves both patterns"));
        verify(String(swallowRegex.Accessible.description)
            .includes("Full-match"));
        verify(String(swallowRegex.Accessible.description)
            .includes("saved"));
        verify(String(swallowExceptionRegex.Accessible.description)
            .includes("full-match"));
        const collectText = function(item) {
            if (!item)
                return "";
            let result = typeof item.text === "string" ? item.text + " " : "";
            for (const child of item.children || [])
                result += collectText(child);
            return result;
        };
        const primaryCopy = collectText(swallowRegex.parent);
        verify(primaryCopy.includes("RE2 pattern"));
        verify(primaryCopy.includes("complete class"));
        verify(primaryCopy.includes("mapped, input-capable process ancestor"));
        verify(primaryCopy.includes("checked on Save"));
        const exceptionCopy = collectText(swallowExceptionRegex.parent);
        verify(exceptionCopy.includes("complete window title"));
        verify(exceptionCopy.includes("Leave empty for no exceptions"));
        const swallowingCopy = collectText(
            findChild(page, "windowsSwallowingCard")
        );
        verify(swallowingCopy.includes("future window maps"));
        verify(swallowingCopy.includes("child unmaps or closes"));
        verify(swallowingCopy.includes("existing swallowed pair"));
        anrEnabled.checked = false;
        anrEnabled.clicked();
        compare(anrThreshold.enabled, false);
        compare(page.draftValue(page.anrMissedPingsId), 5);
        anrEnabled.checked = true;
        anrEnabled.clicked();
        compare(anrThreshold.enabled, true);

        const groupDrag = findChild(page, "windowsGroupDragIntoGroup");
        const mergeDrag = findChild(
            page, "windowsGroupMergeGroupsOnDrag"
        );
        const mergeGroupbar = findChild(
            page, "windowsGroupMergeGroupsOnGroupbar"
        );
        const mergeFloated = findChild(
            page, "windowsGroupMergeFloatedIntoTiledOnGroupbar"
        );
        compare(groupDrag.currentIndex, 1);
        compare(groupDrag.model.length, 3);
        compare(mergeDrag.enabled, true);
        compare(mergeGroupbar.enabled, true);
        compare(mergeFloated.enabled, true);
        mergeDrag.checked = false;
        mergeDrag.clicked();
        compare(mergeGroupbar.enabled, false);
        compare(mergeFloated.enabled, true);
        compare(
            page.draftValue(page.groupMergeGroupsOnGroupbarId), true
        );
        groupDrag.currentIndex = 0;
        groupDrag.activated(0);
        compare(mergeDrag.enabled, false);
        compare(mergeGroupbar.enabled, false);
        compare(mergeFloated.enabled, false);
        compare(page.draftValue(page.groupMergeGroupsOnDragId), false);
        compare(
            page.draftValue(page.groupMergeGroupsOnGroupbarId), true
        );
        compare(
            page.draftValue(
                page.groupMergeFloatedIntoTiledOnGroupbarId
            ),
            false
        );
        groupDrag.currentIndex = 2;
        groupDrag.activated(2);
        compare(mergeDrag.enabled, true);
        compare(mergeGroupbar.enabled, false);
        compare(mergeFloated.enabled, true);

        compare(controls[2].enabled, false);
        compare(controls[3].enabled, false);
        compare(controls[4].enabled, true);
        compare(controls[6].enabled, false);
        compare(controls[7].enabled, false);
        compare(controls[8].enabled, false);
        compare(controls[9].enabled, false);
        compare(controls[11].enabled, true);
        compare(controls[12].enabled, true);
        compare(controls[13].enabled, true);

        controls[1].checked = true;
        controls[1].clicked();
        compare(controls[2].enabled, true);
        compare(controls[3].enabled, true);
        controls[5].checked = true;
        controls[5].clicked();
        compare(controls[6].enabled, true);
        compare(controls[7].enabled, true);
        compare(controls[8].enabled, true);
        compare(controls[9].enabled, true);

        controls[10].currentIndex = 0;
        controls[10].activated(0);
        compare(controls[12].enabled, false);
        compare(controls[13].enabled, false);
        compare(controls[11].enabled, true);
        compare(page.draftValue(page.mouseRefocusId), true);
        compare(page.draftValue(page.followMouseShrinkId), 0);

        const groupbarEnabled = findChild(page, "windowsGroupbarEnabled");
        const stacked = findChild(page, "windowsGroupbarStacked");
        const height = findChild(page, "windowsGroupbarHeight");
        const gapsIn = findChild(page, "windowsGroupbarGapsIn");
        const gapsOut = findChild(page, "windowsGroupbarGapsOut");
        const keepUpperGap = findChild(
            page, "windowsGroupbarKeepUpperGap"
        );
        const rounding = findChild(page, "windowsGroupbarRounding");
        const roundingPower = findChild(
            page, "windowsGroupbarRoundingPower"
        );
        const roundOnlyEdges = findChild(
            page, "windowsGroupbarRoundOnlyEdges"
        );
        const renderTitles = findChild(
            page, "windowsGroupbarRenderTitles"
        );
        const fontFamily = findChild(page, "windowsGroupbarFontFamily");
        const fontWeight = findChild(
            page, "windowsGroupbarFontWeightActive"
        );
        const gradients = findChild(page, "windowsGroupbarGradients");
        const gradientRounding = findChild(
            page, "windowsGroupbarGradientRounding"
        );
        const gradientPower = findChild(
            page, "windowsGroupbarGradientRoundingPower"
        );
        const gradientRoundOnly = findChild(
            page, "windowsGroupbarGradientRoundOnlyEdges"
        );

        compare(groupbarEnabled.checked, true);
        compare(height.enabled, true);
        compare(gapsIn.enabled, true);
        compare(keepUpperGap.enabled, true);
        compare(roundingPower.enabled, true);
        compare(roundOnlyEdges.enabled, true);
        compare(fontFamily.enabled, true);
        compare(fontWeight.enabled, true);
        compare(gradientRounding.enabled, false);
        compare(gradientPower.enabled, false);
        compare(gradientRoundOnly.enabled, false);

        fontFamily.text = "Iosevka Term";
        fontFamily.editingFinished();
        compare(page.draftValue(page.groupbarFontFamilyId), "Iosevka Term");
        fontWeight.value = 735;
        fontWeight.valueModified();
        compare(page.draftValue(page.groupbarFontWeightActiveId), 735);

        stacked.checked = true;
        stacked.clicked();
        compare(gapsIn.enabled, false);
        compare(page.draftValue(page.groupbarGapsInId), 2);
        gapsOut.value = 0;
        gapsOut.valueModified();
        compare(keepUpperGap.enabled, false);
        compare(page.draftValue(page.groupbarKeepUpperGapId), true);
        rounding.value = 0;
        rounding.valueModified();
        compare(roundingPower.enabled, false);
        compare(roundOnlyEdges.enabled, false);
        compare(page.draftValue(page.groupbarRoundingPowerId), 2);
        compare(page.draftValue(page.groupbarRoundOnlyEdgesId), true);

        renderTitles.checked = false;
        renderTitles.clicked();
        compare(fontFamily.enabled, false);
        compare(fontWeight.enabled, false);
        compare(page.draftValue(page.groupbarFontFamilyId), "Iosevka Term");
        compare(page.draftValue(page.groupbarFontWeightActiveId), 735);
        compare(height.enabled, false);
        gradients.checked = true;
        gradients.clicked();
        compare(height.enabled, true);
        compare(gradientRounding.enabled, true);
        gradientRounding.value = 0;
        gradientRounding.valueModified();
        compare(gradientPower.enabled, false);
        compare(gradientRoundOnly.enabled, false);
        compare(page.draftValue(page.groupbarGradientRoundingPowerId), 2);
        compare(
            page.draftValue(page.groupbarGradientRoundOnlyEdgesId), true
        );

        groupDrag.currentIndex = 1;
        groupDrag.activated(1);
        mergeDrag.checked = true;
        mergeDrag.clicked();
        compare(mergeGroupbar.enabled, true);
        compare(mergeFloated.enabled, true);
        groupbarEnabled.checked = false;
        groupbarEnabled.clicked();
        compare(mergeGroupbar.enabled, false);
        compare(mergeFloated.enabled, false);
        compare(fontFamily.enabled, false);
        compare(page.draftValue(page.groupbarFontFamilyId), "Iosevka Term");
        compare(page.draftValue(page.groupbarFontWeightActiveId), 735);

        const actionTargets = [
            "refreshWindowsButton",
            "windowsOpenDisplaysButton",
            "loadCurrentWindowsButton",
            "retryApplyWindowsButton",
            "recoverWindowsButton",
            "discardWindowsDraftButton",
            "resetWindowsDefaultsButton",
            "saveWindowsButton",
            "toggleWindowsLayoutMotionButton",
            "cancelWindowsRecoveryButton",
            "confirmWindowsRecoveryButton"
        ];
        for (const objectName of actionTargets) {
            const target = findChild(page, objectName);
            verify(target !== null, "Missing target " + objectName);
            verify(target.implicitHeight >= 44,
                   objectName + " must provide a 44px interaction target");
        }
    }

    function test_windowsFollowMouseThresholdUsesExactRecoverableDraft() {
        const baseline = windowsDefaults();
        baseline["hyprland.input.follow_mouse_threshold"] = 1e-7;
        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const row = findChild(page, "windowsFollowMouseThresholdRow");
        verify(row !== null);
        let hydrationEditCount = 0;
        row.valueModified.connect(function() { ++hydrationEditCount; });
        configureWindowsPage(page, baseline);
        waitForRendering(page);
        wait(0);

        const field = findChild(page, "windowsFollowMouseThreshold");
        const validation = findChild(
            page, "windowsFollowMouseThresholdValidation"
        );
        const follow = findChild(page, "windowsFollowMouse");
        const dnd = findChild(page, "windowsAlwaysFollowOnDnd");
        const monitorFocus = findChild(
            page, "windowsMouseMoveFocusesMonitor"
        );
        const save = findChild(page, "saveWindowsButton");
        const discard = findChild(page, "discardWindowsDraftButton");
        const reset = findChild(page, "resetWindowsDefaultsButton");
        for (const item of [field, validation, follow, dnd, monitorFocus,
                            save, discard, reset]) {
            verify(item !== null);
        }

        compare(hydrationEditCount, 0);
        compare(page.draftDirty, false);
        compare(page.draftValid, true);
        compare(page.draftValue(page.followMouseThresholdId), 1e-7);
        compare(row.value, 1e-7);
        compare(row.projectedValue, "0.0000001");
        compare(field.text, "0.0000001");
        compare(row.minimumValue, 0);
        compare(row.maximumValue, 1000000);
        compare(row.controlWidth, 190);
        compare(field.inputValid, true);
        verify(field.implicitHeight >= 44);
        compare(validation.visible, false);
        compare(row.visible, true);
        verify(
            row.mapToItem(page, 0, 0).y
                > dnd.parent.mapToItem(page, 0, 0).y
        );
        verify(
            row.mapToItem(page, 0, 0).y
                < monitorFocus.parent.mapToItem(page, 0, 0).y
        );

        page.setDraftValue(page.followMouseThresholdId, "1");
        compare(page.draftValue(page.followMouseThresholdId), 1e-7);
        page.setExactDecimalDraftValue(page.followMouseThresholdId, -1);
        page.setExactDecimalDraftValue(
            page.followMouseThresholdId, 1000000.0001
        );
        page.setExactDecimalDraftValue(
            page.followMouseThresholdId, Infinity
        );
        compare(page.draftValue(page.followMouseThresholdId), 1e-7);

        field.forceActiveFocus();
        field.text = "12.3400";
        field.textEdited();
        compare(page.draftValue(page.followMouseThresholdId), 12.34);
        compare(field.text, "12.3400");
        follow.forceActiveFocus();
        wait(0);
        compare(row.localEditActive, false);
        compare(field.text, "12.34");

        const invalidTexts = [
            "", "+0.5", " 0.5", "01", ".5", "1.", "1e-1",
            "-0.1", "1000000.0001"
        ];
        for (const invalidText of invalidTexts) {
            field.forceActiveFocus();
            field.text = invalidText;
            field.textEdited();
            wait(0);
            compare(
                page.draftValue(page.followMouseThresholdId), invalidText
            );
            compare(field.inputValid, false);
            compare(page.draftValid, false);
            compare(page.draftDirty, true);
            compare(save.enabled, false);
            compare(row.visible, true);
            compare(validation.visible, true);
            compare(
                validation.Accessible.role, Accessible.AlertMessage
            );
            compare(validation.Accessible.name, validation.text);
            verify(String(field.Accessible.description).includes(
                "Enter a plain decimal from 0 through 1000000"
            ));

            field.text = "0.125";
            field.textEdited();
            wait(0);
            compare(page.draftValue(page.followMouseThresholdId), 0.125);
            compare(field.inputValid, true);
            compare(page.draftValid, true);
            compare(validation.visible, false);
        }

        field.forceActiveFocus();
        field.text = "-0";
        field.textEdited();
        follow.forceActiveFocus();
        wait(0);
        compare(page.draftValue(page.followMouseThresholdId), 0);
        compare(field.text, "0");

        page.setExactDecimalDraftValue(page.followMouseThresholdId, 3.75);
        page.setDraftValue(page.followMouseId, 0);
        page.setDraftValue(page.alwaysFollowOnDndId, false);
        compare(field.enabled, false);
        compare(page.draftValue(page.followMouseThresholdId), 3.75);
        page.setDraftValue(page.alwaysFollowOnDndId, true);
        compare(field.enabled, true);
        compare(page.draftValue(page.followMouseThresholdId), 3.75);
        page.setDraftValue(page.followMouseId, 1);
        compare(dnd.enabled, false);
        compare(field.enabled, true);

        field.forceActiveFocus();
        field.text = "1e0";
        field.textEdited();
        follow.forceActiveFocus();
        wait(0);
        compare(page.draftValue(page.followMouseThresholdId), "1e0");
        compare(field.text, "1e0");
        compare(validation.visible, true);
        page.engineTabIndex = 1;
        wait(0);
        compare(row.visible, true);
        compare(page.draftValue(page.followMouseThresholdId), "1e0");
        page.engineTabIndex = 0;

        const newer = windowsDefaults();
        newer[page.followMouseThresholdId] = 0.75;
        page.windowsValues = newer;
        page.revisionToken = "8";
        wait(0);
        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.followMouseThresholdId), "1e0");
        compare(row.visible, true);
        compare(validation.visible, true);
        const loadCurrent = findChild(page, "loadCurrentWindowsButton");
        verify(loadCurrent !== null);
        compare(loadCurrent.enabled, true);
        loadCurrent.clicked();
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.draftValid, true);
        compare(page.draftValue(page.followMouseThresholdId), 0.75);
        compare(field.text, "0.75");

        page.setExactDecimalDraftValue(page.followMouseThresholdId, "bad");
        compare(page.draftValid, false);
        discard.clicked();
        wait(0);
        compare(page.draftValue(page.followMouseThresholdId), 0.75);
        compare(page.draftValid, true);
        page.setExactDecimalDraftValue(page.followMouseThresholdId, "bad");
        reset.clicked();
        wait(0);
        compare(page.draftValue(page.followMouseThresholdId), 0);
        compare(page.draftValid, true);

        page.setExactDecimalDraftValue(
            page.followMouseThresholdId, 0.3333333333333333
        );
        let submitted = null;
        page.saveRequested.connect(function(values) { submitted = values; });
        save.clicked();
        verify(submitted !== null);
        compare(Object.keys(submitted).length, 110);
        compare(
            submitted[page.followMouseThresholdId],
            0.3333333333333333
        );
    }

    function test_windowsAllControlsWriteExactDraftAndPreview() {
        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWindowsPage(page);
        waitForRendering(page);
        wait(0);

        const layout = findChild(page, "windowsDefaultLayout");
        const resize = findChild(page, "windowsResizeOnBorder");
        const grab = findChild(page, "windowsExtendBorderGrabArea");
        const hover = findChild(page, "windowsHoverIconOnBorder");
        const corner = findChild(page, "windowsResizeCorner");
        const snap = findChild(page, "windowsSnapEnabled");
        const overlap = findChild(page, "windowsSnapBorderOverlap");
        const monitorGap = findChild(page, "windowsSnapMonitorGap");
        const respectGaps = findChild(page, "windowsSnapRespectGaps");
        const windowGap = findChild(page, "windowsSnapWindowGap");
        const follow = findChild(page, "windowsFollowMouse");
        const refocus = findChild(page, "windowsMouseRefocus");
        const shrink = findChild(page, "windowsFollowMouseShrink");
        const floatFocus = findChild(
            page, "windowsFloatSwitchOverrideFocus"
        );
        const closeFocus = findChild(page, "windowsFocusOnClose");
        const special = findChild(page, "windowsSpecialFallthrough");
        const noFallback = findChild(page, "windowsNoFocusFallback");
        const modal = findChild(page, "windowsModalParentBlocking");
        const allowPinFullscreen = findChild(
            page, "windowsAllowPinFullscreen"
        );
        const focusPreferredMethod = findChild(
            page, "windowsFocusPreferredMethod"
        );
        const ignoreGroupLock = findChild(
            page, "windowsIgnoreGroupLock"
        );
        const cyclesFullscreen = findChild(
            page, "windowsMovefocusCyclesFullscreen"
        );
        const cyclesGroupfirst = findChild(
            page, "windowsMovefocusCyclesGroupfirst"
        );
        const monitorFallback = findChild(
            page, "windowsWindowDirectionMonitorFallback"
        );
        const anrEnabled = findChild(page, "windowsAnrDialogEnabled");
        const anrThreshold = findChild(page, "windowsAnrMissedPings");
        const sizeLimitsTiled = findChild(page, "windowsSizeLimitsTiled");
        const alwaysFollowOnDnd = findChild(
            page, "windowsAlwaysFollowOnDnd"
        );
        const followMouseThreshold = findChild(
            page, "windowsFollowMouseThreshold"
        );
        const focusOnActivate = findChild(page, "windowsFocusOnActivate");
        const mouseMoveFocusesMonitor = findChild(
            page, "windowsMouseMoveFocusesMonitor"
        );
        const onFocusUnderFullscreen = findChild(
            page, "windowsOnFocusUnderFullscreen"
        );
        const exitWindowRetainsFullscreen = findChild(
            page, "windowsExitWindowRetainsFullscreen"
        );
        const enableSwallow = findChild(page, "windowsEnableSwallow");
        const swallowRegex = findChild(page, "windowsSwallowRegex");
        const swallowExceptionRegex = findChild(
            page, "windowsSwallowExceptionRegex"
        );

        layout.currentIndex = 1;
        layout.activated(1);
        resize.checked = true;
        resize.clicked();
        grab.value = 31;
        grab.valueModified();
        hover.checked = false;
        hover.clicked();
        corner.currentIndex = 4;
        corner.activated(4);
        snap.checked = true;
        snap.clicked();
        overlap.checked = true;
        overlap.clicked();
        monitorGap.value = 23;
        monitorGap.valueModified();
        respectGaps.checked = true;
        respectGaps.clicked();
        windowGap.value = 27;
        windowGap.valueModified();
        refocus.checked = false;
        refocus.clicked();
        shrink.value = 37;
        shrink.valueModified();
        followMouseThreshold.text = "17.125";
        followMouseThreshold.textEdited();
        follow.currentIndex = 3;
        follow.activated(3);
        floatFocus.currentIndex = 2;
        floatFocus.activated(2);
        closeFocus.currentIndex = 2;
        closeFocus.activated(2);
        special.checked = true;
        special.clicked();
        noFallback.checked = true;
        noFallback.clicked();
        modal.checked = false;
        modal.clicked();
        allowPinFullscreen.checked = true;
        allowPinFullscreen.clicked();
        focusPreferredMethod.currentIndex = 1;
        focusPreferredMethod.activated(1);
        ignoreGroupLock.checked = true;
        ignoreGroupLock.clicked();
        cyclesFullscreen.checked = true;
        cyclesFullscreen.clicked();
        cyclesGroupfirst.checked = true;
        cyclesGroupfirst.clicked();
        monitorFallback.checked = false;
        monitorFallback.clicked();
        anrThreshold.value = 13;
        anrThreshold.valueModified();
        anrEnabled.checked = false;
        anrEnabled.clicked();
        sizeLimitsTiled.checked = true;
        sizeLimitsTiled.clicked();
        alwaysFollowOnDnd.checked = false;
        alwaysFollowOnDnd.clicked();
        focusOnActivate.checked = true;
        focusOnActivate.clicked();
        mouseMoveFocusesMonitor.checked = false;
        mouseMoveFocusesMonitor.clicked();
        onFocusUnderFullscreen.currentIndex = 1;
        onFocusUnderFullscreen.activated(1);
        exitWindowRetainsFullscreen.checked = true;
        exitWindowRetainsFullscreen.clicked();
        enableSwallow.checked = true;
        enableSwallow.clicked();
        swallowRegex.text = "^(kitty|Alacritty)$";
        swallowRegex.editingFinished();
        swallowExceptionRegex.text = "^scratch$";
        swallowExceptionRegex.editingFinished();

        compare(page.draftValue(page.layoutId), "master");
        compare(page.draftValue(page.resizeOnBorderId), true);
        compare(page.draftValue(page.extendBorderGrabAreaId), 31);
        compare(page.draftValue(page.hoverIconOnBorderId), false);
        compare(page.draftValue(page.resizeCornerId), 4);
        compare(page.draftValue(page.snapEnabledId), true);
        compare(page.draftValue(page.snapBorderOverlapId), true);
        compare(page.draftValue(page.snapMonitorGapId), 23);
        compare(page.draftValue(page.snapRespectGapsId), true);
        compare(page.draftValue(page.snapWindowGapId), 27);
        compare(page.draftValue(page.followMouseId), 3);
        compare(page.draftValue(page.mouseRefocusId), false);
        compare(page.draftValue(page.followMouseShrinkId), 37);
        compare(page.draftValue(page.floatSwitchOverrideFocusId), 2);
        compare(page.draftValue(page.focusOnCloseId), 2);
        compare(page.draftValue(page.specialFallthroughId), true);
        compare(page.draftValue(page.noFocusFallbackId), true);
        compare(page.draftValue(page.modalParentBlockingId), false);
        compare(page.draftValue(page.allowPinFullscreenId), true);
        compare(page.draftValue(page.focusPreferredMethodId), 1);
        compare(page.draftValue(page.ignoreGroupLockId), true);
        compare(page.draftValue(page.movefocusCyclesFullscreenId), true);
        compare(page.draftValue(page.movefocusCyclesGroupfirstId), true);
        compare(
            page.draftValue(page.windowDirectionMonitorFallbackId), false
        );
        compare(page.draftValue(page.anrDialogEnabledId), false);
        compare(page.draftValue(page.anrMissedPingsId), 13);
        compare(page.draftValue(page.sizeLimitsTiledId), true);
        compare(page.draftValue(page.alwaysFollowOnDndId), false);
        compare(page.draftValue(page.followMouseThresholdId), 17.125);
        compare(followMouseThreshold.enabled, false);
        compare(page.draftValue(page.focusOnActivateId), true);
        compare(page.draftValue(page.mouseMoveFocusesMonitorId), false);
        compare(page.draftValue(page.onFocusUnderFullscreenId), 1);
        compare(page.draftValue(page.exitWindowRetainsFullscreenId), true);
        compare(page.draftValue(page.enableSwallowId), true);
        compare(
            page.draftValue(page.swallowRegexId),
            "^(kitty|Alacritty)$"
        );
        compare(
            page.draftValue(page.swallowExceptionRegexId), "^scratch$"
        );
        compare(anrThreshold.enabled, false);
        compare(page.draftDirty, true);
        compare(page.draftValid, true);
        compare(refocus.enabled, false);
        compare(shrink.enabled, false);

        const preview = findChild(page, "windowsLayoutPreview");
        verify(preview !== null);
        compare(preview.layoutMode, "master");
        compare(preview.resizeOnBorder, true);
        compare(preview.snapEnabled, true);
        compare(preview.motionStory, "master-stack");
    }

    function test_windowsExpandedControlsPreserveDeepAndNonStepValues() {
        const baseline = windowsDefaults();
        baseline["hyprland.general.float_gaps"] = [0, 5, -6, 7];
        baseline["hyprland.layout.single_window_aspect_ratio"] = [16.5, 9.25];
        baseline["hyprland.layout.single_window_aspect_ratio_tolerance"] = 0.373;
        baseline["hyprland.dwindle.default_split_ratio"] = 1.037;
        baseline["hyprland.master.mfact"] = 0.573;
        baseline["hyprland.scrolling.column_width"] = 0.537;
        baseline["hyprland.group.groupbar.rounding_power"] = 2.573;
        baseline["hyprland.group.groupbar.gradient_rounding_power"] = 3.257;
        baseline["hyprland.input.follow_mouse_threshold"] =
            123456.7890123;

        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWindowsPage(page, baseline);
        waitForRendering(page);
        wait(0);

        compare(page.trustedDefinitionsValid, true);
        compare(page.trustedValuesValid, true);
        compare(page.draftValid, true);
        compare(page.expectedOptionIds.length, 110);
        compare(
            JSON.stringify(page.draftValue(page.floatGapsId)),
            JSON.stringify([0, 5, -6, 7])
        );
        compare(
            JSON.stringify(page.draftValue(page.singleWindowAspectRatioId)),
            JSON.stringify([16.5, 9.25])
        );
        compare(
            findChild(page, "windowsAspectRatioTolerance").value, 0.373
        );
        compare(
            findChild(page, "windowsDwindleDefaultSplitRatio").value, 1.037
        );
        compare(findChild(page, "windowsMasterFactor").value, 0.573);
        compare(
            findChild(page, "windowsScrollingColumnWidth").value, 0.537
        );
        compare(
            findChild(page, "windowsGroupbarRoundingPower").value, 2.573
        );
        compare(
            findChild(page, "windowsGroupbarGradientRoundingPower").value,
            3.257
        );
        compare(
            findChild(page, "windowsFollowMouseThreshold").text,
            "123456.7890123"
        );

        baseline["hyprland.general.float_gaps"][1] = 999;
        baseline["hyprland.layout.single_window_aspect_ratio"][0] = 999;
        compare(
            JSON.stringify(page.draftValue(page.floatGapsId)),
            JSON.stringify([0, 5, -6, 7])
        );
        compare(
            JSON.stringify(page.draftValue(page.singleWindowAspectRatioId)),
            JSON.stringify([16.5, 9.25])
        );

        const updates = {};
        updates[page.floatGapsId] = [1, 2, 3, 4];
        updates[page.workspaceGapsId] = 11;
        updates[page.singleWindowAspectRatioId] = [16, 9];
        updates[page.singleWindowAspectRatioToleranceId] = 0.37;
        updates[page.dwindleDefaultSplitRatioId] = 1.03;
        updates[page.dwindleForceSplitId] = 2;
        updates[page.dwindlePermanentDirectionOverrideId] = true;
        updates[page.dwindlePreciseMouseMoveId] = true;
        updates[page.dwindlePreserveSplitId] = true;
        updates[page.dwindleSmartResizingId] = false;
        updates[page.dwindleSmartSplitId] = true;
        updates[page.dwindleSpecialScaleFactorId] = 0.83;
        updates[page.dwindleSplitBiasId] = 1;
        updates[page.dwindleSplitWidthMultiplierId] = 1.07;
        updates[page.dwindleUseActiveForSplitsId] = false;
        updates[page.masterAllowSmallSplitId] = true;
        updates[page.masterAlwaysKeepPositionId] = true;
        updates[page.masterCenterIgnoresReservedId] = true;
        updates[page.masterCenterFallbackId] = "right";
        updates[page.masterDropAtCursorId] = false;
        updates[page.masterFocusOnCloseId] = true;
        updates[page.masterFactorId] = 0.57;
        updates[page.masterNewOnActiveId] = "before";
        updates[page.masterNewOnTopId] = true;
        updates[page.masterNewStatusId] = "master";
        updates[page.masterOrientationId] = "center";
        updates[page.masterCenterSlaveCountId] = 4;
        updates[page.masterSmartResizingId] = false;
        updates[page.masterSpecialScaleFactorId] = 0.73;
        updates[page.scrollingColumnWidthId] = 0.53;
        updates[page.scrollingDirectionId] = "left";
        updates[page.scrollingFocusFitMethodId] = 0;
        updates[page.scrollingFollowFocusId] = false;
        updates[page.scrollingFollowMinimumVisibleId] = 0.37;
        updates[page.scrollingFullscreenOneColumnId] = false;
        updates[page.scrollingWrapFocusId] = false;
        updates[page.scrollingWrapSwapColumnId] = false;
        updates[page.scrollingMoveSnapCursorId] = false;
        updates[page.scrollingMoveSnapGridId] = false;
        updates[page.groupAutoGroupId] = false;
        updates[page.groupInsertAfterCurrentId] = false;
        updates[page.groupFocusRemovedWindowId] = false;
        updates[page.groupDragIntoGroupId] = 2;
        updates[page.groupMergeGroupsOnDragId] = false;
        updates[page.groupMergeGroupsOnGroupbarId] = false;
        updates[page.groupMergeFloatedIntoTiledOnGroupbarId] = true;
        updates[page.groupOnMoveToWorkspaceId] = true;
        updates[page.groupbarEnabledId] = false;
        updates[page.groupbarDisableWhenOnlyId] = true;
        updates[page.groupbarFontFamilyId] = "Iosevka Term";
        updates[page.groupbarFontWeightActiveId] = 735;
        updates[page.groupbarFontWeightInactiveId] = 325;
        updates[page.groupbarFontSizeId] = 17;
        updates[page.groupbarGradientsId] = true;
        updates[page.groupbarHeightId] = 29;
        updates[page.groupbarIndicatorGapId] = 7;
        updates[page.groupbarIndicatorHeightId] = 6;
        updates[page.groupbarStackedId] = true;
        updates[page.groupbarPriorityId] = 5;
        updates[page.groupbarRenderTitlesId] = false;
        updates[page.groupbarScrollingId] = false;
        updates[page.groupbarMiddleClickCloseId] = false;
        updates[page.groupbarRoundingId] = 9;
        updates[page.groupbarRoundingPowerId] = 2.573;
        updates[page.groupbarGradientRoundingId] = 8;
        updates[page.groupbarGradientRoundingPowerId] = 3.257;
        updates[page.groupbarRoundOnlyEdgesId] = false;
        updates[page.groupbarGradientRoundOnlyEdgesId] = false;
        updates[page.groupbarGapsOutId] = 11;
        updates[page.groupbarGapsInId] = 9;
        updates[page.groupbarKeepUpperGapId] = false;
        updates[page.groupbarTextOffsetId] = -7;
        updates[page.groupbarTextPaddingId] = 13;
        updates[page.groupbarBlurId] = true;
        updates[page.allowPinFullscreenId] = true;
        updates[page.focusPreferredMethodId] = 1;
        updates[page.ignoreGroupLockId] = true;
        updates[page.movefocusCyclesFullscreenId] = true;
        updates[page.movefocusCyclesGroupfirstId] = true;
        updates[page.windowDirectionMonitorFallbackId] = false;
        updates[page.anrDialogEnabledId] = false;
        updates[page.anrMissedPingsId] = 17;
        updates[page.sizeLimitsTiledId] = true;
        updates[page.alwaysFollowOnDndId] = false;
        updates[page.focusOnActivateId] = true;
        updates[page.mouseMoveFocusesMonitorId] = false;
        updates[page.onFocusUnderFullscreenId] = 1;
        updates[page.exitWindowRetainsFullscreenId] = true;
        updates[page.enableSwallowId] = true;
        updates[page.swallowRegexId] = "^(kitty|Alacritty)$";
        updates[page.swallowExceptionRegexId] = "^scratch$";
        updates[page.followMouseThresholdId] = 999999.123456;
        compare(Object.keys(updates).length, 92);
        for (const id of Object.keys(updates)) {
            if (id === page.followMouseThresholdId)
                page.setExactDecimalDraftValue(id, updates[id]);
            else
                page.setDraftValue(id, updates[id]);
        }
        for (const id of Object.keys(updates)) {
            if (Array.isArray(updates[id])) {
                compare(JSON.stringify(page.draftValue(id)),
                        JSON.stringify(updates[id]), id);
            } else {
                compare(page.draftValue(id), updates[id], id);
            }
        }
        compare(page.draftDirty, true);
        compare(page.draftValid, true);
        compare(findChild(page, "windowsDwindleForceSplit").enabled, false);
        compare(findChild(page, "windowsMasterCenterFallback").enabled, true);
        compare(findChild(page, "windowsMasterCenterSlaveCount").enabled, true);
        compare(
            findChild(page, "windowsMasterCenterIgnoresReserved").enabled,
            true
        );
        compare(
            findChild(page, "windowsScrollingFollowMinimumVisible").enabled,
            false
        );

        let submitted = null;
        page.saveRequested.connect(function(values) { submitted = values; });
        findChild(page, "saveWindowsButton").clicked();
        verify(submitted !== null);
        compare(Object.keys(submitted).length, 110);
        for (const id of Object.keys(updates)) {
            if (Array.isArray(updates[id])) {
                compare(JSON.stringify(submitted[id]),
                        JSON.stringify(updates[id]), id);
            } else {
                compare(submitted[id], updates[id], id);
            }
        }
    }

    function test_windowsMixedZeroRatioCanBeRepairedWithoutDraftLoss() {
        const baseline = windowsDefaults();
        baseline["hyprland.layout.single_window_aspect_ratio"] = [0, 9];
        baseline["hyprland.general.float_gaps"] = [0, 5, -6, 7];
        baseline["hyprland.dwindle.smart_split"] = true;
        baseline["hyprland.dwindle.force_split"] = 2;
        baseline["hyprland.master.orientation"] = "left";
        baseline["hyprland.master.center_master_fallback"] = "right";
        baseline["hyprland.master.slave_count_for_center_master"] = 4;
        baseline["hyprland.scrolling.follow_focus"] = false;
        baseline["hyprland.scrolling.follow_min_visible"] = 0.37;

        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWindowsPage(page, baseline);
        waitForRendering(page);
        wait(0);

        compare(page.trustedValuesValid, true);
        compare(page.controlsEnabled, true);
        compare(page.draftValid, false);
        compare(findChild(page, "saveWindowsButton").enabled, false);
        compare(findChild(page, "windowsAspectRatioError").visible, true);
        compare(findChild(page, "windowsAspectRatioTolerance").enabled, false);
        compare(findChild(page, "windowsDwindleForceSplit").enabled, false);
        compare(findChild(page, "windowsMasterCenterFallback").enabled, false);
        compare(findChild(page, "windowsMasterCenterSlaveCount").enabled, false);
        compare(
            findChild(page, "windowsMasterCenterIgnoresReserved").enabled,
            false
        );
        compare(
            findChild(page, "windowsScrollingFollowMinimumVisible").enabled,
            false
        );
        compare(page.draftValue(page.dwindleForceSplitId), 2);
        compare(page.draftValue(page.masterCenterFallbackId), "right");
        compare(page.draftValue(page.masterCenterSlaveCountId), 4);
        compare(page.draftValue(page.scrollingFollowMinimumVisibleId), 0.37);

        const width = findChild(page, "windowsAspectRatioWidth");
        verify(width !== null);
        width.text = "16";
        width.editingFinished();
        compare(
            JSON.stringify(page.draftValue(page.singleWindowAspectRatioId)),
            JSON.stringify([16, 9])
        );
        compare(page.draftValid, true);
        compare(findChild(page, "windowsAspectRatioError").visible, false);
        compare(findChild(page, "windowsAspectRatioTolerance").enabled, true);
        compare(findChild(page, "saveWindowsButton").enabled, true);
        compare(
            JSON.stringify(page.draftValue(page.floatGapsId)),
            JSON.stringify([0, 5, -6, 7])
        );

        page.setDraftValue(page.dwindleSmartSplitId, false);
        compare(findChild(page, "windowsDwindleForceSplit").enabled, true);
        page.setDraftValue(page.masterOrientationId, "center");
        compare(findChild(page, "windowsMasterCenterFallback").enabled, true);
        compare(findChild(page, "windowsMasterCenterSlaveCount").enabled, true);
        page.setDraftValue(page.scrollingFollowFocusId, true);
        compare(
            findChild(page, "windowsScrollingFollowMinimumVisible").enabled,
            true
        );

        const topGap = findChild(page, "windowsFloatGapTop");
        verify(topGap !== null);
        topGap.text = "-8";
        topGap.editingFinished();
        compare(
            JSON.stringify(page.draftValue(page.floatGapsId)),
            JSON.stringify([-8, 5, -6, 7])
        );
    }

    function test_windowsDraftActionsPreserveDisabledDependentValues() {
        const baseline = windowsDefaults();
        baseline["hyprland.general.layout"] = "scrolling";
        baseline["hyprland.general.resize_on_border"] = true;
        baseline["hyprland.general.extend_border_grab_area"] = 37;
        baseline["hyprland.general.hover_icon_on_border"] = false;
        baseline["hyprland.general.resize_corner"] = 3;
        baseline["hyprland.general.snap.enabled"] = true;
        baseline["hyprland.general.snap.border_overlap"] = true;
        baseline["hyprland.general.snap.monitor_gap"] = 21;
        baseline["hyprland.general.snap.respect_gaps"] = true;
        baseline["hyprland.general.snap.window_gap"] = 22;
        baseline["hyprland.input.mouse_refocus"] = false;
        baseline["hyprland.input.follow_mouse_shrink"] = 41;
        baseline["hyprland.group.drag_into_group"] = 2;
        baseline["hyprland.group.merge_groups_on_drag"] = false;
        baseline["hyprland.group.merge_groups_on_groupbar"] = true;
        baseline[
            "hyprland.group.merge_floated_into_tiled_on_groupbar"
        ] = true;
        baseline["hyprland.misc.enable_anr_dialog"] = true;
        baseline["hyprland.misc.anr_missed_pings"] = 13;
        baseline["hyprland.misc.enable_swallow"] = true;
        baseline["hyprland.misc.swallow_regex"] = "^Terminal$";
        baseline["hyprland.misc.swallow_exception_regex"] = "^scratch$";
        baseline["hyprland.input.follow_mouse_threshold"] = 41.75;

        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWindowsPage(page, baseline);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.resizeOnBorderId, false);
        page.setDraftValue(page.snapEnabledId, false);
        page.setDraftValue(page.followMouseId, 0);
        page.setDraftValue(page.groupDragIntoGroupId, 0);
        page.setDraftValue(page.anrDialogEnabledId, false);
        page.setDraftValue(page.enableSwallowId, false);
        compare(page.draftDirty, true);
        compare(findChild(page, "windowsExtendBorderGrabArea").enabled, false);
        compare(findChild(page, "windowsSnapMonitorGap").enabled, false);
        compare(findChild(page, "windowsMouseRefocus").enabled, false);
        compare(findChild(page, "windowsResizeCorner").enabled, true);
        compare(findChild(page, "windowsAnrMissedPings").enabled, false);
        compare(findChild(page, "windowsSwallowRegex").enabled, false);
        compare(
            findChild(page, "windowsSwallowExceptionRegex").enabled,
            false
        );
        compare(
            findChild(page, "windowsGroupMergeGroupsOnDrag").enabled,
            false
        );
        compare(
            findChild(page, "windowsGroupMergeGroupsOnGroupbar").enabled,
            false
        );
        compare(
            findChild(
                page, "windowsGroupMergeFloatedIntoTiledOnGroupbar"
            ).enabled,
            false
        );

        let saveCount = 0;
        let submitted = null;
        page.saveRequested.connect(function(values) {
            ++saveCount;
            submitted = values;
        });
        findChild(page, "saveWindowsButton").clicked();
        compare(saveCount, 1);
        compare(Object.keys(submitted).length, 110);
        compare(submitted[page.resizeOnBorderId], false);
        compare(submitted[page.extendBorderGrabAreaId], 37);
        compare(submitted[page.hoverIconOnBorderId], false);
        compare(submitted[page.resizeCornerId], 3);
        compare(submitted[page.snapEnabledId], false);
        compare(submitted[page.snapBorderOverlapId], true);
        compare(submitted[page.snapMonitorGapId], 21);
        compare(submitted[page.snapRespectGapsId], true);
        compare(submitted[page.snapWindowGapId], 22);
        compare(submitted[page.followMouseId], 0);
        compare(submitted[page.mouseRefocusId], false);
        compare(submitted[page.followMouseShrinkId], 41);
        compare(submitted[page.groupDragIntoGroupId], 0);
        compare(submitted[page.groupMergeGroupsOnDragId], false);
        compare(submitted[page.groupMergeGroupsOnGroupbarId], true);
        compare(
            submitted[page.groupMergeFloatedIntoTiledOnGroupbarId], true
        );
        compare(submitted[page.anrDialogEnabledId], false);
        compare(submitted[page.anrMissedPingsId], 13);
        compare(submitted[page.enableSwallowId], false);
        compare(submitted[page.swallowRegexId], "^Terminal$");
        compare(
            submitted[page.swallowExceptionRegexId], "^scratch$"
        );
        compare(submitted[page.followMouseThresholdId], 41.75);
        findChild(page, "saveWindowsButton").clicked();
        compare(saveCount, 1);

        const secondWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(secondWindow !== null);
        const secondPage = secondWindow.page;
        configureWindowsPage(secondPage, baseline);
        waitForRendering(secondPage);
        wait(0);
        secondPage.setDraftValue(secondPage.modalParentBlockingId, false);
        secondPage.setDraftValue(secondPage.enableSwallowId, false);
        secondPage.setDraftValue(
            secondPage.swallowRegexId, "^Changed$"
        );
        secondPage.setExactDecimalDraftValue(
            secondPage.followMouseThresholdId, 9.5
        );
        findChild(secondPage, "discardWindowsDraftButton").clicked();
        compare(secondPage.draftDirty, false);
        compare(secondPage.draftValue(
            secondPage.modalParentBlockingId
        ), true);
        compare(secondPage.draftValue(secondPage.enableSwallowId), true);
        compare(
            secondPage.draftValue(secondPage.swallowRegexId), "^Terminal$"
        );
        compare(
            secondPage.draftValue(secondPage.swallowExceptionRegexId),
            "^scratch$"
        );
        compare(
            secondPage.draftValue(secondPage.followMouseThresholdId),
            41.75
        );

        findChild(secondPage, "resetWindowsDefaultsButton").clicked();
        compare(secondPage.draftDirty, true);
        const defaults = windowsDefaults();
        for (const id of secondPage.expectedOptionIds)
            compare(secondPage.draftValue(id), defaults[id], id);
        compare(findChild(secondPage, "windowsSwallowRegex").enabled, false);
        compare(
            findChild(secondPage, "windowsSwallowExceptionRegex").enabled,
            false
        );
    }

    function test_windowsDormantDragFollowValueSurvivesDraftActions() {
        const baseline = windowsDefaults();
        baseline["hyprland.misc.always_follow_on_dnd"] = false;

        const saveWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(saveWindow !== null);
        const savePage = saveWindow.page;
        configureWindowsPage(savePage, baseline);
        waitForRendering(savePage);
        const saveControl = findChild(
            savePage, "windowsAlwaysFollowOnDnd"
        );
        compare(saveControl.enabled, false);
        compare(saveControl.checked, false);
        savePage.setDraftValue(savePage.sizeLimitsTiledId, true);
        let submitted = null;
        savePage.saveRequested.connect(function(values) {
            submitted = values;
        });
        findChild(savePage, "saveWindowsButton").clicked();
        verify(submitted !== null);
        compare(submitted[savePage.alwaysFollowOnDndId], false);

        const discardWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(discardWindow !== null);
        const discardPage = discardWindow.page;
        configureWindowsPage(discardPage, baseline);
        waitForRendering(discardPage);
        discardPage.setDraftValue(discardPage.alwaysFollowOnDndId, true);
        findChild(discardPage, "discardWindowsDraftButton").clicked();
        compare(discardPage.draftValue(
            discardPage.alwaysFollowOnDndId
        ), false);
        compare(findChild(
            discardPage, "windowsAlwaysFollowOnDnd"
        ).enabled, false);

        const resetWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(resetWindow !== null);
        const resetPage = resetWindow.page;
        configureWindowsPage(resetPage, baseline);
        waitForRendering(resetPage);
        findChild(resetPage, "resetWindowsDefaultsButton").clicked();
        compare(resetPage.draftValue(resetPage.alwaysFollowOnDndId), true);
        compare(findChild(
            resetPage, "windowsAlwaysFollowOnDnd"
        ).enabled, false);
    }

    function test_windowsSwallowPatternClearAndInvalidSavePreserveDraft() {
        const baseline = windowsDefaults();
        baseline["hyprland.misc.enable_swallow"] = true;
        baseline["hyprland.misc.swallow_regex"] = "^Terminal$";
        baseline["hyprland.misc.swallow_exception_regex"] = "^scratch$";

        const clearWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(clearWindow !== null);
        const clearPage = clearWindow.page;
        configureWindowsPage(clearPage, baseline);
        waitForRendering(clearPage);
        const clearPrimary = findChild(clearPage, "windowsSwallowRegex");
        const clearException = findChild(
            clearPage, "windowsSwallowExceptionRegex"
        );
        clearPrimary.text = "";
        clearPrimary.editingFinished();
        clearException.text = "";
        clearException.editingFinished();
        compare(clearPage.draftValue(clearPage.swallowRegexId), "");
        compare(
            clearPage.draftValue(clearPage.swallowExceptionRegexId), ""
        );
        let clearedSubmission = null;
        clearPage.saveRequested.connect(function(values) {
            clearedSubmission = values;
        });
        findChild(clearPage, "saveWindowsButton").clicked();
        verify(clearedSubmission !== null);
        compare(Object.keys(clearedSubmission).length, 110);
        compare(clearedSubmission[clearPage.enableSwallowId], true);
        compare(clearedSubmission[clearPage.swallowRegexId], "");
        compare(clearedSubmission[clearPage.swallowExceptionRegexId], "");

        const invalidWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(invalidWindow !== null);
        const invalidPage = invalidWindow.page;
        configureWindowsPage(invalidPage, baseline);
        waitForRendering(invalidPage);
        const invalidPrimary = findChild(
            invalidPage, "windowsSwallowRegex"
        );
        invalidPrimary.text = "(";
        invalidPrimary.editingFinished();
        compare(invalidPage.draftValue(invalidPage.swallowRegexId), "(");
        compare(invalidPage.draftValid, true);
        let invalidSubmission = null;
        invalidPage.saveRequested.connect(function(values) {
            invalidSubmission = values;
        });
        findChild(invalidPage, "saveWindowsButton").clicked();
        verify(invalidSubmission !== null);
        compare(Object.keys(invalidSubmission).length, 110);
        compare(invalidSubmission[invalidPage.swallowRegexId], "(");
        compare(invalidPage.saveSubmitted, true);

        invalidPage.windowsErrorName =
            "org.hyprshelld.Client.Compositor.Error.InvalidWindows";
        invalidPage.windowsErrorMessage =
            "hyprland.misc.swallow_regex requires valid RE2 syntax";
        wait(0);
        compare(invalidPage.saveSubmitted, false);
        compare(invalidPage.draftValue(invalidPage.swallowRegexId), "(");
        compare(invalidPrimary.text, "(");
        compare(invalidPage.draftDirty, true);
        compare(invalidPage.externalChangeWhileEditing, false);
        compare(
            invalidPage.windowsValues[invalidPage.swallowRegexId],
            "^Terminal$"
        );
    }

    function test_windowsRejectsBadDefinitionsValuesAndNonfiniteNumbers() {
        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWindowsPage(page);
        waitForRendering(page);
        wait(0);

        const definitions = windowsDefinitions();
        definitions[2].max = 101;
        page.windowsOptions = definitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const groupbarDefinitions = windowsDefinitions();
        groupbarDefinitions[67].maxLength = 4095;
        page.windowsOptions = groupbarDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const bindingDefinitions = windowsDefinitions();
        bindingDefinitions[92].defaultValue = true;
        page.windowsOptions = bindingDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const anrDefinitions = windowsDefinitions();
        anrDefinitions[99].max = 21;
        page.windowsOptions = anrDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        const fullscreenDefinitions = windowsDefinitions();
        fullscreenDefinitions[104].choices[2].value = 3;
        page.windowsOptions = fullscreenDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        for (const index of [100, 101, 102, 103, 105, 106]) {
            const booleanDefinitions = windowsDefinitions();
            booleanDefinitions[index].defaultValue =
                !booleanDefinitions[index].defaultValue;
            page.windowsOptions = booleanDefinitions;
            wait(0);
            compare(page.trustedDefinitionsValid, false);
            compare(page.controlsEnabled, false);
        }
        for (const mutation of [
                 definition => { definition.defaultValue = 1; },
                 definition => { definition.min = -1; },
                 definition => { definition.max = 3; },
                 definition => { definition.choices.reverse(); }
             ]) {
            const enumDefinitions = windowsDefinitions();
            mutation(enumDefinitions[104]);
            page.windowsOptions = enumDefinitions;
            wait(0);
            compare(page.trustedDefinitionsValid, false);
            compare(page.controlsEnabled, false);
        }
        for (const index of [107, 108]) {
            for (const mutation of [
                     definition => { definition.defaultValue = 1; },
                     definition => { definition.type = "boolean"; },
                     definition => { definition.control = "toggle"; },
                     definition => { definition.maxLength = 4095; }
                 ]) {
                const stringDefinitions = windowsDefinitions();
                mutation(stringDefinitions[index]);
                page.windowsOptions = stringDefinitions;
                wait(0);
                compare(page.trustedDefinitionsValid, false);
                compare(page.controlsEnabled, false);
            }
        }
        for (const mutation of [
                 definition => { definition.type = "integer"; },
                 definition => { definition.control = "spinBox"; },
                 definition => { definition.defaultValue = 1; },
                 definition => { definition.min = -1; },
                 definition => { definition.max = 999999; },
                 definition => { definition.step = 0.1; },
                 definition => { definition.risk = "caution"; },
                 definition => { delete definition.risk; }
             ]) {
            const thresholdDefinitions = windowsDefinitions();
            mutation(thresholdDefinitions[109]);
            page.windowsOptions = thresholdDefinitions;
            wait(0);
            compare(page.trustedDefinitionsValid, false);
            compare(page.controlsEnabled, false);
        }
        const reorderedDefinitions = windowsDefinitions();
        const priorLast = reorderedDefinitions[108];
        reorderedDefinitions[108] = reorderedDefinitions[109];
        reorderedDefinitions[109] = priorLast;
        page.windowsOptions = reorderedDefinitions;
        wait(0);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);

        page.windowsOptions = windowsDefinitions();
        const badMaps = [];
        let invalid = windowsDefaults();
        delete invalid[page.layoutId];
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid["hyprland.general.unknown"] = true;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.layoutId] = "columns";
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.resizeOnBorderId] = 1;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.extendBorderGrabAreaId] = 15.5;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.extendBorderGrabAreaId] = NaN;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.snapMonitorGapId] = Infinity;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.snapWindowGapId] = -Infinity;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.extendBorderGrabAreaId] = -1;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.snapMonitorGapId] = 101;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.followMouseShrinkId] = 301;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.resizeCornerId] = "1";
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.resizeCornerId] = true;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.followMouseId] = 1.5;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.floatSwitchOverrideFocusId] = 3;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.focusOnCloseId] = -1;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.floatGapsId] = [0, 1, 2];
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.floatGapsId] = [0, 1, 2, 3.5];
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.floatGapsId] = [0, 1, 2, Number.MAX_SAFE_INTEGER + 1];
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.singleWindowAspectRatioId] = [16, NaN];
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.singleWindowAspectRatioId] = [16, 1001];
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.dwindleDefaultSplitRatioId] = 1.91;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.masterOrientationId] = "diagonal";
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.scrollingColumnWidthId] = Infinity;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarEnabledId] = 1;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarHeightId] = 14.5;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarHeightId] = 65;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarFontWeightActiveId] = -1;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarFontWeightActiveId] = 2147483648;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarFontWeightActiveId] = 400.5;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarFontWeightActiveId] = "400";
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarFontWeightActiveId] = true;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarRoundingPowerId] = NaN;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarGradientRoundingPowerId] = Infinity;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarRoundingPowerId] = 1.99;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarGradientRoundingPowerId] = 10.01;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarFontFamilyId] = "bad\u0000font";
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.groupbarFontFamilyId] = "x".repeat(4097);
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.allowPinFullscreenId] = 1;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.focusPreferredMethodId] = 2;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.movefocusCyclesGroupfirstId] = "true";
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.anrDialogEnabledId] = 1;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.sizeLimitsTiledId] = 1;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.alwaysFollowOnDndId] = "true";
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.focusOnActivateId] = 1;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.mouseMoveFocusesMonitorId] = 1;
        badMaps.push(invalid);
        for (const invalidValue of [
                 -1, 3, 1.5, "1", true, NaN, Infinity
             ]) {
            invalid = windowsDefaults();
            invalid[page.onFocusUnderFullscreenId] = invalidValue;
            badMaps.push(invalid);
        }
        invalid = windowsDefaults();
        invalid[page.exitWindowRetainsFullscreenId] = 1;
        badMaps.push(invalid);
        invalid = windowsDefaults();
        invalid[page.enableSwallowId] = 1;
        badMaps.push(invalid);
        for (const id of [
                 page.swallowRegexId, page.swallowExceptionRegexId
             ]) {
            invalid = windowsDefaults();
            invalid[id] = 1;
            badMaps.push(invalid);
            invalid = windowsDefaults();
            invalid[id] = "bad\u0000pattern";
            badMaps.push(invalid);
            invalid = windowsDefaults();
            invalid[id] = "x".repeat(4097);
            badMaps.push(invalid);
        }
        for (const invalidValue of [0, 21, 5.5, "5", true, NaN, Infinity]) {
            invalid = windowsDefaults();
            invalid[page.anrMissedPingsId] = invalidValue;
            badMaps.push(invalid);
        }
        for (const invalidValue of [
                 -0.000001, 1000000.000001, "0", true, NaN,
                 Infinity, -Infinity
             ]) {
            invalid = windowsDefaults();
            invalid[page.followMouseThresholdId] = invalidValue;
            badMaps.push(invalid);
        }
        for (const values of badMaps)
            compare(page.validateValues(values), false);

        const boundaryValues = windowsDefaults();
        boundaryValues[page.layoutId] = "monocle";
        boundaryValues[page.extendBorderGrabAreaId] = 100;
        boundaryValues[page.resizeCornerId] = 4;
        boundaryValues[page.snapMonitorGapId] = 0;
        boundaryValues[page.snapWindowGapId] = 100;
        boundaryValues[page.followMouseId] = 3;
        boundaryValues[page.followMouseShrinkId] = 300;
        boundaryValues[page.floatSwitchOverrideFocusId] = 2;
        boundaryValues[page.focusOnCloseId] = 2;
        boundaryValues[page.floatGapsId] = [
            Number.MIN_SAFE_INTEGER, 0, 1, Number.MAX_SAFE_INTEGER
        ];
        boundaryValues[page.workspaceGapsId] = 100;
        boundaryValues[page.singleWindowAspectRatioId] = [1000, 1000];
        boundaryValues[page.singleWindowAspectRatioToleranceId] = 0.373;
        boundaryValues[page.dwindleDefaultSplitRatioId] = 1.037;
        boundaryValues[page.masterFactorId] = 0.573;
        boundaryValues[page.scrollingColumnWidthId] = 0.537;
        boundaryValues[page.groupbarFontFamilyId] = "x".repeat(4096);
        boundaryValues[page.groupbarFontWeightActiveId] = 0;
        boundaryValues[page.groupbarFontWeightInactiveId] = 2147483647;
        boundaryValues[page.groupbarFontSizeId] = 64;
        boundaryValues[page.groupbarPriorityId] = 6;
        boundaryValues[page.groupbarTextOffsetId] = -20;
        boundaryValues[page.groupbarTextPaddingId] = 22;
        boundaryValues[page.groupbarRoundingPowerId] = 2.573;
        boundaryValues[page.groupbarGradientRoundingPowerId] = 2.573;
        boundaryValues[page.allowPinFullscreenId] = true;
        boundaryValues[page.focusPreferredMethodId] = 1;
        boundaryValues[page.ignoreGroupLockId] = true;
        boundaryValues[page.movefocusCyclesFullscreenId] = true;
        boundaryValues[page.movefocusCyclesGroupfirstId] = true;
        boundaryValues[page.windowDirectionMonitorFallbackId] = false;
        boundaryValues[page.anrDialogEnabledId] = false;
        boundaryValues[page.anrMissedPingsId] = 20;
        boundaryValues[page.sizeLimitsTiledId] = true;
        boundaryValues[page.alwaysFollowOnDndId] = false;
        boundaryValues[page.focusOnActivateId] = true;
        boundaryValues[page.mouseMoveFocusesMonitorId] = false;
        boundaryValues[page.onFocusUnderFullscreenId] = 0;
        boundaryValues[page.exitWindowRetainsFullscreenId] = true;
        boundaryValues[page.enableSwallowId] = true;
        boundaryValues[page.swallowRegexId] = "a".repeat(4096);
        boundaryValues[page.swallowExceptionRegexId] = "b".repeat(4096);
        boundaryValues[page.followMouseThresholdId] = 999999.999999;
        compare(page.validateValues(boundaryValues), true);
        page.windowsValues = boundaryValues;
        wait(0);
        page.reviewProjection();
        compare(page.trustedValuesValid, true);

        const invalidRe2ShapeValues = windowsDefaults();
        invalidRe2ShapeValues[page.enableSwallowId] = true;
        invalidRe2ShapeValues[page.swallowRegexId] = "(";
        invalidRe2ShapeValues[page.swallowExceptionRegexId] = "[";
        compare(page.validateValues(invalidRe2ShapeValues), true);

        const invalidValues = windowsDefaults();
        invalidValues[page.followMouseShrinkId] = 301;
        page.windowsValues = invalidValues;
        wait(0);
        compare(page.trustedValuesValid, false);
        compare(page.controlsEnabled, false);
        compare(findChild(page, "saveWindowsButton").enabled, false);
        verify(String(findChild(page, "windowsStatusMessage").text)
            .includes("trusted Windows & Layout contract"));
    }

    function test_windowsInvalidNewerProjectionPreservesDraftLosslessly() {
        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWindowsPage(page);
        page.revisionToken = "9007199254740992";
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.layoutId, "scrolling");
        page.setDraftValue(page.snapWindowGapId, 27);
        compare(page.draftDirty, true);
        compare(page.synchronizedRevisionToken, "9007199254740992");

        const newer = windowsDefaults();
        newer[page.layoutId] = "master";
        newer[page.snapMonitorGapId] = 31;
        page.windowsProjectionAvailable = false;
        page.windowsAvailable = false;
        page.windowsValues = ({ broken: true });
        page.revisionToken = "9007199254740993";
        wait(0);

        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.layoutId), "scrolling");
        compare(page.draftValue(page.snapWindowGapId), 27);
        compare(page.synchronizedRevisionToken, "9007199254740992");
        const loadCurrent = findChild(page, "loadCurrentWindowsButton");
        verify(loadCurrent !== null);
        compare(loadCurrent.visible, true);
        compare(loadCurrent.enabled, false);
        compare(findChild(page, "saveWindowsButton").enabled, false);
        verify(String(findChild(page, "windowsStatusMessage").text)
            .includes("trusted Windows & Layout contract"));

        page.windowsValues = newer;
        page.windowsProjectionAvailable = true;
        wait(0);
        compare(page.windowsAvailable, false);
        compare(page.trustedValuesValid, true);
        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.layoutId), "scrolling");
        compare(loadCurrent.enabled, true);
        loadCurrent.clicked();
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.layoutId), "master");
        compare(page.draftValue(page.snapMonitorGapId), 31);
        compare(page.synchronizedRevisionToken, "9007199254740993");
    }

    function test_windowsOwnSavedRevisionReconcilesAfterApplyFailure() {
        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWindowsPage(page);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.layoutId, "master");
        page.setDraftValue(page.snapWindowGapId, 29);
        page.setExactDecimalDraftValue(
            page.followMouseThresholdId, 0.812345
        );
        const submitted = page.clone(page.draftValues);
        let requestCount = 0;
        page.saveRequested.connect(function() { ++requestCount; });
        page.submitDraft();
        compare(requestCount, 1);
        compare(page.saveSubmitted, true);

        page.busyOperation = "windows-save";
        page.busy = true;
        page.serviceAvailable = false;
        page.windowsProjectionAvailable = false;
        page.windowsAvailable = false;
        page.windowsValues = ({});
        page.revisionToken = "8";
        page.busy = false;
        page.busyOperation = "";
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, true);
        compare(page.draftValue(page.layoutId), "master");
        compare(page.draftValue(page.followMouseThresholdId), 0.812345);

        page.serviceAvailable = true;
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, true);
        compare(page.draftValue(page.snapWindowGapId), 29);
        compare(page.draftValue(page.followMouseThresholdId), 0.812345);

        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.windowsValues = submitted;
        page.windowsProjectionAvailable = true;
        wait(0);
        compare(page.windowsAvailable, false);
        compare(page.windowsProjectionAvailable, true);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, false);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.layoutId), "master");
        compare(page.draftValue(page.snapWindowGapId), 29);
        compare(page.draftValue(page.followMouseThresholdId), 0.812345);
        compare(page.synchronizedRevisionToken, "8");
        const retry = findChild(page, "retryApplyWindowsButton");
        verify(retry !== null);
        compare(retry.visible, true);
        compare(retry.enabled, true);
    }

    function test_windowsGatesUnsafeStatesAndScopesErrors() {
        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWindowsPage(page);
        waitForRendering(page);
        wait(0);
        const control = findChild(page, "windowsDefaultLayout");
        const save = findChild(page, "saveWindowsButton");
        const status = findChild(page, "windowsStatusMessage");
        verify(control !== null);
        verify(save !== null);
        verify(status !== null);
        compare(page.controlsEnabled, true);

        page.busyOperation = "windows-save";
        page.busy = true;
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("Saving"));
        page.busyOperation = "windows-apply";
        verify(String(status.text).includes("Applying and verifying"));
        page.busy = false;
        page.busyOperation = "";
        page.sharedMutationBusy = true;
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("shared compositor setting"));
        page.sharedMutationBusy = false;
        page.sharedApplySafe = false;
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("verified activation point"));
        page.sharedApplySafe = true;
        page.confirmationState = "awaiting-confirmation";
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("display test is active"));
        page.confirmationState = "idle";
        page.writable = false;
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("read-only"));
        page.writable = true;
        page.managementState = "unmanaged";
        page.windowsAvailable = false;
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("takeover from Displays"));
        compare(findChild(page, "windowsOpenDisplaysButton").visible, true);
        page.managementState = "managed";
        page.windowsAvailable = true;
        page.windowsErrorName = "org.example.Windows";
        page.windowsErrorMessage = "Injected Windows-only failure.";
        verify(String(status.text).includes("Injected Windows-only failure."));
        page.windowsErrorName = "";
        page.windowsErrorMessage = "";
        page.sharedErrorName = "org.example.Apply";
        page.sharedErrorMessage = "Injected shared Apply failure.";
        verify(String(status.text).includes("Injected shared Apply failure."));
        page.sharedErrorName = "";
        page.sharedErrorMessage = "";
        page.revisionToken = "";
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("exact compositor revision token"));
        page.revisionToken = "7";
        page.catalogAvailable = false;
        compare(page.controlsEnabled, false);
        page.catalogAvailable = true;
        page.windowsAvailable = false;
        compare(page.controlsEnabled, false);
        compare(control.enabled, false);
        compare(save.enabled, false);
    }

    function test_windowsRetainedRevisionUsesCancelFirstWholeRecovery() {
        const baseline = windowsDefaults();
        baseline["hyprland.input.follow_mouse_threshold"] = 0.625;
        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWindowsPage(page, baseline);
        page.windowsAvailable = false;
        page.appliedRevision = 6;
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.recoveryAvailable = true;
        waitForRendering(page);
        wait(0);

        let retryCount = 0;
        let recoveryCount = 0;
        page.retryApplyRequested.connect(function() { ++retryCount; });
        page.recoveryRequested.connect(function() { ++recoveryCount; });
        const retry = findChild(page, "retryApplyWindowsButton");
        const recover = findChild(page, "recoverWindowsButton");
        const dialog = findChild(page, "windowsRecoveryDialog");
        const warning = findChild(page, "windowsRecoveryWarning");
        const cancel = findChild(page, "cancelWindowsRecoveryButton");
        const confirm = findChild(page, "confirmWindowsRecoveryButton");
        verify(retry !== null);
        verify(recover !== null);
        verify(dialog !== null);
        verify(warning !== null);
        verify(cancel !== null);
        verify(confirm !== null);
        compare(retry.visible, true);
        compare(recover.visible, true);
        compare(page.draftValue(page.followMouseThresholdId), 0.625);
        retry.clicked();
        compare(retryCount, 1);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        tryCompare(cancel, "activeFocus", true);
        compare(recoveryCount, 0);
        verify(String(warning.text).includes("not limited to Windows & Layout"));
        verify(String(warning.text).includes("every pending compositor"));
        keyClick(Qt.Key_Escape);
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 0);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        cancel.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 0);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        confirm.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 1);
        compare(confirm.enabled, false);
        confirm.clicked();
        compare(recoveryCount, 1);
        compare(page.draftValue(page.followMouseThresholdId), 0.625);
    }

    function test_windowsActionsReachableAtMinimumWindow() {
        const testWindow = createTemporaryObject(
            windowsLayoutPageComponent,
            this,
            { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWindowsPage(page);
        page.setDraftValue(page.layoutId, "master");
        page.setExactDecimalDraftValue(
            page.followMouseThresholdId, "1e0"
        );
        waitForRendering(page);
        wait(0);

        const sticky = findChild(page, "windowsLayoutStickyPreview");
        const preview = findChild(page, "windowsLayoutPreview");
        const disclaimer = findChild(
            page, "windowsLayoutPreviewDisclaimer"
        );
        const motion = findChild(page, "toggleWindowsLayoutMotionButton");
        const scroll = findChild(page, "windowsOptionsScrollView");
        const content = findChild(page, "windowsOptionsContent");
        const save = findChild(page, "saveWindowsButton");
        const thresholdRow = findChild(
            page, "windowsFollowMouseThresholdRow"
        );
        const threshold = findChild(
            page, "windowsFollowMouseThreshold"
        );
        const thresholdValidation = findChild(
            page, "windowsFollowMouseThresholdValidation"
        );
        const cards = [
            findChild(page, "windowsLayoutCard"),
            findChild(page, "windowsSpacingCard"),
            findChild(page, "windowsEngineCard"),
            findChild(page, "windowsGroupsCard"),
            findChild(page, "windowsGroupbarBehaviorCard"),
            findChild(page, "windowsGroupbarLayoutCard"),
            findChild(page, "windowsGroupbarTitlesCard"),
            findChild(page, "windowsGroupbarBackgroundCard"),
            findChild(page, "windowsResizeCard"),
            findChild(page, "windowsSnapCard"),
            findChild(page, "windowsFocusCard"),
            findChild(page, "windowsSwallowingCard"),
            findChild(page, "windowsUnresponsiveApplicationsCard")
        ];
        verify(sticky !== null);
        verify(preview !== null);
        verify(disclaimer !== null);
        verify(motion !== null);
        verify(scroll !== null);
        verify(content !== null);
        verify(save !== null);
        verify(thresholdRow !== null);
        verify(threshold !== null);
        verify(thresholdValidation !== null);
        for (const card of cards)
            verify(card !== null);
        compare(page.compactPreview, true);
        verify(String(disclaimer.text).includes("group bars"));
        verify(String(disclaimer.text).includes("fullscreen"));
        verify(String(disclaimer.text).includes("swallowing"));
        verify(motion.implicitHeight >= 44);
        verify(scroll.contentItem.contentHeight > scroll.height);
        verify(scroll.height >= 100,
               "Windows options viewport height: " + scroll.height);
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width <= scroll.contentWidth + 0.01);
        verify(preview.width > 0);
        compare(thresholdRow.controlWidth, 160);
        compare(threshold.inputValid, false);
        compare(thresholdValidation.visible, true);
        compare(save.enabled, false);

        const stickyBefore = sticky.mapToItem(page, 0, 0);
        const maximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        verify(maximumContentY > 0);
        const scrollPosition = scroll.mapToItem(page, 0, 0);
        for (const name of [
                 "windowsFollowMouseThreshold",
                 "windowsOnFocusUnderFullscreen",
                 "windowsExitWindowRetainsFullscreen",
                 "windowsEnableSwallow",
                 "windowsSwallowRegex",
                 "windowsSwallowExceptionRegex"
             ]) {
            const control = findChild(page, name);
            verify(control !== null, "Missing compact control " + name);
            const contentPosition = control.mapToItem(content, 0, 0);
            verify(contentPosition.x >= 0, name + " starts outside content");
            verify(contentPosition.x + control.width
                <= content.width + 0.01, name + " overflows content");
            const targetY = Math.max(0, Math.min(
                maximumContentY,
                contentPosition.y + control.height / 2 - scroll.height / 2
            ));
            scroll.contentItem.contentY = targetY;
            tryCompare(scroll.contentItem, "contentY", targetY);
            const pagePosition = control.mapToItem(page, 0, 0);
            verify(pagePosition.y + control.height > scrollPosition.y,
                   name + " is above the scroll viewport");
            verify(pagePosition.y < scrollPosition.y + scroll.height,
                   name + " is below the scroll viewport");
        }
        const errorPosition = thresholdValidation.mapToItem(content, 0, 0);
        verify(errorPosition.x >= 0);
        verify(
            errorPosition.x + thresholdValidation.width
                <= content.width + 0.01,
            "Focus movement threshold error label overflows content"
        );
        const errorTargetY = Math.max(0, Math.min(
            maximumContentY,
            errorPosition.y + thresholdValidation.height / 2
                - scroll.height / 2
        ));
        scroll.contentItem.contentY = errorTargetY;
        tryCompare(scroll.contentItem, "contentY", errorTargetY);
        const errorPagePosition = thresholdValidation.mapToItem(page, 0, 0);
        verify(errorPagePosition.y + thresholdValidation.height
            > scrollPosition.y);
        verify(errorPagePosition.y < scrollPosition.y + scroll.height);
        threshold.text = "0.5";
        threshold.textEdited();
        wait(0);
        compare(threshold.inputValid, true);
        compare(thresholdValidation.visible, false);
        compare(save.enabled, true);
        const correctedMaximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        scroll.contentItem.contentY = correctedMaximumContentY;
        tryCompare(
            scroll.contentItem, "contentY", correctedMaximumContentY
        );
        const stickyAfter = sticky.mapToItem(page, 0, 0);
        verify(Math.abs(stickyAfter.x - stickyBefore.x) <= 0.01);
        verify(Math.abs(stickyAfter.y - stickyBefore.y) <= 0.01);
        const savePosition = save.mapToItem(page, 0, 0);
        verify(savePosition.x >= 0);
        verify(savePosition.x + save.width <= page.width + 0.01);
        verify(savePosition.y >= 0);
        verify(savePosition.y + save.height <= page.height + 0.01);
        compare(save.enabled, true);
    }

    function test_workspacesUsesExactControlsDependenciesDraftAndTargets() {
        const baseline = workspacesDefaults();
        baseline["hyprland.gestures.workspace_swipe_cancel_ratio"] = 0.37;
        baseline["hyprland.binds.allow_workspace_cycles"] = true;
        baseline["hyprland.binds.hide_special_on_workspace_change"] = true;
        baseline["hyprland.binds.workspace_back_and_forth"] = true;
        baseline["hyprland.binds.workspace_center_on"] = 0;
        baseline["hyprland.cursor.warp_on_change_workspace"] = 1;
        baseline["hyprland.cursor.warp_on_toggle_special"] = 2;
        const testWindow = createTemporaryObject(
            workspacesPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWorkspacesPage(page, baseline);
        waitForRendering(page);
        wait(0);

        compare(page.trustedDefinitionsValid, true);
        compare(page.trustedValuesValid, true);
        compare(page.controlsEnabled, true);
        compare(page.expectedOptionIds.length, 21);
        const definitions = workspacesDefinitions();
        compare(definitions.length, 21);
        compare(
            JSON.stringify(definitions.map(option => option.id)),
            JSON.stringify(page.expectedOptionIds)
        );
        compare(JSON.stringify(definitions.slice(15)), JSON.stringify([
            {
                id: page.allowWorkspaceCyclesId,
                type: "boolean", control: "toggle", defaultValue: false
            },
            {
                id: page.hideSpecialOnWorkspaceChangeId,
                type: "boolean", control: "toggle", defaultValue: false
            },
            {
                id: page.workspaceBackAndForthId,
                type: "boolean", control: "toggle", defaultValue: false
            },
            {
                id: page.workspaceCenterOnId,
                type: "enum", control: "select", defaultValue: 1,
                choices: [
                    { label: "0", value: 0 },
                    { label: "1", value: 1 }
                ],
                min: 0, max: 1
            },
            {
                id: page.warpOnChangeWorkspaceId,
                type: "enum", control: "select", defaultValue: 0,
                choices: [
                    { label: "0", value: 0 },
                    { label: "1", value: 1 },
                    { label: "2", value: 2 }
                ],
                min: 0, max: 2
            },
            {
                id: page.warpOnToggleSpecialId,
                type: "enum", control: "select", defaultValue: 0,
                choices: [
                    { label: "0", value: 0 },
                    { label: "1", value: 1 },
                    { label: "2", value: 2 }
                ],
                min: 0, max: 2
            }
        ]));
        compare(Object.keys(workspacesDefaults()).length, 21);
        const cards = [
            findChild(page, "workspacesBehaviorCard"),
            findChild(page, "workspacesSwitchingHistoryCard"),
            findChild(page, "workspacesPointerPlacementCard"),
            findChild(page, "workspacesLaunchCard"),
            findChild(page, "workspacesSwipeCard")
        ];
        for (const card of cards)
            verify(card !== null);
        for (let index = 1; index < cards.length; ++index) {
            verify(cards[index].mapToItem(page, 0, 0).y
                > cards[index - 1].mapToItem(page, 0, 0).y);
        }
        const controlNames = [
            "workspacesWraparound",
            "workspacesSwipeCancelRatio",
            "workspacesSwipeCreateNew",
            "workspacesSwipeDirectionLock",
            "workspacesSwipeDirectionLockThreshold",
            "workspacesSwipeDistance",
            "workspacesSwipeForever",
            "workspacesSwipeInvert",
            "workspacesSwipeMinimumSpeed",
            "workspacesSwipeTouch",
            "workspacesSwipeTouchInvert",
            "workspacesSwipeUseRelative",
            "workspacesCloseSpecialOnEmpty",
            "workspacesInitialTracking",
            "workspacesInitialTokenTimeout",
            "workspacesAllowWorkspaceCycles",
            "workspacesHideSpecialOnWorkspaceChange",
            "workspacesBackAndForth",
            "workspacesCenterOn",
            "workspacesWarpOnChangeWorkspace",
            "workspacesWarpOnToggleSpecial"
        ];
        for (const name of controlNames) {
            const control = findChild(page, name);
            verify(control !== null, "Missing control " + name);
            verify(control.implicitHeight >= 44,
                   name + " must provide a 44px interaction target");
            verify(String(control.Accessible.name).length > 0);
        }
        const actionNames = [
            "refreshWorkspacesButton",
            "workspacesOpenDisplaysButton",
            "loadCurrentWorkspacesButton",
            "retryApplyWorkspacesButton",
            "recoverWorkspacesButton",
            "discardWorkspacesDraftButton",
            "resetWorkspacesDefaultsButton",
            "saveWorkspacesButton",
            "cancelWorkspacesRecoveryButton",
            "confirmWorkspacesRecoveryButton"
        ];
        for (const name of actionNames) {
            const control = findChild(page, name);
            verify(control !== null, "Missing action " + name);
            verify(control.implicitHeight >= 44,
                   name + " must provide a 44px interaction target");
        }
        verify(findChild(page, "workspacesLayoutPreview") === null);
        verify(findChild(page, "workspacesStickyPreview") === null);
        verify(String(findChild(page, "workspacesSwipeBindingCopy").text)
            .includes("do not create a touchpad gesture binding"));
        const pointerCopy = String(
            findChild(page, "workspacesPointerPlacementCopy").text
        );
        verify(pointerCopy.includes("cursor:no_warps"));
        verify(pointerCopy.includes("Force bypasses"));
        verify(pointerCopy.includes("cursor:persistent_warps"));
        verify(pointerCopy.includes("do not change"));

        const allowCycles = findChild(
            page, "workspacesAllowWorkspaceCycles"
        );
        const hideSpecial = findChild(
            page, "workspacesHideSpecialOnWorkspaceChange"
        );
        const backAndForth = findChild(page, "workspacesBackAndForth");
        const centerOn = findChild(page, "workspacesCenterOn");
        const warpOnChange = findChild(
            page, "workspacesWarpOnChangeWorkspace"
        );
        const warpOnToggle = findChild(
            page, "workspacesWarpOnToggleSpecial"
        );
        compare(centerOn.model.length, 2);
        verify(String(hideSpecial.parent.description)
            .includes("moving the active workspace to another output"));
        verify(!String(hideSpecial.parent.description)
            .includes("moving a window to another workspace"));
        compare(centerOn.model[0], "Workspace center");
        compare(centerOn.model[1], "Last active window");
        compare(centerOn.currentIndex, 0);
        compare(warpOnChange.model.length, 3);
        compare(warpOnChange.model[0], "Disabled");
        compare(warpOnChange.model[1], "Enabled");
        compare(warpOnChange.model[2], "Force");
        compare(warpOnChange.currentIndex, 1);
        compare(warpOnToggle.currentIndex, 2);

        allowCycles.checked = false;
        allowCycles.clicked();
        compare(page.draftValue(page.allowWorkspaceCyclesId), false);
        compare(page.draftValue(page.workspaceBackAndForthId), true);
        compare(page.draftValue(page.warpOnChangeWorkspaceId), 1);
        hideSpecial.checked = false;
        hideSpecial.clicked();
        compare(page.draftValue(page.hideSpecialOnWorkspaceChangeId), false);
        compare(page.draftValue(page.warpOnToggleSpecialId), 2);
        backAndForth.checked = false;
        backAndForth.clicked();
        compare(page.draftValue(page.workspaceBackAndForthId), false);
        compare(page.draftValue(page.workspaceCenterOnId), 0);
        centerOn.currentIndex = 1;
        centerOn.activated(1);
        compare(page.draftValue(page.workspaceCenterOnId), 1);
        compare(page.draftValue(page.warpOnChangeWorkspaceId), 1);
        warpOnChange.currentIndex = 2;
        warpOnChange.activated(2);
        compare(page.draftValue(page.warpOnChangeWorkspaceId), 2);
        compare(page.draftValue(page.warpOnToggleSpecialId), 2);
        warpOnToggle.currentIndex = 1;
        warpOnToggle.activated(1);
        compare(page.draftValue(page.warpOnToggleSpecialId), 1);
        compare(page.draftValue(page.warpOnChangeWorkspaceId), 2);
        for (const control of [
                 allowCycles, hideSpecial, backAndForth, centerOn,
                 warpOnChange, warpOnToggle
             ]) {
            compare(control.enabled, true);
        }

        const cancelRatio = findChild(page, "workspacesSwipeCancelRatio");
        compare(cancelRatio.value, 0.37);
        compare(findChild(page, "workspacesInitialTokenTimeout").enabled, true);
        compare(
            findChild(page, "workspacesSwipeDirectionLockThreshold").enabled,
            true
        );
        compare(findChild(page, "workspacesSwipeTouchInvert").enabled, false);
        cancelRatio.value = 0.42;
        cancelRatio.moved();
        compare(page.draftValue(page.swipeCancelRatioId), 0.4);

        const updates = {};
        updates[page.workspaceWraparoundId] = true;
        updates[page.swipeCancelRatioId] = 0.37;
        updates[page.swipeCreateNewId] = false;
        updates[page.swipeDirectionLockId] = false;
        updates[page.swipeDirectionLockThresholdId] = 25;
        updates[page.swipeDistanceId] = 500;
        updates[page.swipeForeverId] = true;
        updates[page.swipeInvertId] = false;
        updates[page.swipeMinimumSpeedId] = 44;
        updates[page.swipeTouchId] = true;
        updates[page.swipeTouchInvertId] = true;
        updates[page.swipeUseRelativeId] = true;
        updates[page.closeSpecialOnEmptyId] = false;
        updates[page.initialWorkspaceTrackingId] = 2;
        updates[page.initialWorkspaceTokenTimeoutId] = 75;
        updates[page.allowWorkspaceCyclesId] = true;
        updates[page.hideSpecialOnWorkspaceChangeId] = true;
        updates[page.workspaceBackAndForthId] = true;
        updates[page.workspaceCenterOnId] = 0;
        updates[page.warpOnChangeWorkspaceId] = 2;
        updates[page.warpOnToggleSpecialId] = 1;
        compare(Object.keys(updates).length, 21);
        for (const id of Object.keys(updates))
            page.setDraftValue(id, updates[id]);
        for (const id of Object.keys(updates))
            compare(page.draftValue(id), updates[id], id);
        compare(findChild(page, "workspacesInitialTokenTimeout").enabled, false);
        compare(
            findChild(page, "workspacesSwipeDirectionLockThreshold").enabled,
            false
        );
        compare(findChild(page, "workspacesSwipeTouchInvert").enabled, true);
        compare(page.draftValue(page.initialWorkspaceTokenTimeoutId), 75);
        compare(page.draftValue(page.swipeDirectionLockThresholdId), 25);

        let submitted = null;
        page.saveRequested.connect(function(values) { submitted = values; });
        findChild(page, "saveWorkspacesButton").clicked();
        verify(submitted !== null);
        compare(Object.keys(submitted).length, 21);
        for (const id of Object.keys(updates))
            compare(submitted[id], updates[id], id);

        const secondWindow = createTemporaryObject(
            workspacesPageComponent, this
        );
        verify(secondWindow !== null);
        const secondPage = secondWindow.page;
        configureWorkspacesPage(secondPage, baseline);
        secondPage.setDraftValue(secondPage.workspaceWraparoundId, true);
        findChild(secondPage, "discardWorkspacesDraftButton").clicked();
        compare(secondPage.draftDirty, false);
        for (const id of secondPage.expectedOptionIds)
            compare(secondPage.draftValue(id), baseline[id], id);
        findChild(secondPage, "resetWorkspacesDefaultsButton").clicked();
        compare(secondPage.draftDirty, true);
        const defaults = workspacesDefaults();
        for (const id of secondPage.expectedOptionIds)
            compare(secondPage.draftValue(id), defaults[id], id);

    }

    function test_workspaceRulesAggregateDraftOrderingValidationAndAtomicSave() {
        const testWindow = createTemporaryObject(
            workspacesPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWorkspacesPage(page);
        waitForRendering(page);
        wait(0);

        compare(page.trustedWorkspaceRulesValid, true);
        compare(page.draftWorkspaceRules.length, 0);
        const tabBar = findChild(page, "workspacesTabBar");
        const behaviorTab = findChild(page, "workspacesBehaviorTab");
        const rulesTab = findChild(page, "workspaceRulesTab");
        verify(tabBar !== null);
        verify(behaviorTab !== null);
        verify(rulesTab !== null);
        verify(behaviorTab.implicitHeight >= 44);
        verify(rulesTab.implicitHeight >= 44);
        tabBar.currentIndex = 1;
        wait(0);
        compare(page.workspacesTabIndex, 1);
        const add = findChild(page, "addWorkspaceRuleButton");
        verify(add !== null);
        verify(add.implicitHeight >= 44);
        add.clicked();
        compare(page.draftWorkspaceRules.length, 1);
        compare(page.draftWorkspaceRules[0].id, "workspace-rule-1");
        compare(page.draftWorkspaceRules[0].enabled, false);
        compare(page.draftWorkspaceRules[0].selector, "");
        compare(page.draftValid, false);
        compare(page.saveEnabled, false);
        const selector = findChild(page, "workspaceRuleSelector");
        const validation = findChild(
            page, "workspaceRuleEditorValidationMessage"
        );
        verify(selector !== null);
        verify(validation !== null);
        verify(selector.implicitHeight >= 44);
        compare(validation.visible, true);

        page.setWorkspaceRuleProperty(
            "workspace-rule-1", "selector", "special:music"
        );
        page.setWorkspaceRuleOverride(
            "workspace-rule-1", "gaps_in", true,
            [-9007199254740991, 0, 1, 9007199254740991]
        );
        page.setWorkspaceRuleOverride(
            "workspace-rule-1", "gaps_out", true, [1, 2, 3, 4]
        );
        page.setWorkspaceRuleOverride(
            "workspace-rule-1", "float_gaps", true, [4, 3, 2, 1]
        );
        page.setWorkspaceRuleOverride(
            "workspace-rule-1", "border_size", true, 9007199254740991
        );
        for (const key of [
                 "no_border", "no_rounding", "decorate", "no_shadow"
             ]) {
            page.setWorkspaceRuleOverride(
                "workspace-rule-1", key, true,
                key === "no_border" || key === "decorate"
            );
        }
        page.setWorkspaceRuleOverride(
            "workspace-rule-1", "default_name", true,
            "Authored workspace"
        );
        page.setWorkspaceRuleOverride(
            "workspace-rule-1", "animation", true,
            "slidefadevert left 37%"
        );
        page.setWorkspaceRuleOverride(
            "workspace-rule-1", "layout_opts", true,
            { orientation: "center", direction: "up" }
        );
        compare(page.draftValid, true);
        compare(page.saveEnabled, true);

        page.closeWorkspaceRuleEditor();
        add.clicked();
        page.setWorkspaceRuleProperty("workspace-rule-2", "selector", "2");
        page.closeWorkspaceRuleEditor();
        waitForRendering(page);
        wait(0);
        compare(page.draftWorkspaceRules.length, 2);
        const moveUp = findChild(page, "moveWorkspaceRuleUpButton1");
        verify(moveUp !== null);
        verify(moveUp.implicitHeight >= 44);
        moveUp.clicked();
        compare(page.draftWorkspaceRules[0].id, "workspace-rule-2");
        compare(page.draftWorkspaceRules[1].id, "workspace-rule-1");

        page.setWorkspaceRuleProperty("workspace-rule-2", "selector", "special:music");
        compare(page.draftValid, false);
        compare(page.saveEnabled, false);
        page.setWorkspaceRuleProperty("workspace-rule-2", "selector", "2");
        compare(page.draftValid, true);
        page.setDraftValue(page.swipeDistanceId, 733);
        let submittedValues = null;
        let submittedRules = null;
        let requestCount = 0;
        page.saveRequested.connect(function(values, rules) {
            ++requestCount;
            submittedValues = values;
            submittedRules = rules;
        });
        findChild(page, "saveWorkspacesFromRulesButton").clicked();
        compare(requestCount, 1);
        compare(submittedValues[page.swipeDistanceId], 733);
        compare(submittedRules.length, 2);
        compare(submittedRules[0].id, "workspace-rule-2");
        compare(submittedRules[1].id, "workspace-rule-1");
        compare(Object.keys(submittedRules[1].overrides).length, 11);
        compare(submittedRules[1].overrides.border_size, 9007199254740991);
    }

    function test_workspaceRulesRejectCanonicalStringAndCollectionBoundaries() {
        const testWindow = createTemporaryObject(
            workspacesPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWorkspacesPage(page);

        const valid = workspaceRule("workspace-valid", "1");
        compare(page.validateWorkspaceRuleCollection([valid], false), true);
        let candidate = page.clone(valid);
        candidate.monitor = "desc:" + "D".repeat(256);
        compare(page.validateWorkspaceRuleRecord(candidate, false), true);
        candidate.monitor = "desc:" + "D".repeat(257);
        compare(page.validateWorkspaceRuleRecord(candidate, false), false);
        candidate = page.clone(valid);
        candidate.monitor = "desc: Dell ";
        compare(page.validateWorkspaceRuleRecord(candidate, false), false);
        candidate = page.clone(valid);
        candidate.monitor = "desc:e\u0301";
        compare(page.validateWorkspaceRuleRecord(candidate, false), false);
        candidate = page.clone(valid);
        candidate.monitor = "desc:Dell\u200bDisplay";
        compare(page.validateWorkspaceRuleRecord(candidate, false), false);
        candidate = page.clone(valid);
        candidate.overrides.default_name = "e\u0301";
        compare(page.validateWorkspaceRuleRecord(candidate, false), false);
        candidate = page.clone(valid);
        candidate.overrides.default_name = "Dell\u200bDisplay";
        compare(page.validateWorkspaceRuleRecord(candidate, false), false);
        candidate = page.clone(valid);
        candidate.overrides.default_name = "Dell"
            + String.fromCodePoint(0x110BD) + "Display";
        compare(page.validateWorkspaceRuleRecord(candidate, false), false);
        candidate = page.clone(valid);
        candidate.overrides.animation = "slide 00%";
        compare(page.validateWorkspaceRuleRecord(candidate, false), false);
        candidate = page.clone(valid);
        candidate.overrides.border_size = 9007199254740992;
        compare(page.validateWorkspaceRuleRecord(candidate, false), false);
        candidate = page.clone(valid);
        candidate.overrides.gaps_in = [1, 2.5, 3, 4];
        compare(page.validateWorkspaceRuleRecord(candidate, false), false);
        candidate = page.clone(valid);
        candidate.overrides.on_created_empty = "exec evil";
        compare(page.validateWorkspaceRuleRecord(candidate, false), false);

        const maximum = [];
        for (let index = 0; index < 1024; ++index)
            maximum.push(workspaceRule("workspace-" + index, String(index + 1)));
        compare(page.validateWorkspaceRuleCollection(maximum, false), true);
        maximum.push(workspaceRule("workspace-overflow", "name:overflow"));
        compare(page.validateWorkspaceRuleCollection(maximum, false), false);
        const duplicateId = [valid, workspaceRule("workspace-valid", "2")];
        compare(page.validateWorkspaceRuleCollection(duplicateId, false), false);
        const duplicateSelector = [valid, workspaceRule("workspace-two", "1")];
        compare(
            page.validateWorkspaceRuleCollection(duplicateSelector, false),
            false
        );
        const spoof = workspaceRule(
            "hyprshelld.internal.shared-spacing.maximized", "2"
        );
        compare(page.validateWorkspaceRuleRecord(spoof, false), false);
        spoof.id = "workspace-spoof";
        spoof.selector = "f[1]";
        compare(page.validateWorkspaceRuleRecord(spoof, false), false);
    }

    function test_workspaceRulesNewerRevisionConflictsAggregateDraft() {
        const testWindow = createTemporaryObject(
            workspacesPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const rules = [workspaceRule("workspace-one", "1")];
        configureWorkspacesPage(page);
        page.workspaceRules = page.clone(rules);
        page.reviewProjection();
        wait(0);
        compare(page.draftWorkspaceRules, rules);
        page.setDraftValue(page.swipeDistanceId, 451);
        page.setDraftValue(page.allowWorkspaceCyclesId, true);
        page.setDraftValue(page.workspaceCenterOnId, 0);
        page.setDraftValue(page.warpOnChangeWorkspaceId, 2);
        page.setWorkspaceRuleProperty("workspace-one", "layout", "master");
        compare(page.draftDirty, true);

        page.revisionToken = "8";
        page.workspacesValues = page.clone(workspacesDefaults());
        page.workspaceRules = page.clone(rules);
        page.reviewProjection();
        wait(0);
        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.swipeDistanceId), 451);
        compare(page.draftValue(page.allowWorkspaceCyclesId), true);
        compare(page.draftValue(page.workspaceCenterOnId), 0);
        compare(page.draftValue(page.warpOnChangeWorkspaceId), 2);
        compare(page.draftWorkspaceRules[0].layout, "master");
        compare(page.synchronizedRevisionToken, "7");
        compare(findChild(page, "loadCurrentWorkspacesButton").visible, true);

        const cleanWindow = createTemporaryObject(
            workspacesPageComponent, this
        );
        verify(cleanWindow !== null);
        const cleanPage = cleanWindow.page;
        configureWorkspacesPage(cleanPage);
        cleanPage.workspaceRules = cleanPage.clone(rules);
        cleanPage.reviewProjection();
        cleanPage.revisionToken = "8";
        cleanPage.reviewProjection();
        wait(0);
        compare(cleanPage.externalChangeWhileEditing, false);
        compare(cleanPage.draftDirty, false);
        compare(cleanPage.synchronizedRevisionToken, "8");
    }

    function test_workspaceRulesDiscardAndResetAreAggregate() {
        const testWindow = createTemporaryObject(
            workspacesPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const initialRules = [workspaceRule(
            "workspace-initial", "1", { gaps_out: [1, 2, 3, 4] }
        )];
        configureWorkspacesPage(page);
        page.workspaceRules = page.clone(initialRules);
        page.reviewProjection();
        wait(0);

        page.setDraftValue(page.swipeDistanceId, 451);
        page.setDraftValue(page.allowWorkspaceCyclesId, true);
        page.setDraftValue(page.warpOnToggleSpecialId, 2);
        page.setWorkspaceRuleProperty(
            "workspace-initial", "layout", "master"
        );
        compare(page.draftDirty, true);
        page.workspacesTabIndex = 1;
        wait(0);
        const discard = findChild(
            page, "discardWorkspacesDraftFromRulesButton"
        );
        const reset = findChild(
            page, "resetWorkspacesDefaultsFromRulesButton"
        );
        verify(discard !== null);
        verify(reset !== null);
        discard.clicked();
        compare(page.draftDirty, false);
        compare(page.draftValue(page.swipeDistanceId), 300);
        compare(page.draftValue(page.allowWorkspaceCyclesId), false);
        compare(page.draftValue(page.warpOnToggleSpecialId), 0);
        compare(page.draftWorkspaceRules, initialRules);

        page.setDraftValue(page.swipeDistanceId, 451);
        page.setDraftValue(page.allowWorkspaceCyclesId, true);
        page.setDraftValue(page.warpOnToggleSpecialId, 2);
        page.setWorkspaceRuleProperty(
            "workspace-initial", "layout", "master"
        );
        reset.clicked();
        compare(page.draftValue(page.swipeDistanceId), 300);
        compare(page.draftValue(page.allowWorkspaceCyclesId), false);
        compare(page.draftValue(page.warpOnToggleSpecialId), 0);
        compare(page.draftWorkspaceRules.length, 0);
        compare(page.draftDirty, true);
        compare(page.draftValid, true);
    }

    function test_workspacesRejectsBadContractsAndPreservesConflictDraft() {
        const testWindow = createTemporaryObject(
            workspacesPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWorkspacesPage(page);
        page.revisionToken = "9007199254740992";
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        const badDefinitions = [];
        let definitions = workspacesDefinitions();
        definitions.splice(15, 1);
        badDefinitions.push(definitions);
        definitions = workspacesDefinitions();
        definitions.push(booleanDefinition(
            "hyprland.workspaces.unknown", false
        ));
        badDefinitions.push(definitions);
        definitions = workspacesDefinitions();
        definitions[15].type = "integer";
        badDefinitions.push(definitions);
        definitions = workspacesDefinitions();
        definitions[18].defaultValue = 0;
        badDefinitions.push(definitions);
        definitions = workspacesDefinitions();
        definitions[18].choices = [
            { label: "0", value: 0 },
            { label: "2", value: 2 }
        ];
        badDefinitions.push(definitions);
        definitions = workspacesDefinitions();
        definitions[19].max = 3;
        badDefinitions.push(definitions);
        for (const candidate of badDefinitions) {
            page.workspacesOptions = candidate;
            compare(page.trustedDefinitionsValid, false);
            compare(page.controlsEnabled, false);
        }
        page.workspacesOptions = workspacesDefinitions();
        compare(page.trustedDefinitionsValid, true);

        const badMaps = [];
        let invalid = workspacesDefaults();
        delete invalid[page.warpOnToggleSpecialId];
        badMaps.push(invalid);
        invalid = workspacesDefaults();
        invalid["hyprland.workspaces.unknown"] = true;
        badMaps.push(invalid);
        invalid = workspacesDefaults();
        invalid[page.workspaceWraparoundId] = 1;
        badMaps.push(invalid);
        invalid = workspacesDefaults();
        invalid[page.swipeCancelRatioId] = NaN;
        badMaps.push(invalid);
        invalid = workspacesDefaults();
        invalid[page.swipeCancelRatioId] = 1.01;
        badMaps.push(invalid);
        invalid = workspacesDefaults();
        invalid[page.initialWorkspaceTrackingId] = "1";
        badMaps.push(invalid);
        invalid = workspacesDefaults();
        invalid[page.initialWorkspaceTrackingId] = 3;
        badMaps.push(invalid);
        invalid = workspacesDefaults();
        invalid[page.workspaceCenterOnId] = 2;
        badMaps.push(invalid);
        invalid = workspacesDefaults();
        invalid[page.warpOnChangeWorkspaceId] = 3;
        badMaps.push(invalid);
        invalid = workspacesDefaults();
        invalid[page.warpOnToggleSpecialId] = "1";
        badMaps.push(invalid);
        invalid = workspacesDefaults();
        invalid[page.warpOnChangeWorkspaceId] = Infinity;
        badMaps.push(invalid);
        for (const values of badMaps)
            compare(page.validateValues(values), false);

        const arbitrary = workspacesDefaults();
        arbitrary[page.swipeCancelRatioId] = 0.373;
        compare(page.validateValues(arbitrary), true);
        page.workspacesValues = arbitrary;
        wait(0);
        compare(page.trustedValuesValid, true);
        compare(
            findChild(page, "workspacesSwipeCancelRatio").value, 0.373
        );

        page.setDraftValue(page.workspaceWraparoundId, true);
        page.setDraftValue(page.swipeDistanceId, 451);
        page.setDraftValue(page.hideSpecialOnWorkspaceChangeId, true);
        page.setDraftValue(page.workspaceCenterOnId, 0);
        page.setDraftValue(page.warpOnToggleSpecialId, 2);
        compare(page.draftDirty, true);
        page.workspacesProjectionAvailable = false;
        page.workspacesAvailable = false;
        page.workspacesValues = ({ broken: true });
        page.revisionToken = "9007199254740993";
        wait(0);
        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.workspaceWraparoundId), true);
        compare(page.draftValue(page.swipeDistanceId), 451);
        compare(page.draftValue(page.hideSpecialOnWorkspaceChangeId), true);
        compare(page.draftValue(page.workspaceCenterOnId), 0);
        compare(page.draftValue(page.warpOnToggleSpecialId), 2);
        const load = findChild(page, "loadCurrentWorkspacesButton");
        compare(load.visible, true);
        compare(load.enabled, false);
        verify(String(findChild(page, "workspacesStatusMessage").text)
            .includes("trusted Workspaces contract"));

        const newer = workspacesDefaults();
        newer[page.swipeDistanceId] = 600;
        page.workspacesValues = newer;
        page.workspacesProjectionAvailable = true;
        wait(0);
        compare(page.workspacesAvailable, false);
        compare(page.externalChangeWhileEditing, true);
        compare(load.enabled, true);
        load.clicked();
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.swipeDistanceId), 600);
        compare(page.draftValue(page.hideSpecialOnWorkspaceChangeId), false);
        compare(page.draftValue(page.workspaceCenterOnId), 1);
        compare(page.draftValue(page.warpOnToggleSpecialId), 0);
        compare(page.synchronizedRevisionToken, "9007199254740993");
    }

    function test_workspacesOwnRetainedApplyGatesAndRecovery() {
        const testWindow = createTemporaryObject(
            workspacesPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWorkspacesPage(page);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.workspaceWraparoundId, true);
        page.setDraftValue(page.swipeDistanceId, 451);
        page.draftWorkspaceRules = [workspaceRule(
            "workspace-own-save", "special:music",
            { animation: "slidefadevert left 37%" }
        )];
        const submitted = page.clone(page.draftValues);
        const submittedRules = page.clone(page.draftWorkspaceRules);
        let requestCount = 0;
        page.saveRequested.connect(function() { ++requestCount; });
        page.submitDraft();
        compare(requestCount, 1);
        compare(page.saveSubmitted, true);

        page.busyOperation = "workspaces-save";
        page.busy = true;
        page.serviceAvailable = false;
        page.workspacesProjectionAvailable = false;
        page.workspacesAvailable = false;
        page.workspacesValues = ({});
        page.revisionToken = "8";
        page.busy = false;
        page.busyOperation = "";
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, true);

        page.serviceAvailable = true;
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.recoveryAvailable = true;
        page.workspacesValues = submitted;
        page.workspaceRules = submittedRules;
        page.workspacesProjectionAvailable = true;
        page.workspaceRulesProjectionAvailable = true;
        wait(0);
        compare(page.workspacesAvailable, false);
        compare(page.workspacesProjectionAvailable, true);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, false);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.swipeDistanceId), 451);
        compare(page.draftWorkspaceRules, submittedRules);
        compare(page.synchronizedRevisionToken, "8");

        let retryCount = 0;
        let recoveryCount = 0;
        page.retryApplyRequested.connect(function() { ++retryCount; });
        page.recoveryRequested.connect(function() { ++recoveryCount; });
        const retry = findChild(page, "retryApplyWorkspacesButton");
        const recover = findChild(page, "recoverWorkspacesButton");
        const dialog = findChild(page, "workspacesRecoveryDialog");
        const warning = findChild(page, "workspacesRecoveryWarning");
        const cancel = findChild(page, "cancelWorkspacesRecoveryButton");
        const confirm = findChild(page, "confirmWorkspacesRecoveryButton");
        compare(retry.visible, true);
        compare(retry.enabled, true);
        retry.clicked();
        compare(retryCount, 1);
        recover.clicked();
        tryCompare(dialog, "opened", true);
        tryCompare(cancel, "activeFocus", true);
        verify(String(warning.text).includes("not limited to Workspaces"));
        verify(String(warning.text).includes("every pending compositor"));
        cancel.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 0);
        recover.clicked();
        tryCompare(dialog, "opened", true);
        confirm.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 1);
        confirm.clicked();
        compare(recoveryCount, 1);

        page.confirmationState = "awaiting-confirmation";
        compare(page.controlsEnabled, false);
        page.confirmationState = "idle";
        page.sharedMutationBusy = true;
        compare(page.controlsEnabled, false);
        page.sharedMutationBusy = false;
        page.sharedApplySafe = false;
        compare(page.controlsEnabled, false);
        page.sharedApplySafe = true;
        page.writable = false;
        compare(page.controlsEnabled, false);
        page.writable = true;
        page.managementState = "unmanaged";
        compare(page.controlsEnabled, false);
        compare(findChild(page, "workspacesOpenDisplaysButton").visible, true);
        page.managementState = "managed";
        page.revisionToken = "";
        compare(page.controlsEnabled, false);
    }

    function test_workspacesActionsReachableAtMinimumWindow() {
        const testWindow = createTemporaryObject(
            workspacesPageComponent, this,
            { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWorkspacesPage(page);
        page.setDraftValue(page.workspaceWraparoundId, true);
        waitForRendering(page);
        wait(0);

        const scroll = findChild(page, "workspacesOptionsScrollView");
        const content = findChild(page, "workspacesOptionsContent");
        const save = findChild(page, "saveWorkspacesButton");
        const switchingCard = findChild(
            page, "workspacesSwitchingHistoryCard"
        );
        const pointerCard = findChild(
            page, "workspacesPointerPlacementCard"
        );
        verify(scroll !== null);
        verify(content !== null);
        verify(save !== null);
        verify(switchingCard !== null);
        verify(pointerCard !== null);
        compare(switchingCard.parent, content);
        compare(pointerCard.parent, content);
        verify(pointerCard.y > switchingCard.y);
        compare(page.compactPage, true);
        verify(scroll.contentItem.contentHeight > scroll.height);
        verify(scroll.height >= 100);
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width <= scroll.contentWidth + 0.01);
        const maximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        verify(maximumContentY > 0);
        const scrollPosition = scroll.mapToItem(page, 0, 0);
        const compactControls = [
            "workspacesAllowWorkspaceCycles",
            "workspacesHideSpecialOnWorkspaceChange",
            "workspacesBackAndForth",
            "workspacesCenterOn",
            "workspacesWarpOnChangeWorkspace",
            "workspacesWarpOnToggleSpecial"
        ];
        for (const name of compactControls) {
            const control = findChild(page, name);
            verify(control !== null, "Missing compact control " + name);
            const contentPosition = control.mapToItem(content, 0, 0);
            verify(contentPosition.x >= 0, name + " starts outside content");
            verify(contentPosition.x + control.width <= content.width + 0.01,
                   name + " overflows content horizontally");
            const targetY = Math.max(0, Math.min(
                maximumContentY,
                contentPosition.y + control.height / 2 - scroll.height / 2
            ));
            scroll.contentItem.contentY = targetY;
            tryCompare(scroll.contentItem, "contentY", targetY);
            const pagePosition = control.mapToItem(page, 0, 0);
            verify(pagePosition.x >= 0, name + " starts outside page");
            verify(pagePosition.x + control.width <= page.width + 0.01,
                   name + " overflows page horizontally");
            verify(pagePosition.y + control.height > scrollPosition.y,
                   name + " is above the scroll viewport");
            verify(pagePosition.y < scrollPosition.y + scroll.height,
                   name + " is below the scroll viewport");
        }
        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        const savePosition = save.mapToItem(page, 0, 0);
        verify(savePosition.x >= 0);
        verify(savePosition.x + save.width <= page.width + 0.01);
        verify(savePosition.y >= 0);
        verify(savePosition.y + save.height <= page.height + 0.01);
        compare(save.enabled, true);
    }

    function test_workspaceRulesReachableAtMinimumWindow() {
        const testWindow = createTemporaryObject(
            workspacesPageComponent, this,
            { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureWorkspacesPage(page);
        page.workspaceRules = [workspaceRule("workspace-compact", "1")];
        page.reviewProjection();
        page.workspacesTabIndex = 1;
        waitForRendering(page);
        wait(0);

        const outer = findChild(page, "workspacesOptionsScrollView");
        const list = findChild(page, "workspaceRuleList");
        const add = findChild(page, "addWorkspaceRuleButton");
        const edit = findChild(page, "editWorkspaceRuleButton0");
        const remove = findChild(page, "removeWorkspaceRuleButton0");
        const save = findChild(page, "saveWorkspacesFromRulesButton");
        for (const control of [add, edit, remove, save]) {
            verify(control !== null);
            verify(control.implicitHeight >= 44);
        }
        verify(list !== null);
        verify(outer.contentWidth <= outer.availableWidth + 0.01);
        edit.clicked();
        waitForRendering(page);
        wait(0);
        const editor = findChild(page, "workspaceRuleEditorScrollView");
        const editorContent = findChild(page, "workspaceRuleEditorContent");
        const back = findChild(page, "closeWorkspaceRuleEditorButton");
        const done = findChild(page, "doneEditingWorkspaceRuleButton");
        verify(editor !== null);
        verify(editorContent !== null);
        verify(back !== null);
        verify(done !== null);
        verify(back.implicitHeight >= 44);
        verify(done.implicitHeight >= 44);
        verify(editor.contentWidth <= editor.availableWidth + 0.01);
        verify(editor.contentItem.contentHeight > editor.height);
        editor.contentItem.contentY = Math.max(
            0, editor.contentItem.contentHeight - editor.contentItem.height
        );
        tryCompare(
            editor.contentItem, "contentY",
            Math.max(
                0,
                editor.contentItem.contentHeight - editor.contentItem.height
            )
        );
        const donePosition = done.mapToItem(page, 0, 0);
        verify(donePosition.x >= 0);
        verify(donePosition.x + done.width <= page.width + 0.01);
        verify(donePosition.y >= 0);
        verify(donePosition.y + done.height <= page.height + 0.01);
    }

    function test_advancedExactContractControlsSaveDiscardAndReset() {
        const testWindow = createTemporaryObject(
            advancedPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAdvancedPage(page);
        waitForRendering(page);
        wait(0);

        const expectedIds = [
            "hyprland.misc.allow_session_lock_restore",
            "hyprland.misc.lockdead_screen_delay",
            "hyprland.misc.disable_scale_notification",
            "hyprland.misc.render_unfocused_fps",
            "hyprland.misc.screencopy_force_8b",
            "hyprland.misc.disable_hyprland_logo",
            "hyprland.misc.disable_splash_rendering",
            "hyprland.misc.session_lock_xray",
            "hyprland.misc.session_lock_blur",
            "hyprland.xwayland.use_nearest_neighbor",
            "hyprland.render.expand_undersized_textures",
            "hyprland.render.direct_scanout",
            "hyprland.render.fp16_sdr_tf",
            "hyprland.render.xp_mode",
            "hyprland.input-capture.capture_modifiers",
            "hyprland.input-capture.enforce_barriers"
        ];
        compare(page.expectedOptionIds, expectedIds);
        compare(
            page.advancedOptions.map(option => option.id), expectedIds
        );
        compare(
            page.advancedOptions.map(option => option.risk),
            [
                "safe", "safe", "safe", "safe", "safe",
                "safe", "safe", "safe", "safe", "caution", "caution",
                "caution", "caution", "caution", "caution", "caution"
            ]
        );
        compare(Object.keys(advancedDefaults()).length, 16);
        compare(page.trustedDefinitionsValid, true);
        compare(page.trustedValuesValid, true);
        compare(page.draftValid, true);
        compare(page.draftDirty, false);
        compare(page.controlsEnabled, true);
        const fp16SdrDefinition = page.optionById(
            page.fp16SdrTransferId
        );
        verify(fp16SdrDefinition !== null);
        compare(fp16SdrDefinition.type, "enum");
        compare(fp16SdrDefinition.control, "select");
        compare(fp16SdrDefinition.defaultValue, 0);
        compare(fp16SdrDefinition.min, 0);
        compare(fp16SdrDefinition.max, 1);
        compare(fp16SdrDefinition.risk, "caution");
        compare(page.choiceValues(fp16SdrDefinition), [0, 1]);
        const xpModeDefinition = page.optionById(page.xpModeId);
        verify(xpModeDefinition !== null);
        compare(xpModeDefinition.type, "boolean");
        compare(xpModeDefinition.control, "toggle");
        compare(xpModeDefinition.defaultValue, false);
        compare(xpModeDefinition.min, undefined);
        compare(xpModeDefinition.max, undefined);
        compare(xpModeDefinition.risk, "caution");
        compare(page.choiceValues(xpModeDefinition), []);
        const captureModifiersDefinition = page.optionById(
            page.captureModifiersId
        );
        verify(captureModifiersDefinition !== null);
        compare(captureModifiersDefinition.type, "boolean");
        compare(captureModifiersDefinition.control, "toggle");
        compare(captureModifiersDefinition.defaultValue, false);
        compare(captureModifiersDefinition.min, undefined);
        compare(captureModifiersDefinition.max, undefined);
        compare(captureModifiersDefinition.risk, "caution");
        compare(page.choiceValues(captureModifiersDefinition), []);
        const enforceBarriersDefinition = page.optionById(
            page.enforceCaptureBarriersId
        );
        verify(enforceBarriersDefinition !== null);
        compare(enforceBarriersDefinition.type, "boolean");
        compare(enforceBarriersDefinition.control, "toggle");
        compare(enforceBarriersDefinition.defaultValue, true);
        compare(enforceBarriersDefinition.min, undefined);
        compare(enforceBarriersDefinition.max, undefined);
        compare(enforceBarriersDefinition.risk, "caution");
        compare(page.choiceValues(enforceBarriersDefinition), []);

        const cards = [
            findChild(page, "advancedSessionLockCard"),
            findChild(page, "advancedSessionLockRenderingCard"),
            findChild(page, "advancedBackgroundRenderingCard"),
            findChild(page, "advancedWorkspaceUnderlayRenderingCard"),
            findChild(page, "advancedSdrWorkBufferTransferCard"),
            findChild(page, "advancedDirectScanoutRenderingCard"),
            findChild(page, "advancedNativeWaylandResizeCompatibilityCard"),
            findChild(page, "advancedXWaylandCompatibilityCard"),
            findChild(page, "advancedInputCaptureProtocolCard"),
            findChild(page, "advancedDisplayWarningsCard")
        ];
        for (const card of cards)
            verify(card !== null);
        for (let index = 1; index < cards.length; ++index) {
            verify(cards[index].mapToItem(page, 0, 0).y
                > cards[index - 1].mapToItem(page, 0, 0).y);
        }
        verify(findChild(page, "advancedPreview") === null);

        const allowRestore = findChild(
            page, "advancedAllowSessionLockRestore"
        );
        const lockDelay = findChild(
            page, "advancedLockdeadScreenDelay"
        );
        const scaleWarning = findChild(
            page, "advancedDisableScaleNotification"
        );
        const backgroundFps = findChild(
            page, "advancedRenderUnfocusedFps"
        );
        const force8Bit = findChild(
            page, "advancedScreencopyForce8Bit"
        );
        const disableLogo = findChild(
            page, "advancedDisableHyprlandLogo"
        );
        const disableSplash = findChild(
            page, "advancedDisableSplashRendering"
        );
        const lockXray = findChild(page, "advancedSessionLockXray");
        const lockBlur = findChild(page, "advancedSessionLockBlur");
        const nearestNeighbor = findChild(
            page, "advancedXWaylandUseNearestNeighbor"
        );
        const expandUndersizedTextures = findChild(
            page, "advancedExpandUndersizedTextures"
        );
        const directScanout = findChild(
            page, "advancedDirectScanout"
        );
        const fp16SdrTransfer = findChild(
            page, "advancedFp16SdrTransfer"
        );
        const skipWorkspaceUnderlays = findChild(
            page, "advancedSkipWorkspaceUnderlays"
        );
        const captureModifiers = findChild(
            page, "advancedCaptureModifiers"
        );
        const enforceCaptureBarriers = findChild(
            page, "advancedEnforceCaptureBarriers"
        );
        const lockRenderingCopy = findChild(
            page, "advancedSessionLockRenderingCopy"
        );
        const backgroundRenderingCopy = findChild(
            page, "advancedBackgroundRenderingCopy"
        );
        const workspaceUnderlayCopy = findChild(
            page, "advancedWorkspaceUnderlayRenderingCopy"
        );
        const workspaceUnderlayHeading = findChild(
            page, "advancedWorkspaceUnderlayRenderingHeading"
        );
        const workspaceUnderlayCaution = findChild(
            page, "advancedWorkspaceUnderlayRenderingCaution"
        );
        const workspaceUnderlayCautionMessage = findChild(
            page, "advancedWorkspaceUnderlayRenderingCautionMessage"
        );
        const directScanoutCopy = findChild(
            page, "advancedDirectScanoutRenderingCopy"
        );
        const directScanoutHeading = findChild(
            page, "advancedDirectScanoutRenderingHeading"
        );
        const directScanoutCaution = findChild(
            page, "advancedDirectScanoutCaution"
        );
        const directScanoutCautionMessage = findChild(
            page, "advancedDirectScanoutCautionMessage"
        );
        const sdrWorkBufferCopy = findChild(
            page, "advancedSdrWorkBufferTransferCopy"
        );
        const sdrWorkBufferHeading = findChild(
            page, "advancedSdrWorkBufferTransferHeading"
        );
        const sdrWorkBufferCaution = findChild(
            page, "advancedSdrWorkBufferTransferCaution"
        );
        const sdrWorkBufferCautionMessage = findChild(
            page, "advancedSdrWorkBufferTransferCautionMessage"
        );
        const xwaylandCopy = findChild(
            page, "advancedXWaylandCompatibilityCopy"
        );
        const nativeWaylandCopy = findChild(
            page, "advancedNativeWaylandResizeCompatibilityCopy"
        );
        const nativeWaylandHeading = findChild(
            page, "advancedNativeWaylandResizeCompatibilityHeading"
        );
        const nativeWaylandCaution = findChild(
            page, "advancedNativeWaylandResizeCaution"
        );
        const nativeWaylandCautionMessage = findChild(
            page, "advancedNativeWaylandResizeCautionMessage"
        );
        const xwaylandCaution = findChild(
            page, "advancedXWaylandCaution"
        );
        const xwaylandCautionMessage = findChild(
            page, "advancedXWaylandCautionMessage"
        );
        const inputCaptureCopy = findChild(
            page, "advancedInputCaptureProtocolCopy"
        );
        const inputCaptureHeading = findChild(
            page, "advancedInputCaptureProtocolHeading"
        );
        const inputCaptureCaution = findChild(
            page, "advancedInputCaptureProtocolCaution"
        );
        const inputCaptureCautionMessage = findChild(
            page, "advancedInputCaptureProtocolCautionMessage"
        );
        const controls = [
            allowRestore, lockDelay, lockXray, lockBlur, disableLogo,
            disableSplash, expandUndersizedTextures, nearestNeighbor,
            directScanout, fp16SdrTransfer, skipWorkspaceUnderlays,
            captureModifiers, enforceCaptureBarriers, scaleWarning,
            backgroundFps, force8Bit
        ];
        for (const control of controls) {
            verify(control !== null);
            verify(control.implicitHeight >= page.minimumTargetSize);
            verify(String(control.Accessible.name).length > 0);
        }
        for (const copy of [
                 lockRenderingCopy, backgroundRenderingCopy,
                 workspaceUnderlayCopy, workspaceUnderlayCautionMessage,
                 sdrWorkBufferCopy, sdrWorkBufferCautionMessage,
                 directScanoutCopy, directScanoutCautionMessage,
                 nativeWaylandCopy, nativeWaylandCautionMessage,
                 xwaylandCopy, xwaylandCautionMessage,
                 inputCaptureCopy, inputCaptureCautionMessage
             ]) {
            verify(copy !== null);
            verify(String(copy.Accessible.name).length > 0);
        }
        verify(xwaylandCaution !== null);
        verify(workspaceUnderlayHeading !== null);
        compare(
            String(workspaceUnderlayHeading.text),
            "Workspace underlay rendering"
        );
        compare(
            workspaceUnderlayHeading.Accessible.role,
            Accessible.Heading
        );
        verify(workspaceUnderlayCaution !== null);
        compare(
            workspaceUnderlayCautionMessage.Accessible.role,
            Accessible.AlertMessage
        );
        compare(
            String(workspaceUnderlayCaution.background.color), "#33251a"
        );
        verify(directScanoutHeading !== null);
        compare(
            String(directScanoutHeading.text),
            "Direct scanout rendering"
        );
        compare(directScanoutHeading.Accessible.role, Accessible.Heading);
        verify(directScanoutCaution !== null);
        compare(
            directScanoutCautionMessage.Accessible.role,
            Accessible.AlertMessage
        );
        compare(String(directScanoutCaution.background.color), "#33251a");
        verify(sdrWorkBufferHeading !== null);
        compare(
            String(sdrWorkBufferHeading.text),
            "SDR work-buffer transfer"
        );
        compare(sdrWorkBufferHeading.Accessible.role, Accessible.Heading);
        verify(sdrWorkBufferCaution !== null);
        compare(
            sdrWorkBufferCautionMessage.Accessible.role,
            Accessible.AlertMessage
        );
        compare(String(sdrWorkBufferCaution.background.color), "#33251a");
        verify(nativeWaylandHeading !== null);
        compare(
            String(nativeWaylandHeading.text),
            "Native Wayland resize compatibility"
        );
        compare(nativeWaylandHeading.Accessible.role, Accessible.Heading);
        compare(
            String(expandUndersizedTextures.parent.title),
            "Extend undersized surface textures"
        );
        verify(nativeWaylandCaution !== null);
        compare(
            nativeWaylandCautionMessage.Accessible.role,
            Accessible.AlertMessage
        );
        compare(String(nativeWaylandCaution.background.color), "#33251a");
        compare(
            xwaylandCautionMessage.Accessible.role,
            Accessible.AlertMessage
        );
        compare(String(xwaylandCaution.background.color), "#33251a");
        verify(inputCaptureHeading !== null);
        compare(String(inputCaptureHeading.text), "Input capture protocol");
        compare(inputCaptureHeading.Accessible.role, Accessible.Heading);
        verify(inputCaptureCaution !== null);
        compare(
            inputCaptureCautionMessage.Accessible.role,
            Accessible.AlertMessage
        );
        compare(String(inputCaptureCaution.background.color), "#33251a");
        compare(allowRestore.checked, false);
        compare(lockDelay.from, 0);
        compare(lockDelay.to, 5000);
        compare(lockDelay.value, 1000);
        compare(scaleWarning.checked, false);
        compare(backgroundFps.from, 1);
        compare(backgroundFps.to, 120);
        compare(backgroundFps.value, 15);
        compare(force8Bit.checked, true);
        compare(disableLogo.checked, false);
        compare(disableSplash.checked, false);
        compare(lockXray.checked, false);
        compare(lockBlur.checked, false);
        compare(lockBlur.enabled, false);
        compare(expandUndersizedTextures.checked, true);
        compare(nearestNeighbor.checked, true);
        compare(directScanout.currentIndex, 0);
        compare(directScanout.model.length, 3);
        compare(directScanout.model[0], "Disabled");
        compare(directScanout.model[1], "Enabled");
        compare(directScanout.model[2], "Automatic (games only)");
        compare(fp16SdrTransfer.currentIndex, 0);
        compare(fp16SdrTransfer.model.length, 2);
        compare(fp16SdrTransfer.model[0], "Display transfer (default)");
        compare(fp16SdrTransfer.model[1], "Linear");
        compare(skipWorkspaceUnderlays.checked, false);
        compare(captureModifiers.checked, false);
        compare(enforceCaptureBarriers.checked, true);
        compare(captureModifiers.enabled, true);
        compare(enforceCaptureBarriers.enabled, true);
        compare(
            String(skipWorkspaceUnderlays.parent.title),
            "Skip workspace underlays"
        );
        verify(String(allowRestore.parent.description)
            .includes("does not launch or restart"));
        verify(String(lockDelay.parent.description)
            .includes("stops rendering ordinary workspaces"));
        const frameCallbackCopy = String(backgroundFps.parent.description);
        verify(frameCallbackCopy.includes("Rule-marked windows"));
        verify(frameCallbackCopy.includes("not being rendered"));
        const captureCopy = String(force8Bit.parent.description);
        verify(captureCopy.includes("new screen-share session"));
        verify(captureCopy.includes("XRGB8888"));
        verify(captureCopy.includes("Other monitor formats are unchanged"));
        const warningCopy = String(scaleWarning.parent.description);
        verify(warningCopy.includes("does not change display scale"));
        verify(warningCopy.includes("Displays test preview"));
        const logoCopy = String(disableLogo.parent.description);
        verify(logoCopy.includes("whole compositor-owned"));
        verify(logoCopy.includes("compositor background color"));
        verify(logoCopy.includes("Splash text remains separately controlled"));
        verify(logoCopy.includes("user-configured wallpaper"));
        const splashCopy = String(disableSplash.parent.description);
        verify(splashCopy.includes("Suppress only Hyprland's splash text"));
        verify(splashCopy.includes(
            "splash can still appear over the compositor background color"
        ));
        verify(splashCopy.includes("when the fallback image is hidden"));
        verify(splashCopy.includes("neither suppresses that image"));
        verify(splashCopy.includes("user-configured wallpaper"));
        const xrayCopy = String(lockXray.parent.description);
        verify(xrayCopy.includes("ordinary workspaces rendering"));
        verify(xrayCopy.includes("opaque black primer"));
        verify(xrayCopy.includes("does not unlock"));
        verify(xrayCopy.includes("change input"));
        const blurCopy = String(lockBlur.parent.description);
        verify(blurCopy.includes("non-opaque lock-surface pixels"));
        verify(blurCopy.includes("0.55.0 has no"));
        verify(blurCopy.includes("introduced in 0.56.0"));
        verify(blurCopy.includes("present in 0.56.1"));
        verify(blurCopy.includes("retained while X-ray is off"));
        verify(String(lockRenderingCopy.text).includes(
            "session remains locked"
        ));
        verify(String(lockRenderingCopy.text).includes(
            "input routing does not change"
        ));
        verify(String(backgroundRenderingCopy.text).includes(
            "compositor-owned fallback image"
        ));
        const workspaceUnderlayCopyText = String(
            workspaceUnderlayCopy.text
        );
        verify(workspaceUnderlayCopyText.includes("active workspace"));
        verify(workspaceUnderlayCopyText.includes("background"));
        verify(workspaceUnderlayCopyText.includes("lower-layer passes"));
        const workspaceUnderlayCautionCopy = String(
            workspaceUnderlayCautionMessage.text
        );
        verify(workspaceUnderlayCautionCopy.includes(
            "active-workspace render path"
        ));
        verify(workspaceUnderlayCautionCopy.includes(
            "compositor fallback background"
        ));
        verify(workspaceUnderlayCautionCopy.includes(
            "layer-shell background and bottom passes"
        ));
        verify(workspaceUnderlayCautionCopy.includes(
            "post-wallpaper pass"
        ));
        verify(workspaceUnderlayCautionCopy.includes(
            "Windows and layer-shell top and overlay surfaces still render"
        ));
        verify(workspaceUnderlayCautionCopy.includes(
            "no-workspace render path is unchanged"
        ));
        verify(workspaceUnderlayCautionCopy.includes(
            "does not stop those surfaces or their processes"
        ));
        verify(workspaceUnderlayCautionCopy.includes(
            "does not clear prior buffer pixels"
        ));
        verify(workspaceUnderlayCautionCopy.includes(
            "uncovered pixels may be stale or undefined"
        ));
        const workspaceUnderlayModeCopy = String(
            skipWorkspaceUnderlays.parent.description
        );
        verify(workspaceUnderlayModeCopy.includes("Off by default"));
        verify(workspaceUnderlayModeCopy.includes("active-workspace path"));
        const sdrWorkBufferCopyText = String(sdrWorkBufferCopy.text);
        verify(sdrWorkBufferCopyText.includes("SDR content only"));
        verify(sdrWorkBufferCopyText.includes("internal FP16 or ICC"));
        const sdrWorkBufferCautionCopy = String(
            sdrWorkBufferCautionMessage.text
        );
        verify(sdrWorkBufferCautionCopy.includes(
            "only the internal FP16 or ICC SDR work-buffer transfer"
        ));
        verify(sdrWorkBufferCautionCopy.includes(
            "does not enable FP16, HDR, or ICC"
        ));
        verify(sdrWorkBufferCautionCopy.includes(
            "does not change the output transfer function or color profile"
        ));
        verify(sdrWorkBufferCautionCopy.includes(
            "ordinary sRGB output without an ICC profile"
        ));
        verify(sdrWorkBufferCautionCopy.includes("may remain dormant"));
        verify(sdrWorkBufferCautionCopy.includes(
            "HDR-like paths stay linear regardless"
        ));
        const sdrWorkBufferModeCopy = String(
            fp16SdrTransfer.parent.description
        );
        verify(sdrWorkBufferModeCopy.includes(
            "Display transfer (the default)"
        ));
        verify(sdrWorkBufferModeCopy.includes("internal work buffer"));
        verify(sdrWorkBufferModeCopy.includes(
            "Linear keeps that SDR work-buffer content linear"
        ));
        const directScanoutCopyText = String(directScanoutCopy.text);
        verify(directScanoutCopyText.includes("eligible fullscreen"));
        verify(directScanoutCopyText.includes("directly to an output"));
        verify(directScanoutCopyText.includes(
            "instead of composing an ordinary frame"
        ));
        const directScanoutCautionCopy = String(
            directScanoutCautionMessage.text
        );
        verify(directScanoutCautionCopy.includes(
            "bypasses normal composition"
        ));
        verify(directScanoutCautionCopy.includes(
            "activation is never guaranteed"
        ));
        verify(directScanoutCautionCopy.includes("lock screen"));
        verify(directScanoutCautionCopy.includes("notifications"));
        verify(directScanoutCautionCopy.includes("screen capture"));
        verify(directScanoutCautionCopy.includes("mirrored outputs"));
        verify(directScanoutCautionCopy.includes("software cursor"));
        verify(directScanoutCautionCopy.includes(
            "incompatible buffers, transforms, or color management"
        ));
        verify(directScanoutCautionCopy.includes(
            "does not test display or GPU support"
        ));
        const directScanoutModeCopy = String(
            directScanout.parent.description
        );
        verify(directScanoutModeCopy.includes("Disabled (the default)"));
        verify(directScanoutModeCopy.includes("normal composition"));
        verify(directScanoutModeCopy.includes(
            "exact-fullscreen, solitary DMA surface"
        ));
        verify(directScanoutModeCopy.includes(
            "Automatic adds a requirement"
        ));
        verify(directScanoutModeCopy.includes("game content"));
        const nativeWaylandCopyText = String(nativeWaylandCopy.text);
        verify(nativeWaylandCopyText.includes("native Wayland client"));
        verify(nativeWaylandCopyText.includes("temporarily undersized"));
        verify(nativeWaylandCopyText.includes("submitted"));
        verify(nativeWaylandCopyText.includes("surface mapping"));
        const nativeWaylandCautionCopy = String(
            nativeWaylandCautionMessage.text
        );
        verify(nativeWaylandCautionCopy.includes(
            "native Wayland surfaces only"
        ));
        verify(nativeWaylandCautionCopy.includes(
            "does not resize the client buffer, window, or display"
        ));
        verify(nativeWaylandCautionCopy.includes(
            "does not make the application respond sooner"
        ));
        verify(nativeWaylandCautionCopy.includes(
            "X11 windows handled through XWayland are unaffected"
        ));
        const expandUndersizedTexturesCopy = String(
            expandUndersizedTextures.parent.description
        );
        verify(expandUndersizedTexturesCopy.includes(
            "submitted native Wayland buffer"
        ));
        verify(expandUndersizedTexturesCopy.includes(
            "temporarily undersized"
        ));
        verify(expandUndersizedTexturesCopy.includes("surface mapping"));
        verify(expandUndersizedTexturesCopy.includes(
            "may expose unfilled or stale-size edges"
        ));
        verify(expandUndersizedTexturesCopy.includes("scale-unaware"));
        verify(expandUndersizedTexturesCopy.includes(
            "misaligned-fullscreen correction paths bypass"
        ));
        verify(String(xwaylandCopy.text).includes("X11 application surfaces"));
        verify(String(xwaylandCopy.text).includes("XWayland"));
        const xwaylandCautionCopy = String(xwaylandCautionMessage.text);
        verify(xwaylandCautionCopy.includes("only X11 windows"));
        verify(xwaylandCautionCopy.includes("XWayland"));
        verify(xwaylandCautionCopy.includes(
            "Native Wayland windows are unchanged"
        ));
        verify(xwaylandCautionCopy.includes("per-window"));
        verify(xwaylandCautionCopy.includes(
            "Nearest-neighbor scaling Rule"
        ));
        const nearestNeighborCopy = String(
            nearestNeighbor.parent.description
        );
        verify(nearestNeighborCopy.includes("scales or transforms"));
        verify(nearestNeighborCopy.includes("X11 surface"));
        verify(nearestNeighborCopy.includes("crisp"));
        verify(nearestNeighborCopy.includes("pixelated"));
        verify(nearestNeighborCopy.includes("smoother filtering"));
        verify(nearestNeighborCopy.includes("blurry"));
        verify(nearestNeighborCopy.includes(
            "per-window Nearest-neighbor scaling Rule"
        ));
        verify(nearestNeighborCopy.includes(
            "does not change display scale"
        ));
        verify(nearestNeighborCopy.includes(
            "XWayland coordinate scaling"
        ));
        const inputCaptureCopyText = String(inputCaptureCopy.text);
        verify(inputCaptureCopyText.includes(
            "later keyboard-modifier events"
        ));
        verify(inputCaptureCopyText.includes("new barrier requests"));
        const inputCaptureCautionCopy = String(
            inputCaptureCautionMessage.text
        );
        verify(inputCaptureCautionCopy.includes(
            "do not grant input-capture permission"
        ));
        verify(inputCaptureCautionCopy.includes(
            "create or enable a capture session"
        ));
        verify(inputCaptureCautionCopy.includes(
            "release an active one"
        ));
        verify(inputCaptureCautionCopy.includes("later modifier events"));
        verify(inputCaptureCautionCopy.includes("new barrier requests"));
        verify(inputCaptureCautionCopy.includes(
            "does not synthesize or retract held modifier state"
        ));
        verify(inputCaptureCautionCopy.includes(
            "does not revalidate, repair, or remove existing barriers"
        ));
        const captureModifiersCopy = String(
            captureModifiers.parent.description
        );
        verify(captureModifiersCopy.includes("Off by default"));
        verify(captureModifiersCopy.includes("authorized capture is active"));
        verify(captureModifiersCopy.includes(
            "later keyboard modifier-state changes"
        ));
        verify(captureModifiersCopy.includes("ordinary seat"));
        verify(captureModifiersCopy.includes("input-method delivery"));
        verify(captureModifiersCopy.includes(
            "leaves ordinary modifier routing in place"
        ));
        const enforceBarriersCopy = String(
            enforceCaptureBarriers.parent.description
        );
        verify(enforceBarriersCopy.includes("On by default"));
        verify(enforceBarriersCopy.includes("For each new request"));
        verify(enforceBarriersCopy.includes(
            "valid full edge of exactly one output"
        ));
        verify(enforceBarriersCopy.includes("protocol error"));
        verify(enforceBarriersCopy.includes(
            "logs the invalid request and adds the barrier"
        ));
        verify(enforceBarriersCopy.includes(
            "Existing barriers are not rechecked or removed"
        ));

        allowRestore.checked = true;
        allowRestore.clicked();
        lockDelay.value = 2500;
        lockDelay.valueModified();
        scaleWarning.checked = true;
        scaleWarning.clicked();
        backgroundFps.value = 48;
        backgroundFps.valueModified();
        force8Bit.checked = false;
        force8Bit.clicked();
        disableLogo.checked = true;
        disableLogo.clicked();
        compare(page.draftValue(page.disableHyprlandLogoId), true);
        compare(page.draftValue(page.disableSplashRenderingId), false);
        compare(disableSplash.enabled, true);
        compare(disableSplash.checked, false);
        disableSplash.checked = true;
        disableSplash.clicked();
        compare(page.draftValue(page.disableHyprlandLogoId), true);
        compare(page.draftValue(page.disableSplashRenderingId), true);
        lockXray.checked = true;
        lockXray.clicked();
        compare(lockBlur.enabled, true);
        lockBlur.checked = true;
        lockBlur.clicked();
        lockXray.checked = false;
        lockXray.clicked();
        compare(lockBlur.enabled, false);
        compare(lockBlur.checked, true);
        compare(page.draftValue(page.sessionLockBlurId), true);
        lockXray.checked = true;
        lockXray.clicked();
        compare(lockBlur.enabled, true);
        compare(lockBlur.checked, true);
        expandUndersizedTextures.checked = false;
        expandUndersizedTextures.clicked();
        nearestNeighbor.checked = false;
        nearestNeighbor.clicked();
        directScanout.activated(2);
        fp16SdrTransfer.activated(1);
        skipWorkspaceUnderlays.checked = true;
        skipWorkspaceUnderlays.clicked();
        captureModifiers.checked = true;
        captureModifiers.clicked();
        enforceCaptureBarriers.checked = false;
        enforceCaptureBarriers.clicked();
        compare(captureModifiers.enabled, true);
        compare(enforceCaptureBarriers.enabled, true);
        compare(page.draftValue(page.allowSessionLockRestoreId), true);
        compare(page.draftValue(page.lockdeadScreenDelayId), 2500);
        compare(page.draftValue(page.disableScaleNotificationId), true);
        compare(page.draftValue(page.renderUnfocusedFpsId), 48);
        compare(page.draftValue(page.screencopyForce8BitId), false);
        compare(page.draftValue(page.disableHyprlandLogoId), true);
        compare(page.draftValue(page.disableSplashRenderingId), true);
        compare(page.draftValue(page.sessionLockXrayId), true);
        compare(page.draftValue(page.sessionLockBlurId), true);
        compare(page.draftValue(page.xwaylandUseNearestNeighborId), false);
        compare(page.draftValue(page.expandUndersizedTexturesId), false);
        compare(page.draftValue(page.directScanoutId), 2);
        compare(page.draftValue(page.fp16SdrTransferId), 1);
        compare(page.draftValue(page.xpModeId), true);
        compare(page.draftValue(page.captureModifiersId), true);
        compare(page.draftValue(page.enforceCaptureBarriersId), false);
        compare(page.draftDirty, true);
        compare(page.saveEnabled, true);

        let submitted = null;
        let requestCount = 0;
        page.saveRequested.connect(function(values) {
            ++requestCount;
            submitted = values;
        });
        findChild(page, "saveAdvancedButton").clicked();
        compare(requestCount, 1);
        verify(submitted !== null);
        compare(page.saveSubmitted, true);
        compare(page.submittedRevisionToken, "7");
        compare(Object.keys(submitted).length, 16);
        compare(submitted[page.allowSessionLockRestoreId], true);
        compare(submitted[page.lockdeadScreenDelayId], 2500);
        compare(submitted[page.disableScaleNotificationId], true);
        compare(submitted[page.renderUnfocusedFpsId], 48);
        compare(submitted[page.screencopyForce8BitId], false);
        compare(submitted[page.disableHyprlandLogoId], true);
        compare(submitted[page.disableSplashRenderingId], true);
        compare(submitted[page.sessionLockXrayId], true);
        compare(submitted[page.sessionLockBlurId], true);
        compare(submitted[page.xwaylandUseNearestNeighborId], false);
        compare(submitted[page.expandUndersizedTexturesId], false);
        compare(submitted[page.directScanoutId], 2);
        compare(submitted[page.fp16SdrTransferId], 1);
        compare(submitted[page.xpModeId], true);
        compare(submitted[page.captureModifiersId], true);
        compare(submitted[page.enforceCaptureBarriersId], false);

        const custom = advancedDefaults();
        custom[page.allowSessionLockRestoreId] = true;
        custom[page.lockdeadScreenDelayId] = 2400;
        custom[page.disableScaleNotificationId] = true;
        custom[page.renderUnfocusedFpsId] = 30;
        custom[page.screencopyForce8BitId] = false;
        custom[page.disableHyprlandLogoId] = true;
        custom[page.disableSplashRenderingId] = true;
        custom[page.sessionLockXrayId] = false;
        custom[page.sessionLockBlurId] = true;
        custom[page.xwaylandUseNearestNeighborId] = false;
        custom[page.expandUndersizedTexturesId] = false;
        custom[page.directScanoutId] = 1;
        custom[page.fp16SdrTransferId] = 1;
        custom[page.xpModeId] = true;
        custom[page.captureModifiersId] = true;
        custom[page.enforceCaptureBarriersId] = false;
        const secondWindow = createTemporaryObject(
            advancedPageComponent, this
        );
        verify(secondWindow !== null);
        const secondPage = secondWindow.page;
        configureAdvancedPage(secondPage, custom);
        secondPage.setDraftValue(secondPage.renderUnfocusedFpsId, 60);
        compare(secondPage.draftDirty, true);
        findChild(secondPage, "discardAdvancedDraftButton").clicked();
        compare(secondPage.draftDirty, false);
        compare(
            secondPage.draftValue(secondPage.renderUnfocusedFpsId), 30
        );
        compare(secondPage.draftValue(
            secondPage.disableHyprlandLogoId
        ), true);
        compare(secondPage.draftValue(
            secondPage.disableSplashRenderingId
        ), true);
        compare(secondPage.draftValue(secondPage.sessionLockXrayId), false);
        compare(secondPage.draftValue(secondPage.sessionLockBlurId), true);
        compare(secondPage.draftValue(
            secondPage.xwaylandUseNearestNeighborId
        ), false);
        compare(secondPage.draftValue(
            secondPage.expandUndersizedTexturesId
        ), false);
        compare(secondPage.draftValue(secondPage.directScanoutId), 1);
        compare(secondPage.draftValue(secondPage.fp16SdrTransferId), 1);
        compare(secondPage.draftValue(secondPage.xpModeId), true);
        compare(secondPage.draftValue(secondPage.captureModifiersId), true);
        compare(secondPage.draftValue(
            secondPage.enforceCaptureBarriersId
        ), false);
        compare(findChild(secondPage, "advancedSessionLockBlur").enabled,
            false);
        const reset = findChild(
            secondPage, "resetAdvancedDefaultsButton"
        );
        compare(reset.enabled, true);
        reset.clicked();
        compare(secondPage.draftDirty, true);
        const defaults = advancedDefaults();
        for (const id of secondPage.expectedOptionIds)
            compare(secondPage.draftValue(id), defaults[id], id);
        compare(secondPage.draftValue(
            secondPage.xwaylandUseNearestNeighborId
        ), true);
        compare(secondPage.draftValue(
            secondPage.expandUndersizedTexturesId
        ), true);
        compare(secondPage.draftValue(secondPage.directScanoutId), 0);
        compare(secondPage.draftValue(secondPage.fp16SdrTransferId), 0);
        compare(secondPage.draftValue(secondPage.xpModeId), false);
        compare(secondPage.draftValue(
            secondPage.captureModifiersId
        ), false);
        compare(secondPage.draftValue(
            secondPage.enforceCaptureBarriersId
        ), true);
        compare(findChild(secondPage, "advancedSessionLockBlur").enabled,
            false);
    }

    function test_advancedRejectsBadDefinitionsAndValuesAndPreservesConflict() {
        const testWindow = createTemporaryObject(
            advancedPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAdvancedPage(page);

        const badDefinitions = [];
        let definitions = advancedDefinitions();
        definitions.splice(2, 1);
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions.push({
            id: "hyprland.misc.unknown",
            type: "boolean",
            control: "toggle",
            defaultValue: false,
            risk: "safe"
        });
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        const reordered = definitions[1];
        definitions[1] = definitions[2];
        definitions[2] = reordered;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[0].type = "integer";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[0].defaultValue = true;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[1].min = 1;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[1].max = 4999;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[3].defaultValue = 16;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[4].choices = [true, false];
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[5].defaultValue = true;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[7].min = 0;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[8].type = "integer";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[0].risk = "caution";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[9].risk = "safe";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        delete definitions[9].risk;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[9].defaultValue = false;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[10].risk = "safe";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        delete definitions[10].risk;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[10].defaultValue = false;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[11].type = "integer";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[11].control = "spinBox";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[11].defaultValue = 1;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[11].min = 1;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[11].max = 3;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[11].choices[2].value = 3;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[11].risk = "safe";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[12].type = "integer";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[12].control = "spinBox";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[12].defaultValue = 1;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[12].min = 1;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[12].max = 2;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[12].choices[1].value = 2;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[12].choices.reverse();
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[12].risk = "safe";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        delete definitions[12].risk;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[13].type = "integer";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[13].control = "spinBox";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[13].defaultValue = true;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[13].min = 0;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[13].choices = [false, true];
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[13].risk = "safe";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        delete definitions[13].risk;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[14].type = "integer";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[14].control = "spinBox";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[14].defaultValue = true;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[14].min = 0;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[14].choices = [false, true];
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[14].risk = "safe";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        delete definitions[14].risk;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[15].type = "integer";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[15].control = "spinBox";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[15].defaultValue = false;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[15].max = 1;
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[15].choices = [false, true];
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        definitions[15].risk = "safe";
        badDefinitions.push(definitions);
        definitions = advancedDefinitions();
        delete definitions[15].risk;
        badDefinitions.push(definitions);
        for (const candidate of badDefinitions) {
            page.advancedOptions = candidate;
            compare(page.trustedDefinitionsValid, false);
            compare(page.controlsEnabled, false);
        }
        page.advancedOptions = advancedDefinitions();
        compare(page.trustedDefinitionsValid, true);

        const badValues = [];
        let values = advancedDefaults();
        delete values[page.screencopyForce8BitId];
        badValues.push(values);
        values = advancedDefaults();
        delete values[page.xpModeId];
        badValues.push(values);
        values = advancedDefaults();
        delete values[page.captureModifiersId];
        badValues.push(values);
        values = advancedDefaults();
        delete values[page.enforceCaptureBarriersId];
        badValues.push(values);
        values = advancedDefaults();
        values["hyprland.misc.unknown"] = false;
        badValues.push(values);
        values = advancedDefaults();
        values[page.allowSessionLockRestoreId] = 0;
        badValues.push(values);
        values = advancedDefaults();
        values[page.lockdeadScreenDelayId] = -1;
        badValues.push(values);
        values = advancedDefaults();
        values[page.lockdeadScreenDelayId] = 5001;
        badValues.push(values);
        values = advancedDefaults();
        values[page.lockdeadScreenDelayId] = 0.5;
        badValues.push(values);
        values = advancedDefaults();
        values[page.renderUnfocusedFpsId] = 0;
        badValues.push(values);
        values = advancedDefaults();
        values[page.renderUnfocusedFpsId] = 121;
        badValues.push(values);
        values = advancedDefaults();
        values[page.renderUnfocusedFpsId] = NaN;
        badValues.push(values);
        values = advancedDefaults();
        values[page.renderUnfocusedFpsId] = Infinity;
        badValues.push(values);
        for (const value of [-1, 3, 0.5, "0"]) {
            values = advancedDefaults();
            values[page.directScanoutId] = value;
            badValues.push(values);
        }
        for (const value of [-1, 2, 0.5, "0"]) {
            values = advancedDefaults();
            values[page.fp16SdrTransferId] = value;
            badValues.push(values);
        }
        for (const id of [
                 page.allowSessionLockRestoreId,
                 page.disableScaleNotificationId,
                 page.screencopyForce8BitId,
                 page.disableHyprlandLogoId,
                 page.disableSplashRenderingId,
                 page.sessionLockXrayId,
                 page.sessionLockBlurId,
                 page.xwaylandUseNearestNeighborId,
                 page.expandUndersizedTexturesId,
                 page.xpModeId,
                 page.captureModifiersId,
                 page.enforceCaptureBarriersId
             ]) {
            values = advancedDefaults();
            values[id] = 0;
            badValues.push(values);
            values = advancedDefaults();
            values[id] = "false";
            badValues.push(values);
        }
        for (const candidate of badValues)
            compare(page.validateValues(candidate), false);

        page.setDraftValue(page.allowSessionLockRestoreId, true);
        page.setDraftValue(page.lockdeadScreenDelayId, 2300);
        page.setDraftValue(page.sessionLockXrayId, true);
        page.setDraftValue(page.sessionLockBlurId, true);
        page.setDraftValue(page.xwaylandUseNearestNeighborId, false);
        page.setDraftValue(page.expandUndersizedTexturesId, false);
        page.setDraftValue(page.directScanoutId, 2);
        page.setDraftValue(page.fp16SdrTransferId, 1);
        page.setDraftValue(page.xpModeId, true);
        page.setDraftValue(page.captureModifiersId, true);
        page.setDraftValue(page.enforceCaptureBarriersId, false);
        page.setDraftValue(page.sessionLockXrayId, false);
        compare(page.draftDirty, true);
        page.advancedProjectionAvailable = false;
        page.advancedAvailable = false;
        page.advancedValues = ({ broken: true });
        page.revisionToken = "8";
        wait(0);
        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.allowSessionLockRestoreId), true);
        compare(page.draftValue(page.lockdeadScreenDelayId), 2300);
        compare(page.draftValue(page.sessionLockXrayId), false);
        compare(page.draftValue(page.sessionLockBlurId), true);
        compare(page.draftValue(page.xwaylandUseNearestNeighborId), false);
        compare(page.draftValue(page.expandUndersizedTexturesId), false);
        compare(page.draftValue(page.directScanoutId), 2);
        compare(page.draftValue(page.fp16SdrTransferId), 1);
        compare(page.draftValue(page.xpModeId), true);
        compare(page.draftValue(page.captureModifiersId), true);
        compare(page.draftValue(page.enforceCaptureBarriersId), false);
        const load = findChild(page, "loadCurrentAdvancedButton");
        compare(load.visible, true);
        compare(load.enabled, false);
        verify(String(findChild(page, "advancedStatusMessage").text)
            .includes("trusted Advanced contract"));

        const newer = advancedDefaults();
        newer[page.renderUnfocusedFpsId] = 60;
        newer[page.disableHyprlandLogoId] = true;
        newer[page.sessionLockXrayId] = true;
        newer[page.directScanoutId] = 1;
        newer[page.captureModifiersId] = true;
        newer[page.enforceCaptureBarriersId] = false;
        page.advancedValues = newer;
        page.advancedProjectionAvailable = true;
        wait(0);
        compare(page.advancedAvailable, false);
        compare(page.externalChangeWhileEditing, true);
        compare(load.enabled, true);
        load.clicked();
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.allowSessionLockRestoreId), false);
        compare(page.draftValue(page.lockdeadScreenDelayId), 1000);
        compare(page.draftValue(page.renderUnfocusedFpsId), 60);
        compare(page.draftValue(page.disableHyprlandLogoId), true);
        compare(page.draftValue(page.sessionLockXrayId), true);
        compare(page.draftValue(page.sessionLockBlurId), false);
        compare(page.draftValue(page.xwaylandUseNearestNeighborId), true);
        compare(page.draftValue(page.expandUndersizedTexturesId), true);
        compare(page.draftValue(page.directScanoutId), 1);
        compare(page.draftValue(page.fp16SdrTransferId), 0);
        compare(page.draftValue(page.xpModeId), false);
        compare(page.draftValue(page.captureModifiersId), true);
        compare(page.draftValue(page.enforceCaptureBarriersId), false);
        compare(page.synchronizedRevisionToken, "8");
    }

    function test_advancedOwnRetainedApplyRetryAndRecovery() {
        const testWindow = createTemporaryObject(
            advancedPageComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAdvancedPage(page);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.allowSessionLockRestoreId, true);
        page.setDraftValue(page.lockdeadScreenDelayId, 2200);
        page.setDraftValue(page.renderUnfocusedFpsId, 45);
        page.setDraftValue(page.disableHyprlandLogoId, true);
        page.setDraftValue(page.sessionLockXrayId, true);
        page.setDraftValue(page.sessionLockBlurId, true);
        page.setDraftValue(page.xwaylandUseNearestNeighborId, false);
        page.setDraftValue(page.expandUndersizedTexturesId, false);
        page.setDraftValue(page.directScanoutId, 2);
        page.setDraftValue(page.fp16SdrTransferId, 1);
        page.setDraftValue(page.xpModeId, true);
        page.setDraftValue(page.captureModifiersId, true);
        page.setDraftValue(page.enforceCaptureBarriersId, false);
        const submitted = page.clone(page.draftValues);
        let requestCount = 0;
        page.saveRequested.connect(function() { ++requestCount; });
        page.submitDraft();
        compare(requestCount, 1);
        compare(page.saveSubmitted, true);

        page.busyOperation = "advanced-save";
        page.busy = true;
        page.serviceAvailable = false;
        page.advancedProjectionAvailable = false;
        page.advancedAvailable = false;
        page.advancedValues = ({});
        page.revisionToken = "8";
        page.busy = false;
        page.busyOperation = "";
        wait(0);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, true);

        page.serviceAvailable = true;
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.recoveryAvailable = true;
        page.advancedValues = submitted;
        page.advancedProjectionAvailable = true;
        wait(0);
        compare(page.advancedAvailable, false);
        compare(page.externalChangeWhileEditing, false);
        compare(page.saveSubmitted, false);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.lockdeadScreenDelayId), 2200);
        compare(page.draftValue(page.renderUnfocusedFpsId), 45);
        compare(page.draftValue(page.disableHyprlandLogoId), true);
        compare(page.draftValue(page.sessionLockXrayId), true);
        compare(page.draftValue(page.sessionLockBlurId), true);
        compare(page.draftValue(page.xwaylandUseNearestNeighborId), false);
        compare(page.draftValue(page.expandUndersizedTexturesId), false);
        compare(page.draftValue(page.directScanoutId), 2);
        compare(page.draftValue(page.fp16SdrTransferId), 1);
        compare(page.draftValue(page.xpModeId), true);
        compare(page.draftValue(page.captureModifiersId), true);
        compare(page.draftValue(page.enforceCaptureBarriersId), false);
        compare(page.synchronizedRevisionToken, "8");

        let retryCount = 0;
        let recoveryCount = 0;
        page.retryApplyRequested.connect(function() { ++retryCount; });
        page.recoveryRequested.connect(function() { ++recoveryCount; });
        const retry = findChild(page, "retryApplyAdvancedButton");
        const recover = findChild(page, "recoverAdvancedButton");
        const dialog = findChild(page, "advancedRecoveryDialog");
        const warning = findChild(page, "advancedRecoveryWarning");
        const cancel = findChild(page, "cancelAdvancedRecoveryButton");
        const confirm = findChild(page, "confirmAdvancedRecoveryButton");
        compare(retry.visible, true);
        compare(retry.enabled, true);
        retry.clicked();
        compare(retryCount, 1);
        recover.clicked();
        tryCompare(dialog, "opened", true);
        tryCompare(cancel, "activeFocus", true);
        verify(String(warning.text).includes("not limited to Advanced"));
        verify(String(warning.text).includes("every pending compositor"));
        cancel.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 0);
        recover.clicked();
        tryCompare(dialog, "opened", true);
        confirm.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 1);
        confirm.clicked();
        compare(recoveryCount, 1);

        page.confirmationState = "awaiting-confirmation";
        compare(page.controlsEnabled, false);
        compare(findChild(page, "advancedCaptureModifiers").enabled, false);
        compare(
            findChild(page, "advancedEnforceCaptureBarriers").enabled,
            false
        );
        page.confirmationState = "idle";
        page.sharedMutationBusy = true;
        compare(page.controlsEnabled, false);
        page.sharedMutationBusy = false;
        page.sharedApplySafe = false;
        compare(page.controlsEnabled, false);
        page.sharedApplySafe = true;
        page.writable = false;
        compare(page.controlsEnabled, false);
        page.writable = true;
        page.managementState = "unmanaged";
        compare(page.controlsEnabled, false);
        compare(findChild(page, "advancedOpenDisplaysButton").visible, true);
        page.managementState = "managed";
        page.revisionToken = "";
        compare(page.controlsEnabled, false);
    }

    function test_advancedActionsReachableAtMinimumWindow() {
        const testWindow = createTemporaryObject(
            advancedPageComponent, this,
            { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAdvancedPage(page);
        page.setDraftValue(page.allowSessionLockRestoreId, true);
        waitForRendering(page);
        wait(0);

        const scroll = findChild(page, "advancedOptionsScrollView");
        const content = findChild(page, "advancedOptionsContent");
        const save = findChild(page, "saveAdvancedButton");
        verify(scroll !== null);
        verify(content !== null);
        verify(save !== null);
        compare(page.compactPage, true);
        verify(scroll.contentItem.contentHeight > scroll.height);
        verify(scroll.height >= 100);
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width <= scroll.contentWidth + 0.01);
        compare(findChild(page, "advancedNestedScrollView"), null);
        compare(findChild(page, "advancedPreview"), null);

        const maximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        verify(maximumContentY > 0);
        const scrollPosition = scroll.mapToItem(page, 0, 0);
        for (const name of [
                 "advancedAllowSessionLockRestore",
                 "advancedLockdeadScreenDelay",
                 "advancedSessionLockXray",
                 "advancedSessionLockBlur",
                 "advancedDisableHyprlandLogo",
                 "advancedDisableSplashRendering",
                 "advancedRenderUnfocusedFps",
                 "advancedScreencopyForce8Bit",
                 "advancedSkipWorkspaceUnderlays",
                 "advancedFp16SdrTransfer",
                 "advancedDirectScanout",
                 "advancedExpandUndersizedTextures",
                 "advancedXWaylandUseNearestNeighbor",
                 "advancedCaptureModifiers",
                 "advancedEnforceCaptureBarriers",
                 "advancedDisableScaleNotification"
             ]) {
            const control = findChild(page, name);
            verify(control !== null, "Missing compact control " + name);
            const contentPosition = control.mapToItem(content, 0, 0);
            verify(contentPosition.x >= 0, name + " starts outside content");
            verify(
                contentPosition.x + control.width <= content.width + 0.01,
                name + " overflows content horizontally"
            );
            const targetY = Math.max(0, Math.min(
                maximumContentY,
                contentPosition.y + control.height / 2 - scroll.height / 2
            ));
            scroll.contentItem.contentY = targetY;
            tryCompare(scroll.contentItem, "contentY", targetY);
            const pagePosition = control.mapToItem(page, 0, 0);
            verify(pagePosition.x >= 0, name + " starts outside page");
            verify(
                pagePosition.x + control.width <= page.width + 0.01,
                name + " overflows page horizontally"
            );
            verify(pagePosition.y + control.height > scrollPosition.y);
            verify(pagePosition.y < scrollPosition.y + scroll.height);
        }
        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        const savePosition = save.mapToItem(page, 0, 0);
        verify(savePosition.x >= 0);
        verify(savePosition.x + save.width <= page.width + 0.01);
        verify(savePosition.y >= 0);
        verify(savePosition.y + save.height <= page.height + 0.01);
        verify(save.implicitHeight >= page.minimumTargetSize);
        compare(save.enabled, true);
    }

    function test_advancedSaveLeavesOtherScalarDraftsUntouched() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const appearance = findChild(application, "appearancePage");
        const input = findChild(application, "inputPage");
        const windows = findChild(application, "windowsLayoutPage");
        const workspaces = findChild(application, "workspacesPage");
        const advanced = findChild(application, "advancedPage");
        for (const page of [
                 appearance, input, windows, workspaces, advanced
             ])
            verify(page !== null);
        configureAppearancePage(appearance);
        configureInputPage(input);
        configureWindowsPage(windows);
        configureWorkspacesPage(workspaces);
        configureAdvancedPage(advanced);

        appearance.setDraftValue(appearance.activeOpacityId, 0.9);
        appearance.setExactDecimalDraftValue(
            appearance.blurNoiseId, 0.1234567
        );
        input.setDraftValue(input.repeatRateId, 30);
        windows.setDraftValue(windows.layoutId, "master");
        workspaces.setDraftValue(workspaces.workspaceWraparoundId, true);
        const appearanceDraft = appearance.clone(appearance.draftValues);
        const inputDraft = input.clone(input.draftValues);
        const windowsDraft = windows.clone(windows.draftValues);
        const workspacesDraft = workspaces.clone(workspaces.draftValues);
        for (const page of [appearance, input, windows, workspaces])
            compare(page.draftDirty, true);
        compare(
            appearance.draftValue(appearance.blurNoiseId), 0.1234567
        );

        advanced.setDraftValue(advanced.disableHyprlandLogoId, true);
        advanced.setDraftValue(advanced.sessionLockXrayId, true);
        advanced.setDraftValue(advanced.sessionLockBlurId, true);
        advanced.setDraftValue(
            advanced.xwaylandUseNearestNeighborId, false
        );
        advanced.setDraftValue(advanced.fp16SdrTransferId, 1);
        advanced.setDraftValue(advanced.xpModeId, true);
        advanced.setDraftValue(advanced.captureModifiersId, true);
        advanced.setDraftValue(
            advanced.enforceCaptureBarriersId, false
        );
        advanced.setDraftValue(
            advanced.expandUndersizedTexturesId, false
        );
        advanced.setDraftValue(advanced.directScanoutId, 2);
        let submitted = null;
        advanced.saveRequested.connect(function(values) {
            submitted = values;
        });
        advanced.submitDraft();
        verify(submitted !== null);
        compare(Object.keys(submitted), advanced.expectedOptionIds);
        compare(submitted[advanced.disableHyprlandLogoId], true);
        compare(submitted[advanced.sessionLockXrayId], true);
        compare(submitted[advanced.sessionLockBlurId], true);
        compare(
            submitted[advanced.xwaylandUseNearestNeighborId], false
        );
        compare(
            submitted[advanced.expandUndersizedTexturesId], false
        );
        compare(submitted[advanced.directScanoutId], 2);
        compare(submitted[advanced.fp16SdrTransferId], 1);
        compare(submitted[advanced.xpModeId], true);
        compare(submitted[advanced.captureModifiersId], true);
        compare(submitted[advanced.enforceCaptureBarriersId], false);
        compare(appearance.draftValues, appearanceDraft);
        compare(input.draftValues, inputDraft);
        compare(windows.draftValues, windowsDraft);
        compare(workspaces.draftValues, workspacesDraft);
        for (const page of [appearance, input, windows, workspaces])
            compare(page.draftDirty, true);
    }

    function test_scalarAuthorityFailureKeepsReadableProjectionAndTruthfulStatus() {
        const appearanceWindow = createTemporaryObject(
            appearancePageComponent, this
        );
        const inputWindow = createTemporaryObject(inputPageComponent, this);
        const windowsWindow = createTemporaryObject(
            windowsLayoutPageComponent, this
        );
        const workspacesWindow = createTemporaryObject(
            workspacesPageComponent, this
        );
        const advancedWindow = createTemporaryObject(
            advancedPageComponent, this
        );
        verify(appearanceWindow !== null);
        verify(inputWindow !== null);
        verify(windowsWindow !== null);
        verify(workspacesWindow !== null);
        verify(advancedWindow !== null);
        const appearance = appearanceWindow.page;
        const input = inputWindow.page;
        const windows = windowsWindow.page;
        const workspaces = workspacesWindow.page;
        const advanced = advancedWindow.page;
        configureAppearancePage(appearance);
        configureInputPage(input);
        configureWindowsPage(windows);
        configureWorkspacesPage(workspaces);
        configureAdvancedPage(advanced);

        const detail = "Action, schema, and full-state verification failed.";
        appearance.appearanceAvailable = false;
        appearance.appearanceErrorName = "org.hyprshelld.Client.Compositor.Error.Authority";
        appearance.appearanceErrorMessage = detail;
        input.inputAvailable = false;
        input.inputErrorName = "org.hyprshelld.Client.Compositor.Error.Authority";
        input.inputErrorMessage = detail;
        windows.windowsAvailable = false;
        windows.windowsErrorName = "org.hyprshelld.Client.Compositor.Error.Authority";
        windows.windowsErrorMessage = detail;
        workspaces.workspacesAvailable = false;
        workspaces.workspacesErrorName = "org.hyprshelld.Client.Compositor.Error.Authority";
        workspaces.workspacesErrorMessage = detail;
        advanced.advancedAvailable = false;
        advanced.advancedErrorName = "org.hyprshelld.Client.Compositor.Error.Authority";
        advanced.advancedErrorMessage = detail;
        wait(0);

        const cases = [
            {
                page: appearance,
                projectionAvailable: appearance.appearanceProjectionAvailable,
                prefix: "Compositor appearance authority verification failed"
            },
            {
                page: input,
                projectionAvailable: input.inputProjectionAvailable,
                prefix: "Input authority verification failed"
            },
            {
                page: windows,
                projectionAvailable: windows.windowsProjectionAvailable,
                prefix: "Windows & Layout authority verification failed"
            },
            {
                page: workspaces,
                projectionAvailable: workspaces.workspacesProjectionAvailable,
                prefix: "Workspaces authority verification failed"
            },
            {
                page: advanced,
                projectionAvailable: advanced.advancedProjectionAvailable,
                prefix: "Advanced authority verification failed"
            }
        ];
        for (const testCase of cases) {
            compare(testCase.projectionAvailable, true);
            compare(testCase.page.trustedValuesValid, true);
            compare(testCase.page.controlsEnabled, false);
            compare(testCase.page.projectionInitialized, true);
            const message = String(testCase.page.statusMessage);
            verify(message.includes(testCase.prefix), message);
            verify(message.includes("remain readable"), message);
            verify(message.includes(detail), message);
            verify(!message.includes("operation failed"), message);
            verify(!message.includes("service may be restarting"), message);
        }
    }

    function test_mainScalarNavigationAndCrossPageDraftBadges() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const appearance = findChild(application, "appearancePage");
        const input = findChild(application, "inputPage");
        const displays = findChild(application, "displaysPage");
        const windows = findChild(application, "windowsLayoutPage");
        const workspaces = findChild(application, "workspacesPage");
        const advanced = findChild(application, "advancedPage");
        const keyboardShortcuts = findChild(
            application, "keyboardShortcutsPage"
        );
        const barNavigation = findChild(application, "barNavigationItem");
        const hyprlandNavigation = findChild(
            application, "hyprlandNavigationItem"
        );
        const appearanceNavigation = findChild(
            application, "appearanceNavigationItem"
        );
        const inputNavigation = findChild(
            application, "inputNavigationItem"
        );
        const displaysNavigation = findChild(
            application, "displaysNavigationItem"
        );
        const windowsNavigation = findChild(
            application, "windowsNavigationItem"
        );
        const workspacesNavigation = findChild(
            application, "workspacesNavigationItem"
        );
        const keyboardShortcutsNavigation = findChild(
            application, "keyboardShortcutsNavigationItem"
        );
        const rulesNavigation = findChild(
            application, "rulesNavigationItem"
        );
        const advancedNavigation = findChild(
            application, "advancedNavigationItem"
        );
        const componentsNavigation = findChild(
            application, "componentsNavigationItem"
        );
        const appearanceBadge = findChild(
            application, "appearanceNavigationBadge"
        );
        const inputBadge = findChild(
            application, "inputNavigationBadge"
        );
        const windowsBadge = findChild(
            application, "windowsNavigationBadge"
        );
        const workspacesBadge = findChild(
            application, "workspacesNavigationBadge"
        );
        const advancedBadge = findChild(
            application, "advancedNavigationBadge"
        );
        verify(appearance !== null);
        verify(input !== null);
        verify(displays !== null);
        verify(windows !== null);
        verify(workspaces !== null);
        verify(advanced !== null);
        verify(keyboardShortcuts !== null);
        verify(barNavigation !== null);
        verify(hyprlandNavigation !== null);
        verify(appearanceNavigation !== null);
        verify(inputNavigation !== null);
        verify(displaysNavigation !== null);
        verify(windowsNavigation !== null);
        compare(findChild(application, "groupsNavigationItem"), null);
        verify(workspacesNavigation !== null);
        verify(keyboardShortcutsNavigation !== null);
        verify(rulesNavigation !== null);
        verify(advancedNavigation !== null);
        verify(componentsNavigation !== null);
        verify(appearanceBadge !== null);
        verify(inputBadge !== null);
        verify(windowsBadge !== null);
        verify(workspacesBadge !== null);
        verify(advancedBadge !== null);
        configureAppearancePage(appearance);
        configureInputPage(input);
        configureWindowsPage(windows);
        configureWorkspacesPage(workspaces);
        configureAdvancedPage(advanced);
        wait(0);

        const navigation = [
            barNavigation,
            hyprlandNavigation,
            appearanceNavigation,
            inputNavigation,
            displaysNavigation,
            windowsNavigation,
            workspacesNavigation,
            keyboardShortcutsNavigation,
            rulesNavigation,
            advancedNavigation,
            componentsNavigation
        ];
        for (let index = 1; index < navigation.length; ++index) {
            compare(navigation[index].parent,
                    navigation[index - 1].parent);
            verify(navigation[index].y > navigation[index - 1].y);
        }
        compare(inputNavigation.Accessible.name, "Input settings");
        compare(
            windowsNavigation.Accessible.name,
            "Windows and layout settings"
        );
        compare(
            workspacesNavigation.Accessible.name,
            "Workspace settings"
        );
        compare(
            keyboardShortcutsNavigation.Accessible.name,
            "Shortcut and submap settings"
        );
        compare(advancedNavigation.Accessible.name, "Advanced settings");
        keyboardShortcutsNavigation.clicked();
        compare(application.currentPage, "hyprland");
        compare(application.hyprlandSection, "bindings");
        compare(keyboardShortcutsNavigation.checked, true);
        compare(keyboardShortcuts.visible, false);
        compare(findChild(application, "bindingsPage").visible, true);
        verify(String(keyboardShortcutsNavigation.Accessible.description)
            .includes("active reviewed Hyprland bindings"));
        inputNavigation.clicked();
        compare(application.currentPage, "input");
        compare(inputNavigation.checked, true);
        compare(input.visible, true);

        appearance.setDraftValue(appearance.blurId, false);
        compare(appearance.draftDirty, true);
        compare(application.appearanceDraftNavigationState, "dirty");
        compare(appearanceBadge.visible, true);
        compare(appearanceBadge.text, "Unsaved");
        verify(String(appearanceNavigation.Accessible.description)
            .includes("unsaved changes"));

        const newerAppearance = appearanceDefaults();
        newerAppearance[appearance.borderSizeId] = 2;
        const newerInput = inputDefaults();
        newerInput[input.repeatRateId] = 40;
        newerInput[input.touchDeviceEnabledId] = false;
        appearance.appearanceProjectionAvailable = false;
        appearance.appearanceAvailable = false;
        appearance.appearanceValues = ({});
        input.inputValues = newerInput;
        appearance.revisionToken = "8";
        input.revisionToken = "8";
        wait(0);
        compare(appearance.externalChangeWhileEditing, true);
        compare(appearance.draftValue(appearance.blurId), false);
        compare(application.appearanceDraftNavigationState, "conflict");
        compare(appearanceBadge.text, "Review");
        verify(String(appearanceNavigation.Accessible.description)
            .includes("conflicts with a newer compositor revision"));
        appearanceNavigation.clicked();
        compare(application.currentPage, "appearance");
        const loadAppearance = findChild(
            appearance, "loadCurrentAppearanceButton"
        );
        verify(loadAppearance !== null);
        compare(loadAppearance.visible, true);
        compare(loadAppearance.enabled, false);
        compare(input.externalChangeWhileEditing, false);
        compare(input.draftValue(input.repeatRateId), 40);
        compare(input.draftValue(input.touchDeviceEnabledId), false);

        appearance.appearanceProjectionAvailable = true;
        appearance.appearanceValues = newerAppearance;
        wait(0);
        compare(appearance.appearanceAvailable, false);
        compare(appearance.controlsEnabled, false);
        compare(appearance.externalChangeWhileEditing, true);
        compare(appearance.draftValue(appearance.blurId), false);
        compare(loadAppearance.enabled, true);
        loadAppearance.clicked();
        compare(application.appearanceDraftNavigationState, "clean");
        compare(appearanceBadge.visible, false);
        appearance.appearanceAvailable = true;

        input.setDraftValue(input.tabletTransformId, 6);
        input.setDraftValue(input.cursorInactiveTimeoutId, 2.37);
        input.setDraftValue(input.cursorNoWarpsId, true);
        input.setDraftValue(input.cursorPersistentWarpsId, true);
        compare(input.draftDirty, true);
        appearanceNavigation.clicked();
        compare(application.currentPage, "appearance");
        compare(input.draftValue(input.cursorInactiveTimeoutId), 2.37);
        compare(input.draftValue(input.cursorNoWarpsId), true);
        compare(input.draftValue(input.cursorPersistentWarpsId), true);
        compare(application.inputDraftNavigationState, "dirty");
        compare(inputBadge.visible, true);
        compare(inputBadge.text, "Unsaved");

        appearance.appearanceValues = appearanceDefaults();
        input.inputProjectionAvailable = false;
        input.inputAvailable = false;
        input.inputValues = inputDefaults();
        appearance.revisionToken = "9";
        input.revisionToken = "9";
        wait(0);
        compare(input.externalChangeWhileEditing, true);
        compare(input.draftValue(input.tabletTransformId), 6);
        compare(input.draftValue(input.cursorInactiveTimeoutId), 2.37);
        compare(input.draftValue(input.cursorNoWarpsId), true);
        compare(input.draftValue(input.cursorPersistentWarpsId), true);
        compare(application.inputDraftNavigationState, "conflict");
        compare(inputBadge.text, "Review");
        verify(String(inputNavigation.Accessible.description)
            .includes("conflicts with a newer compositor revision"));
        inputNavigation.clicked();
        compare(application.currentPage, "input");
        const loadInput = findChild(input, "loadCurrentInputButton");
        verify(loadInput !== null);
        compare(loadInput.visible, true);
        compare(loadInput.enabled, false);

        input.inputProjectionAvailable = true;
        input.inputValues = inputDefaults();
        wait(0);
        compare(input.inputAvailable, false);
        compare(input.controlsEnabled, false);
        compare(input.externalChangeWhileEditing, true);
        compare(input.draftValue(input.tabletTransformId), 6);
        compare(input.draftValue(input.cursorInactiveTimeoutId), 2.37);
        compare(input.draftValue(input.cursorNoWarpsId), true);
        compare(input.draftValue(input.cursorPersistentWarpsId), true);
        compare(loadInput.enabled, true);
        loadInput.clicked();
        compare(application.inputDraftNavigationState, "clean");
        compare(inputBadge.visible, false);
        compare(input.draftDirty, false);
        compare(input.draftValue(input.tabletTransformId), 0);
        compare(input.draftValue(input.cursorInactiveTimeoutId), 0);
        compare(input.draftValue(input.cursorNoWarpsId), false);
        compare(input.draftValue(input.cursorPersistentWarpsId), false);

        windows.setDraftValue(windows.layoutId, "scrolling");
        windows.setDraftValue(windows.snapWindowGapId, 29);
        compare(windows.draftDirty, true);
        appearanceNavigation.clicked();
        compare(application.currentPage, "appearance");
        compare(application.windowsDraftNavigationState, "dirty");
        compare(windowsBadge.visible, true);
        compare(windowsBadge.text, "Unsaved");
        verify(String(windowsNavigation.Accessible.description)
            .includes("unsaved changes"));

        const newerWindows = windowsDefaults();
        newerWindows[windows.layoutId] = "master";
        newerWindows[windows.snapMonitorGapId] = 37;
        windows.windowsProjectionAvailable = false;
        windows.windowsAvailable = false;
        windows.windowsValues = ({ broken: true });
        windows.revisionToken = "10";
        wait(0);
        compare(windows.externalChangeWhileEditing, true);
        compare(windows.draftValue(windows.layoutId), "scrolling");
        compare(windows.draftValue(windows.snapWindowGapId), 29);
        compare(application.windowsDraftNavigationState, "conflict");
        compare(windowsBadge.text, "Review");
        verify(String(windowsNavigation.Accessible.description)
            .includes("conflicts with a newer compositor revision"));

        windowsNavigation.clicked();
        compare(application.currentPage, "windows");
        compare(windowsNavigation.checked, true);
        compare(windows.visible, true);
        const loadWindows = findChild(
            windows, "loadCurrentWindowsButton"
        );
        verify(loadWindows !== null);
        compare(loadWindows.visible, true);
        compare(loadWindows.enabled, false);

        windows.windowsValues = newerWindows;
        windows.windowsProjectionAvailable = true;
        wait(0);
        compare(windows.windowsAvailable, false);
        compare(windows.controlsEnabled, false);
        compare(windows.externalChangeWhileEditing, true);
        compare(windows.draftValue(windows.layoutId), "scrolling");
        compare(loadWindows.enabled, true);
        loadWindows.clicked();
        compare(application.windowsDraftNavigationState, "clean");
        compare(windowsBadge.visible, false);
        compare(windows.draftDirty, false);
        compare(windows.draftValue(windows.layoutId), "master");
        compare(windows.draftValue(windows.snapMonitorGapId), 37);

        workspaces.workspacesAvailable = true;
        workspaces.setDraftValue(workspaces.workspaceWraparoundId, true);
        workspaces.setDraftValue(workspaces.swipeDistanceId, 451);
        compare(application.workspacesDraftNavigationState, "dirty");
        compare(workspacesBadge.visible, true);
        compare(workspacesBadge.text, "Unsaved");
        verify(String(workspacesNavigation.Accessible.description)
            .includes("unsaved changes"));

        const newerWorkspaces = workspacesDefaults();
        newerWorkspaces[workspaces.swipeDistanceId] = 600;
        workspaces.workspacesProjectionAvailable = false;
        workspaces.workspacesAvailable = false;
        workspaces.workspacesValues = ({ broken: true });
        workspaces.revisionToken = "11";
        wait(0);
        compare(workspaces.externalChangeWhileEditing, true);
        compare(workspaces.draftValue(workspaces.workspaceWraparoundId), true);
        compare(workspaces.draftValue(workspaces.swipeDistanceId), 451);
        compare(application.workspacesDraftNavigationState, "conflict");
        compare(workspacesBadge.text, "Review");
        workspacesNavigation.clicked();
        compare(application.currentPage, "workspaces");
        compare(workspacesNavigation.checked, true);
        compare(workspaces.visible, true);
        const loadWorkspaces = findChild(
            workspaces, "loadCurrentWorkspacesButton"
        );
        compare(loadWorkspaces.visible, true);
        compare(loadWorkspaces.enabled, false);
        workspaces.workspacesValues = newerWorkspaces;
        workspaces.workspacesProjectionAvailable = true;
        wait(0);
        compare(workspaces.workspacesAvailable, false);
        compare(loadWorkspaces.enabled, true);
        loadWorkspaces.clicked();
        compare(application.workspacesDraftNavigationState, "clean");
        compare(workspacesBadge.visible, false);
        compare(workspaces.draftValue(workspaces.swipeDistanceId), 600);

        advanced.setDraftValue(
            advanced.allowSessionLockRestoreId, true
        );
        advanced.setDraftValue(advanced.renderUnfocusedFpsId, 48);
        advanced.setDraftValue(advanced.sessionLockXrayId, true);
        advanced.setDraftValue(advanced.sessionLockBlurId, true);
        advanced.setDraftValue(
            advanced.xwaylandUseNearestNeighborId, false
        );
        advanced.setDraftValue(advanced.fp16SdrTransferId, 1);
        advanced.setDraftValue(advanced.xpModeId, true);
        advanced.setDraftValue(advanced.captureModifiersId, true);
        advanced.setDraftValue(
            advanced.enforceCaptureBarriersId, false
        );
        compare(application.advancedDraftNavigationState, "dirty");
        compare(advancedBadge.visible, true);
        compare(advancedBadge.text, "Unsaved");
        verify(String(advancedNavigation.Accessible.description)
            .includes("unsaved changes"));
        rulesNavigation.clicked();
        compare(application.currentPage, "rules");
        compare(advanced.draftValue(
            advanced.allowSessionLockRestoreId
        ), true);
        compare(advanced.draftValue(advanced.renderUnfocusedFpsId), 48);
        compare(advanced.draftValue(advanced.sessionLockXrayId), true);
        compare(advanced.draftValue(advanced.sessionLockBlurId), true);
        compare(advanced.draftValue(
            advanced.xwaylandUseNearestNeighborId
        ), false);
        compare(advanced.draftValue(advanced.fp16SdrTransferId), 1);
        compare(advanced.draftValue(advanced.xpModeId), true);
        compare(advanced.draftValue(advanced.captureModifiersId), true);
        compare(advanced.draftValue(
            advanced.enforceCaptureBarriersId
        ), false);

        const newerAdvanced = advancedDefaults();
        newerAdvanced[advanced.lockdeadScreenDelayId] = 2100;
        newerAdvanced[advanced.renderUnfocusedFpsId] = 60;
        newerAdvanced[advanced.disableSplashRenderingId] = true;
        newerAdvanced[advanced.captureModifiersId] = true;
        newerAdvanced[advanced.enforceCaptureBarriersId] = false;
        advanced.advancedProjectionAvailable = false;
        advanced.advancedAvailable = false;
        advanced.advancedValues = ({ broken: true });
        advanced.revisionToken = "12";
        wait(0);
        compare(advanced.externalChangeWhileEditing, true);
        compare(advanced.draftValue(advanced.sessionLockXrayId), true);
        compare(advanced.draftValue(advanced.sessionLockBlurId), true);
        compare(advanced.draftValue(
            advanced.xwaylandUseNearestNeighborId
        ), false);
        compare(advanced.draftValue(advanced.fp16SdrTransferId), 1);
        compare(advanced.draftValue(advanced.xpModeId), true);
        compare(advanced.draftValue(advanced.captureModifiersId), true);
        compare(advanced.draftValue(
            advanced.enforceCaptureBarriersId
        ), false);
        compare(application.advancedDraftNavigationState, "conflict");
        compare(advancedBadge.text, "Review");
        verify(String(advancedNavigation.Accessible.description)
            .includes("conflicts with a newer compositor revision"));
        advancedNavigation.clicked();
        compare(application.currentPage, "advanced");
        compare(advancedNavigation.checked, true);
        compare(advanced.visible, true);
        const loadAdvanced = findChild(
            advanced, "loadCurrentAdvancedButton"
        );
        compare(loadAdvanced.visible, true);
        compare(loadAdvanced.enabled, false);
        advanced.advancedValues = newerAdvanced;
        advanced.advancedProjectionAvailable = true;
        wait(0);
        compare(advanced.advancedAvailable, false);
        compare(loadAdvanced.enabled, true);
        loadAdvanced.clicked();
        compare(application.advancedDraftNavigationState, "clean");
        compare(advancedBadge.visible, false);
        compare(advanced.draftValue(
            advanced.allowSessionLockRestoreId
        ), false);
        compare(advanced.draftValue(
            advanced.lockdeadScreenDelayId
        ), 2100);
        compare(advanced.draftValue(advanced.renderUnfocusedFpsId), 60);
        compare(advanced.draftValue(
            advanced.disableSplashRenderingId
        ), true);
        compare(advanced.draftValue(advanced.sessionLockXrayId), false);
        compare(advanced.draftValue(advanced.sessionLockBlurId), false);
        compare(advanced.draftValue(
            advanced.xwaylandUseNearestNeighborId
        ), true);
        compare(advanced.draftValue(advanced.fp16SdrTransferId), 0);
        compare(advanced.draftValue(advanced.xpModeId), false);
        compare(advanced.draftValue(advanced.captureModifiersId), true);
        compare(advanced.draftValue(
            advanced.enforceCaptureBarriersId
        ), false);

        application.width = 620;
        application.height = 480;
        wait(0);
        const sidebarScroll = findChild(
            application, "sidebarNavigationScroll"
        );
        verify(sidebarScroll !== null);
        verify(sidebarScroll.contentItem.contentHeight
            > sidebarScroll.contentItem.height);
        componentsNavigation.clicked();
        wait(0);
        verify(sidebarScroll.contentItem.contentY > 0);
        barNavigation.clicked();
        wait(0);
        compare(sidebarScroll.contentItem.contentY, 0);
    }

    function test_mainFiltersPageSharedErrorsByOperation() {
        CompositorClient.clearError();
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const appearance = findChild(application, "appearancePage");
        const input = findChild(application, "inputPage");
        const displays = findChild(application, "displaysPage");
        const windows = findChild(application, "windowsLayoutPage");
        const workspaces = findChild(application, "workspacesPage");
        const advanced = findChild(application, "advancedPage");
        verify(appearance !== null);
        verify(input !== null);
        verify(displays !== null);
        verify(windows !== null);
        verify(workspaces !== null);
        verify(advanced !== null);

        const appearanceSharedOperations = [
            "appearance-apply",
            "compositor-apply",
            "recover"
        ];
        for (const operation of appearanceSharedOperations)
            compare(application.appearanceCompositorError(operation), true);
        compare(application.appearanceCompositorError("input-apply"), false);
        compare(application.appearanceCompositorError("windows-apply"), false);

        const inputSharedOperations = [
            "input-apply",
            "compositor-apply",
            "recover"
        ];
        for (const operation of inputSharedOperations)
            compare(application.inputCompositorError(operation), true);
        compare(application.inputCompositorError("appearance-apply"), false);
        compare(application.inputCompositorError("windows-apply"), false);

        const windowsSharedOperations = [
            "windows-apply",
            "compositor-apply",
            "recover"
        ];
        for (const operation of windowsSharedOperations)
            compare(application.windowsCompositorError(operation), true);
        compare(application.windowsCompositorError("appearance-apply"), false);
        compare(application.windowsCompositorError("input-apply"), false);

        const workspacesSharedOperations = [
            "workspaces-apply",
            "compositor-apply",
            "recover"
        ];
        for (const operation of workspacesSharedOperations)
            compare(application.workspacesCompositorError(operation), true);
        compare(
            application.workspacesCompositorError("appearance-apply"), false
        );
        compare(application.workspacesCompositorError("input-apply"), false);
        compare(application.workspacesCompositorError("windows-apply"), false);

        const advancedSharedOperations = [
            "advanced-apply",
            "compositor-apply",
            "recover"
        ];
        for (const operation of advancedSharedOperations)
            compare(application.advancedCompositorError(operation), true);
        compare(
            application.advancedCompositorError("appearance-apply"), false
        );
        compare(application.advancedCompositorError("input-apply"), false);
        compare(application.advancedCompositorError("rules-apply"), false);

        const displayOperations = [
            "adopt",
            "display-preview",
            "display-confirm",
            "display-revert",
            "display-refresh",
            "compositor-apply"
        ];
        for (const operation of displayOperations)
            compare(application.displaysCompositorError(operation), true);
        for (const operation of [
            "appearance-apply",
            "input-apply",
            "windows-apply",
            "workspaces-apply",
            "rules-apply",
            "shared-border-sync",
            "shared-spacing-sync",
            "recover",
            "",
            "unknown"
        ]) {
            compare(application.displaysCompositorError(operation), false);
        }

        const pageLocalOperations = [
            "adopt",
            "display-preview",
            "display-confirm",
            "display-revert",
            "display-refresh",
            "shared-border-sync",
            "shared-spacing-sync",
            "",
            "unknown"
        ];
        for (const operation of pageLocalOperations) {
            compare(application.appearanceCompositorError(operation), false);
            compare(application.inputCompositorError(operation), false);
            compare(application.windowsCompositorError(operation), false);
            compare(application.workspacesCompositorError(operation), false);
            compare(application.advancedCompositorError(operation), false);
        }

        try {
            CompositorClient.previewDisplayConfiguration([], 15);
            compare(
                CompositorClient.lastErrorOperation,
                "display-preview"
            );
            verify(CompositorClient.lastErrorName.length > 0);
            verify(CompositorClient.lastErrorMessage.length > 0);
            compare(appearance.errorName, "");
            compare(appearance.errorMessage, "");
            compare(input.sharedErrorName, "");
            compare(input.sharedErrorMessage, "");
            compare(windows.sharedErrorName, "");
            compare(windows.sharedErrorMessage, "");
            compare(workspaces.sharedErrorName, "");
            compare(workspaces.sharedErrorMessage, "");
            compare(advanced.sharedErrorName, "");
            compare(advanced.sharedErrorMessage, "");
            compare(displays.errorName, CompositorClient.lastErrorName);
            compare(displays.errorMessage, CompositorClient.lastErrorMessage);

            CompositorClient.applyConfiguration();
            compare(
                CompositorClient.lastErrorOperation,
                "compositor-apply"
            );
            compare(appearance.errorName, CompositorClient.lastErrorName);
            compare(
                appearance.errorMessage,
                CompositorClient.lastErrorMessage
            );
            compare(input.sharedErrorName, CompositorClient.lastErrorName);
            compare(
                input.sharedErrorMessage,
                CompositorClient.lastErrorMessage
            );
            compare(windows.sharedErrorName, CompositorClient.lastErrorName);
            compare(
                windows.sharedErrorMessage,
                CompositorClient.lastErrorMessage
            );
            compare(
                workspaces.sharedErrorName, CompositorClient.lastErrorName
            );
            compare(
                workspaces.sharedErrorMessage,
                CompositorClient.lastErrorMessage
            );
            compare(advanced.sharedErrorName, CompositorClient.lastErrorName);
            compare(
                advanced.sharedErrorMessage,
                CompositorClient.lastErrorMessage
            );
            compare(displays.errorName, CompositorClient.lastErrorName);
            compare(displays.errorMessage, CompositorClient.lastErrorMessage);
        } finally {
            CompositorClient.clearError();
        }
        compare(appearance.errorName, "");
        compare(appearance.errorMessage, "");
        compare(input.sharedErrorName, "");
        compare(input.sharedErrorMessage, "");
        compare(windows.sharedErrorName, "");
        compare(windows.sharedErrorMessage, "");
        compare(workspaces.sharedErrorName, "");
        compare(workspaces.sharedErrorMessage, "");
        compare(advanced.sharedErrorName, "");
        compare(advanced.sharedErrorMessage, "");
        compare(displays.errorName, "");
        compare(displays.errorMessage, "");
    }

    function test_mainNavigationIncludesAppearance() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const navigation = findChild(
            application,
            "appearanceNavigationItem"
        );
        const page = findChild(application, "appearancePage");
        verify(navigation !== null);
        verify(page !== null);
        compare(navigation.Accessible.name, "Appearance settings");
        navigation.clicked();
        compare(application.currentPage, "appearance");
        compare(navigation.checked, true);
        compare(page.visible, true);

        // Appearance offers only navigation into the existing confirmed
        // takeover workflow. It never forwards an adoption mutation itself.
        page.openDisplaysRequested();
        compare(application.currentPage, "displays");
        compare(findChild(application, "displaysPage").visible, true);
    }

    function test_mainNavigationIncludesHyprlandHubAndStructuredEditors() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const navigation = findChild(
            application, "hyprlandNavigationItem"
        );
        const overview = findChild(application, "hyprlandOverviewPage");
        const catalog = findChild(application, "hyprlandCatalogPage");
        const bindings = findChild(application, "bindingsPage");
        const input = findChild(application, "inputPage");
        const inputDevices = findChild(application, "inputDevicesPage");
        const environment = findChild(
            application, "environmentVariablesPage"
        );
        const permissions = findChild(application, "permissionsPage");
        verify(navigation !== null);
        verify(overview !== null);
        verify(catalog !== null);
        verify(bindings !== null);
        verify(input !== null);
        verify(inputDevices !== null);
        verify(environment !== null);
        verify(permissions !== null);
        compare(navigation.Accessible.name, "Hyprland settings");

        navigation.clicked();
        compare(application.currentPage, "hyprland");
        compare(application.hyprlandSection, "overview");
        compare(navigation.checked, true);
        compare(overview.visible, true);

        verify(overview.activateCategory("shortcuts"));
        compare(application.hyprlandSection, "catalog");
        compare(application.hyprlandCategory, "shortcuts");
        compare(catalog.visible, true);
        compare(catalog.guideTarget, "bindings");
        catalog.openSurfaceRequested("bindings");
        compare(application.hyprlandSection, "bindings");
        compare(bindings.visible, true);
        bindings.backRequested();
        compare(application.hyprlandSection, "overview");

        verify(overview.activateCategory("devices"));
        compare(application.hyprlandSection, "devices");
        compare(inputDevices.visible, true);
        inputDevices.backRequested();

        application.currentPage = "input";
        input.manageInputDeviceProfilesRequested();
        compare(application.currentPage, "hyprland");
        compare(application.hyprlandSection, "devices");
        compare(inputDevices.visible, true);
        inputDevices.backRequested();

        verify(overview.activateCategory("environment"));
        compare(application.hyprlandSection, "environment");
        compare(environment.visible, true);
        environment.backRequested();
        verify(overview.activateCategory("permissions"));
        compare(application.hyprlandSection, "permissions");
        compare(permissions.visible, true);
        permissions.backRequested();

        verify(overview.activateCategory("displays"));
        compare(application.currentPage, "displays");
        compare(findChild(application, "displaysPage").visible, true);
    }

    function test_unavailableFallbackRemainsVisible() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorAvailable = false;
        warning.fallbackActive = true;
        warning.fallbackAvailable = false;
        warning.fallbackBusy = false;
        waitForRendering(warning);
        compare(warning.warningVisible, true);
        compare(warning.warningTitle, "Service status unavailable");
        compare(warning.failedComponentCount, 0);

        warning.restartError = "The restart request was rejected.";
        const error = findChild(warning, "restartError");
        verify(error !== null);
        compare(error.visible, true);
        verify(error.text.includes("rejected"));
    }
}
