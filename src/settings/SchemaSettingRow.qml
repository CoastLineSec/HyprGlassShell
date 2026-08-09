pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ColumnLayout {
    id: root

    required property var definition
    required property var settingValue
    property bool controlsEnabled: true
    property bool showGroupHeading: false
    property string validationError: ""

    readonly property string settingKey:
        definition && typeof definition.key === "string"
            ? definition.key : ""
    readonly property string settingType:
        definition && typeof definition.type === "string"
            ? definition.type : ""
    readonly property var options: root.listValue(
        definition ? definition.options : null
    )

    signal valueEdited(var value)

    function listValue(value) {
        if (Array.isArray(value))
            return value.slice();
        if (!value || typeof value.length !== "number")
            return [];
        const result = [];
        for (let index = 0; index < value.length; ++index)
            result.push(value[index]);
        return result;
    }

    function boundedText(value) {
        if (value === undefined || value === null)
            return "";
        return String(value);
    }

    function minimumValue() {
        return typeof root.definition.minimum === "number"
            ? root.definition.minimum : -Number.MAX_SAFE_INTEGER;
    }

    function maximumValue() {
        return typeof root.definition.maximum === "number"
            ? root.definition.maximum : Number.MAX_SAFE_INTEGER;
    }

    function commitNumber(text, integerOnly) {
        const trimmed = String(text).trim();
        const value = Number(trimmed);
        if (trimmed.length === 0 || !Number.isFinite(value)
                || (integerOnly && !Number.isInteger(value))
                || value < root.minimumValue()
                || value > root.maximumValue()) {
            root.validationError = integerOnly
                ? qsTr("Enter a whole number within the allowed range.")
                : qsTr("Enter a number within the allowed range.");
            return;
        }
        const step = root.definition.step;
        if (typeof step === "number" && step > 0) {
            const offset = (value - root.minimumValue()) / step;
            if (!Number.isFinite(offset)
                    || Math.abs(offset - Math.round(offset)) > 1e-9) {
                root.validationError = qsTr("Enter a value aligned to the allowed step.");
                return;
            }
        }
        root.validationError = "";
        root.valueEdited(value);
    }

    function commitString(text) {
        const value = String(text);
        const minimumLength = typeof root.definition.minimumLength === "number"
            ? root.definition.minimumLength : 0;
        const maximumLength = typeof root.definition.maximumLength === "number"
            ? root.definition.maximumLength : 4096;
        if (value.length < minimumLength || value.length > maximumLength) {
            root.validationError = qsTr("Enter between %1 and %2 characters.")
                .arg(minimumLength).arg(maximumLength);
            return;
        }
        root.validationError = "";
        root.valueEdited(value);
    }

    function commitColor(text) {
        const value = String(text).trim().toUpperCase();
        if (!/^#[0-9A-F]{6}([0-9A-F]{2})?$/.test(value)) {
            root.validationError = qsTr("Use #RRGGBB or #RRGGBBAA.");
            return;
        }
        root.validationError = "";
        root.valueEdited(value);
    }

    function keybindingText(value) {
        if (!value || typeof value !== "object" || Array.isArray(value)
                || typeof value.key !== "string") {
            return "";
        }
        return root.listValue(value.modifiers).concat([value.key]).join("+");
    }

    function commitKeybinding(text) {
        const value = String(text).trim();
        if (value.length === 0) {
            root.validationError = "";
            root.valueEdited(null);
            return;
        }
        const pieces = value.split("+").map(piece => piece.trim())
            .filter(piece => piece.length > 0);
        if (pieces.length === 0) {
            root.validationError = qsTr("Enter a key, optionally preceded by modifiers.");
            return;
        }
        const key = pieces.pop();
        const canonical = ["ctrl", "alt", "shift", "super", "hyper"];
        const modifiers = [];
        for (const piece of pieces) {
            const modifier = piece.toLowerCase();
            if (!canonical.includes(modifier)
                    || modifiers.includes(modifier)) {
                root.validationError = qsTr("Use Ctrl, Alt, Shift, Super, or Hyper once each.");
                return;
            }
            modifiers.push(modifier);
        }
        if (key.length > 64) {
            root.validationError = qsTr("The key name is too long.");
            return;
        }
        modifiers.sort((left, right) =>
            canonical.indexOf(left) - canonical.indexOf(right));
        root.validationError = "";
        root.valueEdited({ modifiers: modifiers, key: key });
    }

    function optionIndex() {
        const selected = root.settingValue === undefined
            ? "" : String(root.settingValue);
        return root.options.findIndex(option => option
            && String(option.value) === selected);
    }

    objectName: "componentSetting-" + root.settingKey
    spacing: 6

    Label {
        Layout.fillWidth: true
        visible: root.showGroupHeading
        text: root.definition && root.definition.group
            ? String(root.definition.group).replace(/-/g, " ") : ""
        color: root.palette.text
        font.pixelSize: 15
        font.capitalization: Font.Capitalize
        font.weight: Font.DemiBold
        textFormat: Text.PlainText
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 18

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: root.definition && root.definition.label
                    ? root.definition.label : root.settingKey
                color: root.palette.text
                font.pixelSize: 14
                font.weight: Font.Medium
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Label {
                Layout.fillWidth: true
                text: root.definition && root.definition.description
                    ? root.definition.description : ""
                visible: text.length > 0
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
        }

        Switch {
            objectName: "componentSettingBoolean-" + root.settingKey
            visible: root.settingType === "boolean"
            enabled: root.controlsEnabled
            checked: Boolean(root.settingValue)
            Accessible.name: root.definition && root.definition.label
                ? root.definition.label : root.settingKey

            onClicked: {
                root.validationError = "";
                root.valueEdited(checked);
            }
        }

        TextField {
            objectName: "componentSettingNumber-" + root.settingKey
            Layout.preferredWidth: 180
            visible: root.settingType === "integer"
                || root.settingType === "number"
            enabled: root.controlsEnabled
            text: root.boundedText(root.settingValue)
            selectByMouse: true
            Accessible.name: root.definition && root.definition.label
                ? root.definition.label : root.settingKey

            onEditingFinished: root.commitNumber(
                text,
                root.settingType === "integer"
            )
        }

        TextField {
            objectName: "componentSettingString-" + root.settingKey
            Layout.preferredWidth: 240
            visible: root.settingType === "string"
            enabled: root.controlsEnabled
            text: root.boundedText(root.settingValue)
            selectByMouse: true
            Accessible.name: root.definition && root.definition.label
                ? root.definition.label : root.settingKey

            onEditingFinished: root.commitString(text)
        }

        ComboBox {
            id: enumControl

            objectName: "componentSettingEnum-" + root.settingKey
            Layout.preferredWidth: 210
            visible: root.settingType === "enum"
            enabled: root.controlsEnabled
            model: root.options.map(option => option && option.label
                ? option.label : option.value)
            currentIndex: root.optionIndex()
            Accessible.name: root.definition && root.definition.label
                ? root.definition.label : root.settingKey

            contentItem: Label {
                text: enumControl.displayText
                color: enumControl.palette.buttonText
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }

            delegate: ItemDelegate {
                id: optionDelegate

                required property int index
                required property var modelData

                objectName: "componentSettingEnumOption-"
                    + root.settingKey + "-" + index
                width: enumControl.width
                text: String(optionDelegate.modelData)
                highlighted: enumControl.highlightedIndex === index
                Accessible.name: text

                contentItem: Label {
                    text: optionDelegate.text
                    color: optionDelegate.palette.buttonText
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                    Accessible.ignored: true
                }
            }

            onActivated: index => {
                if (index < 0 || index >= root.options.length)
                    return;
                root.validationError = "";
                root.valueEdited(root.options[index].value);
            }
        }

        RowLayout {
            visible: root.settingType === "color"
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                radius: 6
                color: /^#[0-9A-Fa-f]{6}([0-9A-Fa-f]{2})?$/.test(
                    root.boundedText(root.settingValue)
                ) ? root.settingValue : "transparent"
                border.color: root.palette.mid
            }

            TextField {
                objectName: "componentSettingColor-" + root.settingKey
                Layout.preferredWidth: 148
                enabled: root.controlsEnabled
                text: root.boundedText(root.settingValue)
                selectByMouse: true
                Accessible.name: root.definition && root.definition.label
                    ? root.definition.label : root.settingKey

                onEditingFinished: root.commitColor(text)
            }
        }

        TextField {
            objectName: "componentSettingKeybinding-" + root.settingKey
            Layout.preferredWidth: 220
            visible: root.settingType === "keybinding"
            enabled: root.controlsEnabled
            text: root.keybindingText(root.settingValue)
            placeholderText: qsTr("Super+K")
            selectByMouse: true
            Accessible.name: root.definition && root.definition.label
                ? root.definition.label : root.settingKey

            onEditingFinished: root.commitKeybinding(text)
        }

        RowLayout {
            visible: root.settingType === "file"
                || root.settingType === "directory"
            spacing: 8

            Label {
                Layout.preferredWidth: 200
                text: root.settingValue === null
                        || root.settingValue === undefined
                    ? qsTr("Not selected") : String(root.settingValue)
                color: root.palette.placeholderText
                elide: Text.ElideMiddle
                textFormat: Text.PlainText
            }

            Button {
                objectName: "componentSettingChoose-" + root.settingKey
                text: qsTr("Choose…")
                enabled: root.controlsEnabled
                onClicked: {
                    if (root.settingType === "directory")
                        directoryDialog.open();
                    else
                        fileDialog.open();
                }
            }

            Button {
                objectName: "componentSettingClear-" + root.settingKey
                text: qsTr("Clear")
                enabled: root.controlsEnabled
                    && root.settingValue !== null
                    && root.settingValue !== undefined
                onClicked: {
                    root.validationError = "";
                    root.valueEdited(null);
                }
            }
        }
    }

    Label {
        objectName: "componentSettingError-" + root.settingKey
        Layout.fillWidth: true
        visible: root.validationError.length > 0
        text: root.validationError
        color: "#ffb8c3"
        font.pixelSize: 11
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    FileDialog {
        id: fileDialog

        title: qsTr("Choose a file")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            root.validationError = "";
            root.valueEdited(selectedFile.toString());
        }
    }

    FolderDialog {
        id: directoryDialog

        title: qsTr("Choose a directory")
        onAccepted: {
            root.validationError = "";
            root.valueEdited(selectedFolder.toString());
        }
    }
}
