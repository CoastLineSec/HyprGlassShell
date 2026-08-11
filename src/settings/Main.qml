pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.Client

ApplicationWindow {
    id: root

    width: 1080
    height: 720
    minimumWidth: 620
    minimumHeight: 480
    visible: true
    title: qsTr("HyprShelld Settings")
    color: "#101319"

    readonly property real sidebarWidth: Math.min(232, Math.max(196, width * 0.22))
    property string currentPage: "bar"
    readonly property string workspaceComponentId:
        "io.github.coastlinesec.hyprshelld.workspace-switcher"
    readonly property string workspaceInstanceId:
        "7b4e2329-4320-4e15-894d-218fa690d782"
    readonly property var workspaceDefaults: ({
        showIdentifiers: true,
        showNames: false,
        showApplications: false,
        maximumApplications: 3,
        occupiedOnly: false,
        scrollMode: "disabled"
    })
    readonly property var workspaceInstanceState:
        workspaceSettingsFromSnapshot(ComponentConfigClient.snapshot)
    readonly property var workspaceComponentState:
        workspaceComponentStateFromServices(
            ComponentManagerClient.available,
            ComponentManagerClient.catalogDigest,
            ComponentManagerClient.components,
            ComponentConfigClient.available,
            ComponentConfigClient.catalogAvailable,
            ComponentConfigClient.catalogDigest,
            ComponentConfigClient.snapshot
        )
    readonly property int failedComponentCount: shellHealthWarning.failedComponentCount
    readonly property string desktopStatusText: {
        if (CoordinatorClient.available) {
            if (CoordinatorClient.healthy)
                return qsTr("Desktop ready");
            return failedComponentCount === 1
                ? qsTr("1 component failed")
                : qsTr("%1 components failed").arg(failedComponentCount);
        }
        if (shellRuntimeStatus.busy && !shellRuntimeStatus.available)
            return qsTr("Checking services…");
        if (shellRuntimeStatus.available) {
            if (failedComponentCount === 1)
                return qsTr("1 component failed");
            if (failedComponentCount > 1)
                return qsTr("%1 components failed").arg(failedComponentCount);
            return shellRuntimeStatus.targetState === "inactive"
                ? qsTr("Desktop services stopped")
                : qsTr("Health service unavailable");
        }
        return qsTr("Status unavailable");
    }
    readonly property color desktopStatusColor: {
        if (failedComponentCount > 0)
            return "#fb7185";
        if (CoordinatorClient.available && CoordinatorClient.healthy)
            return "#68d391";
        return "#f6ad55";
    }

    function appearanceValuesWithSharedBorder(values) {
        let resolved = null;
        try {
            resolved = JSON.parse(JSON.stringify(values));
        } catch (error) {
            return values;
        }
        if (!resolved || typeof resolved !== "object"
                || Array.isArray(resolved)
                || !ConfigClient.syncHyprlandWindowBorders) {
            return values;
        }
        resolved["hyprland.general.border_size"] =
            ConfigClient.shellBorderEnabled
                ? ConfigClient.shellBorderWidth : 0;
        resolved["hyprland.decoration.rounding"] =
            ConfigClient.shellBorderRadius;
        return resolved;
    }

    function isWorkspaceSettings(settings) {
        if (!settings || typeof settings !== "object"
                || Array.isArray(settings)) {
            return false;
        }
        if (typeof settings.showIdentifiers !== "boolean"
                || typeof settings.showNames !== "boolean"
                || typeof settings.showApplications !== "boolean"
                || typeof settings.occupiedOnly !== "boolean") {
            return false;
        }
        if (typeof settings.maximumApplications !== "number"
                || Math.floor(settings.maximumApplications)
                    !== settings.maximumApplications
                || settings.maximumApplications < 1
                || settings.maximumApplications > 5) {
            return false;
        }
        return ["disabled", "normal", "reversed"].includes(
            settings.scrollMode
        );
    }

    function workspaceSettingsFromSnapshot(snapshot) {
        const fallback = {
            valid: false,
            enabled: false,
            showIdentifiers: root.workspaceDefaults.showIdentifiers,
            showNames: root.workspaceDefaults.showNames,
            showApplications: root.workspaceDefaults.showApplications,
            maximumApplications:
                root.workspaceDefaults.maximumApplications,
            occupiedOnly: root.workspaceDefaults.occupiedOnly,
            scrollMode: root.workspaceDefaults.scrollMode
        };
        if (!snapshot || typeof snapshot !== "object"
                || Array.isArray(snapshot)
                || !snapshot.instances
                || typeof snapshot.instances !== "object"
                || Array.isArray(snapshot.instances)) {
            return fallback;
        }
        const instance = snapshot.instances[root.workspaceInstanceId];
        if (!instance || typeof instance !== "object"
                || Array.isArray(instance)
                || instance.componentId !== root.workspaceComponentId
                || typeof instance.enabled !== "boolean"
                || !root.isWorkspaceSettings(instance.settings)) {
            return fallback;
        }
        return {
            valid: true,
            enabled: instance.enabled,
            showIdentifiers: instance.settings.showIdentifiers,
            showNames: instance.settings.showNames,
            showApplications: instance.settings.showApplications,
            maximumApplications: instance.settings.maximumApplications,
            occupiedOnly: instance.settings.occupiedOnly,
            scrollMode: instance.settings.scrollMode
        };
    }

    function catalogComponent(components, componentId) {
        if (!Array.isArray(components))
            return null;
        for (const component of components) {
            if (component && typeof component === "object"
                    && component.id === componentId) {
                return component;
            }
        }
        return null;
    }

    function workspaceComponentStateFromServices(
        managerAvailable,
        managerDigest,
        components,
        configAvailable,
        configCatalogAvailable,
        configDigest,
        snapshot
    ) {
        const unavailable = {
            available: false,
            desiredEnabled: false,
            instanceEnabled: false,
            previewEnabled: false,
            packageDigest: ""
        };
        const definition = root.catalogComponent(
            components,
            root.workspaceComponentId
        );
        if (typeof managerDigest !== "string"
                || managerDigest.length !== 64
                || managerDigest !== configDigest
                || !definition
                || definition.type !== "bar-widget"
                || definition.origin !== "system"
                || typeof definition.packageDigest !== "string"
                || !/^[0-9a-f]{64}$/.test(definition.packageDigest)
                || !snapshot || typeof snapshot !== "object"
                || Array.isArray(snapshot)
                || !snapshot.components
                || typeof snapshot.components !== "object"
                || Array.isArray(snapshot.components)) {
            return unavailable;
        }
        const desired = snapshot.components[root.workspaceComponentId];
        const instance = root.workspaceSettingsFromSnapshot(snapshot);
        if (!desired || typeof desired !== "object"
                || Array.isArray(desired)
                || desired.packageDigest !== definition.packageDigest
                || typeof desired.enabled !== "boolean"
                || !instance.valid) {
            return unavailable;
        }
        return {
            available: managerAvailable
                && configAvailable
                && configCatalogAvailable,
            desiredEnabled: desired.enabled,
            instanceEnabled: instance.enabled,
            previewEnabled: desired.enabled && instance.enabled,
            packageDigest: definition.packageDigest
        };
    }

    function workspaceNaturalSettingsAvailable(state) {
        return state && state.available
            && (!state.desiredEnabled || state.instanceEnabled);
    }

    function workspaceSnapshotWithSettings(
        snapshot,
        showIdentifiers,
        showNames,
        showApplications,
        maximumApplications,
        occupiedOnly,
        scrollMode
    ) {
        const settings = {
            showIdentifiers: showIdentifiers,
            showNames: showNames,
            showApplications: showApplications,
            maximumApplications: maximumApplications,
            occupiedOnly: occupiedOnly,
            scrollMode: scrollMode
        };
        if (!root.isWorkspaceSettings(settings))
            return null;

        let replacement = null;
        try {
            replacement = JSON.parse(JSON.stringify(snapshot));
        } catch (error) {
            return null;
        }
        if (!replacement || typeof replacement !== "object"
                || Array.isArray(replacement)
                || !replacement.instances
                || typeof replacement.instances !== "object"
                || Array.isArray(replacement.instances)) {
            return null;
        }
        const instance = replacement.instances[root.workspaceInstanceId];
        if (!instance || typeof instance !== "object"
                || Array.isArray(instance)
                || instance.componentId !== root.workspaceComponentId
                || !root.isWorkspaceSettings(instance.settings)) {
            return null;
        }
        instance.settings = settings;
        return replacement;
    }

    function replaceWorkspaceSettings(
        showIdentifiers,
        showNames,
        showApplications,
        maximumApplications,
        occupiedOnly,
        scrollMode
    ) {
        const replacement = root.workspaceSnapshotWithSettings(
            ComponentConfigClient.snapshot,
            showIdentifiers,
            showNames,
            showApplications,
            maximumApplications,
            occupiedOnly,
            scrollMode
        );
        if (replacement)
            ComponentConfigClient.replaceSnapshot(replacement);
    }

    function resetWorkspaceSettings() {
        root.replaceWorkspaceSettings(
            root.workspaceDefaults.showIdentifiers,
            root.workspaceDefaults.showNames,
            root.workspaceDefaults.showApplications,
            root.workspaceDefaults.maximumApplications,
            root.workspaceDefaults.occupiedOnly,
            root.workspaceDefaults.scrollMode
        );
    }

    palette.window: "#101319"
    palette.windowText: "#f4f6fa"
    palette.base: "#171b22"
    palette.text: "#f4f6fa"
    palette.button: "#29303b"
    palette.buttonText: "#f4f6fa"
    palette.highlight: "#7c91ff"
    palette.highlightedText: "#ffffff"
    palette.placeholderText: "#aeb8c6"
    palette.mid: "#3a424f"

    ShellRuntimeStatus {
        id: shellRuntimeStatus

        active: !CoordinatorClient.available
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: root.sidebarWidth
            color: root.palette.base

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    Layout.topMargin: 4
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 11
                        color: root.palette.highlight

                        Label {
                            anchors.centerIn: parent
                            text: "H"
                            color: root.palette.highlightedText
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            Accessible.ignored: true
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Settings")
                        color: root.palette.text
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    text: qsTr("Desktop")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                ItemDelegate {
                    id: barNavigationItem

                    objectName: "barNavigationItem"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    checkable: true
                    checked: root.currentPage === "bar"
                    autoExclusive: true
                    focusPolicy: Qt.StrongFocus
                    leftPadding: 18
                    rightPadding: 12
                    topPadding: 8
                    bottomPadding: 8
                    Accessible.role: Accessible.PageTab
                    Accessible.name: qsTr("Bar settings")
                    Accessible.checked: checked

                    onClicked: root.currentPage = "bar"

                    background: Rectangle {
                        radius: 12
                        color: barNavigationItem.checked
                            ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18)
                            : barNavigationItem.hovered
                                ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06)
                                : "transparent"
                        border.width: barNavigationItem.activeFocus ? 2 : 0
                        border.color: root.palette.highlight

                        Rectangle {
                            anchors {
                                left: parent.left
                                leftMargin: 5
                                verticalCenter: parent.verticalCenter
                            }

                            width: 3
                            height: 24
                            radius: 2
                            visible: barNavigationItem.checked
                            color: root.palette.highlight
                        }
                    }

                    contentItem: RowLayout {
                        spacing: 11

                        Item {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22

                            Rectangle {
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    top: parent.top
                                    topMargin: 3
                                }

                                height: 7
                                radius: 3
                                color: root.palette.text
                            }

                            Rectangle {
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    bottom: parent.bottom
                                }

                                height: 8
                                radius: 3
                                color: root.palette.mid
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Bar")
                            color: root.palette.text
                            font.pixelSize: 14
                            font.weight: barNavigationItem.checked ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                    }
                }

                ItemDelegate {
                    id: appearanceNavigationItem

                    objectName: "appearanceNavigationItem"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    checkable: true
                    checked: root.currentPage === "appearance"
                    autoExclusive: true
                    focusPolicy: Qt.StrongFocus
                    leftPadding: 18
                    rightPadding: 12
                    topPadding: 8
                    bottomPadding: 8
                    Accessible.role: Accessible.PageTab
                    Accessible.name: qsTr("Appearance settings")
                    Accessible.checked: checked

                    onClicked: root.currentPage = "appearance"

                    background: Rectangle {
                        radius: 12
                        color: appearanceNavigationItem.checked
                            ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18)
                            : appearanceNavigationItem.hovered
                                ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06)
                                : "transparent"
                        border.width:
                            appearanceNavigationItem.activeFocus ? 2 : 0
                        border.color: root.palette.highlight

                        Rectangle {
                            anchors {
                                left: parent.left
                                leftMargin: 5
                                verticalCenter: parent.verticalCenter
                            }
                            width: 3
                            height: 24
                            radius: 2
                            visible: appearanceNavigationItem.checked
                            color: root.palette.highlight
                        }
                    }

                    contentItem: RowLayout {
                        spacing: 11

                        Item {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22

                            Rectangle {
                                anchors.centerIn: parent
                                width: 20
                                height: 16
                                radius: 5
                                color: "transparent"
                                border.width: 2
                                border.color: root.palette.text

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 7
                                    height: 7
                                    radius: 3
                                    color: appearanceNavigationItem.checked
                                        ? root.palette.highlight
                                        : root.palette.placeholderText
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Appearance")
                            color: root.palette.text
                            font.pixelSize: 14
                            font.weight: appearanceNavigationItem.checked
                                ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                    }
                }

                ItemDelegate {
                    id: displaysNavigationItem

                    objectName: "displaysNavigationItem"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    checkable: true
                    checked: root.currentPage === "displays"
                    autoExclusive: true
                    focusPolicy: Qt.StrongFocus
                    leftPadding: 18
                    rightPadding: 12
                    topPadding: 8
                    bottomPadding: 8
                    Accessible.role: Accessible.PageTab
                    Accessible.name: qsTr("Display settings")
                    Accessible.checked: checked

                    onClicked: root.currentPage = "displays"

                    background: Rectangle {
                        radius: 12
                        color: displaysNavigationItem.checked
                            ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18)
                            : displaysNavigationItem.hovered
                                ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06)
                                : "transparent"
                        border.width: displaysNavigationItem.activeFocus ? 2 : 0
                        border.color: root.palette.highlight

                        Rectangle {
                            anchors {
                                left: parent.left
                                leftMargin: 5
                                verticalCenter: parent.verticalCenter
                            }
                            width: 3
                            height: 24
                            radius: 2
                            visible: displaysNavigationItem.checked
                            color: root.palette.highlight
                        }
                    }

                    contentItem: RowLayout {
                        spacing: 11

                        Item {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22

                            Rectangle {
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    top: parent.top
                                }
                                height: 14
                                radius: 3
                                color: root.palette.text
                                border.color: displaysNavigationItem.checked
                                    ? root.palette.highlight : root.palette.mid

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: parent.width - 5
                                    height: parent.height - 5
                                    radius: 1
                                    color: root.palette.base
                                }
                            }

                            Rectangle {
                                anchors {
                                    horizontalCenter: parent.horizontalCenter
                                    bottom: parent.bottom
                                }
                                width: 11
                                height: 3
                                radius: 2
                                color: root.palette.placeholderText
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Displays")
                            color: root.palette.text
                            font.pixelSize: 14
                            font.weight: displaysNavigationItem.checked
                                ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                    }
                }

                ItemDelegate {
                    id: componentsNavigationItem

                    objectName: "componentsNavigationItem"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    checkable: true
                    checked: root.currentPage === "components"
                    autoExclusive: true
                    focusPolicy: Qt.StrongFocus
                    leftPadding: 18
                    rightPadding: 12
                    topPadding: 8
                    bottomPadding: 8
                    Accessible.role: Accessible.PageTab
                    Accessible.name: qsTr("Component settings")
                    Accessible.checked: checked

                    onClicked: root.currentPage = "components"

                    background: Rectangle {
                        radius: 12
                        color: componentsNavigationItem.checked
                            ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18)
                            : componentsNavigationItem.hovered
                                ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06)
                                : "transparent"
                        border.width: componentsNavigationItem.activeFocus ? 2 : 0
                        border.color: root.palette.highlight

                        Rectangle {
                            anchors {
                                left: parent.left
                                leftMargin: 5
                                verticalCenter: parent.verticalCenter
                            }

                            width: 3
                            height: 24
                            radius: 2
                            visible: componentsNavigationItem.checked
                            color: root.palette.highlight
                        }
                    }

                    contentItem: RowLayout {
                        spacing: 11

                        Item {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22

                            Repeater {
                                model: 4

                                Rectangle {
                                    required property int index

                                    width: 8
                                    height: 8
                                    x: (index % 2) * 12
                                    y: Math.floor(index / 2) * 12
                                    radius: 2
                                    color: index === 0
                                        ? root.palette.highlight
                                        : root.palette.text
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Components")
                            color: root.palette.text
                            font.pixelSize: 14
                            font.weight: componentsNavigationItem.checked
                                ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    radius: 12
                    color: Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.04)
                    border.color: root.palette.mid

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 9

                        Rectangle {
                            Layout.preferredWidth: 9
                            Layout.preferredHeight: 9
                            radius: width / 2
                            color: root.desktopStatusColor
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.desktopStatusText
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: root.palette.mid
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: shellHealthWarning.warningVisible
                    ? shellHealthWarning.implicitHeight + 48
                    : 0
                visible: shellHealthWarning.warningVisible

                ShellHealthWarning {
                    id: shellHealthWarning

                    anchors {
                        top: parent.top
                        topMargin: 28
                        horizontalCenter: parent.horizontalCenter
                    }
                    width: Math.max(0, Math.min(parent.width - 48, 980))
                    coordinatorAvailable: CoordinatorClient.available
                    coordinatorHealthy: CoordinatorClient.healthy
                    coordinatorFailedUnits: CoordinatorClient.failedUnits
                    restartBusy: CoordinatorClient.busy
                    restartingUnit: CoordinatorClient.restartingUnit
                    restartErrorUnit: CoordinatorClient.lastErrorUnit
                    restartError: CoordinatorClient.lastErrorMessage
                    fallbackActive: shellRuntimeStatus.active
                    fallbackAvailable: shellRuntimeStatus.available
                    fallbackBusy: shellRuntimeStatus.busy
                    targetState: shellRuntimeStatus.targetState
                    coordinatorState: shellRuntimeStatus.coordinatorState
                    configurationState: shellRuntimeStatus.configurationState
                    componentManagerState:
                        shellRuntimeStatus.componentManagerState
                    compositorState: shellRuntimeStatus.compositorState
                    surfaceState: shellRuntimeStatus.surfaceState

                    onRestartRequested: unitName => {
                        CoordinatorClient.restartComponent(unitName);
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentPage === "appearance" ? 1
                    : root.currentPage === "displays" ? 2
                    : root.currentPage === "components" ? 3 : 0

                BarSettingsPage {
                    barHeight: ConfigClient.barHeight
                    minimumBarHeight: ConfigClient.minimumBarHeight
                    maximumBarHeight: ConfigClient.maximumBarHeight
                    defaultBarHeight: ConfigClient.defaultBarHeight
                    shellBorderEnabled: ConfigClient.shellBorderEnabled
                    shellBorderWidth: ConfigClient.shellBorderWidth
                    shellBorderRadius: ConfigClient.shellBorderRadius
                    syncHyprlandWindowBorders:
                        ConfigClient.syncHyprlandWindowBorders
                    workspaceShowIdentifiers:
                        root.workspaceInstanceState.showIdentifiers
                    workspaceShowNames:
                        root.workspaceInstanceState.showNames
                    workspaceShowApplications:
                        root.workspaceInstanceState.showApplications
                    workspaceMaximumApplications:
                        root.workspaceInstanceState.maximumApplications
                    workspaceOccupiedOnly:
                        root.workspaceInstanceState.occupiedOnly
                    workspaceScrollMode:
                        root.workspaceInstanceState.scrollMode
                    workspaceFeatureAvailable:
                        root.workspaceNaturalSettingsAvailable(
                            root.workspaceComponentState
                        )
                    workspaceFeatureEnabled:
                        root.workspaceComponentState.desiredEnabled
                    workspacePreviewEnabled:
                        root.workspaceComponentState.previewEnabled
                    coreServiceAvailable: ConfigClient.available
                    coreBusy: ConfigClient.busy
                    coreErrorText: ConfigClient.lastErrorMessage
                    coreRecoveryState: ConfigClient.recoveryState
                    componentServiceAvailable:
                        ComponentConfigClient.available
                    componentCatalogAvailable:
                        ComponentConfigClient.catalogAvailable
                        && ComponentManagerClient.available
                        && ComponentConfigClient.catalogDigest
                            === ComponentManagerClient.catalogDigest
                    componentWritable: [
                        "normal", "recovered", "defaulted"
                    ].includes(ComponentConfigClient.loadState)
                    workspaceInstanceAvailable:
                        root.workspaceComponentState.available
                    componentBusy: ComponentConfigClient.busy
                        || ComponentManagerClient.busy
                    componentErrorText:
                        ComponentConfigClient.lastErrorComponentId.length === 0
                            ? ComponentConfigClient.lastErrorMessage : ""
                    componentRecoveryState:
                        ComponentConfigClient.loadState
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onBarHeightRequested: height =>
                        ConfigClient.setBarHeight(height)
                    onResetBarHeightRequested: ConfigClient.resetBarHeight()
                    onSharedBorderRequested: (
                        enabled,
                        width,
                        radius,
                        syncHyprlandWindowBorders
                    ) => ConfigClient.setSharedBorder(
                        enabled,
                        width,
                        radius,
                        syncHyprlandWindowBorders
                    )
                    onResetSharedBorderRequested:
                        ConfigClient.resetSharedBorder()
                    onWorkspaceSwitcherRequested: (
                        showIdentifiers,
                        showNames,
                        showApplications,
                        maximumApplications,
                        occupiedOnly,
                        scrollMode
                    ) => root.replaceWorkspaceSettings(
                        showIdentifiers,
                        showNames,
                        showApplications,
                        maximumApplications,
                        occupiedOnly,
                        scrollMode
                    )
                    onResetWorkspaceSwitcherRequested:
                        root.resetWorkspaceSettings()
                }

                AppearancePage {
                    id: appearancePage

                    objectName: "appearancePage"
                    serviceAvailable: CompositorClient.available
                    writable: CompositorClient.writable
                    catalogAvailable: CompositorClient.catalogAvailable
                    appearanceAvailable: CompositorClient.appearanceAvailable
                    busy: CompositorClient.busy
                    busyOperation: CompositorClient.busyOperation
                    appearanceOptions: CompositorClient.appearanceOptions
                    appearanceValues: root.appearanceValuesWithSharedBorder(
                        CompositorClient.appearanceValues
                    )
                    sharedBorderAvailable: ConfigClient.available
                    sharedBorderBusy: ConfigClient.busy
                    windowBorderSynced:
                        ConfigClient.syncHyprlandWindowBorders
                    sharedBorderSyncState:
                        CompositorClient.sharedBorderSyncState
                    sharedBorderSyncError:
                        CompositorClient.sharedBorderSyncError
                    sharedBorderClientError:
                        ConfigClient.lastErrorMessage
                    sharedBorderConfigRevisionToken:
                        ConfigClient.revisionToken
                    sharedBorderVerifiedRevisionToken:
                        CompositorClient.sharedBorderSourceRevisionToken
                    revisionToken: CompositorClient.revisionToken
                    appliedRevision: CompositorClient.appliedRevision
                    loadState: CompositorClient.loadState
                    managementState: CompositorClient.managementState
                    applyState: CompositorClient.applyState
                    requiredActivation: CompositorClient.requiredActivation
                    confirmationState:
                        CompositorClient.displayConfirmationState
                    catalogErrorName:
                        CompositorClient.appearanceErrorName
                    catalogErrorMessage:
                        CompositorClient.appearanceErrorMessage
                    errorName: CompositorClient.lastErrorName
                    errorMessage: CompositorClient.lastErrorMessage
                    retryApplyAvailable:
                        CompositorClient.retryApplyAvailable
                    recoveryAvailable: CompositorClient.recoveryAvailable
                    contentTopMargin:
                        shellHealthWarning.warningVisible ? 0 : 28

                    onRefreshRequested: CompositorClient.refresh()
                    onOpenDisplaysRequested:
                        root.currentPage = "displays"
                    onSaveRequested: values =>
                        CompositorClient.saveAppearance(values)
                    onRetryApplyRequested: CompositorClient.retryApply()
                    onRecoveryRequested:
                        CompositorClient.recoverConfiguration()
                    onWindowBorderSyncRequested: sync =>
                        ConfigClient.setSharedBorder(
                            ConfigClient.shellBorderEnabled,
                            ConfigClient.shellBorderWidth,
                            ConfigClient.shellBorderRadius,
                            sync
                        )
                    onRetrySharedBorderSyncRequested:
                        CompositorClient.retrySharedBorderSync()
                }

                DisplaysPage {
                    id: displaysPage

                    objectName: "displaysPage"
                    serviceAvailable: CompositorClient.available
                        && CompositorClient.displayDiscoveryAvailable
                    writable: CompositorClient.writable
                    busy: CompositorClient.busy
                    snapshot: CompositorClient.snapshot
                    connectedDisplays: CompositorClient.connectedDisplays
                    topologyDigest: CompositorClient.topologyDigest
                    observedAtMs: CompositorClient.displaysObservedAtMs
                    revision: CompositorClient.revision
                    appliedRevision: CompositorClient.appliedRevision
                    loadState: CompositorClient.loadState
                    managementState: CompositorClient.managementState
                    applyState: CompositorClient.applyState
                    requiredActivation: CompositorClient.requiredActivation
                    confirmationState:
                        CompositorClient.displayConfirmationState
                    confirmationRevision:
                        CompositorClient.displayConfirmationRevision
                    confirmationDeadlineMs:
                        CompositorClient.displayConfirmationDeadlineMs
                    confirmationGeneration:
                        CompositorClient.displayConfirmationGeneration
                    confirmationOwned:
                        CompositorClient.displayConfirmationOwned
                    errorName: CompositorClient.lastErrorName
                    errorMessage: CompositorClient.lastErrorMessage
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onRefreshRequested: CompositorClient.refresh()
                    onAdoptionRequested:
                        CompositorClient.adoptManagedConfiguration()
                    onApplyRequested: CompositorClient.applyConfiguration()
                    onPreviewRequested: (outputs, timeoutSeconds) =>
                        CompositorClient.previewDisplayConfiguration(
                            outputs,
                            timeoutSeconds
                        )
                    onConfirmRequested:
                        CompositorClient.confirmDisplayConfiguration()
                    onRevertRequested:
                        CompositorClient.revertDisplayConfiguration()
                }

                ComponentsPage {
                    id: componentsPage
                    objectName: "componentsPage"
                    managerAvailable: ComponentManagerClient.available
                    managerBusy: ComponentManagerClient.busy
                    managerCatalogDigest:
                        ComponentManagerClient.catalogDigest
                    components: ComponentManagerClient.components
                    managerError: ComponentManagerClient.lastError
                    inspectionBusy:
                        ComponentManagerClient.inspectionBusy
                    packageOperationBusy:
                        ComponentManagerClient.packageOperationBusy
                    inspectionReview:
                        ComponentManagerClient.inspectionReview
                    inspectionToken:
                        ComponentManagerClient.inspectionToken
                    packageError: ComponentManagerClient.packageError
                    configAvailable: ComponentConfigClient.available
                    configCatalogAvailable:
                        ComponentConfigClient.catalogAvailable
                    configWritable: [
                        "normal", "recovered", "defaulted"
                    ].includes(ComponentConfigClient.loadState)
                    configBusy: ComponentConfigClient.busy
                    configCatalogDigest:
                        ComponentConfigClient.catalogDigest
                    configSnapshot: ComponentConfigClient.snapshot
                    pendingComponentId:
                        ComponentConfigClient.pendingComponentId
                    lastErrorComponentId:
                        ComponentConfigClient.lastErrorComponentId
                    configError: ComponentConfigClient.lastErrorMessage
                    runtimeAvailable: ComponentRuntimeClient.available
                        && ComponentRuntimeClient.runtimeHealthAvailable
                    thirdPartySafeMode:
                        ComponentRuntimeClient.thirdPartySafeMode
                    runtimeStates: ComponentRuntimeClient.runtimeStates
                    runtimeRetryBusyComponentId:
                        ComponentRuntimeClient.runtimeRetryBusyComponentId
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onComponentEnabledRequested: (
                        componentId,
                        packageDigest,
                        enabled
                    ) => ComponentConfigClient.setComponentEnabled(
                        componentId,
                        packageDigest,
                        enabled
                    )
                    onInspectPackageRequested: packageUrl =>
                        ComponentManagerClient.inspectPackage(packageUrl)
                    onCancelInspectionRequested:
                        ComponentManagerClient.cancelInspection()
                    onInstallInspectedPackageRequested:
                        ComponentManagerClient.installInspectedPackage()
                    onPackageRemovalRequested: (
                        componentId,
                        packageDigest,
                        catalogDigest
                    ) => ComponentManagerClient.removeComponent(
                        componentId,
                        packageDigest,
                        catalogDigest
                    )
                    onComponentSettingsRequested: (
                        componentId,
                        packageDigest,
                        settings
                    ) => ComponentConfigClient.setComponentSettings(
                        componentId,
                        packageDigest,
                        settings
                    )
                    onComponentAdoptionRequested: (
                        componentId,
                        packageDigest,
                        defaultComponentSettings
                    ) => ComponentConfigClient.adoptComponentPackage(
                        componentId,
                        packageDigest,
                        defaultComponentSettings
                    )
                    onComponentAddToBarRequested: (
                        componentId,
                        packageDigest,
                        defaultComponentSettings
                    ) => ComponentConfigClient.addComponentToBar(
                        componentId,
                        packageDigest,
                        defaultComponentSettings
                    )
                    onComponentRetryRequested: (
                        componentId,
                        packageDigest
                    ) => ComponentRuntimeClient.retryComponent(
                        componentId,
                        packageDigest
                    )
                }
            }
        }
    }

    Connections {
        target: ComponentManagerClient

        function onPackageRemoved(componentId) {
            componentsPage.packageRemovalCompleted(componentId);
        }
    }
}
