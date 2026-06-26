pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Services
import qs.Widgets

// Battery settings — laptop only (the sidebar gates this on BatteryService.batteryAvailable).
// Live status + the battery-specific controls relocated here from Power & Sleep
// (per-source power profile, charge limit).
Item {
    id: batteryTab

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    readonly property var profileOptions: [I18n.tr("Don't Change"), Theme.getPowerProfileLabel(0), Theme.getPowerProfileLabel(1), Theme.getPowerProfileLabel(2)]
    readonly property var profileValues: ["", "0", "1", "2"]

    HGSFlickable {
        anchors.fill: parent
        clip: true
        contentHeight: mainColumn.height + Theme.spacingXL
        contentWidth: width

        Column {
            id: mainColumn

            topPadding: 4
            width: Math.min(600, parent.width - Theme.spacingL * 2)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingL

            // === Status ===
            SettingsCard {
                width: parent.width
                title: I18n.tr("Battery")
                iconName: "battery_full"

                Column {
                    width: parent.width
                    spacing: Theme.spacingM

                    Row {
                        width: parent.width
                        spacing: Theme.spacingM

                        StyledText {
                            text: Math.round(BatteryService.batteryLevel) + "%"
                            color: BatteryService.isLowBattery ? Theme.error : Theme.surfaceText
                            font.pixelSize: Theme.fontSizeXLarge
                            font.weight: Font.Medium
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        StyledText {
                            text: BatteryService.isCharging ? I18n.tr("Charging") : (BatteryService.isPluggedIn ? I18n.tr("Plugged in, not charging") : I18n.tr("On battery"))
                            color: BatteryService.isCharging ? Theme.success : Theme.surfaceVariantText
                            font.pixelSize: Theme.fontSizeMedium
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Repeater {
                        model: [
                            {
                                "label": I18n.tr("Condition"),
                                "value": batteryTab.batteryHealthText()
                            },
                            {
                                "label": I18n.tr("Power source"),
                                "value": BatteryService.isPluggedIn ? I18n.tr("Power Adapter") : I18n.tr("Battery")
                            }
                        ]

                        delegate: Row {
                            id: statRow
                            required property var modelData
                            width: parent.width
                            visible: (statRow.modelData.value || "").length > 0
                            spacing: Theme.spacingM

                            StyledText {
                                text: statRow.modelData.label
                                color: Theme.surfaceVariantText
                                font.pixelSize: Theme.fontSizeSmall
                                width: 150
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            StyledText {
                                text: statRow.modelData.value
                                color: Theme.surfaceText
                                font.pixelSize: Theme.fontSizeSmall
                                width: parent.width - 150 - Theme.spacingM
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }

            // === Power Mode (per source) — relocated from Power & Sleep ===
            SettingsCard {
                width: parent.width
                title: I18n.tr("Power Mode")
                iconName: "bolt"

                SettingsDropdownRow {
                    id: acProfileDropdown
                    settingKey: "acProfileName"
                    tags: ["power", "profile", "performance", "balanced", "saver", "ac"]
                    width: parent.width
                    addHorizontalPadding: true
                    text: I18n.tr("Profile on power adapter")
                    options: batteryTab.profileOptions

                    Component.onCompleted: {
                        const idx = batteryTab.profileValues.indexOf(SettingsData.acProfileName);
                        currentValue = batteryTab.profileOptions[idx >= 0 ? idx : 0];
                    }
                    onValueChanged: value => {
                        const idx = batteryTab.profileOptions.indexOf(value);
                        if (idx >= 0)
                            SettingsData.set("acProfileName", batteryTab.profileValues[idx]);
                    }
                }

                SettingsDropdownRow {
                    id: batteryProfileDropdown
                    settingKey: "batteryProfileName"
                    tags: ["power", "profile", "performance", "balanced", "saver", "battery"]
                    width: parent.width
                    addHorizontalPadding: true
                    text: I18n.tr("Profile on battery")
                    options: batteryTab.profileOptions

                    Component.onCompleted: {
                        const idx = batteryTab.profileValues.indexOf(SettingsData.batteryProfileName);
                        currentValue = batteryTab.profileOptions[idx >= 0 ? idx : 0];
                    }
                    onValueChanged: value => {
                        const idx = batteryTab.profileOptions.indexOf(value);
                        if (idx >= 0)
                            SettingsData.set("batteryProfileName", batteryTab.profileValues[idx]);
                    }
                }
            }

            // === Charge limit — relocated from Power & Sleep ===
            SettingsCard {
                width: parent.width
                title: I18n.tr("Charge Limit")
                iconName: "battery_saver"

                SettingsSliderRow {
                    settingKey: "batteryChargeLimit"
                    tags: ["battery", "charge", "limit", "percentage", "power"]
                    text: I18n.tr("Battery Charge Limit")
                    description: I18n.tr("Note: this only changes the percentage, it does not actually limit charging.")
                    value: SettingsData.batteryChargeLimit
                    minimum: 50
                    maximum: 100
                    defaultValue: 100
                    onSliderValueChanged: newValue => SettingsData.set("batteryChargeLimit", newValue)
                }
            }
        }
    }

    function batteryHealthText() {
        const h = BatteryService.batteryHealth;
        return (h && h.length > 0) ? h : "—";
    }
}
