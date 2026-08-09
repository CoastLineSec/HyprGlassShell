pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io

Scope {
    id: root

    required property string socketPath
    property int connectionTimeoutMs: 1000

    readonly property bool ready: eventSocket.connected

    signal connected()
    signal eventLine(string line)
    signal failed(string message)

    property bool _started: false
    property bool _finished: false
    property bool _wasConnected: false

    function start() {
        if (root._started || root._finished)
            return;
        root._started = true;
        connectionTimer.restart();
        eventSocket.connected = true;
    }

    function stop() {
        if (root._finished)
            return;
        root._finished = true;
        connectionTimer.stop();
        eventSocket.connected = false;
    }

    function _connectionChanged() {
        if (root._finished)
            return;

        if (eventSocket.connected) {
            connectionTimer.stop();
            root._wasConnected = true;
            root.connected();
        } else if (root._wasConnected) {
            root._finishFailure(
                qsTr("Hyprland event connection was lost.")
            );
        }
    }

    function _finishFailure(message) {
        if (root._finished)
            return;
        root._finished = true;
        connectionTimer.stop();
        eventSocket.connected = false;
        root.failed(String(message));
    }

    Component.onCompleted: Qt.callLater(root.start)

    Component.onDestruction: {
        root._finished = true;
        connectionTimer.stop();
        eventSocket.connected = false;
    }

    Socket {
        id: eventSocket

        path: root.socketPath

        parser: SplitParser {
            splitMarker: "\n"
            onRead: data => root.eventLine(data)
        }

        onConnectionStateChanged: root._connectionChanged()

        // Quickshell 0.3 exposes an unregistered QLocalSocket enum here.
        // qmllint disable signal-handler-parameters
        onError: root._finishFailure(
            qsTr("Hyprland event connection failed.")
        )
        // qmllint enable signal-handler-parameters
    }

    Timer {
        id: connectionTimer

        interval: root.connectionTimeoutMs
        repeat: false
        onTriggered: root._finishFailure(
            qsTr("Hyprland event connection timed out.")
        )
    }
}
