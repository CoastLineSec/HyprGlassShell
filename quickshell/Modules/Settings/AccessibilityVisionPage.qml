pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Widgets

// Vision / Seeing accessibility. Mixes real backends (Hyprland magnifier, the
// shell's own font scale + animation speed) with dependency-gated and
// placeholder entries. Reused controls are duplicated here, not moved.
Item {
    id: page

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    // Live magnifier factor read from Hyprland (cursor:zoom_factor).
    property int zoomPercent: 100

    function readZoom() {
        Proc.runCommand("a11y-zoom-get", ["sh", "-c", "hyprctl getoption -j cursor:zoom_factor 2>/dev/null"], (out, code) => {
            if (code !== 0 || !out)
                return;
            try {
                var f = JSON.parse(out).float;
                if (typeof f === "number" && f > 0)
                    page.zoomPercent = Math.round(f * 100);
            } catch (e) {}
        });
    }

    function applyZoom(percent) {
        page.zoomPercent = percent;
        Quickshell.execDetached(["hyprctl", "keyword", "cursor:zoom_factor", (percent / 100).toFixed(2)]);
    }

    Component.onCompleted: readZoom()

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

            // Magnifier — Hyprland native (cursor:zoom_factor).
            SettingsCard {
                title: I18n.tr("Zoom")
                iconName: "zoom_in"

                SettingsSliderRow {
                    text: I18n.tr("Magnification")
                    description: I18n.tr("Zoom the screen around the pointer. 100% turns it off.")
                    minimum: 100
                    maximum: 400
                    value: page.zoomPercent
                    unit: "%"
                    defaultValue: 100
                    onSliderValueChanged: newValue => page.applyZoom(newValue)
                }
            }

            // Larger Text — duplicate of the shell font scale (Typography & Motion).
            SettingsCard {
                title: I18n.tr("Larger Text")
                iconName: "format_size"

                SettingsSliderRow {
                    text: I18n.tr("Font Scale")
                    description: I18n.tr("Resize all fonts throughout HGS")
                    minimum: 75
                    maximum: 150
                    value: Math.round(SettingsData.fontScale * 100)
                    unit: "%"
                    defaultValue: 100
                    onSliderValueChanged: newValue => SettingsData.set("fontScale", newValue / 100)
                }
            }

            // Reduce Motion — duplicate of Animation Speed (Typography & Motion).
            SettingsCard {
                title: I18n.tr("Reduce Motion")
                iconName: "animation"

                Item {
                    width: parent.width
                    height: motionGroup.implicitHeight
                    clip: true

                    HGSButtonGroup {
                        id: motionGroup
                        anchors.horizontalCenter: parent.horizontalCenter
                        buttonPadding: parent.width < 480 ? Theme.spacingS : Theme.spacingL
                        minButtonWidth: parent.width < 480 ? 44 : 64
                        textSize: parent.width < 480 ? Theme.fontSizeSmall : Theme.fontSizeMedium
                        model: [I18n.tr("None"), I18n.tr("Short"), I18n.tr("Medium"), I18n.tr("Long"), I18n.tr("Custom")]
                        selectionMode: "single"
                        currentIndex: SettingsData.animationSpeed
                        onSelectionChanged: (index, selected) => {
                            if (!selected)
                                return;
                            SettingsData.set("animationSpeed", index);
                        }

                        Connections {
                            target: SettingsData
                            function onAnimationSpeedChanged() {
                                motionGroup.currentIndex = SettingsData.animationSpeed;
                            }
                        }
                    }
                }

                StyledText {
                    width: parent.width
                    text: I18n.tr("Choose “None” to disable animations throughout the shell.")
                    color: Theme.surfaceVariantText
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }
            }

            // Screen reader — needs Orca (dependency-gated).
            SettingsCard {
                title: I18n.tr("Screen Reader")
                iconName: "record_voice_over"

                DependencyGate {
                    toolKey: "orca"

                    SettingsToggleCard {
                        width: parent.width
                        title: I18n.tr("Enable Screen Reader")
                        description: I18n.tr("Read on-screen elements aloud using Orca")
                        iconName: "record_voice_over"
                        checked: false
                        onToggled: checked => {
                            if (checked)
                                Quickshell.execDetached(["sh", "-c", "orca &"]);
                            else
                                Quickshell.execDetached(["pkill", "-x", "orca"]);
                        }
                    }
                }
            }

            // Not yet implemented — placeholders for later research.
            PlaceholderCard {
                title: I18n.tr("Increase Contrast")
                iconName: "contrast"
            }

            PlaceholderCard {
                title: I18n.tr("Color Filters")
                iconName: "palette"
                note: I18n.tr("Color-blindness filters are planned — likely via a gamma/shader pass.")
            }

            PlaceholderCard {
                title: I18n.tr("Reduce Transparency")
                iconName: "opacity"
                note: I18n.tr("A single global transparency reduction is planned for a future update.")
            }
        }
    }
}
