pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Services
import qs.Widgets

// Date & Time page: live clock + time zone + automatic time (NTP), read from
// `timedatectl`. The NTP toggle writes via `timedatectl set-ntp` (polkit-authorized).
Item {
    id: dtPage

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    property var info: ({})
    property string clock: ""

    function refresh() {
        Proc.runCommand("timedatectl-show", ["sh", "-c", "timedatectl show 2>/dev/null"], (output, code) => {
            if (code !== 0 || !output)
                return;
            const obj = {};
            const lines = output.split("\n");
            for (let i = 0; i < lines.length; i++) {
                const idx = lines[i].indexOf("=");
                if (idx > 0)
                    obj[lines[i].substring(0, idx).trim()] = lines[i].substring(idx + 1).trim();
            }
            dtPage.info = obj;
        });
    }

    function setNtp(enabled) {
        Proc.runCommand("timedatectl-ntp", ["sh", "-c", "timedatectl set-ntp " + (enabled ? "true" : "false")], (output, code) => {
            ToastService.showInfo(code === 0 ? (enabled ? I18n.tr("Automatic time enabled") : I18n.tr("Automatic time disabled")) : I18n.tr("Couldn't change time settings"));
            Qt.callLater(dtPage.refresh);
        });
    }

    function tick() {
        dtPage.clock = Qt.formatDateTime(new Date(), "dddd, MMMM d, yyyy   h:mm:ss AP");
    }

    readonly property bool ntpOn: (info["NTP"] || "") === "yes"
    readonly property bool ntpSynced: (info["NTPSynchronized"] || "") === "yes"

    Component.onCompleted: {
        tick();
        refresh();
    }

    Timer {
        interval: 1000
        running: dtPage.visible
        repeat: true
        onTriggered: dtPage.tick()
    }
    Timer {
        interval: 10000
        running: dtPage.visible
        repeat: true
        onTriggered: dtPage.refresh()
    }

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

            // Clock + zone.
            SettingsCard {
                width: parent.width
                title: I18n.tr("Date & Time")
                iconName: "schedule"

                Column {
                    width: parent.width
                    spacing: Theme.spacingS

                    StyledText {
                        text: dtPage.clock
                        color: Theme.surfaceText
                        font.pixelSize: Theme.fontSizeLarge
                        font.weight: Font.Medium
                    }

                    Row {
                        width: parent.width
                        spacing: Theme.spacingM

                        StyledText {
                            text: I18n.tr("Time zone")
                            color: Theme.surfaceVariantText
                            font.pixelSize: Theme.fontSizeSmall
                            width: 150
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        StyledText {
                            text: dtPage.info["Timezone"] || "—"
                            color: Theme.surfaceText
                            font.pixelSize: Theme.fontSizeSmall
                            width: parent.width - 150 - Theme.spacingM
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: Theme.spacingM

                        StyledText {
                            text: I18n.tr("Synchronized")
                            color: Theme.surfaceVariantText
                            font.pixelSize: Theme.fontSizeSmall
                            width: 150
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        StyledText {
                            text: dtPage.ntpSynced ? I18n.tr("Yes") : I18n.tr("No")
                            color: dtPage.ntpSynced ? Theme.success : Theme.surfaceVariantText
                            font.pixelSize: Theme.fontSizeSmall
                            width: parent.width - 150 - Theme.spacingM
                            horizontalAlignment: Text.AlignRight
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // Automatic time (NTP).
            SettingsToggleCard {
                width: parent.width
                title: I18n.tr("Set time automatically")
                description: I18n.tr("Keep the clock in sync over the network (NTP)")
                iconName: "update"
                checked: dtPage.ntpOn
                onToggled: checked => dtPage.setNtp(checked)
            }

            StyledText {
                width: parent.width
                text: I18n.tr("Changing the time zone is coming soon — it needs a searchable picker (≈600 zones).")
                color: Theme.surfaceVariantText
                font.pixelSize: Theme.fontSizeSmall - 1
                wrapMode: Text.Wrap
            }
        }
    }
}
