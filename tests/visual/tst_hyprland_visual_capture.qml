import "." as Visual
import QtQuick
import QtQuick.Window
import QtTest
import HyprShelld.UI

TestCase {
    id: testCase

    readonly property string captureDirectory: "/tmp/hyprshelld-visual-qa"
    readonly property var scenes: ["theme-selector", "overview", "catalog-appearance", "catalog-input", "catalog-windows", "catalog-shortcuts", "catalog-system", "catalog-session", "bindings-list", "bindings-editor", "bindings-submaps", "input-list", "input-editor", "environment-list", "environment-editor", "permissions-list", "permissions-editor", "displays-advanced", "displays-luminance", "appearance-visuals", "appearance-animations", "guided-input-global", "guided-input-gestures", "windows-layout", "workspaces", "rules-window", "rules-layer"]

    function captureScene(scene, theme, width, height, selectorMode) {
        const selectorVariant = selectorMode
            ? "-" + selectorMode + "-effective" : "";
        const output = "%1/%2%3-%4-%5x%6.png".arg(captureDirectory).arg(scene).arg(selectorVariant).arg(theme).arg(width).arg(height);
        const window = createTemporaryObject(harnessComponent, testCase, {
            "sceneName": scene,
            "themeMode": theme,
            "selectorMode": selectorMode || theme,
            "outputPath": output,
            "width": width,
            "height": height,
            "visible": true
        });
        verify(window !== null, scene);
        tryCompare(window, "fixtureReady", true, 5000);
        if (scene === "overview") {
            compare(window.pageItem.loadedCatalogOptionCount, 353);
            compare(window.pageItem.coverageComplete, true);
            verify(window.pageItem.coverageSummary.indexOf("353") >= 0);
        } else if (scene.startsWith("appearance-")) {
            const summary = window.findByObjectName(window.pageItem, "appearancePreviewSummary");
            verify(summary !== null);
            verify(summary.lineCount <= summary.maximumLineCount);
            verify(summary.height <= window.pageItem.height);
        } else if (scene === "theme-selector" && selectorMode === "automatic") {
            const badge = window.findByObjectName(
                window.pageItem, "themeAutomaticEffectiveBadge"
            );
            verify(badge !== null);
            compare(badge.visible, true);
            verify(badge.text.indexOf(
                theme === "light" ? "Light" : "Dark"
            ) >= 0);
        }
        waitForRendering(window.renderedItem);
        wait(ShellTheme.transitionDuration + 40);
        let completed = false;
        let saved = false;
        window.renderedItem.grabToImage(function (result) {
            saved = result.saveToFile(output);
            completed = true;
        });
        tryVerify(function () {
            return completed;
        }, 5000);
        verify(saved, output);
        window.destroy();
    }

    function test_captureAllSurfaces() {
        for (const theme of ["dark", "light"]) {
            for (const scene of scenes) {
                captureScene(scene, theme, 1080, 720);
                captureScene(scene, theme, 620, 720);
            }
            captureScene("theme-selector", theme, 1080, 720, "automatic");
            captureScene("theme-selector", theme, 620, 720, "automatic");
        }
    }

    function cleanupTestCase() {
        ShellTheme.previewMode = "";
    }

    name: "HyprlandSettingsVisualCapture"
    when: windowShown

    Component {
        id: harnessComponent

        Visual.HyprlandSettingsVisualHarness {
            autoCapture: false
        }
    }
}
