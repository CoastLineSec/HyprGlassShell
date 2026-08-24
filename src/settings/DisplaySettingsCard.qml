pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    property var output: null
    property var observedOutput: null
    property var allOutputs: []
    property bool existingRecord: false
    property bool controlsEnabled: true

    signal outputRequested(var output)
    signal resetRequested(string id)

    function copyOutput() {
        if (!root.output || typeof root.output !== "object")
            return null;
        try {
            return JSON.parse(JSON.stringify(root.output));
        } catch (error) {
            return null;
        }
    }

    function updateField(name, value) {
        const replacement = root.copyOutput();
        if (!replacement || replacement[name] === value)
            return;
        replacement[name] = value;
        root.outputRequested(replacement);
    }

    function positionPart(index) {
        if (!root.output || typeof root.output.position !== "string")
            return 0;
        const match = /^([+-]?[0-9]+)x([+-]?[0-9]+)$/.exec(root.output.position);
        return match ? Number(match[index + 1]) : 0;
    }

    function reservedPart(index) {
        if (!root.output || !Array.isArray(root.output.reserved) || root.output.reserved.length !== 4) {
            return 0;
        }
        return root.output.reserved[index];
    }

    function updateReservedPart(index, value) {
        const replacement = root.copyOutput();
        if (!replacement || index < 0 || index > 3)
            return;
        const reserved = Array.isArray(replacement.reserved) && replacement.reserved.length === 4 ? replacement.reserved.slice() : [0, 0, 0, 0];
        if (reserved[index] === value)
            return;
        reserved[index] = value;
        replacement.reserved = reserved;
        root.outputRequested(replacement);
    }

    function explicitMode(width, height, refreshRate, managedMode) {
        if (typeof managedMode === "string" && /^[1-9][0-9]{0,4}x[1-9][0-9]{0,4}(?:@.+)?$/.test(managedMode)) {
            return managedMode;
        }
        let refresh = Number(refreshRate);
        if (!isFinite(refresh) || refresh <= 0)
            return "%1x%2".arg(width).arg(height);
        let text = refresh.toFixed(3);
        text = text.replace(/0+$/, "").replace(/\.$/, "");
        return "%1x%2@%3".arg(width).arg(height).arg(text);
    }

    function modeChoices() {
        const choices = [
            {
                label: qsTr("Preferred"),
                value: "preferred"
            },
            {
                label: qsTr("Highest refresh rate"),
                value: "highrr"
            },
            {
                label: qsTr("Highest resolution"),
                value: "highres"
            },
            {
                label: qsTr("Widest advertised mode"),
                value: "maxwidth"
            }
        ];
        const seen = {
            preferred: true,
            highrr: true,
            highres: true,
            maxwidth: true
        };
        const modes = root.observedOutput && Array.isArray(root.observedOutput.modes) ? root.observedOutput.modes : [];
        for (const mode of modes) {
            if (!mode || !mode.width || !mode.height)
                continue;
            const value = root.explicitMode(mode.width, mode.height, mode.refreshRate, mode.managedMode);
            if (seen[value])
                continue;
            seen[value] = true;
            const refresh = Number(mode.refreshRate);
            choices.push({
                label: refresh > 0 ? qsTr("%1 × %2 at %3 Hz").arg(mode.width).arg(mode.height).arg(refresh.toFixed(2).replace(/0+$/, "").replace(/\.$/, "")) : qsTr("%1 × %2").arg(mode.width).arg(mode.height),
                value: value
            });
        }
        const current = root.output ? String(root.output.mode || "") : "";
        if (current && !seen[current])
            choices.push({
                label: qsTr("Custom (%1)").arg(current),
                value: current
            });
        return choices;
    }

    function scaleChoices() {
        const choices = [
            {
                label: qsTr("Automatic"),
                value: "auto"
            },
            {
                label: qsTr("100%"),
                value: 1
            },
            {
                label: qsTr("125%"),
                value: 1.25
            },
            {
                label: qsTr("150%"),
                value: 1.5
            },
            {
                label: qsTr("175%"),
                value: 1.75
            },
            {
                label: qsTr("200%"),
                value: 2
            }
        ];
        const current = root.output ? root.output.scale : 1;
        let found = false;
        for (const choice of choices)
            found = found || choice.value === current;
        if (!found && typeof current === "number") {
            choices.push({
                label: qsTr("Custom (%1%)").arg(Math.round(current * 100)),
                value: current
            });
        }
        return choices;
    }

    function mirrorChoices() {
        const choices = [
            {
                label: qsTr("Not mirrored"),
                value: ""
            }
        ];
        if (!Array.isArray(root.allOutputs))
            return choices;
        for (const candidate of root.allOutputs) {
            if (!candidate || !root.output || candidate.id === root.output.id || candidate.enabled !== true || candidate.mirror)
                continue;
            choices.push({
                label: String(candidate.selector),
                value: String(candidate.selector)
            });
        }
        const current = root.output ? String(root.output.mirror || "") : "";
        if (current && !choices.some(choice => choice.value === current)) {
            choices.push({
                label: qsTr("Unavailable target (%1)").arg(current),
                value: current
            });
        }
        return choices;
    }

    function indexForValue(model, value) {
        for (let index = 0; index < model.length; ++index) {
            if (model[index].value === value)
                return index;
        }
        return 0;
    }

    readonly property bool automaticPosition: root.output && typeof root.output.position === "string" && root.output.position.indexOf("auto") === 0
    readonly property var availableModes: root.modeChoices()
    readonly property var availableScales: root.scaleChoices()
    readonly property var availableMirrors: root.mirrorChoices()
    readonly property var automaticPositionChoices: [
        {
            label: qsTr("Automatic"),
            value: "auto"
        },
        {
            label: qsTr("Right of the layout"),
            value: "auto-right"
        },
        {
            label: qsTr("Left of the layout"),
            value: "auto-left"
        },
        {
            label: qsTr("Above the layout"),
            value: "auto-up"
        },
        {
            label: qsTr("Below the layout"),
            value: "auto-down"
        },
        {
            label: qsTr("Right, vertically centered"),
            value: "auto-center-right"
        },
        {
            label: qsTr("Left, vertically centered"),
            value: "auto-center-left"
        },
        {
            label: qsTr("Above, horizontally centered"),
            value: "auto-center-up"
        },
        {
            label: qsTr("Below, horizontally centered"),
            value: "auto-center-down"
        }
    ]

    Layout.fillWidth: true
    padding: 22
    visible: root.output !== null

    background: Rectangle {
        color: root.palette.base
        radius: 16
        border.color: root.palette.mid
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: root.output ? String(root.output.selector) : ""
                    color: root.palette.text
                    font.pixelSize: 19
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: root.observedOutput ? [root.observedOutput.make, root.observedOutput.model].filter(value => value && String(value).length > 0).join(" ") || qsTr("Connected display") : qsTr("Saved display")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            Switch {
                objectName: "displayEnabledSwitch"
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                checked: root.output ? root.output.enabled === true : false
                enabled: root.controlsEnabled
                focusPolicy: Qt.StrongFocus

                onToggled: {
                    if (root.output && checked !== root.output.enabled)
                        root.updateField("enabled", checked);
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width >= 620 ? 2 : 1
            columnSpacing: 18
            rowSpacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("Resolution and refresh rate")
                    color: root.palette.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                ComboBox {
                    objectName: "displayModeComboBox"
                    Layout.fillWidth: true
                    model: root.availableModes
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.indexForValue(root.availableModes, root.output ? root.output.mode : "preferred")
                    enabled: root.controlsEnabled

                    onActivated: root.updateField("mode", currentValue)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("Scale")
                    color: root.palette.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                ComboBox {
                    objectName: "displayScaleComboBox"
                    Layout.fillWidth: true
                    model: root.availableScales
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.indexForValue(root.availableScales, root.output ? root.output.scale : 1)
                    enabled: root.controlsEnabled

                    onActivated: root.updateField("scale", currentValue)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("Orientation")
                    color: root.palette.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                ComboBox {
                    id: orientationControl

                    objectName: "displayOrientationComboBox"
                    Layout.fillWidth: true
                    model: [
                        {
                            label: qsTr("Landscape"),
                            value: 0
                        },
                        {
                            label: qsTr("Portrait right"),
                            value: 1
                        },
                        {
                            label: qsTr("Landscape flipped"),
                            value: 2
                        },
                        {
                            label: qsTr("Portrait left"),
                            value: 3
                        },
                        {
                            label: qsTr("Flipped"),
                            value: 4
                        },
                        {
                            label: qsTr("Flipped portrait right"),
                            value: 5
                        },
                        {
                            label: qsTr("Flipped landscape"),
                            value: 6
                        },
                        {
                            label: qsTr("Flipped portrait left"),
                            value: 7
                        }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.indexForValue(model, root.output ? Number(root.output.transform) : 0)
                    enabled: root.controlsEnabled

                    onActivated: root.updateField("transform", currentValue)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("Arrangement")
                    color: root.palette.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                CheckBox {
                    objectName: "displayAutomaticPositionCheckBox"
                    text: qsTr("Arrange automatically")
                    checked: root.automaticPosition
                    enabled: root.controlsEnabled

                    onToggled: {
                        if (checked !== root.automaticPosition) {
                            root.updateField("position", checked ? "auto-right" : "0x0");
                        }
                    }
                }

                ComboBox {
                    objectName: "displayAutomaticPositionComboBox"
                    Layout.fillWidth: true
                    visible: root.automaticPosition
                    model: root.automaticPositionChoices
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.indexForValue(root.automaticPositionChoices, root.output ? root.output.position : "auto-right")
                    enabled: root.controlsEnabled

                    onActivated: root.updateField("position", currentValue)
                }
            }
        }

        SettingsDecimalRow {
            objectName: "displayExactScaleRow"
            Layout.fillWidth: true
            visible: root.output && root.output.scale !== "auto"
            title: qsTr("Exact display scale")
            description: qsTr("Enter any managed scale of at least 0.25. The common percentages above remain quick presets.")
            value: root.output ? root.output.scale : 1
            minimumValue: 0.25
            maximumValue: 3.4028234663852886e+38
            controlObjectName: "displayExactScaleField"
            validationObjectName: "displayExactScaleValidation"
            validationExample: "1.333333"
            accessibleName: qsTr("Exact display scale")
            enabled: root.controlsEnabled

            onValueModified: value => root.updateField("scale", value)
        }

        RowLayout {
            Layout.fillWidth: true
            visible: !root.automaticPosition
            spacing: 14

            Label {
                text: qsTr("Position")
                color: root.palette.placeholderText
                font.pixelSize: 12
            }

            SpinBox {
                objectName: "displayPositionXSpinBox"
                from: -1000000
                to: 1000000
                value: root.positionPart(0)
                editable: true
                enabled: root.controlsEnabled

                onValueModified: root.updateField("position", "%1x%2".arg(value).arg(root.positionPart(1)))
            }

            Label {
                text: "×"
                color: root.palette.placeholderText
            }

            SpinBox {
                objectName: "displayPositionYSpinBox"
                from: -1000000
                to: 1000000
                value: root.positionPart(1)
                editable: true
                enabled: root.controlsEnabled

                onValueModified: root.updateField("position", "%1x%2".arg(root.positionPart(0)).arg(value))
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("logical pixels")
                color: root.palette.placeholderText
                font.pixelSize: 11
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.palette.mid
        }

        Button {
            id: advancedButton

            objectName: "displayAdvancedButton"
            text: checked ? qsTr("Hide advanced settings") : qsTr("Show advanced settings")
            checkable: true
            flat: true
            icon.name: checked ? "go-up-symbolic" : "go-down-symbolic"
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: advancedButton.checked
            spacing: 3

            Label {
                Layout.fillWidth: true
                text: qsTr("Output signal and color")
                color: root.palette.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Choose the link format, mirroring, adaptive sync, gamut declaration, and optional ICC profile.")
                color: root.palette.placeholderText
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }

        GridLayout {
            Layout.fillWidth: true
            visible: advancedButton.checked
            columns: width >= 620 ? 2 : 1
            columnSpacing: 18
            rowSpacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("Mirror")
                    color: root.palette.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                ComboBox {
                    objectName: "displayMirrorComboBox"
                    Layout.fillWidth: true
                    model: root.availableMirrors
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.indexForValue(root.availableMirrors, root.output ? root.output.mirror : "")
                    enabled: root.controlsEnabled

                    onActivated: root.updateField("mirror", currentValue)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("Bit depth")
                    color: root.palette.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                ComboBox {
                    objectName: "displayBitDepthComboBox"
                    Layout.fillWidth: true
                    model: [
                        {
                            label: qsTr("8-bit"),
                            value: 8
                        },
                        {
                            label: qsTr("10-bit"),
                            value: 10
                        }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.output && root.output.bitdepth === 10 ? 1 : 0
                    enabled: root.controlsEnabled

                    onActivated: root.updateField("bitdepth", currentValue)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("Variable refresh rate")
                    color: root.palette.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                ComboBox {
                    objectName: "displayVrrComboBox"
                    Layout.fillWidth: true
                    model: [
                        {
                            label: qsTr("Use compositor default"),
                            value: -1
                        },
                        {
                            label: qsTr("Off"),
                            value: 0
                        },
                        {
                            label: qsTr("On"),
                            value: 1
                        },
                        {
                            label: qsTr("Fullscreen only"),
                            value: 2
                        },
                        {
                            label: qsTr("Fullscreen games and video"),
                            value: 3
                        }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.indexForValue(model, root.output ? Number(root.output.vrr) : -1)
                    enabled: root.controlsEnabled

                    onActivated: root.updateField("vrr", currentValue)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("Color management")
                    color: root.palette.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                ComboBox {
                    id: colorManagementControl

                    objectName: "displayColorManagementComboBox"
                    Layout.fillWidth: true
                    model: [
                        {
                            label: qsTr("Automatic"),
                            value: "auto"
                        },
                        {
                            label: qsTr("sRGB"),
                            value: "srgb"
                        },
                        {
                            label: qsTr("Wide gamut"),
                            value: "wide"
                        },
                        {
                            label: qsTr("From display (EDID)"),
                            value: "edid"
                        },
                        {
                            label: qsTr("HDR"),
                            value: "hdr"
                        },
                        {
                            label: qsTr("HDR from display"),
                            value: "hdredid"
                        },
                        {
                            label: qsTr("DCI-P3"),
                            value: "dcip3"
                        },
                        {
                            label: qsTr("Display P3"),
                            value: "dp3"
                        },
                        {
                            label: qsTr("Adobe RGB"),
                            value: "adobe"
                        }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.indexForValue(model, root.output ? root.output.cm : "auto")
                    enabled: root.controlsEnabled

                    onActivated: root.updateField("cm", currentValue)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("Wide color support")
                    color: root.palette.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                ComboBox {
                    objectName: "displayWideColorComboBox"
                    Layout.fillWidth: true
                    model: [
                        {
                            label: qsTr("Automatic"),
                            value: -1
                        },
                        {
                            label: qsTr("Unsupported"),
                            value: 0
                        },
                        {
                            label: qsTr("Supported"),
                            value: 1
                        }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.indexForValue(model, root.output ? Number(root.output.supportsWideColor) : -1)
                    enabled: root.controlsEnabled

                    onActivated: root.updateField("supportsWideColor", currentValue)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("HDR support")
                    color: root.palette.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                ComboBox {
                    objectName: "displayHdrComboBox"
                    Layout.fillWidth: true
                    model: [
                        {
                            label: qsTr("Automatic"),
                            value: -1
                        },
                        {
                            label: qsTr("Unsupported"),
                            value: 0
                        },
                        {
                            label: qsTr("Supported"),
                            value: 1
                        }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.indexForValue(model, root.output ? Number(root.output.supportsHdr) : -1)
                    enabled: root.controlsEnabled

                    onActivated: root.updateField("supportsHdr", currentValue)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("ICC profile")
                    color: root.palette.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                TextField {
                    objectName: "displayIccProfileTextField"
                    Layout.fillWidth: true
                    text: root.output ? String(root.output.icc || "") : ""
                    placeholderText: qsTr("No profile")
                    maximumLength: 256
                    enabled: root.controlsEnabled
                    selectByMouse: true

                    onEditingFinished: root.updateField("icc", text)
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Enter a profile path. Leave empty to use no ICC profile.")
                    color: root.palette.placeholderText
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
            }
        }

        Frame {
            objectName: "displaySdrSettingsCard"
            Layout.fillWidth: true
            visible: advancedButton.checked
            padding: 18

            background: Rectangle {
                color: root.palette.alternateBase
                radius: 14
                border.color: root.palette.mid
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("SDR reproduction")
                        color: root.palette.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Tune SDR transfer, brightness, saturation, and the luminance range reported to Hyprland.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }

                SettingsSelectRow {
                    Layout.fillWidth: true
                    title: qsTr("SDR transfer function")
                    description: qsTr("Default follows the monitor rule default; force gamma 2.2 only for displays that need it.")
                    model: [qsTr("Default"), qsTr("Automatic"), qsTr("sRGB"), qsTr("Gamma 2.2"), qsTr("Force gamma 2.2")]
                    currentIndex: root.indexForValue([
                        {
                            value: "default"
                        },
                        {
                            value: "auto"
                        },
                        {
                            value: "srgb"
                        },
                        {
                            value: "gamma22"
                        },
                        {
                            value: "gamma22force"
                        }
                    ], root.output ? root.output.sdrEotf : "default")
                    controlWidth: 210
                    controlObjectName: "displaySdrEotfComboBox"
                    accessibleName: qsTr("SDR transfer function")
                    enabled: root.controlsEnabled

                    onValueModified: index => root.updateField("sdrEotf", ["default", "auto", "srgb", "gamma22", "gamma22force"][index])
                }

                SettingsDecimalRow {
                    objectName: "displaySdrBrightnessRow"
                    Layout.fillWidth: true
                    title: qsTr("SDR brightness")
                    description: qsTr("Scale SDR brightness from 0 through 10 before presentation.")
                    value: root.output ? root.output.sdrBrightness : 1
                    minimumValue: 0
                    maximumValue: 10
                    controlObjectName: "displaySdrBrightnessField"
                    validationObjectName: "displaySdrBrightnessValidation"
                    validationExample: "1.2"
                    accessibleName: qsTr("SDR brightness multiplier")
                    enabled: root.controlsEnabled

                    onValueModified: value => root.updateField("sdrBrightness", value)
                }

                SettingsDecimalRow {
                    objectName: "displaySdrSaturationRow"
                    Layout.fillWidth: true
                    title: qsTr("SDR saturation")
                    description: qsTr("Scale SDR color saturation from 0 through 10.")
                    value: root.output ? root.output.sdrSaturation : 1
                    minimumValue: 0
                    maximumValue: 10
                    controlObjectName: "displaySdrSaturationField"
                    validationObjectName: "displaySdrSaturationValidation"
                    validationExample: "1"
                    accessibleName: qsTr("SDR saturation multiplier")
                    enabled: root.controlsEnabled

                    onValueModified: value => root.updateField("sdrSaturation", value)
                }

                SettingsDecimalRow {
                    objectName: "displaySdrMinimumLuminanceRow"
                    Layout.fillWidth: true
                    title: qsTr("SDR minimum luminance")
                    description: qsTr("Set the SDR black level from 0 through 10000 cd/m².")
                    value: root.output ? root.output.sdrMinLuminance : 0.2
                    minimumValue: 0
                    maximumValue: 10000
                    controlObjectName: "displaySdrMinimumLuminanceField"
                    validationObjectName: "displaySdrMinimumLuminanceValidation"
                    validationExample: "0.2"
                    accessibleName: qsTr("SDR minimum luminance")
                    enabled: root.controlsEnabled

                    onValueModified: value => root.updateField("sdrMinLuminance", value)
                }

                SettingsSpinBoxRow {
                    Layout.fillWidth: true
                    title: qsTr("SDR maximum luminance")
                    description: qsTr("Set the SDR white level in cd/m². Use −1 to defer to display metadata.")
                    from: -1
                    to: 2147483647
                    value: root.output ? Number(root.output.sdrMaxLuminance) : 80
                    editable: true
                    controlObjectName: "displaySdrMaximumLuminanceSpinBox"
                    accessibleName: qsTr("SDR maximum luminance")
                    enabled: root.controlsEnabled

                    onValueModified: value => root.updateField("sdrMaxLuminance", value)
                }
            }
        }

        Frame {
            objectName: "displayHdrMetadataCard"
            Layout.fillWidth: true
            visible: advancedButton.checked
            padding: 18

            background: Rectangle {
                color: root.palette.alternateBase
                radius: 14
                border.color: root.palette.mid
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("HDR luminance metadata")
                        color: root.palette.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Override display luminance metadata used by HDR color management. Leave each value at −1 to use automatic metadata.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }

                SettingsDecimalRow {
                    objectName: "displayMinimumLuminanceRow"
                    Layout.fillWidth: true
                    title: qsTr("Minimum luminance")
                    description: qsTr("Override the black level from 0 through 10000 cd/m², or use −1 for automatic.")
                    value: root.output ? root.output.minLuminance : -1
                    minimumValue: -1
                    maximumValue: 10000
                    controlObjectName: "displayMinimumLuminanceField"
                    validationObjectName: "displayMinimumLuminanceValidation"
                    validationExample: "0.005"
                    accessibleName: qsTr("HDR minimum luminance")
                    enabled: root.controlsEnabled

                    onValueModified: value => root.updateField("minLuminance", value)
                }

                SettingsSpinBoxRow {
                    Layout.fillWidth: true
                    title: qsTr("Maximum luminance")
                    description: qsTr("Override peak luminance in cd/m², or use −1 for automatic.")
                    from: -1
                    to: 2147483647
                    value: root.output ? Number(root.output.maxLuminance) : -1
                    editable: true
                    controlObjectName: "displayMaximumLuminanceSpinBox"
                    accessibleName: qsTr("HDR maximum luminance")
                    enabled: root.controlsEnabled

                    onValueModified: value => root.updateField("maxLuminance", value)
                }

                SettingsSpinBoxRow {
                    Layout.fillWidth: true
                    title: qsTr("Maximum average luminance")
                    description: qsTr("Override maximum frame-average luminance in cd/m², or use −1 for automatic.")
                    from: -1
                    to: 2147483647
                    value: root.output ? Number(root.output.maxAvgLuminance) : -1
                    editable: true
                    controlObjectName: "displayMaximumAverageLuminanceSpinBox"
                    accessibleName: qsTr("HDR maximum average luminance")
                    enabled: root.controlsEnabled

                    onValueModified: value => root.updateField("maxAvgLuminance", value)
                }
            }
        }

        Frame {
            objectName: "displayReservedAreaCard"
            Layout.fillWidth: true
            visible: advancedButton.checked
            padding: 18

            background: Rectangle {
                color: root.palette.alternateBase
                radius: 14
                border.color: root.palette.mid
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Reserved workspace area")
                        color: root.palette.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Reserve signed logical pixels on each edge in CSS order: top, right, bottom, then left.")
                        color: root.palette.placeholderText
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: width >= 620 ? 4 : 2
                    columnSpacing: 12
                    rowSpacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: qsTr("Top")
                            color: root.palette.text
                            font.pixelSize: 12
                        }

                        RuleSafeIntegerField {
                            Layout.fillWidth: true
                            value: root.reservedPart(0)
                            controlObjectName: "displayReservedTopField"
                            accessibleName: qsTr("Reserved top edge")
                            enabled: root.controlsEnabled

                            onValueChanged: Qt.callLater(synchronizeText)
                            onValueModified: value => root.updateReservedPart(0, value)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: qsTr("Right")
                            color: root.palette.text
                            font.pixelSize: 12
                        }

                        RuleSafeIntegerField {
                            Layout.fillWidth: true
                            value: root.reservedPart(1)
                            controlObjectName: "displayReservedRightField"
                            accessibleName: qsTr("Reserved right edge")
                            enabled: root.controlsEnabled

                            onValueChanged: Qt.callLater(synchronizeText)
                            onValueModified: value => root.updateReservedPart(1, value)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: qsTr("Bottom")
                            color: root.palette.text
                            font.pixelSize: 12
                        }

                        RuleSafeIntegerField {
                            Layout.fillWidth: true
                            value: root.reservedPart(2)
                            controlObjectName: "displayReservedBottomField"
                            accessibleName: qsTr("Reserved bottom edge")
                            enabled: root.controlsEnabled

                            onValueChanged: Qt.callLater(synchronizeText)
                            onValueModified: value => root.updateReservedPart(2, value)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: qsTr("Left")
                            color: root.palette.text
                            font.pixelSize: 12
                        }

                        RuleSafeIntegerField {
                            Layout.fillWidth: true
                            value: root.reservedPart(3)
                            controlObjectName: "displayReservedLeftField"
                            accessibleName: qsTr("Reserved left edge")
                            enabled: root.controlsEnabled

                            onValueChanged: Qt.callLater(synchronizeText)
                            onValueModified: value => root.updateReservedPart(3, value)
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: advancedButton.checked

            Label {
                Layout.fillWidth: true
                text: root.existingRecord ? qsTr("All pinned Hyprland 0.56.2 monitor fields for this connected output are shown above. Its stable identity remains preserved automatically.") : qsTr("This newly connected display starts from safe values, and every pinned Hyprland 0.56.2 monitor field is available above.")
                color: root.palette.placeholderText
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            Button {
                objectName: "resetDisplayButton"
                text: qsTr("Reset this display")
                enabled: root.controlsEnabled && root.output

                onClicked: {
                    if (root.output)
                        root.resetRequested(String(root.output.id));
                }
            }
        }
    }
}
