pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    property var device: null
    property var overrideDefinitions: []
    property var groups: []
    property var invalidOverrideKeys: []
    property bool controlsEnabled: false
    property string deviceIssue: ""
    property real minimumTargetSize: 44
    property bool compact: width < 620
    property var kindValues: [
        "keyboard", "pointer", "touchpad", "touch", "tablet",
        "tabletTool", "switch", "other"
    ]
    property var kindLabels: [
        qsTr("Keyboard"), qsTr("Pointing device"), qsTr("Touchpad"),
        qsTr("Touch device"), qsTr("Drawing tablet"),
        qsTr("Tablet tool"), qsTr("Switch"), qsTr("Other")
    ]

    signal closeRequested()
    signal removeRequested(string id)
    signal propertyModified(string id, string propertyName, var value)
    signal overrideModified(
        string id, string key, bool included, var value
    )

    function deviceId() {
        return root.device && typeof root.device.id === "string"
            ? root.device.id : "";
    }

    function overrideIncluded(key) {
        return !!root.device && !!root.device.overrides
            && typeof root.device.overrides === "object"
            && !Array.isArray(root.device.overrides)
            && Object.prototype.hasOwnProperty.call(root.device.overrides, key);
    }

    function overrideValue(key) {
        return root.overrideIncluded(key)
            ? root.device.overrides[key] : undefined;
    }

    function definitionsForGroup(group) {
        return Array.isArray(root.overrideDefinitions)
            ? root.overrideDefinitions.filter(item => item.group === group)
            : [];
    }

    function kindIndex(kind) {
        return Math.max(0, root.kindValues.indexOf(kind));
    }

    function overrideCount() {
        if (!root.device || !root.device.overrides
                || typeof root.device.overrides !== "object"
                || Array.isArray(root.device.overrides)) {
            return 0;
        }
        return Object.keys(root.device.overrides).length;
    }

    objectName: "inputDeviceEditorScrollView"
    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    ColumnLayout {
        objectName: "inputDeviceEditorContent"
        width: root.availableWidth
        spacing: root.compact ? 14 : 18

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                objectName: "closeInputDeviceEditorButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Back to devices")
                Accessible.name: qsTr("Close the selected input-device editor")

                onClicked: root.closeRequested()
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                horizontalAlignment: Text.AlignRight
                text: qsTr("Editing per-device configuration")
                color: root.palette.placeholderText
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
        }

        InputDevicePreview {
            kind: root.device ? String(root.device.kind) : "other"
            selector: root.device ? String(root.device.selector) : ""
            deviceEnabled: !!root.device && root.device.enabled === true
            overrideCount: root.overrideCount()
            compact: root.compact
        }

        Frame {
            objectName: "inputDeviceIdentityCard"
            Layout.fillWidth: true
            padding: root.compact ? 14 : 18

            background: Rectangle {
                color: root.palette.base
                radius: 16
                border.color: root.deviceIssue.length === 0
                    ? root.palette.mid : "#a8606a"
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Device identity")
                    color: root.palette.text
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("The selector is Hyprland's exact per-session device name. Spaces and hyphens share one natural identity, so duplicate forms are rejected. The stable record ID is managed internally.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Record ID · %1").arg(root.deviceId())
                    color: "#bca8dc"
                    font.family: "monospace"
                    font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Hyprland device selector")
                    color: root.palette.text
                    font.weight: Font.Medium
                    textFormat: Text.PlainText
                }

                TextField {
                    objectName: "inputDeviceSelectorField"
                    Layout.fillWidth: true
                    implicitHeight: root.minimumTargetSize
                    enabled: root.controlsEnabled
                    maximumLength: 256
                    selectByMouse: true
                    text: root.device ? String(root.device.selector) : ""
                    placeholderText: qsTr("Exact name from hyprctl devices")
                    Accessible.name: qsTr("Exact Hyprland input-device selector")

                    onTextEdited: root.propertyModified(
                        root.deviceId(), "selector", text
                    )
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Device category")
                    color: root.palette.text
                    font.weight: Font.Medium
                    textFormat: Text.PlainText
                }

                ComboBox {
                    objectName: "inputDeviceKindSelect"
                    Layout.fillWidth: true
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    model: root.kindLabels
                    currentIndex: root.kindIndex(
                        root.device ? root.device.kind : "other"
                    )
                    enabled: root.controlsEnabled
                    Accessible.name: qsTr("Input-device category")

                    onActivated: index => {
                        if (index >= 0 && index < root.kindValues.length) {
                            root.propertyModified(
                                root.deviceId(), "kind",
                                root.kindValues[index]
                            );
                        }
                    }
                }

                SettingsToggleRow {
                    Layout.fillWidth: true
                    title: qsTr("Device record enabled")
                    description: qsTr("Disabled records stay saved and ordered. The Lua output keeps the device selector and emits enabled = false.")
                    checked: !!root.device && root.device.enabled === true
                    enabled: root.controlsEnabled
                    controlObjectName: "inputDeviceEnabledSwitch"
                    accessibleName: qsTr("Enable this managed device record")
                    minimumTargetSize: root.minimumTargetSize

                    onValueModified: value => root.propertyModified(
                        root.deviceId(), "enabled", value
                    )
                }

                Label {
                    objectName: "inputDeviceEditorIssue"
                    Layout.fillWidth: true
                    visible: root.deviceIssue.length > 0
                    text: root.deviceIssue
                    color: "#ffb8c3"
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }
            }
        }

        Frame {
            objectName: "inputDeviceOverrideExplanationCard"
            Layout.fillWidth: true
            padding: root.compact ? 14 : 18

            background: Rectangle {
                color: "#252032"
                radius: 16
                border.color: "#6f5c90"
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Overrides are explicit")
                    color: "#d6c0fa"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Unchecked values inherit the global Input configuration. Checked values are emitted inside this device's hl.device({...}) record. Hyprland and libinput may ignore a valid field when the selected hardware does not support that capability.")
                    color: "#c7b9da"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }
        }

        Repeater {
            model: Array.isArray(root.groups) ? root.groups : []

            Frame {
                id: groupCard

                required property var modelData

                objectName: "inputDeviceOverridesGroup_"
                    + String(groupCard.modelData.key)
                Layout.fillWidth: true
                padding: root.compact ? 12 : 16

                background: Rectangle {
                    color: root.palette.base
                    radius: 16
                    border.color: root.palette.mid
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        text: String(groupCard.modelData.title)
                        color: root.palette.text
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: String(groupCard.modelData.description)
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }

                    Repeater {
                        model: root.definitionsForGroup(
                            String(groupCard.modelData.key)
                        )

                        InputDeviceOverrideRow {
                            required property var modelData

                            definition: modelData
                            included: root.overrideIncluded(modelData.key)
                            value: root.overrideValue(modelData.key)
                            fieldValid: !root.invalidOverrideKeys.includes(
                                modelData.key
                            )
                            controlsEnabled: root.controlsEnabled
                            minimumTargetSize: root.minimumTargetSize

                            onIncludeModified: included =>
                                root.overrideModified(
                                    root.deviceId(), modelData.key, included,
                                    included ? modelData.defaultValue : undefined
                                )
                            onValueModified: value =>
                                root.overrideModified(
                                    root.deviceId(), modelData.key, true, value
                                )
                        }
                    }
                }
            }
        }

        Frame {
            objectName: "inputDeviceEditorActionsCard"
            Layout.fillWidth: true
            padding: root.compact ? 14 : 18

            background: Rectangle {
                color: "#242131"
                radius: 16
                border.color: root.deviceIssue.length === 0
                    ? "#7b68a1" : "#a8606a"
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Device records are saved as one ordered collection")
                    color: root.palette.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Return to the list to review ordering and save. Any device change is restart-required; saving does not claim that the running session already uses these values.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Flow {
                    Layout.fillWidth: true
                    Layout.preferredHeight: childrenRect.height
                    spacing: 10

                    Button {
                        objectName: "doneEditingInputDeviceButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Done")
                        Accessible.name: qsTr("Return to the input-device list")

                        onClicked: root.closeRequested()
                    }

                    Button {
                        objectName: "removeEditedInputDeviceButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Remove from draft")
                        enabled: root.controlsEnabled
                        Accessible.name: qsTr("Remove this managed input device")

                        onClicked: root.removeRequested(root.deviceId())
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 10 }
    }
}
