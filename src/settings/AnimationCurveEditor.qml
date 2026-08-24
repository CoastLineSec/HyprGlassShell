pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property var curve: null
    property bool controlsEnabled: false
    property int referenceCount: 0
    property string curveIssue: ""
    property real minimumTargetSize: 44
    property bool compact: width < 560

    signal closeRequested()
    signal propertyModified(string id, string propertyName, var value)
    signal pointModified(
        string id, int pointIndex, int coordinateIndex, var value
    )

    spacing: 14

    function curveId() {
        return root.curve && typeof root.curve.id === "string"
            ? root.curve.id : "";
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        Button {
            objectName: "closeAnimationCurveEditorButton"
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: qsTr("Back to animations")
            Accessible.name: qsTr("Close the custom curve editor")
            onClicked: root.closeRequested()
        }

        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            horizontalAlignment: Text.AlignRight
            text: qsTr("Editing custom curve")
            color: root.palette.placeholderText
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }
    }

    Frame {
        objectName: "animationCurveIdentityCard"
        Layout.fillWidth: true
        padding: root.compact ? 14 : 18

        background: Rectangle {
            color: root.palette.base
            radius: 16
            border.color: root.palette.mid
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: qsTr("Curve identity")
                color: root.palette.text
                font.pixelSize: 17
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("The internal record ID is preserved automatically. Curve names and types are read-only until HyprShelld has a verified compositor-restart workflow.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Curve name (read only)")
                color: root.palette.text
                font.weight: Font.Medium
                textFormat: Text.PlainText
            }

            TextField {
                objectName: "animationCurveName"
                Layout.fillWidth: true
                implicitHeight: root.minimumTargetSize
                readOnly: true
                selectByMouse: true
                text: root.curve && typeof root.curve.name === "string"
                    ? root.curve.name : ""
                Accessible.name: qsTr("Custom curve name, read only")
            }

            Label {
                objectName: "animationCurveBuiltinShadowMessage"
                Layout.fillWidth: true
                visible: !!root.curve
                    && (root.curve.name === "default"
                        || root.curve.name === "linear")
                text: qsTr("For managed animation rules, this existing custom curve makes %1 resolve to its authored %2 type. Hyprland retains the opposite-type built-in in its separate runtime map. Settings can tune this curve's parameters and order, but cannot rename, remove, or change its type.").arg(
                    root.curve ? root.curve.name : ""
                ).arg(root.curve && root.curve.type === "spring"
                    ? qsTr("spring") : qsTr("Bezier"))
                color: root.palette.highlight
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Settings can tune this curve's parameters and position in the ordered collection. Adding, removing, renaming, and changing a curve type are unavailable until a verified compositor-restart workflow exists. Reset restores the saved curve collection.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Label {
                objectName: "animationCurveValidationMessage"
                Layout.fillWidth: true
                visible: root.curveIssue.length > 0
                text: root.curveIssue
                color: "#ffb8c3"
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
        }
    }

    Frame {
        objectName: "animationCurveParametersCard"
        Layout.fillWidth: true
        padding: root.compact ? 14 : 18

        background: Rectangle {
            color: root.palette.base
            radius: 16
            border.color: root.palette.mid
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: qsTr("Curve shape")
                color: root.palette.text
                font.pixelSize: 17
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Curve type (read only)")
                color: root.palette.text
                font.weight: Font.Medium
                textFormat: Text.PlainText
            }

            TextField {
                objectName: "animationCurveType"
                Layout.fillWidth: true
                implicitHeight: root.minimumTargetSize
                readOnly: true
                selectByMouse: true
                text: root.curve && root.curve.type === "spring"
                    ? qsTr("Spring") : qsTr("Bezier")
                Accessible.name: qsTr("Custom curve type, read only")
            }

            Label {
                Layout.fillWidth: true
                visible: !!root.curve && root.curve.type === "bezier"
                text: qsTr("Set the x and y coordinate of both Bezier control points. Each coordinate accepts −1 through 2.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            GridLayout {
                Layout.fillWidth: true
                visible: !!root.curve && root.curve.type === "bezier"
                columns: root.compact ? 2 : 4
                columnSpacing: 10
                rowSpacing: 10

                Repeater {
                    model: [
                        { point: 0, coordinate: 0, label: qsTr("Point 1 x") },
                        { point: 0, coordinate: 1, label: qsTr("Point 1 y") },
                        { point: 1, coordinate: 0, label: qsTr("Point 2 x") },
                        { point: 1, coordinate: 1, label: qsTr("Point 2 y") }
                    ]

                    ColumnLayout {
                        required property var modelData

                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            Layout.fillWidth: true
                            text: parent.modelData.label
                            color: root.palette.text
                            font.pixelSize: 12
                            textFormat: Text.PlainText
                        }

                        RuleDecimalField {
                            objectName: "animationCurvePoint"
                                + String(parent.modelData.point + 1)
                                + (parent.modelData.coordinate === 0
                                    ? "X" : "Y")
                            Layout.fillWidth: true
                            value: root.curve && Array.isArray(root.curve.points)
                                ? root.curve.points[parent.modelData.point][
                                    parent.modelData.coordinate
                                ] : 0
                            minimumValue: -1
                            maximumValue: 2
                            enabled: root.controlsEnabled
                            accessibleName: parent.modelData.label
                            minimumTargetSize: root.minimumTargetSize
                            onValueModified: value => root.pointModified(
                                root.curveId(), parent.modelData.point,
                                parent.modelData.coordinate, value
                            )
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: !!root.curve && root.curve.type === "spring"
                text: qsTr("Set stiffness, dampening, and mass. Every value must be greater than 0.5 and no greater than 1,000,000.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Repeater {
                model: [
                    { key: "stiffness", label: qsTr("Stiffness") },
                    { key: "dampening", label: qsTr("Dampening") },
                    { key: "mass", label: qsTr("Mass") }
                ]

                ColumnLayout {
                    required property var modelData

                    Layout.fillWidth: true
                    visible: !!root.curve && root.curve.type === "spring"
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: parent.modelData.label
                        color: root.palette.text
                        font.weight: Font.Medium
                        textFormat: Text.PlainText
                    }

                    RuleDecimalField {
                        objectName: "animationCurve"
                            + parent.modelData.key.charAt(0).toUpperCase()
                            + parent.modelData.key.slice(1)
                        Layout.fillWidth: true
                        value: root.curve
                            ? root.curve[parent.modelData.key] : ""
                        minimumValue: 0.5
                        maximumValue: 1000000
                        enabled: root.controlsEnabled
                        accessibleName: parent.modelData.label
                        minimumTargetSize: root.minimumTargetSize
                        onValueModified: value => root.propertyModified(
                            root.curveId(), parent.modelData.key, value
                        )
                    }
                }
            }
        }
    }

    Frame {
        objectName: "animationCurveStructureCard"
        Layout.fillWidth: true
        padding: root.compact ? 14 : 18

        background: Rectangle {
            color: root.palette.base
            radius: 16
            border.color: root.palette.mid
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: root.referenceCount === 0
                    ? qsTr("This curve is not referenced by an animation rule. Its structure is still preserved.")
                    : root.referenceCount === 1
                        ? qsTr("One animation rule uses this curve. Its name and type stay preserved.")
                        : qsTr("%1 animation rules use this curve. Its name and type stay preserved.").arg(root.referenceCount)
                color: root.palette.placeholderText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
        }
    }

    Item { Layout.preferredHeight: 12 }
}
