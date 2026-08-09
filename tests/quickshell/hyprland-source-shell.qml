//@ pragma ShellId hyprshelld-hyprland-source-test
//@ pragma StateDir $BASE/hyprshelld/hyprland-source-test
//@ pragma CacheDir $BASE/hyprshelld/hyprland-source-test
//@ pragma DataDir $BASE/hyprshelld/hyprland-source-test

pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io
import "surfaced"

ShellRoot {
    id: root

    readonly property string runtimeRoot: Quickshell.env("XDG_RUNTIME_DIR")
    readonly property string requestPath: runtimeRoot + "/hyprshelld-request.sock"
    readonly property string eventPath: runtimeRoot + "/hyprshelld-event.sock"

    property int phase: 0
    property bool failed: false
    property bool hungRequest: false
    property bool injectedCycleEvent: false
    property bool legacyStatus: false
    property bool staleActionChecked: false
    property var eventClient: null
    property var commandsReceived: []
    property var commandsCompleted: []
    property var dispatches: []
    property var retainedSnapshot: null
    property int retainedRevision: 0

    function check(condition, message) {
        if (condition)
            return true;
        root.failed = true;
        console.error("HYPRLAND_SOURCE_FAIL: " + message);
        Qt.quit();
        return false;
    }

    function statusResponse() {
        return root.legacyStatus
            ? "unknown request\n"
            : '{"configProvider":"lua"}';
    }

    function workspaceResponse() {
        return JSON.stringify([
            {
                id: 1,
                name: "1",
                monitor: "DP-4",
                monitorID: 0,
                windows: 1,
                ispersistent: false,
                lastwindow: "0xa"
            },
            {
                id: -44,
                name: "écriture",
                monitor: "DP-4",
                monitorID: 0,
                windows: 1,
                ispersistent: true,
                lastwindow: "0xb"
            }
        ]);
    }

    function monitorResponse() {
        return JSON.stringify([{
            id: 0,
            name: "DP-4",
            focused: true,
            activeWorkspace: {
                id: 1,
                name: "1"
            }
        }]);
    }

    function clientResponse() {
        return JSON.stringify([
            {
                address: "a",
                workspace: { id: 1, name: "1" },
                monitor: 0,
                mapped: true,
                hidden: false,
                class: "org.example.Editor",
                initialClass: "org.example.Editor",
                title: "Editor",
                focusHistoryID: 0
            },
            {
                address: "b",
                workspace: { id: -44, name: "écriture" },
                monitor: 0,
                mapped: true,
                hidden: false,
                class: "org.example.Browser",
                initialClass: "org.example.Browser",
                title: "Browser",
                focusHistoryID: 1
            }
        ]);
    }

    function responseFor(command) {
        if (command === "j/status")
            return root.statusResponse();
        if (command === "j/workspaces")
            return root.workspaceResponse();
        if (command === "j/monitors")
            return root.monitorResponse();
        if (command === "j/clients")
            return root.clientResponse();
        return "";
    }

    function handleRequest(connection, command) {
        root.commandsReceived = root.commandsReceived.concat([command]);
        if (!root.hungRequest && command === "j/status") {
            root.hungRequest = true;
            return;
        }

        const response = root.responseFor(command);
        if (!root.check(response.length > 0, "unexpected request " + command))
            return;
        root.commandsCompleted = root.commandsCompleted.concat([command]);

        if (!root.injectedCycleEvent && command === "j/workspaces") {
            root.injectedCycleEvent = true;
            root.emitEvent("openwindow>>c,-44,org.example.Terminal,Terminal");
            for (let index = 0; index < 40; ++index) {
                root.emitEvent(
                    "windowtitlev2>>a,Editor " + String(index)
                );
            }
        }
        connection.respondInChunks(response);
    }

    function emitEvent(line) {
        if (!root.eventClient || !root.eventClient.connected)
            return false;
        root.eventClient.write(String(line) + "\n");
        root.eventClient.flush();
        return true;
    }

    function sequenceMatches(offset) {
        const expected = [
            "j/status",
            "j/workspaces",
            "j/monitors",
            "j/clients"
        ];
        if (root.commandsCompleted.length < offset + expected.length)
            return false;
        for (let index = 0; index < expected.length; ++index) {
            if (root.commandsCompleted[offset + index] !== expected[index])
                return false;
        }
        return true;
    }

    function validateLuaActions() {
        if (!root.check(source.usingLua, "Lua status was not published"))
            return false;
        if (!root.check(source.configProvider === "lua",
                "Lua provider was not published")) {
            return false;
        }
        if (!root.check(source.snapshot.workspaces[1].name === "écriture"
                && source.snapshot.workspaces[1].persistent,
                "real named/persistent workspace fields were not normalized")) {
            return false;
        }
        if (!root.check(source.activateWorkspace("DP-4", -44),
                "valid Lua workspace action was rejected")) {
            return false;
        }
        if (!root.check(source.activateWindow("DP-4", 1, "0xa"),
                "valid Lua window action was rejected")) {
            return false;
        }
        if (!root.check(!source.activateWorkspace("DP-9", -44),
                "cross-output workspace action was accepted")) {
            return false;
        }
        if (!root.check(!source.activateWindow("DP-4", -44, "0xb"),
                "inactive-workspace window action was accepted")) {
            return false;
        }
        return root.check(
            JSON.stringify(root.dispatches) === JSON.stringify([
                'hl.dsp.focus({ workspace = "name:écriture" })',
                'hl.dsp.focus({ window = "address:0xa" })'
            ]),
            "Lua dispatch commands were not exact"
        );
    }

    function validateLegacyActions() {
        if (!root.check(!source.usingLua,
                "legacy status did not replace Lua status")) {
            return false;
        }
        if (!root.check(source.configProvider === "legacy",
                "legacy provider was not published")) {
            return false;
        }
        if (!root.check(source.activateWorkspace("DP-4", -44),
                "valid legacy workspace action was rejected")) {
            return false;
        }
        if (!root.check(source.activateWindow("DP-4", 1, "a"),
                "valid legacy window action was rejected")) {
            return false;
        }
        const count = root.dispatches.length;
        return root.check(
            root.dispatches[count - 2] === "workspace name:écriture"
                && root.dispatches[count - 1]
                    === "focuswindow address:0xa",
            "legacy dispatch commands were not exact"
        );
    }

    function advance() {
        if (root.failed)
            return;

        if (root.phase === 0
                && source.actionsAvailable
                && source.snapshot.revision >= 2) {
            if (!root.check(root.hungRequest,
                    "request timeout fixture was not exercised")) {
                return;
            }
            if (!root.check(root.commandsReceived[0] === "j/status"
                    && root.commandsReceived[1] === "j/status",
                    "timed-out request was not recreated")) {
                return;
            }
            if (!root.check(root.commandsCompleted.length === 8
                    && root.sequenceMatches(0)
                    && root.sequenceMatches(4),
                    "atomic query cycles were out of order or starved")) {
                return;
            }
            if (!root.validateLuaActions())
                return;
            root.legacyStatus = true;
            if (!root.check(root.emitEvent("configreloaded>>"),
                    "could not emit legacy refresh event")) {
                return;
            }
            root.phase = 1;
            return;
        }

        if (root.phase === 1
                && source.actionsAvailable
                && source.snapshot.revision >= 3
                && source.configProvider === "legacy") {
            if (!root.check(root.sequenceMatches(8),
                    "legacy refresh query cycle was out of order")) {
                return;
            }
            if (!root.validateLegacyActions())
                return;
            if (!root.check(root.emitEvent("movewindowv2>>a,-44,écriture"),
                    "could not emit stale-action event")) {
                return;
            }
            root.phase = 2;
            return;
        }

        if (root.phase === 2) {
            if (!root.staleActionChecked && !source.actionsAvailable) {
                const dispatchCount = root.dispatches.length;
                if (!root.check(!source.activateWorkspace("DP-4", -44)
                        && root.dispatches.length === dispatchCount,
                        "stale action was dispatched")) {
                    return;
                }
                root.staleActionChecked = true;
            }
            if (root.staleActionChecked
                    && source.actionsAvailable
                    && source.snapshot.revision >= 4) {
                root.retainedSnapshot = source.snapshot;
                root.retainedRevision = source.snapshot.revision;
                eventServer.active = false;
                if (root.eventClient)
                    root.eventClient.connected = false;
                root.phase = 3;
            }
            return;
        }

        if (root.phase === 3 && !source.available) {
            if (!root.check(!source.actionsAvailable,
                    "actions remained available through event loss")) {
                return;
            }
            if (!root.check(source.snapshot === root.retainedSnapshot
                    && source.snapshot.revision === root.retainedRevision,
                    "last-known-good snapshot was discarded")) {
                return;
            }
            eventServer.active = true;
            root.phase = 4;
            return;
        }

        if (root.phase === 4
                && source.actionsAvailable
                && source.snapshot.revision > root.retainedRevision) {
            if (!root.check(source.available && source.live,
                    "source did not recover after event server returned")) {
                return;
            }
            if (!root.check(root.sequenceMatches(16),
                    "reconnect query cycle was out of order")) {
                return;
            }
            console.log("HYPRLAND_SOURCE_PASS");
            Qt.quit();
        }
    }

    HyprlandWorkspaceSource {
        id: source

        requestSocketPath: root.requestPath
        eventSocketPath: root.eventPath
        onDispatchRequested: command => {
            root.dispatches = root.dispatches.concat([command]);
        }
    }

    SocketServer {
        id: requestServer

        path: root.requestPath
        active: false

        handler: Socket {
            id: requestConnection

            property bool handled: false
            property string pendingTail: ""

            function respondInChunks(response) {
                const text = String(response);
                const split = Math.max(1, Math.floor(text.length / 2));
                requestConnection.pendingTail = text.slice(split);
                requestConnection.write(text.slice(0, split));
                requestConnection.flush();
                requestConnection.responseTailTimer.restart();
            }

            parser: StdioCollector {
                waitForEnd: false
                onDataChanged: {
                    const command = text.trim();
                    if (!requestConnection.handled && command.length > 0) {
                        requestConnection.handled = true;
                        root.handleRequest(requestConnection, command);
                    }
                }
            }

            property Timer responseTailTimer: Timer {
                id: responseTailTimer

                interval: 15
                repeat: false
                onTriggered: {
                    requestConnection.write(requestConnection.pendingTail);
                    requestConnection.flush();
                    requestConnection.pendingTail = "";
                }
            }
        }
    }

    SocketServer {
        id: eventServer

        path: root.eventPath
        active: false

        handler: Socket {
            id: acceptedEventClient

            Component.onCompleted: root.eventClient = acceptedEventClient

            onConnectionStateChanged: {
                if (!connected && root.eventClient === acceptedEventClient)
                    root.eventClient = null;
            }
        }
    }

    Timer {
        interval: 175
        running: true
        repeat: false
        onTriggered: eventServer.active = true
    }

    Timer {
        interval: 550
        running: true
        repeat: false
        onTriggered: requestServer.active = true
    }

    Timer {
        interval: 20
        running: true
        repeat: true
        onTriggered: root.advance()
    }

    Timer {
        interval: 18000
        running: true
        repeat: false
        onTriggered: root.check(false,
            "harness timed out in phase " + root.phase
                + "; eventServer=" + eventServer.active
                + "; requestServer=" + requestServer.active
                + "; live=" + source.live
                + "; available=" + source.available
                + "; lastError=" + source.lastError
                + "; eventAttempts=" + source._eventReconnectAttempt
                + "; received=" + JSON.stringify(root.commandsReceived)
                + "; completed=" + JSON.stringify(root.commandsCompleted)
        )
    }
}
