import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property string labelMode
    required property bool showApplications
    required property int maximumApplications
    required property bool occupiedOnly
    required property string scrollMode
    property bool controlsEnabled: false
    property bool featureAvailable: true
    property bool featureEnabled: true

    readonly property bool effectiveControlsEnabled:
        controlsEnabled && featureAvailable && featureEnabled
    readonly property bool disabledMessageVisible:
        featureAvailable && !featureEnabled

    signal workspaceSwitcherRequested(
        string labelMode,
        bool showApplications,
        int maximumApplications,
        bool occupiedOnly,
        string scrollMode
    )
    signal resetRequested()

    function labelModeIndex(mode) {
        switch (mode) {
        case "compact":
            return 1;
        case "names":
            return 2;
        default:
            return 0;
        }
    }

    function scrollModeIndex(mode) {
        switch (mode) {
        case "normal":
            return 1;
        case "reversed":
            return 2;
        default:
            return 0;
        }
    }

    function requestSnapshot(
        labelMode,
        showApplications,
        maximumApplications,
        occupiedOnly,
        scrollMode
    ) {
        if (!root.effectiveControlsEnabled)
            return;
        if (labelMode === root.labelMode
                && showApplications === root.showApplications
                && maximumApplications === root.maximumApplications
                && occupiedOnly === root.occupiedOnly
                && scrollMode === root.scrollMode) {
            return;
        }
        root.workspaceSwitcherRequested(
            labelMode,
            showApplications,
            maximumApplications,
            occupiedOnly,
            scrollMode
        );
    }

    objectName: "workspaceSwitcherSettings"
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
                text: qsTr("Workspaces")
                color: root.palette.text
                font.pixelSize: 18
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Choose what each workspace indicator shows and how you move between them.")
                color: root.palette.placeholderText
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }
        }

        Frame {
            objectName: "workspaceFeatureDisabledMessage"
            Layout.fillWidth: true
            visible: root.disabledMessageVisible
            padding: 14

            background: Rectangle {
                color: Qt.rgba(
                    root.palette.text.r,
                    root.palette.text.g,
                    root.palette.text.b,
                    0.05
                )
                radius: 11
                border.color: root.palette.mid
            }

            Label {
                objectName: "workspaceFeatureDisabledMessageLabel"
                anchors.fill: parent
                text: qsTr("This feature has been disabled. Enable it from Components → Bar Widgets to change these settings.")
                color: root.palette.text
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }

        ColumnLayout {
            id: settingsControls

            objectName: "workspaceSettingsControls"
            Layout.fillWidth: true
            spacing: 18
            opacity: root.featureEnabled && root.featureAvailable ? 1 : 0.42

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: qsTr("Label style")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Numbers fills the current circle and uses smaller hollow numbered circles for the rest. Compact hides identifiers, and Names extend from a circular anchor.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }

            ComboBox {
                id: labelModeControl

                objectName: "workspaceLabelMode"
                Layout.preferredWidth: 148
                enabled: root.effectiveControlsEnabled
                model: [qsTr("Numbers"), qsTr("Compact"), qsTr("Names")]
                currentIndex: root.labelModeIndex(root.labelMode)
                Accessible.name: qsTr("Workspace label style")

                onActivated: index => {
                    const modes = ["numbers", "compact", "names"];
                    root.requestSnapshot(
                        modes[index],
                        root.showApplications,
                        root.maximumApplications,
                        root.occupiedOnly,
                        root.scrollMode
                    );
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: qsTr("Show application icons")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Append the applications open on each workspace to its anchor.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }

            Switch {
                id: showApplicationsControl

                objectName: "workspaceShowApplications"
                checked: root.showApplications
                enabled: root.effectiveControlsEnabled
                Accessible.name: qsTr("Show application icons")

                onToggled: root.requestSnapshot(
                    root.labelMode,
                    checked,
                    root.maximumApplications,
                    root.occupiedOnly,
                    root.scrollMode
                )
            }
        }

        RowLayout {
            objectName: "workspaceMaximumApplicationsRow"
            Layout.fillWidth: true
            visible: root.showApplications
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: qsTr("Maximum icons")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Extra applications are summarized after this limit.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }

            SpinBox {
                id: maximumApplicationsControl

                objectName: "workspaceMaximumApplications"
                from: 1
                to: 5
                value: root.maximumApplications
                editable: false
                enabled: root.effectiveControlsEnabled
                Accessible.name: qsTr("Maximum workspace application icons")

                onValueModified: root.requestSnapshot(
                    root.labelMode,
                    root.showApplications,
                    value,
                    root.occupiedOnly,
                    root.scrollMode
                )
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: qsTr("Show occupied only")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Show workspaces with open windows while keeping the current workspace visible.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }

            Switch {
                id: occupiedOnlyControl

                objectName: "workspaceOccupiedOnly"
                checked: root.occupiedOnly
                enabled: root.effectiveControlsEnabled
                Accessible.name: qsTr("Show occupied workspaces only")

                onToggled: root.requestSnapshot(
                    root.labelMode,
                    root.showApplications,
                    root.maximumApplications,
                    checked,
                    root.scrollMode
                )
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: qsTr("Scroll to switch")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Choose the direction used when scrolling over the switcher.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }

            ComboBox {
                id: scrollModeControl

                objectName: "workspaceScrollMode"
                Layout.preferredWidth: 148
                enabled: root.effectiveControlsEnabled
                model: [qsTr("Off"), qsTr("Normal"), qsTr("Reversed")]
                currentIndex: root.scrollModeIndex(root.scrollMode)
                Accessible.name: qsTr("Workspace scroll direction")

                onActivated: index => {
                    const modes = ["disabled", "normal", "reversed"];
                    root.requestSnapshot(
                        root.labelMode,
                        root.showApplications,
                        root.maximumApplications,
                        root.occupiedOnly,
                        modes[index]
                    );
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                text: qsTr("Changes are saved automatically and applied to every bar.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Button {
                objectName: "resetWorkspaceSwitcher"
                text: qsTr("Reset")
                enabled: root.effectiveControlsEnabled
                Accessible.name: qsTr("Reset workspace switcher settings")

                onClicked: root.resetRequested()
            }
        }
        }
    }
}
