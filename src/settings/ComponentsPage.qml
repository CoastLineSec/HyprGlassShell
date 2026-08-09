pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Page {
    id: root

    property bool managerAvailable: false
    property bool managerBusy: false
    property string managerCatalogDigest: ""
    property var components: []
    property string managerError: ""
    property bool inspectionBusy: false
    property bool packageOperationBusy: false
    property var inspectionReview: ({})
    property string inspectionToken: ""
    property string packageError: ""
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
    property var settingsComponent: null
    property var removalComponent: null

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
    signal inspectPackageRequested(url packageUrl)
    signal cancelInspectionRequested()
    signal installInspectedPackageRequested()
    signal packageRemovalRequested(
        string componentId,
        string packageDigest,
        string catalogDigest
    )
    signal componentSettingsRequested(
        string componentId,
        string packageDigest,
        var settings
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
            && component.activationSupported !== false
            && root.configWritable
            && !root.managerBusy
            && !root.packageOperationBusy
            && !root.configBusy;
    }

    function inspectionReviewAvailable() {
        return root.inspectionReview
            && typeof root.inspectionReview === "object"
            && !Array.isArray(root.inspectionReview)
            && Object.keys(root.inspectionReview).length > 0;
    }

    function statusText(component) {
        if (!root.catalogJoinAvailable)
            return qsTr("Unavailable");
        const record = root.configRecord(component);
        if (record === null && component.origin === "user")
            return qsTr("Installed disabled. Review it before enabling.");
        if (record === null)
            return qsTr("Configuration does not match the installed package.");
        if (component.origin === "user"
                && component.activationSupported === false) {
            return component.compatibilityReason
                ? qsTr("Installed disabled. %1").arg(
                    component.compatibilityReason
                )
                : qsTr("Installed disabled. This shell cannot activate it.");
        }
        if (component.origin === "user" && !record.enabled)
            return qsTr("Installed disabled. Review it before enabling.");
        return record.enabled ? qsTr("Enabled") : qsTr("Disabled");
    }

    function openComponentSettings(component) {
        const record = root.configRecord(component);
        if (!component || component.origin !== "user"
                || component.type === "shell-application"
                || !root.catalogJoinAvailable || !root.configWritable
                || root.managerBusy || root.configBusy
                || root.packageOperationBusy) {
            return;
        }
        root.settingsComponent = component;
        genericSettings.begin(
            component,
            record && record.settings ? record.settings : {}
        );
        componentSettingsDialog.open();
    }

    function requestComponentRemoval(component) {
        if (!component || component.origin !== "user"
                || component.removable !== true) {
            return;
        }
        root.removalComponent = component;
        componentRemovalDialog.open();
    }

    function packageRemovalCompleted(componentId) {
        if (root.removalComponent
                && root.removalComponent.id === componentId) {
            componentRemovalDialog.close();
            root.removalComponent = null;
        }
    }

    function inspectSelectedPackage(packageUrl) {
        root.inspectPackageRequested(packageUrl);
    }

    onInspectionTokenChanged: {
        if (root.inspectionToken.length > 0
                && root.inspectionReviewAvailable()) {
            componentReviewDialog.open();
        } else if (!root.packageOperationBusy) {
            componentReviewDialog.close();
        }
    }
    onInspectionReviewChanged: {
        if (root.inspectionToken.length > 0
                && root.inspectionReviewAvailable()) {
            componentReviewDialog.open();
        } else if (!root.packageOperationBusy) {
            componentReviewDialog.close();
        }
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

            RowLayout {
                Layout.fillWidth: true
                spacing: 18

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

                Button {
                    objectName: "installComponent"
                    text: root.inspectionBusy
                        ? qsTr("Inspecting…") : qsTr("Install from file…")
                    enabled: root.managerAvailable
                        && !root.managerBusy
                        && !root.inspectionBusy
                        && !root.packageOperationBusy
                    Accessible.name: qsTr("Install a third-party component from a local file")
                    onClicked: componentFileDialog.open()
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

            Frame {
                objectName: "componentPackageWarning"
                Layout.fillWidth: true
                visible: root.packageError.length > 0
                    && root.inspectionToken.length === 0
                padding: 16

                background: Rectangle {
                    color: "#3a1f27"
                    radius: 12
                    border.color: "#8bff7187"
                }

                Label {
                    anchors.fill: parent
                    text: root.packageError
                    color: "#ffb8c3"
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
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
                            packageOperationBusy:
                                root.packageOperationBusy
                            configureEnabled:
                                root.catalogJoinAvailable
                                && root.configWritable
                                && !root.managerBusy
                                && !root.configBusy
                                && !root.packageOperationBusy
                            removeEnabled:
                                root.managerAvailable
                                && root.managerCatalogDigest.length > 0
                                && !root.managerBusy
                                && !root.packageOperationBusy
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
                            onConfigureRequested: component =>
                                root.openComponentSettings(component)
                            onRemoveRequested: component =>
                                root.requestComponentRemoval(component)
                        }
                    }
                }
            }

            Item {
                Layout.preferredHeight: 12
            }
        }
    }

    FileDialog {
        id: componentFileDialog

        objectName: "componentInstallFileDialog"
        title: qsTr("Choose a HyprShelld component")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("HyprShelld components (*.hyprshelld-component)"),
            qsTr("All files (*)")
        ]
        onAccepted: root.inspectSelectedPackage(selectedFile)
    }

    ComponentReviewDialog {
        id: componentReviewDialog

        review: root.inspectionReview
        inspectionToken: root.inspectionToken
        operationBusy: root.packageOperationBusy
        errorText: root.packageError

        onCancelRequested: {
            close();
            root.cancelInspectionRequested();
        }
        onInstallRequested: root.installInspectedPackageRequested()
    }

    Dialog {
        id: componentSettingsDialog

        objectName: "componentSettingsDialog"
        title: root.settingsComponent && root.settingsComponent.name
            ? qsTr("Configure %1").arg(root.settingsComponent.name)
            : qsTr("Configure component")
        modal: true
        width: Math.min(720, root.width - 48)
        height: Math.min(720, root.height - 48)
        standardButtons: Dialog.Close

        contentItem: ScrollView {
            clip: true
            contentWidth: availableWidth

            GenericComponentSettings {
                id: genericSettings

                width: parent.width
                controlsEnabled: root.settingsComponent !== null
                    && root.configWritable
                    && !root.configBusy
                    && !root.packageOperationBusy
                saving: root.settingsComponent !== null
                    && root.pendingComponentId
                        === root.settingsComponent.id
                errorText: root.settingsComponent !== null
                        && root.lastErrorComponentId
                            === root.settingsComponent.id
                    ? root.configError : ""

                onSettingsRequested: settings => {
                    if (!root.settingsComponent)
                        return;
                    root.componentSettingsRequested(
                        root.settingsComponent.id,
                        root.settingsComponent.packageDigest,
                        settings
                    );
                    componentSettingsDialog.close();
                }
            }
        }

        onClosed: root.settingsComponent = null
    }

    Dialog {
        id: componentRemovalDialog

        objectName: "componentRemovalDialog"
        title: qsTr("Remove third-party component?")
        modal: true
        width: Math.min(520, root.width - 48)
        standardButtons: Dialog.NoButton

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: root.removalComponent && root.removalComponent.name
                    ? qsTr("Remove %1 from HyprShelld?").arg(
                        root.removalComponent.name
                    ) : qsTr("Remove this component from HyprShelld?")
                color: root.palette.text
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("The installed package will be removed. Its saved choices and placements are preserved in case you install the same package again. This does not delete the original package file you selected.")
                color: root.palette.placeholderText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            RowLayout {
                Layout.fillWidth: true

                Item { Layout.fillWidth: true }

                Button {
                    objectName: "cancelComponentRemoval"
                    text: qsTr("Cancel")
                    enabled: !root.packageOperationBusy
                    onClicked: componentRemovalDialog.close()
                }

                Button {
                    objectName: "confirmComponentRemoval"
                    text: root.packageOperationBusy
                        ? qsTr("Removing…") : qsTr("Remove")
                    enabled: root.removalComponent !== null
                        && !root.packageOperationBusy
                    highlighted: true
                    onClicked: {
                        const component = root.removalComponent;
                        if (!component)
                            return;
                        root.packageRemovalRequested(
                            component.id,
                            component.packageDigest,
                            root.managerCatalogDigest
                        );
                    }
                }
            }
        }

        onClosed: {
            if (!root.packageOperationBusy)
                root.removalComponent = null;
        }
    }
}
