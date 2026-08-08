import QtQuick
import QtQuick.Window
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

    Component {
        id: healthWarningComponent

        Window {
            width: 375
            height: 480
            visible: true

            property alias warning: healthWarning

            Settings.ShellHealthWarning {
                id: healthWarning

                width: parent.width
                coordinatorAvailable: true
                coordinatorHealthy: true
                coordinatorFailedUnits: []
            }
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

    function test_healthWarningIsQuietWhenHealthy() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        compare(warning.warningVisible, false);
        compare(warning.failedComponentCount, 0);
        compare(warning.visible, false);
    }

    function test_coordinatorFailureOffersOneRestart() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorHealthy = false;
        warning.coordinatorFailedUnits = ["hyprshelld-configd.service"];
        waitForRendering(warning);
        compare(warning.warningVisible, true);
        compare(warning.failedComponentCount, 1);
        compare(warning.friendlyName("hyprshelld-configd.service"), "Settings service");
        verify(!warning.friendlyName("hyprshelld-configd.service").includes(".service"));

        const restartButton = findChild(
            warning,
            "restartButton-hyprshelld-configd.service"
        );
        verify(restartButton !== null);
        compare(restartButton.visible, true);
        compare(restartButton.enabled, true);

        let requestedUnit = "";
        let requestCount = 0;
        warning.restartRequested.connect(function(unitName) {
            requestedUnit = unitName;
            ++requestCount;
        });
        restartButton.clicked();
        compare(requestedUnit, "hyprshelld-configd.service");
        compare(requestCount, 1);

        warning.restartBusy = true;
        warning.restartingUnit = "hyprshelld-configd.service";
        compare(restartButton.enabled, false);
        compare(restartButton.text, "Restarting…");

        warning.restartErrorUnit = "hyprshelld-configd.service";
        warning.restartError = "The restart request was rejected.";
        const error = findChild(warning, "restartError");
        verify(error !== null);
        compare(error.visible, true);
        verify(error.text.includes("Settings service"));

        warning.coordinatorFailedUnits = ["hyprshelld-surfaced.service"];
        compare(error.visible, false);
    }

    function test_systemdFallbackIsReadOnly() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorAvailable = false;
        warning.fallbackActive = true;
        warning.fallbackAvailable = true;
        warning.targetState = "active";
        warning.coordinatorState = "failed";
        warning.configurationState = "active";
        warning.surfaceState = "active";
        waitForRendering(warning);
        compare(warning.warningVisible, true);
        compare(warning.failedComponentCount, 1);
        compare(warning.friendlyName("hyprshelld.service"), "Shell health");

        const restartButton = findChild(
            warning,
            "restartButton-hyprshelld.service"
        );
        verify(restartButton !== null);
        compare(restartButton.visible, false);
        verify(warning.warningDescription.includes("directly from systemd"));
    }

    function test_unavailableFallbackRemainsVisible() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorAvailable = false;
        warning.fallbackActive = true;
        warning.fallbackAvailable = false;
        warning.fallbackBusy = false;
        waitForRendering(warning);
        compare(warning.warningVisible, true);
        compare(warning.warningTitle, "Service status unavailable");
        compare(warning.failedComponentCount, 0);

        warning.restartError = "The restart request was rejected.";
        const error = findChild(warning, "restartError");
        verify(error !== null);
        compare(error.visible, true);
        verify(error.text.includes("rejected"));
    }
}
