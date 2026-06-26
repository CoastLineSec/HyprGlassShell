pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Widgets

// Hearing accessibility. No backend yet — placeholders to research later.
Item {
    id: page

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    HGSFlickable {
        anchors.fill: parent
        clip: true
        contentHeight: col.height + Theme.spacingXL
        contentWidth: width

        Column {
            id: col
            topPadding: 4
            width: Math.min(600, parent.width - Theme.spacingL * 2)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingL

            PlaceholderCard {
                title: I18n.tr("Visual Alerts")
                iconName: "flash_on"
                note: I18n.tr("Flash the screen when an alert sound plays. Planned — would be a custom shell effect.")
            }

            PlaceholderCard {
                title: I18n.tr("Mono Audio")
                iconName: "hearing"
                note: I18n.tr("Combine stereo channels into one. Planned — likely via the audio backend.")
            }
        }
    }
}
