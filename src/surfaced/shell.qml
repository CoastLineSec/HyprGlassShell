//@ pragma ShellId hyprshelld-surfaced
//@ pragma StateDir $BASE/hyprshelld/surfaced
//@ pragma CacheDir $BASE/hyprshelld/surfaced
//@ pragma DataDir $BASE/hyprshelld/surfaced

pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Hyprland
import HyprShelld.Client

ShellRoot {
    id: root

    property date currentTime: new Date()
    property bool failureNoticeVisible: false
    property string failureNoticeScreenName: ""
    property string failureNoticeText: ""
    property bool coordinatorFailureLatched: false
    readonly property bool shellDegraded: coordinatorFailureLatched
        || !CoordinatorClient.healthy
    readonly property string healthSummary: {
        if (coordinatorFailureLatched && !CoordinatorClient.healthy)
            return qsTr("Multiple HyprShelld components need attention.");
        if (coordinatorFailureLatched)
            return qsTr("Shell health needs attention.");
        if (CoordinatorClient.failureSummary.length > 0)
            return CoordinatorClient.failureSummary;
        return CoordinatorClient.healthy
            ? ""
            : qsTr("A HyprShelld component needs attention.");
    }
    readonly property string surfacedUnitName: "hyprshelld-surfaced.service"
    readonly property string coordinatorUnitName: "hyprshelld.service"

    HyprlandWorkspaceSource {
        id: hyprlandWorkspaceSource

        requestSocketPath: Hyprland.requestSocketPath
        eventSocketPath: Hyprland.eventSocketPath
        onDispatchRequested: command => Hyprland.dispatch(command)
    }

    function containsDisplayableFailure(unitNames) {
        for (let index = 0; index < unitNames.length; ++index) {
            if (unitNames[index] !== root.surfacedUnitName)
                return true;
        }

        return false;
    }

    function firstScreenName() {
        return Quickshell.screens.length > 0
            ? Quickshell.screens[0].name
            : "";
    }

    function hasDisplayableFailure() {
        return root.coordinatorFailureLatched
            || root.containsDisplayableFailure(CoordinatorClient.failedUnits);
    }

    function setCoordinatorFailureLatched(failed) {
        if (root.coordinatorFailureLatched === failed)
            return;

        root.coordinatorFailureLatched = failed;
        if (failed) {
            root.showFailureNotice(
                root.healthSummary,
                [root.coordinatorUnitName]
            );
        } else {
            root.reconcileFailureNotice();
        }
    }

    function reconcileCoordinatorFailure() {
        if (CoordinatorClient.available) {
            root.setCoordinatorFailureLatched(false);
            return;
        }

        if (!shellRuntimeStatus.available)
            return;

        if (shellRuntimeStatus.coordinatorState === "failed")
            root.setCoordinatorFailureLatched(true);
        else if (shellRuntimeStatus.coordinatorState === "active")
            root.setCoordinatorFailureLatched(false);
    }

    function dismissFailureNotice() {
        failureNoticeTimer.stop();
        root.failureNoticeVisible = false;
        root.failureNoticeScreenName = "";
        root.failureNoticeText = "";
    }

    function showFailureNotice(summary, newlyFailedUnits) {
        if (!root.containsDisplayableFailure(newlyFailedUnits))
            return;

        const screenName = root.firstScreenName();
        if (screenName.length === 0)
            return;

        root.failureNoticeScreenName = screenName;
        root.failureNoticeText = summary.length > 0
            ? summary
            : qsTr("A HyprShelld component needs attention.");
        root.failureNoticeVisible = true;
        failureNoticeTimer.restart();
    }

    function reconcileFailureNotice() {
        if (!root.hasDisplayableFailure()) {
            root.dismissFailureNotice();
        } else if (root.failureNoticeVisible) {
            root.failureNoticeText = root.healthSummary.length > 0
                ? root.healthSummary
                : qsTr("A HyprShelld component needs attention.");
        }
    }

    function reconcileFailureNoticeScreen() {
        if (!root.failureNoticeVisible)
            return;

        for (let index = 0; index < Quickshell.screens.length; ++index) {
            if (Quickshell.screens[index].name
                    === root.failureNoticeScreenName) {
                return;
            }
        }

        const screenName = root.firstScreenName();
        if (screenName.length === 0) {
            root.dismissFailureNotice();
        } else {
            root.failureNoticeScreenName = screenName;
        }
    }

    function authorizeRuntimePlan() {
        if (Quickshell.screens.length === 0) {
            ComponentRuntimeClient.cancelCurrentPlanAuthorization();
            return;
        }
        if (!ComponentRuntimeClient.planCurrent) {
            return;
        }
        ComponentRuntimeClient.authorizeCurrentPlan();
    }

    Component.onCompleted: root.authorizeRuntimePlan()
    Component.onDestruction:
        ComponentRuntimeClient.cancelCurrentPlanAuthorization()

    Connections {
        target: ComponentRuntimeClient

        function onPlanChanged() {
            Qt.callLater(root.authorizeRuntimePlan);
        }

        function onPlanStateChanged() {
            Qt.callLater(root.authorizeRuntimePlan);
        }
    }

    ShellRuntimeStatus {
        id: shellRuntimeStatus

        active: !CoordinatorClient.available
        onAvailableChanged: root.reconcileCoordinatorFailure()
        onStatesChanged: root.reconcileCoordinatorFailure()
    }

    Connections {
        target: CoordinatorClient

        function onPersistentFailureAdded(summary, unitNames) {
            root.showFailureNotice(
                root.healthSummary.length > 0
                    ? root.healthSummary
                    : summary,
                unitNames
            );
        }

        function onHealthChanged() {
            root.reconcileCoordinatorFailure();
            root.reconcileFailureNotice();
        }

        function onAvailableChanged() {
            root.reconcileCoordinatorFailure();
        }
    }

    Connections {
        target: Quickshell

        function onScreensChanged() {
            root.reconcileFailureNoticeScreen();
            root.authorizeRuntimePlan();
        }
    }

    Timer {
        id: failureNoticeTimer

        interval: 6000
        repeat: false
        onTriggered: root.dismissFailureNotice()
    }

    Timer {
        interval: 30000
        repeat: true
        running: true
        onTriggered: root.currentTime = new Date()
    }

    Variants {
        model: Quickshell.screens

        BarSurface {
            workspaceSource: hyprlandWorkspaceSource
            currentTime: root.currentTime
            shellDegraded: root.shellDegraded
            healthSummary: root.healthSummary
            failureNoticeActive: root.failureNoticeVisible
            failureNoticeScreenName: root.failureNoticeScreenName
            failureNoticeText: root.failureNoticeText
        }
    }
}
