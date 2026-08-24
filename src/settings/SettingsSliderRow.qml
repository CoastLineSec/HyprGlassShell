pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string title: ""
    property string description: ""
    property real from: 0
    property real to: 1
    property real value: 0
    property real stepSize: 0.1
    property int decimals: 1
    property string valueSuffix: ""
    property real controlWidth: 190
    property string controlObjectName: ""
    property string valueObjectName: ""
    property string accessibleName: title
    property real minimumTargetSize: 44

    signal valueModified(real value)

    spacing: 16

    function formattedValue() {
        const numericValue = Number(root.value);
        if (!Number.isFinite(numericValue))
            return "";
        const snapped = Math.round(numericValue / root.stepSize)
            * root.stepSize;
        const text = Math.abs(numericValue - snapped) > 0.000000001
            ? String(numericValue)
            : numericValue.toFixed(root.decimals);
        return text + root.valueSuffix;
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 2

        Label {
            Layout.fillWidth: true
            text: root.title
            color: root.palette.text
            font.pixelSize: 14
            font.weight: Font.Medium
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Label {
            Layout.fillWidth: true
            text: root.description
            color: root.palette.placeholderText
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }
    }

    ColumnLayout {
        Layout.preferredWidth: root.controlWidth
        Layout.maximumWidth: root.controlWidth
        spacing: 0

        Label {
            objectName: root.valueObjectName
            Layout.alignment: Qt.AlignRight
            text: root.formattedValue()
            color: root.palette.text
            font.pixelSize: 12
            textFormat: Text.PlainText
            Accessible.ignored: true
        }

        Slider {
            objectName: root.controlObjectName
            Layout.fillWidth: true
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            from: root.from
            to: root.to
            value: root.value
            stepSize: root.stepSize
            snapMode: Slider.SnapAlways
            enabled: root.enabled
            Accessible.name: root.accessibleName

            onMoved: root.valueModified(value)
        }
    }
}
