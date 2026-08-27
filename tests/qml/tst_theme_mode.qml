import QtQuick
import QtQuick.Window
import QtTest
import HyprShelld.UI
import "../../src/settings" as Settings

TestCase {
    id: testCase

    name: "ThemeMode"
    when: windowShown

    Component {
        id: selectorWindowComponent

        Window {
            width: 760
            height: 300
            visible: true

            property alias selector: selector

            Settings.ThemeModeSelector {
                id: selector
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: 12
                }
                mode: "automatic"
                effectiveMode: "light"
                serviceAvailable: true
            }
        }
    }

    SignalSpy {
        id: modeRequestedSpy
        signalName: "modeRequested"
    }

    function channelLuminance(channel) {
        return channel <= 0.04045
            ? channel / 12.92
            : Math.pow((channel + 0.055) / 1.055, 2.4);
    }

    function luminance(color) {
        let red = color.r;
        let green = color.g;
        let blue = color.b;
        if (![red, green, blue].every(Number.isFinite)) {
            const text = String(color);
            verify(/^#[0-9a-fA-F]{6}$/.test(text), text);
            red = parseInt(text.slice(1, 3), 16) / 255;
            green = parseInt(text.slice(3, 5), 16) / 255;
            blue = parseInt(text.slice(5, 7), 16) / 255;
        }
        return 0.2126 * channelLuminance(red)
            + 0.7152 * channelLuminance(green)
            + 0.0722 * channelLuminance(blue);
    }

    function contrast(first, second) {
        const a = luminance(first);
        const b = luminance(second);
        return (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05);
    }

    function test_selectorStateAccessibilityAndKeyboard() {
        const window = createTemporaryObject(
            selectorWindowComponent, testCase
        );
        verify(window !== null);
        const selector = window.selector;
        const automatic = findChild(selector, "themeMode-automatic");
        const light = findChild(selector, "themeMode-light");
        const dark = findChild(selector, "themeMode-dark");
        verify(automatic !== null);
        verify(light !== null);
        verify(dark !== null);
        compare(automatic.checked, true);
        compare(light.checked, false);
        compare(dark.checked, false);
        compare(automatic.Accessible.role, Accessible.RadioButton);
        compare(automatic.Accessible.name, "Automatic");
        verify(automatic.Accessible.description.indexOf("Light") >= 0);
        verify(automatic.height >= 44);
        verify(light.height >= 44);
        verify(dark.height >= 44);

        modeRequestedSpy.target = selector;
        modeRequestedSpy.clear();
        automatic.forceActiveFocus();
        tryCompare(automatic, "activeFocus", true);
        keyClick(Qt.Key_Right);
        tryCompare(light, "activeFocus", true);
        compare(modeRequestedSpy.count, 1);
        compare(modeRequestedSpy.signalArguments[0][0], "light");

        // The persisted mode remains authoritative while the request is
        // pending; keyboard and pointer activation cannot optimistically
        // replace it.
        compare(automatic.checked, true);
        compare(light.checked, false);
        selector.busy = true;
        mouseClick(dark);
        compare(modeRequestedSpy.count, 1);
        compare(automatic.checked, true);
        compare(dark.checked, false);

        selector.errorText = "Could not save the color mode";
        selector.busy = false;
        const errorLabel = findChild(selector, "themeModeError");
        verify(errorLabel !== null);
        compare(errorLabel.visible, true);
        compare(errorLabel.Accessible.role, Accessible.AlertMessage);
        compare(automatic.checked, true);
        compare(light.checked, false);

        selector.errorText = "";
        selector.mode = "light";
        compare(automatic.checked, false);
        compare(light.checked, true);
        modeRequestedSpy.clear();
        mouseClick(dark);
        compare(modeRequestedSpy.count, 1);
        compare(modeRequestedSpy.signalArguments[0][0], "dark");
        compare(light.checked, true);
        compare(dark.checked, false);
        window.destroy();
    }

    function test_semanticTextContrast_data() {
        return [
            { tag: "dark", mode: "dark" },
            { tag: "light", mode: "light" }
        ];
    }

    function test_semanticTextContrast(data) {
        const mode = data.mode;
        verify(contrast(
            ShellTheme.colorFor(mode, "onSurface"),
            ShellTheme.colorFor(mode, "canvas")
        ) >= 4.5);
        verify(contrast(
            ShellTheme.colorFor(mode, "onSurfaceMuted"),
            ShellTheme.colorFor(mode, "card")
        ) >= 4.5);
        verify(contrast(
            ShellTheme.colorFor(mode, "onPrimary"),
            ShellTheme.colorFor(mode, "primary")
        ) >= 4.5);
        for (const surface of ["canvas", "card"]) {
            verify(contrast(
                ShellTheme.colorFor(mode, "primary"),
                ShellTheme.colorFor(mode, surface)
            ) >= 4.5, mode + " primary on " + surface);
        }
        for (const status of ["Success", "Warning", "Error", "Info"]) {
            verify(contrast(
                ShellTheme.colorFor(mode, "on" + status + "Container"),
                ShellTheme.colorFor(
                    mode,
                    status.charAt(0).toLowerCase()
                        + status.slice(1) + "Container"
                )
            ) >= 4.5, mode + " " + status);
        }
    }

    function cleanup() {
        modeRequestedSpy.target = null;
        modeRequestedSpy.clear();
    }
}
