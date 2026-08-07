//@ pragma ShellId hyprshelld-qualification
//@ pragma StateDir $BASE/hyprshelld/qualification
//@ pragma CacheDir $BASE/hyprshelld/qualification
//@ pragma DataDir $BASE/hyprshelld/qualification

import QtQuick
import Quickshell
import Quickshell.Io

ShellRoot {
    id: root

    signal setPopoutState(string screenName, bool opened)

    property string motionScreenName: ""
    property bool motionOpen: false
    property int motionStepsRemaining: 0

    function hasScreen(screenName) {
        for (let index = 0; index < Quickshell.screens.length; ++index) {
            if (Quickshell.screens[index].name === screenName) {
                return true;
            }
        }

        return false;
    }

    IpcHandler {
        target: "qualification"

        function open(screenName: string): string {
            if (!root.hasScreen(screenName)) {
                return `Unknown screen: ${screenName}`;
            }

            root.setPopoutState(screenName, true);
            return `Opened the test popout on ${screenName}`;
        }

        function close(screenName: string): string {
            if (!root.hasScreen(screenName)) {
                return `Unknown screen: ${screenName}`;
            }

            root.setPopoutState(screenName, false);
            return `Closed the test popout on ${screenName}`;
        }

        function animate(screenName: string): string {
            if (!root.hasScreen(screenName)) {
                return `Unknown screen: ${screenName}`;
            }

            if (motionTimer.running || motionCompletionTimer.running) {
                return "A motion run is already active";
            }

            root.motionScreenName = screenName;
            root.motionOpen = true;
            root.motionStepsRemaining = 55;
            console.info(`HYPRSHELLD_MOTION_START screen=${screenName}`);
            root.setPopoutState(screenName, true);
            motionTimer.start();
            return `Requested a 10-second motion run on ${screenName}`;
        }
    }

    Timer {
        id: motionTimer

        interval: 180
        repeat: true

        onTriggered: {
            root.motionOpen = !root.motionOpen;
            root.setPopoutState(root.motionScreenName, root.motionOpen);
            --root.motionStepsRemaining;

            if (root.motionStepsRemaining === 0) {
                stop();
                motionCompletionTimer.restart();
            }
        }
    }

    Timer {
        id: motionCompletionTimer

        interval: 200

        onTriggered: {
            console.info(`HYPRSHELLD_MOTION_END screen=${root.motionScreenName}`);
        }
    }

    Variants {
        model: Quickshell.screens

        QualificationSurface {
            controller: root
        }
    }
}
