pragma Singleton
pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import Quickshell
import Quickshell.Io
import qs.Common
import qs.Services
import "../Common/ConfigIncludeResolve.js" as ConfigIncludeResolve
import "../Common/KeybindActions.js" as Actions

Singleton {
    id: root
    readonly property var log: Log.scoped("KeybindsService")

    property bool available: true
    readonly property string currentProvider: "hyprland"

    readonly property string cheatsheetProvider: currentProvider
    property bool cheatsheetAvailable: true
    property bool cheatsheetLoading: false
    property var cheatsheet: ({})

    property bool loading: false
    property bool saving: false
    property bool fixing: false
    property string lastError: ""
    property bool hgsBindsIncluded: true

    property var hgsStatus: ({
            "exists": true,
            "included": true,
            "includePosition": -1,
            "totalIncludes": 0,
            "bindsAfterHgs": 0,
            "effective": true,
            "overriddenBy": 0,
            "statusMessage": "",
            "configFormat": "",
            "readOnly": false
        })

    property var _rawData: null
    property var keybinds: ({})
    property var _allBinds: ({})
    property var _categories: []
    property var _flatCache: []
    property var displayList: []
    property int _dataVersion: 0
    property string _pendingSavedKey: ""

    readonly property var categoryOrder: Actions.getCategoryOrder()
    readonly property string configDir: Paths.strip(StandardPaths.writableLocation(StandardPaths.ConfigLocation))
    readonly property string compositorConfigDir: configDir + "/hypr"
    readonly property string hgsBindsPath: compositorConfigDir + "/hgs/keybinds.lua"
    readonly property string mainConfigPath: compositorConfigDir + "/hyprland.lua"
    readonly property bool readOnly: hgsStatus.readOnly === true
    readonly property var actionTypes: Actions.getActionTypes()
    readonly property var hgsActions: getHgsActions()

    signal bindsLoaded
    signal bindSaved(string key)
    signal bindSaveCompleted(bool success)
    signal bindRemoved(string key)
    signal hgsBindsFixed
    signal cheatsheetLoaded

    Process {
        id: cheatsheetProcess
        running: false

        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    root.cheatsheet = JSON.parse(text);
                } catch (e) {
                    log.error("Failed to parse cheatsheet:", e);
                    root.cheatsheet = {};
                }
                root.cheatsheetLoading = false;
                root.cheatsheetLoaded();
            }
        }

        onExited: exitCode => {
            if (exitCode === 0)
                return;
            log.warn("Cheatsheet load failed with code:", exitCode);
            root.cheatsheetLoading = false;
        }
    }

    Process {
        id: loadProcess
        running: false

        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    root._rawData = JSON.parse(text);
                    root._processData();
                } catch (e) {
                    log.error("Failed to parse binds:", e);
                }
                root.loading = false;
            }
        }

        onExited: exitCode => {
            if (exitCode !== 0) {
                log.warn("Load process failed with code:", exitCode);
                root.loading = false;
            }
        }
    }

    Process {
        id: saveProcess
        running: false

        stderr: StdioCollector {
            onStreamFinished: {
                if (!text.trim())
                    return;
                root.lastError = text.trim();
                ToastService.showError(I18n.tr("Failed to save keybind"), "", root.lastError, "keybinds");
            }
        }

        onExited: exitCode => {
            root.saving = false;
            if (exitCode !== 0) {
                log.error("Save failed with code:", exitCode);
                root.bindSaveCompleted(false);
                return;
            }
            root.lastError = "";
            root.bindSaveCompleted(true);
            root.loadBinds(false);
        }
    }

    Process {
        id: removeProcess
        running: false

        stderr: StdioCollector {
            onStreamFinished: {
                if (!text.trim())
                    return;
                root.lastError = text.trim();
                ToastService.showError(I18n.tr("Failed to remove keybind"), "", root.lastError, "keybinds");
            }
        }

        onExited: exitCode => {
            if (exitCode !== 0) {
                log.error("Remove failed with code:", exitCode);
                return;
            }
            root.lastError = "";
            root.loadBinds(false);
        }
    }

    Process {
        id: fixProcess
        running: false

        stderr: StdioCollector {
            onStreamFinished: {
                if (!text.trim())
                    return;
                root.lastError = text.trim();
                ToastService.showError(I18n.tr("Failed to add binds include"), "", root.lastError, "keybinds");
            }
        }

        onExited: exitCode => {
            root.fixing = false;
            if (exitCode !== 0) {
                log.error("Fix failed with code:", exitCode);
                return;
            }
            root.lastError = "";
            root.hgsBindsIncluded = true;
            root.hgsBindsFixed();
            const bindsRel = "hgs/keybinds.lua";
            ToastService.showInfo(I18n.tr("Binds include added"), I18n.tr("%1 is now included in config").arg(bindsRel), "", "keybinds");
            Qt.callLater(root.forceReload);
        }
    }

    function fixHgsBindsInclude() {
        if (fixing || hgsBindsIncluded || !compositorConfigDir)
            return;
        if (readOnly) {
            showHyprlandReadOnlyWarning();
            return;
        }
        fixing = true;
        const timestamp = Math.floor(Date.now() / 1000);
        const backupPath = `${mainConfigPath}.hgsbackup${timestamp}`;
        const script = ConfigIncludeResolve.buildRepairScript({
            configFile: mainConfigPath,
            backupFile: backupPath,
            fragmentFiles: [compositorConfigDir + "/hgs/keybinds.lua", compositorConfigDir + "/hgs/user-keybinds.lua"],
            includes: [
                {
                    grepPattern: "hgs.keybinds",
                    includeLine: "require(\"hgs.keybinds\")"
                },
                {
                    grepPattern: "hgs.user-keybinds",
                    includeLine: "require(\"hgs.user-keybinds\")"
                }
            ]
        });
        fixProcess.command = ["sh", "-c", script];
        fixProcess.running = true;
    }

    function forceReload() {
        _allBinds = {};
        _flatCache = [];
        _categories = [];
        loadBinds(true);
    }

    function loadCheatsheet(provider) {
        if (cheatsheetProcess.running)
            return;
        const target = provider || cheatsheetProvider;
        if (!target)
            return;
        cheatsheetLoading = true;
        cheatsheetProcess.command = ["hgs", "keybinds", "show", target];
        cheatsheetProcess.running = true;
    }

    function loadBinds(showLoading) {
        if (loadProcess.running || !available)
            return;
        const hasData = Object.keys(_allBinds).length > 0;
        loading = showLoading !== false && !hasData;
        loadProcess.command = ["hgs", "keybinds", "show", currentProvider];
        loadProcess.running = true;
    }

    function _processData() {
        keybinds = _rawData || {};
        hgsBindsIncluded = _rawData?.hgsBindsIncluded ?? true;
        const status = _rawData?.hgsStatus;
        if (status) {
            hgsStatus = {
                "exists": status.exists ?? true,
                "included": status.included ?? true,
                "includePosition": status.includePosition ?? -1,
                "totalIncludes": status.totalIncludes ?? 0,
                "bindsAfterHgs": status.bindsAfterHgs ?? 0,
                "effective": status.effective ?? true,
                "overriddenBy": status.overriddenBy ?? 0,
                "statusMessage": status.statusMessage ?? "",
                "configFormat": status.configFormat ?? "",
                "readOnly": status.readOnly === true
            };
        }
        _maybeWarnHyprlandLegacyConf();

        if (!_rawData?.binds) {
            _allBinds = {};
            _categories = [];
            _flatCache = [];
            displayList = [];
            _dataVersion++;
            bindsLoaded();
            if (_pendingSavedKey) {
                bindSaved(_pendingSavedKey);
                _pendingSavedKey = "";
            }
            return;
        }

        const processed = {};
        const bindsData = _rawData.binds;
        for (const cat in bindsData) {
            const binds = bindsData[cat];
            for (var i = 0; i < binds.length; i++) {
                const bind = binds[i];
                const targetCat = Actions.isHgsAction(bind.action) ? "HGS" : cat;
                if (!processed[targetCat])
                    processed[targetCat] = [];
                processed[targetCat].push(bind);
            }
        }

        const sortedCats = Object.keys(processed).sort((a, b) => {
            const ai = categoryOrder.indexOf(a);
            const bi = categoryOrder.indexOf(b);
            return (ai === -1 ? 999 : ai) - (bi === -1 ? 999 : bi);
        });

        const grouped = [];
        const actionMap = {};
        for (var ci = 0; ci < sortedCats.length; ci++) {
            const category = sortedCats[ci];
            const binds = processed[category];
            if (!binds)
                continue;
            for (var i = 0; i < binds.length; i++) {
                const bind = binds[i];
                const action = bind.action || "";
                const sourceStr = bind.source || "config";
                const keyData = {
                    "key": bind.key || "",
                    "desc": bind.desc || "",
                    "source": sourceStr,
                    "isOverride": sourceStr === "hgs",
                    "isHGSManaged": sourceStr === "hgs" || sourceStr === "hgs-default",
                    "hasDefault": bind.hasDefault === true,
                    "cooldownMs": bind.cooldownMs || 0,
                    "flags": bind.flags || "",
                    "allowWhenLocked": bind.allowWhenLocked || false,
                    "allowInhibiting": bind.allowInhibiting,
                    "repeat": bind.repeat
                };
                if (actionMap[action]) {
                    actionMap[action].keys.push(keyData);
                    if (!actionMap[action].desc && bind.desc)
                        actionMap[action].desc = bind.desc;
                    if (!actionMap[action].conflict && bind.conflict)
                        actionMap[action].conflict = bind.conflict;
                } else {
                    const entry = {
                        "category": category,
                        "action": action,
                        "desc": bind.desc || "",
                        "keys": [keyData],
                        "conflict": bind.conflict || null
                    };
                    actionMap[action] = entry;
                    grouped.push(entry);
                }
            }
        }

        const list = [];
        for (const cat of sortedCats) {
            list.push({
                "id": "cat:" + cat,
                "type": "category",
                "name": cat
            });
            const binds = processed[cat];
            if (!binds)
                continue;
            for (const bind of binds)
                list.push({
                    "id": "bind:" + bind.key,
                    "type": "bind",
                    "key": bind.key,
                    "desc": bind.desc
                });
        }

        _allBinds = processed;
        _categories = sortedCats;
        _flatCache = grouped;
        displayList = list;
        _dataVersion++;
        bindsLoaded();
        if (_pendingSavedKey) {
            bindSaved(_pendingSavedKey);
            _pendingSavedKey = "";
        }
    }

    function getCategories() {
        return _categories;
    }

    function getFlatBinds() {
        return _flatCache;
    }

    function keysForAction(actionId) {
        if (!actionId)
            return [];
        for (let i = 0; i < _flatCache.length; i++) {
            const group = _flatCache[i];
            if (!group || group.action !== actionId || !Array.isArray(group.keys))
                continue;
            const keys = [];
            for (let k = 0; k < group.keys.length; k++) {
                const key = group.keys[k]?.key || "";
                if (key)
                    keys.push(key);
            }
            return keys;
        }
        return [];
    }

    function saveBind(originalKey, bindData) {
        if (readOnly) {
            showHyprlandReadOnlyWarning();
            return;
        }
        if (!bindData.key || !Actions.isValidAction(bindData.action))
            return;
        saving = true;
        const cmd = ["hgs", "keybinds", "set", currentProvider, bindData.key, bindData.action, "--desc", bindData.desc || ""];
        if (originalKey && originalKey !== bindData.key)
            cmd.push("--replace-key", originalKey);
        if (bindData.cooldownMs > 0)
            cmd.push("--cooldown-ms", String(bindData.cooldownMs));
        if (bindData.allowWhenLocked)
            cmd.push("--allow-when-locked");
        if (bindData.repeat === false)
            cmd.push("--no-repeat");
        if (bindData.allowInhibiting === false)
            cmd.push("--no-inhibiting");
        if (bindData.flags)
            cmd.push("--flags", bindData.flags);
        saveProcess.command = cmd;
        saveProcess.running = true;
        _pendingSavedKey = bindData.key;
    }

    property bool _hyprlandLegacyWarnShown: false

    function _maybeWarnHyprlandLegacyConf() {
        if (_hyprlandLegacyWarnShown)
            return;
        if (readOnly) {
            _hyprlandLegacyWarnShown = true;
            showHyprlandReadOnlyWarning();
            return;
        }
        if (!hgsStatus.exists || hgsStatus.included)
            return;
        _hyprlandLegacyWarnShown = true;
        ToastService.showWarning(I18n.tr("Hyprland config include missing"), I18n.tr("HGS Settings writes Lua keybinds. Add the HGS include so edits apply."), "hgs setup", "hyprland-migration");
    }

    function showHyprlandReadOnlyWarning() {
        ToastService.showWarning(I18n.tr("Hyprland conf mode"), I18n.tr("This install is still using hyprland.conf. Run hgs setup to migrate before editing shortcuts in Settings."), "hgs setup", "hyprland-migration");
    }

    function removeBind(key) {
        if (readOnly) {
            showHyprlandReadOnlyWarning();
            return;
        }
        if (!key)
            return;
        removeProcess.command = ["hgs", "keybinds", "remove", currentProvider, key];
        removeProcess.running = true;
        bindRemoved(key);
    }

    function resetBind(key) {
        if (readOnly) {
            showHyprlandReadOnlyWarning();
            return;
        }
        if (!key)
            return;
        removeProcess.command = ["hgs", "keybinds", "reset", currentProvider, key];
        removeProcess.running = true;
        bindRemoved(key);
    }

    function isHgsAction(action) {
        return Actions.isHgsAction(action);
    }

    function isValidAction(action) {
        return Actions.isValidAction(action);
    }

    function getActionType(action) {
        return Actions.getActionType(action);
    }

    function getActionLabel(action) {
        return Actions.getActionLabel(action, currentProvider);
    }

    function getCompositorCategories() {
        return Actions.getCompositorCategories(currentProvider);
    }

    function getCompositorActions(category) {
        return Actions.getCompositorActions(currentProvider, category);
    }

    function getHgsActions() {
        return Actions.getHgsActions();
    }

    function buildSpawnAction(command, args) {
        return Actions.buildSpawnAction(command, args);
    }

    function buildShellAction(shellCmd, shell) {
        return Actions.buildShellAction(currentProvider, shellCmd, shell);
    }

    function getShellFromAction(action) {
        return Actions.getShellFromAction(action);
    }

    function parseSpawnCommand(action) {
        return Actions.parseSpawnCommand(action);
    }

    function parseShellCommand(action) {
        return Actions.parseShellCommand(action);
    }
}
