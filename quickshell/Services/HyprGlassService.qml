pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import qs.Common
import qs.Services

Singleton {
    id: root
    readonly property var log: Log.scoped("HyprGlassService")

    readonly property bool disabledByEnv: Quickshell.env("HGS_HYPRGLASS_DISABLE") === "1"
    readonly property bool qmlFallbackIsolationEnabled: Quickshell.env("HGS_HYPRGLASS_ISOLATE_QML_FALLBACK") === "1"
    readonly property bool available: pluginLoaded && !disabledByEnv

    property bool probeComplete: false
    property bool pluginLoaded: false
    property int pluginGeneration: 0
    property int applyCount: 0
    property int descriptorCount: 0
    property string lastError: ""
    property var lastStatus: ({})
    property double lastStatusTimestamp: 0
    property bool statusFresh: false
    property int statusRevision: 0

    property var descriptors: ({})
    // Fluid Glass Labs preview element (window-anchored); null when the Labs is closed.
    property var labsPreview: null
    onLabsPreviewChanged: scheduleApply()
    property int generation: 0
    property int sequence: 0
    property int appearanceRevision: 0
    property bool _applyPendingAfterProbe: false
    property bool _statusErrorLogged: false
    property bool _materialModeErrorLogged: false
    property double lastResyncAttemptTimestamp: 0
    property double lastMaterialModeAttemptTimestamp: 0
    property string lastResyncReason: ""
    property bool _shutdownCleanupIssued: false

    function clamp(value, min, max) {
        const n = Number(value);
        if (!isFinite(n))
            return min;
        return Math.max(min, Math.min(max, n));
    }

    function roundCoord(value) {
        return Math.round(clamp(value, -1000000, 1000000) * 1000) / 1000;
    }

    function envBool(name, fallback) {
        const value = (Quickshell.env(name) || "").toLowerCase();
        if (value === "1" || value === "true" || value === "yes" || value === "on")
            return true;
        if (value === "0" || value === "false" || value === "no" || value === "off")
            return false;
        return fallback;
    }

    function envNumber(name, fallback) {
        const raw = Quickshell.env(name);
        if (!raw)
            return fallback;
        const n = Number(raw);
        return isFinite(n) ? n : fallback;
    }

    function envPresent(name) {
        const raw = Quickshell.env(name);
        return raw !== undefined && raw !== null && raw !== "";
    }

    function normalizeColorSource(value) {
        const source = String(value || "auto").toLowerCase();
        switch (source) {
        case "auto":
        case "custom":
        case "preset":
        case "matugen":
        case "theme":
        case "wallpaper":
            return source;
        default:
            return "auto";
        }
    }

    function descriptorTintModeForSource(source) {
        switch (normalizeColorSource(source)) {
        case "matugen":
        case "theme":
            return "theme";
        case "wallpaper":
            return "wallpaper";
        case "custom":
        case "preset":
            return "manual";
        case "auto":
        default:
            return "neutral";
        }
    }

    function normalizeMaterialMode(value) {
        const mode = String(value || "").toLowerCase();
        switch (mode) {
        case "off":
        case "flat":
        case "blur-native":
        case "glass-v1":
        case "fluid-glass":
            return mode;
        default:
            return "";
        }
    }

    function normalizeHexColor(value, fallback) {
        const color = String(value || "");
        return /^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$/.test(color) ? color : fallback;
    }

    function frostResponse(value, low, anchor, high) {
        const f = clamp(value, 0, 1);
        const anchorFrost = defaultFrostAmount;
        if (f <= anchorFrost) {
            const t = anchorFrost <= 0 ? 0 : f / anchorFrost;
            return low + (anchor - low) * t;
        }
        const t = (f - anchorFrost) / (1 - anchorFrost);
        return anchor + (high - anchor) * t;
    }

    function materialOpacityForFrost(value) {
        return frostResponse(value, 0.12, defaultMaterialOpacity, 0.46);
    }

    function neutralTintOpacityForFrost(tone, value) {
        if (tone === "light")
            return frostResponse(value, 0.018, neutralLightTintOpacity, 0.12);
        return frostResponse(value, 0.025, neutralDarkTintOpacity, 0.15);
    }

    function colorTintOpacityForFrost(value) {
        return frostResponse(value, 0.055, colorTintOpacity, 0.24);
    }

    // Temporary dev overrides only. Normal resolution uses SettingsData.
    readonly property bool colorGlassEnabledEnvSet: envPresent("HGS_HYPRGLASS_COLOR_ENABLED")
    readonly property bool colorSourceEnvSet: envPresent("HGS_HYPRGLASS_COLOR_SOURCE")
    readonly property bool colorTintEnvSet: envPresent("HGS_HYPRGLASS_COLOR")
    readonly property bool frostAmountEnvSet: envPresent("HGS_HYPRGLASS_FROST")
    readonly property real defaultFrostAmount: 0.08
    property bool colorGlassEnabled: colorGlassEnabledEnvSet ? envBool("HGS_HYPRGLASS_COLOR_ENABLED", false) : (SettingsData.hyprGlassColorGlassEnabled ?? false)
    property string colorSource: normalizeColorSource(colorSourceEnvSet ? Quickshell.env("HGS_HYPRGLASS_COLOR_SOURCE") : (SettingsData.hyprGlassColorSource ?? "custom"))
    property string colorTint: normalizeHexColor(colorTintEnvSet ? Quickshell.env("HGS_HYPRGLASS_COLOR") : (SettingsData.hyprGlassCustomColor ?? "#88AADD"), "#88AADD")
    property real frostAmount: clamp(frostAmountEnvSet ? envNumber("HGS_HYPRGLASS_FROST", defaultFrostAmount) : (SettingsData.hyprGlassFrostAmount ?? defaultFrostAmount), 0, 1)

    readonly property string neutralLightTintColor: "#F8FBFF"
    readonly property string neutralDarkTintColor: "#242A33"
    readonly property real neutralLightTintOpacity: 0.045
    readonly property real neutralDarkTintOpacity: 0.075
    readonly property real colorTintOpacity: 0.14
    readonly property real defaultMaterialOpacity: 0.24
    readonly property real materialSaturation: 1.16
    readonly property real materialContrastBias: 0.04
    readonly property int statusPollIntervalMs: 2000
    readonly property int statusFreshnessMs: 5000
    readonly property int resyncCooldownMs: 2000
    readonly property int materialModeReconcileCooldownMs: 2000
    readonly property string desiredMaterialMode: {
        var envMode = normalizeMaterialMode(Quickshell.env("HGS_HYPRGLASS_MATERIAL_MODE"));
        if (envMode !== "")
            return envMode;
        return (typeof SettingsData !== "undefined" && SettingsData.fluidGlassEnabled) ? "fluid-glass" : "";
    }
    readonly property bool desiredMaterialModeEnabled: desiredMaterialMode !== ""
    readonly property var resolvedMaterial: resolveDescriptorMaterial({})

    function themeTone() {
        const light = (typeof Theme !== "undefined" && Theme.isLightMode) || (typeof SessionData !== "undefined" && SessionData.isLightMode);
        return light ? "light" : "dark";
    }

    // Neutral glass tint that follows the system light/dark appearance.
    function neutralGlassTint() {
        return themeTone() === "light" ? neutralLightTintColor : neutralDarkTintColor;
    }

    function _glassHexOf(c) {
        function h(x) {
            return Math.max(0, Math.min(255, Math.round(x * 255))).toString(16).padStart(2, "0");
        }
        return "#" + h(c.r) + h(c.g) + h(c.b);
    }

    // Stained-Glass tint color from the chosen source:
    //   "theme"  -> the active Theme color (matugen / manual seed)
    //   "system" -> a neutral that follows light/dark appearance
    function resolveGlassTintColor() {
        const source = String((typeof SettingsData !== "undefined" ? SettingsData.fluidGlassTintSource : "system") || "system").toLowerCase();
        if (source === "theme" && typeof Theme !== "undefined" && Theme.primary)
            return normalizeHexColor(_glassHexOf(Theme.primary), neutralGlassTint());
        return neutralGlassTint();
    }

    // Advanced Fluid Glass material params (design-px reference; plugin scales).
    // Defaults mirror the plugin's locked GlassElement calibration, so sending
    // them is a no-op until the user tunes them in the Labs.
    function resolveGlassAdvanced() {
        const S = (typeof SettingsData !== "undefined") ? SettingsData : null;
        return {
            refraction: S ? (S.fluidGlassRefraction ?? 45) : 45,
            rimBand:    S ? (S.fluidGlassRimBand ?? 40) : 40,
            bevel:      S ? (S.fluidGlassBevel ?? 46) : 46,
            rimWidth:   S ? (S.fluidGlassRimWidth ?? 3) : 3,
            highlight:  S ? (S.fluidGlassHighlight ?? 0.10) : 0.10,
            shadow:     S ? (S.fluidGlassShadow ?? 0.10) : 0.10,
            lightAngle: S ? (S.fluidGlassLightAngle ?? 90) : 90,
            specular:   S ? (S.fluidGlassSpecular ?? 0.21) : 0.21,
            rimWrap:    S ? (S.fluidGlassRimWrap ?? 0.45) : 0.45
        };
    }

    function markAppearanceChanged() {
        appearanceRevision++;
    }

    function updateStatusFreshness() {
        const fresh = lastStatusTimestamp > 0 && (Date.now() - lastStatusTimestamp) <= statusFreshnessMs;
        if (statusFresh !== fresh) {
            statusFresh = fresh;
            statusRevision++;
        }
    }

    function _descriptorStatus(descriptorId) {
        const _revision = statusRevision;
        void _revision;
        const list = lastStatus?.descriptors ?? [];
        for (let i = 0; i < list.length; i++) {
            if (list[i]?.id === descriptorId)
                return list[i];
        }
        return null;
    }

    function compositorMaterialReadiness(descriptorId) {
        const _revision = statusRevision;
        void _revision;

        if (!descriptorId)
            return { ready: false, reason: "missing descriptor id" };
        if (disabledByEnv)
            return { ready: false, reason: "hyprglass disabled" };
        if (!statusFresh)
            return { ready: false, reason: "plugin status stale" };
        if (!pluginLoaded || lastStatus?.pluginLoaded !== true)
            return { ready: false, reason: "plugin not loaded" };

        const material = lastStatus?.material ?? {};
        const mode = material.mode ?? "off";
        if (material.enabled !== true || (mode !== "flat" && mode !== "blur-native" && mode !== "glass-v1" && mode !== "fluid-glass"))
            return { ready: false, reason: "compositor material disabled" };
        if (material.lastRenderStatus !== "ok")
            return { ready: false, reason: `material render status ${material.lastRenderStatus ?? "unknown"}` };

        const descriptor = _descriptorStatus(descriptorId);
        if (!descriptor)
            return { ready: false, reason: "descriptor missing from plugin status" };

        const surface = descriptor.surfaceMatch ?? {};
        if (surface.status !== "matched" || surface.matched !== true)
            return { ready: false, reason: `surface ${surface.status ?? "unknown"}` };

        const coordinate = descriptor.coordinate ?? {};
        if (coordinate.status !== "aligned" && coordinate.status !== "near")
            return { ready: false, reason: `coordinate ${coordinate.status ?? "unknown"}` };

        const compositorMaterial = descriptor.compositorMaterial ?? {};
        if (compositorMaterial.drawable !== true || compositorMaterial.status !== "drawable")
            return { ready: false, reason: compositorMaterial.reason ?? "compositor material not drawable" };
        if (compositorMaterial.mode !== mode)
            return { ready: false, reason: "descriptor material mode mismatch" };
        const backendUsed = compositorMaterial.backendUsed ?? "";
        const fluidGlassFallback = mode === "fluid-glass" && backendUsed === "glass-v1-fallback";
        if (mode === "fluid-glass" && !fluidGlassFallback && (compositorMaterial.captureReady !== true || compositorMaterial.textureReady !== true))
            return { ready: false, reason: `backdrop capture ${compositorMaterial.captureStatus ?? "not ready"}` };
        if (mode === "fluid-glass" && !fluidGlassFallback && compositorMaterial.shaderReady !== true)
            return { ready: false, reason: `sdf shader ${compositorMaterial.shaderError ?? "not ready"}` };

        return { ready: true, reason: "compositor material drawable" };
    }

    function shouldUseCompositorMaterial(descriptorId) {
        return compositorMaterialReadiness(descriptorId).ready;
    }

    function _hasLocalDescriptors() {
        return Object.keys(descriptors || {}).length > 0;
    }

    function _pluginMissingCurrentDescriptors(status) {
        const localIds = Object.keys(descriptors || {});
        if (localIds.length === 0)
            return false;

        const pluginDescriptors = status?.descriptors ?? [];
        if (pluginDescriptors.length === 0)
            return true;

        const pluginIds = {};
        for (let i = 0; i < pluginDescriptors.length; i++) {
            const id = pluginDescriptors[i]?.id;
            if (id)
                pluginIds[id] = true;
        }

        for (let i = 0; i < localIds.length; i++) {
            if (pluginIds[localIds[i]] !== true)
                return true;
        }
        return false;
    }

    function requestDescriptorResync(reason) {
        if (disabledByEnv || !pluginLoaded || !_hasLocalDescriptors())
            return;

        const now = Date.now();
        if (now - lastResyncAttemptTimestamp < resyncCooldownMs)
            return;

        lastResyncAttemptTimestamp = now;
        lastResyncReason = reason;
        scheduleApply();
    }

    function cleanupDescriptorsOnShutdown() {
        if (disabledByEnv || _shutdownCleanupIssued)
            return;

        _shutdownCleanupIssued = true;

        if (!_hasLocalDescriptors() && !pluginLoaded && !statusFresh)
            return;

        try {
            Quickshell.execDetached(["hgs", "hyprglass", "clear"]);
        } catch (e) {
            log.warn(`failed to launch hgs-hyprglass shutdown cleanup: ${e}`);
        }
    }

    function reconcileMaterialMode(status, reason) {
        if (!desiredMaterialModeEnabled || disabledByEnv || !pluginLoaded)
            return;

        const currentMode = status?.material?.mode ?? "off";
        if (currentMode === desiredMaterialMode)
            return;

        const now = Date.now();
        if (now - lastMaterialModeAttemptTimestamp < materialModeReconcileCooldownMs)
            return;

        lastMaterialModeAttemptTimestamp = now;
        Proc.runCommand("hyprglass-material-mode", ["hgs", "hyprglass", "material", desiredMaterialMode], (_output, exitCode) => {
            if (exitCode !== 0) {
                if (!_materialModeErrorLogged) {
                    _materialModeErrorLogged = true;
                    log.warn(`failed to reconcile hgs-hyprglass material mode: ${reason}`);
                }
                return;
            }
            _materialModeErrorLogged = false;
            refreshStatus(false);
        }, 0, 2500);
    }

    function resolveDescriptorMaterial(options) {
        const colorEnabled = options?.colorGlassEnabled ?? colorGlassEnabled;
        const source = normalizeColorSource(options?.colorSource ?? colorSource);
        const activeColorSource = source === "auto" && colorEnabled ? "custom" : source;
        const tone = themeTone();
        const resolvedFrost = clamp(options?.frost ?? frostAmount, 0, 1);
        const neutralTintColor = tone === "light" ? neutralLightTintColor : neutralDarkTintColor;
        const neutralTintOpacity = neutralTintOpacityForFrost(tone, resolvedFrost);
        const resolvedTintOpacity = colorEnabled ? colorTintOpacityForFrost(resolvedFrost) : neutralTintOpacity;
        const resolvedColor = colorEnabled ? normalizeHexColor(options?.tintColor ?? colorTint, colorTint) : neutralTintColor;

        return {
            colorGlassEnabled: colorEnabled,
            colorSource: colorEnabled ? activeColorSource : "auto",
            themeTone: tone,
            preset: colorEnabled ? "tinted" : "clear",
            tintMode: colorEnabled ? descriptorTintModeForSource(activeColorSource) : "neutral",
            tintColor: resolvedColor,
            tintOpacity: clamp(options?.tintOpacity ?? resolvedTintOpacity, 0, 1),
            opacity: clamp(options?.opacity ?? materialOpacityForFrost(resolvedFrost), 0, 1),
            frost: resolvedFrost,
            saturation: clamp(options?.saturation ?? materialSaturation, 0, 2),
            contrastBias: clamp(options?.contrastBias ?? materialContrastBias, -1, 1)
        };
    }

    onColorGlassEnabledChanged: markAppearanceChanged()
    onColorSourceChanged: markAppearanceChanged()
    onColorTintChanged: markAppearanceChanged()
    onFrostAmountChanged: markAppearanceChanged()

    Connections {
        target: SessionData
        function onIsLightModeChanged() {
            root.markAppearanceChanged();
            root.scheduleApply();
        }
    }

    // Re-push the v2 compositor payload whenever a Fluid Glass setting (or the theme
    // color used by the "Theme Color" tint source) changes, so the live glass tracks
    // the settings UI without waiting for a bar-geometry change.
    Connections {
        target: SettingsData
        function onFluidGlassEnabledChanged() {
            root.scheduleApply();
        }
        function onFluidGlassLevelChanged() {
            root.scheduleApply();
        }
        function onFluidGlassDynamicLightChanged() {
            root.scheduleApply();
        }
        function onFluidGlassStainedChanged() {
            root.scheduleApply();
        }
        function onFluidGlassTintSourceChanged() {
            root.scheduleApply();
        }
        function onFluidGlassRefractionChanged() {
            root.scheduleApply();
        }
        function onFluidGlassRimBandChanged() {
            root.scheduleApply();
        }
        function onFluidGlassBevelChanged() {
            root.scheduleApply();
        }
        function onFluidGlassRimWidthChanged() {
            root.scheduleApply();
        }
        function onFluidGlassHighlightChanged() {
            root.scheduleApply();
        }
        function onFluidGlassShadowChanged() {
            root.scheduleApply();
        }
        function onFluidGlassLightAngleChanged() {
            root.scheduleApply();
        }
        function onFluidGlassSpecularChanged() {
            root.scheduleApply();
        }
        function onFluidGlassRimWrapChanged() {
            root.scheduleApply();
        }
        function onFluidGlassFrostedChanged() {
            root.scheduleApply();
        }
        function onFluidGlassBlurCustomChanged() {
            root.scheduleApply();
        }
        function onFluidGlassTintCustomChanged() {
            root.scheduleApply();
        }
    }

    Connections {
        target: Theme
        function onPrimaryChanged() {
            root.scheduleApply();
        }
    }

    Connections {
        target: Qt.application
        function onAboutToQuit() {
            root.cleanupDescriptorsOnShutdown();
        }
    }

    function upsertDescriptor(id, descriptor) {
        if (disabledByEnv || !id || !descriptor)
            return;

        descriptor.id = id;
        descriptor.version = 1;
        descriptor.sequence = ++sequence;

        const next = {};
        const keys = Object.keys(descriptors || {});
        for (let i = 0; i < keys.length; i++)
            next[keys[i]] = descriptors[keys[i]];
        next[id] = descriptor;
        descriptors = next;
        scheduleApply();
    }

    function removeDescriptor(id) {
        if (disabledByEnv || !id)
            return;
        if (!descriptors || descriptors[id] === undefined)
            return;

        const next = {};
        const keys = Object.keys(descriptors);
        for (let i = 0; i < keys.length; i++) {
            if (keys[i] !== id)
                next[keys[i]] = descriptors[keys[i]];
        }
        descriptors = next;
        scheduleApply();
    }

    function buildBarDescriptor(id, screenName, rect, radius, scale, options) {
        const safeRect = {
            x: roundCoord(rect?.x ?? 0),
            y: roundCoord(rect?.y ?? 0),
            width: Math.max(1, roundCoord(rect?.width ?? 1)),
            height: Math.max(1, roundCoord(rect?.height ?? 1))
        };
        const safeRadius = clamp(radius ?? 0, 0, Math.max(safeRect.width, safeRect.height));
        const resolvedMaterial = resolveDescriptorMaterial(options || {});
        const shapeType = safeRadius >= (Math.min(safeRect.width, safeRect.height) / 2 - 0.5) ? "capsule" : "rounded_rect";

        return {
            version: 1,
            id: id,
            kind: "bar",
            surface: {
                namespace: "hgs:bar",
                layer: options?.layer ?? "top",
                role: "bar",
                monitor: {
                    name: screenName ?? ""
                }
            },
            geometry: {
                logical: safeRect,
                scale: clamp(scale ?? 1, 0.25, 8)
            },
            shape: {
                type: shapeType,
                radius: {
                    topLeft: safeRadius,
                    topRight: safeRadius,
                    bottomRight: safeRadius,
                    bottomLeft: safeRadius
                }
            },
            material: {
                enabled: true,
                preset: resolvedMaterial.preset,
                opacity: resolvedMaterial.opacity,
                frost: resolvedMaterial.frost,
                saturation: resolvedMaterial.saturation,
                contrastBias: resolvedMaterial.contrastBias,
                tint: {
                    mode: resolvedMaterial.tintMode,
                    color: resolvedMaterial.tintColor,
                    opacity: resolvedMaterial.tintOpacity
                },
                refraction: {
                    strength: clamp(options?.refractionStrength ?? 0.55, 0, 4),
                    edgeWidth: clamp(options?.edgeWidth ?? Math.max(10, Math.min(28, safeRadius)), 0, 128),
                    displacement: clamp(options?.displacement ?? 18, 0, 128),
                    chromaticAberration: clamp(options?.chromaticAberration ?? 0.04, 0, 1)
                },
                rim: {
                    opacity: clamp(options?.rimOpacity ?? 0.42, 0, 1),
                    width: clamp(options?.rimWidth ?? 1.25, 0, 32)
                },
                highlight: {
                    opacity: clamp(options?.highlightOpacity ?? 0.28, 0, 1),
                    angle: options?.highlightAngle ?? 315,
                    spread: clamp(options?.highlightSpread ?? 76, 0, 360)
                },
                reflection: {
                    opacity: clamp(options?.reflectionOpacity ?? 0.18, 0, 1),
                    angle: options?.reflectionAngle ?? 22,
                    offset: clamp(options?.reflectionOffset ?? 10, -256, 256),
                    blur: clamp(options?.reflectionBlur ?? 16, 0, 128)
                },
                shadow: {
                    innerOpacity: clamp(options?.innerShadowOpacity ?? 0.12, 0, 1),
                    outerOpacity: clamp(options?.outerShadowOpacity ?? 0.22, 0, 1),
                    radius: clamp(options?.shadowRadius ?? 42, 0, 256)
                },
                suppressFullscreen: true
            },
            debug: {
                name: `HGS bar ${screenName ?? ""}`,
                showBounds: Quickshell.env("HGS_HYPRGLASS_DEBUG_BOUNDS") === "1",
                showSamples: false
            }
        };
    }

    function scheduleApply() {
        if (disabledByEnv)
            return;
        if (!probeComplete) {
            _applyPendingAfterProbe = true;
            refreshStatus(true);
            return;
        }
        if (!pluginLoaded)
            return;
        applyTimer.restart();
    }

    function refreshStatus(applyAfterProbe) {
        if (disabledByEnv)
            return;
        if (applyAfterProbe)
            _applyPendingAfterProbe = true;

        Proc.runCommand("hyprglass-status", ["hyprctl", "-j", "fluidglass-status"], (output, exitCode) => {
            probeComplete = true;
            const wasLoaded = pluginLoaded;
            if (exitCode !== 0 || !output) {
                pluginLoaded = false;
                lastStatus = {};
                lastStatusTimestamp = 0;
                statusFresh = false;
                statusRevision++;
                lastError = "hgs hyprglass status failed";
                if (!_statusErrorLogged) {
                    _statusErrorLogged = true;
                    log.warn(lastError);
                }
                return;
            }

            try {
                const status = JSON.parse(output.trim());
                lastStatus = status;
                pluginLoaded = status.pluginLoaded === true;
                pluginGeneration = Number(status.generation ?? 0);
                applyCount = Number(status.applyCount ?? 0);
                descriptorCount = Number(status.descriptorCount ?? (status.descriptors?.length ?? 0));
                lastError = status.error ?? "";
                lastStatusTimestamp = Date.now();
                updateStatusFreshness();
                statusRevision++;
                _statusErrorLogged = false;

                if (pluginLoaded) {
                    if (!wasLoaded)
                        requestDescriptorResync("plugin became available");
                    else if (_pluginMissingCurrentDescriptors(status))
                        requestDescriptorResync("plugin missing current descriptors");
                    reconcileMaterialMode(status, !wasLoaded ? "plugin became available" : "plugin material mode drifted");
                }
            } catch (e) {
                pluginLoaded = false;
                lastStatus = {};
                lastStatusTimestamp = 0;
                statusFresh = false;
                statusRevision++;
                lastError = `failed to parse hgs-hyprglass status: ${e}`;
                log.warn(lastError);
            }

            if (pluginLoaded && _applyPendingAfterProbe) {
                _applyPendingAfterProbe = false;
                scheduleApply();
            }
        }, 0, 2500);
    }

    function _descriptorList() {
        const keys = Object.keys(descriptors || {}).sort();
        const list = [];
        for (let i = 0; i < keys.length; i++) {
            const descriptor = descriptors[keys[i]];
            if (descriptor)
                list.push(descriptor);
        }
        return list;
    }

    function _applyNow() {
        if (!pluginLoaded || disabledByEnv)
            return;

        const list = _descriptorList();

        // Material shared by every glass element (bars + the Labs preview).
        const dynamicLight = (typeof SettingsData !== "undefined") ? (SettingsData.fluidGlassDynamicLight ?? true) : true;
        const glassLevel = clamp((typeof SettingsData !== "undefined") ? (SettingsData.fluidGlassLevel ?? 0.5) : 0.5, 0, 1);
        const stained = (typeof SettingsData !== "undefined") ? (SettingsData.fluidGlassStained ?? false) : false;
        // Frosted Glass preset couples blur+tint to glassLevel; custom mode sends them
        // independently (blurLevel/tintLevel; -1 = let the plugin derive from glassLevel).
        const frosted = (typeof SettingsData !== "undefined") ? (SettingsData.fluidGlassFrosted ?? true) : true;
        const blurLevel = frosted ? -1 : clamp((typeof SettingsData !== "undefined") ? (SettingsData.fluidGlassBlurCustom ?? 0.5) : 0.5, 0, 1);
        const tintLevel = frosted ? -1 : clamp((typeof SettingsData !== "undefined") ? (SettingsData.fluidGlassTintCustom ?? 0.16) : 0.16, 0, 1);
        const tintColor = resolveGlassTintColor();
        const adv = resolveGlassAdvanced();
        const mat = {
            glassLevel: glassLevel,
            tintEnabled: stained,
            tintColor: tintColor,
            blurLevel: blurLevel,
            tintLevel: tintLevel,
            refraction: adv.refraction,
            rimBand: adv.rimBand,
            bevel: adv.bevel,
            rimWidth: adv.rimWidth,
            highlight: adv.highlight,
            shadow: adv.shadow,
            lightAngle: adv.lightAngle,
            specular: adv.specular,
            rimWrap: adv.rimWrap,
            lightFollowsMouse: dynamicLight
        };

        // v2 plugin schema: {enabled, elements:[{id, monitor, x, y, w, h, radius, …material}]}.
        // The bar descriptors carry monitor-local logical geometry; the plugin scales to physical.
        const elements = list.map(d => Object.assign({
            id: d.id,
            monitor: d.surface?.monitor?.name ?? "",
            x: d.geometry?.logical?.x ?? 0,
            y: d.geometry?.logical?.y ?? 0,
            w: d.geometry?.logical?.width ?? 0,
            h: d.geometry?.logical?.height ?? 0,
            radius: d.shape?.radius?.topLeft ?? 0
        }, mat));

        // Fluid Glass Labs preview — a window-anchored element; the plugin places it over the
        // (floating) Settings window each frame from the window's live server-side position.
        if (labsPreview && labsPreview.anchorWindow && labsPreview.w > 0 && labsPreview.h > 0) {
            elements.push(Object.assign({
                id: "hgs:labs-preview",
                monitor: "",
                anchorWindow: labsPreview.anchorWindow,
                offsetX: labsPreview.offsetX,
                offsetY: labsPreview.offsetY,
                x: 0,
                y: 0,
                w: labsPreview.w,
                h: labsPreview.h,
                radius: labsPreview.radius
            }, mat));
        }

        if (elements.length === 0) {
            Proc.runCommand("hyprglass-clear", ["hyprctl", "fluidglass-clear"], (_output, exitCode) => {
                if (exitCode !== 0)
                    refreshStatus(false);
            }, 0, 2500);
            return;
        }

        ++generation;
        const payload = {
            enabled: (typeof SettingsData !== "undefined" && SettingsData.fluidGlassEnabled) === true,
            elements: elements
        };
        Proc.runCommand("hyprglass-apply", ["hyprctl", "fluidglass-apply-json", JSON.stringify(payload)], (_output, exitCode) => {
            if (exitCode !== 0) {
                log.warn("fluidglass apply failed");
                refreshStatus(false);
                return;
            }
            refreshStatus(false);
        }, 0, 5000);
    }

    Timer {
        id: applyTimer
        interval: 100
        repeat: false
        onTriggered: root._applyNow()
    }

    Timer {
        interval: root.statusPollIntervalMs
        repeat: true
        running: !root.disabledByEnv
        onTriggered: root.refreshStatus(false)
    }

    Timer {
        interval: 1000
        repeat: true
        running: !root.disabledByEnv
        onTriggered: root.updateStatusFreshness()
    }

    Component.onCompleted: {
        if (!disabledByEnv)
            Qt.callLater(() => refreshStatus(true));
    }

    Component.onDestruction: cleanupDescriptorsOnShutdown()
}
