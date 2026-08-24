pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

TextField {
    id: root

    property var value: 0
    property real minimumValue: -1000000
    property real maximumValue: 1000000
    property string accessibleName: ""
    property real minimumTargetSize: 44

    signal valueModified(var value)

    readonly property string projectedText: {
        if (typeof root.value === "number" && Number.isFinite(root.value))
            return String(root.value);
        return typeof root.value === "string" ? root.value : "";
    }
    readonly property bool inputValid:
        typeof root.parseDecimal(root.projectedText) === "number"

    implicitHeight: root.minimumTargetSize
    maximumLength: 64
    inputMethodHints: Qt.ImhFormattedNumbersOnly
    Accessible.name: root.accessibleName

    function parseDecimal(text) {
        if (typeof text !== "string" || text.length < 1
                || text.trim() !== text
                || !/^-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?$/.test(text)) {
            return null;
        }
        const parsed = Number(text);
        if (!Number.isFinite(parsed) || parsed < root.minimumValue
                || parsed > root.maximumValue) {
            return null;
        }
        return Object.is(parsed, -0) ? 0 : parsed;
    }

    function synchronizeText() {
        if (!root.activeFocus && root.text !== root.projectedText)
            root.text = root.projectedText;
    }

    onValueChanged: root.synchronizeText()
    Component.onCompleted: root.synchronizeText()
    onTextEdited: {
        const parsed = root.parseDecimal(text);
        root.valueModified(parsed === null ? text : parsed);
    }
    onActiveFocusChanged: {
        if (!activeFocus)
            root.synchronizeText();
    }
}
