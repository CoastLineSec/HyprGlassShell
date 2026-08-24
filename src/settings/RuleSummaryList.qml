pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ListView {
    id: root

    property string ruleKind: "window"
    property var rules: []
    property bool controlsEnabled: false
    property bool discardEnabled: controlsEnabled
    property bool draftDirty: false
    property bool draftValid: true
    property bool saveEnabled: false
    property bool resetEnabled: false
    property bool busy: false
    property string busyOperation: ""
    property string emptyText: ""
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

    header: ColumnLayout {
        width: ListView.view ? ListView.view.width : 0
        spacing: 8

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            visible: root.count === 0
            text: root.emptyText
            color: root.palette.placeholderText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Button {
            objectName: root.ruleKind === "window"
                ? "addWindowRuleButton" : "addLayerRuleButton"
            Layout.fillWidth: true
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: root.ruleKind === "window"
                ? qsTr("Add window rule") : qsTr("Add layer rule")
            enabled: root.controlsEnabled && root.count < 4096
            Accessible.name: text

            onClicked: root.addRequested()
        }
    }

    delegate: Frame {
        id: ruleCard

        required property int index
        required property var modelData

        objectName: (root.ruleKind === "window"
            ? "windowRuleCard" : "layerRuleCard") + index
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
                        text: ruleCard.modelData
                            && typeof ruleCard.modelData.name === "string"
                            && ruleCard.modelData.name.length > 0
                            ? ruleCard.modelData.name : qsTr("Unnamed rule")
                        color: root.palette.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("%1 matchers · %2 effects")
                            .arg(ruleCard.modelData
                                && ruleCard.modelData.match
                                ? Object.keys(ruleCard.modelData.match).length
                                : 0)
                            .arg(ruleCard.modelData
                                && ruleCard.modelData.effects
                                ? Object.keys(ruleCard.modelData.effects).length
                                : 0)
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        textFormat: Text.PlainText
                    }
                }

                Switch {
                    objectName: (root.ruleKind === "window"
                        ? "windowRuleEnabled" : "layerRuleEnabled")
                        + ruleCard.index
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    checked: ruleCard.modelData
                        && ruleCard.modelData.enabled === true
                    enabled: root.controlsEnabled
                    Accessible.name: qsTr("Enable %1").arg(
                        ruleCard.modelData && ruleCard.modelData.name
                            ? ruleCard.modelData.name : qsTr("rule")
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
                    objectName: (root.ruleKind === "window"
                        ? "editWindowRuleButton" : "editLayerRuleButton")
                        + ruleCard.index
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Edit")
                    enabled: root.controlsEnabled
                    Accessible.name: qsTr("Edit %1").arg(
                        ruleCard.modelData.name || qsTr("rule")
                    )

                    onClicked: root.editRequested(
                        String(ruleCard.modelData.id)
                    )
                }

                Button {
                    objectName: (root.ruleKind === "window"
                        ? "moveWindowRuleUpButton" : "moveLayerRuleUpButton")
                        + ruleCard.index
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Move up")
                    enabled: root.controlsEnabled && ruleCard.index > 0
                    Accessible.name: qsTr("Move %1 up").arg(
                        ruleCard.modelData.name || qsTr("rule")
                    )

                    onClicked: root.moveRequested(
                        String(ruleCard.modelData.id), -1
                    )
                }

                Button {
                    objectName: (root.ruleKind === "window"
                        ? "moveWindowRuleDownButton" : "moveLayerRuleDownButton")
                        + ruleCard.index
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Move down")
                    enabled: root.controlsEnabled
                        && ruleCard.index + 1 < root.count
                    Accessible.name: qsTr("Move %1 down").arg(
                        ruleCard.modelData.name || qsTr("rule")
                    )

                    onClicked: root.moveRequested(
                        String(ruleCard.modelData.id), 1
                    )
                }

                Button {
                    objectName: (root.ruleKind === "window"
                        ? "removeWindowRuleButton" : "removeLayerRuleButton")
                        + ruleCard.index
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: qsTr("Remove")
                    enabled: root.controlsEnabled
                    Accessible.name: qsTr("Remove %1 from the draft").arg(
                        ruleCard.modelData.name || qsTr("rule")
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
            text: qsTr("Removing or reordering a rule changes only this local draft until Save & apply succeeds.")
            color: root.palette.placeholderText
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Label {
            Layout.fillWidth: true
            visible: root.draftDirty && !root.draftValid
            text: qsTr("Every rule needs a unique name, at least one complete matcher, and at least one complete effect before the combined draft can be saved.")
            color: "#ffb8c3"
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
                objectName: root.ruleKind === "window"
                    ? "discardRulesDraftFromWindowButton"
                    : "discardRulesDraftFromLayerButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                visible: root.draftDirty
                text: qsTr("Discard draft")
                enabled: root.discardEnabled
                Accessible.name: qsTr("Discard the complete Window and Layer Rules draft")

                onClicked: root.discardRequested()
            }

            Button {
                objectName: root.ruleKind === "window"
                    ? "resetRulesDefaultsFromWindowButton"
                    : "resetRulesDefaultsFromLayerButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Reset to defaults")
                enabled: root.controlsEnabled && root.resetEnabled
                Accessible.name: qsTr("Prepare an empty Window and Layer Rules draft")

                onClicked: root.resetRequested()
            }

            Button {
                objectName: root.ruleKind === "window"
                    ? "saveRulesFromWindowButton"
                    : "saveRulesFromLayerButton"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: root.busyOperation === "rules-save"
                        || root.busyOperation === "rules-apply"
                    ? qsTr("Saving…") : qsTr("Save & apply")
                enabled: root.saveEnabled
                Accessible.name: qsTr("Save and apply the complete Window and Layer Rules draft")

                onClicked: root.saveRequested()
            }
        }

        Item { Layout.preferredHeight: 12 }
    }
}
