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
    property bool runtimeAvailable: false
    property bool thirdPartySafeMode: false
    property var runtimeStates: []
    property string runtimeRetryBusyComponentId: ""
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
    signal componentAdoptionRequested(
        string componentId,
        string packageDigest,
        var defaultComponentSettings
    )
    signal componentAddToBarRequested(
        string componentId,
        string packageDigest,
        var defaultComponentSettings
    )
    signal componentRetryRequested(
        string componentId,
        string packageDigest
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

    function rawConfigRecord(component) {
        if (!component || !root.configSnapshot
                || typeof root.configSnapshot !== "object"
                || Array.isArray(root.configSnapshot)
                || !root.configSnapshot.components
                || typeof root.configSnapshot.components !== "object"
                || Array.isArray(root.configSnapshot.components)) {
            return null;
        }
        const record = root.configSnapshot.components[component.id];
        if (!record || typeof record !== "object" || Array.isArray(record))
            return null;
        return record;
    }

    function configRecord(component) {
        const record = root.rawConfigRecord(component);
        return record !== null
                && record.packageDigest === component.packageDigest
                && typeof record.enabled === "boolean"
            ? record : null;
    }

    function digestMismatch(component) {
        const record = root.rawConfigRecord(component);
        return record !== null
            && record.packageDigest !== component.packageDigest;
    }

    function desiredStateAvailable(component) {
        return root.catalogJoinAvailable
            && root.configRecord(component) !== null;
    }

    function desiredEnabled(component) {
        const record = root.configRecord(component);
        return record !== null && record.enabled;
    }

    function hasPlacement(component) {
        if (root.configRecord(component) === null || !root.configSnapshot
                || typeof root.configSnapshot !== "object"
                || !root.configSnapshot.instances
                || typeof root.configSnapshot.instances !== "object"
                || Array.isArray(root.configSnapshot.instances)) {
            return false;
        }
        const instanceIds = Object.keys(root.configSnapshot.instances).filter(
            instanceId => {
                const instance = root.configSnapshot.instances[instanceId];
                return instance && typeof instance === "object"
                    && !Array.isArray(instance)
                    && instance.componentId === component.id
                    && instance.enabled === true;
            }
        );
        if (instanceIds.length === 0 || !root.configSnapshot.layouts
                || typeof root.configSnapshot.layouts !== "object"
                || Array.isArray(root.configSnapshot.layouts)
                || !root.configSnapshot.layouts.bars
                || typeof root.configSnapshot.layouts.bars !== "object"
                || Array.isArray(root.configSnapshot.layouts.bars)) {
            return false;
        }
        const bars = root.configSnapshot.layouts.bars;
        const declarativeV1 = component.origin === "user"
            && component.type === "bar-widget"
            && component.runtime
            && component.runtime.kind === "declarative-v1"
            && component.activationSupported !== false;
        const layouts = declarativeV1 ? [bars.main] : Object.values(bars);
        return layouts.some(layout => {
            if (!layout || typeof layout !== "object"
                    || Array.isArray(layout) || !layout.regions
                    || typeof layout.regions !== "object"
                    || Array.isArray(layout.regions)) {
                return false;
            }
            return ["start", "center", "end"].some(regionName =>
                root.listValue(layout.regions[regionName]).some(
                    instanceId => instanceIds.includes(instanceId)
                )
            );
        });
    }

    function addToBarVisible(component) {
        return component && component.origin === "user"
            && component.type === "bar-widget"
            && component.activationSupported !== false
            && !root.digestMismatch(component)
            && !root.hasPlacement(component);
    }

    function adoptPackageVisible(component) {
        return component && component.origin === "user"
            && component.type === "bar-widget"
            && component.activationSupported !== false
            && root.digestMismatch(component);
    }

    function adoptPackageAvailable(component) {
        return root.adoptPackageVisible(component)
            && root.catalogJoinAvailable
            && root.configWritable
            && !root.managerBusy
            && !root.configBusy
            && !root.packageOperationBusy;
    }

    function addToBarAvailable(component) {
        return root.addToBarVisible(component)
            && root.catalogJoinAvailable
            && root.configWritable
            && !root.managerBusy
            && !root.configBusy
            && !root.packageOperationBusy
            && !root.thirdPartySafeMode;
    }

    function defaultComponentSettings(component) {
        const result = {};
        if (!component)
            return result;
        for (const definition of root.listValue(
            component.settingsDefinitions
        )) {
            if (!definition || definition.scope !== "component"
                    || typeof definition.key !== "string"
                    || !Object.prototype.hasOwnProperty.call(
                        definition, "defaultValue"
                    )) {
                continue;
            }
            result[definition.key] = definition.defaultValue;
        }
        return result;
    }

    function runtimeState(component) {
        if (!component)
            return null;
        return root.listValue(root.runtimeStates).find(state => state
            && state.componentId === component.id
            && state.packageDigest === component.packageDigest) || null;
    }

    function quarantined(component) {
        const state = root.runtimeState(component);
        return state !== null && state.state === "quarantined";
    }

    function runtimeFailureDescription(reason) {
        switch (reason) {
        case "timeout":
            return qsTr("The trusted renderer did not stabilize in time.");
        case "incomplete-startup":
            return qsTr("Activation was interrupted before it completed.");
        case "render-failed":
            return qsTr("The trusted renderer could not create this widget.");
        case "protocol-invalid":
            return qsTr("The runtime rejected invalid activation data.");
        default:
            return "";
        }
    }

    function toggleAvailable(component) {
        return root.desiredStateAvailable(component)
            && component.activationSupported !== false
            && root.configWritable
            && !root.managerBusy
            && !root.packageOperationBusy
            && !root.configBusy
            && !(component.origin === "user"
                && root.thirdPartySafeMode);
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
        if (component.origin === "user"
                && component.activationSupported !== false
                && root.thirdPartySafeMode) {
            return qsTr("Temporarily disabled while third-party runtime safe mode is active.");
        }
        if (record === null && component.origin === "user") {
            if (root.digestMismatch(component))
                return qsTr("A different package version is installed. Using it resets this widget's settings to the installed defaults and keeps it disabled.");
            if (component.activationSupported === false) {
                return component.compatibilityReason
                    ? qsTr("Installed disabled. %1").arg(
                        component.compatibilityReason
                    )
                    : qsTr("Installed disabled. This shell cannot activate it.");
            }
            if (root.addToBarVisible(component))
                return qsTr("Installed disabled. Add it to the bar when you're ready.");
            return qsTr("Installed disabled. Review it before enabling.");
        }
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
        const runtime = root.runtimeState(component);
        if (runtime !== null && runtime.state === "quarantined") {
            const detail = root.runtimeFailureDescription(runtime.reason);
            return detail.length > 0
                ? qsTr("Disabled because activation did not complete. %1").arg(
                    detail
                )
                : qsTr("Disabled because activation did not complete. You can try it again when ready.");
        }
        if (root.addToBarVisible(component)) {
            return qsTr("Configured but not on the bar.");
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
                objectName: "componentRuntimeSafeModeWarning"
                Layout.fillWidth: true
                visible: root.thirdPartySafeMode
                padding: 16

                background: Rectangle {
                    color: "#33251a"
                    radius: 12
                    border.color: "#8bf6ad55"
                }

                Label {
                    anchors.fill: parent
                    text: qsTr("Third-party components are temporarily disabled because their runtime recovery data could not be trusted. Built-in features remain available.")
                    color: "#ffd5a1"
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
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
                            adoptPackageVisible:
                                root.adoptPackageVisible(modelData)
                            adoptPackageEnabled:
                                root.adoptPackageAvailable(modelData)
                            addToBarVisible:
                                root.addToBarVisible(modelData)
                            addToBarEnabled:
                                root.addToBarAvailable(modelData)
                            retryVisible: root.quarantined(modelData)
                            retryEnabled: root.runtimeAvailable
                                && !root.thirdPartySafeMode
                                && root.runtimeRetryBusyComponentId.length === 0
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
                            onAdoptPackageRequested: component =>
                                root.componentAdoptionRequested(
                                    component.id,
                                    component.packageDigest,
                                    root.defaultComponentSettings(component)
                                )
                            onAddToBarRequested: component =>
                                root.componentAddToBarRequested(
                                    component.id,
                                    component.packageDigest,
                                    root.defaultComponentSettings(component)
                                )
                            onRetryRequested: component =>
                                root.componentRetryRequested(
                                    component.id,
                                    component.packageDigest
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
