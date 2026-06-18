import QtQuick
import qs.Common
import qs.Services
import qs.Widgets
import qs.Modules.Plugins

PluginComponent {
    id: root

    Ref {
        service: HGSNetworkService
    }

    ccWidgetIcon: "vpn_key"
    ccWidgetPrimaryText: I18n.tr("VPN")
    ccWidgetSecondaryText: {
        if (!HGSNetworkService.connected)
            return I18n.tr("Disconnected");
        const names = HGSNetworkService.activeNames || [];
        if (names.length <= 1)
            return names[0] || I18n.tr("Connected");
        return names[0] + " +" + (names.length - 1);
    }
    ccWidgetIsActive: HGSNetworkService.connected

    onCcWidgetToggled: HGSNetworkService.toggleVpn()

    ccDetailContent: Component {
        VpnDetailContent {
            listHeight: 260
        }
    }
}
