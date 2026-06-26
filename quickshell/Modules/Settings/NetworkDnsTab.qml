pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Services
import qs.Widgets

// systemd-resolved DNS view: per-link DNS servers + global resolver configuration,
// in one card. Reads `resolvectl --json=short status` (no root needed). Read-only
// apart from the root-free "Flush cache" action.
Item {
    id: dnsTab

    implicitHeight: mainColumn.height + Theme.spacingXL

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    property var globalDns: ({})
    property var linkDns: []
    property bool resolvedAvailable: true
    property bool loaded: false

    function refresh() {
        Proc.runCommand("resolvectl-status", ["resolvectl", "--json=short", "status"], (output, code) => {
            dnsTab.loaded = true;
            if (code !== 0 || !output) {
                dnsTab.resolvedAvailable = false;
                return;
            }
            try {
                const arr = JSON.parse(output);
                if (!Array.isArray(arr)) {
                    dnsTab.resolvedAvailable = false;
                    return;
                }
                const links = [];
                let g = {};
                for (let i = 0; i < arr.length; i++) {
                    const e = arr[i];
                    if (!e)
                        continue;
                    if (e.ifname) {
                        if (e.servers && e.servers.length > 0)
                            links.push(e);
                    } else {
                        g = e;
                    }
                }
                dnsTab.globalDns = g;
                dnsTab.linkDns = links;
                dnsTab.resolvedAvailable = true;
            } catch (err) {
                dnsTab.resolvedAvailable = false;
            }
        });
    }

    function serverList(e) {
        if (!e || !e.servers)
            return "";
        return e.servers.map(s => s.addressString || "").filter(x => x.length > 0).join(", ");
    }
    function domainList(e) {
        if (!e || !e.domains)
            return "";
        return e.domains.map(d => (typeof d === "string" ? d : (d.name || ""))).filter(x => x.length > 0).join(", ");
    }
    function fallbackList() {
        const f = globalDns.fallbackServers;
        if (!f)
            return "";
        return f.map(s => s.addressString || "").filter(x => x.length > 0).join(", ");
    }
    function fmt(v) {
        return (v === undefined || v === null || v === "") ? "—" : String(v);
    }
    // Safe, root-free write action: clears the resolver cache.
    function flushCache() {
        Proc.runCommand("resolvectl-flush", ["resolvectl", "flush-caches"], (output, code) => {
            ToastService.showInfo(code === 0 ? I18n.tr("DNS cache flushed") : I18n.tr("Failed to flush DNS cache"));
            dnsTab.refresh();
        });
    }

    readonly property var globalRows: [
        {
            "label": I18n.tr("resolv.conf mode"),
            "value": dnsTab.fmt(globalDns.resolvConfMode)
        },
        {
            "label": "DNSSEC",
            "value": dnsTab.fmt(globalDns.dnssec)
        },
        {
            "label": I18n.tr("DNS over TLS"),
            "value": dnsTab.fmt(globalDns.dnsOverTLS)
        },
        {
            "label": "LLMNR",
            "value": dnsTab.fmt(globalDns.llmnr)
        },
        {
            "label": "mDNS",
            "value": dnsTab.fmt(globalDns.mDNS)
        },
        {
            "label": I18n.tr("Fallback servers"),
            "value": dnsTab.fallbackList() || "—"
        }
    ]

    Component.onCompleted: refresh()

    Timer {
        interval: 15000
        running: dnsTab.visible
        repeat: true
        onTriggered: dnsTab.refresh()
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
                title: I18n.tr("DNS")
                iconName: "dns"

                headerActions: HGSButton {
                    text: I18n.tr("Flush cache")
                    iconName: "cached"
                    buttonHeight: 30
                    horizontalPadding: Theme.spacingM
                    backgroundColor: Theme.surfaceContainerHigh
                    textColor: Theme.surfaceText
                    onClicked: dnsTab.flushCache()
                }

                Column {
                    width: parent.width
                    spacing: Theme.spacingM

                    // Per-link DNS servers.
                    Repeater {
                        model: dnsTab.linkDns

                        delegate: Column {
                            id: linkRow
                            required property var modelData
                            width: parent.width
                            spacing: 2

                            Row {
                                width: parent.width
                                spacing: Theme.spacingS

                                StyledText {
                                    text: linkRow.modelData.ifname || I18n.tr("Link")
                                    color: Theme.surfaceText
                                    font.pixelSize: Theme.fontSizeMedium
                                    font.weight: Font.Medium
                                }

                                StyledText {
                                    visible: linkRow.modelData.defaultRoute === true
                                    text: I18n.tr("Default route")
                                    color: Theme.primary
                                    font.pixelSize: Theme.fontSizeSmall
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            StyledText {
                                width: parent.width
                                text: dnsTab.serverList(linkRow.modelData)
                                color: Theme.surfaceVariantText
                                font.pixelSize: Theme.fontSizeSmall
                                wrapMode: Text.Wrap
                            }

                            StyledText {
                                width: parent.width
                                visible: dnsTab.domainList(linkRow.modelData).length > 0
                                text: I18n.tr("Search domains") + ": " + dnsTab.domainList(linkRow.modelData)
                                color: Theme.surfaceVariantText
                                font.pixelSize: Theme.fontSizeSmall - 1
                                wrapMode: Text.Wrap
                            }
                        }
                    }

                    StyledText {
                        width: parent.width
                        visible: dnsTab.linkDns.length === 0
                        text: !dnsTab.resolvedAvailable ? I18n.tr("systemd-resolved is not available") : (dnsTab.loaded ? I18n.tr("No DNS servers configured") : I18n.tr("Loading…"))
                        color: Theme.surfaceVariantText
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    // Divider between per-link servers and global configuration.
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.outline
                        opacity: 0.15
                        visible: dnsTab.resolvedAvailable && dnsTab.linkDns.length > 0
                    }

                    // Global resolver configuration.
                    Column {
                        width: parent.width
                        spacing: Theme.spacingXS
                        visible: dnsTab.resolvedAvailable

                        Repeater {
                            model: dnsTab.globalRows

                            delegate: Row {
                                id: cfgRow
                                required property var modelData
                                width: parent.width
                                spacing: Theme.spacingM

                                StyledText {
                                    text: cfgRow.modelData.label
                                    color: Theme.surfaceVariantText
                                    font.pixelSize: Theme.fontSizeSmall
                                    width: 150
                                    elide: Text.ElideRight
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                StyledText {
                                    text: cfgRow.modelData.value
                                    color: Theme.surfaceText
                                    font.pixelSize: Theme.fontSizeSmall
                                    width: parent.width - 150 - Theme.spacingM
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
