pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property bool recoveryAvailable: false
    property bool operationBusy: false
    property string busyOperation: ""
    property string settingsAreaName: ""
    property string warningObjectName: ""
    property string cancelObjectName: ""
    property string confirmObjectName: ""
    property real minimumTargetSize: 44
    property bool requestSubmitted: false

    signal recoveryRequested()

    title: qsTr("Restore the last working compositor configuration?")
    modal: true
    width: Math.min(
        620,
        Math.max(280, parent ? parent.width - 48 : 620)
    )
    height: Math.min(
        500,
        Math.max(300, parent ? parent.height - 48 : 500)
    )
    closePolicy: Popup.CloseOnEscape

    onOpened: {
        requestSubmitted = false;
        Qt.callLater(function() {
            if (root.opened)
                cancelButton.forceActiveFocus();
        });
    }

    onRecoveryAvailableChanged: {
        if (root.opened && !root.recoveryAvailable)
            root.close();
    }

    onOperationBusyChanged: {
        if (root.opened && root.operationBusy)
            root.close();
    }

    contentItem: ScrollView {
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: parent.width
            spacing: 16

            Label {
                Layout.fillWidth: true
                text: qsTr("This recovery affects the whole compositor")
                color: root.palette.text
                font.pixelSize: 20
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Frame {
                Layout.fillWidth: true
                padding: 14

                background: Rectangle {
                    color: "#382125"
                    radius: 10
                    border.color: "#8bfb7185"
                }

                Label {
                    objectName: root.warningObjectName
                    anchors.fill: parent
                    text: qsTr("Recovery is not limited to %1. It replaces every pending compositor setting, including display and other settings, with the last verified working snapshot.").arg(
                        root.settingsAreaName
                    )
                    color: "#ffb8c3"
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("HyprShelld creates a new monotonic desired-state revision from the last working snapshot, reloads Hyprland, and verifies that revision. Canceling leaves desired files and the running compositor unchanged.")
                color: root.palette.placeholderText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.name: text
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            id: cancelButton

            objectName: root.cancelObjectName
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: qsTr("Cancel")
            enabled: !root.operationBusy
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            Accessible.name: qsTr("Cancel without changing compositor settings")

            onClicked: root.reject()
        }

        Button {
            objectName: root.confirmObjectName
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: root.busyOperation === "recover"
                ? qsTr("Restoring…")
                : qsTr("Restore whole configuration")
            enabled: root.opened && root.recoveryAvailable
                && !root.operationBusy && !root.requestSubmitted
            highlighted: true
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            Accessible.name: qsTr("Restore the last working whole-compositor configuration")

            onClicked: {
                if (!root.opened || !root.recoveryAvailable
                        || root.operationBusy || root.requestSubmitted) {
                    return;
                }
                root.requestSubmitted = true;
                root.recoveryRequested();
                root.close();
            }
        }
    }
}
