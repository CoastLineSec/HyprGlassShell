import QtQuick
import QtQuick.Effects
import qs.Common
import qs.Services

// Reusable "fluid glass" surface for HyprGlassShell.
//
// Wayland note: a QML surface can't sample other app windows behind it — only the
// wallpaper, which the shell itself renders. So this re-renders the monitor's
// wallpaper, crops the region sitting behind this surface, frosts it, and runs the
// calibrated liquid-glass lensing shader (edge refraction → saturation → tint →
// specular rim → inner shadow). Look + specs match the approved Preview reference.
Item {
    id: root

    // --- Placement on the monitor (logical px), for wallpaper alignment ---
    property string screenName: ""
    property real screenW: 0
    property real screenH: 0
    property real surfaceX: 0      // this surface's top-left on the screen
    property real surfaceY: 0
    property real cornerRadius: Math.min(width, height) / 2

    // --- Locked baseline geometry (user-calibrated, constant across frost/tint) ---
    property real refractionPx: 34
    property real bevelPx: 18
    property real saturation: 1.3
    property real rimStrength: 0.15
    property real shadowStrength: 0.15
    property real lightDirX: -0.4
    property real lightDirY: -1.0

    // --- One continuous "glass level" (0..1) drives frost + tint together ---
    //   0.0 -> frost 0.0 / tint 0.02   (Low)
    //   0.5 -> frost 0.5 / tint 0.18   (Medium, default)
    //   1.0 -> frost 1.0 / tint 0.38   (High)
    property real level: 0.5
    property real frost: level
    property color tintHue: "#ffffff"
    property bool noTint: false
    property real tintStrength: noTint ? 0.0 : (level <= 0.5 ? (0.02 + 0.32 * level) : (0.18 + 0.40 * (level - 0.5)))

    // Optional flat-colour backdrop (e.g. an app's own page background). When set
    // (alpha > 0) the glass refracts/frosts THIS instead of the wallpaper, and it
    // refreshes live whenever the colour changes.
    property color backdropColor: "transparent"
    readonly property bool _useColorBackdrop: backdropColor.a > 0
    onBackdropColorChanged: backdropSrc.scheduleUpdate()

    readonly property string _rawWallpaper: SessionData.getMonitorWallpaper(screenName) || ""
    readonly property bool _isColorWallpaper: _rawWallpaper.startsWith("#")
    readonly property string _wallpaperUrl: {
        if (!_rawWallpaper || _isColorWallpaper)
            return "";
        return _rawWallpaper.startsWith("file://") ? _rawWallpaper : "file://" + _rawWallpaper.split('/').map(s => encodeURIComponent(s)).join('/');
    }
    readonly property bool hasBackdrop: _useColorBackdrop || _wallpaperUrl !== ""

    function _fillMode(name) {
        switch (name) {
        case "Stretch":
            return Image.Stretch;
        case "Fit":
        case "PreserveAspectFit":
            return Image.PreserveAspectFit;
        case "Tile":
            return Image.Tile;
        case "Pad":
            return Image.Pad;
        default:
            return Image.PreserveAspectCrop;
        }
    }

    clip: true

    // Solid-colour wallpaper: nothing to refract — show the colour, softly tinted.
    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        visible: root._isColorWallpaper && !root._useColorBackdrop
        color: root._isColorWallpaper ? root._rawWallpaper : "transparent"
    }

    // Flat-colour backdrop source (e.g. the settings page background). Bound to a
    // theme colour at the call site, so changing the colour refreshes the glass.
    Rectangle {
        id: colorBackdrop
        anchors.fill: parent
        color: root._useColorBackdrop ? root.backdropColor : "transparent"
        visible: false
    }

    // Full-monitor wallpaper copy (matches the wallpaper window's size/fillMode so
    // the crop aligns with what's actually on screen behind this surface).
    Image {
        id: wallpaper
        width: root.screenW
        height: root.screenH
        source: root._useColorBackdrop ? "" : root._wallpaperUrl
        fillMode: root._fillMode(SessionData.getMonitorWallpaperFillMode(root.screenName))
        sourceSize: Qt.size(root.screenW, root.screenH)
        cache: true
        smooth: true
        asynchronous: true
        visible: false
    }

    // Crop the backdrop behind this surface — the wallpaper region, or the flat colour.
    ShaderEffectSource {
        id: backdropSrc
        anchors.fill: parent
        sourceItem: root._useColorBackdrop ? colorBackdrop : wallpaper
        sourceRect: root._useColorBackdrop ? Qt.rect(0, 0, Math.max(1, root.width), Math.max(1, root.height)) : Qt.rect(root.surfaceX, root.surfaceY, Math.max(1, root.width), Math.max(1, root.height))
        live: true
        visible: false
    }

    // Frost (blur) — the "glass level" drives blur amount.
    MultiEffect {
        id: frosted
        anchors.fill: parent
        source: backdropSrc
        blurEnabled: true
        blur: root.frost
        blurMax: 64
        autoPaddingEnabled: false
        visible: false
        layer.enabled: true
    }

    ShaderEffectSource {
        id: frostedSrc
        anchors.fill: parent
        sourceItem: frosted
        live: true
        visible: false
    }

    // The liquid-glass lensing shader. Self-masks to the rounded rect (AA alpha).
    ShaderEffect {
        anchors.fill: parent
        visible: root.hasBackdrop
        property variant src: frostedSrc
        property real widthPx: root.width
        property real heightPx: root.height
        property real cornerRadiusPx: root.cornerRadius
        property real refractionPx: root.refractionPx
        property real bevelPx: root.bevelPx
        property real saturation: root.saturation
        property real rimStrength: root.rimStrength
        property real shadowStrength: root.shadowStrength
        property real lightDirX: root.lightDirX
        property real lightDirY: root.lightDirY
        property vector4d tintColor: Qt.vector4d(root.tintHue.r, root.tintHue.g, root.tintHue.b, root.tintStrength)
        fragmentShader: Qt.resolvedUrl("../Shaders/qsb/liquid_glass.frag.qsb")
    }
}
