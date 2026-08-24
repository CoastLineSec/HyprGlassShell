pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property var curves: []
    property var animations: []
    property bool animationsEnabled: true
    property bool controlsEnabled: false
    property bool inspectionEnabled: false
    property bool draftDirty: false
    property bool draftValid: true
    property real minimumTargetSize: 44

    signal editCurveRequested(string id)
    signal moveCurveRequested(string id, int offset)
    signal addAnimationRequested()
    signal editAnimationRequested(string id)
    signal enabledAnimationRequested(string id, bool enabled)
    signal moveAnimationRequested(string id, int offset)
    signal removeAnimationRequested(string id)
    spacing: 16

    function curveReferenceCount(name) {
        if (typeof name !== "string")
            return 0;
        let count = 0;
        for (const animation of root.animations) {
            if (animation && animation.curve === name)
                ++count;
        }
        return count;
    }

    function curveTitle(curve) {
        return curve && typeof curve.name === "string"
            && curve.name.length > 0 ? curve.name : qsTr("Unnamed curve");
    }

    function curveSummary(curve) {
        const type = curve && curve.type === "spring"
            ? qsTr("Spring") : qsTr("Bezier");
        const references = root.curveReferenceCount(
            curve && typeof curve.name === "string" ? curve.name : ""
        );
        return references === 1
            ? qsTr("%1 · used by 1 rule").arg(type)
            : qsTr("%1 · used by %2 rules").arg(type).arg(references);
    }

    function animationTitle(animation) {
        return animation && typeof animation.name === "string"
            && animation.name.length > 0
            ? animation.name : qsTr("Choose an animation leaf");
    }

    function animationSummary(animation) {
        if (!animation)
            return "";
        const curve = typeof animation.curve === "string"
            && animation.curve.length > 0
            ? animation.curve : qsTr("no curve");
        const speed = typeof animation.speed === "number"
            ? String(animation.speed) : qsTr("unfinished speed");
        return qsTr("Speed %1 · %2").arg(speed).arg(curve);
    }

    Label {
        objectName: "appearanceAnimationsDormantMessage"
        Layout.fillWidth: true
        visible: !root.animationsEnabled
        text: qsTr("Animations are off. Curves and animation rules remain saved and can be inspected, but enable Animations to edit them. The aggregate Discard, Reset, and Save actions remain available.")
        color: root.palette.placeholderText
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
    }

    Frame {
        objectName: "animationCurvesCard"
        Layout.fillWidth: true
        padding: 18

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
                text: qsTr("Custom curves")
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
                text: qsTr("Tune and reorder existing Bezier or spring curves. Curve names and types remain read-only until HyprShelld has a verified compositor-restart workflow. The default and linear names are always available; an existing same-name custom curve is labeled as an override.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Label {
                Layout.fillWidth: true
                visible: root.curves.length === 0
                text: qsTr("No custom curves are saved. Animation rules can use the built-in default and linear curves. Creating a new curve is not available in Settings yet.")
                color: root.palette.placeholderText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            ListView {
                id: curvesList

                objectName: "animationCurvesList"
                Layout.fillWidth: true
                Layout.preferredHeight: root.curves.length === 0 ? 0 : 300
                visible: root.curves.length > 0
                model: Array.isArray(root.curves) ? root.curves : []
                spacing: 8
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Frame {
                    id: curveCard

                    required property int index
                    required property var modelData

                    objectName: "animationCurveCard" + index
                    width: ListView.view ? ListView.view.width : 0
                    padding: 12

                    background: Rectangle {
                        color: root.palette.window
                        radius: 12
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: root.curveTitle(curveCard.modelData)
                            color: root.palette.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.curveSummary(curveCard.modelData)
                            color: root.palette.placeholderText
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: childrenRect.height
                            spacing: 8

                            Button {
                                objectName: "editAnimationCurveButton"
                                    + curveCard.index
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Edit")
                                enabled: root.inspectionEnabled
                                Accessible.name: qsTr("Edit %1").arg(
                                    root.curveTitle(curveCard.modelData)
                                )
                                onClicked: root.editCurveRequested(
                                    String(curveCard.modelData.id)
                                )
                            }

                            Button {
                                objectName: "moveAnimationCurveUpButton"
                                    + curveCard.index
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Move up")
                                enabled: root.controlsEnabled
                                    && curveCard.index > 0
                                Accessible.name: qsTr("Move %1 up").arg(
                                    root.curveTitle(curveCard.modelData)
                                )
                                onClicked: root.moveCurveRequested(
                                    String(curveCard.modelData.id), -1
                                )
                            }

                            Button {
                                objectName: "moveAnimationCurveDownButton"
                                    + curveCard.index
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Move down")
                                enabled: root.controlsEnabled
                                    && curveCard.index + 1 < curvesList.count
                                Accessible.name: qsTr("Move %1 down").arg(
                                    root.curveTitle(curveCard.modelData)
                                )
                                onClicked: root.moveCurveRequested(
                                    String(curveCard.modelData.id), 1
                                )
                            }

                        }
                    }
                }
            }
        }
    }

    Frame {
        objectName: "animationRulesCard"
        Layout.fillWidth: true
        padding: 18

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
                text: qsTr("Animation rules")
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
                text: qsTr("Author one ordered override for each supported Hyprland animation leaf. Disabled rules retain their speed, curve, and style.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Button {
                objectName: "addAnimationRuleButton"
                Layout.fillWidth: true
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Add animation rule")
                enabled: root.controlsEnabled
                    && root.animations.length < 34
                    && root.animations.length < 256
                Accessible.name: text
                onClicked: root.addAnimationRequested()
            }

            Label {
                Layout.fillWidth: true
                visible: root.animations.length === 0
                text: qsTr("No authored animation rules are saved. Hyprland uses its built-in animation configuration.")
                color: root.palette.placeholderText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            ListView {
                id: animationsList

                objectName: "animationRulesList"
                Layout.fillWidth: true
                Layout.preferredHeight: root.animations.length === 0 ? 0 : 340
                visible: root.animations.length > 0
                model: Array.isArray(root.animations) ? root.animations : []
                spacing: 8
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Frame {
                    id: animationCard

                    required property int index
                    required property var modelData

                    objectName: "animationRuleCard" + index
                    width: ListView.view ? ListView.view.width : 0
                    padding: 12

                    background: Rectangle {
                        color: root.palette.window
                        radius: 12
                        border.color: root.palette.mid
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: root.animationTitle(
                                        animationCard.modelData
                                    )
                                    color: root.palette.text
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.animationSummary(
                                        animationCard.modelData
                                    )
                                    color: root.palette.placeholderText
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                            }

                            Switch {
                                objectName: "animationRuleEnabled"
                                    + animationCard.index
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                checked: !!animationCard.modelData
                                    && animationCard.modelData.enabled === true
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Enable %1").arg(
                                    root.animationTitle(
                                        animationCard.modelData
                                    )
                                )
                                onClicked: root.enabledAnimationRequested(
                                    String(animationCard.modelData.id), checked
                                )
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: childrenRect.height
                            spacing: 8

                            Button {
                                objectName: "editAnimationRuleButton"
                                    + animationCard.index
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Edit")
                                enabled: root.inspectionEnabled
                                Accessible.name: qsTr("Edit %1").arg(
                                    root.animationTitle(
                                        animationCard.modelData
                                    )
                                )
                                onClicked: root.editAnimationRequested(
                                    String(animationCard.modelData.id)
                                )
                            }

                            Button {
                                objectName: "moveAnimationRuleUpButton"
                                    + animationCard.index
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Move up")
                                enabled: root.controlsEnabled
                                    && animationCard.index > 0
                                Accessible.name: qsTr("Move %1 up").arg(
                                    root.animationTitle(
                                        animationCard.modelData
                                    )
                                )
                                onClicked: root.moveAnimationRequested(
                                    String(animationCard.modelData.id), -1
                                )
                            }

                            Button {
                                objectName: "moveAnimationRuleDownButton"
                                    + animationCard.index
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Move down")
                                enabled: root.controlsEnabled
                                    && animationCard.index + 1
                                        < animationsList.count
                                Accessible.name: qsTr("Move %1 down").arg(
                                    root.animationTitle(
                                        animationCard.modelData
                                    )
                                )
                                onClicked: root.moveAnimationRequested(
                                    String(animationCard.modelData.id), 1
                                )
                            }

                            Button {
                                objectName: "removeAnimationRuleButton"
                                    + animationCard.index
                                implicitHeight: Math.max(
                                    root.minimumTargetSize,
                                    implicitBackgroundHeight,
                                    implicitContentHeight
                                        + topPadding + bottomPadding
                                )
                                text: qsTr("Remove")
                                enabled: root.controlsEnabled
                                Accessible.name: qsTr("Remove %1").arg(
                                    root.animationTitle(
                                        animationCard.modelData
                                    )
                                )
                                onClicked: root.removeAnimationRequested(
                                    String(animationCard.modelData.id)
                                )
                            }
                        }
                    }
                }
            }
        }
    }

    Label {
        objectName: "appearanceAnimationDraftValidationMessage"
        Layout.fillWidth: true
        visible: root.draftDirty && !root.draftValid
        text: qsTr("Finish every curve and animation rule before the combined Appearance draft can be saved.")
        color: "#ffb8c3"
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    Item { Layout.preferredHeight: 12 }
}
