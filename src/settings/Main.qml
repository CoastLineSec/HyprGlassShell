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
    readonly property var sidebarNavigationFlickable: sidebarNavigationScroll.contentItem
    property string currentPage: "bar"
    property string hyprlandSection: "overview"
    property string hyprlandCategory: "appearance"
    readonly property string appearanceDraftNavigationState: appearancePage.externalChangeWhileEditing ? "conflict" : appearancePage.draftDirty ? "dirty" : "clean"
    readonly property string inputDraftNavigationState: inputPage.externalChangeWhileEditing ? "conflict" : inputPage.draftDirty ? "dirty" : "clean"
    readonly property string windowsDraftNavigationState: windowsLayoutPage.externalChangeWhileEditing ? "conflict" : windowsLayoutPage.draftDirty ? "dirty" : "clean"
    readonly property string workspacesDraftNavigationState: workspacesPage.externalChangeWhileEditing ? "conflict" : workspacesPage.draftDirty ? "dirty" : "clean"
    readonly property string rulesDraftNavigationState: rulesPage.externalChangeWhileEditing ? "conflict" : rulesPage.draftDirty ? "dirty" : "clean"
    readonly property string advancedDraftNavigationState: advancedPage.externalChangeWhileEditing ? "conflict" : advancedPage.draftDirty ? "dirty" : "clean"
    readonly property bool sharedCompositorMutationBusy: ConfigClient.busy || CompositorClient.sharedBorderSyncState === "pending" || CompositorClient.sharedSpacingSyncState === "pending"
    readonly property bool sharedBorderCompositorRevisionVerified: /^(0|[1-9][0-9]*)$/.test(ConfigClient.revisionToken) && /^(0|[1-9][0-9]*)$/.test(CompositorClient.sharedBorderSourceRevisionToken) && ConfigClient.revisionToken === CompositorClient.sharedBorderSourceRevisionToken
    readonly property bool sharedSpacingCompositorRevisionVerified: /^(0|[1-9][0-9]*)$/.test(ConfigClient.revisionToken) && /^(0|[1-9][0-9]*)$/.test(CompositorClient.sharedSpacingSourceRevisionToken) && ConfigClient.revisionToken === CompositorClient.sharedSpacingSourceRevisionToken
    readonly property bool sharedCompositorRevisionVerified: root.sharedBorderCompositorRevisionVerified && root.sharedSpacingCompositorRevisionVerified
    readonly property bool sharedBorderCompositorApplySettled: (!ConfigClient.syncHyprlandWindowBorders && CompositorClient.sharedBorderSyncState === "override") || (ConfigClient.syncHyprlandWindowBorders && (CompositorClient.sharedBorderSyncState === "saved" || CompositorClient.sharedBorderSyncState === "current"))
    readonly property bool sharedSpacingCompositorApplySettled: CompositorClient.sharedSpacingSyncState === "saved" || (!ConfigClient.syncHyprlandWindowSpacing && CompositorClient.sharedSpacingSyncState === "override") || (ConfigClient.syncHyprlandWindowSpacing && CompositorClient.sharedSpacingSyncState === "current")
    readonly property bool sharedCompositorApplySettled: root.sharedBorderCompositorApplySettled && root.sharedSpacingCompositorApplySettled
    readonly property bool sharedCompositorApplySafe: ConfigClient.available && CompositorClient.available && !root.sharedCompositorMutationBusy && root.sharedCompositorRevisionVerified && root.sharedCompositorApplySettled
    readonly property string workspaceComponentId: "io.github.coastlinesec.hyprshelld.workspace-switcher"
    readonly property string workspaceInstanceId: "7b4e2329-4320-4e15-894d-218fa690d782"
    readonly property var workspaceDefaults: ({
            showIdentifiers: true,
            showNames: false,
            showApplications: false,
            maximumApplications: 3,
            occupiedOnly: false,
            scrollMode: "disabled"
        })
    readonly property var workspaceInstanceState: workspaceSettingsFromSnapshot(ComponentConfigClient.snapshot)
    readonly property var workspaceComponentState: workspaceComponentStateFromServices(ComponentManagerClient.available, ComponentManagerClient.catalogDigest, ComponentManagerClient.components, ComponentConfigClient.available, ComponentConfigClient.catalogAvailable, ComponentConfigClient.catalogDigest, ComponentConfigClient.snapshot)
    readonly property int failedComponentCount: shellHealthWarning.failedComponentCount
    readonly property string desktopStatusText: {
        if (CoordinatorClient.available) {
            if (CoordinatorClient.healthy)
                return qsTr("Desktop ready");
            return failedComponentCount === 1 ? qsTr("1 component failed") : qsTr("%1 components failed").arg(failedComponentCount);
        }
        if (shellRuntimeStatus.busy && !shellRuntimeStatus.available)
            return qsTr("Checking services…");
        if (shellRuntimeStatus.available) {
            if (failedComponentCount === 1)
                return qsTr("1 component failed");
            if (failedComponentCount > 1)
                return qsTr("%1 components failed").arg(failedComponentCount);
            return shellRuntimeStatus.targetState === "inactive" ? qsTr("Desktop services stopped") : qsTr("Health service unavailable");
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

    function appearanceCompositorError(operation) {
        return operation === "appearance-apply" || operation === "compositor-apply" || operation === "recover";
    }

    function inputCompositorError(operation) {
        return operation === "input-apply" || operation === "compositor-apply" || operation === "recover";
    }

    function windowsCompositorError(operation) {
        return operation === "windows-apply" || operation === "compositor-apply" || operation === "recover";
    }

    function workspacesCompositorError(operation) {
        return operation === "workspaces-apply" || operation === "compositor-apply" || operation === "recover";
    }

    function rulesCompositorError(operation) {
        return operation === "rules-apply" || operation === "compositor-apply" || operation === "recover";
    }

    function advancedCompositorError(operation) {
        return operation === "advanced-apply" || operation === "compositor-apply" || operation === "recover";
    }

    function hyprlandOptionsCompositorError(operation) {
        return operation === "options-save" || operation === "options-apply" || operation === "compositor-apply" || operation === "recover";
    }

    function bindingsCompositorError(operation) {
        return operation === "bindings-save" || operation === "bindings-apply" || operation === "compositor-apply" || operation === "recover";
    }

    function inputDevicesCompositorError(operation) {
        return operation === "input-devices-save" || operation === "input-devices-apply" || operation === "compositor-apply" || operation === "recover";
    }

    function environmentCompositorError(operation) {
        return operation === "environment-save" || operation === "environment-apply" || operation === "compositor-apply" || operation === "recover";
    }

    function permissionsCompositorError(operation) {
        return operation === "permissions-save" || operation === "permissions-apply" || operation === "compositor-apply" || operation === "recover";
    }

    function openHyprlandSection(section) {
        root.hyprlandSection = section;
        root.currentPage = "hyprland";
    }

    function openHyprlandSurface(pageId) {
        if (["displays", "workspaces", "rules"].includes(pageId)) {
            root.currentPage = pageId;
            return;
        }
        if (pageId === "input-devices") {
            root.openHyprlandSection("devices");
            return;
        }
        if (["bindings", "environment", "permissions"].includes(pageId)) {
            root.openHyprlandSection(pageId);
            return;
        }
        if (["appearance", "input", "windows", "advanced"].includes(pageId))
            root.currentPage = pageId;
    }

    function displaysCompositorError(operation) {
        return operation === "adopt" || operation === "display-preview" || operation === "display-confirm" || operation === "display-revert" || operation === "display-refresh" || operation === "compositor-apply";
    }

    function navigationItem(page) {
        if (page === "bar")
            return barNavigationItem;
        if (page === "hyprland") {
            return root.hyprlandSection === "bindings" ? keyboardShortcutsNavigationItem : hyprlandNavigationItem;
        }
        if (page === "appearance")
            return appearanceNavigationItem;
        if (page === "input")
            return inputNavigationItem;
        if (page === "displays")
            return displaysNavigationItem;
        if (page === "windows")
            return windowsNavigationItem;
        if (page === "workspaces")
            return workspacesNavigationItem;
        if (page === "keyboard-shortcuts")
            return keyboardShortcutsNavigationItem;
        if (page === "rules")
            return rulesNavigationItem;
        if (page === "advanced")
            return advancedNavigationItem;
        if (page === "components")
            return componentsNavigationItem;
        return null;
    }

    function revealNavigationItem(page) {
        const item = root.navigationItem(page);
        const flickable = root.sidebarNavigationFlickable;
        if (!item || !flickable || item.height <= 0)
            return;
        const top = item.y;
        const bottom = top + item.height;
        if (top < flickable.contentY) {
            flickable.contentY = top;
        } else if (bottom > flickable.contentY + flickable.height) {
            flickable.contentY = Math.max(0, bottom - flickable.height);
        }
    }

    onCurrentPageChanged: Qt.callLater(() => root.revealNavigationItem(root.currentPage))
    onHyprlandSectionChanged: Qt.callLater(() => root.revealNavigationItem(root.currentPage))

    function appearanceValuesWithSharedVisual(values) {
        let resolved = null;
        try {
            resolved = JSON.parse(JSON.stringify(values));
        } catch (error) {
            return values;
        }
        if (!resolved || typeof resolved !== "object" || Array.isArray(resolved)) {
            return values;
        }
        if (ConfigClient.syncHyprlandWindowBorders) {
            resolved["hyprland.general.border_size"] = ConfigClient.shellBorderEnabled ? ConfigClient.shellBorderWidth : 0;
            resolved["hyprland.decoration.rounding"] = ConfigClient.shellBorderRadius;
        }
        if (ConfigClient.syncHyprlandWindowSpacing) {
            const inner = ConfigClient.shellInnerSpacing;
            const outer = ConfigClient.shellOuterSpacing;
            resolved["hyprland.general.gaps_in"] = [inner, inner, inner, inner];
            resolved["hyprland.general.gaps_out"] = [0, outer, outer, outer];
        }
        return resolved;
    }

    function appearanceValuesWithSharedBorder(values) {
        return root.appearanceValuesWithSharedVisual(values);
    }

    function isWorkspaceSettings(settings) {
        if (!settings || typeof settings !== "object" || Array.isArray(settings)) {
            return false;
        }
        if (typeof settings.showIdentifiers !== "boolean" || typeof settings.showNames !== "boolean" || typeof settings.showApplications !== "boolean" || typeof settings.occupiedOnly !== "boolean") {
            return false;
        }
        if (typeof settings.maximumApplications !== "number" || Math.floor(settings.maximumApplications) !== settings.maximumApplications || settings.maximumApplications < 1 || settings.maximumApplications > 5) {
            return false;
        }
        return ["disabled", "normal", "reversed"].includes(settings.scrollMode);
    }

    function workspaceSettingsFromSnapshot(snapshot) {
        const fallback = {
            valid: false,
            enabled: false,
            showIdentifiers: root.workspaceDefaults.showIdentifiers,
            showNames: root.workspaceDefaults.showNames,
            showApplications: root.workspaceDefaults.showApplications,
            maximumApplications: root.workspaceDefaults.maximumApplications,
            occupiedOnly: root.workspaceDefaults.occupiedOnly,
            scrollMode: root.workspaceDefaults.scrollMode
        };
        if (!snapshot || typeof snapshot !== "object" || Array.isArray(snapshot) || !snapshot.instances || typeof snapshot.instances !== "object" || Array.isArray(snapshot.instances)) {
            return fallback;
        }
        const instance = snapshot.instances[root.workspaceInstanceId];
        if (!instance || typeof instance !== "object" || Array.isArray(instance) || instance.componentId !== root.workspaceComponentId || typeof instance.enabled !== "boolean" || !root.isWorkspaceSettings(instance.settings)) {
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
            if (component && typeof component === "object" && component.id === componentId) {
                return component;
            }
        }
        return null;
    }

    function workspaceComponentStateFromServices(managerAvailable, managerDigest, components, configAvailable, configCatalogAvailable, configDigest, snapshot) {
        const unavailable = {
            available: false,
            desiredEnabled: false,
            instanceEnabled: false,
            previewEnabled: false,
            packageDigest: ""
        };
        const definition = root.catalogComponent(components, root.workspaceComponentId);
        if (typeof managerDigest !== "string" || managerDigest.length !== 64 || managerDigest !== configDigest || !definition || definition.type !== "bar-widget" || definition.origin !== "system" || typeof definition.packageDigest !== "string" || !/^[0-9a-f]{64}$/.test(definition.packageDigest) || !snapshot || typeof snapshot !== "object" || Array.isArray(snapshot) || !snapshot.components || typeof snapshot.components !== "object" || Array.isArray(snapshot.components)) {
            return unavailable;
        }
        const desired = snapshot.components[root.workspaceComponentId];
        const instance = root.workspaceSettingsFromSnapshot(snapshot);
        if (!desired || typeof desired !== "object" || Array.isArray(desired) || desired.packageDigest !== definition.packageDigest || typeof desired.enabled !== "boolean" || !instance.valid) {
            return unavailable;
        }
        return {
            available: managerAvailable && configAvailable && configCatalogAvailable,
            desiredEnabled: desired.enabled,
            instanceEnabled: instance.enabled,
            previewEnabled: desired.enabled && instance.enabled,
            packageDigest: definition.packageDigest
        };
    }

    function workspaceNaturalSettingsAvailable(state) {
        return state && state.available && (!state.desiredEnabled || state.instanceEnabled);
    }

    function workspaceSnapshotWithSettings(snapshot, showIdentifiers, showNames, showApplications, maximumApplications, occupiedOnly, scrollMode) {
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
        if (!replacement || typeof replacement !== "object" || Array.isArray(replacement) || !replacement.instances || typeof replacement.instances !== "object" || Array.isArray(replacement.instances)) {
            return null;
        }
        const instance = replacement.instances[root.workspaceInstanceId];
        if (!instance || typeof instance !== "object" || Array.isArray(instance) || instance.componentId !== root.workspaceComponentId || !root.isWorkspaceSettings(instance.settings)) {
            return null;
        }
        instance.settings = settings;
        return replacement;
    }

    function replaceWorkspaceSettings(showIdentifiers, showNames, showApplications, maximumApplications, occupiedOnly, scrollMode) {
        const replacement = root.workspaceSnapshotWithSettings(ComponentConfigClient.snapshot, showIdentifiers, showNames, showApplications, maximumApplications, occupiedOnly, scrollMode);
        if (replacement)
            ComponentConfigClient.replaceSnapshot(replacement);
    }

    function resetWorkspaceSettings() {
        root.replaceWorkspaceSettings(root.workspaceDefaults.showIdentifiers, root.workspaceDefaults.showNames, root.workspaceDefaults.showApplications, root.workspaceDefaults.maximumApplications, root.workspaceDefaults.occupiedOnly, root.workspaceDefaults.scrollMode);
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
                spacing: 8

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

                ScrollView {
                    id: sidebarNavigationScroll

                    objectName: "sidebarNavigationScroll"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: sidebarNavigationScroll.availableWidth
                        spacing: 8

                        ItemDelegate {
                            id: barNavigationItem

                            objectName: "barNavigationItem"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.minimumHeight: 44
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
                                color: barNavigationItem.checked ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18) : barNavigationItem.hovered ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06) : "transparent"
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
                            id: hyprlandNavigationItem

                            objectName: "hyprlandNavigationItem"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.minimumHeight: 44
                            checkable: true
                            checked: root.currentPage === "hyprland" && root.hyprlandSection !== "bindings"
                            autoExclusive: true
                            focusPolicy: Qt.StrongFocus
                            leftPadding: 18
                            rightPadding: 12
                            topPadding: 8
                            bottomPadding: 8
                            Accessible.role: Accessible.PageTab
                            Accessible.name: qsTr("Hyprland settings")
                            Accessible.description: qsTr("All reviewed Hyprland 0.56.2 scalar options and structured configuration editors.")
                            Accessible.checked: checked

                            onClicked: {
                                root.hyprlandSection = "overview";
                                root.currentPage = "hyprland";
                            }

                            background: Rectangle {
                                radius: 12
                                color: hyprlandNavigationItem.checked ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18) : hyprlandNavigationItem.hovered ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06) : "transparent"
                                border.width: hyprlandNavigationItem.activeFocus ? 2 : 0
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
                                    visible: hyprlandNavigationItem.checked
                                    color: root.palette.highlight
                                }
                            }

                            contentItem: RowLayout {
                                spacing: 11

                                Item {
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 7
                                        color: Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18)
                                        border.width: 1
                                        border.color: root.palette.highlight

                                        Label {
                                            anchors.centerIn: parent
                                            text: "H"
                                            color: root.palette.highlight
                                            font.pixelSize: 12
                                            font.weight: Font.Bold
                                            Accessible.ignored: true
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Hyprland")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: hyprlandNavigationItem.checked ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        ItemDelegate {
                            id: appearanceNavigationItem

                            objectName: "appearanceNavigationItem"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.minimumHeight: 44
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
                            Accessible.description: root.appearanceDraftNavigationState === "conflict" ? qsTr("Appearance has a preserved draft that conflicts with a newer compositor revision.") : root.appearanceDraftNavigationState === "dirty" ? qsTr("Appearance has unsaved changes.") : ""
                            Accessible.checked: checked

                            onClicked: root.currentPage = "appearance"

                            background: Rectangle {
                                radius: 12
                                color: appearanceNavigationItem.checked ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18) : appearanceNavigationItem.hovered ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06) : "transparent"
                                border.width: appearanceNavigationItem.activeFocus ? 2 : 0
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
                                            color: appearanceNavigationItem.checked ? root.palette.highlight : root.palette.placeholderText
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: qsTr("Appearance")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: appearanceNavigationItem.checked ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }

                                Label {
                                    objectName: "appearanceNavigationBadge"
                                    visible: root.appearanceDraftNavigationState !== "clean"
                                    text: root.appearanceDraftNavigationState === "conflict" ? qsTr("Review") : qsTr("Unsaved")
                                    color: root.appearanceDraftNavigationState === "conflict" ? "#ffb8c3" : root.palette.highlightedText
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    leftPadding: 6
                                    rightPadding: 6
                                    topPadding: 3
                                    bottomPadding: 3
                                    Accessible.ignored: true

                                    background: Rectangle {
                                        radius: 7
                                        color: root.appearanceDraftNavigationState === "conflict" ? "#6b2a36" : root.palette.highlight
                                    }
                                }
                            }
                        }

                        ItemDelegate {
                            id: inputNavigationItem

                            objectName: "inputNavigationItem"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.minimumHeight: 44
                            checkable: true
                            checked: root.currentPage === "input"
                            autoExclusive: true
                            focusPolicy: Qt.StrongFocus
                            leftPadding: 18
                            rightPadding: 12
                            topPadding: 8
                            bottomPadding: 8
                            Accessible.role: Accessible.PageTab
                            Accessible.name: qsTr("Input settings")
                            Accessible.description: root.inputDraftNavigationState === "conflict" ? qsTr("Input has a preserved draft that conflicts with a newer compositor revision.") : root.inputDraftNavigationState === "dirty" ? qsTr("Input has unsaved changes.") : ""
                            Accessible.checked: checked

                            onClicked: root.currentPage = "input"

                            background: Rectangle {
                                radius: 12
                                color: inputNavigationItem.checked ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18) : inputNavigationItem.hovered ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06) : "transparent"
                                border.width: inputNavigationItem.activeFocus ? 2 : 0
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
                                    visible: inputNavigationItem.checked
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
                                        width: 21
                                        height: 15
                                        radius: 3
                                        color: "transparent"
                                        border.width: 2
                                        border.color: root.palette.text

                                        Row {
                                            anchors {
                                                horizontalCenter: parent.horizontalCenter
                                                top: parent.top
                                                topMargin: 4
                                            }
                                            spacing: 2

                                            Repeater {
                                                model: 4

                                                Rectangle {
                                                    required property int index
                                                    width: 2
                                                    height: 2
                                                    radius: 1
                                                    color: index === 0 && inputNavigationItem.checked ? root.palette.highlight : root.palette.placeholderText
                                                }
                                            }
                                        }

                                        Rectangle {
                                            anchors {
                                                horizontalCenter: parent.horizontalCenter
                                                bottom: parent.bottom
                                                bottomMargin: 3
                                            }
                                            width: 11
                                            height: 2
                                            radius: 1
                                            color: root.palette.placeholderText
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: qsTr("Input")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: inputNavigationItem.checked ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }

                                Label {
                                    objectName: "inputNavigationBadge"
                                    visible: root.inputDraftNavigationState !== "clean"
                                    text: root.inputDraftNavigationState === "conflict" ? qsTr("Review") : qsTr("Unsaved")
                                    color: root.inputDraftNavigationState === "conflict" ? "#ffb8c3" : root.palette.highlightedText
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    leftPadding: 6
                                    rightPadding: 6
                                    topPadding: 3
                                    bottomPadding: 3
                                    Accessible.ignored: true

                                    background: Rectangle {
                                        radius: 7
                                        color: root.inputDraftNavigationState === "conflict" ? "#6b2a36" : root.palette.highlight
                                    }
                                }
                            }
                        }

                        ItemDelegate {
                            id: displaysNavigationItem

                            objectName: "displaysNavigationItem"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.minimumHeight: 44
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
                                color: displaysNavigationItem.checked ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18) : displaysNavigationItem.hovered ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06) : "transparent"
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
                                        border.color: displaysNavigationItem.checked ? root.palette.highlight : root.palette.mid

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
                                    font.weight: displaysNavigationItem.checked ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        ItemDelegate {
                            id: windowsNavigationItem

                            objectName: "windowsNavigationItem"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.minimumHeight: 44
                            checkable: true
                            checked: root.currentPage === "windows"
                            autoExclusive: true
                            focusPolicy: Qt.StrongFocus
                            leftPadding: 18
                            rightPadding: 12
                            topPadding: 8
                            bottomPadding: 8
                            Accessible.role: Accessible.PageTab
                            Accessible.name: qsTr("Windows and layout settings")
                            Accessible.description: root.windowsDraftNavigationState === "conflict" ? qsTr("Windows & Layout has a preserved draft that conflicts with a newer compositor revision.") : root.windowsDraftNavigationState === "dirty" ? qsTr("Windows & Layout has unsaved changes.") : ""
                            Accessible.checked: checked

                            onClicked: root.currentPage = "windows"

                            background: Rectangle {
                                radius: 12
                                color: windowsNavigationItem.checked ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18) : windowsNavigationItem.hovered ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06) : "transparent"
                                border.width: windowsNavigationItem.activeFocus ? 2 : 0
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
                                    visible: windowsNavigationItem.checked
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
                                            top: parent.top
                                        }
                                        width: 13
                                        height: 22
                                        radius: 3
                                        color: windowsNavigationItem.checked ? root.palette.highlight : root.palette.text
                                    }

                                    Rectangle {
                                        anchors {
                                            right: parent.right
                                            top: parent.top
                                        }
                                        width: 7
                                        height: 10
                                        radius: 2
                                        color: root.palette.text
                                    }

                                    Rectangle {
                                        anchors {
                                            right: parent.right
                                            bottom: parent.bottom
                                        }
                                        width: 7
                                        height: 10
                                        radius: 2
                                        color: root.palette.placeholderText
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: qsTr("Windows & Layout")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: windowsNavigationItem.checked ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }

                                Label {
                                    objectName: "windowsNavigationBadge"
                                    visible: root.windowsDraftNavigationState !== "clean"
                                    text: root.windowsDraftNavigationState === "conflict" ? qsTr("Review") : qsTr("Unsaved")
                                    color: root.windowsDraftNavigationState === "conflict" ? "#ffb8c3" : root.palette.highlightedText
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    leftPadding: 6
                                    rightPadding: 6
                                    topPadding: 3
                                    bottomPadding: 3
                                    Accessible.ignored: true

                                    background: Rectangle {
                                        radius: 7
                                        color: root.windowsDraftNavigationState === "conflict" ? "#6b2a36" : root.palette.highlight
                                    }
                                }
                            }
                        }

                        ItemDelegate {
                            id: workspacesNavigationItem

                            objectName: "workspacesNavigationItem"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.minimumHeight: 44
                            checkable: true
                            checked: root.currentPage === "workspaces"
                            autoExclusive: true
                            focusPolicy: Qt.StrongFocus
                            leftPadding: 18
                            rightPadding: 12
                            topPadding: 8
                            bottomPadding: 8
                            Accessible.role: Accessible.PageTab
                            Accessible.name: qsTr("Workspace settings")
                            Accessible.description: root.workspacesDraftNavigationState === "conflict" ? qsTr("Workspaces has a preserved draft that conflicts with a newer compositor revision.") : root.workspacesDraftNavigationState === "dirty" ? qsTr("Workspaces has unsaved changes.") : ""
                            Accessible.checked: checked

                            onClicked: root.currentPage = "workspaces"

                            background: Rectangle {
                                radius: 12
                                color: workspacesNavigationItem.checked ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18) : workspacesNavigationItem.hovered ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06) : "transparent"
                                border.width: workspacesNavigationItem.activeFocus ? 2 : 0
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
                                    visible: workspacesNavigationItem.checked
                                    color: root.palette.highlight
                                }
                            }

                            contentItem: RowLayout {
                                spacing: 11

                                Grid {
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22
                                    rows: 2
                                    columns: 2
                                    spacing: 3

                                    Repeater {
                                        model: 4

                                        Rectangle {
                                            required property int index
                                            width: 9
                                            height: 9
                                            radius: 3
                                            color: index === 0 && workspacesNavigationItem.checked ? root.palette.highlight : index === 0 ? root.palette.text : root.palette.placeholderText
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: qsTr("Workspaces")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: workspacesNavigationItem.checked ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }

                                Label {
                                    objectName: "workspacesNavigationBadge"
                                    visible: root.workspacesDraftNavigationState !== "clean"
                                    text: root.workspacesDraftNavigationState === "conflict" ? qsTr("Review") : qsTr("Unsaved")
                                    color: root.workspacesDraftNavigationState === "conflict" ? "#ffb8c3" : root.palette.highlightedText
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    leftPadding: 6
                                    rightPadding: 6
                                    topPadding: 3
                                    bottomPadding: 3
                                    Accessible.ignored: true

                                    background: Rectangle {
                                        radius: 7
                                        color: root.workspacesDraftNavigationState === "conflict" ? "#6b2a36" : root.palette.highlight
                                    }
                                }
                            }
                        }

                        ItemDelegate {
                            id: keyboardShortcutsNavigationItem

                            objectName: "keyboardShortcutsNavigationItem"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.minimumHeight: 44
                            checkable: true
                            checked: root.currentPage === "hyprland" && root.hyprlandSection === "bindings"
                            autoExclusive: true
                            focusPolicy: Qt.StrongFocus
                            leftPadding: 18
                            rightPadding: 12
                            topPadding: 8
                            bottomPadding: 8
                            Accessible.role: Accessible.PageTab
                            Accessible.name: qsTr("Shortcut and submap settings")
                            Accessible.description: qsTr("Edits active reviewed Hyprland bindings and modal submaps in the managed Lua configuration.")
                            Accessible.checked: checked

                            onClicked: root.openHyprlandSection("bindings")

                            background: Rectangle {
                                radius: 12
                                color: keyboardShortcutsNavigationItem.checked ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18) : keyboardShortcutsNavigationItem.hovered ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06) : "transparent"
                                border.width: keyboardShortcutsNavigationItem.activeFocus ? 2 : 0
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
                                    visible: keyboardShortcutsNavigationItem.checked
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
                                        width: 22
                                        height: 15
                                        radius: 3
                                        color: "transparent"
                                        border.width: 2
                                        border.color: keyboardShortcutsNavigationItem.checked ? root.palette.highlight : root.palette.text

                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 2

                                            Repeater {
                                                model: 4

                                                Rectangle {
                                                    required property int index
                                                    width: index === 3 ? 5 : 3
                                                    height: 3
                                                    radius: 1
                                                    color: root.palette.placeholderText
                                                }
                                            }
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: qsTr("Shortcuts & Submaps")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: keyboardShortcutsNavigationItem.checked ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        ItemDelegate {
                            id: rulesNavigationItem

                            objectName: "rulesNavigationItem"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.minimumHeight: 44
                            checkable: true
                            checked: root.currentPage === "rules"
                            autoExclusive: true
                            focusPolicy: Qt.StrongFocus
                            leftPadding: 18
                            rightPadding: 12
                            topPadding: 8
                            bottomPadding: 8
                            Accessible.role: Accessible.PageTab
                            Accessible.name: qsTr("Window and Layer Rules settings")
                            Accessible.description: root.rulesDraftNavigationState === "conflict" ? qsTr("Rules has a preserved draft that conflicts with a newer compositor revision.") : root.rulesDraftNavigationState === "dirty" ? qsTr("Rules has unsaved changes.") : ""
                            Accessible.checked: checked

                            onClicked: root.currentPage = "rules"

                            background: Rectangle {
                                radius: 12
                                color: rulesNavigationItem.checked ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18) : rulesNavigationItem.hovered ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06) : "transparent"
                                border.width: rulesNavigationItem.activeFocus ? 2 : 0
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
                                    visible: rulesNavigationItem.checked
                                    color: root.palette.highlight
                                }
                            }

                            contentItem: RowLayout {
                                spacing: 11

                                Item {
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 4

                                        Repeater {
                                            model: 3

                                            Row {
                                                id: rulesIconRow

                                                required property int index
                                                spacing: 3

                                                Rectangle {
                                                    width: 4
                                                    height: 4
                                                    radius: 2
                                                    color: rulesIconRow.index === 0 && rulesNavigationItem.checked ? root.palette.highlight : root.palette.text
                                                }

                                                Rectangle {
                                                    width: 14
                                                    height: 3
                                                    radius: 2
                                                    color: root.palette.placeholderText
                                                }
                                            }
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: qsTr("Rules")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: rulesNavigationItem.checked ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }

                                Label {
                                    objectName: "rulesNavigationBadge"
                                    visible: root.rulesDraftNavigationState !== "clean"
                                    text: root.rulesDraftNavigationState === "conflict" ? qsTr("Review") : qsTr("Unsaved")
                                    color: root.rulesDraftNavigationState === "conflict" ? "#ffb8c3" : root.palette.highlightedText
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    leftPadding: 6
                                    rightPadding: 6
                                    topPadding: 3
                                    bottomPadding: 3
                                    Accessible.ignored: true

                                    background: Rectangle {
                                        radius: 7
                                        color: root.rulesDraftNavigationState === "conflict" ? "#6b2a36" : root.palette.highlight
                                    }
                                }
                            }
                        }

                        ItemDelegate {
                            id: advancedNavigationItem

                            objectName: "advancedNavigationItem"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.minimumHeight: 44
                            checkable: true
                            checked: root.currentPage === "advanced"
                            autoExclusive: true
                            focusPolicy: Qt.StrongFocus
                            leftPadding: 18
                            rightPadding: 12
                            topPadding: 8
                            bottomPadding: 8
                            Accessible.role: Accessible.PageTab
                            Accessible.name: qsTr("Advanced settings")
                            Accessible.description: root.advancedDraftNavigationState === "conflict" ? qsTr("Advanced has a preserved draft that conflicts with a newer compositor revision.") : root.advancedDraftNavigationState === "dirty" ? qsTr("Advanced has unsaved changes.") : ""
                            Accessible.checked: checked

                            onClicked: root.currentPage = "advanced"

                            background: Rectangle {
                                radius: 12
                                color: advancedNavigationItem.checked ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18) : advancedNavigationItem.hovered ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06) : "transparent"
                                border.width: advancedNavigationItem.activeFocus ? 2 : 0
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
                                    visible: advancedNavigationItem.checked
                                    color: root.palette.highlight
                                }
                            }

                            contentItem: RowLayout {
                                spacing: 11

                                Item {
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22

                                    Rectangle {
                                        x: 1
                                        y: 4
                                        width: 20
                                        height: 2
                                        radius: 1
                                        color: root.palette.placeholderText
                                    }

                                    Rectangle {
                                        x: 1
                                        y: 10
                                        width: 20
                                        height: 2
                                        radius: 1
                                        color: root.palette.placeholderText
                                    }

                                    Rectangle {
                                        x: 1
                                        y: 16
                                        width: 20
                                        height: 2
                                        radius: 1
                                        color: root.palette.placeholderText
                                    }

                                    Rectangle {
                                        x: 5
                                        y: 1
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: advancedNavigationItem.checked ? root.palette.highlight : root.palette.text
                                    }

                                    Rectangle {
                                        x: 12
                                        y: 7
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: advancedNavigationItem.checked ? root.palette.highlight : root.palette.text
                                    }

                                    Rectangle {
                                        x: 3
                                        y: 13
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: advancedNavigationItem.checked ? root.palette.highlight : root.palette.text
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: qsTr("Advanced")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: advancedNavigationItem.checked ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }

                                Label {
                                    objectName: "advancedNavigationBadge"
                                    visible: root.advancedDraftNavigationState !== "clean"
                                    text: root.advancedDraftNavigationState === "conflict" ? qsTr("Review") : qsTr("Unsaved")
                                    color: root.advancedDraftNavigationState === "conflict" ? "#ffb8c3" : root.palette.highlightedText
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    leftPadding: 6
                                    rightPadding: 6
                                    topPadding: 3
                                    bottomPadding: 3
                                    Accessible.ignored: true

                                    background: Rectangle {
                                        radius: 7
                                        color: root.advancedDraftNavigationState === "conflict" ? "#6b2a36" : root.palette.highlight
                                    }
                                }
                            }
                        }

                        ItemDelegate {
                            id: componentsNavigationItem

                            objectName: "componentsNavigationItem"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Layout.minimumHeight: 44
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
                                color: componentsNavigationItem.checked ? Qt.rgba(root.palette.highlight.r, root.palette.highlight.g, root.palette.highlight.b, 0.18) : componentsNavigationItem.hovered ? Qt.rgba(root.palette.text.r, root.palette.text.g, root.palette.text.b, 0.06) : "transparent"
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
                                            color: index === 0 ? root.palette.highlight : root.palette.text
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Components")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: componentsNavigationItem.checked ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
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
                Layout.preferredHeight: shellHealthWarning.warningVisible ? shellHealthWarning.implicitHeight + 48 : 0
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
                    componentManagerState: shellRuntimeStatus.componentManagerState
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
                currentIndex: root.currentPage === "appearance" ? 1 : root.currentPage === "input" ? 2 : root.currentPage === "displays" ? 3 : root.currentPage === "windows" ? 4 : root.currentPage === "workspaces" ? 5 : root.currentPage === "keyboard-shortcuts" ? 6 : root.currentPage === "rules" ? 7 : root.currentPage === "advanced" ? 8 : root.currentPage === "components" ? 9 : root.currentPage === "hyprland" ? 10 : 0

                BarSettingsPage {
                    barHeight: ConfigClient.barHeight
                    minimumBarHeight: ConfigClient.minimumBarHeight
                    maximumBarHeight: ConfigClient.maximumBarHeight
                    defaultBarHeight: ConfigClient.defaultBarHeight
                    shellBorderEnabled: ConfigClient.shellBorderEnabled
                    shellBorderWidth: ConfigClient.shellBorderWidth
                    shellBorderRadius: ConfigClient.shellBorderRadius
                    syncHyprlandWindowBorders: ConfigClient.syncHyprlandWindowBorders
                    shellInnerSpacing: ConfigClient.shellInnerSpacing
                    shellOuterSpacing: ConfigClient.shellOuterSpacing
                    syncHyprlandWindowSpacing: ConfigClient.syncHyprlandWindowSpacing
                    workspaceShowIdentifiers: root.workspaceInstanceState.showIdentifiers
                    workspaceShowNames: root.workspaceInstanceState.showNames
                    workspaceShowApplications: root.workspaceInstanceState.showApplications
                    workspaceMaximumApplications: root.workspaceInstanceState.maximumApplications
                    workspaceOccupiedOnly: root.workspaceInstanceState.occupiedOnly
                    workspaceScrollMode: root.workspaceInstanceState.scrollMode
                    workspaceFeatureAvailable: root.workspaceNaturalSettingsAvailable(root.workspaceComponentState)
                    workspaceFeatureEnabled: root.workspaceComponentState.desiredEnabled
                    workspacePreviewEnabled: root.workspaceComponentState.previewEnabled
                    coreServiceAvailable: ConfigClient.available
                    coreBusy: ConfigClient.busy
                    coreErrorText: ConfigClient.lastErrorMessage
                    coreRecoveryState: ConfigClient.recoveryState
                    componentServiceAvailable: ComponentConfigClient.available
                    componentCatalogAvailable: ComponentConfigClient.catalogAvailable && ComponentManagerClient.available && ComponentConfigClient.catalogDigest === ComponentManagerClient.catalogDigest
                    componentWritable: ["normal", "recovered", "defaulted"].includes(ComponentConfigClient.loadState)
                    workspaceInstanceAvailable: root.workspaceComponentState.available
                    componentBusy: ComponentConfigClient.busy || ComponentManagerClient.busy
                    componentErrorText: ComponentConfigClient.lastErrorComponentId.length === 0 ? ComponentConfigClient.lastErrorMessage : ""
                    componentRecoveryState: ComponentConfigClient.loadState
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onBarHeightRequested: height => ConfigClient.setBarHeight(height)
                    onResetBarHeightRequested: ConfigClient.resetBarHeight()
                    onSharedBorderRequested: (enabled, width, radius, syncHyprlandWindowBorders) => ConfigClient.setSharedBorder(enabled, width, radius, syncHyprlandWindowBorders)
                    onResetSharedBorderRequested: ConfigClient.resetSharedBorder()
                    onSharedSpacingRequested: (innerSpacing, outerSpacing, syncHyprlandWindowSpacing) => ConfigClient.setSharedSpacing(innerSpacing, outerSpacing, syncHyprlandWindowSpacing)
                    onResetSharedSpacingRequested: ConfigClient.resetSharedSpacing()
                    onWorkspaceSwitcherRequested: (showIdentifiers, showNames, showApplications, maximumApplications, occupiedOnly, scrollMode) => root.replaceWorkspaceSettings(showIdentifiers, showNames, showApplications, maximumApplications, occupiedOnly, scrollMode)
                    onResetWorkspaceSwitcherRequested: root.resetWorkspaceSettings()
                }

                AppearancePage {
                    id: appearancePage

                    objectName: "appearancePage"
                    serviceAvailable: CompositorClient.available
                    writable: CompositorClient.writable
                    catalogAvailable: CompositorClient.catalogAvailable
                    appearanceAvailable: CompositorClient.appearanceAvailable
                    appearanceProjectionAvailable: CompositorClient.appearanceProjectionAvailable
                    appearanceAnimationProjectionAvailable: CompositorClient.appearanceAnimationProjectionAvailable
                    busy: CompositorClient.busy
                    busyOperation: CompositorClient.busyOperation
                    appearanceOptions: CompositorClient.appearanceOptions
                    appearanceValues: root.appearanceValuesWithSharedVisual(CompositorClient.appearanceValues)
                    appearanceCurves: CompositorClient.appearanceCurves
                    appearanceAnimations: CompositorClient.appearanceAnimations
                    sharedBorderAvailable: ConfigClient.available
                    sharedBorderBusy: ConfigClient.busy
                    windowBorderSynced: ConfigClient.syncHyprlandWindowBorders
                    sharedBorderSyncState: CompositorClient.sharedBorderSyncState
                    sharedBorderSyncError: CompositorClient.sharedBorderSyncError
                    sharedBorderClientError: ConfigClient.lastErrorMessage
                    sharedBorderConfigRevisionToken: ConfigClient.revisionToken
                    sharedBorderVerifiedRevisionToken: CompositorClient.sharedBorderSourceRevisionToken
                    sharedSpacingAvailable: ConfigClient.available
                    sharedSpacingBusy: ConfigClient.busy
                    windowSpacingSynced: ConfigClient.syncHyprlandWindowSpacing
                    sharedSpacingSyncState: CompositorClient.sharedSpacingSyncState
                    sharedSpacingSyncError: CompositorClient.sharedSpacingSyncError
                    sharedSpacingClientError: ConfigClient.lastErrorMessage
                    sharedSpacingConfigRevisionToken: ConfigClient.revisionToken
                    sharedSpacingVerifiedRevisionToken: CompositorClient.sharedSpacingSourceRevisionToken
                    revisionToken: CompositorClient.revisionToken
                    appliedRevision: CompositorClient.appliedRevision
                    loadState: CompositorClient.loadState
                    managementState: CompositorClient.managementState
                    applyState: CompositorClient.applyState
                    requiredActivation: CompositorClient.requiredActivation
                    confirmationState: CompositorClient.displayConfirmationState
                    appearanceErrorName: CompositorClient.appearanceErrorName
                    appearanceErrorMessage: CompositorClient.appearanceErrorMessage
                    errorName: root.appearanceCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorName : ""
                    errorMessage: root.appearanceCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                    retryApplyAvailable: CompositorClient.retryApplyAvailable
                    recoveryAvailable: CompositorClient.recoveryAvailable
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onRefreshRequested: CompositorClient.refresh()
                    onOpenDisplaysRequested: root.currentPage = "displays"
                    onSaveRequested: (values, curves, animations) => CompositorClient.saveAppearance(values, curves, animations)
                    onRetryApplyRequested: CompositorClient.retryApply()
                    onRecoveryRequested: CompositorClient.recoverConfiguration()
                    onWindowBorderSyncRequested: sync => ConfigClient.setSharedBorder(ConfigClient.shellBorderEnabled, ConfigClient.shellBorderWidth, ConfigClient.shellBorderRadius, sync)
                    onRetrySharedBorderSyncRequested: CompositorClient.retrySharedBorderSync()
                    onWindowSpacingSyncRequested: sync => ConfigClient.setSharedSpacing(ConfigClient.shellInnerSpacing, ConfigClient.shellOuterSpacing, sync)
                    onRetrySharedSpacingSyncRequested: CompositorClient.retrySharedSpacingSync()
                }

                InputPage {
                    id: inputPage

                    objectName: "inputPage"
                    serviceAvailable: CompositorClient.available
                    writable: CompositorClient.writable
                    catalogAvailable: CompositorClient.catalogAvailable
                    inputAvailable: CompositorClient.inputAvailable
                    inputProjectionAvailable: CompositorClient.inputProjectionAvailable
                    busy: CompositorClient.busy
                    busyOperation: CompositorClient.busyOperation
                    inputOptions: CompositorClient.inputOptions
                    inputValues: CompositorClient.inputValues
                    inputGesturesProjectionAvailable: CompositorClient.inputGesturesProjectionAvailable
                    inputGestures: CompositorClient.inputGestures
                    inputGestureCompatibility: CompositorClient.inputGestureCompatibility
                    inputGestureActions: CompositorClient.inputGestureActions
                    revisionToken: CompositorClient.revisionToken
                    appliedRevision: CompositorClient.appliedRevision
                    loadState: CompositorClient.loadState
                    managementState: CompositorClient.managementState
                    applyState: CompositorClient.applyState
                    requiredActivation: CompositorClient.requiredActivation
                    confirmationState: CompositorClient.displayConfirmationState
                    inputErrorName: CompositorClient.inputErrorName
                    inputErrorMessage: CompositorClient.inputErrorMessage
                    inputDeviceDiscoveryAvailable: CompositorClient.inputDeviceDiscoveryAvailable
                    inputDeviceDiscoveryBusy: CompositorClient.inputDeviceDiscoveryBusy
                    connectedInputDevices: CompositorClient.connectedInputDevices
                    inputDevicesObservedAtMs: CompositorClient.inputDevicesObservedAtMs
                    inputDeviceInventoryDigest: CompositorClient.inputDeviceInventoryDigest
                    inputDeviceUnaddressableCounts: CompositorClient.inputDeviceUnaddressableCounts
                    inputDeviceDiscoveryErrorName: CompositorClient.inputDeviceDiscoveryErrorName
                    inputDeviceDiscoveryErrorMessage: CompositorClient.inputDeviceDiscoveryErrorMessage
                    inputDeviceProjectionAvailable: CompositorClient.inputDeviceProjectionAvailable
                    savedInputDevices: CompositorClient.savedInputDevices
                    otherSavedInputDevices: CompositorClient.otherSavedInputDevices
                    inputDeviceProjectionRevisionToken: CompositorClient.inputDeviceProjectionRevisionToken
                    inputDeviceProjectionInventoryDigest: CompositorClient.inputDeviceProjectionInventoryDigest
                    inputDeviceProjectionErrorName: CompositorClient.inputDeviceProjectionErrorName
                    inputDeviceProjectionErrorMessage: CompositorClient.inputDeviceProjectionErrorMessage
                    sharedErrorName: root.inputCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorName : ""
                    sharedErrorMessage: root.inputCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                    retryApplyAvailable: CompositorClient.retryApplyAvailable
                    recoveryAvailable: CompositorClient.recoveryAvailable
                    sharedMutationBusy: root.sharedCompositorMutationBusy
                    sharedApplySafe: root.sharedCompositorApplySafe
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onRefreshRequested: CompositorClient.refresh()
                    onRefreshConnectedInputDevicesRequested: CompositorClient.refreshConnectedInputDevices()
                    onManageInputDeviceProfilesRequested: root.openHyprlandSection("devices")
                    onOpenDisplaysRequested: root.currentPage = "displays"
                    onSaveRequested: (values, gestures) => CompositorClient.saveInput(values, gestures)
                    onRetryApplyRequested: CompositorClient.retryApply()
                    onRecoveryRequested: CompositorClient.recoverConfiguration()
                }

                DisplaysPage {
                    id: displaysPage

                    objectName: "displaysPage"
                    serviceAvailable: CompositorClient.available && CompositorClient.displayDiscoveryAvailable
                    writable: CompositorClient.writable
                    busy: CompositorClient.busy
                    sharedMutationBusy: root.sharedCompositorMutationBusy
                    sharedApplySafe: root.sharedCompositorApplySafe
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
                    confirmationState: CompositorClient.displayConfirmationState
                    confirmationRevision: CompositorClient.displayConfirmationRevision
                    confirmationDeadlineMs: CompositorClient.displayConfirmationDeadlineMs
                    confirmationGeneration: CompositorClient.displayConfirmationGeneration
                    confirmationOwned: CompositorClient.displayConfirmationOwned
                    errorName: root.displaysCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorName : ""
                    errorMessage: root.displaysCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onRefreshRequested: CompositorClient.refresh()
                    onAdoptionRequested: CompositorClient.adoptManagedConfiguration()
                    onApplyRequested: CompositorClient.applyConfiguration()
                    onPreviewRequested: (outputs, timeoutSeconds) => CompositorClient.previewDisplayConfiguration(outputs, timeoutSeconds)
                    onConfirmRequested: CompositorClient.confirmDisplayConfiguration()
                    onRevertRequested: CompositorClient.revertDisplayConfiguration()
                }

                WindowsLayoutPage {
                    id: windowsLayoutPage

                    objectName: "windowsLayoutPage"
                    serviceAvailable: CompositorClient.available
                    writable: CompositorClient.writable
                    catalogAvailable: CompositorClient.catalogAvailable
                    windowsAvailable: CompositorClient.windowsAvailable
                    windowsProjectionAvailable: CompositorClient.windowsProjectionAvailable
                    busy: CompositorClient.busy
                    busyOperation: CompositorClient.busyOperation
                    windowsOptions: CompositorClient.windowsOptions
                    windowsValues: CompositorClient.windowsValues
                    revisionToken: CompositorClient.revisionToken
                    appliedRevision: CompositorClient.appliedRevision
                    loadState: CompositorClient.loadState
                    managementState: CompositorClient.managementState
                    applyState: CompositorClient.applyState
                    requiredActivation: CompositorClient.requiredActivation
                    confirmationState: CompositorClient.displayConfirmationState
                    windowsErrorName: CompositorClient.windowsErrorName
                    windowsErrorMessage: CompositorClient.windowsErrorMessage
                    sharedErrorName: root.windowsCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorName : ""
                    sharedErrorMessage: root.windowsCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                    retryApplyAvailable: CompositorClient.retryApplyAvailable
                    recoveryAvailable: CompositorClient.recoveryAvailable
                    sharedMutationBusy: root.sharedCompositorMutationBusy
                    sharedApplySafe: root.sharedCompositorApplySafe
                    previewAnimationsEnabled: !appearancePage.projectionInitialized || appearancePage.draftValue(appearancePage.animationsId) === true
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onRefreshRequested: CompositorClient.refresh()
                    onOpenDisplaysRequested: root.currentPage = "displays"
                    onSaveRequested: values => CompositorClient.saveWindows(values)
                    onRetryApplyRequested: CompositorClient.retryApply()
                    onRecoveryRequested: CompositorClient.recoverConfiguration()
                }

                WorkspacesPage {
                    id: workspacesPage

                    objectName: "workspacesPage"
                    serviceAvailable: CompositorClient.available
                    writable: CompositorClient.writable
                    catalogAvailable: CompositorClient.catalogAvailable
                    workspacesAvailable: CompositorClient.workspacesAvailable
                    workspacesProjectionAvailable: CompositorClient.workspacesProjectionAvailable
                    workspaceRulesProjectionAvailable: CompositorClient.workspaceRulesProjectionAvailable
                    busy: CompositorClient.busy
                    busyOperation: CompositorClient.busyOperation
                    workspacesOptions: CompositorClient.workspacesOptions
                    workspacesValues: CompositorClient.workspacesValues
                    workspaceRules: CompositorClient.workspaceRules
                    revisionToken: CompositorClient.revisionToken
                    appliedRevision: CompositorClient.appliedRevision
                    loadState: CompositorClient.loadState
                    managementState: CompositorClient.managementState
                    applyState: CompositorClient.applyState
                    requiredActivation: CompositorClient.requiredActivation
                    confirmationState: CompositorClient.displayConfirmationState
                    workspacesErrorName: CompositorClient.workspacesErrorName
                    workspacesErrorMessage: CompositorClient.workspacesErrorMessage
                    sharedErrorName: root.workspacesCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorName : ""
                    sharedErrorMessage: root.workspacesCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                    retryApplyAvailable: CompositorClient.retryApplyAvailable
                    recoveryAvailable: CompositorClient.recoveryAvailable
                    sharedMutationBusy: root.sharedCompositorMutationBusy
                    sharedApplySafe: root.sharedCompositorApplySafe
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onRefreshRequested: CompositorClient.refresh()
                    onOpenDisplaysRequested: root.currentPage = "displays"
                    onSaveRequested: (values, workspaceRules) => CompositorClient.saveWorkspaces(values, workspaceRules)
                    onRetryApplyRequested: CompositorClient.retryApply()
                    onRecoveryRequested: CompositorClient.recoverConfiguration()
                }

                KeyboardShortcutsPage {
                    id: keyboardShortcutsPage

                    objectName: "keyboardShortcutsPage"
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28
                }

                RulesPage {
                    id: rulesPage

                    objectName: "rulesPage"
                    serviceAvailable: CompositorClient.available
                    writable: CompositorClient.writable
                    catalogAvailable: CompositorClient.catalogAvailable
                    rulesAvailable: CompositorClient.rulesAvailable
                    rulesProjectionAvailable: CompositorClient.rulesProjectionAvailable
                    busy: CompositorClient.busy
                    busyOperation: CompositorClient.busyOperation
                    windowRules: CompositorClient.windowRules
                    layerRules: CompositorClient.layerRules
                    revisionToken: CompositorClient.revisionToken
                    loadState: CompositorClient.loadState
                    managementState: CompositorClient.managementState
                    applyState: CompositorClient.applyState
                    requiredActivation: CompositorClient.requiredActivation
                    confirmationState: CompositorClient.displayConfirmationState
                    rulesErrorName: CompositorClient.rulesErrorName
                    rulesErrorMessage: CompositorClient.rulesErrorMessage
                    sharedErrorName: root.rulesCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorName : ""
                    sharedErrorMessage: root.rulesCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                    retryApplyAvailable: CompositorClient.retryApplyAvailable
                    recoveryAvailable: CompositorClient.recoveryAvailable
                    sharedMutationBusy: root.sharedCompositorMutationBusy
                    sharedApplySafe: root.sharedCompositorApplySafe
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onRefreshRequested: CompositorClient.refresh()
                    onOpenDisplaysRequested: root.currentPage = "displays"
                    onSaveRequested: (windowRules, layerRules) => CompositorClient.saveRules(windowRules, layerRules)
                    onRetryApplyRequested: CompositorClient.retryApply()
                    onRecoveryRequested: CompositorClient.recoverConfiguration()
                }

                AdvancedPage {
                    id: advancedPage

                    objectName: "advancedPage"
                    serviceAvailable: CompositorClient.available
                    writable: CompositorClient.writable
                    catalogAvailable: CompositorClient.catalogAvailable
                    advancedAvailable: CompositorClient.advancedAvailable
                    advancedProjectionAvailable: CompositorClient.advancedProjectionAvailable
                    busy: CompositorClient.busy
                    busyOperation: CompositorClient.busyOperation
                    advancedOptions: CompositorClient.advancedOptions
                    advancedValues: CompositorClient.advancedValues
                    revisionToken: CompositorClient.revisionToken
                    appliedRevision: CompositorClient.appliedRevision
                    loadState: CompositorClient.loadState
                    managementState: CompositorClient.managementState
                    applyState: CompositorClient.applyState
                    requiredActivation: CompositorClient.requiredActivation
                    confirmationState: CompositorClient.displayConfirmationState
                    advancedErrorName: CompositorClient.advancedErrorName
                    advancedErrorMessage: CompositorClient.advancedErrorMessage
                    sharedErrorName: root.advancedCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorName : ""
                    sharedErrorMessage: root.advancedCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                    retryApplyAvailable: CompositorClient.retryApplyAvailable
                    recoveryAvailable: CompositorClient.recoveryAvailable
                    sharedMutationBusy: root.sharedCompositorMutationBusy
                    sharedApplySafe: root.sharedCompositorApplySafe
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onRefreshRequested: CompositorClient.refresh()
                    onOpenDisplaysRequested: root.currentPage = "displays"
                    onSaveRequested: values => CompositorClient.saveAdvanced(values)
                    onRetryApplyRequested: CompositorClient.retryApply()
                    onRecoveryRequested: CompositorClient.recoverConfiguration()
                }

                ComponentsPage {
                    id: componentsPage
                    objectName: "componentsPage"
                    managerAvailable: ComponentManagerClient.available
                    managerBusy: ComponentManagerClient.busy
                    managerCatalogDigest: ComponentManagerClient.catalogDigest
                    components: ComponentManagerClient.components
                    managerError: ComponentManagerClient.lastError
                    inspectionBusy: ComponentManagerClient.inspectionBusy
                    packageOperationBusy: ComponentManagerClient.packageOperationBusy
                    inspectionReview: ComponentManagerClient.inspectionReview
                    inspectionToken: ComponentManagerClient.inspectionToken
                    packageError: ComponentManagerClient.packageError
                    configAvailable: ComponentConfigClient.available
                    configCatalogAvailable: ComponentConfigClient.catalogAvailable
                    configWritable: ["normal", "recovered", "defaulted"].includes(ComponentConfigClient.loadState)
                    configBusy: ComponentConfigClient.busy
                    configCatalogDigest: ComponentConfigClient.catalogDigest
                    configSnapshot: ComponentConfigClient.snapshot
                    pendingComponentId: ComponentConfigClient.pendingComponentId
                    lastErrorComponentId: ComponentConfigClient.lastErrorComponentId
                    configError: ComponentConfigClient.lastErrorMessage
                    runtimeAvailable: ComponentRuntimeClient.available && ComponentRuntimeClient.runtimeHealthAvailable
                    thirdPartySafeMode: ComponentRuntimeClient.thirdPartySafeMode
                    runtimeStates: ComponentRuntimeClient.runtimeStates
                    runtimeRetryBusyComponentId: ComponentRuntimeClient.runtimeRetryBusyComponentId
                    contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                    onComponentEnabledRequested: (componentId, packageDigest, enabled) => ComponentConfigClient.setComponentEnabled(componentId, packageDigest, enabled)
                    onInspectPackageRequested: packageUrl => ComponentManagerClient.inspectPackage(packageUrl)
                    onCancelInspectionRequested: ComponentManagerClient.cancelInspection()
                    onInstallInspectedPackageRequested: ComponentManagerClient.installInspectedPackage()
                    onPackageRemovalRequested: (componentId, packageDigest, catalogDigest) => ComponentManagerClient.removeComponent(componentId, packageDigest, catalogDigest)
                    onComponentSettingsRequested: (componentId, packageDigest, settings) => ComponentConfigClient.setComponentSettings(componentId, packageDigest, settings)
                    onComponentAdoptionRequested: (componentId, packageDigest, defaultComponentSettings) => ComponentConfigClient.adoptComponentPackage(componentId, packageDigest, defaultComponentSettings)
                    onComponentAddToBarRequested: (componentId, packageDigest, defaultComponentSettings) => ComponentConfigClient.addComponentToBar(componentId, packageDigest, defaultComponentSettings)
                    onComponentRetryRequested: (componentId, packageDigest) => ComponentRuntimeClient.retryComponent(componentId, packageDigest)
                }

                StackLayout {
                    id: hyprlandPages

                    objectName: "hyprlandPages"
                    currentIndex: root.hyprlandSection === "catalog" ? 1 : root.hyprlandSection === "bindings" ? 2 : root.hyprlandSection === "devices" ? 3 : root.hyprlandSection === "environment" ? 4 : root.hyprlandSection === "permissions" ? 5 : 0

                    HyprlandOverviewPage {
                        id: hyprlandOverviewPage

                        objectName: "hyprlandOverviewPage"
                        allOptions: CompositorClient.allOptions

                        onOpenCategoryRequested: categoryId => {
                            root.hyprlandCategory = categoryId;
                            root.hyprlandSection = "catalog";
                        }
                        onOpenSurfaceRequested: pageId => root.openHyprlandSurface(pageId)
                    }

                    HyprlandCatalogPage {
                        id: hyprlandCatalogPage

                        objectName: "hyprlandCatalogPage"
                        categoryId: root.hyprlandCategory
                        serviceAvailable: CompositorClient.available
                        writable: CompositorClient.writable
                        allOptionsAvailable: CompositorClient.allOptionsAvailable
                        busy: CompositorClient.busy
                        busyOperation: CompositorClient.busyOperation
                        revisionToken: CompositorClient.revisionToken
                        managementState: CompositorClient.managementState
                        applyState: CompositorClient.applyState
                        requiredActivation: CompositorClient.requiredActivation
                        errorName: CompositorClient.allOptionsErrorName
                        errorMessage: CompositorClient.allOptionsErrorMessage.length > 0 ? CompositorClient.allOptionsErrorMessage : root.hyprlandOptionsCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                        allOptions: CompositorClient.allOptions
                        allValues: CompositorClient.allValues
                        contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                        onBackRequested: root.hyprlandSection = "overview"
                        onRefreshRequested: CompositorClient.refresh()
                        onSaveRequested: values => CompositorClient.saveOptions(values)
                        onOpenSurfaceRequested: pageId => root.openHyprlandSurface(pageId)
                    }

                    BindingsPage {
                        id: bindingsPage

                        objectName: "bindingsPage"
                        serviceAvailable: CompositorClient.available
                        writable: CompositorClient.writable
                        catalogAvailable: CompositorClient.catalogAvailable
                        bindingsAvailable: CompositorClient.bindingsAvailable
                        bindingsProjectionAvailable: CompositorClient.bindingsProjectionAvailable
                        actionCatalogAvailable: CompositorClient.actionCatalogAvailable
                        busy: CompositorClient.busy
                        busyOperation: CompositorClient.busyOperation
                        revisionToken: CompositorClient.revisionToken
                        managementState: CompositorClient.managementState
                        applyState: CompositorClient.applyState
                        requiredActivation: CompositorClient.requiredActivation
                        confirmationState: CompositorClient.displayConfirmationState
                        bindings: CompositorClient.bindings
                        defaultBindings: CompositorClient.defaultBindings
                        submaps: CompositorClient.submaps
                        bindingActions: CompositorClient.bindingActions
                        bindingsErrorName: CompositorClient.bindingsErrorName
                        bindingsErrorMessage: CompositorClient.bindingsErrorMessage
                        sharedErrorName: root.bindingsCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorName : ""
                        sharedErrorMessage: root.bindingsCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                        retryApplyAvailable: CompositorClient.retryApplyAvailable
                        recoveryAvailable: CompositorClient.recoveryAvailable
                        sharedMutationBusy: root.sharedCompositorMutationBusy
                        sharedApplySafe: root.sharedCompositorApplySafe
                        contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                        onBackRequested: root.hyprlandSection = "overview"
                        onRefreshRequested: CompositorClient.refresh()
                        onOpenDisplaysRequested: root.currentPage = "displays"
                        onSaveRequested: (bindings, submaps) => CompositorClient.saveBindings(bindings, submaps)
                        onRetryApplyRequested: CompositorClient.retryApply()
                        onRecoveryRequested: CompositorClient.recoverConfiguration()
                    }

                    InputDevicesPage {
                        id: inputDevicesPage

                        objectName: "inputDevicesPage"
                        serviceAvailable: CompositorClient.available
                        writable: CompositorClient.writable
                        catalogAvailable: CompositorClient.catalogAvailable
                        inputDevicesAvailable: CompositorClient.inputDevicesAvailable
                        inputDevicesProjectionAvailable: CompositorClient.inputDevicesProjectionAvailable
                        busy: CompositorClient.busy
                        busyOperation: CompositorClient.busyOperation
                        inputDevices: CompositorClient.inputDevices
                        revisionToken: CompositorClient.revisionToken
                        loadState: CompositorClient.loadState
                        managementState: CompositorClient.managementState
                        applyState: CompositorClient.applyState
                        requiredActivation: CompositorClient.requiredActivation
                        confirmationState: CompositorClient.displayConfirmationState
                        inputDevicesErrorName: CompositorClient.inputDevicesErrorName
                        inputDevicesErrorMessage: CompositorClient.inputDevicesErrorMessage
                        sharedErrorName: root.inputDevicesCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorName : ""
                        sharedErrorMessage: root.inputDevicesCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                        retryApplyAvailable: CompositorClient.retryApplyAvailable
                        recoveryAvailable: CompositorClient.recoveryAvailable
                        sharedMutationBusy: root.sharedCompositorMutationBusy
                        sharedApplySafe: root.sharedCompositorApplySafe
                        contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                        onBackRequested: root.hyprlandSection = "overview"
                        onRefreshRequested: CompositorClient.refresh()
                        onOpenDisplaysRequested: root.currentPage = "displays"
                        onSaveRequested: inputDevices => CompositorClient.saveInputDevices(inputDevices)
                        onRetryApplyRequested: CompositorClient.retryApply()
                        onRecoveryRequested: CompositorClient.recoverConfiguration()
                    }

                    EnvironmentVariablesPage {
                        id: environmentVariablesPage

                        objectName: "environmentVariablesPage"
                        serviceAvailable: CompositorClient.available
                        writable: CompositorClient.writable
                        catalogAvailable: CompositorClient.catalogAvailable
                        environmentAvailable: CompositorClient.environmentAvailable
                        environmentProjectionAvailable: CompositorClient.environmentProjectionAvailable
                        uwsmIntegrationAvailable: false
                        busy: CompositorClient.busy
                        busyOperation: CompositorClient.busyOperation
                        environmentVariables: CompositorClient.environmentVariables
                        revisionToken: CompositorClient.revisionToken
                        loadState: CompositorClient.loadState
                        managementState: CompositorClient.managementState
                        applyState: CompositorClient.applyState
                        requiredActivation: CompositorClient.requiredActivation
                        confirmationState: CompositorClient.displayConfirmationState
                        environmentErrorName: CompositorClient.environmentErrorName
                        environmentErrorMessage: CompositorClient.environmentErrorMessage
                        sharedErrorName: root.environmentCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorName : ""
                        sharedErrorMessage: root.environmentCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                        retryApplyAvailable: CompositorClient.retryApplyAvailable
                        recoveryAvailable: CompositorClient.recoveryAvailable
                        sharedMutationBusy: root.sharedCompositorMutationBusy
                        sharedApplySafe: root.sharedCompositorApplySafe
                        contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                        onBackRequested: root.hyprlandSection = "overview"
                        onRefreshRequested: CompositorClient.refresh()
                        onOpenDisplaysRequested: root.currentPage = "displays"
                        onSaveRequested: environmentVariables => CompositorClient.saveEnvironment(environmentVariables)
                        onRetryApplyRequested: CompositorClient.retryApply()
                        onRecoveryRequested: CompositorClient.recoverConfiguration()
                    }

                    PermissionsPage {
                        id: permissionsPage

                        objectName: "permissionsPage"
                        serviceAvailable: CompositorClient.available
                        writable: CompositorClient.writable
                        catalogAvailable: CompositorClient.catalogAvailable
                        permissionsAvailable: CompositorClient.permissionsAvailable
                        permissionsProjectionAvailable: CompositorClient.permissionsProjectionAvailable
                        busy: CompositorClient.busy
                        busyOperation: CompositorClient.busyOperation
                        permissions: CompositorClient.permissions
                        revisionToken: CompositorClient.revisionToken
                        loadState: CompositorClient.loadState
                        managementState: CompositorClient.managementState
                        applyState: CompositorClient.applyState
                        requiredActivation: CompositorClient.requiredActivation
                        confirmationState: CompositorClient.displayConfirmationState
                        permissionErrorName: CompositorClient.permissionErrorName
                        permissionErrorMessage: CompositorClient.permissionErrorMessage
                        sharedErrorName: root.permissionsCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorName : ""
                        sharedErrorMessage: root.permissionsCompositorError(CompositorClient.lastErrorOperation) ? CompositorClient.lastErrorMessage : ""
                        retryApplyAvailable: CompositorClient.retryApplyAvailable
                        recoveryAvailable: CompositorClient.recoveryAvailable
                        sharedMutationBusy: root.sharedCompositorMutationBusy
                        sharedApplySafe: root.sharedCompositorApplySafe
                        contentTopMargin: shellHealthWarning.warningVisible ? 0 : 28

                        onBackRequested: root.hyprlandSection = "overview"
                        onRefreshRequested: CompositorClient.refresh()
                        onOpenDisplaysRequested: root.currentPage = "displays"
                        onSaveRequested: permissions => CompositorClient.savePermissions(permissions)
                        onRetryApplyRequested: CompositorClient.retryApply()
                        onRecoveryRequested: CompositorClient.recoverConfiguration()
                    }
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
