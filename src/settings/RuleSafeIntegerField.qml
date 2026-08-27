pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

ColumnLayout {
    id: root

    property var value: 0
    property string controlObjectName: ""
    property string accessibleName: ""
    property real minimumTargetSize: 44

    signal valueModified(var value)

    readonly property string projectedText: {
        if (typeof root.value === "number" && Number.isSafeInteger(root.value))
            return String(root.value);
        return typeof root.value === "string" ? root.value : "";
    }
    readonly property bool inputValid:
        root.parseCanonicalInteger(root.projectedText) !== null

    spacing: 4

    function parseCanonicalInteger(text) {
        if (typeof text !== "string" || text.length < 1 || text.length > 17)
            return null;
        if (text === "0")
            return 0;

        let index = 0;
        if (text[0] === "-") {
            if (text.length === 1 || text[1] === "0")
                return null;
            index = 1;
        } else if (text[0] === "0" || text[0] === "+") {
            return null;
        }

        for (; index < text.length; ++index) {
            const code = text.charCodeAt(index);
            if (code < 48 || code > 57)
                return null;
        }

        const parsed = Number(text);
        return Number.isSafeInteger(parsed) ? parsed : null;
    }

    function synchronizeText() {
        if (!integerField.activeFocus
                && integerField.text !== root.projectedText) {
            integerField.text = root.projectedText;
        }
    }

    onValueChanged: root.synchronizeText()
    Component.onCompleted: root.synchronizeText()

    TextField {
        id: integerField

        objectName: root.controlObjectName
        Layout.fillWidth: true
        implicitHeight: root.minimumTargetSize
        maximumLength: 17
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        Accessible.name: root.accessibleName

        onTextEdited: {
            const parsed = root.parseCanonicalInteger(text);
            root.valueModified(parsed === null ? text : parsed);
        }
        onActiveFocusChanged: {
            if (!activeFocus)
                root.synchronizeText();
        }
    }

    Label {
        Layout.fillWidth: true
        visible: !root.inputValid
        text: qsTr("Enter a canonical whole number from −9007199254740991 through 9007199254740991.")
        color: ShellTheme.onErrorContainer
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }
}
