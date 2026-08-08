import QtQuick

Item {
    id: root

    required property int barHeight
    required property date currentTime
    required property string screenName
    required property bool configurationAvailable

    readonly property real cornerRadius: Math.min(16, height * 0.375)

    implicitHeight: barHeight
    height: barHeight

    Rectangle {
        anchors.fill: parent
        color: "#ed171b22"
        radius: root.cornerRadius
        border.color: "#33ffffff"
        border.width: 1

        Rectangle {
            id: statusIndicator

            anchors {
                left: parent.left
                leftMargin: 18
                verticalCenter: parent.verticalCenter
            }

            width: 9
            height: 9
            radius: width / 2
            color: root.configurationAvailable ? "#68d391" : "#f6ad55"
        }

        Text {
            anchors {
                left: statusIndicator.right
                leftMargin: 10
                verticalCenter: parent.verticalCenter
            }

            text: "HyprShelld"
            color: "#f5f7fa"
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        Text {
            anchors.centerIn: parent
            text: Qt.formatDateTime(root.currentTime, "ddd, MMM d  h:mm AP")
            color: "#f5f7fa"
            font.pixelSize: 14
            font.weight: Font.Medium
        }

        Text {
            anchors {
                right: parent.right
                rightMargin: 18
                verticalCenter: parent.verticalCenter
            }

            text: root.screenName
            color: "#aeb8c6"
            font.pixelSize: 12
        }
    }
}
