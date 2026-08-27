pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

ColumnLayout {
    id: root

    property var animation: null
    property var leafChoices: []
    property var curveChoices: []
    property bool controlsEnabled: false
    property string animationIssue: ""
    property real minimumTargetSize: 44
    property bool compact: width < 560

    signal closeRequested()
    signal removeRequested(string id)
    signal propertyModified(string id, string propertyName, var value)

    spacing: 14

    function animationId() {
        return root.animation && typeof root.animation.id === "string"
            ? root.animation.id : "";
    }

    function currentLeaf() {
        return root.animation && typeof root.animation.name === "string"
            ? root.animation.name : "";
    }

    function styleBaseValues(leaf) {
        if (["windows", "windowsIn", "windowsOut", "windowsMove"]
                .includes(leaf)) {
            return ["", "slide", "gnome", "gnomed", "popin"];
        }
        if (["workspaces", "workspacesIn", "workspacesOut",
                "specialWorkspace", "specialWorkspaceIn",
                "specialWorkspaceOut"].includes(leaf)) {
            return [
                "", "fade", "slide", "slidevert", "slidefade",
                "slidefadevert"
            ];
        }
        if (["borderangle", "shadowangle", "glowangle"].includes(leaf))
            return ["", "loop", "once"];
        if (["layers", "layersIn", "layersOut"].includes(leaf))
            return ["", "fade", "slide", "popin"];
        return [""];
    }

    function styleBaseLabels(leaf) {
        const labels = {
            "": qsTr("Default style"),
            slide: qsTr("Slide"),
            gnome: qsTr("GNOME"),
            gnomed: qsTr("GNOME, dynamic"),
            popin: qsTr("Pop in"),
            fade: qsTr("Fade"),
            slidevert: qsTr("Vertical slide"),
            slidefade: qsTr("Slide and fade"),
            slidefadevert: qsTr("Vertical slide and fade"),
            loop: qsTr("Loop"),
            once: qsTr("Once")
        };
        return root.styleBaseValues(leaf).map(value => labels[value]);
    }

    function currentStyle() {
        return root.animation && typeof root.animation.style === "string"
            ? root.animation.style : "";
    }

    function currentStyleBase() {
        const style = root.currentStyle();
        if (style.length === 0)
            return "";
        return style.split(" ")[0];
    }

    function currentDirection() {
        const parts = root.currentStyle().split(" ");
        for (const part of parts) {
            if (["top", "bottom", "left", "right"].includes(part))
                return part;
        }
        return "";
    }

    function currentPercentage() {
        const match = /(?:^| )(0|[1-9][0-9]?|100)%$/.exec(
            root.currentStyle()
        );
        return match ? Number(match[1]) : 0;
    }

    function hasPercentage() {
        return /(?:^| )(?:0|[1-9][0-9]?|100)%$/.test(
            root.currentStyle()
        );
    }

    function supportsDirection(base) {
        return ["slide", "slidevert", "slidefade", "slidefadevert"]
            .includes(base);
    }

    function supportsPercentage(base) {
        return base === "popin"
            || (["workspaces", "workspacesIn", "workspacesOut",
                    "specialWorkspace", "specialWorkspaceIn",
                    "specialWorkspaceOut"].includes(root.currentLeaf())
                && ["slide", "slidevert", "slidefade", "slidefadevert"]
                    .includes(base));
    }

    function composedStyle(base, direction, percentageEnabled, percentage) {
        if (typeof base !== "string" || base.length === 0)
            return "";
        let style = base;
        if (root.supportsDirection(base) && direction.length > 0)
            style += " " + direction;
        if (root.supportsPercentage(base) && percentageEnabled)
            style += " " + Math.max(0, Math.min(100, percentage)) + "%";
        return style;
    }

    function setStyle(base, direction, percentageEnabled, percentage) {
        root.propertyModified(
            root.animationId(), "style",
            root.composedStyle(
                base, direction, percentageEnabled, percentage
            )
        );
    }

    function curveChoiceIndex() {
        const current = root.animation
            && typeof root.animation.curve === "string"
            ? root.animation.curve : "";
        for (let index = 0; index < root.curveChoices.length; ++index) {
            if (root.curveChoices[index].value === current)
                return index;
        }
        return -1;
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        Button {
            objectName: "closeAnimationRuleEditorButton"
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: qsTr("Back to animations")
            Accessible.name: qsTr("Close the animation rule editor")
            onClicked: root.closeRequested()
        }

        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            horizontalAlignment: Text.AlignRight
            text: qsTr("Editing animation rule")
            color: root.palette.placeholderText
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }
    }

    Frame {
        objectName: "animationRuleIdentityCard"
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
                text: qsTr("Animation target")
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
                text: qsTr("Choose one unused leaf from Hyprland's closed animation tree. The internal record ID stays hidden and stable.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Animation leaf")
                color: root.palette.text
                font.weight: Font.Medium
                textFormat: Text.PlainText
            }

            ComboBox {
                id: animationLeafControl

                objectName: "animationRuleLeaf"
                Layout.fillWidth: true
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                model: Array.isArray(root.leafChoices) ? root.leafChoices : []
                currentIndex: root.leafChoices.indexOf(root.currentLeaf())
                enabled: root.controlsEnabled
                Accessible.name: qsTr("Animation leaf")
                onActivated: index => {
                    if (index >= 0 && index < root.leafChoices.length) {
                        root.propertyModified(
                            root.animationId(), "name",
                            root.leafChoices[index]
                        );
                    }
                }
            }

            SettingsToggleRow {
                Layout.fillWidth: true
                title: qsTr("Rule enabled")
                description: qsTr("A disabled rule stays saved and ordered, is registered as disabled, and does not animate that leaf.")
                checked: !!root.animation
                    && root.animation.enabled === true
                enabled: root.controlsEnabled
                controlObjectName: "animationRuleEnabledEditor"
                accessibleName: qsTr("Enable this animation rule")
                minimumTargetSize: root.minimumTargetSize
                onValueModified: value => root.propertyModified(
                    root.animationId(), "enabled", value
                )
            }

            Label {
                objectName: "animationRuleValidationMessage"
                Layout.fillWidth: true
                visible: root.animationIssue.length > 0
                text: root.animationIssue
                color: ShellTheme.onErrorContainer
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
        }
    }

    Frame {
        objectName: "animationRuleTimingCard"
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
                text: qsTr("Timing")
                color: root.palette.text
                font.pixelSize: 17
                font.weight: Font.DemiBold
                textFormat: Text.PlainText
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Speed")
                color: root.palette.text
                font.weight: Font.Medium
                textFormat: Text.PlainText
            }

            RuleDecimalField {
                objectName: "animationRuleSpeed"
                Layout.fillWidth: true
                value: root.animation ? root.animation.speed : ""
                minimumValue: Number.MIN_VALUE
                maximumValue: 100
                enabled: root.controlsEnabled
                accessibleName: qsTr("Animation speed")
                minimumTargetSize: root.minimumTargetSize
                onValueModified: value => root.propertyModified(
                    root.animationId(), "speed", value
                )
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Curve")
                color: root.palette.text
                font.weight: Font.Medium
                textFormat: Text.PlainText
            }

            ComboBox {
                id: animationCurveControl

                objectName: "animationRuleCurve"
                Layout.fillWidth: true
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                textRole: "label"
                valueRole: "value"
                model: Array.isArray(root.curveChoices)
                    ? root.curveChoices : []
                currentIndex: root.curveChoiceIndex()
                enabled: root.controlsEnabled
                Accessible.name: qsTr("Animation curve")
                onActivated: index => {
                    if (index >= 0 && index < root.curveChoices.length) {
                        root.propertyModified(
                            root.animationId(), "curve",
                            root.curveChoices[index].value
                        );
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("A custom curve named default or linear appears once as a custom override, never as a duplicate built-in choice. Managed rules using that name resolve to the custom curve's authored type; Hyprland retains the opposite-type built-in in its separate runtime map.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
        }
    }

    Frame {
        objectName: "animationRuleStyleCard"
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
                text: qsTr("Leaf-specific style")
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
                text: qsTr("Only styles accepted for the selected animation leaf are offered. Leaves without a style extension stay on Default style.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            ComboBox {
                id: animationStyleBaseControl

                objectName: "animationRuleStyleBase"
                Layout.fillWidth: true
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                model: root.styleBaseLabels(root.currentLeaf())
                currentIndex: Math.max(
                    0, root.styleBaseValues(root.currentLeaf())
                        .indexOf(root.currentStyleBase())
                )
                enabled: root.controlsEnabled
                Accessible.name: qsTr("Animation style")
                onActivated: index => {
                    const values = root.styleBaseValues(root.currentLeaf());
                    if (index >= 0 && index < values.length) {
                        root.setStyle(
                            values[index], root.currentDirection(),
                            root.hasPercentage(), root.currentPercentage()
                        );
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: root.supportsDirection(root.currentStyleBase())
                text: qsTr("Direction")
                color: root.palette.text
                font.weight: Font.Medium
                textFormat: Text.PlainText
            }

            ComboBox {
                id: animationStyleDirectionControl

                objectName: "animationRuleStyleDirection"
                Layout.fillWidth: true
                visible: root.supportsDirection(root.currentStyleBase())
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                model: [
                    qsTr("Automatic"), qsTr("Top"), qsTr("Bottom"),
                    qsTr("Left"), qsTr("Right")
                ]
                currentIndex: Math.max(
                    0, ["", "top", "bottom", "left", "right"]
                        .indexOf(root.currentDirection())
                )
                enabled: root.controlsEnabled
                Accessible.name: qsTr("Animation style direction")
                onActivated: index => {
                    const directions = [
                        "", "top", "bottom", "left", "right"
                    ];
                    if (index >= 0 && index < directions.length) {
                        root.setStyle(
                            root.currentStyleBase(), directions[index],
                            root.hasPercentage(), root.currentPercentage()
                        );
                    }
                }
            }

            SettingsToggleRow {
                Layout.fillWidth: true
                visible: root.supportsPercentage(root.currentStyleBase())
                title: qsTr("Use a percentage")
                description: qsTr("Add an explicit 0–100 percent amount to this style.")
                checked: root.hasPercentage()
                enabled: root.controlsEnabled
                controlObjectName: "animationRuleStylePercentageEnabled"
                accessibleName: qsTr("Use an animation style percentage")
                minimumTargetSize: root.minimumTargetSize
                onValueModified: value => root.setStyle(
                    root.currentStyleBase(), root.currentDirection(),
                    value, root.currentPercentage()
                )
            }

            SettingsSpinBoxRow {
                Layout.fillWidth: true
                visible: root.supportsPercentage(root.currentStyleBase())
                    && root.hasPercentage()
                title: qsTr("Percentage")
                description: qsTr("Set the exact style amount from 0 through 100 percent.")
                from: 0
                to: 100
                value: root.currentPercentage()
                enabled: root.controlsEnabled
                controlObjectName: "animationRuleStylePercentage"
                accessibleName: qsTr("Animation style percentage")
                minimumTargetSize: root.minimumTargetSize
                onValueModified: value => root.setStyle(
                    root.currentStyleBase(), root.currentDirection(),
                    true, value
                )
            }
        }
    }

    Frame {
        objectName: "animationRuleActionsCard"
        Layout.fillWidth: true
        padding: root.compact ? 14 : 18

        background: Rectangle {
            color: root.palette.base
            radius: 16
            border.color: root.palette.mid
        }

        Button {
            objectName: "removeEditedAnimationRuleButton"
            anchors.left: parent.left
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitBackgroundHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: qsTr("Remove animation rule")
            enabled: root.controlsEnabled
            Accessible.name: qsTr("Remove this animation rule from the draft")
            onClicked: root.removeRequested(root.animationId())
        }
    }

    Item { Layout.preferredHeight: 12 }
}
