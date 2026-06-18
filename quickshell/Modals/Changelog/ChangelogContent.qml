import QtQuick
import QtQuick.Effects
import qs.Common
import qs.Services
import qs.Widgets

Column {
    id: root

    readonly property real logoSize: Math.round(Theme.iconSize * 2.8)
    readonly property real badgeHeight: Math.round(Theme.fontSizeSmall * 1.7)

    topPadding: Theme.spacingL
    spacing: Theme.spacingL

    Column {
        width: parent.width
        spacing: Theme.spacingM

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingM

            Image {
                width: root.logoSize
                height: width * (569.94629 / 506.50931)
                anchors.verticalCenter: parent.verticalCenter
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                asynchronous: true
                source: "file://" + Theme.shellDir + "/assets/hgslogonormal.svg"
                layer.enabled: true
                layer.smooth: true
                layer.mipmap: true
                layer.effect: MultiEffect {
                    saturation: 0
                    colorization: 1
                    colorizationColor: Theme.primary
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.spacingXS

                Row {
                    spacing: Theme.spacingS

                    StyledText {
                        text: "HGS " + ChangelogService.currentVersion
                        font.pixelSize: Theme.fontSizeXLarge + 2
                        font.weight: Font.Bold
                        color: Theme.surfaceText
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: codenameText.implicitWidth + Theme.spacingM * 2
                        height: root.badgeHeight
                        radius: root.badgeHeight / 2
                        color: Theme.primaryContainer
                        anchors.verticalCenter: parent.verticalCenter

                        StyledText {
                            id: codenameText
                            anchors.centerIn: parent
                            text: "Saffron Bloom"
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.Medium
                            color: Theme.primary
                        }
                    }
                }

                StyledText {
                    text: "New launcher, enhanced plugin system, KDE Connect, & more"
                    font.pixelSize: Theme.fontSizeMedium
                    color: Theme.surfaceVariantText
                }
            }
        }
    }

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.outlineMedium
        opacity: 0.3
    }

    Column {
        width: parent.width
        spacing: Theme.spacingM

        StyledText {
            text: "What's New"
            font.pixelSize: Theme.fontSizeMedium
            font.weight: Font.Medium
            color: Theme.surfaceText
        }

        Grid {
            width: parent.width
            columns: 2
            rowSpacing: Theme.spacingS
            columnSpacing: Theme.spacingS

            ChangelogFeatureCard {
                width: (parent.width - Theme.spacingS) / 2
                iconName: "space_dashboard"
                title: "HGS Launcher V2"
                description: "New capabilities and actions"
                onClicked: PopoutService.openHGSLauncherV2()
            }

            ChangelogFeatureCard {
                width: (parent.width - Theme.spacingS) / 2
                iconName: "smartphone"
                title: "Phone Connect"
                description: "KDE Connect & Valent"
                onClicked: PopoutService.openSettingsWithTab("network")
            }

            ChangelogFeatureCard {
                width: (parent.width - Theme.spacingS) / 2
                iconName: "monitor_heart"
                title: "System Monitor"
                description: "Redesigned process list"
                onClicked: PopoutService.showProcessListModal()
            }

            ChangelogFeatureCard {
                width: (parent.width - Theme.spacingS) / 2
                iconName: "window"
                title: "Window Rules"
                description: "Hyprland window rule manager"
                onClicked: PopoutService.openSettingsWithTab("window_rules")
            }

            ChangelogFeatureCard {
                width: (parent.width - Theme.spacingS) / 2
                iconName: "notifications_active"
                title: "Enhanced Notifications"
                description: "Configurable rules & styling"
                onClicked: PopoutService.openSettingsWithTab("notifications")
            }

            ChangelogFeatureCard {
                width: (parent.width - Theme.spacingS) / 2
                iconName: "dock_to_bottom"
                title: "Dock Enhancements"
                description: "Bar dock widget & more"
                onClicked: PopoutService.openSettingsWithTab("dock")
            }

            ChangelogFeatureCard {
                width: (parent.width - Theme.spacingS) / 2
                iconName: "volume_up"
                title: "Audio Aliases"
                description: "Custom device names"
                onClicked: PopoutService.openSettingsWithTab("audio")
            }

            ChangelogFeatureCard {
                width: (parent.width - Theme.spacingS) / 2
                iconName: "settings_applications"
                title: "Hyprland Controls"
                description: "Compositor-focused settings"
                onClicked: PopoutService.openSettingsWithTab("compositor")
            }

            ChangelogFeatureCard {
                width: (parent.width - Theme.spacingS) / 2
                iconName: "light_mode"
                title: "Auto Light/Dark"
                description: "Automatic mode switching"
                onClicked: PopoutService.openSettingsWithTab("theme")
            }
        }
    }

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.outlineMedium
        opacity: 0.3
    }

    Column {
        width: parent.width
        spacing: Theme.spacingS

        Row {
            spacing: Theme.spacingS

            HGSIcon {
                name: "warning"
                size: Theme.iconSizeSmall
                color: Theme.warning
                anchors.verticalCenter: parent.verticalCenter
            }

            StyledText {
                text: "Upgrade Notes"
                font.pixelSize: Theme.fontSizeMedium
                font.weight: Font.Medium
                color: Theme.surfaceText
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Rectangle {
            width: parent.width
            height: upgradeNotesColumn.height + Theme.spacingM * 2
            radius: Theme.cornerRadius
            color: Theme.withAlpha(Theme.warning, 0.08)
            border.width: 1
            border.color: Theme.withAlpha(Theme.warning, 0.2)

            Column {
                id: upgradeNotesColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.spacingM
                spacing: Theme.spacingS

                ChangelogUpgradeNote {
                    width: parent.width
                    text: "Spotlight replaced by HGS Launcher V2 — check settings for new options"
                }

                ChangelogUpgradeNote {
                    width: parent.width
                    text: "External plugin registry access is disabled in HyprGlassShell"
                }
            }
        }

        // StyledText {
        //     text: "See full release notes for migration steps"
        //     font.pixelSize: Theme.fontSizeSmall
        //     color: Theme.surfaceVariantText
        //     width: parent.width
        // }
    }
}
