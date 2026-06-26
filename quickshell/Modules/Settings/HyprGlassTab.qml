import QtQuick
import qs.Common
import qs.Services
import qs.Widgets
import qs.Modules.Settings.Widgets

// macOS-style "HyprGlass" settings page (top-level sidebar entry, sits under
// Appearance). Owns the Fluid Glass compositor material and the Advanced
// "Fluid Glass Labs" tuning drill-in.
Item {
    id: hyprGlassTab

    property var parentModal: null

    // "" = main pane; "advanced" = Advanced Fluid Glass Settings (Fluid Glass Labs).
    property string sub: ""
    onVisibleChanged: {
        if (!visible)
            sub = "";
    }

    // ===== Main pane =====
    HGSFlickable {
        anchors.fill: parent
        visible: hyprGlassTab.sub === ""
        clip: true
        contentHeight: mainColumn.height + Theme.spacingXL
        contentWidth: width

        Column {
            id: mainColumn
            topPadding: 4
            width: Math.min(550, parent.width - Theme.spacingL * 2)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingXL

            SettingsCard {
                id: fluidGlassCard
                tab: "hyprglass"
                tags: ["hyprglass", "glass", "fluid", "frost", "tint", "color", "light", "mouse"]
                title: I18n.tr("Fluid Glass")
                settingKey: "hyprGlassAppearance"
                iconName: "water_drop"

                // Master toggle — enables/disables the compositor plugin material.
                SettingsToggleRow {
                    tab: "hyprglass"
                    tags: ["fluid", "glass", "hyprglass", "plugin", "material", "enable"]
                    settingKey: "fluidGlassEnabled"
                    text: I18n.tr("Fluid Glass")
                    description: I18n.tr("Render bars and panels with live glass refraction over whatever sits behind them. Off keeps the standard surfaces.")
                    checked: SettingsData.fluidGlassEnabled ?? false
                    onToggled: checked => SettingsData.set("fluidGlassEnabled", checked)
                }

                // Collapsible body — revealed only while Fluid Glass is enabled.
                Item {
                    id: revealWrap
                    width: parent.width
                    clip: true
                    readonly property bool shown: SettingsData.fluidGlassEnabled ?? false
                    height: shown ? revealCol.implicitHeight : 0
                    opacity: shown ? 1 : 0

                    Behavior on height {
                        NumberAnimation {
                            duration: Theme.mediumDuration
                            easing.type: Theme.emphasizedEasing
                        }
                    }
                    Behavior on opacity {
                        NumberAnimation {
                            duration: Theme.shortDuration
                            easing.type: Theme.standardEasing
                        }
                    }

                    Column {
                        id: revealCol
                        width: parent.width
                        spacing: Theme.spacingM

                        // --- Dynamic Mouse Light ---
                        SettingsToggleRow {
                            tab: "hyprglass"
                            tags: ["dynamic", "light", "mouse", "tracking", "gyroscope", "highlight"]
                            settingKey: "fluidGlassDynamicLight"
                            text: I18n.tr("Dynamic Mouse Light")
                            description: I18n.tr("The highlight follows the cursor — expanding, tightening, brightening and dimming with distance, like light catching the glass. Off uses a fixed light angle.")
                            checked: SettingsData.fluidGlassDynamicLight ?? true
                            onToggled: checked => SettingsData.set("fluidGlassDynamicLight", checked)
                        }

                        // --- Stained Glass ---
                        SettingsToggleRow {
                            tab: "hyprglass"
                            tags: ["stained", "glass", "tint", "color"]
                            settingKey: "fluidGlassStained"
                            text: I18n.tr("Stained Glass")
                            description: I18n.tr("Tint the glass with a color. Off keeps the glass clear and neutral.")
                            checked: SettingsData.fluidGlassStained ?? false
                            onToggled: checked => SettingsData.set("fluidGlassStained", checked)
                        }

                        // Stained Glass color source — revealed when Stained Glass is on.
                        SettingsButtonGroupRow {
                            id: sourceRow
                            visible: SettingsData.fluidGlassStained ?? false
                            tab: "hyprglass"
                            tags: ["tint", "source", "system", "appearance", "theme", "color"]
                            text: I18n.tr("Color Source")
                            description: I18n.tr("System Appearance follows light/dark mode; Theme Color uses your Theme color.")
                            model: [I18n.tr("System Appearance"), I18n.tr("Theme Color")]
                            currentIndex: (SettingsData.fluidGlassTintSource === "theme") ? 1 : 0
                            onSelectionChanged: (index, selected) => {
                                if (!selected)
                                    return;
                                SettingsData.set("fluidGlassTintSource", index === 1 ? "theme" : "system");
                            }
                        }

                        // --- Frosted Glass (combined blur + tint level) ---
                        Column {
                            width: parent.width - Theme.spacingM * 2
                            x: Theme.spacingM
                            spacing: Theme.spacingXS

                            StyledText {
                                text: I18n.tr("Frosted Glass")
                                font.pixelSize: Theme.fontSizeMedium
                                font.weight: Font.Medium
                                color: Theme.surfaceText
                                width: parent.width
                                horizontalAlignment: Text.AlignLeft
                            }
                            StyledText {
                                text: I18n.tr("How much the glass blurs and thickens what's behind it.")
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.surfaceVariantText
                                wrapMode: Text.WordWrap
                                width: parent.width
                                horizontalAlignment: Text.AlignLeft
                            }

                            HGSSlider {
                                id: frostSlider
                                width: parent.width
                                height: 32
                                minimum: 0
                                maximum: 100
                                showValue: false
                                wheelEnabled: false
                                thumbOutlineColor: Theme.surfaceContainerHigh
                                value: Math.round((SettingsData.fluidGlassLevel ?? 0.5) * 100)
                                onSliderValueChanged: newValue => SettingsData.set("fluidGlassLevel", newValue / 100)
                                onSliderDragFinished: finalValue => SettingsData.set("fluidGlassLevel", finalValue / 100)
                            }

                            // Low / Medium / High reference points under the slider.
                            Item {
                                width: parent.width
                                height: lowLbl.implicitHeight

                                StyledText {
                                    id: lowLbl
                                    anchors.left: parent.left
                                    text: I18n.tr("Low")
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: Theme.surfaceVariantText
                                }
                                StyledText {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: I18n.tr("Medium")
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: Theme.surfaceVariantText
                                }
                                StyledText {
                                    anchors.right: parent.right
                                    text: I18n.tr("High")
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: Theme.surfaceVariantText
                                }
                            }

                            // Custom-values notice — adjusting frost/tint in the Labs flips
                            // fluidGlassFrosted off; the preset slider stops driving until re-enabled.
                            StyledText {
                                visible: !(SettingsData.fluidGlassFrosted ?? true)
                                width: parent.width
                                text: I18n.tr("Custom values have replaced default settings. Enabling Frosted Glass will reset custom values.")
                                wrapMode: Text.WordWrap
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.warning
                            }
                            HGSButton {
                                visible: !(SettingsData.fluidGlassFrosted ?? true)
                                text: I18n.tr("Enable Frosted Glass")
                                iconName: "ac_unit"
                                onClicked: {
                                    SettingsData.set("fluidGlassBlurCustom", SettingsData.fluidGlassLevel ?? 0.5);
                                    SettingsData.set("fluidGlassTintCustom", SettingsData.fluidGlassLevel ?? 0.5);
                                    SettingsData.set("fluidGlassFrosted", true);
                                }
                            }
                        }

                        // Divider.
                        Rectangle {
                            width: parent.width - Theme.spacingM * 2
                            x: Theme.spacingM
                            height: 1
                            color: Theme.outline
                            opacity: 0.15
                        }

                        // Advanced Settings — opens the Fluid Glass Labs.
                        Item {
                            x: Theme.spacingM
                            width: parent.width - Theme.spacingM * 2
                            height: advLink.implicitHeight

                            StyledText {
                                id: advLink
                                anchors.right: parent.right
                                text: I18n.tr("Advanced Settings")
                                font.pixelSize: Theme.fontSizeSmall
                                font.weight: Font.Medium
                                color: Theme.primary

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: hyprGlassTab.sub = "advanced"
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== Advanced Fluid Glass Settings (Fluid Glass Labs) — drill-in =====
    Item {
        anchors.fill: parent
        visible: hyprGlassTab.sub === "advanced"

        Item {
            id: advBackBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 44

            HGSActionButton {
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingM
                anchors.verticalCenter: parent.verticalCenter
                circular: false
                iconName: "arrow_back"
                iconColor: Theme.surfaceText
                onClicked: hyprGlassTab.sub = ""
            }

            StyledText {
                anchors.centerIn: parent
                text: I18n.tr("Advanced Fluid Glass Settings")
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.Medium
                color: Theme.surfaceText
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.outline
                opacity: 0.12
            }
        }

        // Fluid Glass Labs fills the area below the back-bar and owns its own
        // (right-column) scroll, so the preview stays pinned on the left.
        FluidGlassLabs {
            anchors.top: advBackBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.topMargin: Theme.spacingS
            anchors.leftMargin: Theme.spacingS
            anchors.rightMargin: Theme.spacingS
            anchors.bottomMargin: Theme.spacingS
            parentModal: hyprGlassTab.parentModal
        }
    }
}
