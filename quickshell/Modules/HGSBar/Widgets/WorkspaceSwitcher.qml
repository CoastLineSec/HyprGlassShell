import QtQuick
import Quickshell
import Quickshell.Hyprland
import qs.Common
import qs.Services

Item {
    id: root

    property bool isVertical: axis?.isVertical ?? false
    property var axis: null
    property string screenName: ""
    property real widgetHeight: 30
    property real barThickness: 48
    property var barConfig: null
    property var blurBarWindow: null
    property var hyprlandOverviewLoader: null
    property var parentScreen: null

    readonly property real itemSize: Math.max(18, widgetHeight * 0.76)
    readonly property real itemSpacing: Math.max(2, Theme.spacingXS * 0.5)
    readonly property int activeWorkspace: getActiveWorkspace()
    readonly property var workspaceList: buildWorkspaceList()

    implicitWidth: isVertical ? itemSize : workspaceRow.implicitWidth
    implicitHeight: isVertical ? workspaceColumn.implicitHeight : itemSize
    width: implicitWidth
    height: implicitHeight
    function workspaceValues() {
        const values = Hyprland.workspaces?.values || [];
        return Array.from(values).filter(ws => ws && ws.id > 0);
    }

    function workspaceMonitorName(ws) {
        return ws?.lastIpcObject?.monitor || ws?.monitor?.name || "";
    }

    function getEffectiveScreenName() {
        if (SettingsData.workspaceFollowFocus)
            return Hyprland.focusedWorkspace?.monitor?.name || screenName;
        return screenName;
    }

    function getActiveWorkspace() {
        if (SettingsData.workspaceFollowFocus)
            return Hyprland.focusedWorkspace?.id ?? 1;

        const target = getEffectiveScreenName();
        const monitors = Hyprland.monitors?.values || [];
        const monitor = Array.from(monitors).find(m => m?.name === target);
        return monitor?.activeWorkspace?.id ?? Hyprland.focusedWorkspace?.id ?? 1;
    }

    function cleanWorkspaceName(ws) {
        const raw = String(ws?.name ?? ws?.id ?? "");
        const id = String(ws?.id ?? "");
        if (raw === id)
            return id;
        if (raw.startsWith(id + " "))
            return raw.substring(id.length + 1);
        return raw;
    }

    function mapWorkspace(ws) {
        const occupied = (ws?.lastIpcObject?.windows ?? 0) > 0;
        return {
            "id": ws.id,
            "name": cleanWorkspaceName(ws),
            "occupied": occupied,
            "placeholder": false
        };
    }

    function buildWorkspaceList() {
        const target = getEffectiveScreenName();
        let list = workspaceValues();

        if (target && !SettingsData.workspaceFollowFocus) {
            const filtered = list.filter(ws => workspaceMonitorName(ws) === target);
            if (filtered.length > 0)
                list = filtered;
        }

        list = list.slice().sort((a, b) => a.id - b.id).map(mapWorkspace);

        if (list.length === 0) {
            list = [{
                "id": 1,
                "name": "1",
                "occupied": false,
                "placeholder": true
            }];
        }

        if (SettingsData.showWorkspacePadding) {
            const byId = new Map();
            for (const ws of list)
                byId.set(ws.id, ws);

            const maxId = Math.max(5, ...list.map(ws => ws.id));
            const padded = [];
            for (let id = 1; id <= maxId; id++) {
                padded.push(byId.get(id) || {
                    "id": id,
                    "name": String(id),
                    "occupied": false,
                    "placeholder": true
                });
            }
            list = padded;
        }

        return list;
    }

    function workspaceLabel(ws) {
        if (SettingsData.showWorkspaceName && ws.name && ws.name !== String(ws.id))
            return ws.name;
        if (SettingsData.showWorkspaceIndex)
            return String(ws.id);
        return "";
    }

    function activateWorkspace(ws) {
        if (!ws)
            return;
        HyprlandService.focusWorkspace(ws.id);
    }

    Component {
        id: workspaceDelegate

        Rectangle {
            id: pill

            required property var modelData

            readonly property bool active: modelData.id === root.activeWorkspace
            readonly property bool occupied: modelData.occupied
            readonly property string label: root.workspaceLabel(modelData)

            width: root.isVertical ? root.itemSize : Math.max(root.itemSize, labelText.implicitWidth + Theme.spacingS)
            height: root.itemSize
            radius: height / 2
            color: active ? Theme.primary : (occupied ? Theme.withAlpha(Theme.surfaceText, 0.20) : Theme.withAlpha(Theme.surfaceText, 0.10))
            border.width: active ? 1 : 0
            border.color: Theme.withAlpha(Theme.primary, 0.55)

            Behavior on color {
                ColorAnimation {
                    duration: Theme.shortDuration
                    easing.type: Easing.OutCubic
                }
            }

            Behavior on width {
                NumberAnimation {
                    duration: Theme.shortDuration
                    easing.type: Easing.OutCubic
                }
            }

            Text {
                id: labelText
                anchors.centerIn: parent
                visible: pill.label.length > 0
                text: pill.label
                color: pill.active ? Theme.onPrimary : Theme.surfaceText
                font.pixelSize: Math.max(10, root.itemSize * 0.42)
                font.weight: pill.active ? Font.DemiBold : Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Rectangle {
                anchors.centerIn: parent
                width: pill.active ? Math.max(10, parent.width * 0.42) : (pill.occupied ? Math.max(5, root.itemSize * 0.22) : Math.max(4, root.itemSize * 0.16))
                height: labelText.visible ? 0 : (pill.active ? Math.max(5, root.itemSize * 0.22) : Math.max(4, root.itemSize * 0.16))
                radius: height / 2
                visible: !labelText.visible
                color: pill.active ? Theme.onPrimary : Theme.surfaceText
                opacity: pill.active ? 0.92 : (pill.occupied ? 0.72 : 0.36)

                Behavior on width {
                    NumberAnimation {
                        duration: Theme.shortDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.activateWorkspace(pill.modelData)
            }
        }
    }

    Row {
        id: workspaceRow
        anchors.centerIn: parent
        spacing: root.itemSpacing
        visible: !root.isVertical

        Repeater {
            model: root.workspaceList
            delegate: workspaceDelegate
        }
    }

    Column {
        id: workspaceColumn
        anchors.centerIn: parent
        spacing: root.itemSpacing
        visible: root.isVertical

        Repeater {
            model: root.workspaceList
            delegate: workspaceDelegate
        }
    }
}
