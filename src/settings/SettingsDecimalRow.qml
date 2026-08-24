pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string title: ""
    property string description: ""
    property var value: 0
    property real minimumValue: 0
    property real maximumValue: 1
    property real controlWidth: 190
    property string controlObjectName: ""
    property string validationObjectName: ""
    property string validationExample: "0.5"
    property string accessibleName: title
    property real minimumTargetSize: 44
    property bool localEditActive: false
    readonly property int maximumPlainDecimalLength: 326

    signal valueModified(var value)

    readonly property string projectedValue: {
        if (typeof root.value === "number" && Number.isFinite(root.value))
            return root.plainDecimalString(root.value);
        return typeof root.value === "string" ? root.value : "";
    }
    readonly property bool inputValid: decimalField.inputValid
    readonly property string validationMessage: qsTr(
        "Enter a plain decimal from %1 through %2 (for example, %3). Do not use spaces, a plus sign, redundant leading zeroes, a missing digit beside the decimal point, or exponent notation."
    ).arg(root.plainDecimalString(root.minimumValue))
        .arg(root.plainDecimalString(root.maximumValue))
        .arg(root.validationExample)

    spacing: 16

    function synchronizeProjectedText() {
        if (!root.localEditActive
                && decimalField.text !== root.projectedValue) {
            decimalField.text = root.projectedValue;
        }
    }

    function scheduleProjectedTextSynchronization() {
        Qt.callLater(root.synchronizeProjectedText);
    }

    function plainDecimalString(value) {
        if (typeof value !== "number" || !Number.isFinite(value))
            return "";
        const canonical = Object.is(value, -0) ? 0 : value;
        const text = String(canonical);
        const exponentIndex = text.toLowerCase().indexOf("e");
        if (exponentIndex < 0)
            return text;

        let mantissa = text.substring(0, exponentIndex);
        const exponent = Number(text.substring(exponentIndex + 1));
        let sign = "";
        if (mantissa[0] === "-") {
            sign = "-";
            mantissa = mantissa.substring(1);
        }

        const decimalPoint = mantissa.indexOf(".");
        const integerDigits = decimalPoint < 0
            ? mantissa.length : decimalPoint;
        const digits = decimalPoint < 0
            ? mantissa
            : mantissa.substring(0, decimalPoint)
                + mantissa.substring(decimalPoint + 1);
        const projectedPoint = integerDigits + exponent;

        if (projectedPoint <= 0) {
            return sign + "0." + "0".repeat(-projectedPoint) + digits;
        }
        if (projectedPoint >= digits.length) {
            return sign + digits
                + "0".repeat(projectedPoint - digits.length);
        }
        return sign + digits.substring(0, projectedPoint) + "."
            + digits.substring(projectedPoint);
    }

    onProjectedValueChanged: root.scheduleProjectedTextSynchronization()
    Component.onCompleted: root.scheduleProjectedTextSynchronization()

    Connections {
        target: decimalField

        function onActiveFocusChanged() {
            if (!decimalField.activeFocus) {
                root.localEditActive = false;
                root.synchronizeProjectedText();
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 2

        Label {
            Layout.fillWidth: true
            text: root.title
            color: root.palette.text
            font.pixelSize: 14
            font.weight: Font.Medium
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Label {
            Layout.fillWidth: true
            text: root.description
            color: root.palette.placeholderText
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }
    }

    ColumnLayout {
        Layout.preferredWidth: root.controlWidth
        Layout.maximumWidth: root.controlWidth
        spacing: 4

        RuleDecimalField {
            id: decimalField

            objectName: root.controlObjectName
            Layout.fillWidth: true
            value: root.projectedValue
            minimumValue: root.minimumValue
            maximumValue: root.maximumValue
            enabled: root.enabled
            accessibleName: root.accessibleName
            minimumTargetSize: root.minimumTargetSize
            maximumLength: root.maximumPlainDecimalLength
            Accessible.description: root.inputValid
                ? root.description : root.validationMessage

            onValueModified: value => {
                root.localEditActive = true;
                root.valueModified(value);
            }
        }

        Label {
            objectName: root.validationObjectName
            Layout.fillWidth: true
            visible: !root.inputValid
            text: root.validationMessage
            color: "#ffb8c3"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
    }
}
