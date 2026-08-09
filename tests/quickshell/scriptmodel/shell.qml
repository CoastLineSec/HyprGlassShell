//@ pragma ShellId hyprshelld-scriptmodel-test
//@ pragma StateDir $BASE/hyprshelld/scriptmodel-test
//@ pragma CacheDir $BASE/hyprshelld/scriptmodel-test
//@ pragma DataDir $BASE/hyprshelld/scriptmodel-test

pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import HyprShelld.UI

ShellRoot {
    id: root

    property int stage: 0
    property bool failed: false
    property var firstBefore: null
    property var secondBefore: null
    property var thirdBefore: null
    property var replacedActiveBefore: null
    property real firstCircleWidth: 0
    property var entries: [
        root.workspace(1, "1", false, false, []),
        root.workspace(2, "writing", true, false, [
            root.application("editor", false, 1)
        ]),
        root.workspace(3, "chat", false, false, [])
    ]

    function application(key, active, count) {
        return {
            key: key,
            itemKey: key + ":window",
            activationKey: key + ":window",
            label: key,
            iconSource: "",
            fallbackInitial: key.charAt(0).toUpperCase(),
            active: active,
            count: count,
            activatable: false
        };
    }

    function workspace(id, name, active, urgent, applications) {
        return {
            key: "workspace:" + id,
            workspaceId: id,
            name: name,
            numberLabel: String(id),
            active: active,
            urgent: urgent,
            occupied: applications.length > 0,
            applications: applications
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
        console.error("SCRIPT_MODEL_RECONCILIATION_FAIL: " + message);
        Qt.quit();
        return false;
    }

    function nearlyEqual(left, right, tolerance) {
        return Math.abs(Number(left) - Number(right)) <= tolerance;
    }

    function viewportIsBounded() {
        const flickable = root.findNamed(switcher, "workspaceFlickable");
        if (!flickable)
            return false;
        const maximum = Math.max(0, flickable.contentWidth - flickable.width);
        return flickable.contentX >= -0.5
            && flickable.contentX <= maximum + 0.5;
    }

    function activeIsRevealed(objectName) {
        const active = root.findNamed(switcher, objectName);
        const flickable = root.findNamed(switcher, "workspaceFlickable");
        if (!active || !flickable)
            return false;

        const maximum = Math.max(0, flickable.contentWidth - flickable.width);
        if (active.width >= flickable.width) {
            const logicalLeft = active.mapToItem(
                flickable,
                0,
                active.height / 2
            ).x + flickable.contentX;
            const expected = Math.max(0, Math.min(maximum, logicalLeft));
            return root.nearlyEqual(flickable.contentX, expected, 1);
        }

        const left = active.mapToItem(
            flickable,
            0,
            active.height / 2
        ).x;
        const right = active.mapToItem(
            flickable,
            active.width,
            active.height / 2
        ).x;
        return left >= -1 && right <= flickable.width + 1;
    }

    function circleMatches(indicator, expectedSize) {
        if (!indicator)
            return false;

        const circle = root.findNamed(
            indicator,
            "workspaceCircle-" + indicator["workspaceId"]
        );
        return circle
            && root.nearlyEqual(circle.width, expectedSize, 0.05)
            && root.nearlyEqual(circle.height, expectedSize, 0.05);
    }

    function schedule(milliseconds) {
        runner.interval = milliseconds;
        runner.restart();
    }

    function advance() {
        if (root.failed)
            return;

        if (root.stage === 0) {
            const missingIconName =
                "__hyprshelld_checked_icon_regression_missing__";
            if (!root.check(
                    Quickshell.iconPath(missingIconName, true) === "",
                    "checked missing icon resolution returned a source")) {
                return;
            }
            root.firstBefore = root.findNamed(
                switcher,
                "workspaceIndicator-1"
            );
            root.secondBefore = root.findNamed(
                switcher,
                "workspaceIndicator-2"
            );
            root.thirdBefore = root.findNamed(
                switcher,
                "workspaceIndicator-3"
            );
            if (!root.check(root.firstBefore && root.secondBefore
                    && root.thirdBefore, "initial delegates missing")) {
                return;
            }
            const firstCircle = root.findNamed(
                root.firstBefore,
                "workspaceCircle-1"
            );
            if (!root.check(root.circleMatches(
                    root.firstBefore,
                    switcher["inactiveCircleSize"]
                ) && root.circleMatches(
                    root.secondBefore,
                    switcher["activeCircleSize"]
                ), "initial circle geometry was incorrect")) {
                return;
            }
            root.firstCircleWidth = firstCircle.width;
            switcher.animationsEnabled = true;
            root.entries = [
                root.workspace(1, "1", true, false, []),
                root.workspace(2, "renamed", false, true, [
                    root.application("editor", false, 3)
                ]),
                root.workspace(3, "chat", false, false, [])
            ];
            ++root.stage;
            root.schedule(65);
            return;
        }

        if (root.stage === 1) {
            const first = root.findNamed(switcher, "workspaceIndicator-1");
            const second = root.findNamed(switcher, "workspaceIndicator-2");
            const third = root.findNamed(switcher, "workspaceIndicator-3");
            if (!root.check(first === root.firstBefore
                    && second === root.secondBefore
                    && third === root.thirdBefore,
                    "same keys replaced workspace delegates")) {
                return;
            }
            if (!root.check(first["workspaceActive"]
                    && second["workspaceUrgent"],
                    "active or urgent state did not update")) {
                return;
            }
            const firstCircle = root.findNamed(first, "workspaceCircle-1");
            const secondCircle = root.findNamed(second, "workspaceCircle-2");
            if (!root.check(first.width === switcher["workspaceHitCellWidth"]
                    && firstCircle.width > root.firstCircleWidth
                    && firstCircle.width < switcher["activeCircleSize"]
                    && secondCircle.width > switcher["inactiveCircleSize"]
                    && secondCircle.width < switcher["activeCircleSize"],
                    "circle state transition did not animate in fixed cells")) {
                return;
            }
            if (!root.check(!root.findNamed(
                    switcher,
                    "workspaceSelectionLens"
                ), "removed selection lens was recreated")) {
                return;
            }
            ++root.stage;
            root.schedule(120);
            return;
        }

        if (root.stage === 2) {
            const first = root.findNamed(switcher, "workspaceIndicator-1");
            const second = root.findNamed(switcher, "workspaceIndicator-2");
            const firstCircle = root.findNamed(first, "workspaceCircle-1");
            const secondCircle = root.findNamed(second, "workspaceCircle-2");
            if (!root.check(root.circleMatches(
                    first,
                    switcher["activeCircleSize"]
                ) && root.circleMatches(
                    second,
                    switcher["inactiveCircleSize"]
                ) && first.width === switcher["workspaceHitCellWidth"],
                    "circle state transition did not settle: first="
                        + (firstCircle ? firstCircle.width : "missing")
                        + ", second="
                        + (secondCircle ? secondCircle.width : "missing")
                        + ", cell=" + first.width)) {
                return;
            }
            switcher.showIdentifiers = true;
            switcher.showNames = true;
            switcher.showApplications = true;
            ++root.stage;
            root.schedule(180);
            return;
        }

        if (root.stage === 3) {
            const second = root.findNamed(switcher, "workspaceIndicator-2");
            const application = root.findNamed(
                switcher,
                "workspaceApplication-2-0"
            );
            const identifier = root.findNamed(
                switcher,
                "workspaceIdentifier-2"
            );
            const nameLabel = root.findNamed(
                switcher,
                "workspaceLabel-2"
            );
            if (!root.check(second === root.secondBefore
                    && second["nameLabel"] === "renamed"
                    && identifier && identifier.visible
                    && identifier.text === "2"
                    && nameLabel && nameLabel.visible
                    && nameLabel.text === "renamed",
                    "identifier/name update replaced or missed a delegate")) {
                return;
            }
            if (!root.check(application
                    && application["applicationCount"] === 3
                    && !application["applicationActive"],
                    "nested application data did not reconcile")) {
                return;
            }
            switcher.width = 62;
            root.entries = [
                root.workspace(3, "chat", false, false, []),
                root.workspace(1, "1", true, false, []),
                root.workspace(2, "renamed", false, true, [
                    root.application("editor", false, 3)
                ])
            ];
            ++root.stage;
            root.schedule(180);
            return;
        }

        if (root.stage === 4) {
            const first = root.findNamed(switcher, "workspaceIndicator-1");
            const second = root.findNamed(switcher, "workspaceIndicator-2");
            const third = root.findNamed(switcher, "workspaceIndicator-3");
            if (!root.check(first === root.firstBefore
                    && second === root.secondBefore
                    && third === root.thirdBefore,
                    "reorder replaced workspace delegates")) {
                return;
            }
            if (!root.check(third.x < first.x && first.x < second.x,
                    "reorder did not update visual order: "
                        + third.x + "," + first.x + "," + second.x)) {
                return;
            }
            if (!root.check(root.viewportIsBounded()
                    && root.activeIsRevealed("workspaceIndicator-1"),
                    "reorder left active workspace outside bounded viewport")) {
                return;
            }
            if (!root.check(root.circleMatches(
                    first,
                    switcher["activeCircleSize"]
                ), "active circle diverged after reorder")) {
                return;
            }
            root.entries = [
                root.workspace(4, "persistent", false, false, []),
                root.entries[0],
                root.entries[1]
            ];
            ++root.stage;
            root.schedule(180);
            return;
        }

        if (root.stage === 5) {
            const first = root.findNamed(switcher, "workspaceIndicator-1");
            const third = root.findNamed(switcher, "workspaceIndicator-3");
            const removed = root.findNamed(switcher, "workspaceIndicator-2");
            const persistent = root.findNamed(
                switcher,
                "workspaceIndicator-4"
            );
            if (!root.check(first === root.firstBefore
                    && third === root.thirdBefore && !removed,
                    "add/remove replaced retained delegates")) {
                return;
            }
            const bounded = root.viewportIsBounded();
            const revealed = root.activeIsRevealed(
                "workspaceIndicator-1"
            );
            const flickable = root.findNamed(
                switcher,
                "workspaceFlickable"
            );
            const active = root.findNamed(
                switcher,
                "workspaceIndicator-1"
            );
            if (!root.check(persistent
                    && root.circleMatches(
                        persistent,
                        switcher["inactiveCircleSize"]
                    ) && bounded && revealed,
                    "real empty workspace update broke active reveal: real="
                        + Boolean(persistent) + ", bounded=" + bounded
                        + ", revealed=" + revealed + ", active="
                        + (active ? active.x + "/" + active.width : "none")
                        + ", viewport=" + (flickable
                            ? flickable.contentX + "/" + flickable.width
                                + "/" + flickable.contentWidth
                            : "none"))) {
                return;
            }
            root.entries = [];
            ++root.stage;
            root.schedule(60);
            return;
        }

        if (root.stage === 6) {
            if (!root.check(!root.findNamed(
                    switcher,
                    "workspaceIndicator-1"
                ), "empty model retained removed delegate")) {
                return;
            }
            root.entries = [
                root.workspace(9, "new", true, false, [])
            ];
            ++root.stage;
            root.schedule(100);
            return;
        }

        if (root.stage === 7) {
            const inserted = root.findNamed(
                switcher,
                "workspaceIndicator-9"
            );
            if (!root.check(inserted && inserted["workspaceActive"]
                    && root.viewportIsBounded()
                    && root.activeIsRevealed("workspaceIndicator-9"),
                    "empty-to-active insertion was not revealed")) {
                return;
            }
            root.replacedActiveBefore = inserted;
            root.entries = [
                root.workspace(10, "replacement", true, false, [])
            ];
            ++root.stage;
            root.schedule(100);
            return;
        }

        const replacement = root.findNamed(
            switcher,
            "workspaceIndicator-10"
        );
        if (!root.check(replacement
                && replacement !== root.replacedActiveBefore
                && replacement["workspaceActive"]
                && root.viewportIsBounded()
                && root.activeIsRevealed("workspaceIndicator-10"),
                "active-row replacement was not reconciled and revealed")) {
            return;
        }

        console.log("SCRIPT_MODEL_RECONCILIATION_PASS");
        Qt.quit();
    }

    ScriptModel {
        id: workspaceModel

        values: root.entries
        objectProp: "key"
    }

    Item {
        width: 520
        height: 40

        WorkspaceSwitcher {
            id: switcher

            width: parent.width
            height: parent.height
            workspaces: workspaceModel
            available: true
            outputName: "Test"
            showIdentifiers: false
            showNames: false
            showApplications: false
            maximumApplications: 2
            scrollMode: "disabled"
            interactive: false
            keyboardNavigationEnabled: false
            animationsEnabled: false
        }
    }

    Timer {
        id: runner

        interval: 80
        repeat: false
        running: true
        onTriggered: root.advance()
    }
}
