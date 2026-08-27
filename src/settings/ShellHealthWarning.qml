pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Frame {
    id: root

    objectName: "shellHealthWarning"

    required property bool coordinatorAvailable
    required property bool coordinatorHealthy
    required property var coordinatorFailedUnits
    property bool restartBusy: false
    property string restartingUnit: ""
    property string restartErrorUnit: ""
    property string restartError: ""

    property bool fallbackActive: false
    property bool fallbackAvailable: false
    property bool fallbackBusy: false
    property string targetState: "unknown"
    property string coordinatorState: "unknown"
    property string configurationState: "unknown"
    property string componentManagerState: "unknown"
    property string compositorState: "unknown"
    property string surfaceState: "unknown"

    readonly property var failedComponentUnits: {
        const units = [];
        if (coordinatorAvailable) {
            for (const unitName of coordinatorFailedUnits) {
                if (unitName === "hyprshelld-configd.service"
                        || unitName === "hyprshelld-componentd.service"
                        || unitName === "hyprshelld-compositord.service"
                        || unitName === "hyprshelld-surfaced.service")
                    units.push(unitName);
            }
            return units;
        }

        if (!fallbackAvailable)
            return units;

        if (coordinatorState === "failed")
            units.push("hyprshelld.service");
        if (configurationState === "failed")
            units.push("hyprshelld-configd.service");
        if (componentManagerState === "failed")
            units.push("hyprshelld-componentd.service");
        if (compositorState === "failed")
            units.push("hyprshelld-compositord.service");
        if (surfaceState === "failed")
            units.push("hyprshelld-surfaced.service");
        return units;
    }
    readonly property int failedComponentCount: failedComponentUnits.length
    readonly property bool hasComponentFailures: failedComponentCount > 0
    readonly property bool warningVisible: coordinatorAvailable
        ? !coordinatorHealthy
        : fallbackActive
    readonly property bool errorTone: hasComponentFailures
    readonly property string warningTitle: {
        if (hasComponentFailures)
            return failedComponentCount === 1
                ? qsTr("A desktop component needs attention")
                : qsTr("%1 desktop components need attention").arg(failedComponentCount);
        if (fallbackBusy && !fallbackAvailable)
            return qsTr("Checking desktop services");
        if (fallbackAvailable)
            return targetState === "inactive"
                ? qsTr("HyprShelld is not running")
                : qsTr("Shell health service unavailable");
        return qsTr("Service status unavailable");
    }
    readonly property string warningDescription: {
        if (coordinatorAvailable)
            return qsTr("Automatic recovery stopped after repeated attempts. Use Restart below to try again.");
        if (fallbackBusy && !fallbackAvailable)
            return qsTr("Settings is checking the current service state.");
        if (fallbackAvailable && targetState === "inactive")
            return qsTr("Settings remains available while the desktop services are stopped.");
        if (fallbackAvailable)
            return qsTr("Settings is reading service state directly from systemd. Restart controls will return when the health service reconnects.");
        return qsTr("Settings cannot verify the state of the desktop services right now.");
    }

    signal restartRequested(string unitName)

    function friendlyName(unitName) {
        if (unitName === "hyprshelld.service")
            return qsTr("Shell health");
        if (unitName === "hyprshelld-configd.service")
            return qsTr("Settings service");
        if (unitName === "hyprshelld-componentd.service")
            return qsTr("Component manager");
        if (unitName === "hyprshelld-compositord.service")
            return qsTr("Compositor settings");
        if (unitName === "hyprshelld-surfaced.service")
            return qsTr("Desktop shell");
        return qsTr("Shell component");
    }

    function componentDescription(unitName) {
        if (unitName === "hyprshelld.service")
            return qsTr("Monitors desktop components and reports persistent failures.");
        if (unitName === "hyprshelld-configd.service")
            return qsTr("Saves and applies desktop settings.");
        if (unitName === "hyprshelld-componentd.service")
            return qsTr("Provides the installed component catalog.");
        if (unitName === "hyprshelld-compositord.service")
            return qsTr("Validates and applies managed compositor configuration.");
        if (unitName === "hyprshelld-surfaced.service")
            return qsTr("Displays the bar and other desktop surfaces.");
        return qsTr("Provides part of the HyprShelld desktop.");
    }

    visible: warningVisible
    padding: 18
    Accessible.role: Accessible.AlertMessage
    Accessible.name: warningTitle
    Accessible.description: warningDescription
    Accessible.ignored: !visible

    background: Rectangle {
        color: root.errorTone ? ShellTheme.errorContainer : ShellTheme.warningContainer
        radius: 14
        border.color: root.errorTone ? ShellTheme.errorOutline : ShellTheme.warningOutline
    }

    contentItem: ColumnLayout {
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                radius: 15
                color: root.errorTone ? ShellTheme.error : ShellTheme.warning

                Label {
                    anchors.centerIn: parent
                    text: "!"
                    color: ShellTheme.onPrimary
                    font.pixelSize: 17
                    font.weight: Font.Bold
                    Accessible.ignored: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Label {
                    Layout.fillWidth: true
                    text: root.warningTitle
                    color: root.errorTone ? ShellTheme.onErrorContainer : ShellTheme.onWarningContainer
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    Accessible.ignored: true
                }

                Label {
                    Layout.fillWidth: true
                    text: root.warningDescription
                    color: root.palette.text
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    Accessible.ignored: true
                }
            }
        }

        Repeater {
            model: root.failedComponentUnits

            delegate: Rectangle {
                id: componentRow

                required property string modelData

                Layout.fillWidth: true
                implicitHeight: componentContent.implicitHeight + 20
                radius: 11
                color: Qt.rgba(
                    root.palette.text.r,
                    root.palette.text.g,
                    root.palette.text.b,
                    0.04
                )
                border.color: root.palette.mid

                RowLayout {
                    id: componentContent

                    anchors {
                        fill: parent
                        leftMargin: 13
                        rightMargin: 13
                        topMargin: 10
                        bottomMargin: 10
                    }
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: root.friendlyName(componentRow.modelData)
                            color: root.palette.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.componentDescription(componentRow.modelData)
                            color: root.palette.placeholderText
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                        }
                    }

                    Button {
                        id: restartButton

                        objectName: "restartButton-" + componentRow.modelData
                        visible: root.coordinatorAvailable
                        enabled: !root.restartBusy
                        text: root.restartingUnit === componentRow.modelData
                            ? qsTr("Restarting…")
                            : qsTr("Restart")
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: qsTr("Restart %1").arg(
                            root.friendlyName(componentRow.modelData)
                        )
                        onClicked: root.restartRequested(componentRow.modelData)
                        Keys.onReturnPressed: event => {
                            if (restartButton.enabled)
                                restartButton.clicked();
                            event.accepted = true;
                        }
                        Keys.onEnterPressed: event => {
                            if (restartButton.enabled)
                                restartButton.clicked();
                            event.accepted = true;
                        }
                    }

                    Label {
                        visible: !root.coordinatorAvailable
                        text: qsTr("Needs attention")
                        color: ShellTheme.onErrorContainer
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        Label {
            objectName: "restartError"
            Layout.fillWidth: true
            visible: root.restartError.length > 0
                && (root.restartErrorUnit.length === 0
                    || root.failedComponentUnits.indexOf(
                        root.restartErrorUnit
                    ) >= 0)
            text: root.restartErrorUnit.length > 0
                ? qsTr("%1 could not be restarted. %2")
                    .arg(root.friendlyName(root.restartErrorUnit))
                    .arg(root.restartError)
                : qsTr("Restart could not be requested. %1").arg(
                    root.restartError
                )
            color: ShellTheme.onErrorContainer
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
            Accessible.ignored: !visible
        }
    }
}
