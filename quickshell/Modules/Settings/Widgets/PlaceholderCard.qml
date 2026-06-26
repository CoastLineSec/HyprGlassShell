import QtQuick
import qs.Common
import qs.Widgets

// A settings card for accessibility features that have no implementation yet —
// keeps the macOS-style layout complete while we research backends.
SettingsCard {
    id: card

    property string note: I18n.tr("Not yet available — planned for a future update.")

    StyledText {
        width: parent.width
        text: card.note
        color: Theme.surfaceVariantText
        font.pixelSize: Theme.fontSizeSmall
        wrapMode: Text.WordWrap
    }
}
