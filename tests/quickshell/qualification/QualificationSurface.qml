import QtQuick
import Quickshell

PanelWindow {
    id: root

    property var modelData
    property bool popoutOpen: false

    screen: modelData
    color: "transparent"
    aboveWindows: true
    focusable: popoutOpen
    exclusionMode: ExclusionMode.Ignore
    implicitHeight: bar.height + 12 + popout.expandedHeight

    anchors {
        top: true
        left: true
        right: true
    }

    margins {
        top: 12
        left: 12
        right: 12
    }

    surfaceFormat.opaque: false

    mask: Region {
        Region {
            item: bar
            radius: 16
        }

        Region {
            item: popout
            radius: 18
        }
    }

    QualificationBar {
        id: bar

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        expanded: root.popoutOpen
        screenName: root.modelData.name

        onToggleRequested: root.popoutOpen = !root.popoutOpen
    }

    QualificationPopout {
        id: popout

        anchors {
            top: bar.bottom
            topMargin: 12
            horizontalCenter: parent.horizontalCenter
        }

        width: Math.min(440, root.width)
        expanded: root.popoutOpen

        onCloseRequested: root.popoutOpen = false
    }
}
