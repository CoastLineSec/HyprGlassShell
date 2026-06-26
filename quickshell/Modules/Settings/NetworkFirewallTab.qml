pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Widgets

// Firewall status. Detects the installed/active backend (nftables / firewalld / ufw).
// Note: many setups (incl. James's) run nftables as a *oneshot* that loads the
// ruleset at boot and then exits, so `is-active` reports "inactive" even though the
// firewall is loaded — we treat an enabled oneshot whose last run succeeded as active.
// Read-only: rule details + toggling need root (privileged daemon/polkit phase).
Item {
    id: fwTab

    implicitHeight: mainColumn.height + Theme.spacingXL

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    // Each: { installed, active, enabled, result }
    property var backends: ({})
    property bool loaded: false

    function isProtected(name) {
        const b = backends[name];
        if (!b)
            return false;
        if (b.active === "active")
            return true;
        // oneshot firewalls (e.g. nftables) load at boot then exit: enabled + last
        // run succeeded means the ruleset is in place.
        return b.enabled === "enabled" && (b.result === "success" || b.active === "exited");
    }
    readonly property string activeName: {
        if (isProtected("nftables"))
            return "nftables";
        if (isProtected("firewalld"))
            return "firewalld";
        if (isProtected("ufw"))
            return "ufw";
        return "";
    }
    readonly property bool anyActive: activeName !== ""
    readonly property bool anyInstalled: {
        const b = backends;
        return (b.nftables && b.nftables.installed) || (b.firewalld && b.firewalld.installed) || (b.ufw && b.ufw.installed);
    }

    function stateLabel(name) {
        const b = backends[name];
        if (!b)
            return "";
        if (b.active === "active")
            return I18n.tr("active");
        if (b.enabled === "enabled" && (b.result === "success" || b.active === "exited"))
            return I18n.tr("loaded at boot");
        if (b.enabled === "enabled")
            return I18n.tr("enabled (not loaded)");
        return b.active || I18n.tr("inactive");
    }
    function stateColor(name) {
        return isProtected(name) ? Theme.success : Theme.surfaceVariantText;
    }

    function refresh() {
        const script = "for s in nftables firewalld ufw; do " + "inst=0; " + "case $s in nftables) command -v nft >/dev/null 2>&1 && inst=1;; firewalld) command -v firewall-cmd >/dev/null 2>&1 && inst=1;; ufw) command -v ufw >/dev/null 2>&1 && inst=1;; esac; " + "act=$(systemctl is-active $s 2>/dev/null); " + "en=$(systemctl is-enabled $s 2>/dev/null); " + "res=$(systemctl show $s -p Result --value 2>/dev/null); " + "printf '%s=%s:%s:%s:%s\\n' \"$s\" \"$inst\" \"$act\" \"$en\" \"$res\"; " + "done";
        Proc.runCommand("firewall-probe", ["sh", "-c", script], (output, code) => {
            fwTab.loaded = true;
            if (!output)
                return;
            const next = {};
            const lines = output.split("\n");
            for (let i = 0; i < lines.length; i++) {
                const ln = lines[i].trim();
                const eq = ln.indexOf("=");
                if (eq <= 0)
                    continue;
                const name = ln.substring(0, eq);
                const parts = ln.substring(eq + 1).split(":");
                next[name] = {
                    "installed": parts[0] === "1",
                    "active": (parts[1] || "").trim(),
                    "enabled": (parts[2] || "").trim(),
                    "result": (parts[3] || "").trim()
                };
            }
            fwTab.backends = next;
        });
    }

    readonly property var rows: ["nftables", "firewalld", "ufw"]

    Component.onCompleted: refresh()

    Timer {
        interval: 10000
        running: fwTab.visible
        repeat: true
        onTriggered: fwTab.refresh()
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
                title: I18n.tr("Firewall")
                iconName: "security"

                Column {
                    width: parent.width
                    spacing: Theme.spacingM

                    // Overall status banner.
                    Row {
                        width: parent.width
                        spacing: Theme.spacingM

                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: fwTab.anyActive ? Theme.success : Theme.surfaceVariantText
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        StyledText {
                            text: !fwTab.loaded ? I18n.tr("Checking…") : (fwTab.anyActive ? I18n.tr("Active") + "  ·  " + fwTab.activeName : I18n.tr("No active firewall"))
                            color: Theme.surfaceText
                            font.pixelSize: Theme.fontSizeMedium
                            font.weight: Font.Medium
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // Per-backend rows.
                    Repeater {
                        model: fwTab.rows

                        delegate: Row {
                            id: fwRow
                            required property var modelData
                            width: parent.width
                            visible: fwTab.backends[fwRow.modelData] && fwTab.backends[fwRow.modelData].installed
                            spacing: Theme.spacingM

                            StyledText {
                                text: fwRow.modelData
                                color: Theme.surfaceVariantText
                                font.pixelSize: Theme.fontSizeSmall
                                width: 150
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            StyledText {
                                text: fwTab.stateLabel(fwRow.modelData)
                                color: fwTab.stateColor(fwRow.modelData)
                                font.pixelSize: Theme.fontSizeSmall
                                width: parent.width - 150 - Theme.spacingM
                                horizontalAlignment: Text.AlignRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    StyledText {
                        width: parent.width
                        visible: fwTab.loaded && !fwTab.anyInstalled
                        text: I18n.tr("No firewall backend detected (nftables, firewalld, ufw).")
                        color: Theme.surfaceVariantText
                        font.pixelSize: Theme.fontSizeSmall
                        wrapMode: Text.Wrap
                    }

                    StyledText {
                        width: parent.width
                        text: I18n.tr("Rule details and changes require elevated privileges — coming via the privileged daemon.")
                        color: Theme.surfaceVariantText
                        font.pixelSize: Theme.fontSizeSmall - 1
                        wrapMode: Text.Wrap
                        topPadding: Theme.spacingXS
                    }
                }
            }
        }
    }
}
