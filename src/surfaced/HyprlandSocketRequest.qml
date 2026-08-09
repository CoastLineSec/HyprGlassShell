pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io
import "HyprlandWorkspaceProtocol.js" as WorkspaceProtocol

Scope {
    id: root

    required property string socketPath
    required property string command
    property int timeoutMs: 1000
    property int maximumResponseCharacters: 4 * 1024 * 1024

    signal succeeded(string payload)
    signal failed(string message)

    property bool _started: false
    property bool _written: false
    property bool _finished: false
    property bool _responseComplete: false
    property string _response: ""

    function start() {
        if (root._started || root._finished)
            return;
        root._started = true;
        timeoutTimer.restart();
        requestSocket.connected = true;
    }

    function cancel() {
        if (root._finished)
            return;
        root._finished = true;
        timeoutTimer.stop();
        requestSocket.connected = false;
    }

    function _finishFailure(message) {
        if (root._finished)
            return;
        root._finished = true;
        timeoutTimer.stop();
        requestSocket.connected = false;
        root.failed(String(message));
    }

    function _replaceResponse(data) {
        if (root._finished || root._responseComplete)
            return;

        root._response = String(data || "");
        if (root._response.length > root.maximumResponseCharacters) {
            root._finishFailure(
                qsTr("Hyprland response exceeded the size limit.")
            );
        } else if (WorkspaceProtocol.completeResponse(
                root.command,
                root._response
            )) {
            root._responseComplete = true;
            requestSocket.connected = false;
        }
    }

    function _connectionChanged() {
        if (root._finished)
            return;

        if (requestSocket.connected) {
            root._written = true;
            requestSocket.write(root.command);
            requestSocket.flush();
            return;
        }

        if (!root._started || !root._written)
            return;
        if (!root._responseComplete) {
            root._finishFailure(
                qsTr("Hyprland returned an incomplete response.")
            );
            return;
        }

        root._finished = true;
        timeoutTimer.stop();
        root.succeeded(root._response);
    }

    function _socketError() {
        if (root._responseComplete) {
            requestSocket.connected = false;
            return;
        }
        root._finishFailure(qsTr("Hyprland request failed."));
    }

    Component.onCompleted: Qt.callLater(root.start)

    Component.onDestruction: {
        root._finished = true;
        timeoutTimer.stop();
        requestSocket.connected = false;
    }

    Socket {
        id: requestSocket

        path: root.socketPath

        parser: StdioCollector {
            waitForEnd: false
            onDataChanged: root._replaceResponse(text)
        }

        onConnectionStateChanged: root._connectionChanged()

        // Quickshell 0.3 exposes an unregistered QLocalSocket enum here.
        // qmllint disable signal-handler-parameters
        onError: root._socketError()
        // qmllint enable signal-handler-parameters
    }

    Timer {
        id: timeoutTimer

        interval: root.timeoutMs
        repeat: false
        onTriggered: root._finishFailure(qsTr("Hyprland request timed out."))
    }
}
