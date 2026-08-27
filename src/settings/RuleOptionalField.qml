pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Frame {
    id: root

    required property var definition
    property bool included: false
    property var value: undefined
    property bool fieldValid: true
    property real minimumTargetSize: 44

    signal includeModified(bool included)
    signal valueModified(var value)

    Layout.fillWidth: true
    padding: 12

    background: Rectangle {
        color: Qt.rgba(
            root.palette.text.r,
            root.palette.text.g,
            root.palette.text.b,
            root.included ? 0.045 : 0.02
        )
        radius: 12
        border.color: root.fieldValid ? root.palette.mid : ShellTheme.errorOutline
    }

    function clone(value) {
        try {
            return JSON.parse(JSON.stringify(value));
        } catch (error) {
            return null;
        }
    }

    function choiceIndex() {
        const values = Array.isArray(root.definition.values)
            ? root.definition.values : [];
        const index = values.indexOf(root.value);
        return index >= 0 ? index : 0;
    }

    function editorComponent() {
        switch (root.definition.kind) {
        case "boolean": return booleanEditor;
        case "enum": return enumEditor;
        case "integer": return integerEditor;
        case "safeInteger": return safeIntegerEditor;
        case "number": return numberEditor;
        case "text": return textEditor;
        case "vector2": return vectorEditor;
        case "target": return targetEditor;
        case "fullscreenState": return fullscreenStateEditor;
        case "suppressEvents": return suppressEventsEditor;
        case "animation": return animationEditor;
        case "workspaceAnimation": return workspaceAnimationEditor;
        case "cssGap": return cssGapEditor;
        case "layoutOptions": return layoutOptionsEditor;
        case "opacity": return opacityEditor;
        case "gradient": return gradientEditor;
        default: return null;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        CheckBox {
            objectName: root.definition.includeObjectName || ""
            Layout.fillWidth: true
            implicitHeight: Math.max(
                root.minimumTargetSize,
                implicitIndicatorHeight,
                implicitContentHeight + topPadding + bottomPadding
            )
            text: root.definition.title || ""
            checked: root.included
            enabled: root.enabled
            Accessible.name: qsTr("Use %1").arg(text)

            onClicked: root.includeModified(checked)
        }

        Label {
            Layout.fillWidth: true
            text: root.definition.description || ""
            color: root.palette.placeholderText
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Loader {
            Layout.fillWidth: true
            active: root.included
            sourceComponent: root.editorComponent()
        }

        Label {
            Layout.fillWidth: true
            visible: root.included && !root.fieldValid
            text: root.definition.errorText
                || qsTr("This value is incomplete or outside the supported range.")
            color: ShellTheme.onErrorContainer
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
    }

    Component {
        id: booleanEditor

        SettingsToggleRow {
            title: qsTr("Value")
            description: qsTr("Off is an explicit rule value and is different from removing this field.")
            checked: root.value === true
            enabled: root.enabled
            controlObjectName: root.definition.controlObjectName || ""
            accessibleName: root.definition.title || ""
            minimumTargetSize: root.minimumTargetSize

            onValueModified: value => root.valueModified(value)
        }
    }

    Component {
        id: enumEditor

        SettingsSelectRow {
            title: qsTr("Value")
            description: root.definition.valueDescription || ""
            model: root.definition.labels || []
            currentIndex: root.choiceIndex()
            controlWidth: 190
            enabled: root.enabled
            controlObjectName: root.definition.controlObjectName || ""
            accessibleName: root.definition.title || ""
            minimumTargetSize: root.minimumTargetSize

            onValueModified: index => {
                const values = root.definition.values || [];
                if (index >= 0 && index < values.length)
                    root.valueModified(values[index]);
            }
        }
    }

    Component {
        id: integerEditor

        SettingsSpinBoxRow {
            title: qsTr("Value")
            description: root.definition.valueDescription || ""
            from: Number(root.definition.minimum)
            to: Number(root.definition.maximum)
            value: typeof root.value === "number" ? root.value : from
            enabled: root.enabled
            controlObjectName: root.definition.controlObjectName || ""
            accessibleName: root.definition.title || ""
            minimumTargetSize: root.minimumTargetSize

            onValueModified: value => root.valueModified(value)
        }
    }

    Component {
        id: safeIntegerEditor

        RuleSafeIntegerField {
            Layout.fillWidth: true
            value: root.value
            enabled: root.enabled
            controlObjectName: root.definition.controlObjectName || ""
            accessibleName: root.definition.title || ""
            minimumTargetSize: root.minimumTargetSize

            onValueModified: value => root.valueModified(value)
        }
    }

    Component {
        id: numberEditor

        ColumnLayout {
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: qsTr("Value")
                color: root.palette.text
                font.pixelSize: 14
                font.weight: Font.Medium
                textFormat: Text.PlainText
            }

            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.definition.valueDescription || ""
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            RuleDecimalField {
                objectName: root.definition.controlObjectName || ""
                Layout.fillWidth: true
                value: root.value
                minimumValue: Number(root.definition.minimum)
                maximumValue: Number(root.definition.maximum)
                enabled: root.enabled
                accessibleName: root.definition.title || ""
                minimumTargetSize: root.minimumTargetSize

                onValueModified: value => root.valueModified(value)
            }
        }
    }

    Component {
        id: textEditor

        TextField {
            objectName: root.definition.controlObjectName || ""
            implicitHeight: root.minimumTargetSize
            maximumLength: Number(root.definition.maximumLength || 512)
            text: typeof root.value === "string" ? root.value : ""
            placeholderText: root.definition.placeholder || ""
            enabled: root.enabled
            Accessible.name: root.definition.title || ""

            onTextEdited: root.valueModified(text)
        }
    }

    Component {
        id: vectorEditor

        GridLayout {
            columns: width < 420 ? 1 : 2
            columnSpacing: 10
            rowSpacing: 8

            RuleDecimalField {
                Layout.fillWidth: true
                objectName: (root.definition.controlObjectName || "") + "X"
                value: Array.isArray(root.value) ? root.value[0] : ""
                minimumValue: Number(root.definition.minimum)
                maximumValue: Number(root.definition.maximum)
                minimumTargetSize: root.minimumTargetSize
                accessibleName: qsTr("%1 horizontal value").arg(root.definition.title || "")
                enabled: root.enabled

                onValueModified: value => {
                    const next = Array.isArray(root.value)
                        ? root.clone(root.value) : ["", ""];
                    next[0] = value;
                    root.valueModified(next);
                }
            }

            RuleDecimalField {
                Layout.fillWidth: true
                objectName: (root.definition.controlObjectName || "") + "Y"
                value: Array.isArray(root.value) ? root.value[1] : ""
                minimumValue: Number(root.definition.minimum)
                maximumValue: Number(root.definition.maximum)
                minimumTargetSize: root.minimumTargetSize
                accessibleName: qsTr("%1 vertical value").arg(root.definition.title || "")
                enabled: root.enabled

                onValueModified: value => {
                    const next = Array.isArray(root.value)
                        ? root.clone(root.value) : ["", ""];
                    next[1] = value;
                    root.valueModified(next);
                }
            }
        }
    }

    Component {
        id: targetEditor

        ColumnLayout {
            spacing: 10

            TextField {
                objectName: (root.definition.controlObjectName || "") + "Target"
                Layout.fillWidth: true
                implicitHeight: root.minimumTargetSize
                maximumLength: 261
                text: root.value && typeof root.value.target === "string"
                    ? root.value.target : ""
                placeholderText: root.definition.placeholder || ""
                enabled: root.enabled
                Accessible.name: qsTr("%1 target").arg(root.definition.title || "")

                onTextEdited: {
                    const next = root.value && typeof root.value === "object"
                        ? root.clone(root.value) : { target: "", silent: false };
                    next.target = text;
                    root.valueModified(next);
                }
            }

            SettingsToggleRow {
                title: qsTr("Move without switching")
                description: qsTr("Use Hyprland's silent target form when this rule moves the window.")
                checked: !!root.value && root.value.silent === true
                enabled: root.enabled
                controlObjectName:
                    (root.definition.controlObjectName || "") + "Silent"
                accessibleName: qsTr("Use the silent %1 target").arg(root.definition.title || "")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: value => {
                    const next = root.value && typeof root.value === "object"
                        ? root.clone(root.value) : { target: "", silent: false };
                    next.silent = value;
                    root.valueModified(next);
                }
            }
        }
    }

    Component {
        id: fullscreenStateEditor

        ColumnLayout {
            spacing: 10

            SettingsSelectRow {
                title: qsTr("Internal state")
                description: qsTr("Choose the compositor's internal fullscreen state.")
                model: [qsTr("None"), qsTr("Maximized"), qsTr("Fullscreen")]
                currentIndex: root.value && Number.isInteger(root.value.internal)
                    ? root.value.internal : 0
                controlWidth: 170
                enabled: root.enabled
                controlObjectName:
                    (root.definition.controlObjectName || "") + "Internal"
                accessibleName: qsTr("Internal fullscreen state")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: index => {
                    const next = root.value && typeof root.value === "object"
                        ? root.clone(root.value)
                        : { internal: 0 };
                    next.internal = index;
                    root.valueModified(next);
                }
            }

            CheckBox {
                objectName:
                    (root.definition.controlObjectName || "") + "ClientInclude"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitIndicatorHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Set client fullscreen state")
                checked: !!root.value
                    && Object.prototype.hasOwnProperty.call(root.value, "client")
                enabled: root.enabled

                onClicked: {
                    const next = root.value && typeof root.value === "object"
                        ? root.clone(root.value)
                        : { internal: 0 };
                    if (checked)
                        next.client = 0;
                    else
                        delete next.client;
                    root.valueModified(next);
                }
            }

            SettingsSelectRow {
                title: qsTr("Client state")
                description: qsTr("Optionally report a separate state to the client.")
                model: [qsTr("None"), qsTr("Maximized"), qsTr("Fullscreen")]
                currentIndex: root.value && Number.isInteger(root.value.client)
                    ? root.value.client : 0
                controlWidth: 170
                enabled: root.enabled && !!root.value
                    && Object.prototype.hasOwnProperty.call(root.value, "client")
                controlObjectName:
                    (root.definition.controlObjectName || "") + "Client"
                accessibleName: qsTr("Client fullscreen state")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: index => {
                    const next = root.clone(root.value);
                    if (next)
                        next.client = index;
                    if (next)
                        root.valueModified(next);
                }
            }
        }
    }

    Component {
        id: suppressEventsEditor

        Flow {
            spacing: 8

            Repeater {
                model: [
                    { value: "fullscreen", label: qsTr("Fullscreen") },
                    { value: "maximize", label: qsTr("Maximize") },
                    { value: "activate", label: qsTr("Activate") },
                    { value: "activatefocus", label: qsTr("Focus activation") },
                    { value: "fullscreenoutput", label: qsTr("Fullscreen output") },
                    { value: "x11configurerequest", label: qsTr("X11 configure request") }
                ]

                CheckBox {
                    required property var modelData

                    objectName: (root.definition.controlObjectName || "")
                        + modelData.value
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitIndicatorHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: modelData.label
                    checked: Array.isArray(root.value)
                        && root.value.includes(modelData.value)
                    enabled: root.enabled

                    onClicked: {
                        const next = Array.isArray(root.value)
                            ? root.clone(root.value) : [];
                        const index = next.indexOf(modelData.value);
                        if (checked && index < 0)
                            next.push(modelData.value);
                        else if (!checked && index >= 0)
                            next.splice(index, 1);
                        root.valueModified(next);
                    }
                }
            }
        }
    }

    Component {
        id: cssGapEditor

        GridLayout {
            columns: width < 520 ? 2 : 4
            columnSpacing: 12
            rowSpacing: 8

            Repeater {
                model: [
                    { suffix: "Top", label: qsTr("Top") },
                    { suffix: "Right", label: qsTr("Right") },
                    { suffix: "Bottom", label: qsTr("Bottom") },
                    { suffix: "Left", label: qsTr("Left") }
                ]

                ColumnLayout {
                    id: gapComponent

                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: gapComponent.modelData.label
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        textFormat: Text.PlainText
                    }

                    RuleSafeIntegerField {
                        Layout.fillWidth: true
                        value: Array.isArray(root.value)
                                && gapComponent.index < root.value.length
                            ? root.value[gapComponent.index] : ""
                        enabled: root.enabled
                        controlObjectName:
                            (root.definition.controlObjectName || "")
                            + gapComponent.modelData.suffix
                        accessibleName: qsTr("%1 %2 gap")
                            .arg(root.definition.title || "")
                            .arg(gapComponent.modelData.label)
                        minimumTargetSize: root.minimumTargetSize

                        onValueModified: value => {
                            const next = Array.isArray(root.value)
                                ? root.clone(root.value) : [0, 0, 0, 0];
                            next[gapComponent.index] = value;
                            root.valueModified(next);
                        }
                    }
                }
            }
        }
    }

    Component {
        id: layoutOptionsEditor

        ColumnLayout {
            id: layoutOptionsControls

            spacing: 10

            function hasOption(name) {
                return !!root.value && typeof root.value === "object"
                    && !Array.isArray(root.value)
                    && Object.prototype.hasOwnProperty.call(root.value, name);
            }

            function updateOption(name, included, value) {
                const next = root.value && typeof root.value === "object"
                        && !Array.isArray(root.value)
                    ? root.clone(root.value) : {};
                if (included)
                    next[name] = value;
                else
                    delete next[name];
                root.valueModified(next);
            }

            CheckBox {
                objectName:
                    (root.definition.controlObjectName || "")
                    + "OrientationInclude"
                Layout.fillWidth: true
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitIndicatorHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Set Master orientation")
                checked: layoutOptionsControls.hasOption("orientation")
                enabled: root.enabled
                Accessible.name: text

                onClicked: layoutOptionsControls.updateOption(
                    "orientation", checked, "left"
                )
            }

            SettingsSelectRow {
                title: qsTr("Master orientation")
                description: qsTr("Choose where the Master area sits when this workspace uses the Master layout.")
                model: [
                    qsTr("Left"), qsTr("Right"), qsTr("Top"),
                    qsTr("Bottom"), qsTr("Center")
                ]
                currentIndex: Math.max(
                    0, ["left", "right", "top", "bottom", "center"]
                        .indexOf(root.value && root.value.orientation)
                )
                controlWidth: 170
                enabled: root.enabled
                    && layoutOptionsControls.hasOption("orientation")
                controlObjectName:
                    (root.definition.controlObjectName || "")
                    + "Orientation"
                accessibleName: qsTr("Workspace Master orientation")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: index => {
                    const values = [
                        "left", "right", "top", "bottom", "center"
                    ];
                    if (index >= 0 && index < values.length) {
                        layoutOptionsControls.updateOption(
                            "orientation", true, values[index]
                        );
                    }
                }
            }

            CheckBox {
                objectName:
                    (root.definition.controlObjectName || "")
                    + "DirectionInclude"
                Layout.fillWidth: true
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitIndicatorHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Set Scrolling direction")
                checked: layoutOptionsControls.hasOption("direction")
                enabled: root.enabled
                Accessible.name: text

                onClicked: layoutOptionsControls.updateOption(
                    "direction", checked, "right"
                )
            }

            SettingsSelectRow {
                title: qsTr("Scrolling direction")
                description: qsTr("Choose the tape direction when this workspace uses the Scrolling layout.")
                model: [
                    qsTr("Left"), qsTr("Right"),
                    qsTr("Up"), qsTr("Down")
                ]
                currentIndex: Math.max(
                    0, ["left", "right", "up", "down"]
                        .indexOf(root.value && root.value.direction)
                )
                controlWidth: 170
                enabled: root.enabled
                    && layoutOptionsControls.hasOption("direction")
                controlObjectName:
                    (root.definition.controlObjectName || "")
                    + "Direction"
                accessibleName: qsTr("Workspace Scrolling direction")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: index => {
                    const values = ["left", "right", "up", "down"];
                    if (index >= 0 && index < values.length) {
                        layoutOptionsControls.updateOption(
                            "direction", true, values[index]
                        );
                    }
                }
            }
        }
    }

    Component {
        id: workspaceAnimationEditor

        ColumnLayout {
            id: workspaceAnimationControls

            property string style: typeof root.value === "string"
                ? root.value : ""
            property var tokens: style.split(" ").filter(token => token.length)
            property string baseStyle: tokens.length > 0 ? tokens[0] : ""
            property string direction: tokens.length > 1
                    && ["top", "bottom", "left", "right"].includes(tokens[1])
                ? tokens[1] : ""
            property string percentageToken: {
                if (tokens.length < 2)
                    return "";
                const candidate = direction.length > 0
                    ? (tokens.length > 2 ? tokens[2] : "") : tokens[1];
                return /^(?:0|[1-9][0-9]?|100)%$/.test(candidate)
                    ? candidate : "";
            }
            property bool customPercentage: percentageToken.length > 0
            property int percentage: customPercentage
                ? Number(percentageToken.slice(0, -1)) : 0

            spacing: 10

            function emitStyle(base, direction, custom, percentage) {
                if (!["slide", "slidevert", "slidefade", "slidefadevert"]
                        .includes(base)) {
                    root.valueModified(base);
                    return;
                }
                let result = base;
                if (direction.length > 0)
                    result += " " + direction;
                if (custom)
                    result += " " + percentage + "%";
                root.valueModified(result);
            }

            SettingsSelectRow {
                title: qsTr("Workspace animation")
                description: qsTr("Choose one pinned workspace transition form.")
                model: [
                    qsTr("Default"), qsTr("Fade"), qsTr("Slide"),
                    qsTr("Vertical slide"), qsTr("Slide and fade"),
                    qsTr("Vertical slide and fade")
                ]
                currentIndex: Math.max(
                    0,
                    [
                        "", "fade", "slide", "slidevert",
                        "slidefade", "slidefadevert"
                    ].indexOf(workspaceAnimationControls.baseStyle)
                )
                controlWidth: 210
                enabled: root.enabled
                controlObjectName: root.definition.controlObjectName || ""
                accessibleName: root.definition.title || ""
                minimumTargetSize: root.minimumTargetSize

                onValueModified: index => {
                    const values = [
                        "", "fade", "slide", "slidevert",
                        "slidefade", "slidefadevert"
                    ];
                    if (index >= 0 && index < values.length) {
                        workspaceAnimationControls.emitStyle(
                            values[index], "", false, 0
                        );
                    }
                }
            }

            SettingsSelectRow {
                title: qsTr("Slide direction")
                description: qsTr("Leave automatic or choose an explicit transition edge.")
                model: [
                    qsTr("Automatic"), qsTr("Top"), qsTr("Bottom"),
                    qsTr("Left"), qsTr("Right")
                ]
                currentIndex: Math.max(
                    0, ["", "top", "bottom", "left", "right"]
                        .indexOf(workspaceAnimationControls.direction)
                )
                controlWidth: 170
                enabled: root.enabled
                    && workspaceAnimationControls.baseStyle.startsWith("slide")
                controlObjectName:
                    (root.definition.controlObjectName || "") + "Direction"
                accessibleName: qsTr("Workspace animation slide direction")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: index => {
                    const values = ["", "top", "bottom", "left", "right"];
                    if (index >= 0 && index < values.length) {
                        workspaceAnimationControls.emitStyle(
                            workspaceAnimationControls.baseStyle,
                            values[index],
                            workspaceAnimationControls.customPercentage,
                            workspaceAnimationControls.percentage
                        );
                    }
                }
            }

            CheckBox {
                objectName:
                    (root.definition.controlObjectName || "")
                    + "PercentageInclude"
                Layout.fillWidth: true
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitIndicatorHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                visible:
                    workspaceAnimationControls.baseStyle.startsWith("slide")
                text: qsTr("Set transition distance percentage")
                checked: workspaceAnimationControls.customPercentage
                enabled: root.enabled
                Accessible.name: text

                onClicked: workspaceAnimationControls.emitStyle(
                    workspaceAnimationControls.baseStyle,
                    workspaceAnimationControls.direction,
                    checked,
                    workspaceAnimationControls.percentage
                )
            }

            SettingsSpinBoxRow {
                title: qsTr("Transition distance")
                description: qsTr("Set an explicit transition distance from 0 through 100 percent.")
                from: 0
                to: 100
                value: workspaceAnimationControls.percentage
                enabled: root.enabled
                    && workspaceAnimationControls.baseStyle.startsWith("slide")
                    && workspaceAnimationControls.customPercentage
                controlObjectName:
                    (root.definition.controlObjectName || "") + "Percentage"
                accessibleName: qsTr("Workspace animation transition distance percentage")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: value =>
                    workspaceAnimationControls.emitStyle(
                        workspaceAnimationControls.baseStyle,
                        workspaceAnimationControls.direction,
                        true,
                        value
                    )
            }
        }
    }

    Component {
        id: animationEditor

        ColumnLayout {
            id: animationControls

            property string style: typeof root.value === "string"
                ? root.value : ""
            property bool isLayer: root.definition.animationKind === "layer"
            property string baseStyle: {
                if (style === "fade") return "fade";
                if (style === "gnome") return "gnome";
                if (style === "gnomed") return "gnomed";
                if (style === "slide" || style.startsWith("slide "))
                    return "slide";
                if (style === "popin" || style.startsWith("popin "))
                    return "popin";
                return "";
            }
            property string direction: style.startsWith("slide ")
                ? style.slice(6) : ""
            property bool customPercentage: /^popin [0-9]+%$/.test(style)
            property int percentage: customPercentage
                ? Number(style.slice(6, -1)) : 0

            spacing: 10

            function baseValues() {
                return animationControls.isLayer
                    ? ["", "fade", "slide", "popin"]
                    : ["", "slide", "gnome", "gnomed", "popin"];
            }

            function baseLabels() {
                return animationControls.isLayer
                    ? [qsTr("Default"), qsTr("Fade"), qsTr("Slide"), qsTr("Pop in")]
                    : [qsTr("Default"), qsTr("Slide"), qsTr("GNOME"), qsTr("GNOME down"), qsTr("Pop in")];
            }

            function emitStyle(base, direction, custom, percentage) {
                if (base === "slide") {
                    root.valueModified(direction.length > 0
                        ? "slide " + direction : "slide");
                } else if (base === "popin") {
                    root.valueModified(custom
                        ? "popin " + percentage + "%" : "popin");
                } else {
                    root.valueModified(base);
                }
            }

            SettingsSelectRow {
                title: qsTr("Animation style")
                description: qsTr("Choose a pinned built-in animation form.")
                model: animationControls.baseLabels()
                currentIndex: Math.max(
                    0,
                    animationControls.baseValues().indexOf(
                        animationControls.baseStyle
                    )
                )
                controlWidth: 170
                enabled: root.enabled
                controlObjectName: root.definition.controlObjectName || ""
                accessibleName: root.definition.title || ""
                minimumTargetSize: root.minimumTargetSize

                onValueModified: index => {
                    const values = animationControls.baseValues();
                    if (index >= 0 && index < values.length) {
                        animationControls.emitStyle(
                            values[index], "", false, 0
                        );
                    }
                }
            }

            SettingsSelectRow {
                title: qsTr("Slide direction")
                description: qsTr("Leave automatic or choose an explicit edge.")
                model: [
                    qsTr("Automatic"), qsTr("Top"), qsTr("Bottom"),
                    qsTr("Left"), qsTr("Right")
                ]
                currentIndex: Math.max(
                    0, ["", "top", "bottom", "left", "right"]
                        .indexOf(animationControls.direction)
                )
                controlWidth: 170
                enabled: root.enabled
                    && animationControls.baseStyle === "slide"
                controlObjectName:
                    (root.definition.controlObjectName || "") + "Direction"
                accessibleName: qsTr("Animation slide direction")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: index => {
                    const values = ["", "top", "bottom", "left", "right"];
                    if (index >= 0 && index < values.length) {
                        animationControls.emitStyle(
                            "slide", values[index], false, 0
                        );
                    }
                }
            }

            CheckBox {
                objectName:
                    (root.definition.controlObjectName || "") + "PercentInclude"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitIndicatorHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                visible: animationControls.baseStyle === "popin"
                text: qsTr("Set pop-in percentage")
                checked: animationControls.customPercentage
                enabled: root.enabled

                onClicked: animationControls.emitStyle(
                    "popin", "", checked,
                    animationControls.percentage
                )
            }

            SettingsSpinBoxRow {
                title: qsTr("Pop-in percentage")
                description: qsTr("Choose the explicit pop-in starting percentage.")
                from: 0
                to: 100
                value: animationControls.percentage
                enabled: root.enabled
                    && animationControls.baseStyle === "popin"
                    && animationControls.customPercentage
                controlObjectName:
                    (root.definition.controlObjectName || "") + "Percent"
                accessibleName: qsTr("Animation pop-in percentage")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: value => animationControls.emitStyle(
                    "popin", "", true, value
                )
            }
        }
    }

    Component {
        id: opacityEditor

        ColumnLayout {
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: qsTr("Active opacity")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Set the opacity of an active matching window.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                RuleDecimalField {
                    objectName:
                        (root.definition.controlObjectName || "") + "Active"
                    Layout.fillWidth: true
                    value: root.value ? root.value.active : ""
                    minimumValue: 0
                    maximumValue: 1
                    enabled: root.enabled
                    accessibleName: qsTr("Active window opacity")
                    minimumTargetSize: root.minimumTargetSize

                    onValueModified: value => {
                        const next = root.clone(root.value);
                        if (next) {
                            next.active = value;
                            root.valueModified(next);
                        }
                    }
                }
            }

            SettingsToggleRow {
                title: qsTr("Override active opacity")
                description: qsTr("Force this value instead of combining it with another opacity rule.")
                checked: !!root.value && root.value.overrideActive === true
                enabled: root.enabled
                controlObjectName:
                    (root.definition.controlObjectName || "") + "OverrideActive"
                accessibleName: qsTr("Override active opacity")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: value => {
                    const next = root.clone(root.value);
                    if (next) {
                        next.overrideActive = value;
                        root.valueModified(next);
                    }
                }
            }

            CheckBox {
                objectName:
                    (root.definition.controlObjectName || "") + "InactiveInclude"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitIndicatorHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Set inactive opacity")
                checked: !!root.value
                    && Object.prototype.hasOwnProperty.call(root.value, "inactive")
                enabled: root.enabled

                onClicked: {
                    const next = root.clone(root.value);
                    if (!next)
                        return;
                    if (checked)
                        next.inactive = next.active;
                    else
                        delete next.inactive;
                    root.valueModified(next);
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: qsTr("Inactive opacity")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Set the opacity while another window is active.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                RuleDecimalField {
                    objectName:
                        (root.definition.controlObjectName || "") + "Inactive"
                    Layout.fillWidth: true
                    value: root.value && Object.prototype.hasOwnProperty.call(
                        root.value, "inactive") ? root.value.inactive : ""
                    minimumValue: 0
                    maximumValue: 1
                    enabled: root.enabled && !!root.value
                        && Object.prototype.hasOwnProperty.call(
                            root.value, "inactive")
                    accessibleName: qsTr("Inactive window opacity")
                    minimumTargetSize: root.minimumTargetSize

                    onValueModified: value => {
                        const next = root.clone(root.value);
                        if (next) {
                            next.inactive = value;
                            root.valueModified(next);
                        }
                    }
                }
            }

            SettingsToggleRow {
                title: qsTr("Override inactive opacity")
                description: qsTr("Force the inactive value instead of combining it with another rule.")
                checked: !!root.value && root.value.overrideInactive === true
                enabled: root.enabled
                controlObjectName:
                    (root.definition.controlObjectName || "") + "OverrideInactive"
                accessibleName: qsTr("Override inactive opacity")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: value => {
                    const next = root.clone(root.value);
                    if (next) {
                        next.overrideInactive = value;
                        root.valueModified(next);
                    }
                }
            }

            CheckBox {
                objectName:
                    (root.definition.controlObjectName || "") + "FullscreenInclude"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitIndicatorHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Set fullscreen opacity")
                checked: !!root.value
                    && Object.prototype.hasOwnProperty.call(root.value, "fullscreen")
                enabled: root.enabled

                onClicked: {
                    const next = root.clone(root.value);
                    if (!next)
                        return;
                    if (checked)
                        next.fullscreen = next.active;
                    else
                        delete next.fullscreen;
                    root.valueModified(next);
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: qsTr("Fullscreen opacity")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    textFormat: Text.PlainText
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Set the opacity while the window is fullscreen.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                RuleDecimalField {
                    objectName:
                        (root.definition.controlObjectName || "") + "Fullscreen"
                    Layout.fillWidth: true
                    value: root.value && Object.prototype.hasOwnProperty.call(
                        root.value, "fullscreen") ? root.value.fullscreen : ""
                    minimumValue: 0
                    maximumValue: 1
                    enabled: root.enabled && !!root.value
                        && Object.prototype.hasOwnProperty.call(
                            root.value, "fullscreen")
                    accessibleName: qsTr("Fullscreen window opacity")
                    minimumTargetSize: root.minimumTargetSize

                    onValueModified: value => {
                        const next = root.clone(root.value);
                        if (next) {
                            next.fullscreen = value;
                            root.valueModified(next);
                        }
                    }
                }
            }

            SettingsToggleRow {
                title: qsTr("Override fullscreen opacity")
                description: qsTr("Force the fullscreen value instead of combining it with another rule.")
                checked: !!root.value && root.value.overrideFullscreen === true
                enabled: root.enabled
                controlObjectName:
                    (root.definition.controlObjectName || "") + "OverrideFullscreen"
                accessibleName: qsTr("Override fullscreen opacity")
                minimumTargetSize: root.minimumTargetSize

                onValueModified: value => {
                    const next = root.clone(root.value);
                    if (next) {
                        next.overrideFullscreen = value;
                        root.valueModified(next);
                    }
                }
            }
        }
    }

    Component {
        id: gradientEditor

        ColumnLayout {
            id: gradientControls

            spacing: 10

            Label {
                Layout.fillWidth: true
                text: qsTr("Colors use canonical 0xRRGGBBAA values in gradient order.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Repeater {
                model: root.value && Array.isArray(root.value.colors)
                    ? root.value.colors : []

                RowLayout {
                    id: colorRow

                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        objectName: (root.definition.controlObjectName || "")
                            + "Color" + colorRow.index
                        Layout.fillWidth: true
                        implicitHeight: root.minimumTargetSize
                        maximumLength: 10
                        text: String(colorRow.modelData)
                        enabled: root.enabled
                        Accessible.name: qsTr("Gradient color %1").arg(colorRow.index + 1)

                        onTextEdited: {
                            const next = root.clone(root.value);
                            if (next && Array.isArray(next.colors)) {
                                next.colors[colorRow.index] = text;
                                root.valueModified(next);
                            }
                        }
                    }

                    Button {
                        objectName: (root.definition.controlObjectName || "")
                            + "RemoveColor" + colorRow.index
                        implicitHeight: Math.max(
                            root.minimumTargetSize,
                            implicitBackgroundHeight,
                            implicitContentHeight + topPadding + bottomPadding
                        )
                        text: qsTr("Remove")
                        enabled: root.enabled
                            && !!root.value && Array.isArray(root.value.colors)
                            && root.value.colors.length > 1
                        Accessible.name: qsTr("Remove gradient color %1").arg(colorRow.index + 1)

                        onClicked: {
                            const next = root.clone(root.value);
                            if (next && Array.isArray(next.colors)
                                    && next.colors.length > 1) {
                                next.colors.splice(colorRow.index, 1);
                                root.valueModified(next);
                            }
                        }
                    }
                }
            }

            Button {
                objectName:
                    (root.definition.controlObjectName || "") + "AddColor"
                implicitHeight: Math.max(
                    root.minimumTargetSize,
                    implicitBackgroundHeight,
                    implicitContentHeight + topPadding + bottomPadding
                )
                text: qsTr("Add color")
                enabled: root.enabled && !!root.value
                    && Array.isArray(root.value.colors)
                    && root.value.colors.length < 10
                Accessible.name: qsTr("Add a gradient color")

                onClicked: {
                    const next = root.clone(root.value);
                    if (next && Array.isArray(next.colors)
                            && next.colors.length < 10) {
                        next.colors.push("0xFFFFFFFF");
                        root.valueModified(next);
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: qsTr("Angle")
                    color: root.palette.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                RuleDecimalField {
                    objectName:
                        (root.definition.controlObjectName || "") + "Angle"
                    Layout.fillWidth: true
                    value: root.value ? root.value.angle : ""
                    minimumValue: -3600
                    maximumValue: 3600
                    enabled: root.enabled
                    minimumTargetSize: root.minimumTargetSize
                    accessibleName: qsTr("Gradient angle")

                    onValueModified: value => {
                        const next = root.clone(root.value);
                        if (next) {
                            next.angle = value;
                            root.valueModified(next);
                        }
                    }
                }
            }
        }
    }
}
