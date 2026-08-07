import QtQuick

Item {
    id: root

    required property bool expanded
    required property string screenName
    property date currentTime: new Date()

    signal toggleRequested()

    height: 48

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: root.currentTime = new Date()
    }

    Rectangle {
        anchors.fill: parent
        color: "#e61c2028"
        radius: 16
        border.color: "#3dffffff"
        border.width: 1

        Rectangle {
            id: statusIndicator

            anchors {
                left: parent.left
                leftMargin: 18
                verticalCenter: parent.verticalCenter
            }

            width: 10
            height: 10
            radius: 5
            color: "#73daca"
        }

        Text {
            anchors {
                left: statusIndicator.right
                leftMargin: 10
                verticalCenter: parent.verticalCenter
            }

            text: `HyprShelld qualification · ${root.screenName}`
            color: "#f4f7fb"
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        Text {
            anchors.centerIn: parent
            text: Qt.formatTime(root.currentTime, "hh:mm:ss")
            color: "#f4f7fb"
            font.pixelSize: 15
            font.weight: Font.Medium
        }

        Rectangle {
            id: toggleButton

            anchors {
                right: parent.right
                rightMargin: 8
                verticalCenter: parent.verticalCenter
            }

            width: toggleLabel.implicitWidth + 24
            height: 32
            radius: 12
            color: toggleArea.containsMouse ? "#36506b" : "#29394d"

            Text {
                id: toggleLabel

                anchors.centerIn: parent
                text: root.expanded ? "Close test" : "Open test"
                color: "#f4f7fb"
                font.pixelSize: 13
                font.weight: Font.Medium
            }

            MouseArea {
                id: toggleArea

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleRequested()
            }
        }
    }
}
