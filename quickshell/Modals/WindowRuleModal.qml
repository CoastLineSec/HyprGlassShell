import QtQuick
import Quickshell
import qs.Common
import qs.Services
import qs.Widgets

FloatingWindow {
    id: root

    property bool disablePopupTransparency: true
    property var editingRule: null
    property bool isEditMode: editingRule !== null
    property bool submitting: false
    property var targetWindow: null

    signal ruleSubmitted

    readonly property int inputFieldHeight: Theme.fontSizeMedium + Theme.spacingL * 2
    readonly property int sectionSpacing: Theme.spacingL

    objectName: "windowRuleModal"
    title: isEditMode ? I18n.tr("Edit Window Rule") : I18n.tr("Create Window Rule")
    minimumSize: Qt.size(500, 600)
    maximumSize: Qt.size(500, 600)
    color: Theme.surfaceContainer
    visible: false

    function resetForm() {
        nameInput.text = "";
        appIdInput.text = "";
        titleInput.text = "";
        condFloating.triState = 0;
        condXwayland.triState = 0;
        condFullscreen.triState = 0;
        condPinned.triState = 0;
        condInitialised.triState = 0;
        opacityEnabled.checked = false;
        opacitySlider.value = 100;
        floatingToggle.checked = false;
        maximizedToggle.checked = false;
        fullscreenToggle.checked = false;
        outputInput.text = "";
        workspaceInput.text = "";
        cornerRadiusEnabled.checked = false;
        cornerRadiusSlider.value = 12;
        minWidthInput.text = "";
        maxWidthInput.text = "";
        minHeightInput.text = "";
        maxHeightInput.text = "";
        tileToggle.checked = false;
        noFocusToggle.checked = false;
        noBorderToggle.checked = false;
        noShadowToggle.checked = false;
        noDimToggle.checked = false;
        noBlurToggle.checked = false;
        noAnimToggle.checked = false;
        noRoundingToggle.checked = false;
        pinToggle.checked = false;
        opaqueToggle.checked = false;
        sizeInput.text = "";
        moveInput.text = "";
        monitorInput.text = "";
        hyprWorkspaceInput.text = "";
    }

    function show(window) {
        editingRule = null;
        targetWindow = window || null;
        resetForm();
        if (targetWindow) {
            nameInput.text = targetWindow.appId || "";
            if (targetWindow.appId)
                appIdInput.text = "^" + targetWindow.appId + "$";
            else
                appIdInput.text = "";
        }
        visible = true;
        Qt.callLater(() => nameInput.forceActiveFocus());
    }

    function triFromBool(v) {
        if (v === true)
            return 1;
        if (v === false)
            return 2;
        return 0;
    }

    function populateForm(rule) {
        nameInput.text = rule.name || "";
        const matchList = (rule.matches && rule.matches.length > 0) ? rule.matches : [rule.matchCriteria || {}];
        const match = matchList[0] || {};
        appIdInput.text = match.appId || "";
        titleInput.text = match.title || "";

        condFloating.triState = triFromBool(match.isFloating);
        condXwayland.triState = triFromBool(match.xwayland);
        condFullscreen.triState = triFromBool(match.fullscreen);
        condPinned.triState = triFromBool(match.pinned);
        condInitialised.triState = triFromBool(match.initialised);

        const actions = rule.actions || {};
        const hasOpacity = actions.opacity !== undefined && actions.opacity !== null;
        opacityEnabled.checked = hasOpacity;
        opacitySlider.value = hasOpacity ? Math.round(actions.opacity * 100) : 100;

        floatingToggle.checked = actions.openFloating || false;
        maximizedToggle.checked = actions.openMaximized || false;
        fullscreenToggle.checked = actions.openFullscreen || false;

        outputInput.text = actions.openOnOutput || "";
        workspaceInput.text = actions.openOnWorkspace || "";

        const hasCornerRadius = actions.cornerRadius !== undefined && actions.cornerRadius !== null;
        cornerRadiusEnabled.checked = hasCornerRadius;
        cornerRadiusSlider.value = hasCornerRadius ? actions.cornerRadius : 12;

        minWidthInput.text = actions.minWidth !== undefined ? String(actions.minWidth) : "";
        maxWidthInput.text = actions.maxWidth !== undefined ? String(actions.maxWidth) : "";
        minHeightInput.text = actions.minHeight !== undefined ? String(actions.minHeight) : "";
        maxHeightInput.text = actions.maxHeight !== undefined ? String(actions.maxHeight) : "";

        tileToggle.checked = actions.tile || false;
        noFocusToggle.checked = actions.nofocus || false;
        noBorderToggle.checked = actions.noborder || false;
        noShadowToggle.checked = actions.noshadow || false;
        noDimToggle.checked = actions.nodim || false;
        noBlurToggle.checked = actions.noblur || false;
        noAnimToggle.checked = actions.noanim || false;
        noRoundingToggle.checked = actions.norounding || false;
        pinToggle.checked = actions.pin || false;
        opaqueToggle.checked = actions.opaque || false;
        sizeInput.text = actions.size || "";
        moveInput.text = actions.move || "";
        monitorInput.text = actions.monitor || "";
        hyprWorkspaceInput.text = actions.workspace || "";

    }

    function showEdit(rule) {
        if (!rule) {
            show();
            return;
        }
        editingRule = rule;
        resetForm();
        populateForm(rule);
        visible = true;
        Qt.callLater(() => nameInput.forceActiveFocus());
    }

    function showCopy(rule) {
        if (!rule) {
            show();
            return;
        }
        editingRule = null;
        resetForm();
        populateForm(rule);
        visible = true;
        Qt.callLater(() => nameInput.forceActiveFocus());
    }

    function hide() {
        visible = false;
        editingRule = null;
        targetWindow = null;
    }

    function applyCond(obj, key, triState) {
        if (triState === 1)
            obj[key] = true;
        else if (triState === 2)
            obj[key] = false;
    }

    function submitAndClose() {
        const matchCriteria = {};
        if (appIdInput.text.trim())
            matchCriteria.appId = appIdInput.text.trim();
        if (titleInput.text.trim())
            matchCriteria.title = titleInput.text.trim();

        applyCond(matchCriteria, "isFloating", condFloating.triState);
        applyCond(matchCriteria, "xwayland", condXwayland.triState);
        applyCond(matchCriteria, "fullscreen", condFullscreen.triState);
        applyCond(matchCriteria, "pinned", condPinned.triState);
        applyCond(matchCriteria, "initialised", condInitialised.triState);

        const matches = [];
        if (Object.keys(matchCriteria).length > 0)
            matches.push(matchCriteria);

        const actions = {};

        if (opacityEnabled.checked)
            actions.opacity = opacitySlider.value / 100;
        if (floatingToggle.checked)
            actions.openFloating = true;
        if (maximizedToggle.checked)
            actions.openMaximized = true;
        if (fullscreenToggle.checked)
            actions.openFullscreen = true;
        if (outputInput.text.trim())
            actions.openOnOutput = outputInput.text.trim();
        if (workspaceInput.text.trim())
            actions.openOnWorkspace = workspaceInput.text.trim();
        if (cornerRadiusEnabled.checked)
            actions.cornerRadius = cornerRadiusSlider.value;

        const minW = parseInt(minWidthInput.text);
        const maxW = parseInt(maxWidthInput.text);
        const minH = parseInt(minHeightInput.text);
        const maxH = parseInt(maxHeightInput.text);
        if (!isNaN(minW))
            actions.minWidth = minW;
        if (!isNaN(maxW))
            actions.maxWidth = maxW;
        if (!isNaN(minH))
            actions.minHeight = minH;
        if (!isNaN(maxH))
            actions.maxHeight = maxH;

        if (tileToggle.checked)
            actions.tile = true;
        if (noFocusToggle.checked)
            actions.nofocus = true;
        if (noBorderToggle.checked)
            actions.noborder = true;
        if (noShadowToggle.checked)
            actions.noshadow = true;
        if (noDimToggle.checked)
            actions.nodim = true;
        if (noBlurToggle.checked)
            actions.noblur = true;
        if (noAnimToggle.checked)
            actions.noanim = true;
        if (noRoundingToggle.checked)
            actions.norounding = true;
        if (pinToggle.checked)
            actions.pin = true;
        if (opaqueToggle.checked)
            actions.opaque = true;
        if (sizeInput.text.trim())
            actions.size = sizeInput.text.trim();
        if (moveInput.text.trim())
            actions.move = moveInput.text.trim();
        if (monitorInput.text.trim())
            actions.monitor = monitorInput.text.trim();
        if (hyprWorkspaceInput.text.trim())
            actions.workspace = hyprWorkspaceInput.text.trim();

        const name = nameInput.text.trim() || matchCriteria.appId || I18n.tr("Rule");
        const compositor = "hyprland";

        const ruleData = {
            name: name,
            matchCriteria: matchCriteria,
            actions: actions,
            enabled: true
        };

        submitting = true;

        if (isEditMode) {
            const ruleJson = JSON.stringify(ruleData);
            Proc.runCommand("update-windowrule", ["hgs", "config", "windowrules", "update", compositor, editingRule.id, ruleJson], (output, exitCode) => {
                root.submitting = false;
                if (exitCode !== 0)
                    return;
                root.ruleSubmitted();
                root.hide();
            });
        } else {
            const ruleJson = JSON.stringify(ruleData);
            Proc.runCommand("add-windowrule", ["hgs", "config", "windowrules", "add", compositor, ruleJson], (output, exitCode) => {
                root.submitting = false;
                if (exitCode !== 0)
                    return;
                root.ruleSubmitted();
                root.hide();
            });
        }
    }

    onVisibleChanged: {
        if (!visible) {
            editingRule = null;
            targetWindow = null;
        }
    }

    component SectionHeader: StyledText {
        property string title
        text: title
        font.pixelSize: Theme.fontSizeMedium
        font.weight: Font.Medium
        color: Theme.primary
        topPadding: Theme.spacingM
        bottomPadding: Theme.spacingXS
        width: parent.width
        horizontalAlignment: Text.AlignLeft
    }

    component CheckboxRow: Row {
        property alias checked: checkbox.checked
        property alias label: labelText.text
        property bool indeterminate: false
        spacing: Theme.spacingS
        height: 24

        Rectangle {
            id: checkbox
            property bool checked: false
            width: 20
            height: 20
            radius: 4
            color: parent.indeterminate ? Theme.surfaceVariant : (checked ? Theme.primary : "transparent")
            border.color: parent.indeterminate ? Theme.outlineButton : (checked ? Theme.primary : Theme.outlineButton)
            border.width: 2
            anchors.verticalCenter: parent.verticalCenter

            HGSIcon {
                anchors.centerIn: parent
                name: parent.parent.indeterminate ? "remove" : "check"
                size: 12
                color: parent.parent.indeterminate ? Theme.surfaceVariantText : Theme.background
                visible: parent.checked || parent.parent.indeterminate
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (parent.parent.indeterminate) {
                        parent.parent.indeterminate = false;
                        parent.checked = true;
                    } else {
                        parent.checked = !parent.checked;
                    }
                }
            }
        }

        StyledText {
            id: labelText
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.surfaceText
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    component InputField: Rectangle {
        id: inputFieldRect
        default property alias contentData: inputFieldRect.data
        property bool hasFocus: false
        width: parent.width
        height: root.inputFieldHeight
        radius: Theme.cornerRadius
        color: Theme.surfaceHover
        border.color: hasFocus ? Theme.primary : Theme.outlineStrong
        border.width: hasFocus ? 2 : 1
    }

    // Tri-state toggle: 0 = unset (Inherit/Any), 1 = true, 2 = false
    component MatchCond: Rectangle {
        id: mc
        property string label: ""
        property int triState: 0
        property string unsetLabel: I18n.tr("Default")
        property bool readOnly: false
        readonly property var stateText: [mc.unsetLabel, "true", "false"]
        readonly property var stateColor: [Theme.surfaceVariantText, Theme.primary, Theme.error]

        width: condRow.implicitWidth + Theme.spacingM * 2
        height: root.inputFieldHeight
        radius: Theme.cornerRadius
        color: Theme.surfaceHover
        border.width: 1
        border.color: mc.triState === 0 ? Theme.outlineStrong : mc.stateColor[mc.triState]
        opacity: mc.readOnly ? 0.4 : 1

        Row {
            id: condRow
            anchors.centerIn: parent
            spacing: Theme.spacingXS

            StyledText {
                text: mc.label
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.surfaceText
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle {
                width: stateBadge.implicitWidth + Theme.spacingS * 2
                height: 18
                radius: 9
                color: Theme.withAlpha(mc.stateColor[mc.triState], 0.15)
                anchors.verticalCenter: parent.verticalCenter

                StyledText {
                    id: stateBadge
                    anchors.centerIn: parent
                    text: mc.stateText[mc.triState]
                    font.pixelSize: Theme.fontSizeSmall - 2
                    color: mc.stateColor[mc.triState]
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            enabled: root.visible && !mc.readOnly
            onClicked: mc.triState = (mc.triState + 1) % 3
        }
    }

    FocusScope {
        anchors.fill: parent
        focus: true

        LayoutMirroring.enabled: I18n.isRtl
        LayoutMirroring.childrenInherit: true

        Keys.onEscapePressed: event => {
            hide();
            event.accepted = true;
        }

        Item {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Theme.spacingL
            height: Math.max(headerCol.height, closeBtn.height)

            MouseArea {
                anchors.left: parent.left
                anchors.right: closeBtn.left
                anchors.rightMargin: Theme.spacingM
                height: headerCol.height
                onPressed: windowControls.tryStartMove()

                Column {
                    id: headerCol
                    width: parent.width
                    spacing: Theme.spacingXS

                    StyledText {
                        text: root.isEditMode ? I18n.tr("Edit Window Rule") : I18n.tr("New Window Rule")
                        font.pixelSize: Theme.fontSizeLarge
                        color: Theme.surfaceText
                        font.weight: Font.Medium
                        width: parent.width
                        horizontalAlignment: Text.AlignLeft
                    }

                    StyledText {
                        text: I18n.tr("Configure match criteria and actions")
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.surfaceTextMedium
                        width: parent.width
                        horizontalAlignment: Text.AlignLeft
                    }
                }
            }

            HGSActionButton {
                id: closeBtn
                anchors.right: parent.right
                iconName: "close"
                iconSize: Theme.iconSize - 4
                iconColor: Theme.surfaceText
                onClicked: hide()
            }
        }

        HGSFlickable {
            id: flickable
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            anchors.margins: Theme.spacingL
            anchors.topMargin: Theme.spacingM
            contentWidth: width
            contentHeight: contentCol.implicitHeight
            clip: true

            Column {
                id: contentCol
                width: flickable.width - Theme.spacingM
                spacing: Theme.spacingXS

                InputField {
                    hasFocus: nameInput.activeFocus
                    HGSTextField {
                        id: nameInput
                        anchors.fill: parent
                        font.pixelSize: Theme.fontSizeSmall
                        textColor: Theme.surfaceText
                        placeholderText: I18n.tr("Rule Name")
                        backgroundColor: "transparent"
                        enabled: root.visible
                    }
                }

                SectionHeader {
                    title: I18n.tr("Match Criteria")
                }

                InputField {
                    hasFocus: appIdInput.activeFocus
                    HGSTextField {
                        id: appIdInput
                        anchors.fill: parent
                        font.pixelSize: Theme.fontSizeSmall
                        textColor: Theme.surfaceText
                        placeholderText: I18n.tr("Class regex (e.g. ^firefox$)")
                        backgroundColor: "transparent"
                        enabled: root.visible
                    }
                }

                Row {
                    width: parent.width
                    spacing: Theme.spacingS

                    InputField {
                        width: addTitleBtn.visible ? parent.width - addTitleBtn.width - Theme.spacingS : parent.width
                        hasFocus: titleInput.activeFocus
                        HGSTextField {
                            id: titleInput
                            anchors.fill: parent
                            font.pixelSize: Theme.fontSizeSmall
                            textColor: Theme.surfaceText
                            placeholderText: I18n.tr("Title regex (optional)")
                            backgroundColor: "transparent"
                            enabled: root.visible
                        }
                    }

                    HGSActionButton {
                        id: addTitleBtn
                        width: root.inputFieldHeight
                        height: root.inputFieldHeight
                        circular: false
                        iconName: "add"
                        iconSize: 16
                        iconColor: Theme.surfaceVariantText
                        visible: !root.isEditMode && !!root.targetWindow?.title
                        tooltipText: I18n.tr("Add Title")
                        tooltipSide: "left"
                        onClicked: {
                            if (!root.targetWindow?.title)
                                return;
                            titleInput.text = "^" + root.targetWindow.title + "$";
                        }
                    }
                }


                SectionHeader {
                    title: I18n.tr("Match Conditions")
                }

                StyledText {
                    width: parent.width
                    text: I18n.tr("Optional state-based conditions applied to the first match.")
                    font.pixelSize: Theme.fontSizeSmall - 1
                    color: Theme.surfaceVariantText
                    wrapMode: Text.WordWrap
                }

                Flow {
                    width: parent.width
                    spacing: Theme.spacingS

                    MatchCond {
                        id: condFloating
                        label: I18n.tr("Floating")
                    }
                    MatchCond {
                        id: condXwayland
                        label: I18n.tr("XWayland")
                    }
                    MatchCond {
                        id: condFullscreen
                        label: I18n.tr("Fullscreen")
                    }
                    MatchCond {
                        id: condPinned
                        label: I18n.tr("Pinned")
                    }
                    MatchCond {
                        id: condInitialised
                        label: I18n.tr("Initialised")
                    }
                }

                SectionHeader {
                    title: I18n.tr("Window Opening")
                }

                Flow {
                    width: parent.width
                    spacing: Theme.spacingL

                    CheckboxRow {
                        id: floatingToggle
                        label: I18n.tr("Float")
                    }
                    CheckboxRow {
                        id: maximizedToggle
                        label: I18n.tr("Maximize")
                    }
                    CheckboxRow {
                        id: fullscreenToggle
                        label: I18n.tr("Fullscreen")
                    }
                }

                Row {
                    width: parent.width
                    spacing: Theme.spacingM

                    Column {
                        width: (parent.width - Theme.spacingM) / 2
                        spacing: Theme.spacingXS

                        StyledText {
                            text: I18n.tr("Output")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            width: parent.width
                            horizontalAlignment: Text.AlignLeft
                        }

                        InputField {
                            width: parent.width
                            hasFocus: outputInput.activeFocus
                            HGSTextField {
                                id: outputInput
                                anchors.fill: parent
                                font.pixelSize: Theme.fontSizeSmall
                                textColor: Theme.surfaceText
                                placeholderText: "HDMI-A-1"
                                backgroundColor: "transparent"
                                enabled: root.visible
                            }
                        }
                    }

                    Column {
                        width: (parent.width - Theme.spacingM) / 2
                        spacing: Theme.spacingXS

                        StyledText {
                            text: I18n.tr("Workspace")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            width: parent.width
                            horizontalAlignment: Text.AlignLeft
                        }

                        InputField {
                            width: parent.width
                            hasFocus: workspaceInput.activeFocus
                            HGSTextField {
                                id: workspaceInput
                                anchors.fill: parent
                                font.pixelSize: Theme.fontSizeSmall
                                textColor: Theme.surfaceText
                                placeholderText: "chat"
                                backgroundColor: "transparent"
                                enabled: root.visible
                            }
                        }
                    }
                }


                SectionHeader {
                    title: I18n.tr("Dynamic Properties")
                }

                Row {
                    width: parent.width
                    spacing: Theme.spacingM

                    CheckboxRow {
                        id: opacityEnabled
                        label: I18n.tr("Opacity")
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    HGSSlider {
                        id: opacitySlider
                        wheelEnabled: false
                        width: parent.width - 100
                        minimum: 10
                        maximum: 100
                        value: 100
                        enabled: opacityEnabled.checked
                        opacity: enabled ? 1 : 0.4
                    }
                }




                Row {
                    width: parent.width
                    spacing: Theme.spacingM

                    CheckboxRow {
                        id: cornerRadiusEnabled
                        label: I18n.tr("Corner Radius")
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    HGSSlider {
                        id: cornerRadiusSlider
                        wheelEnabled: false
                        width: parent.width - 130
                        minimum: 0
                        maximum: 24
                        value: 12
                        enabled: cornerRadiusEnabled.checked
                        opacity: enabled ? 1 : 0.4
                    }
                }









                SectionHeader {
                    title: I18n.tr("Size Constraints")
                }

                Row {
                    width: parent.width
                    spacing: Theme.spacingM

                    Column {
                        width: (parent.width - Theme.spacingM * 3) / 4
                        spacing: Theme.spacingXS

                        StyledText {
                            text: I18n.tr("Min W")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            width: parent.width
                            horizontalAlignment: Text.AlignLeft
                        }

                        InputField {
                            width: parent.width
                            hasFocus: minWidthInput.activeFocus
                            HGSTextField {
                                id: minWidthInput
                                anchors.fill: parent
                                font.pixelSize: Theme.fontSizeSmall
                                textColor: Theme.surfaceText
                                placeholderText: "px"
                                backgroundColor: "transparent"
                                enabled: root.visible
                            }
                        }
                    }

                    Column {
                        width: (parent.width - Theme.spacingM * 3) / 4
                        spacing: Theme.spacingXS

                        StyledText {
                            text: I18n.tr("Max W")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            width: parent.width
                            horizontalAlignment: Text.AlignLeft
                        }

                        InputField {
                            width: parent.width
                            hasFocus: maxWidthInput.activeFocus
                            HGSTextField {
                                id: maxWidthInput
                                anchors.fill: parent
                                font.pixelSize: Theme.fontSizeSmall
                                textColor: Theme.surfaceText
                                placeholderText: "px"
                                backgroundColor: "transparent"
                                enabled: root.visible
                            }
                        }
                    }

                    Column {
                        width: (parent.width - Theme.spacingM * 3) / 4
                        spacing: Theme.spacingXS

                        StyledText {
                            text: I18n.tr("Min H")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            width: parent.width
                            horizontalAlignment: Text.AlignLeft
                        }

                        InputField {
                            width: parent.width
                            hasFocus: minHeightInput.activeFocus
                            HGSTextField {
                                id: minHeightInput
                                anchors.fill: parent
                                font.pixelSize: Theme.fontSizeSmall
                                textColor: Theme.surfaceText
                                placeholderText: "px"
                                backgroundColor: "transparent"
                                enabled: root.visible
                            }
                        }
                    }

                    Column {
                        width: (parent.width - Theme.spacingM * 3) / 4
                        spacing: Theme.spacingXS

                        StyledText {
                            text: I18n.tr("Max H")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            width: parent.width
                            horizontalAlignment: Text.AlignLeft
                        }

                        InputField {
                            width: parent.width
                            hasFocus: maxHeightInput.activeFocus
                            HGSTextField {
                                id: maxHeightInput
                                anchors.fill: parent
                                font.pixelSize: Theme.fontSizeSmall
                                textColor: Theme.surfaceText
                                placeholderText: "px"
                                backgroundColor: "transparent"
                                enabled: root.visible
                            }
                        }
                    }
                }

                SectionHeader {
                    title: I18n.tr("Hyprland Options")
                }

                Flow {
                    width: parent.width
                    spacing: Theme.spacingL

                    CheckboxRow {
                        id: tileToggle
                        label: I18n.tr("Tile")
                    }
                    CheckboxRow {
                        id: noFocusToggle
                        label: I18n.tr("No Focus")
                    }
                    CheckboxRow {
                        id: noBorderToggle
                        label: I18n.tr("No Border")
                    }
                    CheckboxRow {
                        id: noShadowToggle
                        label: I18n.tr("No Shadow")
                    }
                    CheckboxRow {
                        id: noDimToggle
                        label: I18n.tr("No Dim")
                    }
                    CheckboxRow {
                        id: noBlurToggle
                        label: I18n.tr("No Blur")
                    }
                    CheckboxRow {
                        id: noAnimToggle
                        label: I18n.tr("No Anim")
                    }
                    CheckboxRow {
                        id: noRoundingToggle
                        label: I18n.tr("No Rounding")
                    }
                    CheckboxRow {
                        id: pinToggle
                        label: I18n.tr("Pin")
                    }
                    CheckboxRow {
                        id: opaqueToggle
                        label: I18n.tr("Opaque")
                    }
                }

                Row {
                    width: parent.width
                    spacing: Theme.spacingM

                    Column {
                        width: (parent.width - Theme.spacingM) / 2
                        spacing: Theme.spacingXS

                        StyledText {
                            text: I18n.tr("Size")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            width: parent.width
                            horizontalAlignment: Text.AlignLeft
                        }

                        InputField {
                            width: parent.width
                            hasFocus: sizeInput.activeFocus
                            HGSTextField {
                                id: sizeInput
                                anchors.fill: parent
                                font.pixelSize: Theme.fontSizeSmall
                                textColor: Theme.surfaceText
                                placeholderText: "800 600"
                                backgroundColor: "transparent"
                                enabled: root.visible
                            }
                        }
                    }

                    Column {
                        width: (parent.width - Theme.spacingM) / 2
                        spacing: Theme.spacingXS

                        StyledText {
                            text: I18n.tr("Move")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            width: parent.width
                            horizontalAlignment: Text.AlignLeft
                        }

                        InputField {
                            width: parent.width
                            hasFocus: moveInput.activeFocus
                            HGSTextField {
                                id: moveInput
                                anchors.fill: parent
                                font.pixelSize: Theme.fontSizeSmall
                                textColor: Theme.surfaceText
                                placeholderText: "100 100"
                                backgroundColor: "transparent"
                                enabled: root.visible
                            }
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: Theme.spacingM

                    Column {
                        width: (parent.width - Theme.spacingM) / 2
                        spacing: Theme.spacingXS

                        StyledText {
                            text: I18n.tr("Monitor")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            width: parent.width
                            horizontalAlignment: Text.AlignLeft
                        }

                        InputField {
                            width: parent.width
                            hasFocus: monitorInput.activeFocus
                            HGSTextField {
                                id: monitorInput
                                anchors.fill: parent
                                font.pixelSize: Theme.fontSizeSmall
                                textColor: Theme.surfaceText
                                placeholderText: "DP-1"
                                backgroundColor: "transparent"
                                enabled: root.visible
                            }
                        }
                    }

                    Column {
                        width: (parent.width - Theme.spacingM) / 2
                        spacing: Theme.spacingXS

                        StyledText {
                            text: I18n.tr("Workspace")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            width: parent.width
                            horizontalAlignment: Text.AlignLeft
                        }

                        InputField {
                            width: parent.width
                            hasFocus: hyprWorkspaceInput.activeFocus
                            HGSTextField {
                                id: hyprWorkspaceInput
                                anchors.fill: parent
                                font.pixelSize: Theme.fontSizeSmall
                                textColor: Theme.surfaceText
                                placeholderText: "1"
                                backgroundColor: "transparent"
                                enabled: root.visible
                            }
                        }
                    }
                }

                Item {
                    width: 1
                    height: Theme.spacingM
                }
            }
        }

        Item {
            id: footer
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Theme.spacingL
            height: 44

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.spacingM

                Rectangle {
                    width: Math.max(70, cancelText.contentWidth + Theme.spacingM * 2)
                    height: 36
                    radius: Theme.cornerRadius
                    color: cancelArea.containsMouse ? Theme.surfaceTextHover : "transparent"
                    border.color: Theme.surfaceVariantAlpha
                    border.width: 1

                    StyledText {
                        id: cancelText
                        anchors.centerIn: parent
                        text: I18n.tr("Cancel")
                        font.pixelSize: Theme.fontSizeMedium
                        color: Theme.surfaceText
                        font.weight: Font.Medium
                    }

                    MouseArea {
                        id: cancelArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: hide()
                    }
                }

                Rectangle {
                    width: Math.max(80, createText.contentWidth + Theme.spacingM * 2)
                    height: 36
                    radius: Theme.cornerRadius
                    color: root.submitting ? Theme.surfaceVariant : (createArea.containsMouse ? Qt.darker(Theme.primary, 1.1) : Theme.primary)

                    StyledText {
                        id: createText
                        anchors.centerIn: parent
                        text: root.submitting ? I18n.tr("Saving...") : (root.isEditMode ? I18n.tr("Update") : I18n.tr("Create"))
                        font.pixelSize: Theme.fontSizeMedium
                        color: root.submitting ? Theme.surfaceVariantText : Theme.background
                        font.weight: Font.Medium
                    }

                    MouseArea {
                        id: createArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: root.submitting ? Qt.ArrowCursor : Qt.PointingHandCursor
                        enabled: !root.submitting
                        onClicked: submitAndClose()
                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.shortDuration
                            easing.type: Theme.standardEasing
                        }
                    }
                }
            }
        }
    }

    FloatingWindowControls {
        id: windowControls
        targetWindow: root
    }
}
