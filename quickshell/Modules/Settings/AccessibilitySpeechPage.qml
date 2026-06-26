pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Widgets

// Speech accessibility. No backend yet — placeholder to research later.
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
                title: I18n.tr("Spoken Content")
                iconName: "campaign"
                note: I18n.tr("Speak selected text or notifications aloud. Planned — likely via speech-dispatcher.")
            }

            PlaceholderCard {
                title: I18n.tr("Live Captions")
                iconName: "closed_caption"
                note: I18n.tr("On-screen captions for spoken audio. Planned — needs a speech-to-text backend.")
            }
        }
    }
}
