pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root

    property bool managerAvailable: false
    property bool managerBusy: false
    property string managerCatalogDigest: ""
    property var components: []
    property string managerError: ""
    property bool configAvailable: false
    property bool configCatalogAvailable: false
    property bool configWritable: false
    property bool configBusy: false
    property string configCatalogDigest: ""
    property var configSnapshot: ({})
    property string pendingComponentId: ""
    property string lastErrorComponentId: ""
    property string configError: ""
    property real contentTopMargin: 28

    readonly property var categories: [
        { type: "bar-widget", title: qsTr("Bar Widgets") },
        { type: "desktop-widget", title: qsTr("Desktop Widgets") },
        { type: "shell-service", title: qsTr("Services") },
        { type: "shell-application", title: qsTr("Shell Applications") }
    ]
    readonly property bool catalogJoinAvailable:
        managerAvailable
        && configAvailable
        && configCatalogAvailable
        && managerCatalogDigest.length > 0
        && managerCatalogDigest === configCatalogDigest
    readonly property string availabilityMessage: {
        if (!managerAvailable)
            return qsTr("The component catalog is unavailable. Installed components remain visible but cannot be changed until it reconnects.");
        if (!configAvailable)
            return qsTr("Component settings are unavailable. Components cannot be enabled or disabled until settings reconnect.");
        if (!configCatalogAvailable)
            return qsTr("The component catalog is not available to the settings service yet. Component changes are temporarily disabled.");
        if (managerCatalogDigest.length === 0
                || configCatalogDigest.length === 0
                || managerCatalogDigest !== configCatalogDigest) {
            return qsTr("The component catalog changed while Settings was loading. Component changes are disabled until both services agree on the current catalog.");
        }
        if (!configWritable)
            return qsTr("Component settings are currently read-only.");
        return "";
    }

    signal componentEnabledRequested(
        string componentId,
        string packageDigest,
        bool enabled
    )

    function componentsForType(type) {
        if (!Array.isArray(root.components))
            return [];
        return root.components.filter(component =>
            component && typeof component === "object"
                && component.type === type
                && typeof component.id === "string"
                && typeof component.packageDigest === "string"
        ).slice().sort((left, right) => {
            const leftName = left.name || left.id;
            const rightName = right.name || right.id;
            const byName = leftName.localeCompare(rightName);
            return byName !== 0 ? byName : left.id.localeCompare(right.id);
        });
    }

    function configRecord(component) {
        if (!component || !root.configSnapshot
                || typeof root.configSnapshot !== "object"
                || Array.isArray(root.configSnapshot)
                || !root.configSnapshot.components
                || typeof root.configSnapshot.components !== "object"
                || Array.isArray(root.configSnapshot.components)) {
            return null;
        }
        const record = root.configSnapshot.components[component.id];
        if (!record || typeof record !== "object" || Array.isArray(record)
                || record.packageDigest !== component.packageDigest
                || typeof record.enabled !== "boolean") {
            return null;
        }
        return record;
    }

    function desiredStateAvailable(component) {
        return root.catalogJoinAvailable
            && root.configRecord(component) !== null;
    }

    function desiredEnabled(component) {
        const record = root.configRecord(component);
        return record !== null && record.enabled;
    }

    function toggleAvailable(component) {
        return root.desiredStateAvailable(component)
            && root.configWritable
            && !root.managerBusy
            && !root.configBusy;
    }

    function statusText(component) {
        if (!root.catalogJoinAvailable)
            return qsTr("Unavailable");
        const record = root.configRecord(component);
        if (record === null)
            return qsTr("Configuration does not match the installed package.");
        return record.enabled ? qsTr("Enabled") : qsTr("Disabled");
    }

    background: Rectangle {
        color: root.palette.window
    }

    ScrollView {
        objectName: "componentsScrollView"
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            x: Math.max(24, (root.width - width) / 2)
            y: root.contentTopMargin
            width: Math.max(0, Math.min(root.width - 48, 980))
            spacing: 22

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: qsTr("Components")
                    color: root.palette.text
                    font.pixelSize: 30
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Choose which built-in and third-party shell features are available. Built-in feature settings stay in their natural Settings pages.")
                    color: root.palette.placeholderText
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                }
            }

            Frame {
                objectName: "componentsAvailabilityWarning"
                Layout.fillWidth: true
                visible: root.availabilityMessage.length > 0
                    || root.managerError.length > 0
                padding: 16

                background: Rectangle {
                    color: "#33251a"
                    radius: 12
                    border.color: "#8bf6ad55"
                }

                Label {
                    anchors.fill: parent
                    text: root.availabilityMessage.length > 0
                        ? root.availabilityMessage : root.managerError
                    color: "#ffd5a1"
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text
                }
            }

            Repeater {
                model: root.categories

                ColumnLayout {
                    required property var modelData

                    Layout.fillWidth: true
                    spacing: 10

                    readonly property var categoryComponents:
                        root.componentsForType(modelData.type)

                    Label {
                        objectName: "componentCategory-" + modelData.type
                        text: modelData.title
                        color: root.palette.text
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: parent.categoryComponents.length === 0
                        text: qsTr("No components are installed in this category.")
                        color: root.palette.placeholderText
                        font.pixelSize: 13
                    }

                    Repeater {
                        model: parent.categoryComponents

                        ComponentPill {
                            required property var modelData

                            Layout.fillWidth: true
                            component: modelData
                            desiredStateAvailable:
                                root.desiredStateAvailable(modelData)
                            desiredEnabled: root.desiredEnabled(modelData)
                            toggleEnabled: root.toggleAvailable(modelData)
                            pending: root.pendingComponentId === modelData.id
                            statusText: root.statusText(modelData)
                            errorText: root.lastErrorComponentId === modelData.id
                                ? root.configError : ""

                            onComponentEnabledRequested: (
                                componentId,
                                packageDigest,
                                enabled
                            ) => root.componentEnabledRequested(
                                componentId,
                                packageDigest,
                                enabled
                            )
                        }
                    }
                }
            }

            Item {
                Layout.preferredHeight: 12
            }
        }
    }
}
