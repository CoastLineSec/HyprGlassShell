pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string title: ""
    property string description: ""
    property bool checked: false
    property string controlObjectName: ""
    property string accessibleName: title
    property real minimumTargetSize: 44

    signal valueModified(bool value)

    spacing: 16

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

    Switch {
        objectName: root.controlObjectName
        implicitHeight: Math.max(
            root.minimumTargetSize,
            implicitBackgroundHeight,
            implicitContentHeight + topPadding + bottomPadding
        )
        checked: root.checked
        enabled: root.enabled
        Accessible.name: root.accessibleName

        onClicked: root.valueModified(checked)
    }
}
