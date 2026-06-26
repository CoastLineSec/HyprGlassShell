import QtQuick
import Quickshell
import qs.Common
import qs.Services
import qs.Widgets
import qs.Modals.FileBrowser

// Fluid Glass Labs — an in-shell port of the WebGL tuning lab. Two-column:
// the preview + backdrop picker + reset stay pinned on the left while the full
// parameter list scrolls on the right, so the glass is always visible while you
// tune. Material values drive the live compositor; frost/tint follow the main
// Fluid Glass card.
Item {
    id: labs

    property var parentModal: null

    readonly property string _home: Quickshell.env("HOME") || ""
    readonly property string _presetsDir: _home + "/.local/share/HyprGlassShell/fluid-glass-labs/presets/"
    readonly property string _customDir: _home + "/.local/share/HyprGlassShell/fluid-glass-labs/custom/"
    readonly property bool tracking: SettingsData.fluidGlassDynamicLight ?? true

    readonly property var presetBackdrops: [
        ({ "path": _presetsDir + "plasma.png", "custom": false }),
        ({ "path": _presetsDir + "blue-panels.png", "custom": false }),
        ({ "path": _presetsDir + "rainbow-arch.jpg", "custom": false }),
        ({ "path": _presetsDir + "vaxrys-gf.png", "custom": false })
    ]
    readonly property var customBackdrops: SettingsData.fluidGlassLabsBackdrops || []
    readonly property string selectedPath: SettingsData.fluidGlassLabsBackdrop || presetBackdrops[0].path
    readonly property var backdropModel: presetBackdrops.concat(customBackdrops.map(p => ({ "path": p, "custom": true })))

    // Local preview state (-1 = use the size-derived default).
    property real userRectW: -1
    property real userRectH: -1
    property real userRectR: -1
    readonly property bool frosted: SettingsData.fluidGlassFrosted ?? true
    readonly property bool stained: SettingsData.fluidGlassStained ?? false
    // Default rect ≈ the 200px design reference so the preview reads calibrated.
    readonly property real rectH: userRectH >= 0 ? userRectH : Math.max(24, Math.min(previewArea.height - 20, 188))
    readonly property real rectW: userRectW >= 0 ? userRectW : Math.max(40, Math.min(previewArea.width - 20, rectH * 2.3))
    readonly property real rectR: userRectR >= 0 ? userRectR : 44

    // The preview is the REAL compositor glass: a window-anchored element placed over the Shape
    // rect (centered in the preview pane). Window movement is tracked server-side by the plugin,
    // so we only recompute the window-relative offset on layout/shape change.
    function updateLabsPreview() {
        if (typeof HyprGlassService === "undefined")
            return;
        if (!visible || !previewArea || previewArea.width <= 0) {
            HyprGlassService.labsPreview = null;
            return;
        }
        const p = previewArea.mapToItem(null, 0, 0);
        if (!p)
            return;
        HyprGlassService.labsPreview = {
            "anchorWindow": "title:HyprGlass Settings",
            "offsetX": Math.round(p.x + (previewArea.width - rectW) / 2),
            "offsetY": Math.round(p.y + (previewArea.height - rectH) / 2),
            "w": Math.round(rectW),
            "h": Math.round(rectH),
            "radius": Math.round(rectR)
        };
    }
    onVisibleChanged: Qt.callLater(updateLabsPreview)
    onRectWChanged: Qt.callLater(updateLabsPreview)
    onRectHChanged: Qt.callLater(updateLabsPreview)
    onRectRChanged: Qt.callLater(updateLabsPreview)
    Component.onCompleted: Qt.callLater(updateLabsPreview)
    Component.onDestruction: {
        if (typeof HyprGlassService !== "undefined")
            HyprGlassService.labsPreview = null;
    }

    function fileUrl(p) {
        if (!p)
            return "";
        if (p.toString().startsWith("file://"))
            return p;
        return "file://" + p.toString().split('/').map(s => encodeURIComponent(s)).join('/');
    }
    function selectBackdrop(p) {
        SettingsData.set("fluidGlassLabsBackdrop", p);
    }
    function addCustomBackdrop(rawPath) {
        if (!rawPath)
            return;
        const src = rawPath.toString().replace(/^file:\/\//, "");
        const base = src.split('/').pop();
        const dest = _customDir + Date.now() + "_" + base;
        Proc.runCommand("fgl-add", ["sh", "-c", "mkdir -p \"" + _customDir + "\" && cp \"" + src + "\" \"" + dest + "\""], (out, code) => {
            if (code !== 0)
                return;
            const list = (SettingsData.fluidGlassLabsBackdrops || []).slice();
            list.push(dest);
            SettingsData.set("fluidGlassLabsBackdrops", list);
            labs.selectBackdrop(dest);
        });
    }
    function removeCustomBackdrop(p) {
        Proc.runCommand("fgl-rm", ["rm", "-f", p], (out, code) => {});
        const list = (SettingsData.fluidGlassLabsBackdrops || []).filter(x => x !== p);
        SettingsData.set("fluidGlassLabsBackdrops", list);
        if (SettingsData.fluidGlassLabsBackdrop === p)
            labs.selectBackdrop("");
    }
    function resetDefaults() {
        SettingsData.set("fluidGlassRefraction", 45);
        SettingsData.set("fluidGlassRimBand", 40);
        SettingsData.set("fluidGlassBevel", 46);
        SettingsData.set("fluidGlassRimWidth", 3);
        SettingsData.set("fluidGlassHighlight", 0.10);
        SettingsData.set("fluidGlassShadow", 0.10);
        SettingsData.set("fluidGlassLightAngle", 90);
        SettingsData.set("fluidGlassSpecular", 0.21);
        SettingsData.set("fluidGlassRimWrap", 0.45);
        SettingsData.set("fluidGlassLevel", 0.5);          // Frosted Glass: Medium
        SettingsData.set("fluidGlassStained", true);        // Stained Glass: System Appearance
        SettingsData.set("fluidGlassTintSource", "system");
        SettingsData.set("fluidGlassFrosted", true);
        SettingsData.set("fluidGlassBlurCustom", 0.5);
        SettingsData.set("fluidGlassTintCustom", 0.16);
        labs.userRectW = -1;
        labs.userRectH = -1;
        labs.userRectR = -1;
    }
    function openPicker() {
        pickerLoader.active = true;
        Qt.callLater(() => {
            if (pickerLoader.item)
                pickerLoader.item.open();
        });
    }

    // ---- reusable slider row ----
    component LabSlider: Column {
        id: ls
        property string label: ""
        property int from: 0
        property int to: 100
        property int val: 0
        property string suffix: ""
        property bool rowEnabled: true
        signal moved(int v)

        width: parent ? parent.width : 0
        spacing: 2
        opacity: rowEnabled ? 1 : 0.35
        // HGSSlider writes its own `value` on drag, which breaks an external
        // binding — re-assert it whenever val changes so Reset to Default (and any
        // other programmatic change) actually moves the thumb.
        onValChanged: sld.value = ls.val

        Row {
            width: parent.width
            StyledText {
                width: parent.width - valLabel.width
                text: ls.label
                font.pixelSize: Theme.fontSizeMedium
                color: Theme.surfaceText
                elide: Text.ElideRight
            }
            StyledText {
                id: valLabel
                text: ls.val + ls.suffix
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.surfaceVariantText
            }
        }
        HGSSlider {
            id: sld
            width: parent.width
            height: 30
            minimum: ls.from
            maximum: ls.to
            value: ls.val
            enabled: ls.rowEnabled
            showValue: false
            wheelEnabled: false
            thumbOutlineColor: Theme.surfaceContainerHigh
            onSliderValueChanged: v => ls.moved(v)
            onSliderDragFinished: v => ls.moved(v)
        }
    }

    component SectionLabel: StyledText {
        font.pixelSize: Theme.fontSizeSmall
        font.weight: Font.Bold
        color: Theme.surfaceVariantText
        topPadding: Theme.spacingS
    }

    // ---- backdrop thumbnail (props only — no outer-id refs) ----
    component BackdropThumb: Rectangle {
        id: thumb
        property string imgUrl: ""
        property bool selected: false
        property bool removable: false
        signal picked
        signal removed

        width: 78
        height: 48
        radius: 8
        clip: true
        color: Theme.surfaceContainer
        border.width: selected ? 2 : 1
        border.color: selected ? Theme.primary : Theme.withAlpha(Theme.outline, 0.3)

        Image {
            anchors.fill: parent
            anchors.margins: 2
            source: thumb.imgUrl
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(92, 56)
            cache: true
            asynchronous: true
            smooth: true
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: thumb.picked()
        }
        Rectangle {
            visible: thumb.removable
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 2
            width: 16
            height: 16
            radius: 8
            color: Qt.rgba(0, 0, 0, 0.55)
            HGSIcon {
                anchors.centerIn: parent
                name: "close"
                size: 11
                color: "#ffffff"
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: thumb.removed()
            }
        }
    }

    // ===== Card =====
    Rectangle {
        anchors.fill: parent
        radius: Theme.cornerRadius
        color: Theme.surfaceContainerHigh

        Row {
            id: headerRow
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Theme.spacingL
            spacing: Theme.spacingS

            HGSIcon {
                name: "experiment"
                size: Theme.iconSize
                color: Theme.surfaceText
                anchors.verticalCenter: parent.verticalCenter
            }
            StyledText {
                text: I18n.tr("Fluid Glass Labs")
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.Bold
                color: Theme.surfaceText
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Two columns: pinned preview (left) + scrolling parameters (right).
        Row {
            id: cols
            anchors.top: headerRow.bottom
            anchors.topMargin: Theme.spacingM
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.spacingL
            anchors.rightMargin: Theme.spacingL
            anchors.bottomMargin: Theme.spacingL
            spacing: Theme.spacingL

            // ----- LEFT: pinned -----
            Column {
                id: leftCol
                width: Math.round((cols.width - cols.spacing) * 0.46)
                spacing: Theme.spacingM

                Rectangle {
                    id: previewArea
                    width: parent.width
                    height: Math.round(width * 0.8)
                    radius: Theme.cornerRadius
                    clip: true
                    color: Theme.surfaceContainer
                    border.width: 1
                    border.color: Theme.withAlpha(Theme.outline, 0.18)

                    onXChanged: Qt.callLater(labs.updateLabsPreview)
                    onYChanged: Qt.callLater(labs.updateLabsPreview)
                    onWidthChanged: Qt.callLater(labs.updateLabsPreview)
                    onHeightChanged: Qt.callLater(labs.updateLabsPreview)

                    // The backdrop the compositor glass refracts — drawn opaquely so it's in the
                    // framebuffer the plugin captures at the Shape rect.
                    Image {
                        id: bdImg
                        anchors.fill: parent
                        source: labs.fileUrl(labs.selectedPath)
                        fillMode: Image.PreserveAspectCrop
                        sourceSize: Qt.size(Math.max(2, Math.round(previewArea.width)), Math.max(2, Math.round(previewArea.height)))
                        cache: true
                        smooth: true
                        asynchronous: true
                    }

                    // Guide showing the glass-surface bounds; the real compositor glass renders here.
                    Rectangle {
                        width: labs.rectW
                        height: labs.rectH
                        radius: labs.rectR
                        anchors.centerIn: parent
                        color: "transparent"
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.35)
                    }

                    StyledText {
                        anchors.centerIn: parent
                        visible: bdImg.status !== Image.Ready
                        text: bdImg.status === Image.Loading ? I18n.tr("Loading…") : I18n.tr("No backdrop")
                        color: Theme.surfaceVariantText
                        font.pixelSize: Theme.fontSizeMedium
                    }

                    StyledText {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottomMargin: Theme.spacingS
                        visible: !(SettingsData.fluidGlassEnabled ?? false)
                        text: I18n.tr("Turn on Fluid Glass to preview")
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.surfaceVariantText
                    }
                }

                SectionLabel {
                    text: I18n.tr("BACKDROP")
                }
                Flow {
                    width: parent.width
                    spacing: Theme.spacingS

                    Repeater {
                        model: labs.backdropModel
                        delegate: BackdropThumb {
                            required property var modelData
                            imgUrl: labs.fileUrl(modelData.path)
                            selected: labs.selectedPath === modelData.path
                            removable: modelData.custom === true
                            onPicked: labs.selectBackdrop(modelData.path)
                            onRemoved: labs.removeCustomBackdrop(modelData.path)
                        }
                    }
                    Rectangle {
                        width: 78
                        height: 48
                        radius: 8
                        color: addMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceContainer
                        border.width: 1
                        border.color: Theme.withAlpha(Theme.outline, 0.3)
                        HGSIcon {
                            anchors.centerIn: parent
                            name: "add"
                            size: Theme.iconSize
                            color: Theme.surfaceVariantText
                        }
                        MouseArea {
                            id: addMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: labs.openPicker()
                        }
                    }
                }

                HGSButton {
                    text: I18n.tr("Reset to Default")
                    iconName: "restart_alt"
                    onClicked: labs.resetDefaults()
                }
            }

            // ----- RIGHT: scrolling parameter list -----
            HGSFlickable {
                width: cols.width - leftCol.width - cols.spacing
                height: cols.height
                clip: true
                contentHeight: rightCol.height + Theme.spacingL
                contentWidth: width

                Column {
                    id: rightCol
                    width: parent.width
                    spacing: Theme.spacingM

                    SectionLabel {
                        text: I18n.tr("SHAPE")
                        topPadding: 0
                    }
                    LabSlider {
                        label: I18n.tr("Width")
                        from: 80
                        to: Math.max(120, Math.round(previewArea.width))
                        val: Math.round(labs.rectW)
                        suffix: " px"
                        onMoved: v => labs.userRectW = v
                    }
                    LabSlider {
                        label: I18n.tr("Height")
                        from: 24
                        to: Math.max(60, Math.round(previewArea.height))
                        val: Math.round(labs.rectH)
                        suffix: " px"
                        onMoved: v => labs.userRectH = v
                    }
                    LabSlider {
                        label: I18n.tr("Corner radius")
                        from: 0
                        to: Math.round(Math.min(labs.rectW, labs.rectH) / 2)
                        val: Math.round(labs.rectR)
                        suffix: " px"
                        onMoved: v => labs.userRectR = v
                    }

                    SectionLabel {
                        text: I18n.tr("FROSTED GLASS")
                    }
                    LabSlider {
                        label: I18n.tr("Frost level")
                        from: 0
                        to: 100
                        val: Math.round((labs.frosted ? (SettingsData.fluidGlassLevel ?? 0.5) : (SettingsData.fluidGlassBlurCustom ?? 0.5)) * 100)
                        suffix: "%"
                        onMoved: v => {
                            if (labs.frosted)
                                SettingsData.set("fluidGlassTintCustom", SettingsData.fluidGlassLevel ?? 0.5);
                            SettingsData.set("fluidGlassBlurCustom", v / 100);
                            SettingsData.set("fluidGlassFrosted", false);
                        }
                    }

                    SectionLabel {
                        text: I18n.tr("EDGE LENSING")
                    }
                    LabSlider {
                        label: I18n.tr("Refraction")
                        from: 0
                        to: 90
                        val: Math.round(SettingsData.fluidGlassRefraction ?? 45)
                        onMoved: v => SettingsData.set("fluidGlassRefraction", v)
                    }
                    LabSlider {
                        label: I18n.tr("Rim band")
                        from: 4
                        to: 240
                        val: Math.round(SettingsData.fluidGlassRimBand ?? 40)
                        onMoved: v => SettingsData.set("fluidGlassRimBand", v)
                    }

                    SectionLabel {
                        text: I18n.tr("CONVEX BEVEL")
                    }
                    StyledText {
                        visible: labs.tracking
                        width: parent.width
                        text: I18n.tr("Mouse tracking enabled. Disable mouse tracking to adjust values.")
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.warning
                    }
                    LabSlider {
                        label: I18n.tr("Bevel band")
                        rowEnabled: !labs.tracking
                        from: 4
                        to: 240
                        val: Math.round(SettingsData.fluidGlassBevel ?? 46)
                        onMoved: v => SettingsData.set("fluidGlassBevel", v)
                    }
                    LabSlider {
                        label: I18n.tr("Highlight")
                        rowEnabled: !labs.tracking
                        from: 0
                        to: 100
                        val: Math.round((SettingsData.fluidGlassHighlight ?? 0.10) * 100)
                        suffix: "%"
                        onMoved: v => SettingsData.set("fluidGlassHighlight", v / 100)
                    }
                    LabSlider {
                        label: I18n.tr("Shadow")
                        rowEnabled: !labs.tracking
                        from: 0
                        to: 100
                        val: Math.round((SettingsData.fluidGlassShadow ?? 0.10) * 100)
                        suffix: "%"
                        onMoved: v => SettingsData.set("fluidGlassShadow", v / 100)
                    }
                    LabSlider {
                        label: I18n.tr("Light angle")
                        rowEnabled: !labs.tracking
                        from: 0
                        to: 360
                        val: Math.round(SettingsData.fluidGlassLightAngle ?? 90)
                        suffix: "°"
                        onMoved: v => SettingsData.set("fluidGlassLightAngle", v)
                    }

                    SectionLabel {
                        text: I18n.tr("SPECULAR RIM")
                    }
                    LabSlider {
                        label: I18n.tr("Strength")
                        from: 0
                        to: 100
                        val: Math.round((SettingsData.fluidGlassSpecular ?? 0.21) * 100)
                        suffix: "%"
                        onMoved: v => SettingsData.set("fluidGlassSpecular", v / 100)
                    }
                    LabSlider {
                        label: I18n.tr("Rim width")
                        from: 1
                        to: 20
                        val: Math.round(SettingsData.fluidGlassRimWidth ?? 3)
                        suffix: " px"
                        onMoved: v => SettingsData.set("fluidGlassRimWidth", v)
                    }
                    LabSlider {
                        label: I18n.tr("Rim wrap")
                        from: 0
                        to: 100
                        val: Math.round((SettingsData.fluidGlassRimWrap ?? 0.45) * 100)
                        suffix: "%"
                        onMoved: v => SettingsData.set("fluidGlassRimWrap", v / 100)
                    }

                    SectionLabel {
                        text: I18n.tr("TINT")
                    }
                    StyledText {
                        visible: !(SettingsData.fluidGlassStained ?? false)
                        width: parent.width
                        text: I18n.tr("Tint disabled. Enable \"Stained Glass\" to adjust values.")
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.warning
                    }
                    LabSlider {
                        label: I18n.tr("Tint strength")
                        rowEnabled: labs.stained
                        from: 0
                        to: 100
                        val: Math.round((labs.frosted ? (SettingsData.fluidGlassLevel ?? 0.5) : (SettingsData.fluidGlassTintCustom ?? 0.16)) * 100)
                        suffix: "%"
                        onMoved: v => {
                            if (labs.frosted)
                                SettingsData.set("fluidGlassBlurCustom", SettingsData.fluidGlassLevel ?? 0.5);
                            SettingsData.set("fluidGlassTintCustom", v / 100);
                            SettingsData.set("fluidGlassFrosted", false);
                        }
                    }
                }
            }
        }
    }

    Loader {
        id: pickerLoader
        active: false
        sourceComponent: Component {
            FileBrowserModal {
                browserTitle: I18n.tr("Select Backdrop Image")
                browserIcon: "image"
                fileExtensions: ["*.png", "*.jpg", "*.jpeg", "*.bmp", "*.webp", "*.jxl", "*.avif", "*.heif"]
                parentModal: labs.parentModal
                onFileSelected: path => labs.addCustomBackdrop(path)
            }
        }
    }
}
