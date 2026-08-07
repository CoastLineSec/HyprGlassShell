import QtQuick

FocusScope {
    id: root

    required property bool expanded
    readonly property int expandedHeight: 184

    signal closeRequested()

    height: expanded ? expandedHeight : 0
    opacity: expanded ? 1 : 0
    scale: expanded ? 1 : 0.97
    visible: height > 0
    clip: true
    focus: expanded
    transformOrigin: Item.Top

    onExpandedChanged: {
        if (expanded) {
            Qt.callLater(input.forceActiveFocus);
        }
    }

    Keys.onEscapePressed: event => {
        root.closeRequested();
        event.accepted = true;
    }

    Behavior on height {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: 140
            easing.type: Easing.OutCubic
        }
    }

    Behavior on scale {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#f21c2028"
        radius: 18
        border.color: "#3dffffff"
        border.width: 1

        Text {
            id: heading

            anchors {
                top: parent.top
                left: parent.left
                topMargin: 20
                leftMargin: 20
            }

            text: "Surface interaction test"
            color: "#f4f7fb"
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }

        Text {
            anchors {
                top: heading.bottom
                left: heading.left
                right: parent.right
                topMargin: 8
                rightMargin: 20
            }

            text: "Type below to verify keyboard focus. Press Escape to close."
            color: "#aeb8c6"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        Rectangle {
            id: inputFrame

            anchors {
                left: parent.left
                right: closeButton.left
                bottom: parent.bottom
                leftMargin: 20
                rightMargin: 12
                bottomMargin: 20
            }

            height: 42
            radius: 12
            color: "#202b3a"
            border.color: input.activeFocus ? "#73daca" : "#405167"
            border.width: 1

            TextInput {
                id: input

                anchors {
                    fill: parent
                    leftMargin: 14
                    rightMargin: 14
                }

                verticalAlignment: TextInput.AlignVCenter
                color: "#f4f7fb"
                selectionColor: "#527a94"
                selectedTextColor: "#ffffff"
                font.pixelSize: 14
                clip: true
            }

            Text {
                anchors {
                    left: parent.left
                    leftMargin: 14
                    verticalCenter: parent.verticalCenter
                }

                visible: input.text.length === 0 && !input.activeFocus
                text: "Keyboard focus test"
                color: "#718096"
                font.pixelSize: 14
            }
        }

        Rectangle {
            id: closeButton

            anchors {
                right: parent.right
                bottom: parent.bottom
                rightMargin: 20
                bottomMargin: 20
            }

            width: closeLabel.implicitWidth + 24
            height: 42
            radius: 12
            color: closeArea.containsMouse ? "#63404c" : "#4a3440"

            Text {
                id: closeLabel

                anchors.centerIn: parent
                text: "Close"
                color: "#f4f7fb"
                font.pixelSize: 13
                font.weight: Font.Medium
            }

            MouseArea {
                id: closeArea

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.closeRequested()
            }
        }
    }
}
