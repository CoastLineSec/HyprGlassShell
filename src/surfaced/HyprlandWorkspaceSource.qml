pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import "HyprlandWorkspaceProtocol.js" as WorkspaceProtocol

Scope {
    id: root

    property string requestSocketPath: ""
    property string eventSocketPath: ""

    readonly property var snapshot: root._snapshot
    readonly property bool available: root._available
    readonly property bool live: Boolean(
        root._eventStream && root._eventStream.ready
    )
    readonly property string lastError: root._lastError
    readonly property bool usingLua: root._usingLua
    readonly property string configProvider: root._configProvider
    readonly property bool actionsAvailable: root._available
        && root.live
        && !root._cycleActive
        && !root._refreshQueued
        && !refreshTimer.running
        && !retryTimer.running

    signal dispatchRequested(string command)

    property var _snapshot: WorkspaceProtocol.emptySnapshot()
    property var _urgentAddresses: ({})
    property bool _available: false
    property string _lastError: ""
    property bool _usingLua: false
    property string _configProvider: "unknown"
    property int _consecutiveFailures: 0
    property int _retryAttempt: 0

    property bool _started: false
    property var _eventStream: null
    property int _eventGeneration: 0
    property int _eventReconnectAttempt: 0

    property bool _cycleActive: false
    property bool _refreshQueued: false
    property int _cycleEventGeneration: 0
    property int _cycleStage: 0
    property var _cyclePayloads: ({})
    property var _activeRequest: null

    readonly property int maximumResponseCharacters: 4 * 1024 * 1024
    readonly property int requestTimeoutMs: 1000
    readonly property int eventConnectionTimeoutMs: 1000
    readonly property int eventLossGraceMs: 3000

    function refreshNow() {
        root._queueRefresh(0);
    }

    function activateWorkspace(outputName, workspaceId) {
        if (!root.actionsAvailable)
            return false;

        const monitor = WorkspaceProtocol.findMonitor(
            root.snapshot,
            outputName
        );
        const workspace = WorkspaceProtocol.findWorkspace(
            root.snapshot,
            outputName,
            workspaceId
        );
        if (!monitor
                || !workspace
                || monitor.activeWorkspaceId === workspace.id) {
            return false;
        }

        const command = WorkspaceProtocol.workspaceDispatch(
            root.usingLua,
            workspace
        );
        if (command.length === 0)
            return false;
        root.dispatchRequested(command);
        return true;
    }

    function activateWindow(outputName, workspaceId, address) {
        if (!root.actionsAvailable)
            return false;

        const monitor = WorkspaceProtocol.findMonitor(
            root.snapshot,
            outputName
        );
        const workspace = WorkspaceProtocol.findWorkspace(
            root.snapshot,
            outputName,
            workspaceId
        );
        const client = WorkspaceProtocol.findClient(root.snapshot, address);
        if (!monitor
                || !workspace
                || !client
                || !client.mapped
                || monitor.activeWorkspaceId !== workspace.id
                || client.workspaceId !== workspace.id
                || client.workspaceName !== workspace.name
                || client.monitorId !== monitor.id) {
            return false;
        }

        const command = WorkspaceProtocol.windowDispatch(
            root.usingLua,
            client.address
        );
        if (command.length === 0)
            return false;
        root.dispatchRequested(command);
        return true;
    }

    function _connectEventSocket() {
        if (!root._started
                || root._eventStream
                || root.eventSocketPath.length === 0) {
            return;
        }

        const stream = eventStreamComponent.createObject(root, {
            socketPath: root.eventSocketPath,
            connectionTimeoutMs: root.eventConnectionTimeoutMs
        });
        if (!stream) {
            root._handleEventFailure(
                null,
                qsTr("Hyprland event connection could not be created.")
            );
            return;
        }
        root._eventStream = stream;
    }

    function _destroyEventStream(stream) {
        if (!stream)
            return;
        if (root._eventStream === stream)
            root._eventStream = null;
        stream.stop();
        stream.destroy();
    }

    function _eventConnected(stream) {
        if (stream !== root._eventStream)
            return;
        eventReconnectTimer.stop();
        eventLossTimer.stop();
        root._eventReconnectAttempt = 0;
        root._eventGeneration += 1;
        root._refreshQueued = true;
        // The source-level `live` binding updates after this nested signal.
        Qt.callLater(function() {
            root._queueRefresh(0);
        });
    }

    function _handleEventFailure(stream, message) {
        if (stream && stream !== root._eventStream)
            return;
        if (stream)
            root._destroyEventStream(stream);

        root._eventGeneration += 1;
        root._refreshQueued = true;
        root._lastError = String(message || qsTr(
            "Hyprland event connection is unavailable."
        ));
        refreshTimer.stop();
        if (root._activeRequest)
            root._abortRequest(root._lastError);
        else if (root._cycleActive)
            root._failCycle(root._lastError);

        if (!root._started)
            return;
        if (!eventLossTimer.running)
            eventLossTimer.start();
        const exponent = Math.min(root._eventReconnectAttempt, 5);
        eventReconnectTimer.interval = Math.min(
            100 * Math.pow(2, exponent),
            2000
        );
        root._eventReconnectAttempt += 1;
        eventReconnectTimer.restart();
    }

    function _handleEventLine(line) {
        if (String(line || "").length > 256 * 1024) {
            root._queueRefresh(0);
            return;
        }

        const event = WorkspaceProtocol.parseEvent(line);
        if (!event.relevant)
            return;

        root._urgentAddresses = WorkspaceProtocol.applyUrgentEvent(
            root._urgentAddresses,
            event,
            root._snapshot
        );
        root._queueRefresh(event.valid ? 40 : 0);
    }

    function _queueRefresh(delayMs) {
        root._refreshQueued = true;
        if (!root._started || !root.live || root._cycleActive)
            return;

        const delay = Math.max(0, Number(delayMs));
        if (refreshTimer.running && refreshTimer.interval <= delay)
            return;
        refreshTimer.interval = delay;
        refreshTimer.restart();
    }

    function _beginCycle() {
        if (!root._started || !root.live)
            return;
        if (root._cycleActive) {
            root._refreshQueued = true;
            return;
        }

        retryTimer.stop();
        root._cycleActive = true;
        root._refreshQueued = false;
        root._cycleEventGeneration = root._eventGeneration;
        root._cycleStage = 0;
        root._cyclePayloads = ({});
        root._startRequest("j/status");
    }

    function _startRequest(command) {
        if (!root._cycleActive || root._activeRequest)
            return;
        if (root.requestSocketPath.length === 0) {
            root._failCycle(qsTr(
                "Hyprland request socket is unavailable."
            ));
            return;
        }

        const request = requestComponent.createObject(root, {
            socketPath: root.requestSocketPath,
            command: command,
            timeoutMs: root.requestTimeoutMs,
            maximumResponseCharacters: root.maximumResponseCharacters
        });
        if (!request) {
            root._failCycle(qsTr(
                "Hyprland request could not be created."
            ));
            return;
        }
        root._activeRequest = request;
    }

    function _destroyRequest(request) {
        if (!request)
            return;
        if (root._activeRequest === request)
            root._activeRequest = null;
        request.cancel();
        request.destroy();
    }

    function _requestSucceeded(request, payload) {
        if (request !== root._activeRequest)
            return;
        const command = request.command;
        root._destroyRequest(request);
        root._acceptResponse(command, payload);
    }

    function _requestFailed(request, message) {
        if (request !== root._activeRequest)
            return;
        root._destroyRequest(request);
        root._failCycle(message);
    }

    function _abortRequest(message) {
        if (root._activeRequest)
            root._destroyRequest(root._activeRequest);
        root._failCycle(message);
    }

    function _acceptResponse(command, payload) {
        if (!root._cycleActive)
            return;

        if (root._cycleEventGeneration !== root._eventGeneration
                || !root.live) {
            root._discardInvalidatedCycle();
            return;
        }

        const payloads = Object.assign({}, root._cyclePayloads);
        payloads[command] = payload;
        root._cyclePayloads = payloads;
        root._cycleStage += 1;

        const commands = [
            "j/status",
            "j/workspaces",
            "j/monitors",
            "j/clients"
        ];
        if (root._cycleStage < commands.length) {
            const nextCommand = commands[root._cycleStage];
            Qt.callLater(function() {
                root._startRequest(nextCommand);
            });
        } else {
            Qt.callLater(root._commitCycle);
        }
    }

    function _commitCycle() {
        if (!root._cycleActive)
            return;
        if (root._cycleEventGeneration !== root._eventGeneration
                || !root.live) {
            root._discardInvalidatedCycle();
            return;
        }

        const status = WorkspaceProtocol.parseStatus(
            root._cyclePayloads["j/status"]
        );
        if (!status.ok) {
            root._failCycle(status.error);
            return;
        }
        const result = WorkspaceProtocol.buildSnapshot(
            root._cyclePayloads["j/workspaces"],
            root._cyclePayloads["j/monitors"],
            root._cyclePayloads["j/clients"],
            root._urgentAddresses,
            root._snapshot.revision + 1
        );
        if (!result.ok) {
            root._failCycle(result.error);
            return;
        }

        root._usingLua = status.usingLua;
        root._configProvider = status.configProvider;
        root._snapshot = Object.assign({}, result.snapshot, {
            configProvider: status.configProvider,
            usingLua: status.usingLua
        });
        root._urgentAddresses = result.urgentAddresses;
        root._cycleActive = false;
        root._available = true;
        root._lastError = "";
        root._consecutiveFailures = 0;
        root._retryAttempt = 0;

        if (root._refreshQueued)
            root._queueRefresh(0);
    }

    function _discardInvalidatedCycle() {
        if (root._activeRequest)
            root._destroyRequest(root._activeRequest);
        root._cycleActive = false;
        root._refreshQueued = true;
        if (root.live)
            root._queueRefresh(0);
    }

    function _failCycle(message) {
        if (root._activeRequest)
            root._destroyRequest(root._activeRequest);
        root._cycleActive = false;
        root._refreshQueued = false;
        root._lastError = String(message || qsTr(
            "Hyprland workspace state is temporarily unavailable."
        ));
        root._consecutiveFailures += 1;
        if (root._consecutiveFailures >= 3)
            root._available = false;

        if (!root._started || !root.live)
            return;
        const delays = [100, 250, 500, 1000, 2000];
        retryTimer.interval = delays[Math.min(
            root._retryAttempt,
            delays.length - 1
        )];
        root._retryAttempt += 1;
        retryTimer.restart();
    }

    onEventSocketPathChanged: {
        if (!root._started)
            return;
        if (root._eventStream) {
            root._handleEventFailure(
                root._eventStream,
                qsTr("Hyprland event socket changed.")
            );
        } else if (root.eventSocketPath.length > 0) {
            eventReconnectTimer.interval = 0;
            eventReconnectTimer.restart();
        }
    }

    Component.onCompleted: {
        root._started = true;
        root._connectEventSocket();
    }

    Component.onDestruction: {
        root._started = false;
        refreshTimer.stop();
        retryTimer.stop();
        eventReconnectTimer.stop();
        eventLossTimer.stop();
        if (root._activeRequest)
            root._destroyRequest(root._activeRequest);
        if (root._eventStream)
            root._destroyEventStream(root._eventStream);
    }

    Component {
        id: requestComponent

        HyprlandSocketRequest {
            id: request

            onSucceeded: payload => {
                root._requestSucceeded(request, payload);
            }
            onFailed: message => {
                root._requestFailed(request, message);
            }
        }
    }

    Component {
        id: eventStreamComponent

        HyprlandEventStream {
            id: stream

            onConnected: root._eventConnected(stream)
            onEventLine: line => {
                if (root._eventStream === stream)
                    root._handleEventLine(line);
            }
            onFailed: message => {
                root._handleEventFailure(stream, message);
            }
        }
    }

    Timer {
        id: refreshTimer

        interval: 0
        repeat: false
        onTriggered: root._beginCycle()
    }

    Timer {
        id: retryTimer

        repeat: false
        onTriggered: root._queueRefresh(0)
    }

    Timer {
        id: eventReconnectTimer

        repeat: false
        onTriggered: root._connectEventSocket()
    }

    Timer {
        id: eventLossTimer

        interval: root.eventLossGraceMs
        repeat: false
        onTriggered: {
            if (!root.live) {
                root._available = false;
                root._lastError = qsTr(
                    "Hyprland event connection is unavailable."
                );
            }
        }
    }
}
