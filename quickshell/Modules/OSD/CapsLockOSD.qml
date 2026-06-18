import QtQuick
import qs.Common
import qs.Services
import qs.Widgets

HGSOSD {
    id: root

    osdWidth: Theme.iconSize + Theme.spacingS * 2
    osdHeight: Theme.iconSize + Theme.spacingS * 2
    autoHideInterval: 2000
    enableMouseInteraction: false

    property bool lastCapsLockState: false

    Connections {
        target: HGSService

        function onCapsLockStateChanged() {
            if (lastCapsLockState !== HGSService.capsLockState && SettingsData.osdCapsLockEnabled) {
                root.show()
            }
            lastCapsLockState = HGSService.capsLockState
        }
    }

    Component.onCompleted: {
        lastCapsLockState = HGSService.capsLockState
    }

    content: HGSIcon {
        anchors.centerIn: parent
        name: HGSService.capsLockState ? "shift_lock" : "shift_lock_off"
        size: Theme.iconSize
        color: Theme.primary
    }
}
