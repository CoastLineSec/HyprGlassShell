pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    required property string displayText
    required property string tooltipText
    required property int maximumWidth

    objectName: "declarativeTextPill"
    implicitWidth: Math.min(root.maximumWidth, label.implicitWidth + 24)
    implicitHeight: Math.min(28, Math.max(20, label.implicitHeight + 8))
    radius: height / 2
    color: shellPalette.alternateBase
    border.color: shellPalette.highlight
    border.width: 1
    clip: true

    Accessible.role: Accessible.StaticText
    Accessible.name: root.displayText
    Accessible.description: root.tooltipText

    Text {
        id: label

        objectName: "declarativeTextPillLabel"
        anchors {
            fill: parent
            leftMargin: 12
            rightMargin: 12
        }
        text: root.displayText
        textFormat: Text.PlainText
        color: shellPalette.text
        font.pixelSize: 13
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        maximumLineCount: 1
        wrapMode: Text.NoWrap
    }

    HoverHandler {
        id: hover
    }

    SystemPalette {
        id: shellPalette

        colorGroup: SystemPalette.Active
    }

    ToolTip {
        visible: hover.hovered && root.tooltipText.length > 0
        delay: 500
        contentItem: Text {
            text: root.tooltipText
            textFormat: Text.PlainText
            color: shellPalette.text
            wrapMode: Text.Wrap
        }
    }
}
