pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Widgets

// macOS-style "services" overview: every managed network interface with a status
// dot, type and state. Reads `networkctl --json=short list` (systemd-networkd,
// no root). Read-only.
Item {
    id: svcTab

    implicitHeight: mainColumn.height + Theme.spacingXL

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    property var services: []
    property bool loaded: false

    function refresh() {
        Proc.runCommand("networkctl-list", ["networkctl", "--json=short", "list"], (output, code) => {
            svcTab.loaded = true;
            if (code !== 0 || !output) {
                svcTab.services = [];
                return;
            }
            try {
                const d = JSON.parse(output);
                const ifs = (d && d.Interfaces) ? d.Interfaces : (Array.isArray(d) ? d : []);
                // Hide loopback to match how macOS lists services.
                svcTab.services = ifs.filter(i => i && i.Type !== "loopback" && i.Name !== "lo");
            } catch (e) {
                svcTab.services = [];
            }
        });
    }

    function statusColor(i) {
        const op = (i.OperationalState || "").toLowerCase();
        const on = (i.OnlineState || "").toLowerCase();
        if (op === "routable" || on === "online")
            return Theme.success;
        if (op === "degraded" || op === "carrier" || on === "partial")
            return Theme.warning;
        return Theme.surfaceVariantText;
    }
    function typeIcon(t) {
        switch ((t || "").toLowerCase()) {
        case "wlan":
            return "wifi";
        case "ether":
            return "settings_ethernet";
        case "wireguard":
            return "vpn_key";
        case "bridge":
            return "lan";
        case "tun":
        case "tunnel":
            return "vpn_lock";
        default:
            return "device_hub";
        }
    }
    function stateText(i) {
        const op = i.OperationalState || I18n.tr("unknown");
        const on = (i.OnlineState && i.OnlineState !== "unknown") ? (" · " + i.OnlineState) : "";
        return (i.Type || "") + "  ·  " + op + on;
    }

    Component.onCompleted: refresh()

    Timer {
        interval: 8000
        running: svcTab.visible
        repeat: true
        onTriggered: svcTab.refresh()
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
                title: I18n.tr("Services")
                iconName: "lan"

                Column {
                    width: parent.width
                    spacing: 2

                    Repeater {
                        model: svcTab.services

                        delegate: Rectangle {
                            id: svcRow
                            required property var modelData

                            width: parent.width
                            height: 52
                            radius: Theme.cornerRadius
                            color: rowMouse.containsMouse ? Theme.surfaceHover : "transparent"

                            Rectangle {
                                id: dot
                                width: 10
                                height: 10
                                radius: 5
                                color: svcTab.statusColor(svcRow.modelData)
                                anchors.left: parent.left
                                anchors.leftMargin: Theme.spacingM
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            HGSIcon {
                                id: typeIco
                                name: svcTab.typeIcon(svcRow.modelData.Type)
                                size: Theme.iconSize - 2
                                color: Theme.surfaceText
                                anchors.left: dot.right
                                anchors.leftMargin: Theme.spacingM
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Column {
                                anchors.left: typeIco.right
                                anchors.leftMargin: Theme.spacingM
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.spacingM
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 1

                                StyledText {
                                    width: parent.width
                                    text: svcRow.modelData.Name || I18n.tr("Interface")
                                    color: Theme.surfaceText
                                    font.pixelSize: Theme.fontSizeMedium
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }

                                StyledText {
                                    width: parent.width
                                    text: svcTab.stateText(svcRow.modelData)
                                    color: Theme.surfaceVariantText
                                    font.pixelSize: Theme.fontSizeSmall
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                        }
                    }

                    StyledText {
                        width: parent.width
                        visible: svcTab.services.length === 0
                        text: svcTab.loaded ? I18n.tr("No managed interfaces") : I18n.tr("Loading…")
                        color: Theme.surfaceVariantText
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
        }
    }
}
