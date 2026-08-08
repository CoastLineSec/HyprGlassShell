pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import HyprShelld.UI

Control {
    id: root

    required property int barHeight
    property bool configurationAvailable: true
    property bool adjusting: false
    property bool animationsEnabled: true
    readonly property real previewScale: barFrame.scale

    implicitHeight: 286
    padding: 14
    Accessible.ignored: true

    background: Rectangle {
        color: root.palette.base
        radius: 16
        border.color: root.palette.mid
    }

    contentItem: Item {
        id: scene

        clip: true

        Rectangle {
            anchors.fill: parent
            radius: 12
            border.color: Qt.rgba(
                root.palette.highlight.r,
                root.palette.highlight.g,
                root.palette.highlight.b,
                0.28
            )

            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: Qt.rgba(
                        root.palette.highlight.r,
                        root.palette.highlight.g,
                        root.palette.highlight.b,
                        0.24
                    )
                }

                GradientStop {
                    position: 0.56
                    color: root.palette.window
                }

                GradientStop {
                    position: 1
                    color: Qt.rgba(
                        root.palette.mid.r,
                        root.palette.mid.g,
                        root.palette.mid.b,
                        0.7
                    )
                }
            }

            Rectangle {
                width: Math.max(160, parent.width * 0.48)
                height: width
                x: parent.width - width * 0.68
                y: -height * 0.56
                radius: width / 2
                color: root.palette.highlight
                opacity: 0.1
            }

            Rectangle {
                width: Math.max(110, parent.width * 0.28)
                height: width
                x: -width * 0.45
                y: parent.height - height * 0.52
                radius: width / 2
                color: root.palette.highlight
                opacity: 0.07
            }

            Item {
                id: barFrame

                objectName: "previewBarFrame"
                x: 18
                y: 18
                width: Math.max(560, parent.width - 36)
                height: previewBar.height
                scale: Math.min(1, Math.max(0, parent.width - 36) / width)
                transformOrigin: Item.TopLeft

                Bar {
                    id: previewBar

                    objectName: "previewBarVisual"
                    width: parent.width
                    barHeight: root.barHeight
                    currentTime: new Date(2026, 7, 7, 15, 42)
                    screenName: qsTr("Preview")
                    configurationAvailable: root.configurationAvailable

                    Behavior on barHeight {
                        enabled: root.animationsEnabled && !root.adjusting

                        NumberAnimation {
                            duration: 180
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }

            Rectangle {
                id: reservedBoundary

                x: 18
                y: barFrame.y + barFrame.height * barFrame.scale + 10
                width: Math.max(0, parent.width - 36)
                height: 1
                color: root.palette.highlight
                opacity: 0.45
            }

            Text {
                anchors {
                    right: reservedBoundary.right
                    top: reservedBoundary.bottom
                    topMargin: 5
                }

                objectName: "reservedWorkspaceLabel"
                text: qsTr("Reserved workspace")
                color: root.palette.placeholderText
                font.pixelSize: 11
            }

            Rectangle {
                id: primaryWindow

                x: 34
                y: reservedBoundary.y + 26
                width: Math.max(120, parent.width * 0.55)
                height: Math.max(54, parent.height - y - 22)
                radius: 10
                color: Qt.rgba(
                    root.palette.base.r,
                    root.palette.base.g,
                    root.palette.base.b,
                    0.88
                )
                border.color: root.palette.mid

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                    }

                    height: 25
                    radius: 10
                    color: Qt.rgba(
                        root.palette.text.r,
                        root.palette.text.g,
                        root.palette.text.b,
                        0.06
                    )

                    Rectangle {
                        anchors {
                            left: parent.left
                            leftMargin: 12
                            verticalCenter: parent.verticalCenter
                        }

                        width: 48
                        height: 5
                        radius: 3
                        color: root.palette.placeholderText
                        opacity: 0.45
                    }
                }
            }

            Rectangle {
                x: primaryWindow.x + primaryWindow.width + 14
                y: primaryWindow.y + 16
                width: Math.max(70, parent.width - x - 34)
                height: Math.max(42, primaryWindow.height - 32)
                radius: 10
                color: Qt.rgba(
                    root.palette.base.r,
                    root.palette.base.g,
                    root.palette.base.b,
                    0.72
                )
                border.color: root.palette.mid

                Column {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: 13
                    }

                    spacing: 9

                    Repeater {
                        model: 3

                        Rectangle {
                            required property int index

                            width: parent.width * (0.9 - index * 0.12)
                            height: 5
                            radius: 3
                            color: root.palette.placeholderText
                            opacity: 0.36
                        }
                    }
                }
            }
        }
    }
}
