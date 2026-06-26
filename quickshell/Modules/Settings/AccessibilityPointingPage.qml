pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Widgets

// Pointing & Clicking accessibility. Cursor size is real (Hyprland setcursor);
// mouse keys and click assist are placeholders — not exposed by Hyprland.
Item {
    id: page

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    property int cursorSize: 24

    function readCursor() {
        Proc.runCommand("a11y-cursor-get", ["sh", "-c", "echo ${XCURSOR_SIZE:-24}"], (out, code) => {
            if (code !== 0 || !out)
                return;
            var v = parseInt((out || "").trim());
            if (v > 0)
                page.cursorSize = v;
        });
    }

    function applyCursor(v) {
        page.cursorSize = v;
        // Re-apply with the active theme; fall back to a common default.
        Quickshell.execDetached(["sh", "-c", "hyprctl setcursor \"${XCURSOR_THEME:-Adwaita}\" " + v]);
    }

    Component.onCompleted: readCursor()

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

            // Cursor size — Hyprland native (hyprctl setcursor).
            SettingsCard {
                title: I18n.tr("Pointer Size")
                iconName: "mouse"

                SettingsSliderRow {
                    text: I18n.tr("Cursor Size")
                    description: I18n.tr("Make the mouse pointer larger and easier to find")
                    minimum: 16
                    maximum: 96
                    value: page.cursorSize
                    unit: "px"
                    defaultValue: 24
                    onSliderValueChanged: newValue => page.applyCursor(newValue)
                }
            }

            // Not exposed by Hyprland yet — placeholders.
            PlaceholderCard {
                title: I18n.tr("Mouse Keys")
                iconName: "dialpad"
                note: I18n.tr("Move the pointer with the numeric keypad. Hyprland has no control for this yet.")
            }

            PlaceholderCard {
                title: I18n.tr("Click Assist")
                iconName: "ads_click"
                note: I18n.tr("Hover-to-click and simulated secondary click. Hyprland has no control for this yet.")
            }
        }
    }
}
