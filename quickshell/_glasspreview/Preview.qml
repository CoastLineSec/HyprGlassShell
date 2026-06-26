import QtQuick
import QtQuick.Window
import QtQuick.Effects

// Throwaway tuning harness for the liquid-glass shader. Not shipped.
// Renders a fixed-size stage and saves it to /tmp/grab.png, then quits —
// deterministic, no screen-capture / window-geometry guessing.
// Run: qml6 Preview.qml
Window {
    id: win
    visible: true
    width: 1000
    height: 360
    color: "#0b0b0e"
    title: "LiquidGlassPreview"

    // Locked baseline geometry (constant across the glass level).
    property real refractionPx: 34
    property real bevelPx: 18
    property real saturation: 1.3
    property real rimStrength: 0.40
    property real shadowStrength: 0.15

    // ---- ONE "glass level" slider drives frost + tint together (user-calibrated):
    //   level 0.0 (Low)    -> frost 0.0, tint 0.02
    //   level 0.5 (Medium) -> frost 0.5, tint 0.18   (default)
    //   level 1.0 (High)   -> frost 1.0, tint 0.38
    // Interpolated piecewise-linearly between those anchors.
    property real level: 0.5
    property real frost: level
    // Tint mode: tintHue is set per mode (System/Matugen/Custom); noTint forces 0.
    property color tintHue: "#ffffff"
    property bool noTint: false
    property real tintStrength: noTint ? 0.0 : (level <= 0.5 ? (0.02 + 0.32 * level) : (0.18 + 0.40 * (level - 0.5)))

    // Fixed-size stage that gets grabbed to file (deterministic output).
    Item {
        id: stage
        width: 980
        height: 320
        anchors.centerIn: parent
        clip: true

        // Backdrop: Apple's own reference photo (pill cropped out) for honest matching.
        Image {
            id: bg
            width: 1000
            height: 640
            anchors.centerIn: parent
            source: "file:///tmp/glass_bg.png"
            fillMode: Image.PreserveAspectCrop
            smooth: true
        }

        // The glass search pill, centred.
        Item {
            id: pill
            width: 660
            height: 100
            anchors.centerIn: parent

            ShaderEffectSource {
                id: backdropSrc
                anchors.fill: parent
                sourceItem: bg
                // pill and bg are both centred, so the pill maps to the centred rect of bg.
                sourceRect: Qt.rect((bg.width - pill.width) / 2, (bg.height - pill.height) / 2, pill.width, pill.height)
                live: true
                visible: false
            }

            MultiEffect {
                id: frosted
                anchors.fill: parent
                source: backdropSrc
                blurEnabled: true
                blur: win.frost
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

            ShaderEffect {
                anchors.fill: parent
                property variant src: frostedSrc
                property real widthPx: pill.width
                property real heightPx: pill.height
                property real cornerRadiusPx: pill.height / 2
                property real refractionPx: win.refractionPx
                property real bevelPx: win.bevelPx
                property real saturation: win.saturation
                property real rimStrength: win.rimStrength
                property real shadowStrength: win.shadowStrength
                property real lightDirX: -0.4
                property real lightDirY: -1.0
                property vector4d tintColor: Qt.vector4d(win.tintHue.r, win.tintHue.g, win.tintHue.b, win.tintStrength)
                fragmentShader: "file:///home/james/Github/CoastLineSec/HyprGlassShell-fork/quickshell/Shaders/qsb/liquid_glass.frag.qsb"
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 28
                anchors.verticalCenter: parent.verticalCenter
                spacing: 14
                Text { text: "⌕"; color: "white"; font.pixelSize: 30; anchors.verticalCenter: parent.verticalCenter }
                Text { text: "Search"; color: Qt.rgba(1, 1, 1, 0.9); font.pixelSize: 26; anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }

    // Auto-grab the stage to a deterministic file, then quit.
    Timer {
        interval: 1400
        running: true
        repeat: false
        onTriggered: stage.grabToImage(function(result) {
            result.saveToFile("/tmp/grab.png");
            Qt.callLater(Qt.quit);
        }, Qt.size(stage.width * 2, stage.height * 2))
    }
}
