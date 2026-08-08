import QtQuick
import QtTest
import "../../src/settings" as Settings

TestCase {
    name: "BarSettingsPage"
    when: windowShown

    Component {
        id: pageComponent

        Settings.BarSettingsPage {
            width: 720
            height: 520
            barHeight: 40
            minimumBarHeight: 24
            maximumBarHeight: 96
            defaultBarHeight: 40
        }
    }

    function test_serviceAvailability() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);

        const warning = findChild(page, "serviceWarning");
        const control = findChild(page, "barHeightControl");
        const preview = findChild(page, "barPreview");
        verify(warning !== null);
        verify(control !== null);
        verify(preview !== null);
        compare(page.serviceWarningVisible, true);
        compare(page.controlsEnabled, false);
        compare(control.busy, true);
        compare(preview.barHeight, 40);
        compare(preview.configurationAvailable, false);

        page.serviceAvailable = true;
        compare(page.serviceWarningVisible, false);
        compare(page.controlsEnabled, true);
        compare(control.busy, false);
        compare(preview.configurationAvailable, true);
    }

    function test_recoveryMessages() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);

        const warning = findChild(page, "recoveryWarning");
        verify(warning !== null);
        compare(page.recoveryWarningVisible, false);

        page.recoveryState = "recovered";
        compare(page.recoveryWarningVisible, true);
        verify(page.recoveryMessage.includes("last known good"));

        page.recoveryState = "defaulted";
        compare(page.recoveryWarningVisible, true);
        verify(page.recoveryMessage.includes("safe defaults"));

        page.recoveryState = "normal";
        compare(page.recoveryWarningVisible, false);
    }

    function test_requestsAreForwardedOnce() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        page.serviceAvailable = true;

        const control = findChild(page, "barHeightControl");
        verify(control !== null);

        let requestedHeight = 0;
        let heightRequestCount = 0;
        let resetRequestCount = 0;
        page.barHeightRequested.connect(function(height) {
            requestedHeight = height;
            ++heightRequestCount;
        });
        page.resetBarHeightRequested.connect(function() {
            ++resetRequestCount;
        });

        control.valueRequested(64);
        compare(requestedHeight, 64);
        compare(heightRequestCount, 1);

        control.resetRequested();
        compare(resetRequestCount, 1);
    }

    function test_busyAndErrorsAreWired() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        page.serviceAvailable = true;

        const control = findChild(page, "barHeightControl");
        verify(control !== null);
        compare(control.busy, false);
        compare(control.errorText, "");

        page.busy = true;
        page.errorText = "Could not save the setting.";
        compare(control.busy, true);
        compare(control.errorText, "Could not save the setting.");
    }

    function test_narrowPreviewUsesDesktopScale() {
        const page = createTemporaryObject(pageComponent, this, {
            width: 423,
            height: 480
        });
        verify(page !== null);

        const preview = findChild(page, "barPreview");
        const frame = findChild(page, "previewBarFrame");
        const bar = findChild(page, "previewBarVisual");
        const reservedLabel = findChild(page, "reservedWorkspaceLabel");
        verify(preview !== null);
        verify(frame !== null);
        verify(bar !== null);
        verify(reservedLabel !== null);
        verify(preview.previewScale < 1);
        verify(frame.width * frame.scale <= preview.width);
        verify(reservedLabel.y >= frame.y + bar.height * frame.scale);
    }

}
