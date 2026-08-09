pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    required property var activation
    required property string outputName
    required property var workspaceSource
    property bool interactive: true
    property bool keyboardNavigationEnabled: false
    property bool animationsEnabled: true

    readonly property var settings: root.activation
        ? (root.activation.settings || ({}))
        : ({})
    readonly property bool settingsValid:
        root.validWorkspaceSettings(root.settings)
    readonly property bool authorityValid: {
        if (!root.activation)
            return false;
        if (typeof root.activation.compiledFallback !== "boolean")
            return false;
        if (root.activation.compiledFallback === true)
            return String(root.activation.packageDigest || "").length === 0;
        return /^[0-9a-f]{64}$/.test(
            String(root.activation.packageDigest || "")
        );
    }
    readonly property bool workspaceFactorySupported:
        root.activation
        && String(root.activation.componentId || "")
            === "io.github.coastlinesec.hyprshelld.workspace-switcher"
        && String(root.activation.componentType || "") === "bar-widget"
        && String(root.activation.runtimeKind || "") === "builtin-v1"
        && String(root.activation.factory || "") === "workspace-switcher"
        && root.authorityValid
        && root.settingsValid
    readonly property Item loadedComponent: componentLoader.item as Item

    function hasExactKeys(object, expectedKeys) {
        if (!object || typeof object !== "object" || Array.isArray(object))
            return false;
        const actual = Object.keys(object).sort();
        const expected = Array.from(expectedKeys).sort();
        if (actual.length !== expected.length)
            return false;
        for (let index = 0; index < expected.length; ++index) {
            if (actual[index] !== expected[index])
                return false;
        }
        return true;
    }

    function validWorkspaceSettings(settings) {
        if (!root.hasExactKeys(settings, [
                "labelMode",
                "showApplications",
                "maximumApplications",
                "occupiedOnly",
                "scrollMode"
            ])) {
            return false;
        }
        if (!["numbers", "compact", "names"].includes(
                String(settings.labelMode))) {
            return false;
        }
        if (typeof settings.showApplications !== "boolean"
                || typeof settings.maximumApplications !== "number"
                || !Number.isInteger(settings.maximumApplications)
                || settings.maximumApplications < 1
                || settings.maximumApplications > 5
                || typeof settings.occupiedOnly !== "boolean") {
            return false;
        }
        return ["disabled", "normal", "reversed"].includes(
            String(settings.scrollMode)
        );
    }

    objectName: root.activation
        ? "builtinComponentFactory-" + String(
            root.activation.instanceId || "invalid"
        )
        : "builtinComponentFactory-invalid"
    implicitWidth: root.loadedComponent
        ? root.loadedComponent.implicitWidth
        : 0
    implicitHeight: root.loadedComponent
        ? root.loadedComponent.implicitHeight
        : 0

    Loader {
        id: componentLoader

        objectName: "builtinComponentLoader"
        anchors.fill: parent
        active: root.workspaceFactorySupported
        sourceComponent: root.workspaceFactorySupported
            ? workspaceSwitcherComponent
            : null
    }

    Component {
        id: workspaceSwitcherComponent

        WorkspaceSwitcherComponent {
            activation: root.activation
            outputName: root.outputName
            workspaceSource: root.workspaceSource
            interactive: root.interactive
            keyboardNavigationEnabled:
                root.keyboardNavigationEnabled
            animationsEnabled: root.animationsEnabled
        }
    }
}
