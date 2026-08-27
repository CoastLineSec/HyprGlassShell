pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

ListView {
    id: root

    property var rules: []
    property bool controlsEnabled: false
    property bool discardEnabled: false
    property bool draftDirty: false
    property bool draftValid: true
    property bool saveEnabled: false
    property bool resetEnabled: false
    property bool busy: false
    property string busyOperation: ""
    property real minimumTargetSize: 44

    signal addRequested()
    signal editRequested(string id)
    signal enabledRequested(string id, bool enabled)
    signal moveRequested(string id, int offset)
    signal removeRequested(string id)
    signal discardRequested()
    signal resetRequested()
    signal saveRequested()

    model: Array.isArray(root.rules) ? root.rules : []
    clip: true
    spacing: 10
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    function ruleTitle(rule) {
        if (!rule || typeof rule.selector !== "string"
                || rule.selector.length === 0) {
            return qsTr("Choose a workspace");
        }
        if (rule.selector === "special")
            return qsTr("Special workspace");
        if (rule.selector.startsWith("special:"))
            return qsTr("Special workspace %1").arg(rule.selector.slice(8));
        if (rule.selector.startsWith("name:"))
            return qsTr("Named workspace %1").arg(rule.selector.slice(5));
        return qsTr("Workspace %1").arg(rule.selector);
    }

    function ruleSummary(rule) {
        if (!rule)
            return "";
        const parts = [];
        if (typeof rule.monitor === "string" && rule.monitor.length > 0)
            parts.push(qsTr("assigned output"));
        if (rule.persistent === true)
            parts.push(qsTr("persistent"));
        if (rule.isDefault === true)
            parts.push(qsTr("default"));
        if (typeof rule.layout === "string" && rule.layout.length > 0)
            parts.push(qsTr("%1 layout").arg(rule.layout));
        const overrideCount = rule.overrides
                && typeof rule.overrides === "object"
                && !Array.isArray(rule.overrides)
            ? Object.keys(rule.overrides).length : 0;
        parts.push(qsTr("%1 overrides").arg(overrideCount));
        return parts.join(qsTr(" · "));
    }

    header: ColumnLayout {
        width: ListView.view ? ListView.view.width : 0
        spacing: 8

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            visible: root.count === 0
            text: qsTr("No user Workspace Rules are saved. HyprShelld's internal maximized-window integration is protected separately and is never shown here.")
            color: root.palette.placeholderText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Button {
            objectName: "addWorkspaceRuleButton"
            Layout.fillWidth: true
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: qsTr("Add workspace rule")
            enabled: root.controlsEnabled && root.count < 1024
            Accessible.name: text

            onClicked: root.addRequested()
        }
    }

    delegate: Frame {
        id: ruleCard

        required property int index
        required property var modelData

        objectName: "workspaceRuleCard" + index
        width: ListView.view ? ListView.view.width : 0
        padding: 14

        background: Rectangle {
            color: root.palette.base
            radius: 14
            border.color: root.palette.mid
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: root.ruleTitle(ruleCard.modelData)
                        color: root.palette.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.ruleSummary(ruleCard.modelData)
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }
                }

                Switch {
                    objectName: "workspaceRuleEnabled" + ruleCard.index
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    checked: !!ruleCard.modelData
                        && ruleCard.modelData.enabled === true
                    enabled: root.controlsEnabled
                    Accessible.name: qsTr("Enable %1").arg(
                        root.ruleTitle(ruleCard.modelData)
                    )

                    onClicked: root.enabledRequested(
                        String(ruleCard.modelData.id), checked
                    )
                }
            }

            Flow {
                Layout.fillWidth: true
                Layout.preferredHeight: childrenRect.height
                spacing: 8

                Button {
                    objectName: "editWorkspaceRuleButton" + ruleCard.index
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Edit")
                    enabled: root.controlsEnabled
                    Accessible.name: qsTr("Edit %1").arg(
                        root.ruleTitle(ruleCard.modelData)
                    )

                    onClicked: root.editRequested(
                        String(ruleCard.modelData.id)
                    )
                }

                Button {
                    objectName: "moveWorkspaceRuleUpButton" + ruleCard.index
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Move up")
                    enabled: root.controlsEnabled && ruleCard.index > 0
                    Accessible.name: qsTr("Move %1 up").arg(
                        root.ruleTitle(ruleCard.modelData)
                    )

                    onClicked: root.moveRequested(
                        String(ruleCard.modelData.id), -1
                    )
                }

                Button {
                    objectName: "moveWorkspaceRuleDownButton" + ruleCard.index
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Move down")
                    enabled: root.controlsEnabled
                        && ruleCard.index + 1 < root.count
                    Accessible.name: qsTr("Move %1 down").arg(
                        root.ruleTitle(ruleCard.modelData)
                    )

                    onClicked: root.moveRequested(
                        String(ruleCard.modelData.id), 1
                    )
                }

                Button {
                    objectName: "removeWorkspaceRuleButton" + ruleCard.index
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Remove")
                    enabled: root.controlsEnabled
                    Accessible.name: qsTr("Remove %1 from the draft").arg(
                        root.ruleTitle(ruleCard.modelData)
                    )

                    onClicked: root.removeRequested(
                        String(ruleCard.modelData.id)
                    )
                }
            }
        }
    }

    footer: ColumnLayout {
        width: ListView.view ? ListView.view.width : 0
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: qsTr("Removing or reordering a user rule changes only this Workspaces draft until Save & apply succeeds.")
            color: root.palette.placeholderText
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Label {
            Layout.fillWidth: true
            visible: root.draftDirty && !root.draftValid
            text: qsTr("Every rule needs one unique valid workspace selector and complete values before the combined draft can be saved.")
            color: ShellTheme.onErrorContainer
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Flow {
            Layout.fillWidth: true
            Layout.preferredHeight: childrenRect.height
            spacing: 10

            Button {
                objectName: "discardWorkspacesDraftFromRulesButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                visible: root.draftDirty
                text: qsTr("Discard draft")
                enabled: root.discardEnabled
                Accessible.name: qsTr("Discard the complete Workspaces behavior and rules draft")

                onClicked: root.discardRequested()
            }

            Button {
                objectName: "resetWorkspacesDefaultsFromRulesButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Reset to defaults")
                enabled: root.controlsEnabled && root.resetEnabled
                Accessible.name: qsTr("Reset Workspaces behavior and user rules to defaults")

                onClicked: root.resetRequested()
            }

            Button {
                objectName: "saveWorkspacesFromRulesButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: root.busyOperation === "workspaces-save"
                        || root.busyOperation === "workspaces-apply"
                    ? qsTr("Saving…") : qsTr("Save & apply")
                enabled: root.saveEnabled
                Accessible.name: qsTr("Save and apply the complete Workspaces behavior and rules draft")

                onClicked: root.saveRequested()
            }
        }

        Item { Layout.preferredHeight: 12 }
    }
}
