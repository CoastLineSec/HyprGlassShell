pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import HyprShelld.UI
import "../WorkspaceProjection.js" as WorkspaceProjection

Item {
    id: root

    required property var activation
    required property string outputName
    required property var workspaceSource
    property bool interactive: true
    property bool keyboardNavigationEnabled: false
    property bool animationsEnabled: true
    property int desktopEntriesRevision: 0

    readonly property var settings: root.activation.settings || ({})
    readonly property bool workspaceOutputAvailable:
        root.workspaceSource
        && root.workspaceSource.available
        && WorkspaceProjection.outputAvailable(
            root.workspaceSource.snapshot,
            root.outputName
        )
    readonly property var workspaceEntries: {
        root.desktopEntriesRevision;
        return WorkspaceProjection.project(
            root.workspaceSource
                ? root.workspaceSource.snapshot
                : null,
            root.outputName,
            {
                occupiedOnly: Boolean(root.settings.occupiedOnly),
                showApplications: Boolean(root.settings.showApplications),
                resolveApplication: rawId => root.resolveApplication(rawId)
            }
        );
    }

    function resolveIconSource(iconName) {
        const normalizedIconName = String(iconName || "");
        if (normalizedIconName.length === 0)
            return "";

        const primarySource = Quickshell.iconPath(
            normalizedIconName,
            true
        );
        if (primarySource.length > 0)
            return primarySource;

        return Quickshell.iconPath("application-x-executable", true);
    }

    function resolveApplication(rawId) {
        const normalizedId = String(rawId || "");
        const entry = normalizedId.length > 0
            ? (DesktopEntries.byId(normalizedId)
                || DesktopEntries.heuristicLookup(normalizedId))
            : null;
        const key = String(entry ? entry.id : normalizedId).toLowerCase();
        const label = String(
            entry && entry.name
                ? entry.name
                : (normalizedId || qsTr("Application"))
        );
        return {
            key: key || "application",
            label: label,
            iconSource: root.resolveIconSource(entry ? entry.icon : ""),
            fallbackInitial: label.length > 0
                ? label.charAt(0).toUpperCase()
                : "?"
        };
    }

    function activateWorkspace(workspaceId) {
        if (!root.workspaceSource)
            return;
        root.workspaceSource.activateWorkspace(
            root.outputName,
            workspaceId
        );
    }

    function activateApplication(workspaceId, activationKey) {
        if (!root.workspaceSource)
            return;
        root.workspaceSource.activateWindow(
            root.outputName,
            workspaceId,
            activationKey
        );
    }

    function stepWorkspace(direction) {
        if (!root.workspaceSource || !root.workspaceOutputAvailable)
            return;

        const monitor = WorkspaceProjection.monitorForOutput(
            root.workspaceSource.snapshot,
            root.outputName
        );
        if (!monitor)
            return;

        const targetId = WorkspaceProjection.adjacentWorkspaceId(
            root.workspaceEntries,
            monitor.activeWorkspaceId,
            direction
        );
        if (targetId === null)
            return;
        root.workspaceSource.activateWorkspace(root.outputName, targetId);
    }

    objectName: "workspaceSwitcherComponent"
    implicitWidth: workspaceSwitcher.implicitWidth
    implicitHeight: workspaceSwitcher.implicitHeight

    Connections {
        target: DesktopEntries

        function onApplicationsChanged() {
            ++root.desktopEntriesRevision;
        }
    }

    ScriptModel {
        id: workspaceModel

        values: root.workspaceEntries
        objectProp: "key"
    }

    WorkspaceSwitcher {
        id: workspaceSwitcher

        anchors.fill: parent
        workspaces: workspaceModel
        available: root.workspaceOutputAvailable
        outputName: root.outputName
        showIdentifiers: Boolean(root.settings.showIdentifiers)
        showNames: Boolean(root.settings.showNames)
        showApplications: Boolean(root.settings.showApplications)
        maximumApplications: Number(root.settings.maximumApplications)
        scrollMode: String(root.settings.scrollMode)
        interactive: root.interactive
            && root.workspaceSource
            && root.workspaceSource.actionsAvailable
        keyboardNavigationEnabled:
            root.keyboardNavigationEnabled
        animationsEnabled: root.animationsEnabled

        onWorkspaceRequested: workspaceId => {
            root.activateWorkspace(workspaceId);
        }

        onApplicationRequested: (workspaceId, activationKey) => {
            root.activateApplication(workspaceId, activationKey);
        }

        onWorkspaceStepRequested: direction => {
            root.stepWorkspace(direction);
        }
    }
}
