pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import HyprShelld.UI

Rectangle {
    id: root

    required property string displayText
    required property string tooltipText
    required property int maximumWidth

    objectName: "declarativeTextPill"
    implicitWidth: Math.min(root.maximumWidth, label.implicitWidth + 24)
    implicitHeight: Math.min(28, Math.max(20, label.implicitHeight + 8))
    radius: height / 2
    color: ShellTheme.floating
    border.color: ShellTheme.primary
    border.width: 1
    clip: true

    Behavior on color {
        ColorAnimation {
            duration: ShellTheme.transitionDuration
            easing.type: Easing.OutCubic
        }
    }

    Behavior on border.color {
        ColorAnimation {
            duration: ShellTheme.transitionDuration
            easing.type: Easing.OutCubic
        }
    }

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
        color: ShellTheme.onSurface
        font.pixelSize: 13
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        maximumLineCount: 1
        wrapMode: Text.NoWrap

        Behavior on color {
            ColorAnimation {
                duration: ShellTheme.transitionDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    HoverHandler {
        id: hover
    }

    ToolTip {
        visible: hover.hovered && root.tooltipText.length > 0
        delay: 500
        background: Rectangle {
            color: ShellTheme.floating
            border.color: ShellTheme.outline
            border.width: 1
            radius: 6
        }
        contentItem: Text {
            text: root.tooltipText
            textFormat: Text.PlainText
            color: ShellTheme.onSurface
            wrapMode: Text.Wrap
        }
    }
}
