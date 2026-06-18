import QtQuick
import qs.Common
import qs.Services
import qs.Widgets
import qs.Modules.Settings.Widgets

Item {
    HGSFlickable {
        anchors.fill: parent
        clip: true
        contentHeight: layoutColumn.height + Theme.spacingXL
        contentWidth: width

        Column {
            id: layoutColumn

            topPadding: Theme.spacingXL
            bottomPadding: Theme.spacingXL
            width: Math.min(550, parent.width - Theme.spacingL * 2)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingXL

            SettingsCard {
                width: parent.width
                tags: ["hyprland", "layout", "gaps", "radius", "window", "border", "rounding"]
                title: I18n.tr("Hyprland Layout Overrides")
                settingKey: "hyprlandLayout"
                iconName: "crop_square"
                SettingsToggleRow {
                    tags: ["hyprland", "gaps", "override"]
                    settingKey: "hyprlandLayoutGapsOverrideEnabled"
                    text: I18n.tr("Override Gaps")
                    description: I18n.tr("Use custom gaps instead of bar spacing")
                    checked: SettingsData.hyprlandLayoutGapsOverride >= 0
                    onToggled: checked => {
                        if (checked) {
                            const currentGaps = Math.max(4, (SettingsData.barConfigs[0]?.spacing ?? 4));
                            SettingsData.set("hyprlandLayoutGapsOverride", currentGaps);
                            return;
                        }
                        SettingsData.set("hyprlandLayoutGapsOverride", -1);
                    }
                }

                SettingsSliderRow {
                    tags: ["hyprland", "gaps", "override"]
                    settingKey: "hyprlandLayoutGapsOverride"
                    text: I18n.tr("Window Gaps")
                    description: I18n.tr("Space between windows") + " (gaps_in/gaps_out)"
                    visible: SettingsData.hyprlandLayoutGapsOverride >= 0
                    value: Math.max(0, SettingsData.hyprlandLayoutGapsOverride)
                    minimum: 0
                    maximum: 50
                    unit: "px"
                    defaultValue: Math.max(4, (SettingsData.barConfigs[0]?.spacing ?? 4))
                    onSliderValueChanged: newValue => SettingsData.set("hyprlandLayoutGapsOverride", newValue)
                }

                SettingsToggleRow {
                    tags: ["hyprland", "radius", "override", "rounding"]
                    settingKey: "hyprlandLayoutRadiusOverrideEnabled"
                    text: I18n.tr("Override Corner Radius")
                    description: I18n.tr("Use custom window radius instead of theme radius")
                    checked: SettingsData.hyprlandLayoutRadiusOverride >= 0
                    onToggled: checked => {
                        if (checked) {
                            SettingsData.set("hyprlandLayoutRadiusOverride", SettingsData.cornerRadius);
                            return;
                        }
                        SettingsData.set("hyprlandLayoutRadiusOverride", -1);
                    }
                }

                SettingsSliderRow {
                    tags: ["hyprland", "radius", "override", "rounding"]
                    settingKey: "hyprlandLayoutRadiusOverride"
                    text: I18n.tr("Window Corner Radius")
                    description: I18n.tr("Rounded corners for windows") + " (decoration.rounding)"
                    visible: SettingsData.hyprlandLayoutRadiusOverride >= 0
                    value: Math.max(0, SettingsData.hyprlandLayoutRadiusOverride)
                    minimum: 0
                    maximum: 100
                    unit: "px"
                    defaultValue: SettingsData.cornerRadius
                    onSliderValueChanged: newValue => SettingsData.set("hyprlandLayoutRadiusOverride", newValue)
                }

                SettingsToggleRow {
                    tags: ["hyprland", "border", "override"]
                    settingKey: "hyprlandLayoutBorderSizeEnabled"
                    text: I18n.tr("Override Border Size")
                    description: I18n.tr("Use custom border size")
                    checked: SettingsData.hyprlandLayoutBorderSize >= 0
                    onToggled: checked => {
                        if (checked) {
                            SettingsData.set("hyprlandLayoutBorderSize", 2);
                            return;
                        }
                        SettingsData.set("hyprlandLayoutBorderSize", -1);
                    }
                }

                SettingsSliderRow {
                    tags: ["hyprland", "border", "override"]
                    settingKey: "hyprlandLayoutBorderSize"
                    text: I18n.tr("Border Size")
                    description: I18n.tr("Width of window border") + " (general.border_size)"
                    visible: SettingsData.hyprlandLayoutBorderSize >= 0
                    value: Math.max(0, SettingsData.hyprlandLayoutBorderSize)
                    minimum: 0
                    maximum: 10
                    unit: "px"
                    defaultValue: 2
                    onSliderValueChanged: newValue => SettingsData.set("hyprlandLayoutBorderSize", newValue)
                }

                SettingsToggleRow {
                    tags: ["hyprland", "resize", "border", "mouse", "drag"]
                    settingKey: "hyprlandResizeOnBorder"
                    text: I18n.tr("Resize on Border")
                    description: I18n.tr("Resize windows by dragging their edges with the mouse")
                    checked: SettingsData.hyprlandResizeOnBorder
                    onToggled: checked => SettingsData.set("hyprlandResizeOnBorder", checked)
                }
            }
        }
    }
}
