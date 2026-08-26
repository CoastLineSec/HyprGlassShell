pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    property var binding: null
    property var submaps: []
    property var actions: []
    property bool controlsEnabled: false
    property string issue: ""
    property string bindingOrigin: "custom"
    property bool canReset: false
    property real minimumTargetSize: 44
    property string argumentsIssue: ""
    property string deviceIssue: ""

    signal recordModified(var record)
    signal closeRequested
    signal removeRequested(string id)
    signal resetRequested(string id)

    readonly property var modifierOrder: ["shift", "caps", "ctrl", "alt", "mod2", "mod3", "super", "mod5"]
    readonly property var optionDefinitions: [
        {
            key: "repeating",
            label: qsTr("Repeat"),
            description: qsTr("Repeat while held")
        },
        {
            key: "locked",
            label: qsTr("Locked"),
            description: qsTr("Works while session is locked")
        },
        {
            key: "release",
            label: qsTr("On release"),
            description: qsTr("Run when the key is released")
        },
        {
            key: "nonConsuming",
            label: qsTr("Non-consuming"),
            description: qsTr("Pass the event onward")
        },
        {
            key: "autoConsuming",
            label: qsTr("Auto-consuming"),
            description: qsTr("Consume only when handled")
        },
        {
            key: "transparent",
            label: qsTr("Transparent"),
            description: qsTr("Continue matching lower-priority binds")
        },
        {
            key: "ignoreMods",
            label: qsTr("Ignore modifiers"),
            description: qsTr("Allow extra modifiers")
        },
        {
            key: "dontInhibit",
            label: qsTr("Ignore inhibitor"),
            description: qsTr("Works through shortcut inhibition")
        },
        {
            key: "longPress",
            label: qsTr("Long press"),
            description: qsTr("Trigger after a hold")
        },
        {
            key: "submapUniversal",
            label: qsTr("Universal"),
            description: qsTr("Available in every submap")
        },
        {
            key: "click",
            label: qsTr("Click"),
            description: qsTr("Mouse click binding")
        },
        {
            key: "drag",
            label: qsTr("Drag"),
            description: qsTr("Mouse drag binding")
        },
        {
            key: "allowInputCapture",
            label: qsTr("Input capture"),
            description: qsTr("Allow during input capture")
        }
    ]

    onBindingChanged: {
        root.argumentsIssue = "";
        root.deviceIssue = "";
    }

    padding: 18

    background: Rectangle {
        color: root.palette.base
        radius: 16
        border.width: 1
        border.color: root.issue.length > 0 || root.argumentsIssue.length > 0 || root.deviceIssue.length > 0 ? "#8bfb7185" : root.palette.mid
    }

    function clone(value) {
        try {
            return JSON.parse(JSON.stringify(value));
        } catch (error) {
            return null;
        }
    }

    function listValue(value) {
        if (Array.isArray(value))
            return value.slice();
        if (!value || typeof value.length !== "number")
            return [];
        const result = [];
        for (let index = 0; index < value.length; ++index)
            result.push(value[index]);
        return result;
    }

    function humanize(value) {
        const text = String(value || "").replace(/[-_.:]+/g, " ").replace(/\s+/g, " ").trim();
        return text.length > 0 ? text.charAt(0).toUpperCase() + text.slice(1) : "";
    }

    function actionTypeOf(action) {
        if (!action || typeof action !== "object")
            return "dispatcher";
        if (typeof action.actionType === "string")
            return action.actionType;
        if (action.kind === "defaultApp" || action.kind === "hyprshelld")
            return action.kind;
        return "dispatcher";
    }

    function actionIdOf(action) {
        return action && typeof action.id === "string" ? action.id : "";
    }

    function actionLabelOf(action) {
        return action && typeof action.label === "string" ? action.label : root.humanize(root.actionIdOf(action));
    }

    function actionsForType(type) {
        return root.listValue(root.actions).filter(action => root.actionTypeOf(action) === type);
    }

    function actionTypeIndex() {
        const values = ["dispatcher", "defaultApp", "hyprshelld"];
        const current = root.binding && typeof root.binding.actionType === "string" ? root.binding.actionType : "dispatcher";
        const index = values.indexOf(current);
        return index >= 0 ? index : 0;
    }

    function actionIndex() {
        const type = root.binding ? root.binding.actionType : "dispatcher";
        const choices = root.actionsForType(type);
        const current = root.binding ? root.binding.action : "";
        const index = choices.findIndex(action => root.actionIdOf(action) === current);
        return index >= 0 ? index : 0;
    }

    function submapNames() {
        return [qsTr("Global")].concat(root.listValue(root.submaps).filter(record => record && record.enabled !== false && typeof record.name === "string").map(record => record.name));
    }

    function submapIndex() {
        const current = root.binding && typeof root.binding.submap === "string" ? root.binding.submap : "";
        const names = root.submapNames();
        const index = current.length === 0 ? 0 : names.indexOf(current);
        return index >= 0 ? index : 0;
    }

    function hasModifier(modifier) {
        return root.binding && Array.isArray(root.binding.modifiers) && root.binding.modifiers.includes(modifier);
    }

    function optionValue(key) {
        return root.binding && root.binding.options && root.binding.options[key] === true;
    }

    function deviceListText() {
        const device = root.binding && root.binding.options ? root.binding.options.device : null;
        return device && Array.isArray(device.list) ? device.list.join(", ") : "";
    }

    function deviceInclusive() {
        const device = root.binding && root.binding.options ? root.binding.options.device : null;
        return !device || device.inclusive !== false;
    }

    function commitDeviceList(text) {
        const list = String(text).split(",").map(value => value.trim()).filter(value => value.length > 0);
        const unique = [];
        for (const value of list) {
            if (!unique.includes(value))
                unique.push(value);
        }
        if (unique.length > 64 || unique.some(value => value.length > 512)) {
            root.deviceIssue = qsTr("Use at most 64 unique device names.");
            return;
        }
        root.deviceIssue = "";
        root.modify(function (candidate) {
            if (unique.length === 0) {
                delete candidate.options.device;
            } else {
                candidate.options.device = {
                    inclusive: root.deviceInclusive(),
                    list: unique
                };
            }
        });
    }

    function modify(mutator) {
        if (!root.controlsEnabled || !root.binding)
            return;
        const candidate = root.clone(root.binding);
        if (!candidate)
            return;
        mutator(candidate);
        root.recordModified(candidate);
    }

    function setModifier(modifier, enabled) {
        root.modify(function (candidate) {
            const selected = new Set(candidate.modifiers || []);
            if (enabled)
                selected.add(modifier);
            else
                selected.delete(modifier);
            candidate.modifiers = root.modifierOrder.filter(value => selected.has(value));
        });
    }

    function setActionType(type) {
        root.modify(function (candidate) {
            const choices = root.actionsForType(type);
            candidate.actionType = type;
            candidate.action = choices.length > 0 ? root.actionIdOf(choices[0]) : "";
            candidate.arguments = {};
        });
        root.argumentsIssue = "";
    }

    function setAction(action) {
        root.modify(function (candidate) {
            candidate.action = root.actionIdOf(action);
            candidate.arguments = {};
        });
        root.argumentsIssue = "";
    }

    function setOption(key, enabled) {
        root.modify(function (candidate) {
            if (!candidate.options)
                candidate.options = {};
            candidate.options[key] = enabled;
            if (key === "repeating" && enabled) {
                candidate.options.release = false;
                candidate.options.click = false;
                candidate.options.drag = false;
                candidate.options.longPress = false;
            }
            if (key === "click" && enabled) {
                candidate.options.drag = false;
                candidate.options.release = true;
                candidate.options.repeating = false;
            }
            if (key === "drag" && enabled) {
                candidate.options.click = false;
                candidate.options.release = true;
                candidate.options.repeating = false;
            }
            if ((key === "release" && enabled) || (key === "longPress" && enabled)) {
                candidate.options.repeating = false;
            }
        });
    }

    function commitArguments(text) {
        let parsed = null;
        try {
            parsed = JSON.parse(String(text));
        } catch (error) {
            root.argumentsIssue = qsTr("Arguments must be a valid JSON object.");
            return;
        }
        if (!parsed || typeof parsed !== "object" || Array.isArray(parsed) || Object.keys(parsed).length > 16) {
            root.argumentsIssue = qsTr("Arguments must be an object with at most 16 fields.");
            return;
        }
        root.argumentsIssue = "";
        root.modify(function (candidate) {
            candidate.arguments = parsed;
        });
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: root.bindingOrigin === "default" ? qsTr("Edit shipped default") : root.bindingOrigin === "override" ? qsTr("Edit user override") : root.bindingOrigin === "disabled" ? qsTr("Default shortcut disabled") : qsTr("Edit custom shortcut")
                    color: root.palette.text
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: root.binding ? String(root.binding.id) : ""
                    color: root.palette.placeholderText
                    font.family: "monospace"
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: root.bindingOrigin === "default" ? qsTr("Changes create a user override; the shipped baseline remains intact.") : root.bindingOrigin === "override" ? qsTr("Saved in the persistent user layer after the shipped defaults.") : root.bindingOrigin === "disabled" ? qsTr("The persistent user layer suppresses this default until Reset is used.") : qsTr("Saved as a custom shortcut in the persistent user layer.")
                    color: root.bindingOrigin === "default" ? root.palette.placeholderText : root.palette.highlight
                    font.pixelSize: 10
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            Switch {
                checked: root.binding && root.binding.enabled === true
                enabled: root.controlsEnabled
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                Accessible.name: qsTr("Shortcut enabled")
                onClicked: root.modify(function (candidate) {
                    candidate.enabled = checked;
                })
            }

            Button {
                text: qsTr("Done")
                enabled: root.controlsEnabled && root.issue.length === 0 && root.argumentsIssue.length === 0 && root.deviceIssue.length === 0
                onClicked: root.closeRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: previewRow.implicitHeight + 24
            radius: 14
            color: Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.09)
            border.width: 1
            border.color: Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.30)

            RowLayout {
                id: previewRow
                anchors {
                    fill: parent
                    margins: 12
                }
                spacing: 8

                Flow {
                    Layout.fillWidth: true
                    spacing: 5

                    Repeater {
                        model: root.binding ? root.listValue(root.binding.modifiers).concat([root.binding.key || qsTr("Key")]) : [qsTr("Key")]

                        delegate: Rectangle {
                            id: keycap
                            required property string modelData
                            width: keyLabel.implicitWidth + 18
                            height: 34
                            radius: 8
                            color: root.palette.button
                            border.width: 1
                            border.color: root.palette.mid

                            Label {
                                id: keyLabel
                                anchors.centerIn: parent
                                text: root.humanize(keycap.modelData)
                                color: root.palette.buttonText
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }

                Label {
                    text: "→"
                    color: root.palette.highlight
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    Accessible.ignored: true
                }

                Rectangle {
                    Layout.preferredWidth: Math.min(240, Math.max(100, actionPreview.implicitWidth + 20))
                    Layout.preferredHeight: 36
                    radius: 10
                    color: root.palette.highlight

                    Label {
                        id: actionPreview
                        anchors.centerIn: parent
                        width: parent.width - 18
                        text: root.binding ? root.humanize(root.binding.action) : qsTr("Action")
                        color: root.palette.highlightedText
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }
            }
        }

        GroupBox {
            Layout.fillWidth: true
            title: qsTr("Key chord")

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Flow {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: root.modifierOrder

                        delegate: CheckBox {
                            id: modifierCheck
                            required property string modelData
                            text: root.humanize(modelData)
                            checked: root.hasModifier(modelData)
                            enabled: root.controlsEnabled
                            onClicked: root.setModifier(modelData, checked)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Label {
                        text: qsTr("Key")
                        color: root.palette.text
                        font.weight: Font.Medium
                    }

                    TextField {
                        Layout.fillWidth: true
                        implicitHeight: root.minimumTargetSize
                        text: root.binding ? root.binding.key || "" : ""
                        enabled: root.controlsEnabled
                        selectByMouse: true
                        placeholderText: qsTr("k, comma, code:38, mouse:272…")
                        Accessible.name: qsTr("Shortcut key")
                        onEditingFinished: root.modify(function (candidate) {
                            candidate.key = text.trim();
                        })
                    }

                    ComboBox {
                        Layout.preferredWidth: 180
                        implicitHeight: root.minimumTargetSize
                        model: root.submapNames()
                        currentIndex: root.submapIndex()
                        enabled: root.controlsEnabled
                        Accessible.name: qsTr("Shortcut submap")
                        onActivated: index => root.modify(function (candidate) {
                                const names = root.submapNames();
                                candidate.submap = index <= 0 ? "" : names[index];
                            })
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.deviceIssue.length > 0
                    text: root.deviceIssue
                    color: "#ffb8c3"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }
            }
        }

        GroupBox {
            Layout.fillWidth: true
            title: qsTr("Action")

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ComboBox {
                        Layout.preferredWidth: 170
                        implicitHeight: root.minimumTargetSize
                        model: [qsTr("Hyprland action"), qsTr("Default application"), qsTr("HyprShelld action")]
                        currentIndex: root.actionTypeIndex()
                        enabled: root.controlsEnabled
                        Accessible.name: qsTr("Shortcut action type")
                        onActivated: index => root.setActionType(["dispatcher", "defaultApp", "hyprshelld"][index])
                    }

                    ComboBox {
                        id: actionControl
                        Layout.fillWidth: true
                        implicitHeight: root.minimumTargetSize
                        readonly property var choices: root.actionsForType(root.binding ? root.binding.actionType : "dispatcher")
                        model: choices.map(action => root.actionLabelOf(action))
                        currentIndex: root.actionIndex()
                        enabled: root.controlsEnabled && choices.length > 0
                        Accessible.name: qsTr("Shortcut action")
                        onActivated: index => {
                            if (index >= 0 && index < choices.length)
                                root.setAction(choices[index]);
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: {
                        const choices = actionControl.choices;
                        const index = root.actionIndex();
                        return index >= 0 && index < choices.length && choices[index].description ? choices[index].description : "";
                    }
                    visible: text.length > 0
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Arguments use the reviewed action schema. Empty actions use {}. Fields are validated again before the desired state is saved.")
                    color: root.palette.placeholderText
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }

                TextArea {
                    id: argumentsField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 96
                    text: root.binding ? JSON.stringify(root.binding.arguments || {}, null, 2) : "{}"
                    enabled: root.controlsEnabled
                    selectByMouse: true
                    wrapMode: TextEdit.NoWrap
                    font.family: "monospace"
                    Accessible.name: qsTr("Shortcut action arguments")
                    background: Rectangle {
                        radius: 10
                        color: root.palette.base
                        border.width: 1
                        border.color: root.argumentsIssue.length > 0 ? "#fb7185" : root.palette.mid
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        visible: root.argumentsIssue.length > 0
                        text: root.argumentsIssue
                        color: "#ffb8c3"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }

                    Button {
                        text: qsTr("Apply Arguments")
                        enabled: root.controlsEnabled
                        onClicked: root.commitArguments(argumentsField.text)
                    }
                }
            }
        }

        GroupBox {
            Layout.fillWidth: true
            title: qsTr("Binding behavior")

            GridLayout {
                anchors.fill: parent
                columns: root.width >= 720 ? 3 : root.width >= 470 ? 2 : 1
                columnSpacing: 10
                rowSpacing: 6

                Repeater {
                    model: root.optionDefinitions

                    delegate: CheckBox {
                        id: optionCheck
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.label
                        checked: root.optionValue(modelData.key)
                        enabled: root.controlsEnabled
                        Accessible.name: modelData.label
                        Accessible.description: modelData.description
                        ToolTip.visible: hovered
                        ToolTip.text: modelData.description
                        onClicked: root.setOption(modelData.key, checked)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Label {
                    text: qsTr("Description")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                TextField {
                    Layout.fillWidth: true
                    implicitHeight: root.minimumTargetSize
                    text: root.binding ? root.binding.description || "" : ""
                    enabled: root.controlsEnabled
                    selectByMouse: true
                    placeholderText: qsTr("What this shortcut does")
                    Accessible.name: qsTr("Shortcut description")
                    onEditingFinished: root.modify(function (candidate) {
                        candidate.description = text;
                    })
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 300
                spacing: 3

                Label {
                    text: qsTr("Input devices (optional)")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    ComboBox {
                        Layout.preferredWidth: 112
                        implicitHeight: root.minimumTargetSize
                        model: [qsTr("Only"), qsTr("Except")]
                        currentIndex: root.deviceInclusive() ? 0 : 1
                        enabled: root.controlsEnabled && root.deviceListText().length > 0
                        Accessible.name: qsTr("Device filter mode")
                        onActivated: index => root.modify(function (candidate) {
                                if (candidate.options.device)
                                    candidate.options.device.inclusive = index === 0;
                            })
                    }

                    TextField {
                        Layout.fillWidth: true
                        implicitHeight: root.minimumTargetSize
                        text: root.deviceListText()
                        enabled: root.controlsEnabled
                        selectByMouse: true
                        placeholderText: qsTr("All devices · comma separated")
                        Accessible.name: qsTr("Shortcut input devices")
                        onEditingFinished: root.commitDeviceList(text)
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.issue.length > 0
            text: root.issue
            color: "#ffb8c3"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: root.bindingOrigin === "custom" ? qsTr("Remove Shortcut") : root.bindingOrigin === "disabled" ? qsTr("Default Disabled") : qsTr("Disable Default")
                enabled: root.controlsEnabled && root.binding && root.bindingOrigin !== "disabled"
                onClicked: {
                    if (root.binding)
                        root.removeRequested(root.binding.id);
                }
            }

            Button {
                objectName: "resetBindingButton"
                visible: root.canReset
                text: qsTr("Reset to Default")
                enabled: root.controlsEnabled && root.binding
                onClicked: {
                    if (root.binding) {
                        root.argumentsIssue = "";
                        root.deviceIssue = "";
                        root.resetRequested(root.binding.id);
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: root.bindingOrigin === "default" ? qsTr("Managed default layer") : qsTr("Persistent user layer")
                color: root.palette.placeholderText
                font.pixelSize: 11
            }
        }
    }
}
