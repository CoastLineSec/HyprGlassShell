import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.Client

ApplicationWindow {
    id: root

    width: 1080
    height: 720
    minimumWidth: 620
    minimumHeight: 480
    visible: true
    title: qsTr("HyprShelld Settings")
    color: "#101319"

    readonly property real sidebarWidth: Math.min(232, Math.max(196, width * 0.22))
    readonly property int failedComponentCount: shellHealthWarning.failedComponentCount
    readonly property string desktopStatusText: {
        if (CoordinatorClient.available) {
            if (CoordinatorClient.healthy)
                return qsTr("Desktop ready");
            return failedComponentCount === 1
                ? qsTr("1 component failed")
                : qsTr("%1 components failed").arg(failedComponentCount);
        }
        if (shellRuntimeStatus.busy && !shellRuntimeStatus.available)
            return qsTr("Checking services…");
        if (shellRuntimeStatus.available) {
            if (failedComponentCount === 1)
                return qsTr("1 component failed");
            if (failedComponentCount > 1)
                return qsTr("%1 components failed").arg(failedComponentCount);
            return shellRuntimeStatus.targetState === "inactive"
                ? qsTr("Desktop services stopped")
                : qsTr("Health service unavailable");
        }
        return qsTr("Status unavailable");
    }
    readonly property color desktopStatusColor: {
        if (failedComponentCount > 0)
            return "#fb7185";
        if (CoordinatorClient.available && CoordinatorClient.healthy)
            return "#68d391";
        return "#f6ad55";
    }

    palette.window: "#101319"
    palette.windowText: "#f4f6fa"
    palette.base: "#171b22"
    palette.text: "#f4f6fa"
    palette.button: "#29303b"
    palette.buttonText: "#f4f6fa"
    palette.highlight: "#7c91ff"
    palette.highlightedText: "#ffffff"
    palette.placeholderText: "#aeb8c6"
    palette.mid: "#3a424f"

    ShellRuntimeStatus {
        id: shellRuntimeStatus

        active: !CoordinatorClient.available
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: root.sidebarWidth
            color: root.palette.base

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    Layout.topMargin: 4
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 11
                        color: root.palette.highlight

                        Label {
                            anchors.centerIn: parent
                            text: "H"
                            color: root.palette.highlightedText
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.ignored: true
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Settings")
                        color: root.palette.text
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    text: qsTr("Desktop")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                ItemDelegate {
                    id: barNavigationItem

                    objectName: "barNavigationItem"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    checkable: true
                    checked: true
                    autoExclusive: true
                    focusPolicy: Qt.StrongFocus
                    leftPadding: 18
                    rightPadding: 12
                    topPadding: 8
                    bottomPadding: 8
                    Accessible.role: Accessible.PageTab
                    Accessible.name: qsTr("Bar settings")
                    Accessible.checked: checked

                    background: Rectangle {
                        radius: 12
                        color: barNavigationItem.checked
                            ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18)
                            : barNavigationItem.hovered
                                ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06)
                                : "transparent"
                        border.width: barNavigationItem.activeFocus ? 2 : 0
                        border.color: root.palette.highlight

                        Rectangle {
                            anchors {
                                left: parent.left
                                leftMargin: 5
                                verticalCenter: parent.verticalCenter
                            }

                            width: 3
                            height: 24
                            radius: 2
                            visible: barNavigationItem.checked
                            color: root.palette.highlight
                        }
                    }

                    contentItem: RowLayout {
                        spacing: 11

                        Item {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22

                            Rectangle {
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    top: parent.top
                                    topMargin: 3
                                }

                                height: 7
                                radius: 3
                                color: root.palette.text
                            }

                            Rectangle {
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    bottom: parent.bottom
                                }

                                height: 8
                                radius: 3
                                color: root.palette.mid
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Bar")
                            color: root.palette.text
                            font.pixelSize: 14
                            font.weight: barNavigationItem.checked ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    radius: 12
                    color: Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.04)
                    border.color: root.palette.mid

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 9

                        Rectangle {
                            Layout.preferredWidth: 9
                            Layout.preferredHeight: 9
                            radius: width / 2
                            color: root.desktopStatusColor
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.desktopStatusText
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: root.palette.mid
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: shellHealthWarning.warningVisible
                    ? shellHealthWarning.implicitHeight + 20
                    : 0
                visible: shellHealthWarning.warningVisible

                ShellHealthWarning {
                    id: shellHealthWarning

                    anchors {
                        top: parent.top
                        horizontalCenter: parent.horizontalCenter
                    }
                    width: Math.max(0, Math.min(parent.width - 48, 980))
                    coordinatorAvailable: CoordinatorClient.available
                    coordinatorHealthy: CoordinatorClient.healthy
                    coordinatorFailedUnits: CoordinatorClient.failedUnits
                    restartBusy: CoordinatorClient.busy
                    restartingUnit: CoordinatorClient.restartingUnit
                    restartErrorUnit: CoordinatorClient.lastErrorUnit
                    restartError: CoordinatorClient.lastErrorMessage
                    fallbackActive: shellRuntimeStatus.active
                    fallbackAvailable: shellRuntimeStatus.available
                    fallbackBusy: shellRuntimeStatus.busy
                    targetState: shellRuntimeStatus.targetState
                    coordinatorState: shellRuntimeStatus.coordinatorState
                    configurationState: shellRuntimeStatus.configurationState
                    surfaceState: shellRuntimeStatus.surfaceState

                    onRestartRequested: unitName => {
                        CoordinatorClient.restartComponent(unitName);
                    }
                }
            }

            BarSettingsPage {
                Layout.fillWidth: true
                Layout.fillHeight: true
                barHeight: ConfigClient.barHeight
                minimumBarHeight: ConfigClient.minimumBarHeight
                maximumBarHeight: ConfigClient.maximumBarHeight
                defaultBarHeight: ConfigClient.defaultBarHeight
                serviceAvailable: ConfigClient.available
                busy: ConfigClient.busy
                errorText: ConfigClient.lastErrorMessage
                recoveryState: ConfigClient.recoveryState

                onBarHeightRequested: height => ConfigClient.setBarHeight(height)
                onResetBarHeightRequested: ConfigClient.resetBarHeight()
            }
        }
    }
}
