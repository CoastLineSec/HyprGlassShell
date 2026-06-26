import QtCore
import QtQuick
import QtQuick.Effects
import Quickshell
import qs.Common
import qs.Modals.FileBrowser
import qs.Services
import qs.Widgets
import qs.Modules.Settings.Widgets
import "../../Common/ConfigIncludeResolve.js" as ConfigIncludeResolve

Item {
    id: themeColorsTab

    property var parentModal: null
    readonly property bool connectedFrameModeActive: SettingsData.connectedFrameModeActive
    readonly property bool frameModeActive: SettingsData.frameEnabled
    property var cachedMatugenSchemes: Theme.availableMatugenSchemes.map(option => option.label)
    property var templateDetection: []


    function isTemplateDetected(templateId) {
        if (!templateDetection || templateDetection.length === 0)
            return true;
        var item = templateDetection.find(i => i.id === templateId);
        return !item || item.detected !== false;
    }

    function getTemplateDescription(templateId, baseDescription) {
        if (isTemplateDetected(templateId))
            return baseDescription;
        if (baseDescription)
            return baseDescription + " · " + I18n.tr("Not detected");
        return I18n.tr("Not detected");
    }

    function getTemplateDescriptionColor(templateId) {
        if (isTemplateDetected(templateId))
            return Theme.surfaceVariantText;
        return Theme.warning;
    }

    function openBlurBorderColorPicker() {
        PopoutService.colorPickerModal.selectedColor = SettingsData.blurBorderCustomColor ?? "#ffffff";
        PopoutService.colorPickerModal.pickerTitle = I18n.tr("Blur Border Color");
        PopoutService.colorPickerModal.onColorSelectedCallback = function (color) {
            SettingsData.set("blurBorderCustomColor", color.toString());
        };
        PopoutService.colorPickerModal.open();
    }

    function openM3ShadowColorPicker() {
        PopoutService.colorPickerModal.selectedColor = SettingsData.m3ElevationCustomColor ?? "#000000";
        PopoutService.colorPickerModal.pickerTitle = I18n.tr("Shadow Color");
        PopoutService.colorPickerModal.onColorSelectedCallback = function (color) {
            SettingsData.set("m3ElevationCustomColor", color.toString());
        };
        PopoutService.colorPickerModal.show();
    }


    Component.onCompleted: {
        Proc.runCommand("template-check", ["hgs", "matugen", "check"], (output, exitCode) => {
            if (exitCode !== 0)
                return;
            try {
                themeColorsTab.templateDetection = JSON.parse(output.trim());
            } catch (e) {}
        });
    }


    HGSFlickable {
        anchors.fill: parent
        clip: true
        contentHeight: mainColumn.height + Theme.spacingXL
        contentWidth: width

        Column {
            id: mainColumn
            topPadding: 4

            width: Math.min(550, parent.width - Theme.spacingL * 2)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingXL




            SettingsCard {
                tab: "theme"
                tags: ["transparency", "opacity", "widget", "styling"]
                title: I18n.tr("Widget Styling")
                settingKey: "widgetStyling"
                iconName: "opacity"

                SettingsButtonGroupRow {
                    tab: "theme"
                    tags: ["widget", "style", "colorful", "default"]
                    settingKey: "widgetColorMode"
                    text: I18n.tr("Widget Style")
                    description: I18n.tr("Change bar appearance")
                    model: [I18n.tr("Default", "widget style option"), I18n.tr("Colorful", "widget style option")]
                    currentIndex: SettingsData.widgetColorMode === "colorful" ? 1 : 0
                    onSelectionChanged: (index, selected) => {
                        if (!selected)
                            return;
                        SettingsData.set("widgetColorMode", index === 1 ? "colorful" : "default");
                    }
                }

                SettingsButtonGroupRow {
                    tab: "theme"
                    tags: ["widget", "background", "color"]
                    settingKey: "widgetBackgroundColor"
                    text: I18n.tr("Widget Background Color")
                    description: I18n.tr("Choose the background color for widgets")
                    model: ["sth", "s", "sc", "sch"]
                    buttonHeight: 20
                    minButtonWidth: 32
                    buttonPadding: Theme.spacingS
                    checkIconSize: Theme.iconSizeSmall - 2
                    textSize: Theme.fontSizeSmall - 2
                    spacing: 1
                    currentIndex: {
                        switch (SettingsData.widgetBackgroundColor) {
                        case "sth":
                            return 0;
                        case "s":
                            return 1;
                        case "sc":
                            return 2;
                        case "sch":
                            return 3;
                        default:
                            return 0;
                        }
                    }
                    onSelectionChanged: (index, selected) => {
                        if (!selected)
                            return;
                        const colorOptions = ["sth", "s", "sc", "sch"];
                        SettingsData.set("widgetBackgroundColor", colorOptions[index]);
                    }
                }

                SettingsDropdownRow {
                    tab: "theme"
                    tags: ["control", "center", "tile", "button", "color", "active"]
                    settingKey: "controlCenterTileColorMode"
                    text: I18n.tr("Control Center Tile Color")
                    description: I18n.tr("Active tile background and icon color", "control center tile color setting description")
                    options: [I18n.tr("Primary", "tile color option"), I18n.tr("Primary Container", "tile color option"), I18n.tr("Secondary", "tile color option"), I18n.tr("Surface Variant", "tile color option")]
                    currentValue: {
                        switch (SettingsData.controlCenterTileColorMode) {
                        case "primaryContainer":
                            return I18n.tr("Primary Container", "tile color option");
                        case "secondary":
                            return I18n.tr("Secondary", "tile color option");
                        case "surfaceVariant":
                            return I18n.tr("Surface Variant", "tile color option");
                        default:
                            return I18n.tr("Primary", "tile color option");
                        }
                    }
                    onValueChanged: value => {
                        if (value === I18n.tr("Primary Container", "tile color option")) {
                            SettingsData.set("controlCenterTileColorMode", "primaryContainer");
                        } else if (value === I18n.tr("Secondary", "tile color option")) {
                            SettingsData.set("controlCenterTileColorMode", "secondary");
                        } else if (value === I18n.tr("Surface Variant", "tile color option")) {
                            SettingsData.set("controlCenterTileColorMode", "surfaceVariant");
                        } else {
                            SettingsData.set("controlCenterTileColorMode", "primary");
                        }
                    }
                }

                SettingsDropdownRow {
                    tab: "theme"
                    tags: ["button", "color", "primary", "accent"]
                    settingKey: "buttonColorMode"
                    text: I18n.tr("Button Color")
                    description: I18n.tr("Color for primary action buttons")
                    options: [I18n.tr("Primary", "button color option"), I18n.tr("Primary Container", "button color option"), I18n.tr("Secondary", "button color option"), I18n.tr("Surface Variant", "button color option")]
                    currentValue: {
                        switch (SettingsData.buttonColorMode) {
                        case "primaryContainer":
                            return I18n.tr("Primary Container", "button color option");
                        case "secondary":
                            return I18n.tr("Secondary", "button color option");
                        case "surfaceVariant":
                            return I18n.tr("Surface Variant", "button color option");
                        default:
                            return I18n.tr("Primary", "button color option");
                        }
                    }
                    onValueChanged: value => {
                        if (value === I18n.tr("Primary Container", "button color option")) {
                            SettingsData.set("buttonColorMode", "primaryContainer");
                        } else if (value === I18n.tr("Secondary", "button color option")) {
                            SettingsData.set("buttonColorMode", "secondary");
                        } else if (value === I18n.tr("Surface Variant", "button color option")) {
                            SettingsData.set("buttonColorMode", "surfaceVariant");
                        } else {
                            SettingsData.set("buttonColorMode", "primary");
                        }
                    }
                }

                SettingsControlledByFrame {
                    visible: themeColorsTab.connectedFrameModeActive
                    parentModal: themeColorsTab.parentModal
                    settingLabel: I18n.tr("Surface Opacity")
                    reason: I18n.tr("Managed by Frame in Connected Mode")
                }

                SettingsSliderRow {
                    tab: "theme"
                    tags: ["surface", "popup", "transparency", "opacity", "modal"]
                    settingKey: "popupTransparency"
                    text: I18n.tr("Surface Opacity")
                    description: I18n.tr("Controls opacity of shell surfaces, popouts, and modals")
                    visible: !themeColorsTab.connectedFrameModeActive
                    value: Math.round(SettingsData.popupTransparency * 100)
                    minimum: 0
                    maximum: 100
                    unit: "%"
                    defaultValue: 100
                    onSliderValueChanged: newValue => SettingsData.set("popupTransparency", newValue / 100)
                }

                SettingsSliderRow {
                    tab: "theme"
                    tags: ["corner", "radius", "rounded", "square"]
                    settingKey: "cornerRadius"
                    text: I18n.tr("Corner Radius")
                    description: I18n.tr("0 = square corners")
                    value: SettingsData.cornerRadius
                    minimum: 0
                    maximum: 32
                    unit: "px"
                    defaultValue: 12
                    onSliderValueChanged: newValue => SettingsData.setCornerRadius(newValue)
                }
            }

            SettingsCard {
                tab: "theme"
                tags: ["blur", "background", "transparency", "glass", "frosted"]
                title: I18n.tr("Background Blur")
                settingKey: "blurEnabled"
                iconName: "blur_on"

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["blur", "background", "transparency", "glass", "frosted"]
                    settingKey: "blurEnabled"
                    text: I18n.tr("Background Blur")
                    description: !BlurService.available ? I18n.tr("Your compositor does not support background blur (ext-background-effect-v1)") : I18n.tr("Blur the background behind bars, popouts, modals, and notifications. Requires compositor support. Adjust Opacity accordingly.")
                    checked: SettingsData.blurEnabled ?? false
                    enabled: BlurService.available
                    onToggled: checked => SettingsData.set("blurEnabled", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["blur", "foreground", "layers", "contrast", "glass", "frosted"]
                    settingKey: "blurForegroundLayers"
                    text: I18n.tr("Foreground Layers")
                    description: I18n.tr("Show foreground surfaces on blurred panels for stronger contrast")
                    checked: SettingsData.blurForegroundLayers ?? true
                    visible: BlurService.available && (SettingsData.blurEnabled ?? false)
                    enabled: BlurService.available
                    onToggled: checked => SettingsData.set("blurForegroundLayers", checked)
                }

                SettingsSliderRow {
                    tab: "theme"
                    tags: ["blur", "foreground", "layers", "outline", "border", "cards", "widgets", "notifications", "control center"]
                    settingKey: "blurLayerOutlineOpacity"
                    text: I18n.tr("Layer Outline Opacity")
                    description: I18n.tr("Controls outlines around blurred foreground cards, pills, and notification cards")
                    visible: BlurService.available && (SettingsData.blurEnabled ?? false)
                    value: Math.round((SettingsData.blurLayerOutlineOpacity ?? 0.12) * 100)
                    minimum: 0
                    maximum: 40
                    unit: "%"
                    defaultValue: 12
                    onSliderValueChanged: newValue => SettingsData.set("blurLayerOutlineOpacity", newValue / 100)
                }

                SettingsDropdownRow {
                    tab: "theme"
                    tags: ["blur", "border", "outline", "edge"]
                    settingKey: "blurBorderColor"
                    text: I18n.tr("Blur Border Color")
                    description: I18n.tr("Border color around blurred surfaces")
                    visible: SettingsData.blurEnabled
                    options: [I18n.tr("Outline", "blur border color"), I18n.tr("Primary", "blur border color"), I18n.tr("Secondary", "blur border color"), I18n.tr("Text Color", "blur border color"), I18n.tr("Custom", "blur border color")]
                    currentValue: {
                        switch (SettingsData.blurBorderColor) {
                        case "primary":
                            return I18n.tr("Primary", "blur border color");
                        case "secondary":
                            return I18n.tr("Secondary", "blur border color");
                        case "surfaceText":
                            return I18n.tr("Text Color", "blur border color");
                        case "custom":
                            return I18n.tr("Custom", "blur border color");
                        default:
                            return I18n.tr("Outline", "blur border color");
                        }
                    }
                    onValueChanged: value => {
                        if (value === I18n.tr("Primary", "blur border color")) {
                            SettingsData.set("blurBorderColor", "primary");
                        } else if (value === I18n.tr("Secondary", "blur border color")) {
                            SettingsData.set("blurBorderColor", "secondary");
                        } else if (value === I18n.tr("Text Color", "blur border color")) {
                            SettingsData.set("blurBorderColor", "surfaceText");
                        } else if (value === I18n.tr("Custom", "blur border color")) {
                            SettingsData.set("blurBorderColor", "custom");
                            openBlurBorderColorPicker();
                        } else {
                            SettingsData.set("blurBorderColor", "outline");
                        }
                    }
                }

                SettingsSliderRow {
                    tab: "theme"
                    tags: ["blur", "border", "opacity"]
                    settingKey: "blurBorderOpacity"
                    text: I18n.tr("Blur Border Opacity")
                    description: I18n.tr("Controls the outer edge of protocol-blurred windows")
                    visible: SettingsData.blurEnabled
                    value: Math.round((SettingsData.blurBorderOpacity ?? 0.35) * 100)
                    minimum: 0
                    maximum: 100
                    unit: "%"
                    defaultValue: 35
                    onSliderValueChanged: newValue => SettingsData.set("blurBorderOpacity", newValue / 100)
                }
            }

            SettingsCard {
                tab: "theme"
                tags: ["elevation", "shadow", "lift", "m3", "material"]
                title: I18n.tr("Shadows")
                settingKey: "m3ElevationEnabled"
                iconName: "layers"

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["elevation", "shadow", "lift", "m3", "material"]
                    settingKey: "m3ElevationEnabled"
                    text: I18n.tr("Shadows")
                    description: I18n.tr("Material inspired shadows and elevation on modals, popouts, and dialogs")
                    checked: SettingsData.m3ElevationEnabled ?? true
                    onToggled: checked => SettingsData.set("m3ElevationEnabled", checked)
                }

                SettingsSliderRow {
                    tab: "theme"
                    tags: ["elevation", "shadow", "intensity", "blur", "m3"]
                    settingKey: "m3ElevationIntensity"
                    text: I18n.tr("Shadow Intensity")
                    description: I18n.tr("Controls the base blur radius and offset of shadows")
                    value: SettingsData.m3ElevationIntensity ?? 12
                    minimum: 0
                    maximum: 100
                    unit: "px"
                    defaultValue: 12
                    visible: SettingsData.m3ElevationEnabled ?? true
                    onSliderValueChanged: newValue => SettingsData.set("m3ElevationIntensity", newValue)
                }

                SettingsSliderRow {
                    tab: "theme"
                    tags: ["elevation", "shadow", "opacity", "transparency", "m3"]
                    settingKey: "m3ElevationOpacity"
                    text: I18n.tr("Shadow Opacity")
                    description: I18n.tr("Controls the opacity of the shadow")
                    value: SettingsData.m3ElevationOpacity ?? 30
                    minimum: 0
                    maximum: 100
                    unit: "%"
                    defaultValue: 30
                    visible: SettingsData.m3ElevationEnabled ?? true
                    onSliderValueChanged: newValue => SettingsData.set("m3ElevationOpacity", newValue)
                }

                SettingsDropdownRow {
                    tab: "theme"
                    tags: ["elevation", "shadow", "color", "m3"]
                    settingKey: "m3ElevationColorMode"
                    text: I18n.tr("Shadow Color")
                    description: I18n.tr("Base color for shadows (opacity is applied automatically)")
                    options: [I18n.tr("Default (Black)", "shadow color option"), I18n.tr("Text Color", "shadow color option"), I18n.tr("Primary", "shadow color option"), I18n.tr("Surface Variant", "shadow color option"), I18n.tr("Custom", "shadow color option")]
                    currentValue: {
                        switch (SettingsData.m3ElevationColorMode) {
                        case "text":
                            return I18n.tr("Text Color", "shadow color option");
                        case "primary":
                            return I18n.tr("Primary", "shadow color option");
                        case "surfaceVariant":
                            return I18n.tr("Surface Variant", "shadow color option");
                        case "custom":
                            return I18n.tr("Custom", "shadow color option");
                        default:
                            return I18n.tr("Default (Black)", "shadow color option");
                        }
                    }
                    visible: SettingsData.m3ElevationEnabled ?? true
                    onValueChanged: value => {
                        if (value === I18n.tr("Primary", "shadow color option")) {
                            SettingsData.set("m3ElevationColorMode", "primary");
                        } else if (value === I18n.tr("Surface Variant", "shadow color option")) {
                            SettingsData.set("m3ElevationColorMode", "surfaceVariant");
                        } else if (value === I18n.tr("Custom", "shadow color option")) {
                            SettingsData.set("m3ElevationColorMode", "custom");
                            openM3ShadowColorPicker();
                        } else if (value === I18n.tr("Text Color", "shadow color option")) {
                            SettingsData.set("m3ElevationColorMode", "text");
                        } else {
                            SettingsData.set("m3ElevationColorMode", "default");
                        }
                    }
                }

                SettingsDropdownRow {
                    tab: "theme"
                    tags: ["elevation", "shadow", "direction", "light", "advanced", "m3"]
                    settingKey: "m3ElevationLightDirection"
                    text: I18n.tr("Light Direction")
                    description: I18n.tr("Controls shadow cast direction for elevation layers")
                    options: [I18n.tr("Auto (Bar-aware)", "shadow direction option"), I18n.tr("Top (Default)", "shadow direction option"), I18n.tr("Top Left", "shadow direction option"), I18n.tr("Top Right", "shadow direction option"), I18n.tr("Bottom", "shadow direction option")]
                    currentValue: {
                        switch (SettingsData.m3ElevationLightDirection) {
                        case "autoBar":
                            return I18n.tr("Auto (Bar-aware)", "shadow direction option");
                        case "topLeft":
                            return I18n.tr("Top Left", "shadow direction option");
                        case "topRight":
                            return I18n.tr("Top Right", "shadow direction option");
                        case "bottom":
                            return I18n.tr("Bottom", "shadow direction option");
                        default:
                            return I18n.tr("Top (Default)", "shadow direction option");
                        }
                    }
                    visible: SettingsData.m3ElevationEnabled ?? true
                    onValueChanged: value => {
                        if (value === I18n.tr("Auto (Bar-aware)", "shadow direction option")) {
                            SettingsData.set("m3ElevationLightDirection", "autoBar");
                        } else if (value === I18n.tr("Top Left", "shadow direction option")) {
                            SettingsData.set("m3ElevationLightDirection", "topLeft");
                        } else if (value === I18n.tr("Top Right", "shadow direction option")) {
                            SettingsData.set("m3ElevationLightDirection", "topRight");
                        } else if (value === I18n.tr("Bottom", "shadow direction option")) {
                            SettingsData.set("m3ElevationLightDirection", "bottom");
                        } else {
                            SettingsData.set("m3ElevationLightDirection", "top");
                        }
                    }
                }

                Item {
                    visible: (SettingsData.m3ElevationEnabled ?? true) && SettingsData.m3ElevationColorMode === "custom"
                    width: parent.width
                    implicitHeight: 36
                    height: implicitHeight

                    Row {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.spacingM

                        StyledText {
                            text: I18n.tr("Custom Shadow Color")
                            color: Theme.surfaceText
                            font.pixelSize: Theme.fontSizeMedium
                            verticalAlignment: Text.AlignVCenter
                        }

                        Rectangle {
                            width: 26
                            height: 26
                            radius: 13
                            color: SettingsData.m3ElevationCustomColor ?? "#000000"
                            border.color: Theme.outline
                            border.width: 1

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: openM3ShadowColorPicker()
                            }
                        }
                    }
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["elevation", "shadow", "modal", "dialog", "m3"]
                    settingKey: "modalElevationEnabled"
                    text: I18n.tr("Modal Shadows")
                    description: I18n.tr("Shadow elevation on modals and dialogs")
                    checked: SettingsData.modalElevationEnabled ?? true
                    visible: SettingsData.m3ElevationEnabled ?? true
                    onToggled: checked => SettingsData.set("modalElevationEnabled", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["elevation", "shadow", "popout", "popup", "osd", "dropdown", "m3"]
                    settingKey: "popoutElevationEnabled"
                    text: I18n.tr("Popout Shadows")
                    description: I18n.tr("Shadow elevation on popouts, OSDs, and dropdowns")
                    checked: SettingsData.popoutElevationEnabled ?? true
                    visible: SettingsData.m3ElevationEnabled ?? true
                    onToggled: checked => SettingsData.set("popoutElevationEnabled", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["elevation", "shadow", "bar", "panel", "navigation", "m3"]
                    settingKey: "barElevationEnabled"
                    text: I18n.tr("Bar Shadows")
                    description: I18n.tr("Shadow elevation on bars and panels")
                    checked: SettingsData.barElevationEnabled ?? true
                    visible: SettingsData.m3ElevationEnabled ?? true
                    onToggled: checked => SettingsData.set("barElevationEnabled", checked)
                }
            }

            SettingsCard {
                tab: "theme"
                tags: ["modal", "darken", "background", "overlay"]
                title: I18n.tr("Modal Background")
                settingKey: "modalBackground"
                iconName: "layers"

                SettingsControlledByFrame {
                    visible: themeColorsTab.frameModeActive
                    parentModal: themeColorsTab.parentModal
                    settingLabel: I18n.tr("Darken Modal Background")
                    reason: I18n.tr("Disabled by Frame Mode")
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["modal", "darken", "background", "overlay"]
                    settingKey: "modalDarkenBackground"
                    text: I18n.tr("Darken Modal Background")
                    description: I18n.tr("Show darkened overlay behind modal dialogs")
                    visible: !themeColorsTab.frameModeActive
                    checked: SettingsData.modalDarkenBackground
                    onToggled: checked => SettingsData.set("modalDarkenBackground", checked)
                }
            }

            SettingsCard {
                tab: "theme"
                tags: ["applications", "portal", "dark", "terminal"]
                title: I18n.tr("Applications")
                settingKey: "applications"
                iconName: "apps"

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["portal", "sync", "dark", "mode"]
                    settingKey: "syncModeWithPortal"
                    text: I18n.tr("Sync Mode with Portal")
                    description: I18n.tr("Sync dark mode with settings portals for system-wide theme hints")
                    checked: SettingsData.syncModeWithPortal
                    onToggled: checked => SettingsData.set("syncModeWithPortal", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["terminal", "dark", "always"]
                    settingKey: "terminalsAlwaysDark"
                    text: I18n.tr("Terminals - Always use Dark Theme")
                    description: I18n.tr("Force terminal applications to always use dark color schemes")
                    checked: SettingsData.terminalsAlwaysDark
                    onToggled: checked => SettingsData.set("terminalsAlwaysDark", checked)
                }
            }



            SettingsCard {
                tab: "theme"
                tags: ["matugen", "templates", "theming"]
                title: I18n.tr("Matugen Templates")
                settingKey: "matugenTemplates"
                iconName: "auto_awesome"
                collapsible: true
                expanded: false
                visible: Theme.matugenAvailable

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "user", "templates"]
                    settingKey: "runUserMatugenTemplates"
                    text: I18n.tr("Run User Templates")
                    description: ""
                    checked: SettingsData.runUserMatugenTemplates
                    onToggled: checked => SettingsData.set("runUserMatugenTemplates", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "hgs", "templates"]
                    settingKey: "runHgsMatugenTemplates"
                    text: I18n.tr("Run HGS Templates")
                    description: ""
                    checked: SettingsData.runHgsMatugenTemplates
                    onToggled: checked => SettingsData.set("runHgsMatugenTemplates", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "gtk", "template"]
                    settingKey: "matugenTemplateGtk"
                    text: "GTK"
                    description: getTemplateDescription("gtk", "")
                    descriptionColor: getTemplateDescriptionColor("gtk")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateGtk
                    onToggled: checked => SettingsData.set("matugenTemplateGtk", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "hyprland", "template"]
                    settingKey: "matugenTemplateHyprland"
                    text: "Hyprland"
                    description: getTemplateDescription("hyprland", "")
                    descriptionColor: getTemplateDescriptionColor("hyprland")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateHyprland
                    onToggled: checked => SettingsData.set("matugenTemplateHyprland", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "qt5ct", "template"]
                    settingKey: "matugenTemplateQt5ct"
                    text: "qt5ct"
                    description: getTemplateDescription("qt5ct", "")
                    descriptionColor: getTemplateDescriptionColor("qt5ct")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateQt5ct
                    onToggled: checked => SettingsData.set("matugenTemplateQt5ct", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "qt6ct", "template"]
                    settingKey: "matugenTemplateQt6ct"
                    text: "qt6ct"
                    description: getTemplateDescription("qt6ct", "")
                    descriptionColor: getTemplateDescriptionColor("qt6ct")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateQt6ct
                    onToggled: checked => SettingsData.set("matugenTemplateQt6ct", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "firefox", "template"]
                    settingKey: "matugenTemplateFirefox"
                    text: "Firefox"
                    description: getTemplateDescription("firefox", "")
                    descriptionColor: getTemplateDescriptionColor("firefox")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateFirefox
                    onToggled: checked => SettingsData.set("matugenTemplateFirefox", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "pywalfox", "template"]
                    settingKey: "matugenTemplatePywalfox"
                    text: "pywalfox"
                    description: getTemplateDescription("pywalfox", "")
                    descriptionColor: getTemplateDescriptionColor("pywalfox")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplatePywalfox
                    onToggled: checked => SettingsData.set("matugenTemplatePywalfox", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "zenbrowser", "template"]
                    settingKey: "matugenTemplateZenBrowser"
                    text: "zenbrowser"
                    description: getTemplateDescription("zenbrowser", "")
                    descriptionColor: getTemplateDescriptionColor("zenbrowser")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateZenBrowser
                    onToggled: checked => SettingsData.set("matugenTemplateZenBrowser", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "vesktop", "discord", "template"]
                    settingKey: "matugenTemplateVesktop"
                    text: "vesktop"
                    description: getTemplateDescription("vesktop", "")
                    descriptionColor: getTemplateDescriptionColor("vesktop")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateVesktop
                    onToggled: checked => SettingsData.set("matugenTemplateVesktop", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "vencord", "discord", "template"]
                    settingKey: "matugenTemplateVencord"
                    text: "vencord"
                    description: getTemplateDescription("vencord", "")
                    descriptionColor: getTemplateDescriptionColor("vencord")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateVencord
                    onToggled: checked => SettingsData.set("matugenTemplateVencord", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "equibop", "discord", "template"]
                    settingKey: "matugenTemplateEquibop"
                    text: "equibop"
                    description: getTemplateDescription("equibop", "")
                    descriptionColor: getTemplateDescriptionColor("equibop")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateEquibop
                    onToggled: checked => SettingsData.set("matugenTemplateEquibop", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "ghostty", "terminal", "template"]
                    settingKey: "matugenTemplateGhostty"
                    text: "Ghostty"
                    description: getTemplateDescription("ghostty", "")
                    descriptionColor: getTemplateDescriptionColor("ghostty")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateGhostty
                    onToggled: checked => SettingsData.set("matugenTemplateGhostty", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "kitty", "terminal", "template"]
                    settingKey: "matugenTemplateKitty"
                    text: "kitty"
                    description: getTemplateDescription("kitty", "")
                    descriptionColor: getTemplateDescriptionColor("kitty")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateKitty
                    onToggled: checked => SettingsData.set("matugenTemplateKitty", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "foot", "terminal", "template"]
                    settingKey: "matugenTemplateFoot"
                    text: "foot"
                    description: getTemplateDescription("foot", "")
                    descriptionColor: getTemplateDescriptionColor("foot")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateFoot
                    onToggled: checked => SettingsData.set("matugenTemplateFoot", checked)
                }

                SettingsDivider {
                    visible: neovimThemeToggle.visible && neovimThemeToggle.checked
                }

                SettingsToggleRow {
                    id: neovimThemeToggle
                    tab: "theme"
                    tags: ["matugen", "neovim", "terminal", "template"]
                    settingKey: "matugenTemplateNeovim"
                    text: "neovim"
                    description: getTemplateDescription("nvim", I18n.tr("Required plugin: ") + "https://github.com/AvengeMedia/base46")
                    descriptionColor: getTemplateDescriptionColor("nvim")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateNeovim
                    onToggled: checked => SettingsData.set("matugenTemplateNeovim", checked)
                }

                SettingsDropdownRow {
                    text: I18n.tr("Dark mode base")
                    tab: "theme"
                    tags: ["matugen", "neovim", "terminal", "template"]
                    settingKey: "matugenTemplateNeovimSettings"
                    description: "Base to derive dark theme from"
                    visible: neovimThemeToggle.visible && neovimThemeToggle.checked
                    currentValue: SettingsData.matugenTemplateNeovimSettings?.dark?.baseTheme ?? "github_dark"
                    options: ["aquarium", "ashes", "aylin", "ayu_dark", "bearded-arc", "carbonfox", "catppuccin", "chadracula", "chadracula-evondev", "chadtain", "chocolate", "darcula-dark", "dark_horizon", "decay", "default-dark", "doomchad", "eldritch", "embark", "everblush", "everforest", "falcon", "flexoki", "flouromachine", "gatekeeper", "github_dark", "gruvbox", "gruvchad", "hiberbee", "horizon", "jabuti", "jellybeans", "kanagawa", "kanagawa-dragon", "material-darker", "material-deep-ocean", "melange", "midnight_breeze", "mito-laser", "monekai", "monochrome", "mountain", "neofusion", "nightfox", "nightlamp", "nightowl", "nord", "obsidian-ember", "oceanic-next", "onedark", "onenord", "oxocarbon", "palenight", "pastelDark", "pastelbeans", "penumbra_dark", "poimandres", "radium", "rosepine", "rxyhn", "scaryforest", "seoul256_dark", "solarized_dark", "solarized_osaka", "starlight", "sweetpastel", "tokyodark", "tokyonight", "tomorrow_night", "tundra", "vesper", "vscode_dark", "wombat", "yoru", "zenburn"]
                    enableFuzzySearch: true
                    onValueChanged: value => {
                        const settings = SettingsData.matugenTemplateNeovimSettings;
                        settings.dark.baseTheme = value;
                        SettingsData.set("matugenTemplateNeovimSettings", settings);
                    }
                }

                SettingsDropdownRow {
                    text: I18n.tr("Light mode base")
                    tab: "theme"
                    tags: ["matugen", "neovim", "terminal", "template"]
                    settingKey: "matugenTemplateNeovimSettings"
                    description: "Base to derive light theme from"
                    visible: neovimThemeToggle.visible && neovimThemeToggle.checked
                    currentValue: SettingsData.matugenTemplateNeovimSettings?.light?.baseTheme ?? "github_light"
                    options: ["ayu_light", "blossom_light", "catppuccin-latte", "default-light", "everforest_light", "flex-light", "flexoki-light", "github_light", "gruvbox_light", "material-lighter", "nano-light", "oceanic-light", "one_light", "onenord_light", "penumbra_light", "rosepine-dawn", "seoul256_light", "solarized_light", "sunrise_breeze", "vscode_light"]
                    enableFuzzySearch: true
                    onValueChanged: value => {
                        const settings = SettingsData.matugenTemplateNeovimSettings;
                        settings.light.baseTheme = value;
                        SettingsData.set("matugenTemplateNeovimSettings", settings);
                    }
                }

                SettingsSliderRow {
                    text: I18n.tr("Dark mode harmony")
                    tags: ["matugen", "neovim", "terminal", "template"]
                    settingKey: "matugenTemplateNeovimSettings"
                    description: "How much should the base dark theme be tinted"
                    visible: neovimThemeToggle.visible && neovimThemeToggle.checked
                    minimum: 0
                    maximum: 100
                    value: (SettingsData.matugenTemplateNeovimSettings?.dark?.harmony ?? 0.5) * 100
                    defaultValue: 50
                    onSliderValueChanged: value => {
                        const settings = SettingsData.matugenTemplateNeovimSettings;
                        settings.dark.harmony = value / 100;
                        SettingsData.set("matugenTemplateNeovimSettings", settings);
                    }
                }

                SettingsSliderRow {
                    text: I18n.tr("Light mode harmony")
                    tags: ["matugen", "neovim", "terminal", "template"]
                    settingKey: "matugenTemplateNeovimSettings"
                    description: "How much should the base light theme be tinted"
                    visible: neovimThemeToggle.visible && neovimThemeToggle.checked
                    minimum: 0
                    maximum: 100
                    value: (SettingsData.matugenTemplateNeovimSettings?.light?.harmony ?? 0.5) * 100
                    defaultValue: 50
                    onSliderValueChanged: value => {
                        const settings = SettingsData.matugenTemplateNeovimSettings;
                        settings.light.harmony = value / 100;
                        SettingsData.set("matugenTemplateNeovimSettings", settings);
                    }
                }

                SettingsToggleRow {
                    text: I18n.tr("Follow HGS background color")
                    tags: ["matugen", "neovim", "terminal", "template"]
                    settingKey: "matugenTemplateNeovimSetBackground"
                    visible: neovimThemeToggle.visible && neovimThemeToggle.checked
                    checked: SettingsData.matugenTemplateNeovimSetBackground ?? true
                    onToggled: checked => SettingsData.set("matugenTemplateNeovimSetBackground", checked)
                }

                SettingsDivider {
                    visible: neovimThemeToggle.visible && neovimThemeToggle.checked
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "alacritty", "terminal", "template"]
                    settingKey: "matugenTemplateAlacritty"
                    text: "Alacritty"
                    description: getTemplateDescription("alacritty", "")
                    descriptionColor: getTemplateDescriptionColor("alacritty")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateAlacritty
                    onToggled: checked => SettingsData.set("matugenTemplateAlacritty", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "wezterm", "terminal", "template"]
                    settingKey: "matugenTemplateWezterm"
                    text: "WezTerm"
                    description: getTemplateDescription("wezterm", "")
                    descriptionColor: getTemplateDescriptionColor("wezterm")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateWezterm
                    onToggled: checked => SettingsData.set("matugenTemplateWezterm", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "dgop", "template"]
                    settingKey: "matugenTemplateDgop"
                    text: "dgop"
                    description: getTemplateDescription("dgop", "")
                    descriptionColor: getTemplateDescriptionColor("dgop")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateDgop
                    onToggled: checked => SettingsData.set("matugenTemplateDgop", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "kcolorscheme", "kde", "template"]
                    settingKey: "matugenTemplateKcolorscheme"
                    text: "KColorScheme"
                    description: getTemplateDescription("kcolorscheme", "")
                    descriptionColor: getTemplateDescriptionColor("kcolorscheme")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateKcolorscheme
                    onToggled: checked => SettingsData.set("matugenTemplateKcolorscheme", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "vscode", "code", "template"]
                    settingKey: "matugenTemplateVscode"
                    text: "VS Code"
                    description: getTemplateDescription("vscode", "")
                    descriptionColor: getTemplateDescriptionColor("vscode")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateVscode
                    onToggled: checked => SettingsData.set("matugenTemplateVscode", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "emacs", "template"]
                    settingKey: "matugenTemplateEmacs"
                    text: "Emacs"
                    description: getTemplateDescription("emacs", "")
                    descriptionColor: getTemplateDescriptionColor("emacs")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateEmacs
                    onToggled: checked => SettingsData.set("matugenTemplateEmacs", checked)
                }

                SettingsToggleRow {
                    tab: "theme"
                    tags: ["matugen", "zed", "template"]
                    settingKey: "matugenTemplateZed"
                    text: "Zed"
                    description: getTemplateDescription("zed", "")
                    descriptionColor: getTemplateDescriptionColor("zed")
                    visible: SettingsData.runHgsMatugenTemplates
                    checked: SettingsData.matugenTemplateZed
                    onToggled: checked => SettingsData.set("matugenTemplateZed", checked)
                }
            }

            Rectangle {
                width: parent.width
                height: warningText.implicitHeight + Theme.spacingM * 2
                radius: Theme.cornerRadius
                color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.12)

                Row {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingM
                    spacing: Theme.spacingM

                    HGSIcon {
                        name: "info"
                        size: Theme.iconSizeSmall
                        color: Theme.warning
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    StyledText {
                        id: warningText
                        font.pixelSize: Theme.fontSizeSmall
                        text: I18n.tr("The below settings will modify your GTK and Qt settings. If you wish to preserve your current configurations, please back them up (qt5ct.conf|qt6ct.conf and ~/.config/gtk-3.0|gtk-4.0).")
                        wrapMode: Text.WordWrap
                        width: parent.width - Theme.iconSizeSmall - Theme.spacingM
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            SettingsCard {
                tab: "theme"
                tags: ["system", "app", "theming", "gtk", "qt"]
                title: I18n.tr("System App Theming")
                settingKey: "systemAppTheming"
                iconName: "brush"
                visible: Theme.matugenAvailable

                Row {
                    width: parent.width
                    spacing: Theme.spacingM

                    Rectangle {
                        width: (parent.width - Theme.spacingM) / 2
                        height: 48
                        radius: Theme.cornerRadius
                        color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.12)

                        Row {
                            anchors.centerIn: parent
                            spacing: Theme.spacingS

                            HGSIcon {
                                name: "settings"
                                size: 16
                                color: Theme.primary
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            StyledText {
                                text: I18n.tr("Apply GTK Colors")
                                font.pixelSize: Theme.fontSizeMedium
                                color: Theme.primary
                                font.weight: Font.Medium
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Theme.applyGtkColors()
                        }
                    }

                    Rectangle {
                        width: (parent.width - Theme.spacingM) / 2
                        height: 48
                        radius: Theme.cornerRadius
                        color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.12)

                        Row {
                            anchors.centerIn: parent
                            spacing: Theme.spacingS

                            HGSIcon {
                                name: "settings"
                                size: 16
                                color: Theme.primary
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            StyledText {
                                text: I18n.tr("Apply Qt Colors")
                                font.pixelSize: Theme.fontSizeMedium
                                color: Theme.primary
                                font.weight: Font.Medium
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Theme.applyQtColors()
                        }
                    }
                }

                StyledText {
                    text: I18n.tr('Generate baseline GTK3/4 or QT5/QT6 (requires qt6ct-kde) configurations to follow HGS colors. Only needed once.<br /><br />It is recommended to configure <a href="https://github.com/CoastLineSec/HyprGlassShell/blob/master/README.md#Theming" style="text-decoration:none; color:%1;">adw-gtk3</a> prior to applying GTK themes.').arg(Theme.primary)
                    textFormat: Text.RichText
                    linkColor: Theme.primary
                    onLinkActivated: url => Qt.openUrlExternally(url)
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.surfaceVariantText
                    wrapMode: Text.WordWrap
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                        acceptedButtons: Qt.NoButton
                        propagateComposedEvents: true
                    }
                }
            }
        }
    }

}
