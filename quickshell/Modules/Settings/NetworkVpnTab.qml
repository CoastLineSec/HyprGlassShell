pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Modals.Common
import qs.Modals.FileBrowser
import qs.Services
import qs.Widgets
import "VpnProviders.js" as VpnProviders

Item {
    id: networkVpnTab

    // Lets this tab be stacked at its content height inside the combined Network page.
    implicitHeight: mainColumn.height + Theme.spacingXL

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    // --- Detected VPN (universal; works on any backend incl. iwd/networkd) ---
    // vpnInterfaces: tunnel interfaces from networkd (every provider). vpnProvider:
    // optional rich detail { provider, connected, status, rows[] } from a provider CLI.
    property var vpnInterfaces: []
    property var vpnProvider: null

    function refreshDetectedVpn() {
        Proc.runCommand("networkctl-vpn", ["networkctl", "--json=short", "list"], (output, code) => {
            if (code !== 0 || !output)
                return;
            try {
                const d = JSON.parse(output);
                const ifs = (d && d.Interfaces) ? d.Interfaces : [];
                networkVpnTab.vpnInterfaces = ifs.filter(i => i && (i.Type === "wireguard" || i.Type === "tun" || i.Type === "tap" || i.Type === "ppp"));
            } catch (e) {
                networkVpnTab.vpnInterfaces = [];
            }
        });
        Proc.runCommand("vpn-provider-status", ["sh", "-c", VpnProviders.probeScript()], (output, code) => {
            networkVpnTab.vpnProvider = (code === 0 && output) ? VpnProviders.parse(output) : null;
        });
    }

    Timer {
        interval: 10000
        running: networkVpnTab.visible
        repeat: true
        onTriggered: networkVpnTab.refreshDetectedVpn()
    }

    Component.onCompleted: {
        NetworkService.addRef();
        refreshDetectedVpn();
    }

    Component.onDestruction: {
        NetworkService.removeRef();
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
                id: root

                // NetworkManager-only management card. Hidden on backends without daemon
                // VPN support (iwd/networkd) — the detected-VPN card above covers those.
                visible: HGSNetworkService.vpnAvailable

                property string expandedVpnUuid: ""

                title: I18n.tr("VPN")
                iconName: "vpn_key"
                settingKey: "networkVpn"
                tags: ["vpn", "network", "profiles", "import", "openvpn", "wireguard"]

                function openVpnFileBrowser() {
                    vpnFileBrowserLoader.active = true;
                    if (vpnFileBrowserLoader.item)
                        vpnFileBrowserLoader.item.open();
                }

                property var vpnFileBrowserLoader: LazyLoader {
                    active: false

                    FileBrowserModal {
                        browserTitle: I18n.tr("Import VPN")
                        browserIcon: "vpn_key"
                        browserType: "vpn"
                        fileExtensions: VPNService.getFileFilter()

                        onFileSelected: path => {
                            VPNService.importVpn(path.replace("file://", ""));
                        }
                    }
                }

                property var deleteVpnConfirm: ConfirmModal {}

                width: parent.width

                Column {
                    id: vpnSection

                    width: parent.width
                    spacing: Theme.spacingM

                    StyledText {
                        text: I18n.tr("Unavailable")
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.surfaceVariantText
                        width: parent.width
                        horizontalAlignment: Text.AlignLeft
                        visible: !HGSNetworkService.vpnAvailable
                    }

                    Row {
                        width: parent.width
                        spacing: Theme.spacingM
                        visible: HGSNetworkService.vpnAvailable

                        StyledText {
                            text: {
                                if (!HGSNetworkService.connected)
                                    return I18n.tr("Disconnected");
                                const names = HGSNetworkService.activeNames || [];
                                if (names.length <= 1)
                                    return names[0] || I18n.tr("Connected");
                                return names[0] + " +" + (names.length - 1);
                            }
                            font.pixelSize: Theme.fontSizeSmall
                            color: HGSNetworkService.connected ? Theme.primary : Theme.surfaceVariantText
                            width: parent.width - vpnHeaderControls.width - Theme.spacingM
                            horizontalAlignment: Text.AlignLeft
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Row {
                            id: vpnHeaderControls
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.spacingS

                            Rectangle {
                                height: 28
                                radius: 14
                                width: importVpnRow.width + Theme.spacingM * 2
                                color: importVpnArea.containsMouse ? Theme.primaryHoverLight : Theme.surfaceLight
                                opacity: VPNService.importing ? 0.5 : 1.0

                                Row {
                                    id: importVpnRow
                                    anchors.centerIn: parent
                                    spacing: Theme.spacingXS

                                    HGSIcon {
                                        name: VPNService.importing ? "sync" : "add"
                                        size: Theme.fontSizeSmall
                                        color: Theme.primary
                                    }

                                    StyledText {
                                        text: I18n.tr("Import")
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.primary
                                        font.weight: Font.Medium
                                    }
                                }

                                MouseArea {
                                    id: importVpnArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: VPNService.importing ? Qt.BusyCursor : Qt.PointingHandCursor
                                    enabled: !VPNService.importing
                                    onClicked: root.openVpnFileBrowser()
                                }
                            }

                            Rectangle {
                                height: 28
                                radius: 14
                                width: disconnectAllRow.width + Theme.spacingM * 2
                                color: disconnectAllArea.containsMouse ? Theme.errorHover : Theme.surfaceLight
                                visible: HGSNetworkService.connected
                                opacity: HGSNetworkService.isBusy ? 0.5 : 1.0

                                Row {
                                    id: disconnectAllRow
                                    anchors.centerIn: parent
                                    spacing: Theme.spacingXS

                                    HGSIcon {
                                        name: "link_off"
                                        size: Theme.fontSizeSmall
                                        color: Theme.surfaceText
                                    }

                                    StyledText {
                                        text: I18n.tr("Disconnect")
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.surfaceText
                                        font.weight: Font.Medium
                                    }
                                }

                                MouseArea {
                                    id: disconnectAllArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: HGSNetworkService.isBusy ? Qt.BusyCursor : Qt.PointingHandCursor
                                    enabled: !HGSNetworkService.isBusy
                                    onClicked: HGSNetworkService.disconnectAllActive()
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Qt.rgba(Theme.outline.r, Theme.outline.g, Theme.outline.b, 0.12)
                        visible: HGSNetworkService.vpnAvailable
                    }

                    Item {
                        width: parent.width
                        height: 100
                        visible: HGSNetworkService.vpnAvailable && HGSNetworkService.profiles.length === 0

                        Column {
                            anchors.centerIn: parent
                            spacing: Theme.spacingS

                            HGSIcon {
                                name: "vpn_key_off"
                                size: 36
                                color: Theme.surfaceVariantText
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            StyledText {
                                text: I18n.tr("No VPN profiles")
                                font.pixelSize: Theme.fontSizeMedium
                                color: Theme.surfaceVariantText
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            StyledText {
                                text: I18n.tr("Click Import to add a .ovpn or .conf")
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.surfaceVariantText
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 4
                        visible: HGSNetworkService.vpnAvailable && HGSNetworkService.profiles.length > 0

                        Repeater {
                            model: HGSNetworkService.profiles

                            delegate: Rectangle {
                                id: vpnProfileRow
                                required property var modelData
                                required property int index

                                readonly property bool isActive: HGSNetworkService.isActiveUuid(modelData.uuid)
                                readonly property bool isTransient: !!modelData.transient
                                readonly property bool canExpand: modelData.canExpand !== false
                                readonly property bool canDelete: modelData.canDelete !== false
                                readonly property bool isExpanded: root.expandedVpnUuid === modelData.uuid
                                readonly property var configData: (!isTransient && isExpanded) ? VPNService.editConfig : null

                                width: parent.width
                                height: isExpanded ? 56 + vpnExpandedContent.height : 56
                                radius: Theme.cornerRadius
                                color: vpnRowArea.containsMouse ? Theme.primaryHoverLight : (isActive ? Theme.primaryPressed : Theme.surfaceLight)
                                border.width: isActive ? 2 : 0
                                border.color: Theme.primary
                                opacity: HGSNetworkService.isBusy ? 0.6 : 1.0
                                clip: true

                                Behavior on height {
                                    NumberAnimation {
                                        duration: 150
                                        easing.type: Easing.OutQuad
                                    }
                                }

                                MouseArea {
                                    id: vpnRowArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: HGSNetworkService.isBusy ? Qt.BusyCursor : Qt.PointingHandCursor
                                    enabled: !HGSNetworkService.isBusy
                                    onClicked: HGSNetworkService.toggle(modelData.uuid)
                                }

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingS
                                    spacing: Theme.spacingS

                                    Row {
                                        width: parent.width
                                        height: 56 - Theme.spacingS * 2
                                        spacing: Theme.spacingS

                                        HGSIcon {
                                            name: isActive ? "vpn_lock" : "vpn_key_off"
                                            size: 20
                                            color: isActive ? Theme.primary : Theme.surfaceText
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Column {
                                            spacing: 2
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: parent.width - 20 - ((canExpand ? 28 : 0) + (canDelete ? 28 : 0)) - Theme.spacingS * 4

                                            StyledText {
                                                text: modelData.name
                                                font.pixelSize: Theme.fontSizeMedium
                                                color: isActive ? Theme.primary : Theme.surfaceText
                                                elide: Text.ElideRight
                                                width: parent.width
                                                horizontalAlignment: Text.AlignLeft
                                            }

                                            StyledText {
                                                text: VPNService.getVpnTypeFromProfile(modelData)
                                                font.pixelSize: Theme.fontSizeSmall
                                                color: Theme.surfaceVariantText
                                                anchors.left: parent.left
                                            }
                                        }

                                        Item {
                                            width: Theme.spacingXS
                                            height: 1
                                        }

                                        Rectangle {
                                            width: 28
                                            height: 28
                                            radius: 14
                                            color: vpnExpandBtn.containsMouse ? Theme.surfacePressed : "transparent"
                                            anchors.verticalCenter: parent.verticalCenter
                                            visible: canExpand

                                            HGSIcon {
                                                anchors.centerIn: parent
                                                name: isExpanded ? "expand_less" : "expand_more"
                                                size: 18
                                                color: Theme.surfaceText
                                            }

                                            MouseArea {
                                                id: vpnExpandBtn
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (isExpanded) {
                                                        root.expandedVpnUuid = "";
                                                    } else {
                                                        root.expandedVpnUuid = modelData.uuid;
                                                        VPNService.getConfig(modelData.uuid);
                                                    }
                                                }
                                            }
                                        }

                                        Rectangle {
                                            width: 28
                                            height: 28
                                            radius: 14
                                            color: vpnDeleteBtn.containsMouse ? Theme.errorHover : "transparent"
                                            anchors.verticalCenter: parent.verticalCenter
                                            visible: canDelete

                                            HGSIcon {
                                                anchors.centerIn: parent
                                                name: "delete"
                                                size: 18
                                                color: vpnDeleteBtn.containsMouse ? Theme.error : Theme.surfaceVariantText
                                            }

                                            MouseArea {
                                                id: vpnDeleteBtn
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    deleteVpnConfirm.showWithOptions({
                                                        title: I18n.tr("Delete VPN"),
                                                        message: I18n.tr("Delete \"%1\"?").arg(modelData.name),
                                                        confirmText: I18n.tr("Delete"),
                                                        confirmColor: Theme.error,
                                                        onConfirm: () => VPNService.deleteVpn(modelData.uuid)
                                                    });
                                                }
                                            }
                                        }
                                    }

                                    Column {
                                        id: vpnExpandedContent
                                        width: parent.width
                                        spacing: Theme.spacingXS
                                        visible: !isTransient && isExpanded

                                        Rectangle {
                                            width: parent.width
                                            height: 1
                                            color: Theme.outlineLight
                                        }

                                        Item {
                                            width: parent.width
                                            height: VPNService.configLoading ? 40 : 0
                                            visible: VPNService.configLoading

                                            HGSSpinner {
                                                anchors.centerIn: parent
                                                size: 20
                                            }
                                        }

                                        Flow {
                                            width: parent.width
                                            spacing: Theme.spacingXS
                                            visible: !VPNService.configLoading && configData

                                            Repeater {
                                                model: {
                                                    if (!configData)
                                                        return [];
                                                    const fields = [];
                                                    const data = configData.data || {};

                                                    if (data.remote)
                                                        fields.push({
                                                            label: I18n.tr("Server"),
                                                            value: data.remote
                                                        });
                                                    if (configData.username || data.username)
                                                        fields.push({
                                                            label: I18n.tr("Username"),
                                                            value: configData.username || data.username
                                                        });
                                                    if (data.cipher)
                                                        fields.push({
                                                            label: I18n.tr("Cipher"),
                                                            value: data.cipher
                                                        });
                                                    if (data.auth)
                                                        fields.push({
                                                            label: I18n.tr("Auth"),
                                                            value: data.auth
                                                        });
                                                    if (data["proto-tcp"] === "yes" || data["proto-tcp"] === "no")
                                                        fields.push({
                                                            label: I18n.tr("Protocol"),
                                                            value: data["proto-tcp"] === "yes" ? "TCP" : "UDP"
                                                        });
                                                    if (data["tunnel-mtu"])
                                                        fields.push({
                                                            label: I18n.tr("MTU"),
                                                            value: data["tunnel-mtu"]
                                                        });
                                                    if (data["connection-type"])
                                                        fields.push({
                                                            label: I18n.tr("Auth Type"),
                                                            value: data["connection-type"]
                                                        });
                                                    return fields;
                                                }

                                                delegate: Rectangle {
                                                    required property var modelData
                                                    required property int index

                                                    width: vpnFieldContent.width + Theme.spacingM * 2
                                                    height: 32
                                                    radius: Theme.cornerRadius - 2
                                                    color: Theme.surfaceContainerHigh
                                                    border.width: 1
                                                    border.color: Theme.outlineLight

                                                    Row {
                                                        id: vpnFieldContent
                                                        anchors.centerIn: parent
                                                        spacing: Theme.spacingXS

                                                        StyledText {
                                                            text: modelData.label + ":"
                                                            font.pixelSize: Theme.fontSizeSmall
                                                            color: Theme.surfaceVariantText
                                                            anchors.verticalCenter: parent.verticalCenter
                                                        }

                                                        StyledText {
                                                            text: modelData.value
                                                            font.pixelSize: Theme.fontSizeSmall
                                                            color: Theme.surfaceText
                                                            font.weight: Font.Medium
                                                            anchors.verticalCenter: parent.verticalCenter
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        HGSToggle {
                                            width: parent.width
                                            text: I18n.tr("Autoconnect")
                                            checked: configData ? (configData.autoconnect || false) : false
                                            visible: !VPNService.configLoading && configData !== null
                                            onToggled: checked => {
                                                VPNService.updateConfig(modelData.uuid, {
                                                    autoconnect: checked
                                                });
                                            }
                                        }

                                        Item {
                                            width: 1
                                            height: Theme.spacingXS
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Detected VPN — works for ANY provider. The interface list is the universal
            // view (every VPN, WireGuard/OpenVPN/etc., shows up as a tunnel interface);
            // a provider CLI (e.g. NordVPN) adds richer detail when it's installed.
            SettingsCard {
                width: parent.width
                title: I18n.tr("VPN Connection")
                iconName: "shield"
                visible: networkVpnTab.vpnInterfaces.length > 0 || (networkVpnTab.vpnProvider && networkVpnTab.vpnProvider.connected)

                Column {
                    width: parent.width
                    spacing: Theme.spacingM

                    // Universal: every active VPN tunnel interface, any provider.
                    Repeater {
                        model: networkVpnTab.vpnInterfaces

                        delegate: Row {
                            id: vifRow
                            required property var modelData
                            width: parent.width
                            spacing: Theme.spacingS

                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: vifRow.modelData.OperationalState === "routable" ? Theme.success : Theme.warning
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            StyledText {
                                text: (vifRow.modelData.Name || "") + "  ·  " + (vifRow.modelData.Type || "") + "  ·  " + (vifRow.modelData.OperationalState || "")
                                color: Theme.surfaceText
                                font.pixelSize: Theme.fontSizeSmall
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    // Divider before optional provider-specific detail.
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.outline
                        opacity: 0.15
                        visible: networkVpnTab.vpnProvider && networkVpnTab.vpnProvider.connected && networkVpnTab.vpnInterfaces.length > 0
                    }

                    // Provider enrichment (shown when a detected provider CLI reports connected).
                    Column {
                        width: parent.width
                        spacing: 3
                        visible: networkVpnTab.vpnProvider && networkVpnTab.vpnProvider.connected

                        Row {
                            spacing: Theme.spacingS

                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: Theme.success
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            StyledText {
                                text: (networkVpnTab.vpnProvider ? networkVpnTab.vpnProvider.provider : "") + "  ·  " + (networkVpnTab.vpnProvider ? networkVpnTab.vpnProvider.status : "")
                                color: Theme.surfaceText
                                font.pixelSize: Theme.fontSizeMedium
                                font.weight: Font.Medium
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        Repeater {
                            model: networkVpnTab.vpnProvider ? networkVpnTab.vpnProvider.rows : []

                            delegate: Row {
                                id: provRow
                                required property var modelData
                                width: parent.width
                                visible: (provRow.modelData.value || "").length > 0
                                spacing: Theme.spacingM

                                StyledText {
                                    text: provRow.modelData.label
                                    color: Theme.surfaceVariantText
                                    font.pixelSize: Theme.fontSizeSmall
                                    width: 110
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                StyledText {
                                    text: provRow.modelData.value
                                    color: Theme.surfaceText
                                    font.pixelSize: Theme.fontSizeSmall
                                    width: parent.width - 110 - Theme.spacingM
                                    horizontalAlignment: Text.AlignRight
                                    elide: Text.ElideRight
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
