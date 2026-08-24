pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property var gestures: []
    property bool controlsEnabled: false
    property bool draftValid: true
    property bool canAddGesture: false
    property real minimumTargetSize: 44
    property var compatibilityForId: function(id) { return null; }
    property var canMoveGesture: function(id, offset) { return false; }

    signal addRequested()
    signal editRequested(string id)
    signal moveRequested(string id, int offset)
    signal removeRequested(string id)

    spacing: 12

    function actionTitle(action) {
        if (!action || typeof action.type !== "string")
            return qsTr("Unknown action");
        const labels = {
            close: qsTr("Close"),
            cursorZoom: qsTr("Cursor zoom"),
            float: qsTr("Floating state"),
            fullscreen: qsTr("Fullscreen state"),
            move: qsTr("Move window"),
            resize: qsTr("Resize window"),
            scrollMove: qsTr("Move scrolling window"),
            special: qsTr("Special workspace"),
            unset: qsTr("Unset matching gesture"),
            workspace: qsTr("Navigate workspaces")
        };
        return labels[action.type] || qsTr("Unknown action");
    }

    function actionDetail(action) {
        if (!action || typeof action.type !== "string")
            return "";
        if (action.type === "special")
            return qsTr("Special workspace %1").arg(action.workspace);
        if (action.type === "float")
            return qsTr("Mode: %1").arg(action.mode);
        if (action.type === "fullscreen")
            return qsTr("Mode: %1").arg(action.mode);
        if (action.type === "cursorZoom") {
            return qsTr("Mode: %1 · level %2")
                .arg(action.mode).arg(action.zoomLevel);
        }
        return "";
    }

    function directionTitle(direction) {
        const labels = {
            swipe: qsTr("Swipe"),
            left: qsTr("Swipe left"),
            right: qsTr("Swipe right"),
            up: qsTr("Swipe up"),
            down: qsTr("Swipe down"),
            horizontal: qsTr("Horizontal swipe"),
            vertical: qsTr("Vertical swipe"),
            pinch: qsTr("Pinch"),
            pinchIn: qsTr("Pinch in"),
            pinchOut: qsTr("Pinch out")
        };
        return labels[direction] || direction;
    }

    function modifierSummary(modifiers) {
        if (!Array.isArray(modifiers) || modifiers.length === 0)
            return qsTr("No modifiers");
        const labels = {
            shift: qsTr("Shift"), caps: qsTr("Caps Lock"),
            ctrl: qsTr("Ctrl"), alt: qsTr("Alt"),
            mod2: qsTr("Mod2"), mod3: qsTr("Mod3"),
            super: qsTr("Super"), mod5: qsTr("Mod5")
        };
        return modifiers.map(value => labels[value] || value).join(" + ");
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 3

            Label {
                Layout.fillWidth: true
                text: qsTr("Gesture bindings")
                color: root.palette.text
                font.pixelSize: 17
                font.weight: Font.DemiBold
                textFormat: Text.PlainText
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Order matters: Hyprland uses the first matching finger, direction, and modifier pattern.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
        }

        Label {
            text: qsTr("%1 / 64").arg(
                Array.isArray(root.gestures) ? root.gestures.length : 0
            )
            color: root.palette.placeholderText
            textFormat: Text.PlainText
            Accessible.name: qsTr("%1 of 64 gesture records").arg(
                Array.isArray(root.gestures) ? root.gestures.length : 0
            )
        }
    }

    Button {
        objectName: "addGestureButton"
        Layout.fillWidth: true
        implicitHeight: Math.max(
            root.minimumTargetSize,
            implicitBackgroundHeight,
            implicitContentHeight + topPadding + bottomPadding
        )
        text: qsTr("Add gesture")
        enabled: root.controlsEnabled && root.draftValid
            && root.canAddGesture
        Accessible.name: qsTr("Add a gesture binding")

        onClicked: root.addRequested()
    }

    Label {
        Layout.fillWidth: true
        visible: Array.isArray(root.gestures) && root.gestures.length === 0
        text: qsTr("No managed gesture bindings are saved.")
        color: root.palette.placeholderText
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
    }

    Repeater {
        model: Array.isArray(root.gestures) ? root.gestures : []

        delegate: Frame {
            id: gestureCard

            required property int index
            required property var modelData
            readonly property var compatibility:
                root.compatibilityForId(String(modelData.id))
            readonly property bool editable:
                compatibility === null || compatibility.editable === true

            objectName: "gestureCard" + index
            Layout.fillWidth: true
            padding: 14

            background: Rectangle {
                color: root.palette.base
                radius: 14
                border.color: gestureCard.editable
                    ? root.palette.mid : "#8bf6ad55"
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 9

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: root.actionTitle(gestureCard.modelData.action)
                            color: root.palette.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("%1 fingers · %2 · %3")
                                .arg(gestureCard.modelData.fingers)
                                .arg(root.directionTitle(
                                    gestureCard.modelData.direction
                                ))
                                .arg(root.modifierSummary(
                                    gestureCard.modelData.modifiers
                                ))
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Record %1 · scale %2")
                                .arg(gestureCard.modelData.id)
                                .arg(gestureCard.modelData.scale)
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: root.actionDetail(
                                gestureCard.modelData.action
                            )
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }

                    Label {
                        visible: !gestureCard.editable
                        Layout.preferredWidth: Math.min(
                            180, gestureCard.width * 0.42
                        )
                        Layout.maximumWidth: Math.min(
                            180, gestureCard.width * 0.42
                        )
                        text: qsTr("Compatibility record — read only")
                        color: "#ffd5a1"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.name: text
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: !gestureCard.editable
                        && gestureCard.compatibility !== null
                        && gestureCard.compatibility.reason.length > 0
                    text: gestureCard.compatibility !== null
                        ? gestureCard.compatibility.reason : ""
                    color: "#ffd5a1"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    visible: gestureCard.modelData.disableInhibit === true
                    text: qsTr("Allowed while compositor gestures are inhibited")
                    color: "#ffd5a1"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Flow {
                    Layout.fillWidth: true
                    Layout.preferredHeight: childrenRect.height
                    spacing: 8

                    Button {
                        objectName: "editGestureButton" + gestureCard.index
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        visible: gestureCard.editable
                        text: qsTr("Edit")
                        enabled: root.controlsEnabled
                        Accessible.name: qsTr("Edit %1 gesture")
                            .arg(root.actionTitle(
                                gestureCard.modelData.action
                            ))

                        onClicked: root.editRequested(
                            String(gestureCard.modelData.id)
                        )
                    }

                    Button {
                        objectName: "moveGestureUpButton" + gestureCard.index
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Move up")
                        enabled: root.controlsEnabled
                            && root.canMoveGesture(
                                String(gestureCard.modelData.id), -1
                            )
                        Accessible.name: qsTr("Move gesture up")

                        onClicked: root.moveRequested(
                            String(gestureCard.modelData.id), -1
                        )
                    }

                    Button {
                        objectName: "moveGestureDownButton" + gestureCard.index
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Move down")
                        enabled: root.controlsEnabled
                            && root.canMoveGesture(
                                String(gestureCard.modelData.id), 1
                            )
                        Accessible.name: qsTr("Move gesture down")

                        onClicked: root.moveRequested(
                            String(gestureCard.modelData.id), 1
                        )
                    }

                    Button {
                        objectName: "removeGestureButton" + gestureCard.index
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Remove")
                        enabled: root.controlsEnabled
                        Accessible.name: qsTr("Remove gesture from the Input draft")

                        onClicked: root.removeRequested(
                            String(gestureCard.modelData.id)
                        )
                    }
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: !root.draftValid
        text: qsTr("This ordered gesture draft is not valid. Remove the orphaned compatibility record or restore the current Input draft before saving.")
        color: "#ffb8c3"
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }
}
