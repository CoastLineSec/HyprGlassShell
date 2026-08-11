import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Page {
    id: root

    required property int barHeight
    required property int minimumBarHeight
    required property int maximumBarHeight
    required property int defaultBarHeight
    required property bool shellBorderEnabled
    required property int shellBorderWidth
    required property int shellBorderRadius
    required property bool syncHyprlandWindowBorders
    required property bool workspaceShowIdentifiers
    required property bool workspaceShowNames
    required property bool workspaceShowApplications
    required property int workspaceMaximumApplications
    required property bool workspaceOccupiedOnly
    required property string workspaceScrollMode
    property bool coreServiceAvailable: false
    property bool coreBusy: false
    property string coreErrorText: ""
    property string coreRecoveryState: ""
    property bool componentServiceAvailable: false
    property bool componentCatalogAvailable: false
    property bool componentWritable: false
    property bool workspaceInstanceAvailable: false
    property bool workspaceFeatureAvailable: workspaceInstanceAvailable
    property bool workspaceFeatureEnabled: true
    property bool workspacePreviewEnabled:
        workspaceFeatureAvailable && workspaceFeatureEnabled
    property bool componentBusy: false
    property string componentErrorText: ""
    property string componentRecoveryState: ""
    property bool previewAnimationsEnabled: true
    property real contentTopMargin: 28
    property bool borderDraftEnabled: shellBorderEnabled
    property int borderDraftWidth: shellBorderWidth
    property int borderDraftRadius: shellBorderRadius
    property bool borderDraftSync: syncHyprlandWindowBorders
    readonly property bool compactPreview:
        root.width < 560 || root.height < 640
    readonly property bool defaultShellBorderEnabled: true
    readonly property int defaultShellBorderWidth: 1
    readonly property int defaultShellBorderRadius: 15
    readonly property bool defaultSyncHyprlandWindowBorders: true
    readonly property bool borderDraftDirty:
        borderDraftEnabled !== defaultShellBorderEnabled
        || borderDraftWidth !== defaultShellBorderWidth
        || borderDraftRadius !== defaultShellBorderRadius
        || borderDraftSync !== defaultSyncHyprlandWindowBorders

    readonly property string coreRecoveryMessage: {
        if (coreRecoveryState === "recovered")
            return qsTr("Your bar settings were restored from their last known good copy because the main settings file could not be read.");
        if (coreRecoveryState === "defaulted")
            return qsTr("Your bar settings could not be recovered, so their safe defaults are in use. Review them before continuing.");
        return "";
    }
    readonly property string componentRecoveryMessage: {
        if (componentRecoveryState === "recovered")
            return qsTr("Your workspace choices were restored from their last known good copy because the main component settings file could not be read.");
        if (componentRecoveryState === "defaulted")
            return qsTr("Your workspace choices could not be recovered, so safe defaults are in use. Review them before continuing.");
        return "";
    }
    readonly property string componentWarningMessage: {
        if (componentRecoveryState === "unsupported") {
            return componentServiceAvailable
                ? qsTr("Workspace settings have a protected newer-format recovery copy. The current choices remain visible but read-only so HyprShelld does not overwrite it.")
                : qsTr("Workspace settings use a newer format that this HyprShelld version cannot read. The file is preserved, and core Bar changes remain available.");
        }
        if (!componentServiceAvailable)
            return qsTr("Workspace settings are unavailable. Their displayed values may be stale, and workspace changes are disabled until the component settings reconnect.");
        if (!componentCatalogAvailable)
            return qsTr("The component catalog is unavailable. Core Bar changes still work, but workspace changes are disabled until the catalog reconnects.");
        if (!componentWritable)
            return qsTr("Workspace settings are currently read-only. Core Bar changes remain available.");
        if (!workspaceInstanceAvailable)
            return qsTr("The built-in workspace switcher configuration is unavailable. Core Bar changes remain available.");
        if (!workspaceFeatureAvailable)
            return qsTr("The built-in workspace switcher placement is unavailable. Its settings remain read-only, and core Bar changes remain available.");
        return "";
    }
    readonly property bool coreServiceWarningVisible:
        !coreServiceAvailable
    readonly property bool componentServiceWarningVisible:
        componentWarningMessage.length > 0
    readonly property bool coreRecoveryWarningVisible:
        coreRecoveryMessage.length > 0
    readonly property bool componentRecoveryWarningVisible:
        componentRecoveryMessage.length > 0
    readonly property bool coreConfigurationErrorVisible:
        coreErrorText.length > 0
    readonly property bool componentConfigurationErrorVisible:
        componentErrorText.length > 0
    readonly property bool coreControlsEnabled:
        coreServiceAvailable && !coreBusy
    readonly property bool workspaceControlsEnabled:
        componentServiceAvailable
        && componentCatalogAvailable
        && componentWritable
        && workspaceInstanceAvailable
        && workspaceFeatureAvailable
        && workspaceFeatureEnabled
        && !componentBusy

    signal barHeightRequested(int height)
    signal resetBarHeightRequested()
    signal sharedBorderRequested(
        bool enabled,
        int width,
        int radius,
        bool syncHyprlandWindowBorders
    )
    signal resetSharedBorderRequested()
    signal workspaceSwitcherRequested(
        bool showIdentifiers,
        bool showNames,
        bool showApplications,
        int maximumApplications,
        bool occupiedOnly,
        string scrollMode
    )
    signal resetWorkspaceSwitcherRequested()

    function synchronizeBorderDraft() {
        root.borderDraftEnabled = root.shellBorderEnabled;
        root.borderDraftWidth = root.shellBorderWidth;
        root.borderDraftRadius = root.shellBorderRadius;
        root.borderDraftSync = root.syncHyprlandWindowBorders;
    }

    function requestSharedBorder(enabled, width, radius, sync) {
        if (!root.coreControlsEnabled) {
            root.synchronizeBorderDraft();
            return;
        }

        root.borderDraftEnabled = enabled;
        root.borderDraftWidth = Math.max(0, Math.min(20, width));
        root.borderDraftRadius = Math.max(0, Math.min(20, radius));
        root.borderDraftSync = sync;
        root.sharedBorderRequested(
            root.borderDraftEnabled,
            root.borderDraftWidth,
            root.borderDraftRadius,
            root.borderDraftSync
        );
    }

    function resetSharedBorder() {
        if (!root.coreControlsEnabled)
            return;

        root.borderDraftEnabled = root.defaultShellBorderEnabled;
        root.borderDraftWidth = root.defaultShellBorderWidth;
        root.borderDraftRadius = root.defaultShellBorderRadius;
        root.borderDraftSync = root.defaultSyncHyprlandWindowBorders;
        root.resetSharedBorderRequested();
    }

    onShellBorderEnabledChanged: {
        if (!root.coreBusy)
            root.synchronizeBorderDraft();
    }
    onShellBorderWidthChanged: {
        if (!root.coreBusy)
            root.synchronizeBorderDraft();
    }
    onShellBorderRadiusChanged: {
        if (!root.coreBusy)
            root.synchronizeBorderDraft();
    }
    onSyncHyprlandWindowBordersChanged: {
        if (!root.coreBusy)
            root.synchronizeBorderDraft();
    }
    onCoreBusyChanged: {
        if (!root.coreBusy)
            root.synchronizeBorderDraft();
    }

    Component.onCompleted: root.synchronizeBorderDraft()

    background: Rectangle {
        color: root.palette.window
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: root.compactPreview
            ? Math.min(root.contentTopMargin, 12)
            : root.contentTopMargin
        spacing: root.compactPreview ? 12 : 20

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: stickyPreview.implicitHeight
            Layout.minimumHeight: stickyPreview.implicitHeight

            ColumnLayout {
                id: stickyPreview

                objectName: "barStickyPreview"
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.max(0, Math.min(parent.width - 48, 980))
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Desktop preview")
                        color: root.palette.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        text: qsTr("%1 px").arg(heightControl.previewValue)
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                    }
                }

                BarPreview {
                    id: barPreview

                    objectName: "barPreview"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 286
                    Layout.minimumHeight: 286
                    barHeight: heightControl.previewValue
                    shellBorderEnabled: root.borderDraftEnabled
                    shellBorderWidth: root.borderDraftWidth
                    shellBorderRadius: root.borderDraftRadius
                    adjusting: heightControl.adjusting
                    configurationAvailable: root.coreServiceAvailable
                    animationsEnabled: root.previewAnimationsEnabled
                    workspaceShowIdentifiers:
                        root.workspaceShowIdentifiers
                    workspaceShowNames: root.workspaceShowNames
                    workspaceShowApplications:
                        root.workspaceShowApplications
                    workspaceMaximumApplications:
                        root.workspaceMaximumApplications
                    workspaceOccupiedOnly: root.workspaceOccupiedOnly
                    workspaceScrollMode: root.workspaceScrollMode
                    workspaceComponentEnabled:
                        root.workspacePreviewEnabled
                }
            }
        }

        ScrollView {
            objectName: "barOptionsScrollView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                objectName: "barOptionsContent"
                x: Math.max(24, (root.width - width) / 2)
                width: Math.max(0, Math.min(root.width - 48, 980))
                spacing: 20

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: qsTr("Bar")
                            color: root.palette.text
                            font.pixelSize: 30
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Shape how the bar looks and uses space on every display.")
                            color: root.palette.placeholderText
                            font.pixelSize: 14
                            wrapMode: Text.Wrap
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: liveStatusLabel.implicitWidth + 34
                        Layout.preferredHeight: 34
                        radius: 17
                        color: Qt.rgba(
                            root.palette.highlight.r,
                            root.palette.highlight.g,
                            root.palette.highlight.b,
                            0.12
                        )
                        border.color: Qt.rgba(
                            root.palette.highlight.r,
                            root.palette.highlight.g,
                            root.palette.highlight.b,
                            0.36
                        )

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 7

                            Rectangle {
                                Layout.preferredWidth: 7
                                Layout.preferredHeight: 7
                                radius: width / 2
                                color: root.palette.highlight
                            }

                            Label {
                                id: liveStatusLabel

                                text: qsTr("Live preview")
                                color: root.palette.text
                                font.pixelSize: 12
                            }
                        }
                    }
                }

                Frame {
                    id: coreServiceWarning

                    objectName: "coreServiceWarning"
                    Layout.fillWidth: true
                    visible: root.coreServiceWarningVisible
                    padding: 16

                    background: Rectangle {
                        color: "#33251a"
                        radius: 12
                        border.color: "#8bf6ad55"
                    }

                    Label {
                        anchors.fill: parent
                        text: qsTr("Bar settings are unavailable. The displayed size and border may be stale, and core Bar changes are disabled until settings reconnect.")
                        color: "#ffd5a1"
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }
                }

                Frame {
                    id: componentServiceWarning

                    objectName: "componentServiceWarning"
                    Layout.fillWidth: true
                    visible: root.componentServiceWarningVisible
                    padding: 16

                    background: Rectangle {
                        color: "#33251a"
                        radius: 12
                        border.color: "#8bf6ad55"
                    }

                    Label {
                        objectName: "componentServiceWarningLabel"
                        anchors.fill: parent
                        text: root.componentWarningMessage
                        color: "#ffd5a1"
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }
                }

                Frame {
                    id: coreRecoveryWarning

                    objectName: "coreRecoveryWarning"
                    Layout.fillWidth: true
                    visible: root.coreRecoveryWarningVisible
                    padding: 16

                    background: Rectangle {
                        color: root.coreRecoveryState === "defaulted"
                            ? "#382125"
                            : "#1c2f34"
                        radius: 12
                        border.color: root.coreRecoveryState === "defaulted"
                            ? "#8bfb7185"
                            : "#8b63d7e6"
                    }

                    Label {
                        anchors.fill: parent
                        text: root.coreRecoveryMessage
                        color: root.coreRecoveryState === "defaulted"
                            ? "#ffb8c3"
                            : "#b9eef4"
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }
                }

                Frame {
                    id: componentRecoveryWarning

                    objectName: "componentRecoveryWarning"
                    Layout.fillWidth: true
                    visible: root.componentRecoveryWarningVisible
                    padding: 16

                    background: Rectangle {
                        color: root.componentRecoveryState === "defaulted"
                            ? "#382125"
                            : "#1c2f34"
                        radius: 12
                        border.color:
                            root.componentRecoveryState === "defaulted"
                                ? "#8bfb7185"
                                : "#8b63d7e6"
                    }

                    Label {
                        anchors.fill: parent
                        text: root.componentRecoveryMessage
                        color: root.componentRecoveryState === "defaulted"
                            ? "#ffb8c3"
                            : "#b9eef4"
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }
                }

                Frame {
                    id: coreConfigurationError

                    objectName: "coreConfigurationError"
                    Layout.fillWidth: true
                    visible: root.coreConfigurationErrorVisible
                    padding: 16

                    background: Rectangle {
                        color: "#382125"
                        radius: 12
                        border.color: "#8bfb7185"
                    }

                    Label {
                        objectName: "coreConfigurationErrorLabel"
                        anchors.fill: parent
                        text: qsTr("The Bar settings could not be saved. %1").arg(
                            root.coreErrorText
                        )
                        color: "#ffb8c3"
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }
                }

                Frame {
                    id: componentConfigurationError

                    objectName: "componentConfigurationError"
                    Layout.fillWidth: true
                    visible: root.componentConfigurationErrorVisible
                    padding: 16

                    background: Rectangle {
                        color: "#382125"
                        radius: 12
                        border.color: "#8bfb7185"
                    }

                    Label {
                        objectName: "componentConfigurationErrorLabel"
                        anchors.fill: parent
                        text: qsTr("The workspace settings could not be saved. %1").arg(
                            root.componentErrorText
                        )
                        color: "#ffb8c3"
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    padding: 22

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                text: qsTr("Size")
                                color: root.palette.text
                                font.pixelSize: 18
                                font.weight: Font.DemiBold
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Changes are previewed immediately and saved when you release the control.")
                                color: root.palette.placeholderText
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                            }
                        }

                        BarHeightControl {
                            id: heightControl

                            objectName: "barHeightControl"
                            Layout.fillWidth: true
                            value: root.barHeight
                            minimumValue: root.minimumBarHeight
                            maximumValue: root.maximumBarHeight
                            defaultValue: root.defaultBarHeight
                            busy: !root.coreControlsEnabled
                            enabled: root.coreControlsEnabled

                            onValueRequested: value => root.barHeightRequested(value)
                            onResetRequested: root.resetBarHeightRequested()
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Changes are saved automatically and applied to every bar.")
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }

                Frame {
                    objectName: "sharedBorderSettingsCard"
                    Layout.fillWidth: true
                    padding: 22

                    background: Rectangle {
                        color: root.palette.base
                        radius: 16
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                text: qsTr("Border")
                                color: root.palette.text
                                font.pixelSize: 18
                                font.weight: Font.DemiBold
                                Accessible.role: Accessible.Heading
                                Accessible.name: text
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Use one HyprShelld border shape for the bar and synchronized Hyprland windows.")
                                color: root.palette.placeholderText
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Show border")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }

                                Label {
                                    objectName: "shellBorderEnabledDescription"
                                    Layout.fillWidth: true
                                    text: qsTr("Show the shared border line on the bar and, while synced, on Hyprland windows. Width and radius are kept when hidden.")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }

                            Switch {
                                id: shellBorderEnabledControl

                                objectName: "shellBorderEnabled"
                                implicitHeight: Math.max(
                                    44,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                checked: root.borderDraftEnabled
                                enabled: root.coreControlsEnabled
                                text: checked ? qsTr("On") : qsTr("Off")
                                Accessible.name: qsTr("Show shared border on the bar and synchronized windows")

                                onClicked: root.requestSharedBorder(
                                    checked,
                                    root.borderDraftWidth,
                                    root.borderDraftRadius,
                                    root.borderDraftSync
                                )
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.width < 720 ? 1 : 2
                            columnSpacing: 22
                            rowSpacing: 14

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Border width")
                                        color: root.palette.text
                                        font.pixelSize: 14
                                        font.weight: Font.Medium
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Thickness in logical pixels.")
                                        color: root.palette.placeholderText
                                        font.pixelSize: 12
                                        wrapMode: Text.Wrap
                                    }
                                }

                                SpinBox {
                                    id: shellBorderWidthControl

                                    objectName: "shellBorderWidth"
                                    Layout.preferredWidth: 104
                                    implicitHeight: Math.max(
                                        44,
                                        implicitBackgroundHeight,
                                        implicitContentHeight + topPadding + bottomPadding
                                    )
                                    from: 0
                                    to: 20
                                    value: root.borderDraftWidth
                                    editable: false
                                    enabled: root.coreControlsEnabled
                                    Accessible.name: qsTr("Shared border width")

                                    onValueModified: root.requestSharedBorder(
                                        root.borderDraftEnabled,
                                        value,
                                        root.borderDraftRadius,
                                        root.borderDraftSync
                                    )
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Corner radius")
                                        color: root.palette.text
                                        font.pixelSize: 14
                                        font.weight: Font.Medium
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Round the bar and synchronized window corners.")
                                        color: root.palette.placeholderText
                                        font.pixelSize: 12
                                        wrapMode: Text.Wrap
                                    }
                                }

                                SpinBox {
                                    id: shellBorderRadiusControl

                                    objectName: "shellBorderRadius"
                                    Layout.preferredWidth: 104
                                    implicitHeight: Math.max(
                                        44,
                                        implicitBackgroundHeight,
                                        implicitContentHeight + topPadding + bottomPadding
                                    )
                                    from: 0
                                    to: 20
                                    value: root.borderDraftRadius
                                    editable: false
                                    enabled: root.coreControlsEnabled
                                    Accessible.name: qsTr("Shared corner radius")

                                    onValueModified: root.requestSharedBorder(
                                        root.borderDraftEnabled,
                                        root.borderDraftWidth,
                                        value,
                                        root.borderDraftSync
                                    )
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Sync Hyprland window borders")
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }

                                Label {
                                    objectName: "sharedBorderAuthorityMessage"
                                    Layout.fillWidth: true
                                    text: root.borderDraftSync
                                        ? qsTr("HyprShelld controls Hyprland's window border width and corner radius. The matching Hyprland options remain read-only while synchronization is on.")
                                        : qsTr("Hyprland can use its own window border override without changing the HyprShelld bar.")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                }
                            }

                            CheckBox {
                                id: syncHyprlandWindowBordersControl

                                objectName: "syncHyprlandWindowBorders"
                                implicitHeight: Math.max(
                                    44,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                checked: root.borderDraftSync
                                enabled: root.coreControlsEnabled
                                text: qsTr("Sync")
                                Accessible.name: qsTr("Sync Hyprland window borders with HyprShelld")

                                onClicked: root.requestSharedBorder(
                                    root.borderDraftEnabled,
                                    root.borderDraftWidth,
                                    root.borderDraftRadius,
                                    checked
                                )
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Each change is saved as one shared border update.")
                                color: root.palette.placeholderText
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }

                            Button {
                                objectName: "resetSharedBorder"
                                implicitHeight: Math.max(
                                    44,
                                    implicitBackgroundHeight,
                                    implicitContentHeight + topPadding + bottomPadding
                                )
                                text: qsTr("Reset")
                                enabled: root.coreControlsEnabled
                                    && root.borderDraftDirty
                                Accessible.name: qsTr("Reset shared border settings")

                                onClicked: root.resetSharedBorder()
                            }
                        }
                    }
                }

                WorkspaceSwitcherSettings {
                    objectName: "workspaceSwitcherSettingsCard"
                    Layout.fillWidth: true
                    showIdentifiers: root.workspaceShowIdentifiers
                    showNames: root.workspaceShowNames
                    showApplications: root.workspaceShowApplications
                    maximumApplications: root.workspaceMaximumApplications
                    occupiedOnly: root.workspaceOccupiedOnly
                    scrollMode: root.workspaceScrollMode
                    controlsEnabled: root.workspaceControlsEnabled
                    featureAvailable: root.workspaceFeatureAvailable
                    featureEnabled: root.workspaceFeatureEnabled
                    synchronizationBusy: root.componentBusy
                    synchronizationError: root.componentErrorText

                    onWorkspaceSwitcherRequested: (
                        showIdentifiers,
                        showNames,
                        showApplications,
                        maximumApplications,
                        occupiedOnly,
                        scrollMode
                    ) => root.workspaceSwitcherRequested(
                        showIdentifiers,
                        showNames,
                        showApplications,
                        maximumApplications,
                        occupiedOnly,
                        scrollMode
                    )
                    onResetRequested: root.resetWorkspaceSwitcherRequested()
                }

                Item {
                    Layout.preferredHeight: 12
                }
            }
        }
    }
}
