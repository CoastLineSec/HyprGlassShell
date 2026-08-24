pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    property var gesture: null
    property bool controlsEnabled: false
    property string issue: ""
    property real minimumTargetSize: 44

    signal recordModified(var record)
    signal closeRequested()
    signal removeRequested(string id)

    padding: 18

    background: Rectangle {
        color: root.palette.base
        radius: 16
        border.color: root.issue.length > 0 ? "#8bfb7185" : root.palette.mid
    }

    function clone(value) {
        try {
            return JSON.parse(JSON.stringify(value));
        } catch (error) {
            return null;
        }
    }

    function actionType() {
        return root.gesture && root.gesture.action
            && typeof root.gesture.action.type === "string"
            ? root.gesture.action.type : "workspace";
    }

    function actionMode() {
        return root.gesture && root.gesture.action
            && typeof root.gesture.action.mode === "string"
            ? root.gesture.action.mode : "";
    }

    function isPinchDirection(direction) {
        return ["pinch", "pinchIn", "pinchOut"].includes(direction);
    }

    function directionChoices() {
        const swipe = [
            {value: "swipe", label: qsTr("Swipe — any direction")},
            {value: "left", label: qsTr("Swipe left")},
            {value: "right", label: qsTr("Swipe right")},
            {value: "up", label: qsTr("Swipe up")},
            {value: "down", label: qsTr("Swipe down")},
            {value: "horizontal", label: qsTr("Horizontal swipe")},
            {value: "vertical", label: qsTr("Vertical swipe")}
        ];
        const pinch = [
            {value: "pinch", label: qsTr("Pinch — either direction")},
            {value: "pinchIn", label: qsTr("Pinch in")},
            {value: "pinchOut", label: qsTr("Pinch out")}
        ];
        if (root.actionType() === "cursorZoom"
                && root.actionMode() === "live") {
            return pinch;
        }
        if (root.actionType() === "scrollMove")
            return swipe;
        return swipe.concat(pinch);
    }

    function directionIndex() {
        const direction = root.gesture ? root.gesture.direction : "swipe";
        const index = root.directionChoices().findIndex(
            choice => choice.value === direction
        );
        return index >= 0 ? index : 0;
    }

    function actionIndex() {
        const values = [
            "close", "cursorZoom", "float", "fullscreen", "move", "resize",
            "scrollMove", "special", "workspace"
        ];
        const index = values.indexOf(root.actionType());
        return index >= 0 ? index : 8;
    }

    function modify(mutator) {
        if (!root.controlsEnabled || !root.gesture)
            return;
        const candidate = root.clone(root.gesture);
        if (!candidate)
            return;
        mutator(candidate);
        root.recordModified(candidate);
    }

    function setAction(type) {
        root.modify(function(candidate) {
            if (type === "cursorZoom") {
                candidate.action = {
                    type: "cursorZoom", zoomLevel: 2, mode: "toggle"
                };
            } else if (type === "float") {
                candidate.action = {type: "float", mode: "toggle"};
            } else if (type === "fullscreen") {
                candidate.action = {
                    type: "fullscreen", mode: "fullscreen"
                };
            } else if (type === "special") {
                candidate.action = {type: "special", workspace: "special"};
            } else {
                candidate.action = {type};
            }
            if (type === "scrollMove"
                    && root.isPinchDirection(candidate.direction)) {
                candidate.direction = "swipe";
            }
            if (root.isPinchDirection(candidate.direction))
                candidate.scale = 1;
        });
    }

    function setDirection(direction) {
        root.modify(function(candidate) {
            candidate.direction = direction;
            if (root.isPinchDirection(direction))
                candidate.scale = 1;
        });
    }

    function setModifier(modifier, selected) {
        root.modify(function(candidate) {
            const order = [
                "shift", "caps", "ctrl", "alt",
                "mod2", "mod3", "super", "mod5"
            ];
            const chosen = new Set(candidate.modifiers);
            if (selected)
                chosen.add(modifier);
            else
                chosen.delete(modifier);
            candidate.modifiers = order.filter(value => chosen.has(value));
        });
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Edit gesture")
                    color: root.palette.text
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: root.gesture ? String(root.gesture.id) : ""
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }
            }

            Button {
                objectName: "closeGestureEditorButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Done")
                enabled: root.controlsEnabled && root.issue.length === 0
                Accessible.name: qsTr("Finish editing this gesture")

                onClicked: root.closeRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Fingers")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Use between two and nine fingers.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            SpinBox {
                objectName: "gestureFingers"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                from: 2
                to: 9
                value: root.gesture ? Number(root.gesture.fingers) : 3
                editable: true
                enabled: root.controlsEnabled
                Accessible.name: qsTr("Gesture finger count")

                onValueModified: root.modify(function(candidate) {
                    candidate.fingers = value;
                })
            }
        }

        SettingsSelectRow {
            Layout.fillWidth: true
            title: qsTr("Direction")
            description: qsTr("Choose a swipe axis, an exact swipe direction, or a pinch direction supported by this action.")
            model: root.directionChoices().map(choice => choice.label)
            currentIndex: root.directionIndex()
            enabled: root.controlsEnabled
            controlObjectName: "gestureDirection"
            accessibleName: qsTr("Gesture direction")
            minimumTargetSize: root.minimumTargetSize

            onValueModified: index => {
                const choices = root.directionChoices();
                if (index >= 0 && index < choices.length)
                    root.setDirection(choices[index].value);
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: root.gesture
                && !root.isPinchDirection(root.gesture.direction)
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Gesture scale")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Scale swipe movement from 0.1 through 10. Pinch gestures always use 1.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            RuleDecimalField {
                objectName: "gestureScale"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.maximumWidth: Math.max(0, root.width - root.leftPadding - root.rightPadding)
                value: root.gesture ? root.gesture.scale : 1
                minimumValue: 0.1
                maximumValue: 10
                enabled: root.controlsEnabled
                accessibleName: qsTr("Gesture movement scale")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: value => root.modify(function(candidate) {
                    candidate.scale = value;
                })
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("Required modifiers")
                color: root.palette.text
                font.pixelSize: 14
                font.weight: Font.Medium
                textFormat: Text.PlainText
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("The gesture matches only while every selected modifier is held.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Flow {
                Layout.fillWidth: true
                Layout.preferredHeight: childrenRect.height
                spacing: 8

                Repeater {
                    model: [
                        {value: "shift", label: qsTr("Shift")},
                        {value: "caps", label: qsTr("Caps Lock")},
                        {value: "ctrl", label: qsTr("Ctrl")},
                        {value: "alt", label: qsTr("Alt")},
                        {value: "mod2", label: qsTr("Mod2")},
                        {value: "mod3", label: qsTr("Mod3")},
                        {value: "super", label: qsTr("Super")},
                        {value: "mod5", label: qsTr("Mod5")}
                    ]

                    delegate: CheckBox {
                        id: modifierCheck

                        required property int index
                        required property var modelData

                        objectName: "gestureModifier" + index
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: modelData.label
                        checked: root.gesture
                            && Array.isArray(root.gesture.modifiers)
                            && root.gesture.modifiers.includes(modelData.value)
                        enabled: root.controlsEnabled
                        Accessible.name: qsTr("Require %1").arg(modelData.label)

                        onToggled: root.setModifier(modelData.value, checked)
                    }
                }
            }
        }

        SettingsSelectRow {
            Layout.fillWidth: true
            title: qsTr("Action")
            description: qsTr("Choose one of HyprShelld's authenticated gesture actions.")
            model: [
                qsTr("Close"), qsTr("Cursor zoom"), qsTr("Floating state"),
                qsTr("Fullscreen state"), qsTr("Move window"),
                qsTr("Resize window"), qsTr("Move scrolling window"),
                qsTr("Special workspace"), qsTr("Navigate workspaces")
            ]
            currentIndex: root.actionIndex()
            enabled: root.controlsEnabled
            controlObjectName: "gestureAction"
            accessibleName: qsTr("Gesture action")
            minimumTargetSize: root.minimumTargetSize

            onValueModified: index => {
                const values = [
                    "close", "cursorZoom", "float", "fullscreen", "move",
                    "resize", "scrollMove", "special", "workspace"
                ];
                if (index >= 0 && index < values.length)
                    root.setAction(values[index]);
            }
        }

        SettingsSelectRow {
            Layout.fillWidth: true
            visible: root.actionType() === "float"
            title: qsTr("Floating action")
            description: qsTr("Make the active window floating, tiled, or toggle between the two states.")
            model: [qsTr("Float"), qsTr("Tile"), qsTr("Toggle")]
            currentIndex: {
                const values = ["float", "tile", "toggle"];
                const index = values.indexOf(root.actionMode());
                return index >= 0 ? index : 2;
            }
            enabled: root.controlsEnabled
            controlObjectName: "gestureFloatMode"
            accessibleName: qsTr("Floating-state gesture action")
            minimumTargetSize: root.minimumTargetSize

            onValueModified: index => {
                const values = ["float", "tile", "toggle"];
                if (index >= 0 && index < values.length) {
                    root.modify(function(candidate) {
                        candidate.action.mode = values[index];
                    });
                }
            }
        }

        SettingsSelectRow {
            Layout.fillWidth: true
            visible: root.actionType() === "fullscreen"
            title: qsTr("Fullscreen action")
            description: qsTr("Enter true fullscreen or maximize the active window while keeping compositor surfaces visible.")
            model: [qsTr("Fullscreen"), qsTr("Maximize")]
            currentIndex: root.actionMode() === "maximize" ? 1 : 0
            enabled: root.controlsEnabled
            controlObjectName: "gestureFullscreenMode"
            accessibleName: qsTr("Fullscreen gesture action")
            minimumTargetSize: root.minimumTargetSize

            onValueModified: index => root.modify(function(candidate) {
                candidate.action.mode = index === 1 ? "maximize" : "fullscreen";
            })
        }

        SettingsSelectRow {
            Layout.fillWidth: true
            visible: root.actionType() === "cursorZoom"
            title: qsTr("Zoom action")
            description: qsTr("Toggle a target zoom, multiply the current zoom, or follow a pinch continuously.")
            model: [qsTr("Toggle"), qsTr("Multiply"), qsTr("Live pinch")]
            currentIndex: {
                const values = ["toggle", "mult", "live"];
                const index = values.indexOf(root.actionMode());
                return index >= 0 ? index : 0;
            }
            enabled: root.controlsEnabled
            controlObjectName: "gestureCursorZoomMode"
            accessibleName: qsTr("Cursor zoom gesture mode")
            minimumTargetSize: root.minimumTargetSize

            onValueModified: index => {
                const values = ["toggle", "mult", "live"];
                if (index < 0 || index >= values.length)
                    return;
                root.modify(function(candidate) {
                    candidate.action.mode = values[index];
                    if (values[index] === "live"
                            && !root.isPinchDirection(candidate.direction)) {
                        candidate.direction = "pinch";
                        candidate.scale = 1;
                    }
                });
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: root.actionType() === "cursorZoom"
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Zoom level")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Use a target or multiplier from 0.01 through 100.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            RuleDecimalField {
                objectName: "gestureCursorZoomLevel"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.maximumWidth: Math.max(0, root.width - root.leftPadding - root.rightPadding)
                value: root.gesture && root.gesture.action
                    ? root.gesture.action.zoomLevel : 2
                minimumValue: 0.01
                maximumValue: 100
                enabled: root.controlsEnabled
                accessibleName: qsTr("Cursor zoom level")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: value => root.modify(function(candidate) {
                    candidate.action.zoomLevel = value;
                })
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: root.actionType() === "special"
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Special workspace name")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Enter the exact non-empty special-workspace name.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            TextField {
                objectName: "gestureSpecialWorkspace"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.maximumWidth: Math.max(0, root.width - root.leftPadding - root.rightPadding)
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: root.gesture && root.gesture.action
                    ? String(root.gesture.action.workspace) : ""
                maximumLength: 256
                placeholderText: qsTr("special")
                enabled: root.controlsEnabled
                Accessible.name: qsTr("Special workspace name")

                onTextEdited: root.modify(function(candidate) {
                    candidate.action.workspace = text;
                })
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            CheckBox {
                objectName: "gestureDisableInhibit"
                Layout.fillWidth: true
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Allow while compositor gestures are inhibited")
                checked: root.gesture
                    && root.gesture.disableInhibit === true
                enabled: root.controlsEnabled
                Accessible.name: text

                onToggled: root.modify(function(candidate) {
                    candidate.disableInhibit = checked;
                })
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Advanced: this bypasses an application's request to inhibit compositor gestures. It can make a managed gesture activate during interactions that expect gestures to be reserved.")
                color: "#ffd5a1"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.issue.length > 0
            text: root.issue
            color: "#ffb8c3"
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Button {
            objectName: "removeGestureFromEditorButton"
            Layout.fillWidth: true
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: qsTr("Remove gesture")
            enabled: root.controlsEnabled && root.gesture !== null
            Accessible.name: qsTr("Remove this gesture from the Input draft")

            onClicked: {
                if (root.gesture)
                    root.removeRequested(String(root.gesture.id));
            }
        }
    }
}
