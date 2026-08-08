import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id: root

    required property int value
    required property int minimumValue
    required property int maximumValue
    required property int defaultValue
    property bool busy: false
    property string errorText: ""
    readonly property int previewValue: Math.round(slider.value)
    readonly property bool adjusting: slider.pressed

    signal valueRequested(int value)
    signal resetRequested()

    implicitWidth: 360
    implicitHeight: content.implicitHeight

    Connections {
        target: root

        function onValueChanged() {
            if (!slider.pressed)
                slider.value = root.value;
        }

        function onBusyChanged() {
            if (!root.busy && !slider.pressed)
                slider.value = root.value;
        }
    }

    contentItem: ColumnLayout {
        id: content

        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Bar height")
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("%1 px").arg(Math.round(slider.value))
            }
        }

        Slider {
            id: slider

            objectName: "barHeightSlider"
            Layout.fillWidth: true
            from: root.minimumValue
            to: root.maximumValue
            stepSize: 1
            snapMode: Slider.SnapAlways
            value: root.value
            enabled: !root.busy
            Accessible.name: qsTr("Bar height")

            onMoved: {
                if (!pressed && Math.round(value) !== root.value)
                    root.valueRequested(Math.round(value));
            }

            onPressedChanged: {
                if (!pressed && Math.round(value) !== root.value)
                    root.valueRequested(Math.round(value));
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                visible: root.errorText.length > 0
                text: root.errorText
                wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }

            Button {
                text: qsTr("Reset")
                enabled: !root.busy && root.value !== root.defaultValue
                Accessible.name: qsTr("Reset bar height")
                onClicked: root.resetRequested()
            }
        }
    }
}
