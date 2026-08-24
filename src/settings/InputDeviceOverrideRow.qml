pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property var definition
    property bool included: false
    property var value: undefined
    property bool fieldValid: true
    property bool controlsEnabled: false
    property real minimumTargetSize: 44

    signal includeModified(bool included)
    signal valueModified(var value)

    function numberDraft(text, integerOnly) {
        const trimmed = String(text).trim();
        if (trimmed.length === 0)
            return "";
        if (integerOnly && !/^-?(?:0|[1-9][0-9]*)$/.test(trimmed))
            return trimmed;
        const parsed = Number(trimmed);
        return Number.isFinite(parsed) ? parsed : trimmed;
    }

    function vectorDraft(component, text) {
        const current = Array.isArray(root.value) && root.value.length === 2
            ? [root.value[0], root.value[1]] : [0, 0];
        current[component] = root.numberDraft(text, false);
        return current;
    }

    function enumIndex() {
        if (!root.definition || !Array.isArray(root.definition.values))
            return -1;
        return root.definition.values.indexOf(root.value);
    }

    objectName: "inputDeviceOverrideRow_" + String(root.definition.key)
    Layout.fillWidth: true
    padding: 12

    background: Rectangle {
        color: root.included ? "#242131" : root.palette.alternateBase
        radius: 12
        border.color: !root.fieldValid
            ? "#a8606a"
            : root.included ? "#7b68a1" : root.palette.mid
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 9

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            CheckBox {
                objectName: "inputDeviceOverrideToggle_"
                    + String(root.definition.key)
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                checked: root.included
                enabled: root.controlsEnabled
                text: qsTr("Override")
                Accessible.name: qsTr("Override %1 for this device")
                    .arg(String(root.definition.title))

                onToggled: root.includeModified(checked)
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: String(root.definition.title)
                    color: root.palette.text
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: String(root.definition.description)
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }
        }

        Switch {
            objectName: "inputDeviceBooleanControl_"
                + String(root.definition.key)
            Layout.leftMargin: 12
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            visible: root.definition.type === "boolean" && root.included
            checked: root.value === true
            enabled: root.controlsEnabled && root.included
            text: checked ? qsTr("On") : qsTr("Off")
            Accessible.name: String(root.definition.title)

            onToggled: root.valueModified(checked)
        }

        ComboBox {
            objectName: "inputDeviceEnumControl_"
                + String(root.definition.key)
            Layout.fillWidth: true
            Layout.leftMargin: 12
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            visible: root.definition.type === "enum" && root.included
            model: Array.isArray(root.definition.labels)
                ? root.definition.labels : []
            currentIndex: root.enumIndex()
            enabled: root.controlsEnabled && root.included
            Accessible.name: String(root.definition.title)

            onActivated: index => {
                if (index >= 0 && index < root.definition.values.length)
                    root.valueModified(root.definition.values[index]);
            }
        }

        TextField {
            objectName: "inputDeviceScalarControl_"
                + String(root.definition.key)
            Layout.fillWidth: true
            Layout.leftMargin: 12
            implicitHeight: root.minimumTargetSize
            visible: root.included && ["integer", "number", "string"]
                .includes(root.definition.type)
            enabled: root.controlsEnabled && root.included
            text: root.value === undefined || root.value === null
                ? "" : String(root.value)
            maximumLength: root.definition.type === "string" ? 256 : 64
            selectByMouse: true
            placeholderText: root.definition.placeholder || ""
            inputMethodHints: root.definition.type === "integer"
                ? Qt.ImhFormattedNumbersOnly
                : root.definition.type === "number"
                    ? Qt.ImhFormattedNumbersOnly : Qt.ImhNone
            Accessible.name: String(root.definition.title)

            onTextEdited: {
                if (root.definition.type === "string")
                    root.valueModified(text);
                else
                    root.valueModified(root.numberDraft(
                        text, root.definition.type === "integer"));
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            visible: root.definition.type === "vector2" && root.included
            spacing: 8

            TextField {
                objectName: "inputDeviceVectorXControl_"
                    + String(root.definition.key)
                Layout.fillWidth: true
                implicitHeight: root.minimumTargetSize
                enabled: root.controlsEnabled && root.included
                text: Array.isArray(root.value) && root.value.length === 2
                    ? String(root.value[0]) : ""
                maximumLength: 64
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                placeholderText: qsTr("X")
                Accessible.name: qsTr("%1 X value")
                    .arg(String(root.definition.title))

                onTextEdited: root.valueModified(root.vectorDraft(0, text))
            }

            TextField {
                objectName: "inputDeviceVectorYControl_"
                    + String(root.definition.key)
                Layout.fillWidth: true
                implicitHeight: root.minimumTargetSize
                enabled: root.controlsEnabled && root.included
                text: Array.isArray(root.value) && root.value.length === 2
                    ? String(root.value[1]) : ""
                maximumLength: 64
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                placeholderText: qsTr("Y")
                Accessible.name: qsTr("%1 Y value")
                    .arg(String(root.definition.title))

                onTextEdited: root.valueModified(root.vectorDraft(1, text))
            }
        }

        Label {
            objectName: "inputDeviceOverrideIssue_"
                + String(root.definition.key)
            Layout.fillWidth: true
            Layout.leftMargin: 12
            visible: root.included && !root.fieldValid
            text: qsTr("Enter a value accepted by the pinned Hyprland 0.56.2 device schema.")
            color: "#ffb8c3"
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
    }
}
