.pragma library

    .import "./SettingsSpec.js" as SpecModule

function parse(root, jsonObj) {
    var SPEC = SpecModule.SPEC;

    if (!jsonObj) return;

    for (var k in SPEC) {
        if (k === "pluginSettings") continue;
        // Runtime-only keys are never in the JSON; resetting them here
        // would wipe values set by detection processes on every reload.
        if (SPEC[k].persist === false) continue;
        if (!(k in jsonObj)) {
            root[k] = SPEC[k].def;
        }
    }

    for (var k in jsonObj) {
        if (!SPEC[k]) continue;
        if (k === "pluginSettings") continue;
        var raw = jsonObj[k];
        var spec = SPEC[k];
        var coerce = spec.coerce;
        root[k] = coerce ? (coerce(raw) !== undefined ? coerce(raw) : root[k]) : raw;
    }
}

function toJson(root) {
    var SPEC = SpecModule.SPEC;
    var out = {};
    for (var k in SPEC) {
        if (SPEC[k].persist === false) continue;
        if (k === "pluginSettings") continue;
        out[k] = root[k];
    }
    out.configVersion = root.settingsConfigVersion;
    return out;
}

function migrateToVersion(obj, targetVersion) {
    if (!obj) return null;

    var settings = JSON.parse(JSON.stringify(obj));
    var currentVersion = settings.configVersion || 0;

    if (currentVersion >= targetVersion) {
        return null;
    }

    if (currentVersion < 2) {
        console.info("Migrating settings from version", currentVersion, "to version 2");

        if (settings.barConfigs === undefined) {
            var position = 0;
            if (settings.hgsBarAtBottom !== undefined || settings.topBarAtBottom !== undefined) {
                var atBottom = settings.hgsBarAtBottom !== undefined ? settings.hgsBarAtBottom : settings.topBarAtBottom;
                position = atBottom ? 1 : 0;
            } else if (settings.hgsBarPosition !== undefined) {
                position = settings.hgsBarPosition;
            }

            var defaultConfig = {
                id: "default",
                name: "Main Bar",
                enabled: true,
                position: position,
                screenPreferences: ["all"],
                showOnLastDisplay: true,
                leftWidgets: settings.hgsBarLeftWidgets || ["launcherButton", "workspaceSwitcher", "focusedWindow"],
                centerWidgets: settings.hgsBarCenterWidgets || ["music", "clock", "weather"],
                rightWidgets: settings.hgsBarRightWidgets || ["systemTray", "clipboard", "cpuUsage", "memUsage", "notificationButton", "battery", "controlCenterButton"],
                spacing: settings.hgsBarSpacing !== undefined ? settings.hgsBarSpacing : 4,
                innerPadding: settings.hgsBarInnerPadding !== undefined ? settings.hgsBarInnerPadding : 4,
                bottomGap: settings.hgsBarBottomGap !== undefined ? settings.hgsBarBottomGap : 0,
                transparency: settings.hgsBarTransparency !== undefined ? settings.hgsBarTransparency : 1.0,
                widgetTransparency: settings.hgsBarWidgetTransparency !== undefined ? settings.hgsBarWidgetTransparency : 1.0,
                squareCorners: settings.hgsBarSquareCorners !== undefined ? settings.hgsBarSquareCorners : false,
                noBackground: settings.hgsBarNoBackground !== undefined ? settings.hgsBarNoBackground : false,
                gothCornersEnabled: settings.hgsBarGothCornersEnabled !== undefined ? settings.hgsBarGothCornersEnabled : false,
                gothCornerRadiusOverride: settings.hgsBarGothCornerRadiusOverride !== undefined ? settings.hgsBarGothCornerRadiusOverride : false,
                gothCornerRadiusValue: settings.hgsBarGothCornerRadiusValue !== undefined ? settings.hgsBarGothCornerRadiusValue : 12,
                borderEnabled: settings.hgsBarBorderEnabled !== undefined ? settings.hgsBarBorderEnabled : false,
                borderColor: settings.hgsBarBorderColor || "surfaceText",
                borderOpacity: settings.hgsBarBorderOpacity !== undefined ? settings.hgsBarBorderOpacity : 1.0,
                borderThickness: settings.hgsBarBorderThickness !== undefined ? settings.hgsBarBorderThickness : 1,
                fontScale: settings.hgsBarFontScale !== undefined ? settings.hgsBarFontScale : 1.0,
                autoHide: settings.hgsBarAutoHide !== undefined ? settings.hgsBarAutoHide : false,
                autoHideDelay: settings.hgsBarAutoHideDelay !== undefined ? settings.hgsBarAutoHideDelay : 250,
                visible: settings.hgsBarVisible !== undefined ? settings.hgsBarVisible : true,
                popupGapsAuto: settings.popupGapsAuto !== undefined ? settings.popupGapsAuto : true,
                popupGapsManual: settings.popupGapsManual !== undefined ? settings.popupGapsManual : 4
            };

            settings.barConfigs = [defaultConfig];

            var legacyKeys = [
                "hgsBarLeftWidgets", "hgsBarCenterWidgets", "hgsBarRightWidgets",
                "hgsBarWidgetOrder", "hgsBarAutoHide", "hgsBarAutoHideDelay",
                "hgsBarVisible", "hgsBarSpacing",
                "hgsBarBottomGap", "hgsBarInnerPadding", "hgsBarPosition",
                "hgsBarSquareCorners", "hgsBarNoBackground", "hgsBarGothCornersEnabled",
                "hgsBarGothCornerRadiusOverride", "hgsBarGothCornerRadiusValue",
                "hgsBarBorderEnabled", "hgsBarBorderColor", "hgsBarBorderOpacity",
                "hgsBarBorderThickness", "popupGapsAuto", "popupGapsManual",
                "hgsBarAtBottom", "topBarAtBottom", "hgsBarTransparency", "hgsBarWidgetTransparency"
            ];

            for (var i = 0; i < legacyKeys.length; i++) {
                delete settings[legacyKeys[i]];
            }

            console.info("Migrated single bar settings to barConfigs");
        }

        settings.configVersion = 2;
    }

    if (currentVersion < 3) {
        console.info("Migrating settings from version", currentVersion, "to version 3");
        console.info("Per-widget controlCenterButton config now supported via widgetData properties");
        settings.configVersion = 3;
    }

    if (currentVersion < 4) {
        console.info("Migrating settings from version", currentVersion, "to version 4");
        console.info("Migrating desktop widgets to unified desktopWidgetInstances");

        var instances = [];

        if (settings.desktopClockEnabled) {
            var clockPositions = {};
            if (settings.desktopClockX !== undefined && settings.desktopClockX >= 0) {
                clockPositions["default"] = {
                    x: settings.desktopClockX,
                    y: settings.desktopClockY,
                    width: settings.desktopClockWidth || 280,
                    height: settings.desktopClockHeight || 180
                };
            }

            instances.push({
                id: "dw_clock_primary",
                widgetType: "desktopClock",
                name: "Desktop Clock",
                enabled: true,
                config: {
                    style: settings.desktopClockStyle || "analog",
                    transparency: settings.desktopClockTransparency !== undefined ? settings.desktopClockTransparency : 0.8,
                    colorMode: settings.desktopClockColorMode || "primary",
                    customColor: settings.desktopClockCustomColor || "#ffffff",
                    showDate: settings.desktopClockShowDate !== false,
                    showAnalogNumbers: settings.desktopClockShowAnalogNumbers || false,
                    showAnalogSeconds: settings.desktopClockShowAnalogSeconds !== false,
                    displayPreferences: settings.desktopClockDisplayPreferences || ["all"]
                },
                positions: clockPositions
            });
        }

        if (settings.systemMonitorEnabled) {
            var sysmonPositions = {};
            if (settings.systemMonitorX !== undefined && settings.systemMonitorX >= 0) {
                sysmonPositions["default"] = {
                    x: settings.systemMonitorX,
                    y: settings.systemMonitorY,
                    width: settings.systemMonitorWidth || 320,
                    height: settings.systemMonitorHeight || 480
                };
            }

            instances.push({
                id: "dw_sysmon_primary",
                widgetType: "systemMonitor",
                name: "System Monitor",
                enabled: true,
                config: {
                    showHeader: settings.systemMonitorShowHeader !== false,
                    transparency: settings.systemMonitorTransparency !== undefined ? settings.systemMonitorTransparency : 0.8,
                    colorMode: settings.systemMonitorColorMode || "primary",
                    customColor: settings.systemMonitorCustomColor || "#ffffff",
                    showCpu: settings.systemMonitorShowCpu !== false,
                    showCpuGraph: settings.systemMonitorShowCpuGraph !== false,
                    showCpuTemp: settings.systemMonitorShowCpuTemp !== false,
                    showGpuTemp: settings.systemMonitorShowGpuTemp || false,
                    gpuPciId: settings.systemMonitorGpuPciId || "",
                    showMemory: settings.systemMonitorShowMemory !== false,
                    showMemoryGraph: settings.systemMonitorShowMemoryGraph !== false,
                    showNetwork: settings.systemMonitorShowNetwork !== false,
                    showNetworkGraph: settings.systemMonitorShowNetworkGraph !== false,
                    showDisk: settings.systemMonitorShowDisk !== false,
                    showTopProcesses: settings.systemMonitorShowTopProcesses || false,
                    topProcessCount: settings.systemMonitorTopProcessCount || 3,
                    topProcessSortBy: settings.systemMonitorTopProcessSortBy || "cpu",
                    layoutMode: settings.systemMonitorLayoutMode || "auto",
                    graphInterval: settings.systemMonitorGraphInterval || 60,
                    displayPreferences: settings.systemMonitorDisplayPreferences || ["all"]
                },
                positions: sysmonPositions
            });
        }

        var variants = settings.systemMonitorVariants || [];
        for (var i = 0; i < variants.length; i++) {
            var v = variants[i];
            instances.push({
                id: v.id,
                widgetType: "systemMonitor",
                name: v.name || ("System Monitor " + (i + 2)),
                enabled: true,
                config: v.config || {},
                positions: v.positions || {}
            });
        }

        settings.desktopWidgetInstances = instances;
        settings.configVersion = 4;
    }

    if (currentVersion < 5) {
        console.info("Migrating settings from version", currentVersion, "to version 5");
        console.info("Moving sensitive data (weather location, coordinates) to session.json");

        delete settings.weatherLocation;
        delete settings.weatherCoordinates;

        settings.configVersion = 5;
    }

    if (currentVersion < 6) {
        console.info("Migrating settings from version", currentVersion, "to version 6");

        if (settings.barElevationEnabled === undefined) {
            var legacyBars = Array.isArray(settings.barConfigs) ? settings.barConfigs : [];
            var hadLegacyBarShadowEnabled = false;
            for (var j = 0; j < legacyBars.length; j++) {
                var legacyIntensity = Number(legacyBars[j] && legacyBars[j].shadowIntensity);
                if (!isNaN(legacyIntensity) && legacyIntensity > 0) {
                    hadLegacyBarShadowEnabled = true;
                    break;
                }
            }
            settings.barElevationEnabled = hadLegacyBarShadowEnabled;
        }

        settings.configVersion = 6;
    }

    if (currentVersion < 11) {
        settings.configVersion = 11;
    }

    return settings;
}

function cleanup(fileText) {
    var getValidKeys = SpecModule.getValidKeys;
    if (!fileText || !fileText.trim()) return;

    try {
        var settings = JSON.parse(fileText);
        var validKeys = getValidKeys();
        var needsSave = false;

        for (var key in settings) {
            if (validKeys.indexOf(key) < 0) {
                delete settings[key];
                needsSave = true;
            }
        }

        return needsSave ? JSON.stringify(settings, null, 2) : null;
    } catch (e) {
        console.warn("SettingsData: Failed to cleanup unused keys:", e.message);
        return null;
    }
}
