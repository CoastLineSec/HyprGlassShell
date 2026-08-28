pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Frame {
    id: root

    property string mode: "dark"
    property string effectiveMode: "dark"
    property bool serviceAvailable: false
    property bool busy: false
    property string errorText: ""

    signal modeRequested(string mode)

    readonly property int minimumTargetSize: 44
    readonly property var modes: [
        {
            mode: "automatic",
            label: qsTr("Automatic"),
            description: qsTr("Follow desktop, schedule, or Night Light")
        },
        {
            mode: "light",
            label: qsTr("Light"),
            description: qsTr("A calm, low-glare light palette")
        },
        {
            mode: "dark",
            label: qsTr("Dark"),
            description: qsTr("A deep, layered dark palette")
        }
    ]

    objectName: "themeModeSelector"
    padding: 18

    background: Rectangle {
        color: root.palette.base
        radius: 16
        border.color: root.palette.mid
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Color mode")
                    color: root.palette.text
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("One paired color system for Settings and the desktop shell.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            Label {
                objectName: "themeAutomaticEffectiveBadge"
                visible: root.mode === "automatic"
                text: root.effectiveMode === "light"
                    ? qsTr("Using Light") : qsTr("Using Dark")
                color: root.palette.highlight
                font.pixelSize: 12
                font.weight: Font.DemiBold
                leftPadding: 9
                rightPadding: 9
                topPadding: 5
                bottomPadding: 5

                background: Rectangle {
                    radius: 10
                    color: Qt.rgba(
                        root.palette.highlight.r,
                        root.palette.highlight.g,
                        root.palette.highlight.b,
                        0.13
                    )
                    border.color: root.palette.highlight
                }
            }
        }

        GridLayout {
            id: modeGrid

            Layout.fillWidth: true
            columns: 3
            columnSpacing: 10
            rowSpacing: 10

            Repeater {
                id: modeRepeater
                model: root.modes

                delegate: AbstractButton {
                    id: modeButton

                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: 164
                    implicitHeight: 164
                    // The service-backed mode is authoritative. Keeping this
                    // non-checkable prevents AbstractButton from replacing the
                    // checked binding while a request is still pending.
                    checkable: false
                    checked: root.mode === modeButton.modelData.mode
                    enabled: root.serviceAvailable && !root.busy
                    focusPolicy: Qt.StrongFocus
                    hoverEnabled: true
                    objectName: "themeMode-" + modeButton.modelData.mode

                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modeButton.modelData.label
                    Accessible.description:
                        modeButton.modelData.mode === "automatic"
                        ? qsTr("Use the selected automatic source. Currently using %1.")
                              .arg(root.effectiveMode === "light"
                                  ? qsTr("Light") : qsTr("Dark"))
                        : modeButton.modelData.description
                    Accessible.checked: checked

                    onClicked:
                        root.modeRequested(modeButton.modelData.mode)

                    function moveAndRequest(offset) {
                        const targetIndex = (index + offset
                            + modeRepeater.count) % modeRepeater.count;
                        const target = modeRepeater.itemAt(targetIndex);
                        if (!target)
                            return;
                        target.forceActiveFocus();
                        const targetMode = root.modes[targetIndex].mode;
                        if (target.enabled && targetMode !== root.mode)
                            root.modeRequested(targetMode);
                    }

                    Keys.onLeftPressed: event => {
                        modeButton.moveAndRequest(-1);
                        event.accepted = true;
                    }
                    Keys.onRightPressed: event => {
                        modeButton.moveAndRequest(1);
                        event.accepted = true;
                    }

                    background: Rectangle {
                        radius: 14
                        color: modeButton.hovered || modeButton.down
                            ? ShellTheme.overlay(
                                  root.palette.text,
                                  modeButton.down ? 0.14 : 0.07,
                                  root.palette.button
                              )
                            : root.palette.button
                        border.width: modeButton.activeFocus ? 4
                            : modeButton.checked ? 3 : 1
                        border.color: modeButton.activeFocus
                            ? root.palette.text
                            : modeButton.checked
                                ? root.palette.highlight : root.palette.mid

                        Behavior on color {
                            ColorAnimation {
                                duration: ShellTheme.transitionDuration
                            }
                        }
                    }

                    contentItem: ColumnLayout {
                        spacing: 8

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 90

                            Rectangle {
                                id: modePreview
                                anchors.fill: parent
                                radius: 9
                                clip: true
                                color: modeButton.modelData.mode === "automatic"
                                    ? "transparent"
                                    : ShellTheme.colorFor(
                                          modeButton.modelData.mode, "canvas"
                                      )
                                border.color: ShellTheme.colorFor(
                                    modeButton.modelData.mode === "automatic"
                                        ? root.effectiveMode
                                        : modeButton.modelData.mode,
                                    "outline"
                                )

                                Row {
                                    anchors.fill: parent
                                    visible:
                                        modeButton.modelData.mode === "automatic"

                                    Repeater {
                                        model: ["light", "dark"]

                                        Rectangle {
                                            id: automaticHalf

                                            required property string modelData
                                            width: modePreview.width / 2
                                            height: modePreview.height
                                            color: ShellTheme.colorFor(
                                                automaticHalf.modelData,
                                                "canvas"
                                            )

                                            Rectangle {
                                                anchors {
                                                    left: parent.left
                                                    right: parent.right
                                                    top: parent.top
                                                    margins: 7
                                                }
                                                height: 13
                                                radius: 4
                                                color: ShellTheme.colorFor(
                                                    automaticHalf.modelData,
                                                    "floating"
                                                )
                                            }

                                            Rectangle {
                                                anchors {
                                                    left: parent.left
                                                    right: parent.right
                                                    top: parent.top
                                                    bottom: parent.bottom
                                                    leftMargin: 7
                                                    rightMargin: 7
                                                    topMargin: 27
                                                    bottomMargin: 7
                                                }
                                                radius: 5
                                                color: ShellTheme.colorFor(
                                                    automaticHalf.modelData,
                                                    "card"
                                                )

                                                Rectangle {
                                                    anchors {
                                                        left: parent.left
                                                        right: parent.right
                                                        top: parent.top
                                                        margins: 7
                                                    }
                                                    height: 5
                                                    radius: 3
                                                    color: ShellTheme.colorFor(
                                                        automaticHalf.modelData,
                                                        "primary"
                                                    )
                                                }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    visible:
                                        modeButton.modelData.mode !== "automatic"
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    height: 14
                                    radius: 5
                                    color: ShellTheme.colorFor(
                                        modeButton.modelData.mode, "floating"
                                    )

                                    Row {
                                        anchors {
                                            left: parent.left
                                            verticalCenter: parent.verticalCenter
                                            leftMargin: 6
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
                                                    ? ShellTheme.colorFor(
                                                          modeButton.modelData.mode,
                                                          "primary"
                                                      )
                                                    : ShellTheme.colorFor(
                                                          modeButton.modelData.mode,
                                                          "outlineStrong"
                                                      )
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    visible:
                                        modeButton.modelData.mode !== "automatic"
                                    anchors {
                                        left: parent.left
                                        top: parent.top
                                        bottom: parent.bottom
                                        leftMargin: 8
                                        topMargin: 30
                                        bottomMargin: 8
                                    }
                                    width: Math.max(18, parent.width * 0.25)
                                    radius: 5
                                    color: ShellTheme.colorFor(
                                        modeButton.modelData.mode, "card"
                                    )

                                    Rectangle {
                                        anchors {
                                            left: parent.left
                                            right: parent.right
                                            top: parent.top
                                            margins: 5
                                        }
                                        height: 5
                                        radius: 3
                                        color: ShellTheme.colorFor(
                                            modeButton.modelData.mode, "primary"
                                        )
                                    }
                                }

                                Rectangle {
                                    visible:
                                        modeButton.modelData.mode !== "automatic"
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        bottom: parent.bottom
                                        leftMargin: Math.max(
                                            32, parent.width * 0.25 + 14
                                        )
                                        rightMargin: 8
                                        topMargin: 30
                                        bottomMargin: 8
                                    }
                                    radius: 5
                                    color: ShellTheme.colorFor(
                                        modeButton.modelData.mode, "floating"
                                    )
                                }
                            }

                            Rectangle {
                                anchors {
                                    right: parent.right
                                    top: parent.top
                                    margins: 7
                                }
                                visible: modeButton.checked
                                width: 22
                                height: 22
                                radius: 11
                                color: root.palette.highlight

                                Label {
                                    anchors.centerIn: parent
                                    text: "✓"
                                    color: root.palette.highlightedText
                                    font.pixelSize: 13
                                    font.weight: Font.Bold
                                    Accessible.ignored: true
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: modeButton.modelData.label
                            color: root.palette.text
                            font.pixelSize: 13
                            font.weight: modeButton.checked
                                ? Font.DemiBold : Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: modeButton.modelData.description
                            color: root.palette.placeholderText
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Label {
            objectName: "themeModeError"
            Layout.fillWidth: true
            visible: !root.serviceAvailable || root.errorText.length > 0
            text: root.errorText.length > 0
                ? root.errorText
                : qsTr("The settings service is unavailable. Your current display mode is still being used.")
            color: root.errorText.length > 0
                ? ShellTheme.onErrorContainer : root.palette.placeholderText
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: root.errorText.length > 0
                ? Accessible.AlertMessage : Accessible.StaticText
            Accessible.name: text

            background: Rectangle {
                visible: root.errorText.length > 0
                radius: 8
                color: ShellTheme.errorContainer
                border.color: ShellTheme.errorOutline
            }
            leftPadding: root.errorText.length > 0 ? 10 : 0
            rightPadding: root.errorText.length > 0 ? 10 : 0
            topPadding: root.errorText.length > 0 ? 8 : 0
            bottomPadding: root.errorText.length > 0 ? 8 : 0
        }
    }
}
