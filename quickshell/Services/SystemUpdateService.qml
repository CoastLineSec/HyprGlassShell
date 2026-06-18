pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io
import qs.Common
import qs.Services

Singleton {
    id: root

    property int refCount: 0

    property bool sysupdateAvailable: false

    property var availableUpdates: []
    property bool isChecking: false
    property bool isUpgrading: false
    property bool hasError: false
    property string errorMessage: ""
    property string errorCode: ""
    property var backends: []
    property string distribution: ""
    property string distributionPretty: ""
    property string pkgManager: ""
    property bool distributionSupported: false
    property var recentLog: []
    property int intervalSeconds: 1800
    property int lastCheckUnix: 0
    property int nextCheckUnix: 0

    readonly property int updateCount: availableUpdates.length
    readonly property bool helperAvailable: sysupdateAvailable && backends.length > 0

    Connections {
        target: HGSService
        function onCapabilitiesReceived() {
            root.checkCapabilities();
        }
        function onConnectionStateChanged() {
            if (HGSService.isConnected) {
                root.checkCapabilities();
            } else {
                root.sysupdateAvailable = false;
                root._startupCheckDone = false;
            }
            Qt.callLater(() => root._maybeStartupCheck());
        }
        function onSysupdateStateUpdate(data) {
            root._applyState(data);
        }
    }

    Connections {
        target: SettingsData
        function onUpdaterCheckOnStartChanged() {
            Qt.callLater(() => root._maybeStartupCheck());
        }
        function on_HasLoadedChanged() {
            Qt.callLater(() => root._maybeStartupCheck());
        }
    }

    Component.onCompleted: {
        if (HGSService.hgsAvailable) {
            checkCapabilities();
        }
        Qt.callLater(() => root._maybeStartupCheck());
    }

    function checkCapabilities() {
        if (!HGSService.capabilities || !Array.isArray(HGSService.capabilities)) {
            sysupdateAvailable = false;
            Qt.callLater(() => root._maybeStartupCheck());
            return;
        }
        const has = HGSService.capabilities.includes("sysupdate");
        if (has && !sysupdateAvailable) {
            sysupdateAvailable = true;
            requestState();
        } else if (!has) {
            sysupdateAvailable = false;
        }
        Qt.callLater(() => root._maybeStartupCheck());
    }

    function requestState() {
        if (!HGSService.isConnected || !sysupdateAvailable) {
            return;
        }
        HGSService.sysupdateGetState(resp => {
            if (resp && resp.result) {
                _applyState(resp.result);
            }
        });
    }

    function _applyState(data) {
        if (!data) {
            return;
        }
        availableUpdates = data.packages || [];
        backends = data.backends || [];
        distribution = data.distro || "";
        distributionPretty = data.distroPretty || "";
        distributionSupported = (backends.length > 0);
        recentLog = data.recentLog || [];
        intervalSeconds = data.intervalSeconds || 1800;
        lastCheckUnix = data.lastCheckUnix || 0;
        nextCheckUnix = data.nextCheckUnix || 0;

        const phase = data.phase || "idle";
        switch (phase) {
        case "refreshing":
            isChecking = true;
            isUpgrading = false;
            break;
        case "upgrading":
            isChecking = false;
            isUpgrading = true;
            break;
        default:
            isChecking = false;
            isUpgrading = false;
        }

        if (data.error) {
            hasError = true;
            errorMessage = data.error.message || "";
            errorCode = data.error.code || "";
        } else {
            hasError = false;
            errorMessage = "";
            errorCode = "";
        }

        if (backends.length > 0) {
            const sys = backends.find(b => b.repo === "system" || b.repo === "ostree");
            pkgManager = sys ? sys.id : backends[0].id;
        } else {
            pkgManager = "";
        }
    }

    function checkForUpdates() {
        HGSService.sysupdateRefresh(false, null);
    }

    function runUpdates(opts) {
        const params = opts || {};
        if (SettingsData.updaterUseCustomCommand && SettingsData.updaterCustomCommand.length > 0) {
            _runCustomTerminalCommand();
            return;
        }
        HGSService.sysupdateUpgrade(params, null);
    }

    function cancelUpdates() {
        HGSService.sysupdateCancel(null);
    }

    function setInterval(seconds) {
        HGSService.sysupdateSetInterval(seconds, null);
    }

    function _runCustomTerminalCommand() {
        const terminal = SessionData.resolveTerminal();
        if (!terminal || terminal.length === 0) {
            ToastService.showError(I18n.tr("No terminal configured"), I18n.tr("Pick a terminal in Settings → Launcher (or set $TERMINAL)."));
            return;
        }
        const updateCommand = `${SettingsData.updaterCustomCommand} && echo -n "Updates complete! " ; echo "Press Enter to close..." && read`;
        const termClass = SettingsData.updaterTerminalAdditionalParams || "";
        var argv = [terminal];
        if (termClass.length > 0) {
            argv = argv.concat(termClass.split(" "));
        }
        argv.push("-e");
        argv.push("sh");
        argv.push("-c");
        argv.push(updateCommand);
        customRunner.command = argv;
        customRunner.running = true;
    }

    Process {
        id: customRunner
    }

    property bool _startupCheckDone: false

    function _maybeStartupCheck() {
        if (refCount <= 0) {
            _startupCheckDone = false;
            return;
        }
        if (!SettingsData.updaterCheckOnStart)
            return;
        if (_startupCheckDone)
            return;
        if (!HGSService.isConnected || !sysupdateAvailable)
            return;
        _startupCheckDone = true;
        Qt.callLater(() => root.checkForUpdates());
    }

    onRefCountChanged: {
        if (refCount <= 0)
            _startupCheckDone = false;
        _syncAcquire();
        Qt.callLater(() => root._maybeStartupCheck());
    }
    onSysupdateAvailableChanged: _syncAcquire()

    property bool _acquired: false

    function _syncAcquire() {
        const want = refCount > 0 && sysupdateAvailable;
        if (want === _acquired) {
            return;
        }
        _acquired = want;
        if (want) {
            HGSService.sysupdateAcquire(null);
            return;
        }
        HGSService.sysupdateRelease(null);
    }

}
