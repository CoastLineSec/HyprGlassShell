pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Widgets

// Storage page: mounted filesystems with usage bars, read from `df` (no root).
Item {
    id: storagePage

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    property var filesystems: []
    property bool loaded: false

    function human(bytes) {
        const b = Number(bytes);
        if (!isFinite(b) || b <= 0)
            return "0 B";
        const units = ["B", "KB", "MB", "GB", "TB", "PB"];
        let i = 0;
        let v = b;
        while (v >= 1024 && i < units.length - 1) {
            v /= 1024;
            i++;
        }
        return (v >= 100 ? Math.round(v) : v.toFixed(1)) + " " + units[i];
    }

    function refresh() {
        Proc.runCommand("df-usage", ["sh", "-c", "df -B1 --output=target,size,used,avail,pcent -x tmpfs -x devtmpfs -x efivarfs -x overlay 2>/dev/null"], (output, code) => {
            storagePage.loaded = true;
            if (code !== 0 || !output) {
                storagePage.filesystems = [];
                return;
            }
            const rows = [];
            const lines = output.split("\n");
            for (let i = 1; i < lines.length; i++) {
                const parts = lines[i].trim().split(/\s+/);
                if (parts.length < 5)
                    continue;
                const target = parts[0];
                // Skip pseudo/boot mounts that aren't user-relevant.
                if (target.startsWith("/boot") || target.startsWith("/sys") || target.startsWith("/proc") || target.startsWith("/run") || target === "/efi")
                    continue;
                const size = Number(parts[1]);
                if (!(size > 0))
                    continue;
                rows.push({
                    "target": target,
                    "size": size,
                    "used": Number(parts[2]),
                    "avail": Number(parts[3]),
                    "pct": parseInt(parts[4]) || 0
                });
            }
            storagePage.filesystems = rows;
        });
    }

    Component.onCompleted: refresh()

    Timer {
        interval: 15000
        running: storagePage.visible
        repeat: true
        onTriggered: storagePage.refresh()
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

            SettingsCard {
                width: parent.width
                title: I18n.tr("Storage")
                iconName: "storage"

                Column {
                    width: parent.width
                    spacing: Theme.spacingL

                    Repeater {
                        model: storagePage.filesystems

                        delegate: Column {
                            id: fsCol
                            required property var modelData
                            width: parent.width
                            spacing: Theme.spacingXS

                            Row {
                                width: parent.width
                                spacing: Theme.spacingM

                                StyledText {
                                    text: fsCol.modelData.target
                                    color: Theme.surfaceText
                                    font.pixelSize: Theme.fontSizeMedium
                                    font.weight: Font.Medium
                                    width: parent.width - usageText.width - Theme.spacingM
                                    elide: Text.ElideMiddle
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                StyledText {
                                    id: usageText
                                    text: storagePage.human(fsCol.modelData.used) + " / " + storagePage.human(fsCol.modelData.size)
                                    color: Theme.surfaceVariantText
                                    font.pixelSize: Theme.fontSizeSmall
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            // Usage bar.
                            Rectangle {
                                width: parent.width
                                height: 8
                                radius: 4
                                color: Theme.surfaceContainerHighest

                                Rectangle {
                                    width: Math.max(0, Math.min(1, fsCol.modelData.pct / 100)) * parent.width
                                    height: parent.height
                                    radius: 4
                                    color: fsCol.modelData.pct >= 90 ? Theme.error : (fsCol.modelData.pct >= 75 ? Theme.warning : Theme.primary)
                                }
                            }

                            StyledText {
                                text: fsCol.modelData.pct + "% " + I18n.tr("used") + "  ·  " + storagePage.human(fsCol.modelData.avail) + " " + I18n.tr("available")
                                color: Theme.surfaceVariantText
                                font.pixelSize: Theme.fontSizeSmall - 1
                            }
                        }
                    }

                    StyledText {
                        width: parent.width
                        visible: storagePage.filesystems.length === 0
                        text: storagePage.loaded ? I18n.tr("No filesystems found") : I18n.tr("Loading…")
                        color: Theme.surfaceVariantText
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
        }
    }
}
