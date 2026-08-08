import QtQuick

Item {
    id: root

    required property int barHeight
    required property date currentTime
    required property string screenName
    required property bool configurationAvailable
    property bool shellDegraded: false
    property string healthSummary: ""
    property bool failureNoticeVisible: false
    property string failureNoticeText: ""

    readonly property real cornerRadius: Math.min(16, height * 0.375)
    readonly property string accessibleHealthSummary: healthSummary.length > 0
        ? healthSummary
        : qsTr("A HyprShelld component needs attention.")

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

            objectName: "configurationStatusIndicator"

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
            id: brandLabel

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

        Rectangle {
            id: healthIndicator

            objectName: "shellHealthIndicator"
            visible: root.shellDegraded

            anchors {
                left: brandLabel.right
                leftMargin: 8
                verticalCenter: parent.verticalCenter
            }

            width: Math.min(18, Math.max(14, root.height - 6))
            height: width
            radius: width / 2
            color: "#fb7185"
            border.color: "#66ffffff"
            border.width: 1

            Accessible.role: Accessible.Indicator
            Accessible.name: qsTr("HyprShelld needs attention")
            Accessible.description: root.accessibleHealthSummary
            Accessible.ignored: !visible

            Text {
                objectName: "shellHealthIndicatorGlyph"
                anchors.centerIn: parent
                text: "!"
                color: "#24171b"
                font.pixelSize: Math.max(10, parent.height - 6)
                font.weight: Font.Bold
                Accessible.ignored: true
            }
        }

        Text {
            id: clockLabel

            objectName: "clockLabel"
            anchors.centerIn: parent
            visible: !root.failureNoticeVisible
            text: Qt.formatDateTime(root.currentTime, "ddd, MMM d  h:mm AP")
            color: "#f5f7fa"
            font.pixelSize: 14
            font.weight: Font.Medium
        }

        Rectangle {
            id: failureNotice

            objectName: "failureNotice"
            anchors.centerIn: parent
            visible: root.failureNoticeVisible
            width: Math.min(parent.width * 0.52, noticeLabel.implicitWidth + 28)
            height: Math.min(28, Math.max(20, root.height - 6))
            radius: height / 2
            color: "#4a2732"
            border.color: "#99fb7185"
            border.width: 1

            Accessible.role: Accessible.AlertMessage
            Accessible.name: root.failureNoticeText
            Accessible.ignored: !visible

            Text {
                id: noticeLabel

                objectName: "failureNoticeLabel"
                anchors {
                    fill: parent
                    leftMargin: 12
                    rightMargin: 12
                }
                text: qsTr("!  %1").arg(root.failureNoticeText)
                color: "#ffe4e9"
                font.pixelSize: 12
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                Accessible.ignored: true
            }
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
