pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string title: ""
    property string description: ""
    property var model: []
    property int currentIndex: 0
    property real controlWidth: 160
    property string controlObjectName: ""
    property string accessibleName: title
    property real minimumTargetSize: 44

    signal valueModified(int index)

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

    ComboBox {
        objectName: root.controlObjectName
        Layout.preferredWidth: root.controlWidth
        Layout.maximumWidth: root.controlWidth
        implicitHeight: Math.max(
            root.minimumTargetSize,
            implicitBackgroundHeight,
            implicitContentHeight + topPadding + bottomPadding
        )
        model: root.model
        currentIndex: root.currentIndex
        enabled: root.enabled
        Accessible.name: root.accessibleName

        onActivated: index => root.valueModified(index)
    }
}
