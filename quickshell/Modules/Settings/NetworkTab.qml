pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Services
import qs.Widgets

// Combined Network page: all sections shown at once, stacked. Each is a settings tab
// reused here at its content height (its own inner scroll is inert, so the single
// outer scroll moves the whole page).
Item {
    id: networkTab

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    HGSFlickable {
        anchors.fill: parent
        clip: true
        contentHeight: stack.height + Theme.spacingXL
        contentWidth: width

        Column {
            id: stack
            width: parent.width
            topPadding: Theme.spacingS
            spacing: Theme.spacingM

            NetworkStatusTab {
                width: parent.width
                height: implicitHeight
            }

            NetworkEthernetTab {
                width: parent.width
                height: implicitHeight
            }

            NetworkVpnTab {
                width: parent.width
                height: implicitHeight
            }

            NetworkFirewallTab {
                width: parent.width
                height: implicitHeight
            }

            NetworkDnsTab {
                width: parent.width
                height: implicitHeight
            }

            NetworkServicesTab {
                width: parent.width
                height: implicitHeight
            }
        }
    }
}
