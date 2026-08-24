pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id: root

    property int borderSize: 1
    property int rounding: 0
    property real roundingPower: 2
    property bool blurEnabled: true
    property bool shadowEnabled: true
    property int shadowRange: 4
    property int shadowRenderPower: 3
    property bool shadowSharp: false
    property real shadowOffsetX: 0
    property real shadowOffsetY: 0
    property real shadowScale: 1
    property bool glowEnabled: false
    property int glowRange: 10
    property int glowRenderPower: 3
    property bool borderPartOfWindow: true
    property bool dimInactive: false
    property real dimStrength: 0.5
    property real activeOpacity: 1
    property real inactiveOpacity: 1
    property real fullscreenOpacity: 1
    property bool dimModal: true
    property real dimSpecial: 0.2
    property real dimAround: 0.4
    property int blurSize: 8
    property int blurPasses: 1
    property bool blurIgnoreOpacity: true
    property bool blurOptimizations: true
    property bool blurXray: false
    property real blurBrightness: 1
    property real blurContrast: 0.8916
    property real blurNoise: 0.0117
    property real blurVibrancy: 0.1696
    property real blurVibrancyDarkness: 0
    property bool blurSpecial: false
    property bool blurPopups: false
    property real blurPopupsIgnoreAlpha: 0.2
    property bool blurInputMethods: false
    property real blurInputMethodsIgnoreAlpha: 0.2
    property bool animationsEnabled: true
    property string layoutMode: "dwindle"
    property bool resizeOnBorder: false
    property bool snapEnabled: false
    property string summaryMode: "appearance"
    property string motionButtonObjectName: "toggleAppearanceMotionButton"
    property bool motionPaused: false
    property real motionProgress: 0
    property string motionPhase: "resting"
    readonly property real minimumTargetSize: 44
    readonly property bool motionRunning: motionLoop.running && !motionLoop.paused
    readonly property real effectiveDimStrength: typeof root.dimStrength === "number" && Number.isFinite(root.dimStrength) ? Math.max(0, Math.min(1, root.dimStrength)) : 0
    readonly property real effectiveActiveOpacity: root.boundedUnitValue(root.activeOpacity)
    readonly property real effectiveInactiveOpacity: root.boundedUnitValue(root.inactiveOpacity)
    readonly property real effectiveFullscreenOpacity: root.boundedUnitValue(root.fullscreenOpacity)
    readonly property real effectiveDimSpecial: root.boundedUnitValue(root.dimSpecial)
    readonly property real effectiveDimAround: root.boundedUnitValue(root.dimAround)
    readonly property real effectiveBlurPopupsIgnoreAlpha: root.boundedUnitValue(root.blurPopupsIgnoreAlpha)
    readonly property real effectiveBlurInputMethodsIgnoreAlpha: root.boundedUnitValue(root.blurInputMethodsIgnoreAlpha)
    readonly property string motionStory: {
        if (root.layoutMode === "master")
            return "master-stack";
        if (root.layoutMode === "scrolling")
            return "scrolling-strip";
        if (root.layoutMode === "monocle")
            return "monocle-replace";
        return "dwindle-split";
    }
    readonly property string motionStatus: {
        if (!root.animationsEnabled)
            return qsTr("off");
        return root.motionPaused ? qsTr("paused") : qsTr("playing");
    }

    readonly property string layoutName: {
        if (root.layoutMode === "master")
            return qsTr("Master");
        if (root.layoutMode === "scrolling")
            return qsTr("Scrolling");
        if (root.layoutMode === "monocle")
            return qsTr("Monocle");
        return qsTr("Dwindle");
    }
    readonly property string accessibleSummary: {
        if (root.summaryMode === "windows") {
            return qsTr("Illustrative preview. %1 layout. Border resizing %2. Floating-window snapping %3. Motion %4.").arg(root.layoutName).arg(root.resizeOnBorder ? qsTr("on") : qsTr("off")).arg(root.snapEnabled ? qsTr("on") : qsTr("off")).arg(root.motionStatus);
        }
        return qsTr("Illustrative preview in a representative Dwindle layout. Border thickness %1. Corner radius %2. Window corner power %3. Blur %4. Shadows %5. Shadow range %6. Soft-shadow falloff power %7. Sharp shadow edges %8. Shadow scale %9. Horizontal shadow offset %10. Vertical shadow offset %11. Visible borders included in window-shadow bounds %12. Inactive-window dimming %13. Dimming strength %14. Active-window opacity %15. Inactive-window opacity %16. True-fullscreen opacity %17. Modal-parent dimming %18. Special-workspace dimming %19. Dim-around strength %20. Blur size %21. Blur passes %22. Blur ignores opacity %23. Optimized blur path %24. X-ray blur %25. Blur brightness %26. Blur contrast %27. Blur noise %28. Blur vibrancy %29. Dark-area vibrancy %30. Special-workspace blur %31. Popup blur %32. Popup ignore-alpha threshold %33. Input-method blur %34. Input-method ignore-alpha threshold %35. Animations %36. Motion %37. Inner window glow %38. Glow range %39. Glow falloff power %40.").arg(root.borderSize).arg(root.rounding).arg(root.exactSummaryValue(root.roundingPower)).arg(root.blurEnabled ? qsTr("on") : qsTr("off")).arg(root.shadowEnabled ? qsTr("on") : qsTr("off")).arg(root.shadowRange).arg(root.shadowRenderPower).arg(root.shadowSharp ? qsTr("on") : qsTr("off")).arg(root.exactSummaryValue(root.shadowScale)).arg(root.exactSummaryValue(root.shadowOffsetX)).arg(root.exactSummaryValue(root.shadowOffsetY)).arg(root.borderPartOfWindow ? qsTr("on") : qsTr("off")).arg(root.dimInactive ? qsTr("on") : qsTr("off")).arg(root.effectiveDimStrength.toFixed(2)).arg(root.effectiveActiveOpacity.toFixed(2)).arg(root.effectiveInactiveOpacity.toFixed(2)).arg(root.effectiveFullscreenOpacity.toFixed(2)).arg(root.dimModal ? qsTr("on") : qsTr("off")).arg(root.effectiveDimSpecial.toFixed(2)).arg(root.effectiveDimAround.toFixed(2)).arg(root.blurSize).arg(root.blurPasses).arg(root.blurIgnoreOpacity ? qsTr("on") : qsTr("off")).arg(root.blurOptimizations ? qsTr("on") : qsTr("off")).arg(root.blurXray ? qsTr("on") : qsTr("off")).arg(root.exactSummaryValue(root.blurBrightness)).arg(root.exactSummaryValue(root.blurContrast)).arg(root.exactSummaryValue(root.blurNoise)).arg(root.exactSummaryValue(root.blurVibrancy)).arg(root.exactSummaryValue(root.blurVibrancyDarkness)).arg(root.blurSpecial ? qsTr("on") : qsTr("off")).arg(root.blurPopups ? qsTr("on") : qsTr("off")).arg(root.effectiveBlurPopupsIgnoreAlpha.toFixed(2)).arg(root.blurInputMethods ? qsTr("on") : qsTr("off")).arg(root.effectiveBlurInputMethodsIgnoreAlpha.toFixed(2)).arg(root.animationsEnabled ? qsTr("on") : qsTr("off")).arg(root.motionStatus).arg(root.glowEnabled ? qsTr("on") : qsTr("off")).arg(root.glowRange).arg(root.glowRenderPower);
    }

    implicitHeight: 330

    function boundedUnitValue(value) {
        return typeof value === "number" && Number.isFinite(value) ? Math.max(0, Math.min(1, value)) : 0;
    }

    function exactSummaryValue(value) {
        if (typeof value !== "number" || !Number.isFinite(value))
            return qsTr("invalid draft");
        return String(Object.is(value, -0) ? 0 : value);
    }

    function synchronizeMotion(restart) {
        if (restart) {
            motionLoop.stop();
            root.motionProgress = 0;
            root.motionPhase = root.animationsEnabled ? "resting" : "off";
        }

        if (!root.visible || !root.animationsEnabled) {
            motionLoop.stop();
            root.motionProgress = 0;
            root.motionPhase = root.animationsEnabled ? "resting" : "off";
            return;
        }

        if (root.motionPaused) {
            if (motionLoop.running)
                motionLoop.pause();
            return;
        }

        if (motionLoop.running) {
            if (motionLoop.paused)
                motionLoop.resume();
            return;
        }

        motionLoop.start();
    }

    function toggleMotion() {
        if (root.animationsEnabled)
            root.motionPaused = !root.motionPaused;
    }

    onAnimationsEnabledChanged: root.synchronizeMotion(true)
    onLayoutModeChanged: root.synchronizeMotion(true)
    onMotionPausedChanged: root.synchronizeMotion(false)
    onVisibleChanged: root.synchronizeMotion(true)

    Component.onCompleted: root.synchronizeMotion(true)

    SequentialAnimation {
        id: motionLoop

        loops: Animation.Infinite

        ScriptAction {
            script: {
                root.motionProgress = 0;
                root.motionPhase = "opening";
            }
        }

        NumberAnimation {
            target: root
            property: "motionProgress"
            from: 0
            to: 1
            duration: 480
            easing.type: Easing.OutCubic
        }

        ScriptAction {
            script: root.motionPhase = "settled"
        }

        PauseAnimation {
            duration: 1550
        }

        ScriptAction {
            script: root.motionPhase = "closing"
        }

        NumberAnimation {
            target: root
            property: "motionProgress"
            from: 1
            to: 0
            duration: 380
            easing.type: Easing.InOutCubic
        }

        ScriptAction {
            script: root.motionPhase = "resting"
        }

        PauseAnimation {
            duration: 1200
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Rectangle {
            id: stage

            objectName: "appearancePreviewStage"
            readonly property real windowAreaLeft: width * 0.07
            readonly property real windowAreaRight: width * 0.93
            readonly property real windowAreaWidth: windowAreaRight - windowAreaLeft
            readonly property real windowAreaTop: 45
            readonly property real windowAreaHeight: height - 63
            readonly property real windowGap: Math.max(8, Math.round(width * 0.02))
            readonly property real halfWindowWidth: (windowAreaWidth - windowGap) / 2
            readonly property real masterWidth: (windowAreaWidth - windowGap) * 0.55
            readonly property real masterStackWidth: windowAreaWidth - windowGap - masterWidth
            readonly property real masterStackTileHeight: (windowAreaHeight - windowGap) / 2
            readonly property real scrollingAreaTop: 54
            readonly property real scrollingAreaHeight: height - 74
            readonly property real scrollingTravel: halfWindowWidth + windowGap
            readonly property real dwindleAreaLeft: windowAreaLeft
            readonly property real dwindleAreaRight: windowAreaRight
            readonly property real dwindleAreaWidth: windowAreaWidth
            readonly property real dwindleGap: windowGap
            readonly property real dwindleTileWidth: halfWindowWidth

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 240
            radius: 14
            color: "#111722"
            border.color: root.palette.mid
            clip: true
            Accessible.ignored: true

            Rectangle {
                width: Math.max(86, stage.width * 0.24)
                height: width
                x: stage.width * 0.08
                y: stage.height * 0.14
                radius: width / 2
                color: "#6558d8"
                opacity: 0.58
                Accessible.ignored: true
            }

            Rectangle {
                width: Math.max(72, stage.width * 0.2)
                height: width
                x: stage.width * 0.67
                y: stage.height * 0.52
                radius: width / 2
                color: "#c85c8e"
                opacity: 0.5
                Accessible.ignored: true
            }

            Rectangle {
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                }
                height: 28
                color: "#d9171b22"

                RowLayout {
                    anchors {
                        fill: parent
                        leftMargin: 10
                        rightMargin: 10
                    }
                    spacing: 7

                    Repeater {
                        model: 3

                        Rectangle {
                            required property int index

                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: width / 2
                            color: index === 0 ? "#7c91ff" : "#687386"
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Label {
                        text: root.layoutName
                        color: "#c9d1df"
                        font.pixelSize: 10
                    }
                }
            }

            Rectangle {
                id: snapGuideVertical

                objectName: "appearancePreviewSnapGuide"
                visible: root.snapEnabled
                width: 2
                height: stage.height - 48
                x: Math.round(stage.width / 2)
                y: 38
                color: "#997c91ff"
                Accessible.ignored: true
            }

            Rectangle {
                visible: root.snapEnabled
                width: stage.width - 32
                height: 2
                x: 16
                y: Math.round(stage.height / 2) + 10
                color: "#997c91ff"
                Accessible.ignored: true
            }

            Rectangle {
                id: secondaryShadow

                objectName: "appearancePreviewSecondaryShadow"
                visible: root.shadowEnabled && secondaryWindow.visible
                x: secondaryWindow.x + 7
                y: secondaryWindow.y + 8
                width: secondaryWindow.width
                height: secondaryWindow.height
                radius: secondaryWindow.radius
                color: "#80000000"
                opacity: secondaryWindow.opacity
                Accessible.ignored: true
            }

            Rectangle {
                id: secondaryWindow

                objectName: "appearancePreviewSecondaryWindow"
                visible: root.layoutMode === "master" || root.layoutMode === "scrolling"
                x: {
                    if (root.layoutMode === "scrolling")
                        return stage.windowAreaLeft + stage.scrollingTravel * (1 - root.motionProgress);
                    return stage.windowAreaLeft + stage.masterWidth + stage.windowGap;
                }
                y: root.layoutMode === "scrolling" ? stage.scrollingAreaTop : stage.windowAreaTop
                width: root.layoutMode === "scrolling" ? stage.halfWindowWidth : stage.masterStackWidth
                height: root.layoutMode === "master" ? stage.windowAreaHeight - (stage.windowAreaHeight - stage.masterStackTileHeight) * root.motionProgress : stage.scrollingAreaHeight
                radius: Math.min(height / 2, root.rounding * 1.15)
                color: root.blurEnabled ? "#c52a3342" : "#2a3342"
                border.width: Math.min(6, Math.max(0, root.borderSize))
                border.color: root.borderSize > 0 ? "#8490a3" : "transparent"
                opacity: root.effectiveInactiveOpacity
                Accessible.ignored: true

                Rectangle {
                    anchors {
                        fill: parent
                        margins: 12
                    }
                    radius: Math.max(0, parent.radius - 6)
                    color: root.blurEnabled ? "#245e6f91" : "#344055"
                }
            }

            Rectangle {
                id: activeShadow

                visible: root.shadowEnabled
                x: activeWindow.x + 8
                y: activeWindow.y + 9
                width: activeWindow.width
                height: activeWindow.height
                radius: activeWindow.radius
                color: "#90000000"
                opacity: activeWindow.opacity
                Accessible.ignored: true
            }

            Rectangle {
                id: activeWindow

                objectName: "appearancePreviewActiveWindow"
                x: {
                    if (root.layoutMode === "scrolling")
                        return stage.windowAreaLeft - stage.scrollingTravel * root.motionProgress;
                    if (root.layoutMode === "dwindle")
                        return stage.dwindleAreaLeft;
                    return stage.windowAreaLeft;
                }
                y: root.layoutMode === "scrolling" ? stage.scrollingAreaTop : stage.windowAreaTop
                width: {
                    if (root.layoutMode === "dwindle")
                        return stage.dwindleAreaWidth - (stage.dwindleAreaWidth - stage.dwindleTileWidth) * root.motionProgress;
                    if (root.layoutMode === "master")
                        return stage.masterWidth;
                    if (root.layoutMode === "scrolling")
                        return stage.halfWindowWidth;
                    if (root.layoutMode === "monocle")
                        return stage.windowAreaWidth;
                    return stage.width * 0.47;
                }
                height: root.layoutMode === "scrolling" ? stage.scrollingAreaHeight : stage.windowAreaHeight
                radius: Math.min(height / 2, root.rounding * 1.15)
                color: root.blurEnabled ? "#d92a3342" : "#2a3342"
                border.width: Math.min(6, Math.max(0, root.borderSize))
                border.color: root.borderSize > 0 ? "#7c91ff" : "transparent"
                opacity: {
                    if (root.layoutMode === "dwindle") {
                        return root.effectiveActiveOpacity + (root.effectiveInactiveOpacity - root.effectiveActiveOpacity) * root.motionProgress;
                    }
                    if (root.layoutMode === "monocle" || root.layoutMode === "scrolling") {
                        return (1 - root.motionProgress) * root.effectiveActiveOpacity;
                    }
                    return root.effectiveActiveOpacity;
                }

                Rectangle {
                    anchors {
                        fill: parent
                        margins: 14
                    }
                    radius: Math.max(0, parent.radius - 7)
                    color: root.blurEnabled ? "#365f75a0" : "#3e4a60"

                    ColumnLayout {
                        anchors {
                            fill: parent
                            margins: 12
                        }
                        spacing: 7

                        Rectangle {
                            Layout.preferredWidth: Math.max(30, parent.width * 0.62)
                            Layout.preferredHeight: 7
                            radius: 4
                            color: "#b7c2d5"
                        }

                        Rectangle {
                            Layout.preferredWidth: Math.max(24, parent.width * 0.42)
                            Layout.preferredHeight: 5
                            radius: 3
                            color: "#71809a"
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }

                Rectangle {
                    objectName: "appearancePreviewInactiveDimOverlay"
                    anchors.fill: parent
                    radius: parent.radius
                    color: "#000000"
                    opacity: root.dimInactive ? root.effectiveDimStrength * root.motionProgress : 0
                    Accessible.ignored: true
                }

                Repeater {
                    model: root.resizeOnBorder ? 4 : 0

                    Rectangle {
                        required property int index

                        width: 7
                        height: 7
                        radius: 2
                        color: "#f4f6fa"
                        x: index % 2 === 0 ? -3 : activeWindow.width - 4
                        y: index < 2 ? -3 : activeWindow.height - 4
                        Accessible.ignored: true
                    }
                }
            }

            Rectangle {
                id: spawnedShadow

                visible: root.shadowEnabled && spawnedWindow.opacity > 0
                x: spawnedWindow.x + 8
                y: spawnedWindow.y + 9
                width: spawnedWindow.width
                height: spawnedWindow.height
                radius: spawnedWindow.radius
                color: "#90000000"
                opacity: spawnedWindow.opacity * 0.9
                scale: spawnedWindow.scale
                Accessible.ignored: true
            }

            Rectangle {
                id: spawnedWindow

                objectName: "appearancePreviewSpawnedWindow"
                x: {
                    if (root.layoutMode === "master")
                        return stage.windowAreaLeft + stage.masterWidth + stage.windowGap;
                    if (root.layoutMode === "scrolling")
                        return stage.windowAreaLeft + stage.scrollingTravel * (2 - root.motionProgress);
                    if (root.layoutMode === "monocle")
                        return stage.windowAreaLeft;
                    return activeWindow.x + activeWindow.width + stage.dwindleGap;
                }
                y: {
                    if (root.layoutMode === "master")
                        return secondaryWindow.y + secondaryWindow.height + stage.windowGap;
                    return root.layoutMode === "scrolling" ? stage.scrollingAreaTop : stage.windowAreaTop;
                }
                width: {
                    if (root.layoutMode === "scrolling")
                        return stage.halfWindowWidth;
                    if (root.layoutMode === "monocle")
                        return stage.windowAreaWidth;
                    if (root.layoutMode === "dwindle")
                        return stage.dwindleTileWidth;
                    return stage.masterStackWidth;
                }
                height: {
                    if (root.layoutMode === "master")
                        return stage.masterStackTileHeight;
                    return root.layoutMode === "scrolling" ? stage.scrollingAreaHeight : stage.windowAreaHeight;
                }
                radius: Math.min(height / 2, root.rounding * 1.15)
                color: root.blurEnabled ? "#e2364254" : "#364254"
                border.width: Math.min(6, Math.max(0, root.borderSize))
                border.color: root.borderSize > 0 ? "#a8b4ff" : "transparent"
                opacity: root.motionProgress * root.effectiveActiveOpacity
                scale: root.layoutMode === "monocle" ? 1 : 0.90 + 0.10 * root.motionProgress
                Accessible.ignored: true

                Rectangle {
                    anchors {
                        fill: parent
                        margins: 13
                    }
                    radius: Math.max(0, parent.radius - 7)
                    color: root.blurEnabled ? "#466f86b2" : "#46536a"

                    ColumnLayout {
                        anchors {
                            fill: parent
                            margins: 11
                        }
                        spacing: 7

                        Rectangle {
                            Layout.preferredWidth: Math.max(24, parent.width * 0.56)
                            Layout.preferredHeight: 7
                            radius: 4
                            color: "#d2d8e3"
                        }

                        Rectangle {
                            Layout.preferredWidth: Math.max(20, parent.width * 0.38)
                            Layout.preferredHeight: 5
                            radius: 3
                            color: "#8290aa"
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }
            }

            RowLayout {
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    margins: 10
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Illustrative preview")
                    color: "#aeb8c6"
                    font.pixelSize: 10
                }

                Button {
                    objectName: root.motionButtonObjectName
                    implicitHeight: Math.max(root.minimumTargetSize, implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
                    text: !root.animationsEnabled ? qsTr("Motion off") : root.motionPaused ? qsTr("Play motion") : qsTr("Pause motion")
                    enabled: root.animationsEnabled
                    Accessible.name: !root.animationsEnabled ? qsTr("Illustrative window motion is off") : root.motionPaused ? qsTr("Play the illustrative window motion") : qsTr("Pause the illustrative window motion")

                    onClicked: root.toggleMotion()
                }
            }
        }

        Label {
            objectName: "appearancePreviewSummary"
            Layout.fillWidth: true
            text: root.accessibleSummary
            color: root.palette.placeholderText
            font.pixelSize: 11
            wrapMode: Text.Wrap
            maximumLineCount: root.width < 700 ? 4 : 5
            elide: Text.ElideRight
            textFormat: Text.PlainText
            Accessible.name: text
        }
    }
}
