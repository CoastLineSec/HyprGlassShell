pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    property var rule: null
    property var overrideDefinitions: []
    property var invalidOverrideKeys: []
    property bool controlsEnabled: false
    property string ruleIssue: ""
    property real minimumTargetSize: 44
    property bool compact: width < 560
    property string loadedRuleId: ""
    property int selectorType: 0
    property int monitorType: 0

    signal closeRequested()
    signal removeRequested(string id)
    signal propertyModified(string id, string propertyName, var value)
    signal overrideModified(
        string id, string key, bool included, var value
    )

    objectName: "workspaceRuleEditorScrollView"
    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    function selectorKind(selector) {
        if (typeof selector !== "string")
            return 0;
        if (selector.startsWith("name:"))
            return 1;
        if (selector === "special")
            return 2;
        if (selector.startsWith("special:"))
            return 3;
        return 0;
    }

    function selectorPayload(selector) {
        if (typeof selector !== "string")
            return "";
        if (selector.startsWith("name:"))
            return selector.slice(5);
        if (selector.startsWith("special:"))
            return selector.slice(8);
        return selector === "special" ? "" : selector;
    }

    function selectorFromParts(kind, payload) {
        if (kind === 1)
            return "name:" + payload;
        if (kind === 2)
            return "special";
        if (kind === 3)
            return "special:" + payload;
        return payload;
    }

    function monitorKind(monitor) {
        if (typeof monitor === "string" && monitor.startsWith("desc:"))
            return 2;
        return typeof monitor === "string" && monitor.length > 0 ? 1 : 0;
    }

    function monitorPayload(monitor) {
        if (typeof monitor !== "string")
            return "";
        return monitor.startsWith("desc:") ? monitor.slice(5) : monitor;
    }

    function monitorFromParts(kind, payload) {
        if (kind === 2)
            return "desc:" + payload;
        return kind === 1 ? payload : "";
    }

    function ruleId() {
        return root.rule && typeof root.rule.id === "string"
            ? root.rule.id : "";
    }

    function overrideIncluded(key) {
        return !!root.rule && !!root.rule.overrides
            && typeof root.rule.overrides === "object"
            && !Array.isArray(root.rule.overrides)
            && Object.prototype.hasOwnProperty.call(
                root.rule.overrides, key
            );
    }

    function overrideValue(key) {
        return root.overrideIncluded(key) ? root.rule.overrides[key] : undefined;
    }

    function definitionsForGroup(group) {
        return Array.isArray(root.overrideDefinitions)
            ? root.overrideDefinitions.filter(item => item.group === group)
            : [];
    }

    function synchronizeEditorKinds() {
        const id = root.ruleId();
        if (id === root.loadedRuleId)
            return;
        root.loadedRuleId = id;
        root.selectorType = root.selectorKind(
            root.rule ? root.rule.selector : ""
        );
        root.monitorType = root.monitorKind(
            root.rule ? root.rule.monitor : ""
        );
    }

    onRuleChanged: root.synchronizeEditorKinds()
    Component.onCompleted: root.synchronizeEditorKinds()

    ColumnLayout {
        objectName: "workspaceRuleEditorContent"
        width: root.availableWidth
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                objectName: "closeWorkspaceRuleEditorButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Back to rules")
                Accessible.name: qsTr("Close the selected Workspace Rule editor")

                onClicked: root.closeRequested()
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                horizontalAlignment: Text.AlignRight
                text: qsTr("Editing Workspace Rule")
                color: root.palette.placeholderText
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
        }

        Frame {
            objectName: "workspaceRuleTargetCard"
            Layout.fillWidth: true
            padding: root.compact ? 14 : 18

            background: Rectangle {
                color: root.palette.base
                radius: 16
                border.color: root.palette.mid
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 14

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Workspace target")
                    color: root.palette.text
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Choose one exact numeric, named, or special workspace. The stable internal rule ID is generated and preserved automatically.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                SettingsSelectRow {
                    Layout.fillWidth: true
                    title: qsTr("Workspace kind")
                    description: qsTr("Choose how this rule identifies its workspace.")
                    model: [
                        qsTr("Numeric"), qsTr("Named"),
                        qsTr("Any special workspace"),
                        qsTr("Named special workspace")
                    ]
                    currentIndex: root.selectorType
                    controlWidth: 220
                    enabled: root.controlsEnabled
                    controlObjectName: "workspaceRuleSelectorType"
                    accessibleName: qsTr("Workspace Rule selector type")
                    minimumTargetSize: root.minimumTargetSize

                    onValueModified: index => {
                        if (index < 0 || index > 3)
                            return;
                        root.selectorType = index;
                        root.propertyModified(
                            root.ruleId(), "selector",
                            root.selectorFromParts(index, "")
                        );
                    }
                }

                TextField {
                    objectName: "workspaceRuleSelector"
                    Layout.fillWidth: true
                    implicitHeight: root.minimumTargetSize
                    visible: root.selectorType !== 2
                    enabled: root.controlsEnabled
                    maximumLength: root.selectorType === 0 ? 10 : 128
                    text: root.selectorPayload(
                        root.rule ? root.rule.selector : ""
                    )
                    placeholderText: root.selectorType === 0
                        ? qsTr("Positive workspace number")
                        : root.selectorType === 1
                            ? qsTr("Workspace name")
                            : qsTr("Special workspace name")
                    inputMethodHints: root.selectorType === 0
                        ? Qt.ImhDigitsOnly : Qt.ImhNone
                    Accessible.name: root.selectorType === 0
                        ? qsTr("Workspace number")
                        : root.selectorType === 1
                            ? qsTr("Workspace name")
                            : qsTr("Special workspace name")

                    onTextEdited: root.propertyModified(
                        root.ruleId(), "selector",
                        root.selectorFromParts(root.selectorType, text)
                    )
                }

                SettingsToggleRow {
                    Layout.fillWidth: true
                    title: qsTr("Rule enabled")
                    description: qsTr("A disabled rule stays saved and ordered but is not emitted into the active compositor configuration.")
                    checked: !!root.rule && root.rule.enabled === true
                    enabled: root.controlsEnabled
                    controlObjectName: "workspaceRuleEnabledEditor"
                    accessibleName: qsTr("Enable this Workspace Rule")
                    minimumTargetSize: root.minimumTargetSize

                    onValueModified: value => root.propertyModified(
                        root.ruleId(), "enabled", value
                    )
                }

                Label {
                    objectName: "workspaceRuleEditorValidationMessage"
                    Layout.fillWidth: true
                    visible: root.ruleIssue.length > 0
                    text: root.ruleIssue
                    color: "#ffb8c3"
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }
            }
        }

        Frame {
            objectName: "workspaceRuleBehaviorCard"
            Layout.fillWidth: true
            padding: root.compact ? 14 : 18

            background: Rectangle {
                color: root.palette.base
                radius: 16
                border.color: root.palette.mid
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 14

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Assignment and behavior")
                    color: root.palette.text
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    textFormat: Text.PlainText
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                SettingsSelectRow {
                    Layout.fillWidth: true
                    title: qsTr("Output assignment")
                    description: qsTr("Leave unassigned or bind the workspace to one stable output name or description.")
                    model: [
                        qsTr("Any output"), qsTr("Output name"),
                        qsTr("Output description")
                    ]
                    currentIndex: root.monitorType
                    controlWidth: 190
                    enabled: root.controlsEnabled
                    controlObjectName: "workspaceRuleMonitorType"
                    accessibleName: qsTr("Workspace Rule output selector type")
                    minimumTargetSize: root.minimumTargetSize

                    onValueModified: index => {
                        if (index < 0 || index > 2)
                            return;
                        root.monitorType = index;
                        root.propertyModified(
                            root.ruleId(), "monitor",
                            root.monitorFromParts(index, "")
                        );
                    }
                }

                TextField {
                    objectName: "workspaceRuleMonitor"
                    Layout.fillWidth: true
                    implicitHeight: root.minimumTargetSize
                    visible: root.monitorType !== 0
                    enabled: root.controlsEnabled
                    maximumLength: root.monitorType === 2 ? 256 : 128
                    text: root.monitorPayload(
                        root.rule ? root.rule.monitor : ""
                    )
                    placeholderText: root.monitorType === 2
                        ? qsTr("Exact output description")
                        : qsTr("Output name, such as DP-1")
                    Accessible.name: root.monitorType === 2
                        ? qsTr("Workspace Rule output description")
                        : qsTr("Workspace Rule output name")

                    onTextEdited: root.propertyModified(
                        root.ruleId(), "monitor",
                        root.monitorFromParts(root.monitorType, text)
                    )
                }

                SettingsToggleRow {
                    Layout.fillWidth: true
                    title: qsTr("Persistent workspace")
                    description: qsTr("Keep this workspace present even when it has no windows.")
                    checked: !!root.rule && root.rule.persistent === true
                    enabled: root.controlsEnabled
                    controlObjectName: "workspaceRulePersistent"
                    accessibleName: qsTr("Keep this workspace persistent")
                    minimumTargetSize: root.minimumTargetSize

                    onValueModified: value => root.propertyModified(
                        root.ruleId(), "persistent", value
                    )
                }

                SettingsToggleRow {
                    Layout.fillWidth: true
                    title: qsTr("Default workspace for the assigned output")
                    description: qsTr("Select this workspace by default on its assigned output. An output assignment is required for this to have an effect.")
                    checked: !!root.rule && root.rule.isDefault === true
                    enabled: root.controlsEnabled
                    controlObjectName: "workspaceRuleDefault"
                    accessibleName: qsTr("Make this the default workspace for its output")
                    minimumTargetSize: root.minimumTargetSize

                    onValueModified: value => root.propertyModified(
                        root.ruleId(), "isDefault", value
                    )
                }

                SettingsSelectRow {
                    Layout.fillWidth: true
                    title: qsTr("Workspace layout")
                    description: qsTr("Keep the global layout or select one built-in layout for this workspace.")
                    model: [
                        qsTr("Use global layout"), qsTr("Dwindle"),
                        qsTr("Master"), qsTr("Scrolling"),
                        qsTr("Monocle")
                    ]
                    currentIndex: Math.max(
                        0, ["", "dwindle", "master", "scrolling", "monocle"]
                            .indexOf(root.rule ? root.rule.layout : "")
                    )
                    controlWidth: 190
                    enabled: root.controlsEnabled
                    controlObjectName: "workspaceRuleLayout"
                    accessibleName: qsTr("Workspace-specific layout")
                    minimumTargetSize: root.minimumTargetSize

                    onValueModified: index => {
                        const values = [
                            "", "dwindle", "master", "scrolling", "monocle"
                        ];
                        if (index >= 0 && index < values.length) {
                            root.propertyModified(
                                root.ruleId(), "layout", values[index]
                            );
                        }
                    }
                }
            }
        }

        Repeater {
            model: [
                {
                    key: "spacing",
                    title: qsTr("Workspace spacing"),
                    description: qsTr("Override tiled inner gaps, monitor-edge gaps, floating-window gaps, or border size for this workspace.")
                },
                {
                    key: "appearance",
                    title: qsTr("Workspace appearance"),
                    description: qsTr("Override border, rounding, decoration, and shadow behavior for windows on this workspace.")
                },
                {
                    key: "identity",
                    title: qsTr("Name, motion, and layout details"),
                    description: qsTr("Set a default name, workspace transition style, or engine-specific layout choices.")
                }
            ]

            Frame {
                id: overrideCard

                required property var modelData

                objectName: "workspaceRule"
                    + modelData.key.charAt(0).toUpperCase()
                    + modelData.key.slice(1) + "OverridesCard"
                Layout.fillWidth: true
                padding: root.compact ? 14 : 18

                background: Rectangle {
                    color: root.palette.base
                    radius: 16
                    border.color: root.palette.mid
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: overrideCard.modelData.title
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
                        text: overrideCard.modelData.description
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }

                    Repeater {
                        model: root.definitionsForGroup(
                            overrideCard.modelData.key
                        )

                        RuleOptionalField {
                            required property var modelData

                            definition: modelData
                            included: root.overrideIncluded(modelData.key)
                            value: root.overrideValue(modelData.key)
                            fieldValid: !root.invalidOverrideKeys.includes(
                                modelData.key
                            )
                            enabled: root.controlsEnabled
                            minimumTargetSize: root.minimumTargetSize

                            onIncludeModified: included =>
                                root.overrideModified(
                                    root.ruleId(),
                                    modelData.key,
                                    included,
                                    included ? modelData.defaultValue : undefined
                                )
                            onValueModified: value =>
                                root.overrideModified(
                                    root.ruleId(),
                                    modelData.key,
                                    true,
                                    value
                                )
                        }
                    }
                }
            }
        }

        Frame {
            objectName: "workspaceRuleEditorActionsCard"
            Layout.fillWidth: true
            padding: root.compact ? 14 : 18

            background: Rectangle {
                color: root.palette.base
                radius: 16
                border.color: root.ruleIssue.length === 0
                    ? root.palette.highlight : root.palette.mid
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Behavior and rules share one Workspaces draft")
                    color: root.palette.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Return to the rule list to save all 16 behavior values and every ordered user Workspace Rule atomically. HyprShelld preserves its internal maximized-window rule separately.")
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
                        objectName: "doneEditingWorkspaceRuleButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Done")
                        Accessible.name: qsTr("Return to the Workspace Rule list")

                        onClicked: root.closeRequested()
                    }

                    Button {
                        objectName: "removeEditedWorkspaceRuleButton"
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Remove from draft")
                        enabled: root.controlsEnabled
                        Accessible.name: qsTr("Remove this Workspace Rule from the draft")

                        onClicked: root.removeRequested(root.ruleId())
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }
}
