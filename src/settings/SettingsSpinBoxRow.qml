pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string title: ""
    property string description: ""
    property int from: 0
    property int to: 100
    property int value: 0
    property bool editable: false
    property int stepSize: 1
    property string controlObjectName: ""
    property string accessibleName: title
    property real minimumTargetSize: 44

    signal valueModified(int value)

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

    SpinBox {
        objectName: root.controlObjectName
        implicitHeight: Math.max(
            root.minimumTargetSize,
            implicitBackgroundHeight,
            implicitContentHeight + topPadding + bottomPadding
        )
        from: root.from
        to: root.to
        value: root.value
        editable: root.editable
        stepSize: root.stepSize
        enabled: root.enabled
        Accessible.name: root.accessibleName

        onValueModified: root.valueModified(value)
    }
}
