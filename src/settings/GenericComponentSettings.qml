pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Frame {
    id: root

    property var component: null
    property var settings: ({})
    property bool controlsEnabled: true
    property bool saving: false
    property string errorText: ""
    property var draftSettings: ({})

    property var definitions: []
    readonly property var componentDefinitions: root.definitions.filter(
        definition => definition && definition.scope === "component"
    ).slice().sort(root.compareDefinitions)
    readonly property var instanceDefinitions: root.definitions.filter(
        definition => definition && definition.scope === "instance"
    ).slice().sort(root.compareDefinitions)
    readonly property bool dirty:
        JSON.stringify(root.draftSettings) !== JSON.stringify(root.settings)
    readonly property bool hasValidationError: {
        for (const row of settingsRows.children) {
            if (row && typeof row.validationError === "string"
                    && row.validationError.length > 0) {
                return true;
            }
        }
        return false;
    }

    signal settingsRequested(var settings)

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

    function clone(value) {
        if (!value || typeof value !== "object" || Array.isArray(value))
            return {};
        return JSON.parse(JSON.stringify(value));
    }

    function compareDefinitions(left, right) {
        const leftGroup = left && left.group ? String(left.group) : "";
        const rightGroup = right && right.group ? String(right.group) : "";
        const byGroup = leftGroup.localeCompare(rightGroup);
        if (byGroup !== 0)
            return byGroup;
        const leftOrder = left && typeof left.order === "number"
            ? left.order : 0;
        const rightOrder = right && typeof right.order === "number"
            ? right.order : 0;
        if (leftOrder !== rightOrder)
            return leftOrder - rightOrder;
        return String(left.key || "").localeCompare(String(right.key || ""));
    }

    function begin(component, settings) {
        root.component = component;
        root.definitions = root.listValue(
            component ? component.settingsDefinitions : null
        );
        const initial = {};
        for (const definition of root.componentDefinitions) {
            if (definition
                    && Object.prototype.hasOwnProperty.call(
                        definition,
                        "defaultValue"
                    )) {
                initial[definition.key] = definition.defaultValue;
            }
        }
        const provided = root.clone(settings);
        for (const key of Object.keys(provided))
            initial[key] = provided[key];
        root.settings = root.clone(initial);
        root.draftSettings = root.clone(initial);
        root.errorText = "";
    }

    function valueFor(key) {
        return Object.prototype.hasOwnProperty.call(root.draftSettings, key)
            ? root.draftSettings[key] : undefined;
    }

    function setValue(key, value) {
        const next = root.clone(root.draftSettings);
        next[key] = value;
        root.draftSettings = next;
    }

    function definitionVisible(definition) {
        if (!definition || !definition.visibleWhen)
            return true;
        const condition = definition.visibleWhen;
        return JSON.stringify(root.valueFor(condition.key))
            === JSON.stringify(condition.equals);
    }

    function resetDraft() {
        const next = {};
        for (const definition of root.componentDefinitions) {
            if (definition
                    && Object.prototype.hasOwnProperty.call(
                        definition,
                        "defaultValue"
                    )) {
                next[definition.key] = definition.defaultValue;
            }
        }
        root.draftSettings = root.clone(next);
    }

    objectName: "genericComponentSettings"
    padding: 0

    background: Rectangle {
        color: "transparent"
    }

    contentItem: ColumnLayout {
        spacing: 16

        Label {
            Layout.fillWidth: true
            visible: root.componentDefinitions.length === 0
            text: root.instanceDefinitions.length > 0
                ? qsTr("This component has no shared settings.")
                : qsTr("This component does not provide configurable settings.")
            color: root.palette.placeholderText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        ColumnLayout {
            id: settingsRows

            Layout.fillWidth: true
            spacing: 14

            Repeater {
                model: root.componentDefinitions

                SchemaSettingRow {
                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    visible: root.definitionVisible(modelData)
                    definition: modelData
                    settingValue: root.valueFor(modelData.key)
                    controlsEnabled: root.controlsEnabled && !root.saving
                    showGroupHeading: index === 0
                        || root.componentDefinitions[index - 1].group
                            !== modelData.group

                    onValueEdited: value => root.setValue(
                        modelData.key,
                        value
                    )
                }
            }
        }

        Frame {
            objectName: "componentInstanceSettingsNotice"
            Layout.fillWidth: true
            visible: root.instanceDefinitions.length > 0
            padding: 12

            background: Rectangle {
                color: Qt.rgba(
                    root.palette.highlight.r,
                    root.palette.highlight.g,
                    root.palette.highlight.b,
                    0.09
                )
                radius: 10
                border.color: root.palette.mid
            }

            Label {
                anchors.fill: parent
                text: qsTr("Settings that belong to an individual widget instance are changed where that widget is placed on the bar or desktop.")
                color: root.palette.placeholderText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.name: text
            }
        }

        Label {
            objectName: "genericComponentSettingsError"
            Layout.fillWidth: true
            visible: root.errorText.length > 0
            text: root.errorText
            color: ShellTheme.onErrorContainer
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                objectName: "resetGenericComponentSettings"
                text: qsTr("Reset")
                enabled: root.controlsEnabled && !root.saving
                    && root.componentDefinitions.length > 0
                onClicked: root.resetDraft()
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                objectName: "saveGenericComponentSettings"
                text: root.saving ? qsTr("Saving…") : qsTr("Save")
                enabled: root.controlsEnabled && !root.saving
                    && root.dirty && !root.hasValidationError
                    && root.componentDefinitions.length > 0
                highlighted: true
                onClicked: root.settingsRequested(
                    root.clone(root.draftSettings)
                )
            }
        }
    }
}
