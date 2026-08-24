pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property bool discoveryAvailable: false
    property bool discoveryBusy: false
    property var connectedDevices: []
    property double observedAtMs: 0
    property string inventoryDigest: ""
    property var unaddressableCounts: ({})
    property string discoveryErrorName: ""
    property string discoveryErrorMessage: ""
    property bool projectionAvailable: false
    property var savedDevices: []
    property var otherSavedDevices: []
    property string projectionRevisionToken: ""
    property string projectionInventoryDigest: ""
    property string projectionErrorName: ""
    property string projectionErrorMessage: ""
    property real minimumTargetSize: 44

    signal refreshRequested()
    signal manageProfilesRequested()

    readonly property real minimumRowHeight: 52
    readonly property var deviceGroups: [
        {
            kind: "keyboard",
            title: qsTr("Keyboards"),
            objectName: "inputKeyboardsSection"
        },
        {
            kind: "pointer",
            title: qsTr("Pointing devices"),
            objectName: "inputPointingDevicesSection"
        },
        {
            kind: "touch",
            title: qsTr("Touch devices"),
            objectName: "inputTouchDevicesSection"
        },
        {
            kind: "tablet",
            title: qsTr("Tablets"),
            objectName: "inputTabletsSection"
        }
    ]
    readonly property int unaddressableTotal:
        Number(root.unaddressableCounts.switches || 0)
        + Number(root.unaddressableCounts.tabletPads || 0)
        + Number(root.unaddressableCounts.tabletTools || 0)

    function rowsForKind(kind) {
        const rows = [];
        for (let index = 0; index < root.connectedDevices.length; ++index) {
            const row = root.connectedDevices[index];
            if (row.observedKind === kind)
                rows.push({ sourceIndex: index, value: row });
        }
        return rows;
    }

    function savedRows() {
        const rows = [];
        for (let index = 0; index < root.otherSavedDevices.length; ++index) {
            rows.push({ sourceIndex: index, value: root.otherSavedDevices[index] });
        }
        return rows;
    }

    function connectedStatus(state) {
        if (state === "matched")
            return qsTr("Observed — saved settings found");
        if (state === "not-saved")
            return qsTr("Observed — no saved settings");
        if (state === "kind-mismatch")
            return qsTr("Observed — saved kind differs");
        return qsTr("Observed — saved-settings status unavailable");
    }

    function savedStatus(state) {
        if (state === "not-observed")
            return qsTr("Not observed when last checked");
        if (state === "unobservable")
            return qsTr("Connection unknown");
        if (state === "kind-mismatch")
            return qsTr("Saved kind differs");
        if (state === "inventory-unavailable")
            return qsTr("Connection status unavailable");
        return qsTr("Observed");
    }

    function kindLabel(kind) {
        if (kind === "keyboard")
            return qsTr("Keyboard");
        if (kind === "pointer")
            return qsTr("Pointing device");
        if (kind === "touchpad")
            return qsTr("Touchpad");
        if (kind === "touch")
            return qsTr("Touch device");
        if (kind === "tablet")
            return qsTr("Tablet");
        if (kind === "tabletTool")
            return qsTr("Tablet tool");
        if (kind === "switch")
            return qsTr("Switch");
        return qsTr("Other device");
    }

    function overrideText(count) {
        return Number(count) === 1
            ? qsTr("1 saved override")
            : qsTr("%1 saved overrides").arg(Number(count));
    }

    function observationText() {
        if (!root.discoveryAvailable || root.observedAtMs <= 0)
            return qsTr("No current observation receipt");
        const stamp = Qt.formatDateTime(
            new Date(root.observedAtMs), "yyyy-MM-dd hh:mm:ss t"
        );
        return qsTr("Observed at %1").arg(stamp);
    }

    function unaddressableText() {
        const parts = [];
        const switches = Number(root.unaddressableCounts.switches || 0);
        const pads = Number(root.unaddressableCounts.tabletPads || 0);
        const tools = Number(root.unaddressableCounts.tabletTools || 0);
        if (switches > 0)
            parts.push(switches === 1
                ? qsTr("1 switch") : qsTr("%1 switches").arg(switches));
        if (pads > 0)
            parts.push(pads === 1
                ? qsTr("1 tablet pad") : qsTr("%1 tablet pads").arg(pads));
        if (tools > 0)
            parts.push(tools === 1
                ? qsTr("1 tablet tool") : qsTr("%1 tablet tools").arg(tools));
        return qsTr("Observed but not addressable by saved settings: %1.")
            .arg(parts.join(", "));
    }

    objectName: "inputDevicesPane"
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    spacing: 16

    RowLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 3

            Label {
                Layout.fillWidth: true
                text: qsTr("Current session devices")
                font.pixelSize: 20
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Label {
                objectName: "inputDevicesObservationCopy"
                Layout.fillWidth: true
                text: root.observationText()
                color: palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.name: text
            }
        }

        Button {
            objectName: "refreshConnectedInputDevicesButton"
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: root.discoveryBusy ? qsTr("Checking…") : qsTr("Refresh")
            enabled: !root.discoveryBusy
            icon.name: "view-refresh-symbolic"
            Accessible.name: qsTr("Refresh the authenticated input-device observation")

            onClicked: root.refreshRequested()
        }
    }

    Label {
        objectName: "inputDeviceSessionIdentityCopy"
        Layout.fillWidth: true
        text: qsTr("This one-time inventory is reported by the current authenticated Hyprland session. It is not a device-support test. Identical devices can receive numbered names according to connection order.")
        color: palette.placeholderText
        font.pixelSize: 12
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        Accessible.name: text
    }

    Button {
        objectName: "manageInputDeviceProfilesButton"
        Layout.alignment: Qt.AlignRight
        implicitHeight: Math.max(
            root.minimumTargetSize,
            implicitBackgroundHeight,
            implicitContentHeight + topPadding + bottomPadding
        )
        text: qsTr("Manage device profiles")
        icon.name: "document-edit-symbolic"
        Accessible.name: qsTr("Open editable per-device Hyprland profiles")

        onClicked: root.manageProfilesRequested()
    }

    Frame {
        objectName: "inputDeviceDiscoveryStatusCard"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        visible: root.discoveryBusy
            || !root.discoveryAvailable
            || root.discoveryErrorMessage.length > 0
            || !root.projectionAvailable
            || root.projectionErrorMessage.length > 0
        padding: 14

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            Label {
                objectName: "inputDeviceDiscoveryStatusMessage"
                Layout.fillWidth: true
                text: {
                    if (root.discoveryBusy)
                        return qsTr("Checking the current authenticated Hyprland session…");
                    if (root.discoveryErrorMessage.length > 0)
                        return qsTr("Connection status unavailable. %1")
                            .arg(root.discoveryErrorMessage);
                    if (!root.discoveryAvailable)
                        return qsTr("Connection status unavailable.");
                    return qsTr("Observed device inventory is available.");
                }
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }

            Label {
                objectName: "inputDeviceProjectionStatusMessage"
                Layout.fillWidth: true
                visible: !root.projectionAvailable
                    || root.projectionErrorMessage.length > 0
                text: root.projectionErrorMessage.length > 0
                    ? qsTr("Saved device settings are unavailable. %1")
                        .arg(root.projectionErrorMessage)
                    : qsTr("Saved device settings are unavailable.")
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
        }
    }

    Label {
        objectName: "inputUnaddressableDevicesSummary"
        Layout.fillWidth: true
        visible: root.discoveryAvailable && root.unaddressableTotal > 0
        text: root.unaddressableText()
        color: palette.placeholderText
        font.pixelSize: 12
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        Accessible.name: text
    }

    Repeater {
        model: root.deviceGroups

        delegate: ColumnLayout {
            required property var modelData

            readonly property var rows: root.rowsForKind(modelData.kind)

            objectName: modelData.objectName
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: rows.length > 0
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: parent.modelData.title
                font.pixelSize: 16
                font.weight: Font.DemiBold
                textFormat: Text.PlainText
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Repeater {
                model: parent.rows
                delegate: connectedDeviceDelegate
            }
        }
    }

    Label {
        objectName: "inputNoConnectedDevicesMessage"
        Layout.fillWidth: true
        visible: root.discoveryAvailable && root.connectedDevices.length === 0
        text: qsTr("No addressable input devices were observed when this inventory was checked.")
        color: palette.placeholderText
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        Accessible.name: text
    }

    ColumnLayout {
        objectName: "inputOtherSavedDevicesSection"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: qsTr("Other saved device settings")
            font.pixelSize: 16
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        Label {
            Layout.fillWidth: true
            visible: root.projectionAvailable
            text: root.projectionRevisionToken.length > 0
                ? qsTr("Preserved from saved compositor revision %1. Saved enabled state is configuration, not observed runtime state.")
                    .arg(root.projectionRevisionToken)
                : qsTr("Saved enabled state is configuration, not observed runtime state.")
            color: palette.placeholderText
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.name: text
        }

        Repeater {
            model: root.savedRows()
            delegate: savedDeviceDelegate
        }

        Label {
            objectName: "inputNoOtherSavedDevicesMessage"
            Layout.fillWidth: true
            visible: root.projectionAvailable
                && root.otherSavedDevices.length === 0
            text: qsTr("No other saved device settings need a connection-status explanation.")
            color: palette.placeholderText
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.name: text
        }
    }

    Component {
        id: connectedDeviceDelegate

        Frame {
            id: connectedRow

            required property var modelData

            readonly property var row: modelData.value
            readonly property string statusText:
                root.connectedStatus(row.savedSettingsState)

            objectName: "inputConnectedDeviceRow" + modelData.sourceIndex
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: root.minimumRowHeight
            implicitHeight: Math.max(
                root.minimumRowHeight,
                connectedRowContent.implicitHeight + topPadding + bottomPadding
            )
            padding: 14
            Accessible.role: Accessible.ListItem
            Accessible.name: String(row.sessionSelector) + ". " + statusText

            contentItem: ColumnLayout {
                id: connectedRowContent
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: String(connectedRow.row.sessionSelector)
                    font.weight: Font.DemiBold
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: connectedRow.statusText
                    color: palette.placeholderText
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    visible: connectedRow.row.activeKeymap !== null
                        && connectedRow.row.activeKeymap !== undefined
                        && String(connectedRow.row.activeKeymap).length > 0
                    text: qsTr("Reported active keymap: %1")
                        .arg(String(connectedRow.row.activeKeymap || ""))
                    color: palette.placeholderText
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    visible: connectedRow.row.savedSettingsState === "matched"
                        || connectedRow.row.savedSettingsState === "kind-mismatch"
                    text: qsTr("Saved as %1 · %2 · %3")
                        .arg(root.kindLabel(connectedRow.row.configuredKind))
                        .arg(connectedRow.row.configuredEnabled
                            ? qsTr("enabled") : qsTr("disabled"))
                        .arg(root.overrideText(connectedRow.row.overrideCount))
                    color: palette.placeholderText
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }
        }
    }

    Component {
        id: savedDeviceDelegate

        Frame {
            id: savedRow

            required property var modelData

            readonly property var row: modelData.value
            readonly property string statusText: root.savedStatus(row.matchState)

            objectName: "inputSavedDeviceRow" + modelData.sourceIndex
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: root.minimumRowHeight
            implicitHeight: Math.max(
                root.minimumRowHeight,
                savedRowContent.implicitHeight + topPadding + bottomPadding
            )
            padding: 14
            Accessible.role: Accessible.ListItem
            Accessible.name: String(row.selector) + ". " + statusText

            contentItem: ColumnLayout {
                id: savedRowContent
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: String(savedRow.row.selector)
                    font.weight: Font.DemiBold
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: savedRow.statusText
                    color: palette.placeholderText
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Saved as %1 · %2 · %3")
                        .arg(root.kindLabel(savedRow.row.configuredKind))
                        .arg(savedRow.row.configuredEnabled
                            ? qsTr("enabled") : qsTr("disabled"))
                        .arg(root.overrideText(savedRow.row.overrideCount))
                    color: palette.placeholderText
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }
        }
    }
}
