pragma ComponentBehavior: Bound

import QtQuick
import HyprShelld.Client

Item {
    id: root

    required property var activation
    required property string outputName
    required property var workspaceSource
    property bool interactive: true
    property bool keyboardNavigationEnabled: false
    property bool animationsEnabled: true
    readonly property int stabilizationWindowMs: 2000

    readonly property bool digestValid: root.activation
        && /^[0-9a-f]{64}$/.test(
            String(root.activation.packageDigest || "")
        )
    readonly property bool planDigestValid: root.activation
        && /^[0-9a-f]{64}$/.test(
            String(root.activation.surfacePlanDigest || "")
        )
    readonly property bool declarativeSupported:
        root.activation
        && root.activation.compiledFallback === false
        && String(root.activation.componentType || "") === "bar-widget"
        && String(root.activation.runtimeKind || "") === "declarative-v1"
        && String(root.activation.factory || "").length === 0
        && root.digestValid
        && root.planDigestValid
        && typeof root.activation.declarativeText === "string"
        && root.activation.declarativeText.length > 0
        && root.activation.declarativeText.length <= 128
        && typeof root.activation.declarativeTooltip === "string"
        && root.activation.declarativeTooltip.length <= 256
        && typeof root.activation.declarativeMaximumWidth === "number"
        && Number.isInteger(root.activation.declarativeMaximumWidth)
        && root.activation.declarativeMaximumWidth >= 48
        && root.activation.declarativeMaximumWidth <= 512
        && root.hasExactKeys(root.activation.settings || ({}), [])
    readonly property bool builtinRequested: root.activation
        && String(root.activation.runtimeKind || "") === "builtin-v1"
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

    function reportStable() {
        if (!root.declarativeSupported
                || componentLoader.status !== Loader.Ready)
            return;
        ComponentRuntimeClient.reportActivationStable(
            String(root.activation.instanceId),
            String(root.activation.componentId),
            String(root.activation.packageDigest),
            String(root.activation.surfacePlanDigest)
        );
    }

    function reportFailure(reason) {
        if (!root.declarativeSupported)
            return;
        ComponentRuntimeClient.reportActivationFailed(
            String(root.activation.instanceId),
            String(root.activation.componentId),
            String(root.activation.packageDigest),
            String(root.activation.surfacePlanDigest),
            reason
        );
    }

    objectName: root.builtinRequested
        ? "builtinComponentFactory-" + String(
            root.activation ? root.activation.instanceId : "invalid"
        )
        : "declarativeComponentFactory-" + String(
            root.activation ? root.activation.instanceId : "invalid"
        )
    implicitWidth: root.loadedComponent
        ? root.loadedComponent.implicitWidth
        : 0
    implicitHeight: root.loadedComponent
        ? root.loadedComponent.implicitHeight
        : 0

    Loader {
        id: componentLoader

        objectName: "trustedComponentLoader"
        anchors.fill: parent
        active: root.builtinRequested || root.declarativeSupported
        sourceComponent: root.builtinRequested
            ? builtinComponent
            : root.declarativeSupported
                ? declarativeComponent
                : null

        onLoaded: stabilizationTimer.restart()
        onStatusChanged: {
            if (status === Loader.Error)
                root.reportFailure("render-failed");
        }
    }

    Timer {
        id: stabilizationTimer

        objectName: "declarativeStabilizationTimer"
        interval: root.stabilizationWindowMs
        repeat: false
        onTriggered: root.reportStable()
    }

    Component {
        id: builtinComponent

        BuiltinComponentFactory {
            activation: root.activation
            outputName: root.outputName
            workspaceSource: root.workspaceSource
            interactive: root.interactive
            keyboardNavigationEnabled: root.keyboardNavigationEnabled
            animationsEnabled: root.animationsEnabled
        }
    }

    Component {
        id: declarativeComponent

        DeclarativeComponentFactory {
            displayText: String(root.activation.declarativeText)
            tooltipText: String(root.activation.declarativeTooltip)
            maximumWidth: Number(root.activation.declarativeMaximumWidth)
        }
    }
}
