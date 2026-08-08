import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Page {
    id: root

    required property int barHeight
    required property int minimumBarHeight
    required property int maximumBarHeight
    required property int defaultBarHeight
    property bool serviceAvailable: false
    property bool busy: false
    property string errorText: ""
    property string recoveryState: ""
    property bool previewAnimationsEnabled: true
    property real contentTopMargin: 28

    readonly property string recoveryMessage: {
        if (recoveryState === "recovered")
            return qsTr("Your settings were restored from the last known good copy because the main file could not be read.");
        if (recoveryState === "defaulted")
            return qsTr("Your settings could not be recovered, so safe defaults are in use. Review your choices before continuing.");
        return "";
    }
    readonly property bool serviceWarningVisible: !serviceAvailable
    readonly property bool recoveryWarningVisible: recoveryMessage.length > 0
    readonly property bool controlsEnabled: serviceAvailable && !busy

    signal barHeightRequested(int height)
    signal resetBarHeightRequested()

    background: Rectangle {
        color: root.palette.window
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            x: Math.max(24, (root.width - width) / 2)
            y: root.contentTopMargin
            width: Math.max(0, Math.min(root.width - 48, 980))
            spacing: 20

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        text: qsTr("Bar")
                        color: root.palette.text
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Shape how the bar looks and uses space on every display.")
                        color: root.palette.placeholderText
                        font.pixelSize: 14
                        wrapMode: Text.Wrap
                    }
                }

                Rectangle {
                    Layout.preferredWidth: liveStatusLabel.implicitWidth + 34
                    Layout.preferredHeight: 34
                    radius: 17
                    color: Qt.rgba(
                        root.palette.highlight.r,
                        root.palette.highlight.g,
                        root.palette.highlight.b,
                        0.12
                    )
                    border.color: Qt.rgba(
                        root.palette.highlight.r,
                        root.palette.highlight.g,
                        root.palette.highlight.b,
                        0.36
                    )

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 7

                        Rectangle {
                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: width / 2
                            color: root.palette.highlight
                        }

                        Label {
                            id: liveStatusLabel

                            text: qsTr("Live preview")
                            color: root.palette.text
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Frame {
                id: serviceWarning

                objectName: "serviceWarning"
                Layout.fillWidth: true
                visible: root.serviceWarningVisible
                padding: 16

                background: Rectangle {
                    color: "#33251a"
                    radius: 12
                    border.color: "#8bf6ad55"
                }

                Label {
                    anchors.fill: parent
                    text: qsTr("Settings service is unavailable. Displayed values may be stale, and changes are disabled until it reconnects.")
                    color: "#ffd5a1"
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }
            }

            Frame {
                id: recoveryWarning

                objectName: "recoveryWarning"
                Layout.fillWidth: true
                visible: root.recoveryWarningVisible
                padding: 16

                background: Rectangle {
                    color: root.recoveryState === "defaulted" ? "#382125" : "#1c2f34"
                    radius: 12
                    border.color: root.recoveryState === "defaulted" ? "#8bfb7185" : "#8b63d7e6"
                }

                Label {
                    anchors.fill: parent
                    text: root.recoveryMessage
                    color: root.recoveryState === "defaulted" ? "#ffb8c3" : "#b9eef4"
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Desktop preview")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        text: qsTr("%1 px").arg(heightControl.previewValue)
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                    }
                }

                BarPreview {
                    id: barPreview

                    objectName: "barPreview"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 286
                    barHeight: heightControl.previewValue
                    adjusting: heightControl.adjusting
                    configurationAvailable: root.serviceAvailable
                    animationsEnabled: root.previewAnimationsEnabled
                }
            }

            Frame {
                Layout.fillWidth: true
                padding: 22

                background: Rectangle {
                    color: root.palette.base
                    radius: 16
                    border.color: root.palette.mid
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 18

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Label {
                            text: qsTr("Size")
                            color: root.palette.text
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Changes are previewed immediately and saved when you release the control.")
                            color: root.palette.placeholderText
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                        }
                    }

                    BarHeightControl {
                        id: heightControl

                        objectName: "barHeightControl"
                        Layout.fillWidth: true
                        value: root.barHeight
                        minimumValue: root.minimumBarHeight
                        maximumValue: root.maximumBarHeight
                        defaultValue: root.defaultBarHeight
                        busy: !root.controlsEnabled
                        errorText: root.errorText
                        enabled: root.controlsEnabled

                        onValueRequested: value => root.barHeightRequested(value)
                        onResetRequested: root.resetBarHeightRequested()
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Changes are saved automatically and applied to every bar.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }
            }

            Item {
                Layout.preferredHeight: 12
            }
        }
    }
}
