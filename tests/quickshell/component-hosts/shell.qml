//@ pragma ShellId hyprshelld-component-hosts-test
//@ pragma StateDir $BASE/hyprshelld/component-hosts-test
//@ pragma CacheDir $BASE/hyprshelld/component-hosts-test
//@ pragma DataDir $BASE/hyprshelld/component-hosts-test

pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import "surfaced/components" as SurfaceComponents

ShellRoot {
    id: root

    property int stage: 0
    property bool failed: false
    property var firstBefore: null
    property var secondBefore: null
    readonly property string firstInstanceId:
        "7b4e2329-4320-4e15-894d-218fa690d782"
    readonly property string secondInstanceId:
        "d520f90e-1521-4e0d-9ddc-ae20a4e948da"
    readonly property var validSettings: ({
        labelMode: "numbers",
        showApplications: false,
        maximumApplications: 3,
        occupiedOnly: false,
        scrollMode: "disabled"
    })
    property var instances: [
        root.fallbackActivation(root.firstInstanceId),
        root.fallbackActivation(root.secondInstanceId)
    ]

    function fallbackActivation(instanceId) {
        return {
            instanceId: instanceId,
            componentId:
                "io.github.coastlinesec.hyprshelld.workspace-switcher",
            componentType: "bar-widget",
            packageDigest: "",
            runtimeKind: "builtin-v1",
            factory: "workspace-switcher",
            settings: Object.assign({}, root.validSettings),
            compiledFallback: true
        };
    }

    function findNamed(item, name) {
        if (!item)
            return null;
        if (item.objectName === name)
            return item;

        const children = item.children || [];
        for (let index = 0; index < children.length; ++index) {
            const result = root.findNamed(children[index], name);
            if (result)
                return result;
        }
        return null;
    }

    function check(condition, message) {
        if (condition)
            return true;

        root.failed = true;
        console.error("COMPONENT_HOSTS_FAIL: " + message);
        Qt.quit();
        return false;
    }

    function schedule(milliseconds) {
        runner.interval = milliseconds;
        runner.restart();
    }

    function advance() {
        if (root.failed)
            return;

        if (root.stage === 0) {
            const validLoader = root.findNamed(
                validFactory,
                "builtinComponentLoader"
            );
            const unknownLoader = root.findNamed(
                unknownFactory,
                "builtinComponentLoader"
            );
            const invalidSettingsLoader = root.findNamed(
                invalidSettingsFactory,
                "builtinComponentLoader"
            );
            if (!root.check(validLoader && validLoader.active
                    && root.findNamed(
                        validFactory,
                        "workspaceSwitcherComponent"
                    ), "compiled fallback did not load the builtin")) {
                return;
            }
            if (!root.check(unknownLoader && !unknownLoader.active
                    && invalidSettingsLoader
                    && !invalidSettingsLoader.active,
                    "closed factory accepted an unknown or invalid tuple")) {
                return;
            }

            root.firstBefore = root.findNamed(
                host,
                "builtinComponentFactory-" + root.firstInstanceId
            );
            root.secondBefore = root.findNamed(
                host,
                "builtinComponentFactory-" + root.secondInstanceId
            );
            if (!root.check(root.firstBefore && root.secondBefore
                    && root.firstBefore.x < root.secondBefore.x
                    && host.implicitWidth > 0,
                    "initial keyed host order was not materialized")) {
                return;
            }

            root.instances = [
                root.fallbackActivation(root.secondInstanceId),
                root.fallbackActivation(root.firstInstanceId)
            ];
            ++root.stage;
            root.schedule(100);
            return;
        }

        if (root.stage === 1) {
            const first = root.findNamed(
                host,
                "builtinComponentFactory-" + root.firstInstanceId
            );
            const second = root.findNamed(
                host,
                "builtinComponentFactory-" + root.secondInstanceId
            );
            if (!root.check(first === root.firstBefore
                    && second === root.secondBefore
                    && second.x < first.x,
                    "keyed reorder recreated delegates or kept stale order")) {
                return;
            }

            // This models the first authoritative plan being empty. The host
            // must not recreate the compiled first-run fallback.
            root.instances = [];
            ++root.stage;
            root.schedule(100);
            return;
        }

        if (!root.check(host.implicitWidth === 0
                && !root.findNamed(
                    host,
                    "builtinComponentFactory-" + root.firstInstanceId
                )
                && !root.findNamed(
                    host,
                    "builtinComponentFactory-" + root.secondInstanceId
                ), "authoritative empty plan resurrected fallback content")) {
            return;
        }

        console.log("COMPONENT_HOSTS_PASS");
        Qt.quit();
    }

    QtObject {
        id: workspaceSource

        property bool available: false
        property bool actionsAvailable: false
        property var snapshot: ({
            revision: 0,
            monitors: [],
            workspaces: [],
            clients: []
        })

        function activateWorkspace(outputName, workspaceId) {
            return false;
        }

        function activateWindow(outputName, workspaceId, address) {
            return false;
        }
    }

    SurfaceComponents.BuiltinComponentFactory {
        id: validFactory

        width: implicitWidth
        height: 40
        outputName: "DP-4"
        workspaceSource: workspaceSource
        interactive: false
        animationsEnabled: false
        activation: root.fallbackActivation(root.firstInstanceId)
    }

    SurfaceComponents.BuiltinComponentFactory {
        id: unknownFactory

        width: implicitWidth
        height: 40
        outputName: "DP-4"
        workspaceSource: workspaceSource
        interactive: false
        animationsEnabled: false
        activation: Object.assign(
            {},
            root.fallbackActivation(root.firstInstanceId),
            { factory: "untrusted-qml-path" }
        )
    }

    SurfaceComponents.BuiltinComponentFactory {
        id: invalidSettingsFactory

        width: implicitWidth
        height: 40
        outputName: "DP-4"
        workspaceSource: workspaceSource
        interactive: false
        animationsEnabled: false
        activation: Object.assign(
            {},
            root.fallbackActivation(root.firstInstanceId),
            {
                settings: Object.assign({}, root.validSettings, {
                    maximumApplications: "3"
                })
            }
        )
    }

    SurfaceComponents.BarComponentHost {
        id: host

        width: implicitWidth
        height: 40
        outputName: "DP-4"
        workspaceSource: workspaceSource
        interactive: false
        animationsEnabled: false
        instances: root.instances
    }

    Timer {
        id: runner

        interval: 100
        repeat: false
        running: true
        onTriggered: root.advance()
    }
}
