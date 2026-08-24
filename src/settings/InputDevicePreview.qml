pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    property string kind: "other"
    property string selector: ""
    property bool deviceEnabled: true
    property int overrideCount: 0
    property bool compact: width < 560

    function kindLabel() {
        const labels = {
            keyboard: qsTr("Keyboard"),
            pointer: qsTr("Pointing device"),
            touchpad: qsTr("Touchpad"),
            touch: qsTr("Touch device"),
            tablet: qsTr("Drawing tablet"),
            tabletTool: qsTr("Tablet tool"),
            switch: qsTr("Switch"),
            other: qsTr("Other input device")
        };
        return labels[root.kind] || labels.other;
    }

    objectName: "inputDeviceVisualPreview"
    Layout.fillWidth: true
    padding: root.compact ? 14 : 18
    Accessible.name: qsTr("%1 preview for %2. %3 with %4 overrides.").arg(root.kindLabel()).arg(root.selector.length > 0 ? root.selector : qsTr("unnamed device")).arg(root.deviceEnabled ? qsTr("Enabled") : qsTr("Disabled")).arg(root.overrideCount)

    background: Rectangle {
        color: "#201f2c"
        radius: 18
        border.color: root.deviceEnabled ? "#8068ad" : "#5b5866"
    }

    RowLayout {
        anchors.fill: parent
        spacing: root.compact ? 14 : 22

        Item {
            id: stage

            Layout.preferredWidth: root.compact ? 154 : 210
            Layout.preferredHeight: root.compact ? 128 : 154

            Rectangle {
                anchors.fill: parent
                radius: 16
                color: "#171621"
                border.color: "#484259"
            }

            Item {
                id: glyph

                anchors.centerIn: parent
                width: stage.width - 34
                height: stage.height - 34
                opacity: root.deviceEnabled ? 1 : 0.45

                Rectangle {
                    id: keyboardBody

                    visible: root.kind === "keyboard"
                    anchors.centerIn: parent
                    width: glyph.width
                    height: Math.min(glyph.height, width * 0.54)
                    radius: 10
                    color: "#302b42"
                    border.color: "#a48bd0"

                    Grid {
                        anchors.fill: parent
                        anchors.margins: 10
                        columns: 5
                        spacing: 5

                        Repeater {
                            model: 15

                            Rectangle {
                                required property int index

                                width: (keyboardBody.width - 40) / 5
                                height: (keyboardBody.height - 30) / 3
                                radius: 3
                                color: index === 12 ? "#8f72c1" : "#504665"
                                border.color: "#74658d"
                            }
                        }
                    }
                }

                Rectangle {
                    id: pointerBody

                    visible: root.kind === "pointer"
                    anchors.centerIn: parent
                    width: Math.min(78, glyph.width * 0.48)
                    height: Math.min(112, glyph.height)
                    radius: width / 2
                    color: "#302b42"
                    border.color: "#a48bd0"

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 9
                        width: 4
                        height: 24
                        radius: 2
                        color: "#a98ada"
                    }

                    Rectangle {
                        x: parent.width / 2 - 1
                        y: 0
                        width: 2
                        height: parent.height * 0.44
                        color: "#5e526f"
                    }

                    Rectangle {
                        x: 5
                        y: parent.height * 0.44
                        width: parent.width - 10
                        height: 2
                        color: "#5e526f"
                    }
                }

                Rectangle {
                    id: touchpadBody

                    visible: root.kind === "touchpad"
                    anchors.centerIn: parent
                    width: glyph.width * 0.9
                    height: glyph.height * 0.72
                    radius: 12
                    color: "#302b42"
                    border.color: "#a48bd0"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 7
                        height: 22
                        radius: 6
                        color: "#463d58"
                        border.color: "#665879"

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 1
                            height: parent.height
                            color: "#6e607f"
                        }
                    }

                    Rectangle {
                        x: parent.width * 0.62
                        y: parent.height * 0.24
                        width: 13
                        height: 13
                        radius: 7
                        color: "#aa8bda"
                    }
                }

                Rectangle {
                    id: tabletBody

                    visible: ["touch", "tablet", "tabletTool"].includes(root.kind)
                    anchors.centerIn: parent
                    width: glyph.width * 0.9
                    height: glyph.height * 0.78
                    radius: 9
                    color: "#292538"
                    border.width: 2
                    border.color: "#a48bd0"

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 11
                        radius: 5
                        color: "#171621"
                        border.color: "#4e4560"

                        Rectangle {
                            x: parent.width * 0.18
                            y: parent.height * 0.62
                            width: parent.width * 0.55
                            height: 4
                            rotation: -24
                            radius: 2
                            color: "#8f72c1"
                        }

                        Rectangle {
                            x: parent.width * 0.65
                            y: parent.height * 0.24
                            width: 15
                            height: 15
                            radius: 8
                            color: "#d0b5ff"
                        }
                    }
                }

                Rectangle {
                    id: genericBody

                    visible: !["keyboard", "pointer", "touchpad", "touch", "tablet", "tabletTool"].includes(root.kind)
                    anchors.centerIn: parent
                    width: Math.min(glyph.width * 0.78, 124)
                    height: Math.min(glyph.height * 0.78, 104)
                    radius: 14
                    color: "#302b42"
                    border.color: "#a48bd0"

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.48
                        height: parent.height * 0.48
                        radius: 10
                        color: "#514568"
                        border.color: "#c0a2eb"
                    }

                    Repeater {
                        model: 4

                        Rectangle {
                            required property int index

                            x: index < 2 ? 10 : genericBody.width - 18
                            y: index % 2 === 0 ? 18 : genericBody.height - 26
                            width: 8
                            height: 8
                            radius: 4
                            color: "#a98ada"
                        }
                    }
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: 18
                    height: 18
                    radius: 9
                    color: root.deviceEnabled ? "#79d39a" : "#8c8994"
                    border.color: "#d9f5e3"
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 7

            Label {
                Layout.fillWidth: true
                text: root.kindLabel()
                color: "#f2edf7"
                font.pixelSize: root.compact ? 18 : 22
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Label {
                Layout.fillWidth: true
                text: root.selector.length > 0 ? root.selector : qsTr("No Hyprland selector yet")
                color: "#c9b7e8"
                font.family: "monospace"
                font.pixelSize: 12
                wrapMode: Text.WrapAnywhere
                textFormat: Text.PlainText
            }

            Label {
                Layout.fillWidth: true
                text: root.deviceEnabled ? qsTr("The record is emitted into the managed Lua device list.") : qsTr("The record remains ordered and saved, but Hyprland receives enabled = false.")
                color: "#b8b2c4"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            RowLayout {
                spacing: 8

                Label {
                    text: root.deviceEnabled ? qsTr("ENABLED") : qsTr("DISABLED")
                    color: root.deviceEnabled ? "#9ce4b5" : "#c3c0ca"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    textFormat: Text.PlainText
                }

                Rectangle {
                    Layout.preferredWidth: 4
                    Layout.preferredHeight: 4
                    radius: 2
                    color: "#8f899a"
                }

                Label {
                    text: root.overrideCount === 1 ? qsTr("1 override") : qsTr("%1 overrides").arg(root.overrideCount)
                    color: "#b8b2c4"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    textFormat: Text.PlainText
                }
            }
        }
    }
}
