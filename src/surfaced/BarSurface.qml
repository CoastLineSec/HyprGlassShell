pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Wayland
import HyprShelld.Client
import HyprShelld.UI
import "components" as Components

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
    required property var workspaceSource
    readonly property int outerMargin: 12
    readonly property int sideMargin: 12
    readonly property int inwardSpacing: 8
    readonly property string outputName: root.modelData
        ? root.modelData.name
        : ""
    readonly property var startComponentInstances: {
        ComponentRuntimeClient.planRevision;
        ComponentRuntimeClient.planDigest;
        ComponentRuntimeClient.usingFallback;
        return ComponentRuntimeClient.barInstances(
            "main",
            root.outputName,
            "start"
        );
    }
    readonly property var centerComponentInstances: {
        ComponentRuntimeClient.planRevision;
        ComponentRuntimeClient.planDigest;
        ComponentRuntimeClient.usingFallback;
        return ComponentRuntimeClient.barInstances(
            "main",
            root.outputName,
            "center"
        );
    }
    readonly property var endComponentInstances: {
        ComponentRuntimeClient.planRevision;
        ComponentRuntimeClient.planDigest;
        ComponentRuntimeClient.usingFallback;
        return ComponentRuntimeClient.barInstances(
            "main",
            root.outputName,
            "end"
        );
    }

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
        startComponent: Component {
            Components.BarComponentHost {
                instances: root.startComponentInstances
                outputName: root.outputName
                workspaceSource: root.workspaceSource
                interactive: root.workspaceSource
                    ? root.workspaceSource.actionsAvailable
                    : false
                keyboardNavigationEnabled: false
                height: bar.height
            }
        }
        centerComponent: Component {
            Components.BarComponentHost {
                instances: root.centerComponentInstances
                outputName: root.outputName
                workspaceSource: root.workspaceSource
                interactive: root.workspaceSource
                    ? root.workspaceSource.actionsAvailable
                    : false
                keyboardNavigationEnabled: false
                height: bar.height
            }
        }
        endComponent: Component {
            Components.BarComponentHost {
                instances: root.endComponentInstances
                outputName: root.outputName
                workspaceSource: root.workspaceSource
                interactive: root.workspaceSource
                    ? root.workspaceSource.actionsAvailable
                    : false
                keyboardNavigationEnabled: false
                height: bar.height
            }
        }
        shellDegraded: root.shellDegraded
        healthSummary: root.healthSummary
        failureNoticeVisible: root.failureNoticeActive
            && root.modelData
            && root.modelData.name === root.failureNoticeScreenName
        failureNoticeText: root.failureNoticeText
    }
}
