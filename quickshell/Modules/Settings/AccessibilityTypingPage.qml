pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Widgets

// Typing accessibility. Key repeat is real (Hyprland input:repeat_*), the
// on-screen keyboard is dependency-gated, and AccessX features (sticky/slow/
// bounce keys) are placeholders — Hyprland exposes no control for them.
Item {
    id: page

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    property int repeatRate: 25
    property int repeatDelay: 600

    function readInts() {
        Proc.runCommand("a11y-rrate-get", ["sh", "-c", "hyprctl getoption -j input:repeat_rate 2>/dev/null"], (out, code) => {
            if (code === 0 && out) {
                try {
                    var v = JSON.parse(out).int;
                    if (typeof v === "number" && v > 0)
                        page.repeatRate = v;
                } catch (e) {}
            }
        });
        Proc.runCommand("a11y-rdelay-get", ["sh", "-c", "hyprctl getoption -j input:repeat_delay 2>/dev/null"], (out, code) => {
            if (code === 0 && out) {
                try {
                    var v = JSON.parse(out).int;
                    if (typeof v === "number" && v > 0)
                        page.repeatDelay = v;
                } catch (e) {}
            }
        });
    }

    function applyRate(v) {
        page.repeatRate = v;
        Quickshell.execDetached(["hyprctl", "keyword", "input:repeat_rate", String(v)]);
    }

    function applyDelay(v) {
        page.repeatDelay = v;
        Quickshell.execDetached(["hyprctl", "keyword", "input:repeat_delay", String(v)]);
    }

    Component.onCompleted: readInts()

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

            // Key repeat — Hyprland native (input:repeat_delay / input:repeat_rate).
            SettingsCard {
                title: I18n.tr("Key Repeat")
                iconName: "keyboard"

                SettingsSliderRow {
                    text: I18n.tr("Repeat Delay")
                    description: I18n.tr("How long to hold a key before it starts repeating")
                    minimum: 150
                    maximum: 2000
                    value: page.repeatDelay
                    unit: "ms"
                    defaultValue: 600
                    onSliderValueChanged: newValue => page.applyDelay(newValue)
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.outline
                    opacity: 0.15
                }

                SettingsSliderRow {
                    text: I18n.tr("Repeat Rate")
                    description: I18n.tr("How fast a held key repeats (characters per second)")
                    minimum: 1
                    maximum: 60
                    value: page.repeatRate
                    unit: "/s"
                    defaultValue: 25
                    onSliderValueChanged: newValue => page.applyRate(newValue)
                }
            }

            // On-screen keyboard — needs wvkbd (dependency-gated).
            SettingsCard {
                title: I18n.tr("On-Screen Keyboard")
                iconName: "keyboard_alt"

                DependencyGate {
                    toolKey: "osk"

                    SettingsToggleCard {
                        width: parent.width
                        title: I18n.tr("Show On-Screen Keyboard")
                        description: I18n.tr("Display a virtual keyboard for pointer-only input")
                        iconName: "keyboard_alt"
                        checked: false
                        onToggled: checked => {
                            if (checked)
                                Quickshell.execDetached(["sh", "-c", "wvkbd-mobintl &"]);
                            else
                                Quickshell.execDetached(["pkill", "-x", "wvkbd-mobintl"]);
                        }
                    }
                }
            }

            // AccessX — Hyprland has no control for these yet.
            PlaceholderCard {
                title: I18n.tr("Sticky Keys")
                iconName: "filter_none"
                note: I18n.tr("Press modifier keys one at a time. Hyprland has no control for this yet.")
            }

            PlaceholderCard {
                title: I18n.tr("Slow Keys")
                iconName: "hourglass_empty"
                note: I18n.tr("Add a delay before a keypress registers. Hyprland has no control for this yet.")
            }

            PlaceholderCard {
                title: I18n.tr("Bounce Keys")
                iconName: "block"
                note: I18n.tr("Ignore rapid duplicate keypresses. Hyprland has no control for this yet.")
            }
        }
    }
}
