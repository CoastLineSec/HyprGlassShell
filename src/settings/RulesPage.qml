pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    property bool serviceAvailable: false
    property bool writable: false
    property bool catalogAvailable: false
    property bool rulesAvailable: false
    property bool rulesProjectionAvailable: false
    property bool busy: false
    property string busyOperation: ""
    property var windowRules: []
    property var layerRules: []
    property string revisionToken: "0"
    property string loadState: "unavailable"
    property string managementState: "unmanaged"
    property string applyState: "unavailable"
    property string requiredActivation: "none"
    property string confirmationState: "idle"
    property string rulesErrorName: ""
    property string rulesErrorMessage: ""
    property string sharedErrorName: ""
    property string sharedErrorMessage: ""
    property bool retryApplyAvailable: false
    property bool recoveryAvailable: false
    property bool sharedMutationBusy: false
    property bool sharedApplySafe: false
    property real contentTopMargin: 28
    property int rulesTabIndex: 0

    property var draftWindowRules: []
    property var draftLayerRules: []
    property var synchronizedWindowRules: []
    property var synchronizedLayerRules: []
    property var submittedWindowRules: []
    property var submittedLayerRules: []
    property string synchronizedRevisionToken: ""
    property string submittedRevisionToken: ""
    property bool projectionInitialized: false
    property bool externalChangeWhileEditing: false
    property bool saveSubmitted: false
    property string editingKind: ""
    property string editingRuleId: ""

    signal refreshRequested()
    signal openDisplaysRequested()
    signal saveRequested(var windowRules, var layerRules)
    signal retryApplyRequested()
    signal recoveryRequested()

    readonly property real minimumTargetSize: 44
    readonly property bool compactPage: root.width < 560
    readonly property var windowMatcherKeys: [
        "class", "title", "initial_class", "initial_title", "tag",
        "xwayland", "float", "fullscreen", "pin", "focus", "group",
        "modal", "fullscreen_state_internal", "fullscreen_state_client",
        "workspace", "content", "xdg_tag", "namespace"
    ]
    readonly property var windowEffectKeys: [
        "float", "tile", "fullscreen", "maximize", "center", "pseudo",
        "no_initial_focus", "pin", "fullscreen_state", "move", "size",
        "monitor", "workspace", "suppress_event", "content",
        "no_close_for", "scrolling_width", "rounding", "border_size",
        "rounding_power", "scroll_mouse", "scroll_touchpad", "animation",
        "idle_inhibit", "opacity", "tag", "max_size", "min_size",
        "border_color", "persistent_size", "allows_input", "dim_around",
        "decorate", "focus_on_activate", "keep_aspect_ratio",
        "nearest_neighbor", "no_anim", "no_blur", "no_dim", "no_focus",
        "no_follow_mouse", "no_max_size", "no_shadow",
        "no_shortcuts_inhibit", "opaque", "force_rgbx",
        "sync_fullscreen", "immediate", "xray", "render_unfocused",
        "no_screen_share", "no_vrr", "no_auto_hdr", "stay_focused",
        "confine_pointer", "tonemap"
    ]
    readonly property var layerMatcherKeys: ["namespace"]
    readonly property var layerEffectKeys: [
        "no_anim", "blur", "blur_popups", "ignore_alpha", "dim_around",
        "xray", "animation", "order", "above_lock", "no_screen_share"
    ]

    readonly property var windowMatcherDefinitions: [
        root.field("class", qsTr("Window class"), qsTr("Match the current application class with an RE2 pattern."), "text", "^$", "windowMatchClass", "identity", { maximumLength: 512, validation: "regex", placeholder: "^(firefox|org.example.App)$" }),
        root.field("title", qsTr("Window title"), qsTr("Match the current window title with an RE2 pattern."), "text", "^$", "windowMatchTitle", "identity", { maximumLength: 512, validation: "regex", placeholder: "^Document title$" }),
        root.field("initial_class", qsTr("Initial class"), qsTr("Match the application class captured when the window first appeared."), "text", "^$", "windowMatchInitialClass", "identity", { maximumLength: 512, validation: "regex" }),
        root.field("initial_title", qsTr("Initial title"), qsTr("Match the title captured when the window first appeared."), "text", "^$", "windowMatchInitialTitle", "identity", { maximumLength: 512, validation: "regex" }),
        root.field("tag", qsTr("Window tag"), qsTr("Match one exact nonempty window tag."), "text", "tag", "windowMatchTag", "identity", { maximumLength: 256, validation: "nonEmpty" }),
        root.field("content", qsTr("Content type pattern"), qsTr("Match the current content type with an RE2 pattern."), "text", "^$", "windowMatchContent", "identity", { maximumLength: 512, validation: "regex" }),
        root.field("xdg_tag", qsTr("XDG tag pattern"), qsTr("Match the window's XDG tag with an RE2 pattern."), "text", "^$", "windowMatchXdgTag", "identity", { maximumLength: 512, validation: "regex" }),
        root.field("namespace", qsTr("Namespace pattern"), qsTr("Match the window namespace with an RE2 pattern."), "text", "^$", "windowMatchNamespace", "identity", { maximumLength: 512, validation: "regex" }),
        root.field("xwayland", qsTr("XWayland window"), qsTr("Require or reject an XWayland-backed window."), "boolean", true, "windowMatchXwayland", "state"),
        root.field("float", qsTr("Floating state"), qsTr("Require or reject a floating window."), "boolean", true, "windowMatchFloat", "state"),
        root.field("fullscreen", qsTr("Fullscreen state"), qsTr("Require or reject a fullscreen window."), "boolean", true, "windowMatchFullscreen", "state"),
        root.field("pin", qsTr("Pinned state"), qsTr("Require or reject a pinned window."), "boolean", true, "windowMatchPin", "state"),
        root.field("focus", qsTr("Focus state"), qsTr("Require or reject the currently focused window."), "boolean", true, "windowMatchFocus", "state"),
        root.field("group", qsTr("Grouped state"), qsTr("Require or reject a grouped window."), "boolean", true, "windowMatchGroup", "state"),
        root.field("modal", qsTr("Modal state"), qsTr("Require or reject a modal window."), "boolean", true, "windowMatchModal", "state"),
        root.field("fullscreen_state_internal", qsTr("Internal fullscreen mode"), qsTr("Match the compositor's internal fullscreen mode."), "enum", 0, "windowMatchFullscreenStateInternal", "state", { values: [0, 1, 2], labels: [qsTr("None"), qsTr("Maximized"), qsTr("Fullscreen")] }),
        root.field("fullscreen_state_client", qsTr("Client fullscreen mode"), qsTr("Match the fullscreen mode reported to the client."), "enum", 0, "windowMatchFullscreenStateClient", "state", { values: [0, 1, 2], labels: [qsTr("None"), qsTr("Maximized"), qsTr("Fullscreen")] }),
        root.field("workspace", qsTr("Workspace"), qsTr("Match one positive numeric, named, or special workspace selector."), "text", "1", "windowMatchWorkspace", "state", { maximumLength: 136, validation: "workspaceSelector", placeholder: "1, name:work, or special" })
    ]

    readonly property var windowEffectDefinitions: [
        root.field("float", qsTr("Set floating"), qsTr("Set or clear the floating state."), "boolean", true, "windowEffectFloat", "placement"),
        root.field("tile", qsTr("Set tiled"), qsTr("Set or clear the tiled state."), "boolean", true, "windowEffectTile", "placement"),
        root.field("fullscreen", qsTr("Set fullscreen"), qsTr("Set or clear compositor fullscreen."), "boolean", true, "windowEffectFullscreen", "placement"),
        root.field("maximize", qsTr("Set maximized"), qsTr("Set or clear the maximized state."), "boolean", true, "windowEffectMaximize", "placement"),
        root.field("center", qsTr("Center window"), qsTr("Center or stop centering the matching window."), "boolean", true, "windowEffectCenter", "placement"),
        root.field("pseudo", qsTr("Pseudo-tile"), qsTr("Set or clear pseudo-tiled sizing."), "boolean", true, "windowEffectPseudo", "placement"),
        root.field("pin", qsTr("Pin window"), qsTr("Set or clear the pinned state."), "boolean", true, "windowEffectPin", "placement"),
        root.field("fullscreen_state", qsTr("Fullscreen state pair"), qsTr("Set an internal fullscreen mode and an optional client mode."), "fullscreenState", { internal: 0 }, "windowEffectFullscreenState", "placement"),
        root.field("move", qsTr("Move position"), qsTr("Move the window to an exact compositor-space X and Y position."), "vector2", [0, 0], "windowEffectMove", "placement", { minimum: -1000000, maximum: 1000000 }),
        root.field("size", qsTr("Window size"), qsTr("Set exact width and height values."), "vector2", [0, 0], "windowEffectSize", "placement", { minimum: -1000000, maximum: 1000000 }),
        root.field("monitor", qsTr("Move to monitor"), qsTr("Choose a pinned monitor target token or unset a previous target."), "target", { target: "unset", silent: false }, "windowEffectMonitor", "placement", { targetKind: "monitor", placeholder: "current, DP-1, desc:…, +1, or unset" }),
        root.field("workspace", qsTr("Move to workspace"), qsTr("Choose a pinned workspace target or unset a previous target."), "target", { target: "unset", silent: false }, "windowEffectWorkspace", "placement", { targetKind: "workspace", placeholder: "1, name:work, special, next, or unset" }),

        root.field("no_initial_focus", qsTr("Block initial focus"), qsTr("Set whether a newly matching window may take initial focus."), "boolean", true, "windowEffectNoInitialFocus", "focus"),
        root.field("allows_input", qsTr("Allow input"), qsTr("Set whether the matching window accepts input."), "boolean", true, "windowEffectAllowsInput", "focus"),
        root.field("focus_on_activate", qsTr("Focus on activation"), qsTr("Set whether activation requests focus the window."), "boolean", true, "windowEffectFocusOnActivate", "focus"),
        root.field("no_focus", qsTr("Block focus"), qsTr("Set whether the window is excluded from focus."), "boolean", true, "windowEffectNoFocus", "focus"),
        root.field("no_follow_mouse", qsTr("Ignore pointer focus"), qsTr("Set whether pointer-follow focus ignores this window."), "boolean", true, "windowEffectNoFollowMouse", "focus"),
        root.field("stay_focused", qsTr("Stay focused"), qsTr("Set whether focus should remain on this window."), "boolean", true, "windowEffectStayFocused", "focus"),
        root.field("confine_pointer", qsTr("Confine pointer"), qsTr("Set whether the pointer is confined to the window."), "boolean", true, "windowEffectConfinePointer", "focus"),
        root.field("no_shortcuts_inhibit", qsTr("Ignore shortcut inhibition"), qsTr("Set whether the window may inhibit compositor shortcuts."), "boolean", true, "windowEffectNoShortcutsInhibit", "focus"),

        root.field("rounding", qsTr("Corner radius"), qsTr("Set a per-window corner radius from 0 through 20."), "integer", 0, "windowEffectRounding", "appearance", { minimum: 0, maximum: 20 }),
        root.field("border_size", qsTr("Border size"), qsTr("Set a lossless signed safe-integer border size."), "safeInteger", 0, "windowEffectBorderSize", "appearance"),
        root.field("rounding_power", qsTr("Rounding power"), qsTr("Set the corner-curve power from 1 through 10."), "number", 2, "windowEffectRoundingPower", "appearance", { minimum: 1, maximum: 10 }),
        root.field("border_color", qsTr("Border gradient"), qsTr("Set one to ten canonical RGBA colors and an angle."), "gradient", { colors: ["0xFFFFFFFF"], angle: 0 }, "windowEffectBorderColor", "appearance"),
        root.field("opacity", qsTr("Window opacity"), qsTr("Set active, optional inactive and fullscreen opacity with explicit override flags."), "opacity", { active: 1, overrideActive: false, overrideInactive: false, overrideFullscreen: false }, "windowEffectOpacity", "appearance"),
        root.field("decorate", qsTr("Decorations"), qsTr("Set whether Hyprland draws window decorations."), "boolean", true, "windowEffectDecorate", "appearance"),
        root.field("dim_around", qsTr("Dim around window"), qsTr("Set whether surrounding content is dimmed."), "boolean", true, "windowEffectDimAround", "appearance"),
        root.field("no_anim", qsTr("Disable animation"), qsTr("Set whether window animation is disabled."), "boolean", true, "windowEffectNoAnim", "appearance"),
        root.field("no_blur", qsTr("Disable blur"), qsTr("Set whether blur is disabled behind the window."), "boolean", true, "windowEffectNoBlur", "appearance"),
        root.field("no_dim", qsTr("Disable dimming"), qsTr("Set whether inactive-window dimming is disabled."), "boolean", true, "windowEffectNoDim", "appearance"),
        root.field("no_shadow", qsTr("Disable shadow"), qsTr("Set whether the window shadow is disabled."), "boolean", true, "windowEffectNoShadow", "appearance"),
        root.field("opaque", qsTr("Treat as opaque"), qsTr("Set whether Hyprland treats the surface as opaque."), "boolean", true, "windowEffectOpaque", "appearance"),
        root.field("xray", qsTr("X-ray blur"), qsTr("Set whether blur uses X-ray behavior."), "boolean", true, "windowEffectXray", "appearance"),
        root.field("render_unfocused", qsTr("Render while unfocused"), qsTr("Set whether the window keeps rendering when unfocused."), "boolean", true, "windowEffectRenderUnfocused", "appearance"),
        root.field("force_rgbx", qsTr("Force RGBX"), qsTr("Set whether the surface alpha channel is ignored."), "boolean", true, "windowEffectForceRgbx", "appearance"),
        root.field("nearest_neighbor", qsTr("Nearest-neighbor scaling"), qsTr("Set whether the window uses nearest-neighbor scaling."), "boolean", true, "windowEffectNearestNeighbor", "appearance"),

        root.field("suppress_event", qsTr("Suppress events"), qsTr("Choose one or more window events not forwarded to clients."), "suppressEvents", ["fullscreen"], "windowEffectSuppressEvent", "behavior"),
        root.field("content", qsTr("Content type"), qsTr("Set the managed content classification."), "enum", "none", "windowEffectContent", "behavior", { values: ["none", "photo", "video", "game"], labels: [qsTr("None"), qsTr("Photo"), qsTr("Video"), qsTr("Game")] }),
        root.field("no_close_for", qsTr("Delay closing"), qsTr("Block closing for this many milliseconds after the window appears."), "integer", 0, "windowEffectNoCloseFor", "behavior", { minimum: 0, maximum: 2147483647 }),
        root.field("scrolling_width", qsTr("Scrolling column width"), qsTr("Set the Scrolling layout column-width fraction."), "number", 0.5, "windowEffectScrollingWidth", "behavior", { minimum: 0, maximum: 1 }),
        root.field("scroll_mouse", qsTr("Mouse scroll factor"), qsTr("Set the window-specific mouse scroll factor."), "number", 1, "windowEffectScrollMouse", "behavior", { minimum: 0.01, maximum: 10 }),
        root.field("scroll_touchpad", qsTr("Touchpad scroll factor"), qsTr("Set the window-specific touchpad scroll factor."), "number", 1, "windowEffectScrollTouchpad", "behavior", { minimum: 0.01, maximum: 10 }),
        root.field("animation", qsTr("Animation style"), qsTr("Choose one pinned window-animation form."), "animation", "", "windowEffectAnimation", "behavior", { animationKind: "window" }),
        root.field("idle_inhibit", qsTr("Idle inhibition"), qsTr("Choose when this window inhibits desktop idle."), "enum", "none", "windowEffectIdleInhibit", "behavior", { values: ["none", "always", "focus", "fullscreen"], labels: [qsTr("Never"), qsTr("Always"), qsTr("While focused"), qsTr("While fullscreen")] }),
        root.field("tag", qsTr("Change window tag"), qsTr("Apply, remove, or replace one canonical tag token."), "text", "tag", "windowEffectTag", "behavior", { maximumLength: 129, validation: "windowTag", placeholder: "+tag, -tag, or tag" }),
        root.field("max_size", qsTr("Maximum size"), qsTr("Set maximum width and height values."), "vector2", [0, 0], "windowEffectMaxSize", "behavior", { minimum: -1000000, maximum: 1000000 }),
        root.field("min_size", qsTr("Minimum size"), qsTr("Set minimum width and height values."), "vector2", [0, 0], "windowEffectMinSize", "behavior", { minimum: -1000000, maximum: 1000000 }),
        root.field("persistent_size", qsTr("Remember size"), qsTr("Set whether Hyprland remembers this window's size."), "boolean", true, "windowEffectPersistentSize", "behavior"),
        root.field("keep_aspect_ratio", qsTr("Keep aspect ratio"), qsTr("Set whether resize operations preserve the window ratio."), "boolean", true, "windowEffectKeepAspectRatio", "behavior"),
        root.field("no_max_size", qsTr("Ignore maximum size"), qsTr("Set whether client maximum-size hints are ignored."), "boolean", true, "windowEffectNoMaxSize", "behavior"),
        root.field("sync_fullscreen", qsTr("Synchronize fullscreen"), qsTr("Set whether fullscreen state is synchronized with the client."), "boolean", true, "windowEffectSyncFullscreen", "behavior"),
        root.field("immediate", qsTr("Immediate presentation"), qsTr("Set immediate presentation behavior for the window."), "boolean", true, "windowEffectImmediate", "behavior"),
        root.field("no_screen_share", qsTr("Exclude from screen sharing"), qsTr("Set whether the window is hidden from screen sharing."), "boolean", true, "windowEffectNoScreenShare", "behavior"),
        root.field("no_vrr", qsTr("Disable VRR"), qsTr("Set whether variable refresh rate is disabled for the window."), "boolean", true, "windowEffectNoVrr", "behavior"),
        root.field("no_auto_hdr", qsTr("Disable automatic HDR"), qsTr("Set whether automatic HDR is disabled for the window."), "boolean", true, "windowEffectNoAutoHdr", "behavior"),
        root.field("tonemap", qsTr("Tone mapping"), qsTr("Choose the managed tone-mapping mode."), "enum", "on", "windowEffectTonemap", "behavior", { values: ["on", "off", "clamp", "limited"], labels: [qsTr("On"), qsTr("Off"), qsTr("Clamp"), qsTr("Limited")] })
    ]

    readonly property var layerMatcherDefinitions: [
        root.field("namespace", qsTr("Layer namespace"), qsTr("Match the layer surface namespace with an RE2 pattern."), "text", "^$", "layerMatchNamespace", "identity", { maximumLength: 512, validation: "regex", placeholder: "^(waybar|launcher)$" })
    ]

    readonly property var layerEffectDefinitions: [
        root.field("no_anim", qsTr("Disable animation"), qsTr("Set whether layer-surface animation is disabled."), "boolean", true, "layerEffectNoAnim", "appearance"),
        root.field("blur", qsTr("Blur behind surface"), qsTr("Set whether the compositor blurs behind the layer surface."), "boolean", true, "layerEffectBlur", "appearance"),
        root.field("blur_popups", qsTr("Blur popups"), qsTr("Set whether popups belonging to the surface are blurred."), "boolean", true, "layerEffectBlurPopups", "appearance"),
        root.field("ignore_alpha", qsTr("Blur alpha threshold"), qsTr("Ignore pixels below this alpha value when blurring."), "number", 0, "layerEffectIgnoreAlpha", "appearance", { minimum: 0, maximum: 1 }),
        root.field("dim_around", qsTr("Dim around surface"), qsTr("Set whether content around the surface is dimmed."), "boolean", true, "layerEffectDimAround", "appearance"),
        root.field("xray", qsTr("X-ray blur"), qsTr("Set whether layer blur uses X-ray behavior."), "boolean", true, "layerEffectXray", "appearance"),
        root.field("animation", qsTr("Animation style"), qsTr("Choose one pinned layer-animation form."), "animation", "", "layerEffectAnimation", "appearance", { animationKind: "layer" }),
        root.field("order", qsTr("Rule order value"), qsTr("Set the full signed safe-integer layer-rule order value."), "safeInteger", 0, "layerEffectOrder", "behavior"),
        root.field("above_lock", qsTr("Above-lock level"), qsTr("Choose the pinned above-lock level from 0 through 2."), "enum", 0, "layerEffectAboveLock", "behavior", { values: [0, 1, 2], labels: [qsTr("Level 0"), qsTr("Level 1"), qsTr("Level 2")] }),
        root.field("no_screen_share", qsTr("Exclude from screen sharing"), qsTr("Set whether the layer surface is hidden from screen sharing."), "boolean", true, "layerEffectNoScreenShare", "behavior")
    ]

    readonly property bool definitionsValid:
        root.definitionKeysEqual(
            root.windowMatcherDefinitions, root.windowMatcherKeys)
        && root.definitionKeysEqual(
            root.windowEffectDefinitions, root.windowEffectKeys)
        && root.definitionKeysEqual(
            root.layerMatcherDefinitions, root.layerMatcherKeys)
        && root.definitionKeysEqual(
            root.layerEffectDefinitions, root.layerEffectKeys)
    readonly property bool trustedValuesValid:
        root.rulesProjectionAvailable
        && root.definitionsValid
        && root.validateRuleCollection(root.windowRules, "window")
        && root.validateRuleCollection(root.layerRules, "layer")
    readonly property bool revisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.revisionToken)
    readonly property bool synchronizedRevisionTokenValid:
        /^(0|[1-9][0-9]*)$/.test(root.synchronizedRevisionToken)
    readonly property bool draftValid:
        root.definitionsValid
        && root.validateRuleCollection(root.draftWindowRules, "window")
        && root.validateRuleCollection(root.draftLayerRules, "layer")
    readonly property bool draftDirty:
        root.projectionInitialized
        && (!root.valueEqual(
                root.draftWindowRules, root.synchronizedWindowRules)
            || !root.valueEqual(
                root.draftLayerRules, root.synchronizedLayerRules))
    readonly property bool displayTestActive:
        root.confirmationState !== "idle"
        || root.managementState === "preview"
    readonly property bool controlsEnabled:
        root.serviceAvailable
        && root.writable
        && root.catalogAvailable
        && root.rulesAvailable
        && root.revisionTokenValid
        && root.definitionsValid
        && root.trustedValuesValid
        && !root.busy
        && !root.saveSubmitted
        && !root.sharedMutationBusy
        && root.sharedApplySafe
        && !root.externalChangeWhileEditing
        && !root.displayTestActive
    readonly property bool saveEnabled:
        root.controlsEnabled && root.draftDirty && root.draftValid
        && !root.saveSubmitted && root.sharedApplySafe
    readonly property bool discardEnabled:
        root.serviceAvailable && root.rulesProjectionAvailable
        && root.trustedValuesValid && root.projectionInitialized
        && !root.busy && !root.saveSubmitted && !root.sharedMutationBusy
        && !root.externalChangeWhileEditing
    readonly property bool resetEnabled:
        root.draftWindowRules.length > 0 || root.draftLayerRules.length > 0
    readonly property var editingRule:
        root.ruleById(root.editingKind, root.editingRuleId)
    readonly property bool editorActive:
        (root.editingKind === "window" || root.editingKind === "layer")
        && root.editingRule !== null
    readonly property string editorIssue: root.currentRuleIssue()
    readonly property var editorGroups: {
        if (root.editingKind === "window") {
            return [
                {
                    section: "match",
                    group: "identity",
                    title: qsTr("Identity matchers"),
                    description: qsTr("Choose the application, title, tag, content, or namespace that identifies a window.")
                },
                {
                    section: "match",
                    group: "state",
                    title: qsTr("State matchers"),
                    description: qsTr("Narrow this rule to a current window state or workspace.")
                },
                {
                    section: "effects",
                    group: "placement",
                    title: qsTr("Placement effects"),
                    description: qsTr("Control tiling state, position, size, monitor, and workspace placement.")
                },
                {
                    section: "effects",
                    group: "focus",
                    title: qsTr("Focus and input effects"),
                    description: qsTr("Control focus, activation, pointer confinement, and shortcut inhibition.")
                },
                {
                    section: "effects",
                    group: "appearance",
                    title: qsTr("Appearance effects"),
                    description: qsTr("Apply per-window border, opacity, decoration, blur, shadow, and rendering choices.")
                },
                {
                    section: "effects",
                    group: "behavior",
                    title: qsTr("Behavior effects"),
                    description: qsTr("Apply the remaining managed window behavior and presentation rules.")
                }
            ];
        }
        if (root.editingKind === "layer") {
            return [
                {
                    section: "match",
                    group: "identity",
                    title: qsTr("Namespace matcher"),
                    description: qsTr("Match the layer surface namespace with one RE2 pattern.")
                },
                {
                    section: "effects",
                    group: "appearance",
                    title: qsTr("Appearance effects"),
                    description: qsTr("Control blur, animation, alpha handling, dimming, and X-ray behavior.")
                },
                {
                    section: "effects",
                    group: "behavior",
                    title: qsTr("Behavior effects"),
                    description: qsTr("Control rule order, lock-screen placement, and screen-sharing visibility.")
                }
            ];
        }
        return [];
    }
    readonly property bool statusVisible:
        !root.serviceAvailable
        || !root.writable
        || !root.catalogAvailable
        || !root.rulesAvailable
        || !root.rulesProjectionAvailable
        || !root.revisionTokenValid
        || !root.definitionsValid
        || !root.trustedValuesValid
        || root.loadState === "recovered"
        || root.loadState === "defaulted"
        || root.loadState === "unsupported"
        || root.managementState !== "managed"
        || root.applyState !== "current"
        || root.requiredActivation !== "none"
        || root.displayTestActive
        || root.externalChangeWhileEditing
        || root.rulesErrorMessage.length > 0
        || root.sharedErrorMessage.length > 0
        || root.busy
        || root.sharedMutationBusy
        || !root.sharedApplySafe
    readonly property bool statusIsDanger:
        root.managementState === "conflict"
        || root.applyState === "failed"
        || root.loadState === "unsupported"
        || (root.catalogAvailable && !root.definitionsValid)
        || (root.serviceAvailable && root.catalogAvailable
            && !root.rulesProjectionAvailable
            && root.rulesErrorMessage.length > 0)
        || (root.rulesProjectionAvailable && !root.trustedValuesValid)
        || root.externalChangeWhileEditing
    readonly property string statusMessage: {
        const rulesDetail = root.rulesErrorMessage.length > 0
            ? " " + root.rulesErrorMessage : "";
        const sharedDetail = root.sharedErrorMessage.length > 0
            ? " " + root.sharedErrorMessage : "";
        if (!root.serviceAvailable)
            return qsTr("Rule settings are unavailable. The compositor settings service may be restarting.%1").arg(sharedDetail);
        if (root.displayTestActive)
            return qsTr("A display test is active. Rule changes stay locked until that test is kept or reverted.");
        if (root.managementState === "unmanaged")
            return qsTr("HyprShelld is not managing the Hyprland entrypoint yet. Review takeover from Displays before changing rules.");
        if (root.managementState === "conflict")
            return qsTr("The managed compositor entrypoint or its ownership state changed unexpectedly. Rule changes are locked to preserve it.%1").arg(sharedDetail);
        if (!root.writable)
            return qsTr("This compositor configuration is read-only and has been preserved.");
        if (!root.catalogAvailable)
            return qsTr("The trusted Hyprland catalog is unavailable or does not match the compositor authority. Rule changes are disabled.%1").arg(rulesDetail);
        if (!root.definitionsValid)
            return qsTr("The trusted Rules contract does not match this Settings build. No compositor values will be written.%1").arg(rulesDetail);
        if (!root.rulesProjectionAvailable) {
            return root.rulesErrorMessage.length > 0
                ? qsTr("Rules authority verification failed. Existing scalar settings may remain readable, but rules cannot be trusted or changed until this check succeeds.%1").arg(rulesDetail)
                : qsTr("Rules are waiting for a current, verified compositor projection before they can be read or changed.");
        }
        if (!root.trustedValuesValid)
            return qsTr("The current Window or Layer Rules projection is not a valid managed-v1 rules document. No compositor values will be written.%1").arg(rulesDetail);
        if (!root.revisionTokenValid)
            return qsTr("The exact compositor revision token is unavailable. Rule changes are disabled to prevent overwriting another revision.");
        if (root.externalChangeWhileEditing)
            return qsTr("Compositor settings changed outside this draft. Your complete Window and Layer Rules draft is preserved, but it cannot be saved over the newer revision. Load the current settings to continue.");
        if (root.busy) {
            if (root.busyOperation === "rules-save")
                return qsTr("Saving the validated Window and Layer Rules draft…");
            if (root.busyOperation === "compositor-apply"
                    || root.busyOperation === "rules-apply") {
                return qsTr("Applying and verifying the saved compositor revision…");
            }
            if (root.busyOperation === "recover")
                return qsTr("Restoring and verifying the last working compositor configuration…");
            return qsTr("Another compositor operation is in progress. Rule changes are temporarily locked.");
        }
        if (root.sharedMutationBusy)
            return qsTr("A shared compositor setting is changing. Rule changes remain locked until that transition is verified.");
        if (root.applyState === "retained"
                || root.applyState === "failed"
                || root.applyState === "inactive") {
            if (root.requiredActivation === "reload") {
                return root.retryApplyAvailable
                    ? qsTr("The desired compositor settings were saved, but they are not active. Retry the exact saved revision or restore the last working configuration.%1").arg(sharedDetail)
                    : qsTr("The desired compositor settings are saved but not active. Wait for retry or recovery to become available.%1").arg(sharedDetail);
            }
            if (root.requiredActivation === "restart")
                return qsTr("The saved desired state requires a verified compositor-restart workflow that HyprShelld does not have yet, so this revision cannot be activated from Settings. Restore the last working compositor configuration to continue.%1").arg(sharedDetail);
            if (root.requiredActivation === "session")
                return qsTr("The saved desired state requires a verified new-session workflow that HyprShelld does not have yet, so this revision cannot be activated from Settings. Restore the last working compositor configuration to continue.%1").arg(sharedDetail);
            return qsTr("The desired compositor state is not the active state. Review recovery options before making another change.%1").arg(sharedDetail);
        }
        if (root.loadState === "recovered")
            return qsTr("Compositor settings were restored from the last known good desired-state copy. Review the rules before changing them.");
        if (root.loadState === "defaulted")
            return qsTr("Compositor settings could not be recovered, so safe desired-state defaults are in use. Review the rules before continuing.");
        if (root.loadState === "unsupported")
            return qsTr("The compositor settings use a newer format. They were preserved and remain read-only.");
        if (root.rulesErrorMessage.length > 0)
            return qsTr("The Rules operation failed. Your draft was preserved.%1").arg(rulesDetail);
        if (root.sharedErrorMessage.length > 0)
            return qsTr("The compositor operation failed.%1").arg(sharedDetail);
        if (!root.rulesAvailable)
            return qsTr("Rule changes are waiting for a current, verified compositor baseline.%1").arg(rulesDetail);
        if (!root.sharedApplySafe)
            return qsTr("A shared compositor setting is not at a verified activation point. Rule controls remain locked until the exact source transition is verified.");
        return "";
    }

    function field(
        key, title, description, kind, defaultValue,
        controlObjectName, group, extra
    ) {
        const result = {
            key,
            title,
            description,
            kind,
            defaultValue,
            controlObjectName,
            includeObjectName: controlObjectName + "Include",
            group
        };
        if (extra && typeof extra === "object") {
            for (const name of Object.keys(extra))
                result[name] = extra[name];
        }
        return result;
    }

    function clone(value) {
        try {
            return JSON.parse(JSON.stringify(value));
        } catch (error) {
            return null;
        }
    }

    function valueEqual(left, right) {
        if (Array.isArray(left) || Array.isArray(right)) {
            if (!Array.isArray(left) || !Array.isArray(right)
                    || left.length !== right.length) {
                return false;
            }
            for (let index = 0; index < left.length; ++index) {
                if (!root.valueEqual(left[index], right[index]))
                    return false;
            }
            return true;
        }
        if (left && right && typeof left === "object"
                && typeof right === "object") {
            const leftKeys = Object.keys(left).sort();
            const rightKeys = Object.keys(right).sort();
            if (!root.valueEqual(leftKeys, rightKeys))
                return false;
            for (const key of leftKeys) {
                if (!root.valueEqual(left[key], right[key]))
                    return false;
            }
            return true;
        }
        return left === right;
    }

    function definitionKeysEqual(definitions, keys) {
        if (!Array.isArray(definitions) || !Array.isArray(keys)
                || definitions.length !== keys.length) {
            return false;
        }
        const authored = definitions.map(item => item.key).sort();
        const expected = root.clone(keys).sort();
        return root.valueEqual(authored, expected)
            && new Set(authored).size === authored.length;
    }

    function definitions(kind, section) {
        if (kind === "window") {
            return section === "match"
                ? root.windowMatcherDefinitions
                : root.windowEffectDefinitions;
        }
        return section === "match"
            ? root.layerMatcherDefinitions : root.layerEffectDefinitions;
    }

    function definitionsForGroup(definitions, group) {
        return definitions.filter(item => item.group === group);
    }

    function definitionByKey(kind, section, key) {
        for (const definition of root.definitions(kind, section)) {
            if (definition.key === key)
                return definition;
        }
        return null;
    }

    function hasDisallowedCharacter(value) {
        if (typeof value !== "string")
            return true;
        for (let index = 0; index < value.length; ++index) {
            const code = value.charCodeAt(index);
            if (code <= 31 || code === 127)
                return true;
        }
        return false;
    }

    function isSchemaString(value, maximum, allowEmpty) {
        return typeof value === "string"
            && value.length <= maximum
            && (allowEmpty || value.length > 0)
            && !root.hasDisallowedCharacter(value);
    }

    function isStableId(value) {
        return typeof value === "string" && value.length >= 1
            && value.length <= 128
            && /^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/.test(value);
    }

    function isWorkspaceSelector(value) {
        if (typeof value !== "string")
            return false;
        if (/^[1-9][0-9]*$/.test(value)) {
            const number = Number(value);
            return Number.isInteger(number) && number <= 2147483647;
        }
        if (/^name:[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$/.test(value))
            return true;
        return /^special(?::[A-Za-z0-9_][A-Za-z0-9_.-]{0,127})?$/.test(value);
    }

    function isMonitorTarget(value) {
        if (typeof value !== "string")
            return false;
        if (value === "unset" || value === "current" || value === "left"
                || value === "right" || value === "up" || value === "down") {
            return true;
        }
        if (/^[+-][1-9][0-9]*$/.test(value)) {
            const relative = Number(value.slice(1));
            return Number.isInteger(relative) && relative <= 2147483647;
        }
        if (/^(0|[1-9][0-9]*)$/.test(value)) {
            const index = Number(value);
            return Number.isInteger(index) && index <= 2147483647;
        }
        if (value.startsWith("desc:")) {
            const description = value.slice(5);
            return root.isSchemaString(description, 256, false)
                && description === description.trim();
        }
        return /^[A-Za-z][A-Za-z0-9_.-]{0,127}$/.test(value)
            && !["current", "left", "right", "up", "down"].includes(value);
    }

    function isWorkspaceTarget(value) {
        return value === "unset" || value === "previous"
            || value === "previous_per_monitor" || value === "next"
            || value === "empty" || root.isWorkspaceSelector(value);
    }

    function isWindowTag(value) {
        return typeof value === "string" && value.length >= 1
            && value.length <= 129
            && /^[+-]?[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/.test(value);
    }

    function isAnimation(value, layer) {
        if (typeof value !== "string" || value.length > 128)
            return false;
        if (value === "" || value === "slide" || value === "popin")
            return true;
        if (layer && value === "fade")
            return true;
        if (!layer && (value === "gnome" || value === "gnomed"))
            return true;
        if (["slide top", "slide bottom", "slide left", "slide right"]
                .includes(value)) {
            return true;
        }
        if (!value.startsWith("popin ") || !value.endsWith("%"))
            return false;
        const percent = value.slice(6, -1);
        if (!/^(0|[1-9][0-9]?|100)$/.test(percent))
            return false;
        return true;
    }

    function objectHasExactKeys(value, required, optional) {
        if (!value || typeof value !== "object" || Array.isArray(value))
            return false;
        const allowed = required.concat(optional || []);
        for (const key of required) {
            if (!Object.prototype.hasOwnProperty.call(value, key))
                return false;
        }
        return Object.keys(value).every(key => allowed.includes(key));
    }

    function validateField(definition, value) {
        if (!definition)
            return false;
        if (definition.kind === "boolean")
            return typeof value === "boolean";
        if (definition.kind === "enum")
            return Array.isArray(definition.values)
                && definition.values.includes(value);
        if (definition.kind === "integer") {
            return typeof value === "number" && Number.isFinite(value)
                && Number.isInteger(value)
                && value >= definition.minimum
                && value <= definition.maximum;
        }
        if (definition.kind === "safeInteger")
            return typeof value === "number" && Number.isSafeInteger(value);
        if (definition.kind === "number") {
            return typeof value === "number" && Number.isFinite(value)
                && value >= definition.minimum
                && value <= definition.maximum;
        }
        if (definition.kind === "text") {
            if (definition.validation === "workspaceSelector")
                return root.isWorkspaceSelector(value);
            if (definition.validation === "windowTag")
                return root.isWindowTag(value);
            return root.isSchemaString(
                value, definition.maximumLength || 512, false
            );
        }
        if (definition.kind === "vector2") {
            return Array.isArray(value) && value.length === 2
                && value.every(item => typeof item === "number"
                    && Number.isFinite(item)
                    && item >= definition.minimum
                    && item <= definition.maximum);
        }
        if (definition.kind === "target") {
            return root.objectHasExactKeys(value, ["target", "silent"], [])
                && typeof value.silent === "boolean"
                && (definition.targetKind === "monitor"
                    ? root.isMonitorTarget(value.target)
                    : root.isWorkspaceTarget(value.target));
        }
        if (definition.kind === "fullscreenState") {
            return root.objectHasExactKeys(
                    value, ["internal"], ["client"])
                && Number.isInteger(value.internal)
                && value.internal >= 0 && value.internal <= 2
                && (!Object.prototype.hasOwnProperty.call(value, "client")
                    || (Number.isInteger(value.client)
                        && value.client >= 0 && value.client <= 2));
        }
        if (definition.kind === "suppressEvents") {
            const allowed = [
                "fullscreen", "maximize", "activate", "activatefocus",
                "fullscreenoutput", "x11configurerequest"
            ];
            return Array.isArray(value) && value.length >= 1
                && value.length <= 6 && new Set(value).size === value.length
                && value.every(item => allowed.includes(item));
        }
        if (definition.kind === "animation")
            return root.isAnimation(
                value, definition.animationKind === "layer"
            );
        if (definition.kind === "opacity") {
            if (!root.objectHasExactKeys(
                    value,
                    ["active", "overrideActive", "overrideInactive",
                        "overrideFullscreen"],
                    ["inactive", "fullscreen"])) {
                return false;
            }
            for (const key of ["active", "inactive", "fullscreen"]) {
                if (Object.prototype.hasOwnProperty.call(value, key)
                        && (typeof value[key] !== "number"
                            || !Number.isFinite(value[key])
                            || value[key] < 0 || value[key] > 1)) {
                    return false;
                }
            }
            return typeof value.overrideActive === "boolean"
                && typeof value.overrideInactive === "boolean"
                && typeof value.overrideFullscreen === "boolean";
        }
        if (definition.kind === "gradient") {
            if (!root.objectHasExactKeys(value, ["colors", "angle"], [])
                    || !Array.isArray(value.colors)
                    || value.colors.length < 1 || value.colors.length > 10
                    || typeof value.angle !== "number"
                    || !Number.isFinite(value.angle)
                    || value.angle < -3600 || value.angle > 3600) {
                return false;
            }
            return value.colors.every(color =>
                typeof color === "string"
                    && /^0x[0-9A-F]{8}$/.test(color));
        }
        return false;
    }

    function validateFieldMap(value, kind, section) {
        if (!value || typeof value !== "object" || Array.isArray(value)
                || Object.keys(value).length < 1) {
            return false;
        }
        for (const key of Object.keys(value)) {
            const definition = root.definitionByKey(kind, section, key);
            if (!definition || !root.validateField(definition, value[key]))
                return false;
        }
        return true;
    }

    function validateRuleRecord(record, kind) {
        if (!root.objectHasExactKeys(
                record, ["id", "name", "enabled", "match", "effects"], [])) {
            return false;
        }
        return root.isStableId(record.id)
            && root.isSchemaString(record.name, 256, false)
            && typeof record.enabled === "boolean"
            && root.validateFieldMap(record.match, kind, "match")
            && root.validateFieldMap(record.effects, kind, "effects");
    }

    function validateRuleCollection(rules, kind) {
        if (!Array.isArray(rules) || rules.length > 4096)
            return false;
        const ids = new Set();
        const names = new Set();
        for (const record of rules) {
            if (!root.validateRuleRecord(record, kind)
                    || ids.has(record.id) || names.has(record.name)) {
                return false;
            }
            ids.add(record.id);
            names.add(record.name);
        }
        return true;
    }

    function rulesForKind(kind) {
        return kind === "window"
            ? root.draftWindowRules : root.draftLayerRules;
    }

    function assignRules(kind, rules) {
        if (kind === "window")
            root.draftWindowRules = rules;
        else
            root.draftLayerRules = rules;
    }

    function ruleIndex(kind, id) {
        const rules = root.rulesForKind(kind);
        for (let index = 0; index < rules.length; ++index) {
            if (rules[index] && rules[index].id === id)
                return index;
        }
        return -1;
    }

    function ruleById(kind, id) {
        const index = root.ruleIndex(kind, id);
        return index >= 0 ? root.rulesForKind(kind)[index] : null;
    }

    function replaceRule(kind, id, record) {
        if (!root.controlsEnabled)
            return;
        const rules = root.clone(root.rulesForKind(kind));
        const index = root.ruleIndex(kind, id);
        if (!rules || index < 0)
            return;
        rules[index] = record;
        root.assignRules(kind, rules);
    }

    function setRuleProperty(kind, id, propertyName, value) {
        const record = root.clone(root.ruleById(kind, id));
        if (!record)
            return;
        record[propertyName] = value;
        root.replaceRule(kind, id, record);
    }

    function setRuleField(kind, id, section, key, included, value) {
        const record = root.clone(root.ruleById(kind, id));
        if (!record || (section !== "match" && section !== "effects"))
            return;
        if (included)
            record[section][key] = root.clone(value);
        else
            delete record[section][key];
        root.replaceRule(kind, id, record);
    }

    function fieldIncluded(kind, section, key) {
        const record = root.editingRule;
        return record && record[section]
            && Object.prototype.hasOwnProperty.call(record[section], key);
    }

    function fieldValue(section, key) {
        const record = root.editingRule;
        return record && record[section] ? record[section][key] : undefined;
    }

    function fieldValid(kind, section, definition) {
        if (!root.fieldIncluded(kind, section, definition.key))
            return true;
        return root.validateField(
            definition, root.fieldValue(section, definition.key)
        );
    }

    function nextIdentity(kind) {
        const prefix = kind === "window" ? "window-rule-" : "layer-rule-";
        const rules = root.rulesForKind(kind);
        const ids = new Set(rules.map(item => item.id));
        let suffix = 1;
        while (ids.has(prefix + suffix))
            ++suffix;
        return prefix + suffix;
    }

    function nextName(kind) {
        const base = kind === "window"
            ? qsTr("New window rule") : qsTr("New layer rule");
        const names = new Set(root.rulesForKind(kind).map(item => item.name));
        if (!names.has(base))
            return base;
        let suffix = 2;
        while (names.has(base + " " + suffix))
            ++suffix;
        return base + " " + suffix;
    }

    function addRule(kind) {
        if (!root.controlsEnabled || root.rulesForKind(kind).length >= 4096)
            return;
        const rules = root.clone(root.rulesForKind(kind));
        const id = root.nextIdentity(kind);
        rules.push({
            id,
            name: root.nextName(kind),
            enabled: false,
            match: {},
            effects: {}
        });
        root.assignRules(kind, rules);
        root.editingKind = kind;
        root.editingRuleId = id;
    }

    function removeRule(kind, id) {
        if (!root.controlsEnabled)
            return;
        const rules = root.clone(root.rulesForKind(kind));
        const index = root.ruleIndex(kind, id);
        if (!rules || index < 0)
            return;
        rules.splice(index, 1);
        root.assignRules(kind, rules);
        if (root.editingKind === kind && root.editingRuleId === id) {
            root.editingKind = "";
            root.editingRuleId = "";
        }
    }

    function moveRule(kind, id, offset) {
        if (!root.controlsEnabled || (offset !== -1 && offset !== 1))
            return;
        const rules = root.clone(root.rulesForKind(kind));
        const index = root.ruleIndex(kind, id);
        const target = index + offset;
        if (!rules || index < 0 || target < 0 || target >= rules.length)
            return;
        const record = rules[index];
        rules[index] = rules[target];
        rules[target] = record;
        root.assignRules(kind, rules);
    }

    function openRule(kind, id) {
        if (root.ruleIndex(kind, id) < 0)
            return;
        root.editingKind = kind;
        root.editingRuleId = id;
    }

    function closeEditor() {
        root.editingKind = "";
        root.editingRuleId = "";
    }

    function currentRuleIssue() {
        const record = root.editingRule;
        if (!record)
            return "";
        if (!root.isSchemaString(record.name, 256, false))
            return qsTr("Give this rule a nonempty name without control characters.");
        const rules = root.rulesForKind(root.editingKind);
        if (rules.some(item => item.id !== record.id
                && item.name === record.name)) {
            return qsTr("Rule names must be unique within this tab.");
        }
        if (!record.match || Object.keys(record.match).length < 1)
            return qsTr("Add at least one matcher before saving.");
        if (!root.validateFieldMap(
                record.match, root.editingKind, "match")) {
            return qsTr("Finish every included matcher before saving.");
        }
        if (!record.effects || Object.keys(record.effects).length < 1)
            return qsTr("Add at least one effect before saving.");
        if (!root.validateFieldMap(
                record.effects, root.editingKind, "effects")) {
            return qsTr("Finish every included effect before saving.");
        }
        return "";
    }

    function resetDraftToDefaults() {
        if (!root.controlsEnabled)
            return;
        root.draftWindowRules = [];
        root.draftLayerRules = [];
        root.closeEditor();
    }

    function synchronizeDraft() {
        if (!root.serviceAvailable || !root.rulesProjectionAvailable
                || !root.revisionTokenValid || !root.trustedValuesValid
                || root.busy || root.sharedMutationBusy) {
            return;
        }
        const windows = root.clone(root.windowRules);
        const layers = root.clone(root.layerRules);
        if (!windows || !layers)
            return;
        root.synchronizedWindowRules = root.clone(windows);
        root.synchronizedLayerRules = root.clone(layers);
        root.draftWindowRules = windows;
        root.draftLayerRules = layers;
        root.synchronizedRevisionToken = root.revisionToken;
        root.projectionInitialized = true;
        root.externalChangeWhileEditing = false;
        root.saveSubmitted = false;
        root.submittedWindowRules = [];
        root.submittedLayerRules = [];
        root.submittedRevisionToken = "";
        root.closeEditor();
    }

    function submitDraft() {
        if (!root.saveEnabled)
            return;
        const windows = root.clone(root.draftWindowRules);
        const layers = root.clone(root.draftLayerRules);
        if (!windows || !layers)
            return;
        root.saveSubmitted = true;
        root.submittedWindowRules = root.clone(windows);
        root.submittedLayerRules = root.clone(layers);
        root.submittedRevisionToken = root.revisionToken;
        root.saveRequested(windows, layers);
        root.scheduleProjectionReview();
    }

    function reviewProjection() {
        if (!root.serviceAvailable || !root.trustedValuesValid) {
            if (root.serviceAvailable && !root.saveSubmitted
                    && root.projectionInitialized && root.draftDirty
                    && root.revisionTokenValid
                    && root.synchronizedRevisionTokenValid
                    && root.revisionToken
                        !== root.synchronizedRevisionToken) {
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
            if (root.valueEqual(root.windowRules, root.submittedWindowRules)
                    && root.valueEqual(
                        root.layerRules, root.submittedLayerRules)) {
                root.synchronizeDraft();
                return;
            }
            if (root.revisionToken === root.submittedRevisionToken) {
                root.saveSubmitted = false;
                root.submittedWindowRules = [];
                root.submittedLayerRules = [];
                root.submittedRevisionToken = "";
                return;
            }
            root.saveSubmitted = false;
            root.externalChangeWhileEditing = true;
            return;
        }
        const projectionChanged = root.revisionToken
                !== root.synchronizedRevisionToken
            || !root.valueEqual(root.windowRules, root.synchronizedWindowRules)
            || !root.valueEqual(root.layerRules, root.synchronizedLayerRules);
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

    onWindowRulesChanged: root.scheduleProjectionReview()
    onLayerRulesChanged: root.scheduleProjectionReview()
    onRulesProjectionAvailableChanged: root.scheduleProjectionReview()
    onRevisionTokenChanged: root.scheduleProjectionReview()
    onServiceAvailableChanged: root.scheduleProjectionReview()
    onBusyChanged: {
        root.scheduleProjectionReview();
        if (rulesRecoveryDialog.opened && root.busy)
            rulesRecoveryDialog.close();
    }
    onSharedMutationBusyChanged: root.scheduleProjectionReview()
    onRulesErrorNameChanged: root.scheduleProjectionReview()
    onRulesErrorMessageChanged: root.scheduleProjectionReview()
    onSharedErrorNameChanged: root.scheduleProjectionReview()
    onSharedErrorMessageChanged: root.scheduleProjectionReview()
    onApplyStateChanged: root.scheduleProjectionReview()
    onRecoveryAvailableChanged: {
        if (rulesRecoveryDialog.opened && !root.recoveryAvailable)
            rulesRecoveryDialog.close();
    }
    Component.onCompleted: root.scheduleProjectionReview()

    background: Rectangle { color: root.palette.window }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compactPage ? 16 : 24
        anchors.rightMargin: root.compactPage ? 16 : 24
        anchors.topMargin: root.compactPage
            ? Math.min(root.contentTopMargin, 12) : root.contentTopMargin
        anchors.bottomMargin: root.compactPage ? 12 : 20
        spacing: root.compactPage ? 10 : 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Rules")
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
                    text: qsTr("Create ordered, typed Window and Layer Rules without exposing raw configuration or commands.")
                    color: root.palette.placeholderText
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            Button {
                objectName: "refreshRulesButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Refresh")
                enabled: !root.busy && !root.displayTestActive
                icon.name: "view-refresh-symbolic"
                Accessible.name: qsTr("Refresh compositor rules")

                onClicked: root.refreshRequested()
            }
        }

        Frame {
            objectName: "rulesStatusCard"
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
                    objectName: "rulesStatusMessage"
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
                        objectName: "rulesOpenDisplaysButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        visible: root.serviceAvailable
                            && root.managementState === "unmanaged"
                        text: qsTr("Review takeover in Displays")
                        enabled: !root.busy
                        Accessible.name: qsTr("Open Displays to review compositor takeover")

                        onClicked: root.openDisplaysRequested()
                    }

                    Button {
                        objectName: "loadCurrentRulesButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        visible: root.externalChangeWhileEditing
                        text: qsTr("Load current settings")
                        enabled: !root.busy && !root.sharedMutationBusy
                            && !root.saveSubmitted
                            && root.rulesProjectionAvailable
                            && root.trustedValuesValid
                        Accessible.name: qsTr("Discard the complete Rules draft and load the current compositor settings")

                        onClicked: root.synchronizeDraft()
                    }

                    Button {
                        objectName: "retryApplyRulesButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        visible: root.retryApplyAvailable
                        text: root.busyOperation === "compositor-apply"
                                || root.busyOperation === "rules-apply"
                            ? qsTr("Retrying apply…") : qsTr("Retry apply")
                        enabled: root.retryApplyAvailable && !root.busy
                            && !root.sharedMutationBusy
                            && root.sharedApplySafe
                        Accessible.name: qsTr("Retry applying the exact saved compositor revision")

                        onClicked: root.retryApplyRequested()
                    }

                    Button {
                        objectName: "recoverRulesButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        visible: root.recoveryAvailable
                        text: qsTr("Restore last working configuration")
                        enabled: root.recoveryAvailable && !root.busy
                            && !root.sharedMutationBusy
                        Accessible.name: qsTr("Review whole-compositor recovery")

                        onClicked: rulesRecoveryDialog.open()
                    }
                }
            }
        }

        TabBar {
            id: rulesTabs

            objectName: "rulesTabs"
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? root.minimumTargetSize : 0
            visible: !root.editorActive
            currentIndex: root.rulesTabIndex

            onCurrentIndexChanged: root.rulesTabIndex = currentIndex

            TabButton {
                objectName: "windowRulesTab"
                implicitHeight: root.minimumTargetSize
                text: qsTr("Window Rules")
                Accessible.name: text
            }

            TabButton {
                objectName: "layerRulesTab"
                implicitHeight: root.minimumTargetSize
                text: qsTr("Layer Rules")
                Accessible.name: text
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 100
            currentIndex: root.editorActive ? 2 : root.rulesTabIndex

            RuleSummaryList {
                objectName: "windowRulesList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                ruleKind: "window"
                rules: root.draftWindowRules
                controlsEnabled: root.controlsEnabled
                discardEnabled: root.discardEnabled
                draftDirty: root.draftDirty
                draftValid: root.draftValid
                saveEnabled: root.saveEnabled
                resetEnabled: root.resetEnabled
                busy: root.busy
                busyOperation: root.busyOperation
                emptyText: qsTr("No Window Rules are saved. Add a disabled draft rule, then choose at least one matcher and effect before saving.")
                minimumTargetSize: root.minimumTargetSize

                onAddRequested: root.addRule("window")
                onEditRequested: id => root.openRule("window", id)
                onEnabledRequested: (id, enabled) =>
                    root.setRuleProperty("window", id, "enabled", enabled)
                onMoveRequested: (id, offset) =>
                    root.moveRule("window", id, offset)
                onRemoveRequested: id => root.removeRule("window", id)
                onDiscardRequested: root.synchronizeDraft()
                onResetRequested: root.resetDraftToDefaults()
                onSaveRequested: root.submitDraft()
            }

            RuleSummaryList {
                objectName: "layerRulesList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                ruleKind: "layer"
                rules: root.draftLayerRules
                controlsEnabled: root.controlsEnabled
                discardEnabled: root.discardEnabled
                draftDirty: root.draftDirty
                draftValid: root.draftValid
                saveEnabled: root.saveEnabled
                resetEnabled: root.resetEnabled
                busy: root.busy
                busyOperation: root.busyOperation
                emptyText: qsTr("No Layer Rules are saved. Add a disabled draft rule, then choose its namespace matcher and at least one effect before saving.")
                minimumTargetSize: root.minimumTargetSize

                onAddRequested: root.addRule("layer")
                onEditRequested: id => root.openRule("layer", id)
                onEnabledRequested: (id, enabled) =>
                    root.setRuleProperty("layer", id, "enabled", enabled)
                onMoveRequested: (id, offset) =>
                    root.moveRule("layer", id, offset)
                onRemoveRequested: id => root.removeRule("layer", id)
                onDiscardRequested: root.synchronizeDraft()
                onResetRequested: root.resetDraftToDefaults()
                onSaveRequested: root.submitDraft()
            }

            ScrollView {
                id: rulesEditorScrollView

                objectName: "rulesEditorScrollView"
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    objectName: "rulesEditorContent"
                    width: rulesEditorScrollView.availableWidth
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Button {
                            objectName: "closeRuleEditorButton"
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight
                                    + topPadding + bottomPadding
                            )
                            text: qsTr("Back to rules")
                            Accessible.name: qsTr("Close the selected rule editor")

                            onClicked: root.closeEditor()
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            horizontalAlignment: Text.AlignRight
                            text: root.editingKind === "window"
                                ? qsTr("Editing Window Rule")
                                : qsTr("Editing Layer Rule")
                            color: root.palette.placeholderText
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }
                    }

                    Frame {
                        objectName: "ruleIdentityCard"
                        Layout.fillWidth: true
                        padding: root.compactPage ? 14 : 18

                        background: Rectangle {
                            color: root.palette.base
                            radius: 16
                            border.color: root.palette.mid
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Rule identity")
                                color: root.palette.text
                                font.pixelSize: 18
                                font.weight: Font.DemiBold
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("The stable internal ID is preserved automatically when this rule is renamed or reordered.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            TextField {
                                objectName: root.editingKind === "window"
                                    ? "windowRuleNameField"
                                    : "layerRuleNameField"
                                Layout.fillWidth: true
                                implicitHeight: root.minimumTargetSize
                                maximumLength: 256
                                text: root.editingRule
                                    && typeof root.editingRule.name === "string"
                                    ? root.editingRule.name : ""
                                placeholderText: qsTr("Unique rule name")
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Rule name")

                                onTextEdited: root.setRuleProperty(
                                    root.editingKind,
                                    root.editingRuleId,
                                    "name",
                                    text
                                )
                            }

                            SettingsToggleRow {
                                title: qsTr("Rule enabled")
                                description: qsTr("Disabled rules stay saved and ordered but are not emitted into the active compositor configuration.")
                                checked: root.editingRule
                                    && root.editingRule.enabled === true
                                enabled: root.controlsEnabled
                                controlObjectName: root.editingKind === "window"
                                    ? "windowRuleEnabledEditor"
                                    : "layerRuleEnabledEditor"
                                accessibleName: qsTr("Enable this rule")
                                minimumTargetSize: root.minimumTargetSize

                                onValueModified: value =>
                                    root.setRuleProperty(
                                        root.editingKind,
                                        root.editingRuleId,
                                        "enabled",
                                        value
                                    )
                            }

                            Label {
                                objectName: "ruleEditorValidationMessage"
                                Layout.fillWidth: true
                                visible: root.editorIssue.length > 0
                                text: root.editorIssue
                                color: "#ffb8c3"
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                Accessible.role: Accessible.AlertMessage
                                Accessible.name: text
                            }
                        }
                    }

                    Repeater {
                        model: root.editorGroups

                        Frame {
                            id: ruleFieldGroup

                            required property var modelData

                            objectName: root.editingKind
                                + "Rule" + modelData.group.charAt(0).toUpperCase()
                                + modelData.group.slice(1)
                                + (modelData.section === "match"
                                    ? "MatchersCard" : "EffectsCard")
                            Layout.fillWidth: true
                            padding: root.compactPage ? 14 : 18

                            background: Rectangle {
                                color: root.palette.base
                                radius: 16
                                border.color: root.palette.mid
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 12

                                Label {
                                    Layout.fillWidth: true
                                    text: ruleFieldGroup.modelData.title
                                    color: root.palette.text
                                    font.pixelSize: 18
                                    font.weight: Font.DemiBold
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                    Accessible.role: Accessible.Heading
                                    Accessible.name: text
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: ruleFieldGroup.modelData.description
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                }

                                Repeater {
                                    model: root.definitionsForGroup(
                                        root.definitions(
                                            root.editingKind,
                                            ruleFieldGroup.modelData.section
                                        ),
                                        ruleFieldGroup.modelData.group
                                    )

                                    RuleOptionalField {
                                        id: optionalRuleField

                                        required property var modelData

                                        definition: modelData
                                        included: root.fieldIncluded(
                                            root.editingKind,
                                            ruleFieldGroup.modelData.section,
                                            modelData.key
                                        )
                                        value: root.fieldValue(
                                            ruleFieldGroup.modelData.section,
                                            modelData.key
                                        )
                                        fieldValid: root.fieldValid(
                                            root.editingKind,
                                            ruleFieldGroup.modelData.section,
                                            modelData
                                        )
                                        enabled: root.controlsEnabled
                                        minimumTargetSize:
                                            root.minimumTargetSize

                                        onIncludeModified: included =>
                                            root.setRuleField(
                                                root.editingKind,
                                                root.editingRuleId,
                                                ruleFieldGroup.modelData.section,
                                                modelData.key,
                                                included,
                                                included
                                                    ? modelData.defaultValue
                                                    : undefined
                                            )
                                        onValueModified: value =>
                                            root.setRuleField(
                                                root.editingKind,
                                                root.editingRuleId,
                                                ruleFieldGroup.modelData.section,
                                                modelData.key,
                                                true,
                                                value
                                            )
                                    }
                                }
                            }
                        }
                    }

                    Frame {
                        objectName: "ruleEditorActionsCard"
                        Layout.fillWidth: true
                        padding: root.compactPage ? 14 : 18

                        background: Rectangle {
                            color: root.palette.base
                            radius: 16
                            border.color: root.editorIssue.length === 0
                                ? root.palette.highlight : root.palette.mid
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Window and Layer tabs share one draft")
                                color: root.palette.text
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Return to either rule list to save both ordered collections atomically. Invalid RE2 syntax is checked by the trusted full-state parser before any desired-state revision is replaced.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }

                            Flow {
                                Layout.fillWidth: true
                                Layout.preferredHeight: childrenRect.height
                                spacing: 10

                                Button {
                                    objectName: "doneEditingRuleButton"
                                    implicitHeight: Math.max(
                                        root.minimumTargetSize,
                                        implicitBackgroundHeight,
                                        implicitContentHeight
                                            + topPadding + bottomPadding
                                    )
                                    text: qsTr("Done")
                                    Accessible.name: qsTr("Return to the rule list")

                                    onClicked: root.closeEditor()
                                }

                                Button {
                                    objectName: "removeEditedRuleButton"
                                    implicitHeight: Math.max(
                                        root.minimumTargetSize,
                                        implicitBackgroundHeight,
                                        implicitContentHeight
                                            + topPadding + bottomPadding
                                    )
                                    text: qsTr("Remove from draft")
                                    enabled: root.controlsEnabled
                                    Accessible.name: qsTr("Remove this rule from the combined draft")

                                    onClicked: root.removeRule(
                                        root.editingKind,
                                        root.editingRuleId
                                    )
                                }
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 8 }
                }
            }
        }
    }

    CompositorRecoveryDialog {
        id: rulesRecoveryDialog

        objectName: "rulesRecoveryDialog"
        recoveryAvailable: root.recoveryAvailable
        operationBusy: root.busy || root.sharedMutationBusy
        busyOperation: root.busyOperation
        settingsAreaName: qsTr("Rules")
        warningObjectName: "rulesRecoveryWarning"
        cancelObjectName: "cancelRulesRecoveryButton"
        confirmObjectName: "confirmRulesRecoveryButton"
        minimumTargetSize: root.minimumTargetSize

        onRecoveryRequested: root.recoveryRequested()
    }
}
