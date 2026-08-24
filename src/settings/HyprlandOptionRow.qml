pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var definition
    required property var settingValue
    property bool controlsEnabled: true
    property real minimumTargetSize: 44
    property string validationError: ""

    signal valueEdited(var value)

    readonly property string optionId: definition && typeof definition.id === "string" ? definition.id : ""
    readonly property string optionPath: definition && typeof definition.path === "string" ? definition.path : optionId.replace(/^hyprland\./, "")
    readonly property string settingType: definition && typeof definition.type === "string" ? definition.type : ""
    readonly property string controlKind: definition && typeof definition.control === "string" ? definition.control : ""
    readonly property bool writable: !definition || definition.writable === undefined ? true : definition.writable === true
    readonly property bool inherited: Boolean(settingValue && typeof settingValue === "object" && !Array.isArray(settingValue) && settingValue.kind === "inherit" && typeof settingValue.from === "string")
    readonly property bool canEdit: controlsEnabled && writable && !inherited
    readonly property bool differsFromDefault: root.canonical(root.settingValue) !== root.canonical(root.definition.defaultValue)

    implicitHeight: content.implicitHeight + 24
    radius: 16
    color: Qt.rgba(palette.base.r, palette.base.g, palette.base.b, 0.78)
    border.width: 1
    border.color: Qt.rgba(palette.text.r, palette.text.g, palette.text.b, 0.09)

    function canonical(value) {
        try {
            return JSON.stringify(value);
        } catch (error) {
            return "";
        }
    }

    function humanize(value) {
        const text = String(value || "").replace(/[-_.:]+/g, " ").replace(/\s+/g, " ").trim();
        if (text.length === 0)
            return qsTr("Hyprland option");
        return text.charAt(0).toUpperCase() + text.slice(1);
    }

    function titleText() {
        const pieces = root.optionPath.split(":");
        return root.humanize(pieces[pieces.length - 1]);
    }

    function sectionText() {
        const pieces = root.optionPath.split(":");
        if (pieces.length <= 1)
            return root.definition.module ? root.humanize(root.definition.module) : "";
        pieces.pop();
        return pieces.map(piece => root.humanize(piece)).join("  /  ");
    }

    function choiceList() {
        return root.definition && Array.isArray(root.definition.choices) ? root.definition.choices : [];
    }

    function choiceValue(index) {
        const choices = root.choiceList();
        if (index < 0 || index >= choices.length)
            return undefined;
        const choice = choices[index];
        return choice && typeof choice === "object" && !Array.isArray(choice) && choice.value !== undefined ? choice.value : choice;
    }

    function choiceLabel(index) {
        const choices = root.choiceList();
        if (index < 0 || index >= choices.length)
            return "";
        const choice = choices[index];
        if (choice && typeof choice === "object" && !Array.isArray(choice) && choice.label !== undefined) {
            return root.humanize(choice.label);
        }
        return root.humanize(choice);
    }

    function currentChoiceIndex() {
        const choices = root.choiceList();
        const selected = root.canonical(root.settingValue);
        for (let index = 0; index < choices.length; ++index) {
            if (root.canonical(root.choiceValue(index)) === selected)
                return index;
        }
        return -1;
    }

    function numberMinimum(index) {
        const value = root.definition ? root.definition.min : undefined;
        if (Array.isArray(value))
            return Number(value[index]);
        return typeof value === "number" ? value : -2147483648;
    }

    function numberMaximum(index) {
        const value = root.definition ? root.definition.max : undefined;
        if (Array.isArray(value))
            return Number(value[index]);
        return typeof value === "number" ? value : 2147483647;
    }

    function commitNumber(text, integerOnly) {
        const trimmed = String(text).trim();
        const value = Number(trimmed);
        if (trimmed.length === 0 || !Number.isFinite(value) || (integerOnly && !Number.isInteger(value)) || value < root.numberMinimum(0) || value > root.numberMaximum(0)) {
            root.validationError = integerOnly ? qsTr("Enter a whole number from %1 through %2.").arg(root.numberMinimum(0)).arg(root.numberMaximum(0)) : qsTr("Enter a number from %1 through %2.").arg(root.numberMinimum(0)).arg(root.numberMaximum(0));
            return;
        }
        root.validationError = "";
        root.valueEdited(value);
    }

    function commitString(text) {
        const value = String(text);
        const maximum = typeof root.definition.maxLength === "number" ? root.definition.maxLength : 4096;
        if (value.length > maximum) {
            root.validationError = qsTr("Keep this value under %1 characters.").arg(maximum);
            return;
        }
        if (typeof root.definition.pattern === "string") {
            try {
                if (!(new RegExp(root.definition.pattern)).test(value)) {
                    root.validationError = qsTr("This value does not match Hyprland's required format.");
                    return;
                }
            } catch (error) {
                root.validationError = qsTr("The catalog format rule could not be evaluated.");
                return;
            }
        }
        root.validationError = "";
        root.valueEdited(value);
    }

    function canonicalColor(text) {
        const value = String(text).trim();
        if (!/^0x[0-9A-Fa-f]{8}$/.test(value))
            return "";
        return "0x" + value.slice(2).toUpperCase();
    }

    function previewColor(text) {
        const value = root.canonicalColor(text);
        return value.length === 10 ? "#" + value.slice(2) : "transparent";
    }

    function commitColor(text) {
        const value = root.canonicalColor(text);
        if (value.length === 0) {
            root.validationError = qsTr("Use Hyprland's canonical 0xAARRGGBB color format.");
            return;
        }
        root.validationError = "";
        root.valueEdited(value);
    }

    function vectorPart(index) {
        return Array.isArray(root.settingValue) && root.settingValue.length === 2 ? root.settingValue[index] : 0;
    }

    function commitVector(first, second) {
        const values = [Number(String(first).trim()), Number(String(second).trim())];
        for (let index = 0; index < 2; ++index) {
            if (!Number.isFinite(values[index]) || values[index] < root.numberMinimum(index) || values[index] > root.numberMaximum(index)) {
                root.validationError = qsTr("Both coordinates must be inside their catalog ranges.");
                return;
            }
        }
        root.validationError = "";
        root.valueEdited(values);
    }

    function gapPart(index) {
        return Array.isArray(root.settingValue) && root.settingValue.length === 4 ? root.settingValue[index] : 0;
    }

    function commitGap(fields) {
        const values = [];
        for (const field of fields) {
            const value = Number(String(field).trim());
            if (!Number.isInteger(value) || value < -2147483648 || value > 2147483647) {
                root.validationError = qsTr("Top, right, bottom, and left must be whole pixel values.");
                return;
            }
            values.push(value);
        }
        root.validationError = "";
        root.valueEdited(values);
    }

    function gradientColorsText() {
        if (!root.settingValue || typeof root.settingValue !== "object" || Array.isArray(root.settingValue) || !Array.isArray(root.settingValue.colors)) {
            return "";
        }
        return root.settingValue.colors.join(", ");
    }

    function gradientAngleText() {
        return root.settingValue && typeof root.settingValue === "object" && !Array.isArray(root.settingValue) && typeof root.settingValue.angle === "number" ? String(root.settingValue.angle) : "0";
    }

    function commitGradient(colorsText, angleText) {
        const colors = String(colorsText).split(",").map(color => root.canonicalColor(color)).filter(color => color.length > 0);
        const rawColors = String(colorsText).split(",").map(color => color.trim()).filter(color => color.length > 0);
        const angle = Number(String(angleText).trim());
        if (colors.length !== rawColors.length || colors.length < 1 || colors.length > 10 || !Number.isFinite(angle) || angle < -3600 || angle > 3600) {
            root.validationError = qsTr("Enter 1–10 comma-separated 0xAARRGGBB colors and an angle from −3600° through 3600°.");
            return;
        }
        root.validationError = "";
        root.valueEdited({
            colors: colors,
            angle: angle
        });
    }

    function beginOverride() {
        if (!root.inherited)
            return;
        root.validationError = "";
        root.valueEdited(root.settingType === "gradient" ? {
            colors: ["0xFFFFFFFF"],
            angle: 0
        } : "0xFFFFFFFF");
    }

    ColumnLayout {
        id: content

        anchors {
            fill: parent
            margins: 12
        }
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: root.titleText()
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: root.sectionText()
                    visible: text.length > 0
                    color: root.palette.highlight
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: root.definition.description || ""
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            Rectangle {
                Layout.preferredWidth: tierLabel.implicitWidth + 14
                Layout.preferredHeight: 24
                radius: 8
                color: Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.14)

                Label {
                    id: tierLabel
                    anchors.centerIn: parent
                    text: root.humanize(root.definition.uiTier || "common")
                    color: root.palette.highlight
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }

            Rectangle {
                visible: root.definition.risk && root.definition.risk !== "safe"
                Layout.preferredWidth: riskLabel.implicitWidth + 14
                Layout.preferredHeight: 24
                radius: 8
                color: root.definition.risk === "dangerous" ? "#6b2a36" : "#5b431f"

                Label {
                    id: riskLabel
                    anchors.centerIn: parent
                    text: root.definition.risk === "dangerous" ? qsTr("Danger") : qsTr("Caution")
                    color: root.definition.risk === "dangerous" ? "#ffb8c3" : "#ffd89a"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }

            Button {
                visible: root.differsFromDefault && root.writable
                text: qsTr("Reset")
                flat: true
                enabled: root.controlsEnabled
                Accessible.name: qsTr("Reset %1 to the Hyprland default").arg(root.titleText())
                onClicked: {
                    root.validationError = "";
                    root.valueEdited(root.definition.defaultValue);
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: inheritedRow.implicitHeight + 16
            visible: root.inherited
            radius: 10
            color: Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.10)

            RowLayout {
                id: inheritedRow
                anchors {
                    fill: parent
                    margins: 8
                }
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Inherits from %1").arg(root.settingValue && root.settingValue.from ? root.settingValue.from : qsTr("another option"))
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                Button {
                    text: qsTr("Override")
                    enabled: root.controlsEnabled && root.writable
                    onClicked: root.beginOverride()
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !root.writable
            text: qsTr("This value is preserved by the managed Lua contract but is read-only because its syntax is not safely editable yet.")
            color: "#ffd89a"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Switch {
            visible: root.settingType === "boolean" && !root.inherited
            Layout.alignment: Qt.AlignRight
            implicitHeight: root.minimumTargetSize
            checked: root.settingValue === true
            enabled: root.canEdit
            text: checked ? qsTr("Enabled") : qsTr("Disabled")
            Accessible.name: root.titleText()
            onClicked: {
                root.validationError = "";
                root.valueEdited(checked);
            }
        }

        ComboBox {
            id: enumControl

            visible: root.settingType === "enum" && !root.inherited
            Layout.fillWidth: true
            Layout.maximumWidth: 320
            Layout.alignment: Qt.AlignRight
            implicitHeight: root.minimumTargetSize
            model: root.choiceList().map((choice, index) => root.choiceLabel(index))
            currentIndex: root.currentChoiceIndex()
            enabled: root.canEdit
            Accessible.name: root.titleText()
            onActivated: index => {
                const value = root.choiceValue(index);
                if (value !== undefined) {
                    root.validationError = "";
                    root.valueEdited(value);
                }
            }
        }

        TextField {
            visible: (root.settingType === "integer" || root.settingType === "number" || root.settingType === "fontWeight") && !root.inherited
            Layout.fillWidth: true
            Layout.maximumWidth: 260
            Layout.alignment: Qt.AlignRight
            implicitHeight: root.minimumTargetSize
            text: root.settingValue === undefined || root.settingValue === null ? "" : String(root.settingValue)
            enabled: root.canEdit
            selectByMouse: true
            placeholderText: qsTr("%1 to %2").arg(root.numberMinimum(0)).arg(root.numberMaximum(0))
            Accessible.name: root.titleText()
            onEditingFinished: root.commitNumber(text, root.settingType !== "number")
        }

        TextField {
            visible: root.settingType === "string" && !root.inherited
            Layout.fillWidth: true
            implicitHeight: root.minimumTargetSize
            text: root.settingValue === undefined || root.settingValue === null ? "" : String(root.settingValue)
            enabled: root.canEdit
            selectByMouse: true
            placeholderText: qsTr("Hyprland value")
            Accessible.name: root.titleText()
            onEditingFinished: root.commitString(text)
        }

        RowLayout {
            visible: root.settingType === "color" && !root.inherited
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                radius: 10
                color: root.previewColor(root.settingValue)
                border.width: 1
                border.color: root.palette.mid
            }

            TextField {
                Layout.preferredWidth: 200
                implicitHeight: root.minimumTargetSize
                text: root.settingValue === undefined || root.settingValue === null ? "" : String(root.settingValue)
                enabled: root.canEdit
                selectByMouse: true
                placeholderText: "0xAARRGGBB"
                Accessible.name: root.titleText()
                onEditingFinished: root.commitColor(text)
            }
        }

        RowLayout {
            visible: root.settingType === "vector2" && !root.inherited
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: "X"
                color: root.palette.placeholderText
            }
            TextField {
                id: vectorX
                Layout.preferredWidth: 112
                implicitHeight: root.minimumTargetSize
                text: String(root.vectorPart(0))
                enabled: root.canEdit
                selectByMouse: true
                Accessible.name: qsTr("%1 X coordinate").arg(root.titleText())
                onEditingFinished: root.commitVector(text, vectorY.text)
            }
            Label {
                text: "Y"
                color: root.palette.placeholderText
            }
            TextField {
                id: vectorY
                Layout.preferredWidth: 112
                implicitHeight: root.minimumTargetSize
                text: String(root.vectorPart(1))
                enabled: root.canEdit
                selectByMouse: true
                Accessible.name: qsTr("%1 Y coordinate").arg(root.titleText())
                onEditingFinished: root.commitVector(vectorX.text, text)
            }
        }

        RowLayout {
            visible: root.settingType === "cssGap" && !root.inherited
            Layout.fillWidth: true
            spacing: 6

            Item {
                Layout.fillWidth: true
            }

            Repeater {
                id: gapRepeater
                model: [qsTr("Top"), qsTr("Right"), qsTr("Bottom"), qsTr("Left")]

                delegate: ColumnLayout {
                    id: gapDelegate
                    required property int index
                    required property string modelData
                    property alias fieldText: gapField.text
                    spacing: 2

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: gapDelegate.modelData
                        color: root.palette.placeholderText
                        font.pixelSize: 10
                    }

                    TextField {
                        id: gapField
                        Layout.preferredWidth: 78
                        implicitHeight: root.minimumTargetSize
                        text: String(root.gapPart(gapDelegate.index))
                        enabled: root.canEdit
                        horizontalAlignment: TextInput.AlignHCenter
                        selectByMouse: true
                        Accessible.name: qsTr("%1 %2 gap").arg(root.titleText()).arg(gapDelegate.modelData)
                        onEditingFinished: {
                            const fields = [];
                            for (let index = 0; index < gapRepeater.count; ++index) {
                                const item = gapRepeater.itemAt(index);
                                // qmllint disable missing-property
                                fields.push(item ? String(item["fieldText"]) : "");
                                // qmllint enable missing-property
                            }
                            root.commitGap(fields);
                        }
                    }
                }
            }
        }

        RowLayout {
            visible: root.settingType === "gradient" && !root.inherited
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Label {
                    text: qsTr("Color stops")
                    color: root.palette.placeholderText
                    font.pixelSize: 10
                }

                TextField {
                    id: gradientColors
                    Layout.fillWidth: true
                    implicitHeight: root.minimumTargetSize
                    text: root.gradientColorsText()
                    enabled: root.canEdit
                    selectByMouse: true
                    placeholderText: "0xAARRGGBB, 0xAARRGGBB"
                    Accessible.name: qsTr("%1 color stops").arg(root.titleText())
                    onEditingFinished: root.commitGradient(text, gradientAngle.text)
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 112
                spacing: 3

                Label {
                    text: qsTr("Angle")
                    color: root.palette.placeholderText
                    font.pixelSize: 10
                }

                TextField {
                    id: gradientAngle
                    Layout.fillWidth: true
                    implicitHeight: root.minimumTargetSize
                    text: root.gradientAngleText()
                    enabled: root.canEdit
                    selectByMouse: true
                    placeholderText: "0"
                    Accessible.name: qsTr("%1 angle").arg(root.titleText())
                    onEditingFinished: root.commitGradient(gradientColors.text, text)
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.validationError.length > 0
            text: root.validationError
            color: "#ffb8c3"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: root.optionId
                color: Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.48)
                font.family: "monospace"
                font.pixelSize: 10
                elide: Text.ElideMiddle
                textFormat: Text.PlainText
            }

            Label {
                text: root.humanize(root.definition.applyMode || "reload")
                color: root.palette.placeholderText
                font.pixelSize: 10
                textFormat: Text.PlainText
            }
        }
    }
}
