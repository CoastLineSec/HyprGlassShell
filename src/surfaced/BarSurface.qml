import QtQuick
import Quickshell
import Quickshell.Wayland
import HyprShelld.Client
import HyprShelld.UI

// PanelWindow and margins are supplied by Quickshell's runtime backend.
PanelWindow { // qmllint disable uncreatable-type
    id: root

    property var modelData
    required property date currentTime
    required property bool shellDegraded
    required property string healthSummary
    required property bool failureNoticeActive
    required property string failureNoticeScreenName
    required property string failureNoticeText
    readonly property int outerMargin: 12
    readonly property int sideMargin: 12
    readonly property int inwardSpacing: 8

    screen: modelData
    color: "transparent"
    aboveWindows: true
    implicitHeight: ConfigClient.barHeight
    exclusionMode: ExclusionMode.Auto

    anchors {
        top: true
        left: true
        right: true
    }

    // qmllint disable unqualified
    // qmllint disable unresolved-type
    margins {
        top: root.outerMargin
        bottom: root.inwardSpacing
        left: root.sideMargin
        right: root.sideMargin
    }
    // qmllint enable unresolved-type
    // qmllint enable unqualified

    surfaceFormat.opaque: false

    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "hyprshelld:bar"
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None

    mask: Region {
        item: bar
        radius: bar.cornerRadius
    }

    Bar {
        id: bar

        anchors.fill: parent
        barHeight: ConfigClient.barHeight
        currentTime: root.currentTime
        screenName: root.modelData ? root.modelData.name : ""
        configurationAvailable: ConfigClient.available
        shellDegraded: root.shellDegraded
        healthSummary: root.healthSummary
        failureNoticeVisible: root.failureNoticeActive
            && root.modelData
            && root.modelData.name === root.failureNoticeScreenName
        failureNoticeText: root.failureNoticeText
    }
}
