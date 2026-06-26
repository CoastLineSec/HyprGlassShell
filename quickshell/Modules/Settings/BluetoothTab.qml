pragma ComponentBehavior: Bound

import QtQuick
import qs.Common
import qs.Modules.Settings.Widgets
import qs.Services
import qs.Widgets

// Bluetooth settings page — adapter toggle, scan, paired ("My Devices") and
// discovered ("Other Devices") lists with connect / disconnect / pair actions.
Item {
    id: bluetoothTab

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    readonly property bool btAvailable: BluetoothService.available
    readonly property bool btEnabled: BluetoothService.enabled
    readonly property bool scanning: BluetoothService.adapter?.discovering ?? false

    // A device only "has a name" if it advertised a real one — BlueZ falls back to the
    // MAC address for nameless devices, which clutters the scan with hex gibberish.
    function looksLikeMac(s) {
        return /^([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}$/.test((s || "").trim());
    }
    function bestName(dev) {
        if (!dev)
            return "";
        if (dev.deviceName && dev.deviceName.length > 0 && !looksLikeMac(dev.deviceName))
            return dev.deviceName;
        if (dev.name && dev.name.length > 0 && !looksLikeMac(dev.name))
            return dev.name;
        return "";
    }

    readonly property var pairedDevices: BluetoothService.pairedDevices ? BluetoothService.pairedDevices : []
    readonly property var availableDevices: {
        const d = BluetoothService.devices;
        if (!d || !d.values)
            return [];
        // Only show discovered devices that advertised a real (non-MAC) name.
        return d.values.filter(dev => dev && !dev.paired && !dev.trusted && bluetoothTab.bestName(dev).length > 0);
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

            // Adapter on/off.
            SettingsToggleCard {
                width: parent.width
                title: I18n.tr("Bluetooth")
                description: !bluetoothTab.btAvailable ? I18n.tr("No Bluetooth adapter found") : (bluetoothTab.btEnabled ? I18n.tr("On") : I18n.tr("Off"))
                iconName: "bluetooth"
                checked: bluetoothTab.btEnabled
                onToggled: checked => {
                    if (BluetoothService.adapter)
                        BluetoothService.adapter.enabled = checked;
                }
            }

            // Paired devices.
            SettingsCard {
                width: parent.width
                visible: bluetoothTab.btEnabled
                title: I18n.tr("My Devices")
                iconName: "devices"

                Column {
                    width: parent.width
                    spacing: Theme.spacingXS

                    Repeater {
                        model: bluetoothTab.pairedDevices
                        delegate: bluetoothTab.deviceRow
                    }

                    StyledText {
                        width: parent.width
                        visible: bluetoothTab.pairedDevices.length === 0
                        text: I18n.tr("No paired devices")
                        color: Theme.surfaceVariantText
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignHCenter
                        topPadding: Theme.spacingS
                    }
                }
            }

            // Discovered devices.
            SettingsCard {
                width: parent.width
                visible: bluetoothTab.btEnabled
                title: I18n.tr("Other Devices")
                iconName: "bluetooth_searching"

                headerActions: HGSButton {
                    text: bluetoothTab.scanning ? I18n.tr("Scanning…") : I18n.tr("Scan")
                    iconName: "search"
                    buttonHeight: 30
                    horizontalPadding: Theme.spacingM
                    backgroundColor: bluetoothTab.scanning ? Theme.primary : Theme.surfaceContainerHigh
                    textColor: bluetoothTab.scanning ? Theme.primaryText : Theme.surfaceText
                    onClicked: {
                        if (BluetoothService.adapter)
                            BluetoothService.adapter.discovering = !BluetoothService.adapter.discovering;
                    }
                }

                Column {
                    width: parent.width
                    spacing: Theme.spacingXS

                    Repeater {
                        model: bluetoothTab.availableDevices
                        delegate: bluetoothTab.deviceRow
                    }

                    StyledText {
                        width: parent.width
                        visible: bluetoothTab.availableDevices.length === 0
                        text: bluetoothTab.scanning ? I18n.tr("Searching…") : I18n.tr("Press Scan to find nearby devices")
                        color: Theme.surfaceVariantText
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignHCenter
                        topPadding: Theme.spacingS
                    }
                }
            }
        }
    }

    // One reusable row, shared by both lists.
    property Component deviceRow: Component {
        Rectangle {
            id: row

            required property var modelData

            readonly property bool isPaired: (modelData.paired ?? false) || (modelData.trusted ?? false)
            readonly property bool isConnected: modelData.connected ?? false
            readonly property bool busy: BluetoothService.isDeviceBusy(modelData)

            width: parent ? parent.width : 0
            height: 56
            radius: Theme.cornerRadius
            color: rowMouse.containsMouse ? Theme.surfaceHover : "transparent"

            HGSIcon {
                id: devIcon
                name: BluetoothService.getDeviceIcon(row.modelData)
                size: Theme.iconSize
                color: row.isConnected ? Theme.primary : Theme.surfaceText
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingM
                anchors.verticalCenter: parent.verticalCenter
            }

            HGSButton {
                id: actionBtn
                anchors.right: parent.right
                anchors.rightMargin: Theme.spacingM
                anchors.verticalCenter: parent.verticalCenter
                buttonHeight: 30
                horizontalPadding: Theme.spacingM
                enabled: !row.busy
                text: row.busy ? I18n.tr("…") : (row.isPaired ? (row.isConnected ? I18n.tr("Disconnect") : I18n.tr("Connect")) : I18n.tr("Pair"))
                backgroundColor: (row.isPaired && row.isConnected) ? Theme.surfaceContainerHigh : Theme.primary
                textColor: (row.isPaired && row.isConnected) ? Theme.surfaceText : Theme.primaryText
                onClicked: {
                    if (row.busy)
                        return;
                    if (!row.isPaired) {
                        BluetoothService.pairDevice(row.modelData, function () {});
                    } else if (row.isConnected) {
                        row.modelData.disconnect();
                    } else {
                        BluetoothService.connectDeviceWithTrust(row.modelData);
                    }
                }
            }

            Column {
                anchors.left: devIcon.right
                anchors.leftMargin: Theme.spacingM
                anchors.right: actionBtn.left
                anchors.rightMargin: Theme.spacingM
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1

                StyledText {
                    width: parent.width
                    text: bluetoothTab.bestName(row.modelData) || row.modelData.address || I18n.tr("Unknown Device")
                    color: Theme.surfaceText
                    font.pixelSize: Theme.fontSizeMedium
                    font.weight: row.isConnected ? Font.Medium : Font.Normal
                    elide: Text.ElideRight
                }

                StyledText {
                    width: parent.width
                    visible: text.length > 0
                    text: {
                        if (row.busy)
                            return I18n.tr("Working…");
                        if (row.isConnected) {
                            if ((row.modelData.batteryAvailable ?? false) && (row.modelData.battery ?? 0) > 0)
                                return I18n.tr("Connected") + " • " + Math.round(row.modelData.battery * 100) + "%";
                            return I18n.tr("Connected");
                        }
                        return row.isPaired ? I18n.tr("Not Connected") : "";
                    }
                    color: row.isConnected ? Theme.primary : Theme.surfaceVariantText
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
}
