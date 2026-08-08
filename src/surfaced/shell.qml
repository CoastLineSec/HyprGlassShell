//@ pragma ShellId hyprshelld-surfaced
//@ pragma StateDir $BASE/hyprshelld/surfaced
//@ pragma CacheDir $BASE/hyprshelld/surfaced
//@ pragma DataDir $BASE/hyprshelld/surfaced

pragma ComponentBehavior: Bound

import QtQuick
import Quickshell

ShellRoot {
    id: root

    property date currentTime: new Date()

    Timer {
        interval: 30000
        repeat: true
        running: true
        onTriggered: root.currentTime = new Date()
    }

    Variants {
        model: Quickshell.screens

        BarSurface {
            currentTime: root.currentTime
        }
    }
}
