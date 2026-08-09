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

    readonly property string coreRecoveryMessage: {
        if (coreRecoveryState === "recovered")
            return qsTr("Your bar size was restored from its last known good copy because the main settings file could not be read.");
        if (coreRecoveryState === "defaulted")
            return qsTr("Your bar size could not be recovered, so its safe default is in use. Review it before continuing.");
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
                : qsTr("Workspace settings use a newer format that this HyprShelld version cannot read. The file is preserved, and bar size changes remain available.");
        }
        if (!componentServiceAvailable)
            return qsTr("Workspace settings are unavailable. Their displayed values may be stale, and workspace changes are disabled until the component settings reconnect.");
        if (!componentCatalogAvailable)
            return qsTr("The component catalog is unavailable. Bar size changes still work, but workspace changes are disabled until the catalog reconnects.");
        if (!componentWritable)
            return qsTr("Workspace settings are currently read-only. Bar size changes remain available.");
        if (!workspaceInstanceAvailable)
            return qsTr("The built-in workspace switcher configuration is unavailable. Bar size changes remain available.");
        if (!workspaceFeatureAvailable)
            return qsTr("The built-in workspace switcher placement is unavailable. Its settings remain read-only, and bar size changes remain available.");
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
    signal workspaceSwitcherRequested(
        bool showIdentifiers,
        bool showNames,
        bool showApplications,
        int maximumApplications,
        bool occupiedOnly,
        string scrollMode
    )
    signal resetWorkspaceSwitcherRequested()

    background: Rectangle {
        color: root.palette.window
    }

    ScrollView {
        objectName: "barSettingsScrollView"
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            x: Math.max(24, (root.width - width) / 2)
            y: root.contentTopMargin
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
                    text: qsTr("Bar size settings are unavailable. The displayed size may be stale, and size changes are disabled until core settings reconnect.")
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
                    text: qsTr("The bar size could not be saved. %1").arg(
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

            ColumnLayout {
                Layout.fillWidth: true
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
                    barHeight: heightControl.previewValue
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
