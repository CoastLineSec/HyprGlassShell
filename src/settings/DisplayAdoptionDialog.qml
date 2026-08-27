pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Dialog {
    id: root

    property bool eligible: false
    property bool operationBusy: false
    property bool requestSubmitted: false

    readonly property real minimumTargetSize: 44

    signal adoptionConfirmed()

    objectName: "displayAdoptionDialog"
    title: qsTr("Let HyprShelld manage Hyprland?")
    modal: true
    width: Math.min(
        640,
        Math.max(280, parent ? parent.width - 48 : 640)
    )
    height: Math.min(
        620,
        Math.max(320, parent ? parent.height - 48 : 620)
    )
    closePolicy: Popup.CloseOnEscape

    function confirmAdoption() {
        // Eligibility is deliberately checked here as well as by the button.
        // The service projection can change while this modal is open.
        if (!root.opened || !root.eligible || root.operationBusy
                || root.requestSubmitted) {
            return;
        }
        root.requestSubmitted = true;
        root.adoptionConfirmed();
        root.close();
    }

    onOpened: {
        root.requestSubmitted = false;
        Qt.callLater(function() {
            if (root.opened)
                cancelButton.forceActiveFocus();
        });
    }

    onEligibleChanged: {
        // Losing any prerequisite dismisses the stale prompt. Closing it is
        // intentionally side-effect free.
        if (root.opened && !root.eligible)
            root.close();
    }

    contentItem: ScrollView {
        objectName: "displayAdoptionScrollView"
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            objectName: "displayAdoptionContent"
            width: parent.width
            spacing: 16

            Label {
                objectName: "displayAdoptionHeading"
                Layout.fillWidth: true
                text: qsTr("Review this takeover before continuing")
                color: root.palette.text
                font.pixelSize: 20
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Frame {
                objectName: "displayAdoptionWarning"
                Layout.fillWidth: true
                padding: 14

                background: Rectangle {
                    color: ShellTheme.warningContainer
                    radius: 10
                    border.color: ShellTheme.warningOutline
                }

                Label {
                    anchors.fill: parent
                    text: qsTr("This changes which file owns your Hyprland configuration.")
                    color: ShellTheme.onWarningContainer
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }
            }

            Label {
                objectName: "displayAdoptionExplanation"
                Layout.fillWidth: true
                text: qsTr("HyprShelld will validate a generated configuration, replace the active Hyprland entrypoint, and reload Hyprland. Management is committed only after the reload is verified.\n\nYour existing entrypoint settings are not imported. If hyprland.lua exists, the exact original is preserved privately for recovery; if it does not exist, that absence is recorded. Other legacy configuration files stay where they are, but the managed entrypoint no longer selects them after takeover.\n\nHyprShelld preserves an existing user-custom.lua; if that file is absent, HyprShelld creates it. This user-owned file is loaded last, after every managed module.\n\nThere is currently no user-facing action to stop managing the entrypoint. Canceling leaves your files and running compositor unchanged.")
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

            objectName: "cancelDisplayAdoptionButton"
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: qsTr("Cancel")
            enabled: !root.operationBusy
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            Accessible.name: qsTr("Cancel without changing Hyprland")

            onClicked: root.reject()
        }

        Button {
            objectName: "confirmDisplayAdoptionButton"
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: root.operationBusy
                ? qsTr("Starting management…")
                : qsTr("Start managing Hyprland")
            enabled: root.opened && root.eligible
                && !root.operationBusy && !root.requestSubmitted
            highlighted: true
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            Accessible.name: qsTr("Confirm and let HyprShelld manage Hyprland")

            onClicked: root.confirmAdoption()
        }
    }
}
