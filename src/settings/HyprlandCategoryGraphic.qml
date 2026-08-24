pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Control {
    id: root

    property string kind: "appearance"
    property color accentColor: palette.highlight

    readonly property color accentSoft: Qt.rgba(
        accentColor.r, accentColor.g, accentColor.b, 0.18
    )
    readonly property color accentFaint: Qt.rgba(
        accentColor.r, accentColor.g, accentColor.b, 0.08
    )

    implicitWidth: 132
    implicitHeight: 96
    padding: 10
    Accessible.ignored: true

    background: Rectangle {
        radius: 16
        color: root.palette.base
        border.color: Qt.rgba(
            root.accentColor.r,
            root.accentColor.g,
            root.accentColor.b,
            0.34
        )

        gradient: Gradient {
            GradientStop {
                position: 0
                color: root.accentSoft
            }
            GradientStop {
                position: 0.68
                color: root.accentFaint
            }
            GradientStop {
                position: 1
                color: root.palette.base
            }
        }

        Rectangle {
            anchors {
                right: parent.right
                top: parent.top
                margins: 9
            }
            width: 18
            height: 18
            radius: 9
            color: root.accentFaint
        }
    }

    contentItem: Loader {
        sourceComponent: {
            switch (root.kind) {
            case "input": return inputGraphic;
            case "windows": return windowsGraphic;
            case "displays": return displaysGraphic;
            case "workspaces": return workspacesGraphic;
            case "rules": return rulesGraphic;
            case "shortcuts": return shortcutsGraphic;
            case "system": return systemGraphic;
            case "security": return securityGraphic;
            default: return appearanceGraphic;
            }
        }
    }

    Component {
        id: appearanceGraphic

        Item {
            Rectangle {
                x: 19
                y: 17
                width: 76
                height: 48
                radius: 13
                color: Qt.rgba(0, 0, 0, 0.25)
            }

            Rectangle {
                x: 13
                y: 11
                width: 76
                height: 48
                radius: 13
                color: root.palette.button
                border.width: 2
                border.color: root.accentColor

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                    }
                    height: 13
                    radius: parent.radius
                    color: root.accentSoft
                }

                Row {
                    anchors {
                        left: parent.left
                        top: parent.top
                        margins: 5
                    }
                    spacing: 3

                    Repeater {
                        model: 3

                        Rectangle {
                            required property int index

                            width: 4
                            height: 4
                            radius: 2
                            color: index === 0
                                ? root.accentColor
                                : root.palette.placeholderText
                        }
                    }
                }
            }

            Repeater {
                model: 3

                Rectangle {
                    required property int index

                    x: 94 + index * 7
                    y: 17 + index * 13
                    width: 13 - index * 2
                    height: 3
                    radius: 2
                    rotation: -22
                    color: index === 1
                        ? root.accentColor
                        : root.palette.placeholderText
                    opacity: 0.82 - index * 0.12
                }
            }
        }
    }

    Component {
        id: inputGraphic

        Item {
            Rectangle {
                x: 13
                y: 7
                width: 41
                height: 61
                radius: 21
                color: root.palette.button
                border.width: 2
                border.color: root.accentColor

                Rectangle {
                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                        topMargin: 9
                    }
                    width: 4
                    height: 12
                    radius: 2
                    color: root.accentColor
                }

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    height: 1
                    color: root.palette.mid
                }
            }

            Repeater {
                model: 3

                Rectangle {
                    required property int index

                    x: 69 + index * 15
                    y: 13 + Math.abs(1 - index) * 8
                    width: 12
                    height: 12
                    radius: 6
                    color: index === 1
                        ? root.accentColor
                        : root.palette.placeholderText
                }
            }

            Repeater {
                model: 3

                Rectangle {
                    required property int index

                    x: 73 + index * 15
                    y: 42 + Math.abs(1 - index) * 5
                    width: 3
                    height: 22
                    radius: 2
                    rotation: index === 0 ? -22 : index === 2 ? 22 : 0
                    color: root.accentColor
                    opacity: 0.78
                }
            }
        }
    }

    Component {
        id: windowsGraphic

        Item {
            Rectangle {
                x: 8
                y: 8
                width: 105
                height: 59
                radius: 9
                color: "transparent"
                border.width: 2
                border.color: root.palette.placeholderText

                Rectangle {
                    x: 5
                    y: 5
                    width: 42
                    height: parent.height - 10
                    radius: 6
                    color: root.accentSoft
                    border.color: root.accentColor
                }

                Rectangle {
                    x: 52
                    y: 5
                    width: parent.width - 57
                    height: 20
                    radius: 5
                    color: root.palette.button
                    border.color: root.palette.mid
                }

                Rectangle {
                    x: 52
                    y: 30
                    width: parent.width - 57
                    height: parent.height - 35
                    radius: 5
                    color: root.palette.button
                    border.color: root.accentColor
                }
            }
        }
    }

    Component {
        id: displaysGraphic

        Item {
            Rectangle {
                x: 8
                y: 11
                width: 68
                height: 43
                radius: 7
                color: root.palette.button
                border.width: 2
                border.color: root.accentColor

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                        margins: 5
                    }
                    height: 4
                    radius: 2
                    color: root.accentSoft
                }
            }

            Rectangle {
                x: 57
                y: 25
                width: 58
                height: 38
                radius: 7
                color: root.palette.button
                border.width: 2
                border.color: root.palette.placeholderText

                Rectangle {
                    anchors.centerIn: parent
                    width: 26
                    height: 4
                    radius: 2
                    color: root.accentColor
                    opacity: 0.72
                }
            }

            Rectangle {
                x: 36
                y: 55
                width: 10
                height: 9
                color: root.palette.placeholderText
            }

            Rectangle {
                x: 26
                y: 63
                width: 30
                height: 3
                radius: 2
                color: root.palette.placeholderText
            }
        }
    }

    Component {
        id: workspacesGraphic

        Item {
            Grid {
                anchors.centerIn: parent
                columns: 3
                spacing: 6

                Repeater {
                    model: 6

                    Rectangle {
                        id: workspaceTile

                        required property int index

                        width: 29
                        height: 24
                        radius: 7
                        color: index === 1
                            ? root.accentSoft
                            : root.palette.button
                        border.width: index === 1 ? 2 : 1
                        border.color: index === 1
                            ? root.accentColor
                            : root.palette.mid

                        Rectangle {
                            visible: workspaceTile.index === 1
                                || workspaceTile.index === 4
                            anchors.centerIn: parent
                            width: workspaceTile.index === 1 ? 12 : 7
                            height: 4
                            radius: 2
                            color: workspaceTile.index === 1
                                ? root.accentColor
                                : root.palette.placeholderText
                        }
                    }
                }
            }
        }
    }

    Component {
        id: rulesGraphic

        Item {
            Column {
                x: 8
                y: 10
                spacing: 8

                Repeater {
                    model: 3

                    Rectangle {
                        required property int index

                        width: 55 - index * 8
                        height: 10
                        radius: 5
                        color: index === 1
                            ? root.accentSoft
                            : root.palette.button
                        border.color: index === 1
                            ? root.accentColor
                            : root.palette.mid
                    }
                }
            }

            Rectangle {
                x: 67
                y: 36
                width: 25
                height: 3
                radius: 2
                color: root.accentColor
            }

            Rectangle {
                x: 85
                y: 31
                width: 10
                height: 3
                radius: 2
                rotation: 38
                color: root.accentColor
            }

            Rectangle {
                x: 85
                y: 41
                width: 10
                height: 3
                radius: 2
                rotation: -38
                color: root.accentColor
            }

            Rectangle {
                x: 95
                y: 19
                width: 22
                height: 37
                radius: 7
                color: root.palette.button
                border.width: 2
                border.color: root.palette.placeholderText
            }
        }
    }

    Component {
        id: shortcutsGraphic

        Item {
            Flow {
                x: 7
                y: 12
                width: 75
                spacing: 5

                Repeater {
                    model: ["S", "U", "B", "1", "2"]

                    Rectangle {
                        id: shortcutKey

                        required property string modelData
                        required property int index

                        width: index < 3 ? 20 : 31
                        height: 20
                        radius: 5
                        color: index === 1
                            ? root.accentSoft
                            : root.palette.button
                        border.color: index === 1
                            ? root.accentColor
                            : root.palette.mid

                        Label {
                            anchors.centerIn: parent
                            text: shortcutKey.modelData
                            color: shortcutKey.index === 1
                                ? root.accentColor
                                : root.palette.text
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }

            Rectangle {
                x: 91
                y: 14
                width: 3
                height: 46
                radius: 2
                color: root.palette.placeholderText
            }

            Repeater {
                model: 2

                Rectangle {
                    required property int index

                    x: 92
                    y: 20 + index * 28
                    width: 23
                    height: 3
                    radius: 2
                    color: index === 0
                        ? root.accentColor
                        : root.palette.placeholderText
                }
            }
        }
    }

    Component {
        id: systemGraphic

        Item {
            Item {
                x: 10
                y: 7
                width: 62
                height: 62

                Repeater {
                    model: 8

                    Rectangle {
                        required property int index

                        x: 28 + Math.cos(index * Math.PI / 4) * 23
                        y: 28 + Math.sin(index * Math.PI / 4) * 23
                        width: 8
                        height: 14
                        radius: 3
                        rotation: index * 45 + 90
                        color: index % 2 === 0
                            ? root.accentColor
                            : root.palette.placeholderText
                    }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 38
                    height: 38
                    radius: 19
                    color: root.palette.button
                    border.width: 2
                    border.color: root.accentColor

                    Rectangle {
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        radius: 6
                        color: root.accentSoft
                    }
                }
            }

            Column {
                x: 85
                y: 14
                spacing: 7

                Repeater {
                    model: 4

                    Rectangle {
                        required property int index

                        width: 29
                        height: 7
                        radius: 4
                        color: index === 2
                            ? root.accentColor
                            : root.palette.button
                        border.color: index === 2
                            ? root.accentColor
                            : root.palette.mid
                    }
                }
            }
        }
    }

    Component {
        id: securityGraphic

        Item {
            Rectangle {
                x: 28
                y: 28
                width: 55
                height: 39
                radius: 10
                color: root.palette.button
                border.width: 2
                border.color: root.accentColor

                Rectangle {
                    anchors.centerIn: parent
                    width: 8
                    height: 14
                    radius: 4
                    color: root.accentColor
                }
            }

            Rectangle {
                x: 40
                y: 8
                width: 31
                height: 35
                radius: 15
                color: "transparent"
                border.width: 4
                border.color: root.palette.placeholderText
            }

            Repeater {
                model: 3

                Rectangle {
                    required property int index

                    x: 92 + index * 8
                    y: 18 + index * 16
                    width: 9
                    height: 9
                    radius: 5
                    color: index === 1
                        ? root.accentColor
                        : root.palette.placeholderText
                }
            }
        }
    }
}
