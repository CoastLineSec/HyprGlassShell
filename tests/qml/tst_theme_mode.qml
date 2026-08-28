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

    Component {
        id: appearanceWindowComponent

        Window {
            width: 900
            height: 1100
            visible: true

            property alias page: appearancePage

            Settings.AppearancePage {
                id: appearancePage

                anchors.fill: parent
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

    function test_themeAutomationIntegrationAndValidation() {
        const window = createTemporaryObject(
            appearanceWindowComponent, testCase
        );
        verify(window !== null);
        const page = window.page;
        page.shellAppearanceServiceAvailable = true;
        page.shellAppearanceMode = "dark";
        page.shellEffectiveAppearanceMode = "light";
        page.shellAppearanceAutomationSource = "schedule";
        page.shellAppearanceScheduleMode = "time";
        page.shellAppearanceDarkStartMinute = 20 * 60 + 30;
        page.shellAppearanceLightStartMinute = 6 * 60 + 30;
        page.shellAppearanceLocationSource = "manual";
        page.shellAppearanceHasLocation = true;
        page.shellAppearanceLatitude = 47.6062;
        page.shellAppearanceLongitude = -122.3321;
        page.hyprsunsetAvailable = true;
        page.nightLightAutomatic = true;
        waitForRendering(page);
        wait(0);

        const card = findChild(page, "themeAutomationCard");
        const desktop = findChild(
            page, "themeAutomationSource-desktop"
        );
        const schedule = findChild(
            page, "themeAutomationSource-schedule"
        );
        const nightLight = findChild(
            page, "themeAutomationSource-night-light"
        );
        verify(card !== null);
        verify(desktop !== null);
        verify(schedule !== null);
        verify(nightLight !== null);
        compare(card.visible, false);

        page.shellAppearanceMode = "automatic";
        wait(0);
        compare(card.visible, true);
        compare(card.source, "schedule");
        compare(card.scheduleMode, "time");
        compare(card.darkStartMinute, 20 * 60 + 30);
        compare(card.lightStartMinute, 6 * 60 + 30);
        compare(card.locationSource, "manual");
        compare(card.hasLocation, true);
        compare(card.latitude, 47.6062);
        compare(card.longitude, -122.3321);
        compare(card.effectiveMode, "light");
        compare(schedule.checked, true);
        compare(desktop.checked, false);
        compare(nightLight.enabled, true);
        verify(String(nightLight.Accessible.description).indexOf(
            "Night Light schedule"
        ) >= 0);
        compare(String(nightLight.Accessible.description).indexOf(
            "Hyprsunset schedule"
        ), -1);

        let request = null;
        page.shellAppearanceAutomationRequested.connect(function(
            source,
            scheduleMode,
            darkStartMinute,
            lightStartMinute,
            locationSource,
            hasLocation,
            latitude,
            longitude
        ) {
            request = [
                source,
                scheduleMode,
                darkStartMinute,
                lightStartMinute,
                locationSource,
                hasLocation,
                latitude,
                longitude
            ];
        });

        schedule.forceActiveFocus();
        tryCompare(schedule, "activeFocus", true);
        keyClick(Qt.Key_Left);
        tryCompare(desktop, "activeFocus", true);
        verify(request !== null);
        compare(request, [
            "desktop",
            "time",
            20 * 60 + 30,
            6 * 60 + 30,
            "manual",
            true,
            47.6062,
            -122.3321
        ]);

        request = null;
        desktop.clicked();
        verify(request !== null);
        compare(request, [
            "desktop",
            "time",
            20 * 60 + 30,
            6 * 60 + 30,
            "manual",
            true,
            47.6062,
            -122.3321
        ]);
        // The saved projection remains authoritative until Config1
        // publishes the accepted tuple.
        compare(card.source, "schedule");
        compare(schedule.checked, true);
        compare(desktop.checked, false);

        request = null;
        card.darkStartDraft = "06:30";
        card.lightStartDraft = "06:30";
        card.applyTimeDraft();
        compare(request, null);
        verify(card.timeValidationText.indexOf("different") >= 0);
        compare(card.darkStartMinute, 20 * 60 + 30);

        card.darkStartDraft = "21:15";
        card.lightStartDraft = "06:45";
        card.applyTimeDraft();
        verify(request !== null);
        compare(request, [
            "schedule",
            "time",
            21 * 60 + 15,
            6 * 60 + 45,
            "manual",
            true,
            47.6062,
            -122.3321
        ]);
        compare(card.darkStartMinute, 20 * 60 + 30);
        compare(card.lightStartMinute, 6 * 60 + 30);

        page.shellAppearanceScheduleMode = "location";
        page.shellAppearanceLocationSource = "geoclue";
        page.shellAppearanceAutomationStatus = "ready";
        wait(0);
        const geoclueStatus = findChild(
            page, "themeAutomationGeoClueStatus"
        );
        verify(geoclueStatus !== null);
        verify(geoclueStatus.visible);
        verify(String(geoclueStatus.text).indexOf("not copied") >= 0);
        compare(String(geoclueStatus.text).indexOf("47.6062"), -1);
        compare(String(geoclueStatus.text).indexOf("-122.3321"), -1);

        page.shellAppearanceAutomationSource = "desktop";
        page.shellAppearanceAutomationStatus = "desktop";
        wait(0);
        const automationStatus = findChild(
            page, "themeAutomationStatus"
        );
        verify(automationStatus !== null);
        compare(automationStatus.text,
                "Following the desktop appearance preference.");
        compare(automationStatus.text, card.normalizedStatusText());

        const transition = "2035-07-08T03:04:05Z";
        compare(card.displayNextTransition(transition),
                new Date(transition).toLocaleString(
                    Qt.locale(), Locale.ShortFormat
                ));

        page.shellAppearanceAutomationError = "Appearance schedule failed";
        page.nightLightSettingsError = "";
        wait(0);
        compare(card.errorText, "Appearance schedule failed");
        const scopedNightLightCard = findChild(page, "nightLightCard");
        verify(scopedNightLightCard !== null);
        compare(scopedNightLightCard.errorText, "");
        page.shellAppearanceAutomationError = "";

        page.nightLightAutomatic = false;
        wait(0);
        const unavailableNightLight = findChild(
            page, "themeAutomationSource-night-light"
        );
        verify(unavailableNightLight !== null);
        compare(unavailableNightLight.enabled, false);
        page.shellAppearanceMode = "light";
        wait(0);
        compare(card.visible, false);
        window.destroy();
    }

    function test_themeNightLightIntegrationAvailabilityAndValidation() {
        const window = createTemporaryObject(
            appearanceWindowComponent, testCase
        );
        verify(window !== null);
        const page = window.page;
        page.shellAppearanceServiceAvailable = true;
        page.nightLightEnabled = false;
        page.nightLightAutomatic = true;
        page.nightLightScheduleMode = "time";
        page.nightLightDarkStartMinute = 20 * 60;
        page.nightLightLightStartMinute = 6 * 60;
        page.nightLightLocationSource = "manual";
        page.nightLightHasLocation = true;
        page.nightLightLatitude = 35.6762;
        page.nightLightLongitude = 139.6503;
        page.nightLightTemperature = 4100;
        page.nightLightDayTemperature = 6700;
        page.nightLightGradual = false;
        page.hyprsunsetAvailable = false;
        waitForRendering(page);
        wait(0);

        const card = findChild(page, "nightLightCard");
        const enabledSwitch = findChild(
            page, "nightLightEnabledSwitch"
        );
        const availabilityError = findChild(page, "nightLightError");
        verify(card !== null);
        verify(enabledSwitch !== null);
        verify(availabilityError !== null);
        compare(card.visible, true);
        compare(card.nightLightEnabled, false);
        compare(card.automatic, true);
        compare(card.scheduleMode, "time");
        compare(card.darkStartMinute, 20 * 60);
        compare(card.lightStartMinute, 6 * 60);
        compare(card.locationSource, "manual");
        compare(card.hasLocation, true);
        compare(card.latitude, 35.6762);
        compare(card.longitude, 139.6503);
        compare(card.nightTemperature, 4100);
        compare(card.dayTemperature, 6700);
        compare(card.gradual, false);
        compare(enabledSwitch.enabled, false);
        compare(availabilityError.visible, true);
        verify(String(availabilityError.text).indexOf("hyprsunset") >= 0);

        page.hyprsunsetAvailable = true;
        wait(0);
        compare(enabledSwitch.enabled, true);
        compare(availabilityError.visible, false);
        const automaticSwitch = findChild(
            page, "nightLightAutomaticSwitch"
        );
        const timeMode = findChild(page, "nightLightScheduleMode-time");
        const locationMode = findChild(
            page, "nightLightScheduleMode-location"
        );
        verify(automaticSwitch !== null);
        verify(timeMode !== null);
        verify(locationMode !== null);
        compare(automaticSwitch.enabled, true);
        compare(timeMode.enabled, true);

        let request = null;
        page.nightLightSettingsRequested.connect(function(
            nightLightEnabled,
            automatic,
            scheduleMode,
            darkStartMinute,
            lightStartMinute,
            locationSource,
            hasLocation,
            latitude,
            longitude,
            nightTemperature,
            dayTemperature,
            gradual
        ) {
            request = [
                nightLightEnabled,
                automatic,
                scheduleMode,
                darkStartMinute,
                lightStartMinute,
                locationSource,
                hasLocation,
                latitude,
                longitude,
                nightTemperature,
                dayTemperature,
                gradual
            ];
        });

        // The schedule is intentionally editable while the actual color
        // filter is off, so appearance automation can follow it dry.
        timeMode.forceActiveFocus();
        tryCompare(timeMode, "activeFocus", true);
        keyClick(Qt.Key_Down);
        tryCompare(locationMode, "activeFocus", true);
        verify(request !== null);
        compare(request, [
            false,
            true,
            "location",
            20 * 60,
            6 * 60,
            "manual",
            true,
            35.6762,
            139.6503,
            4100,
            6700,
            false
        ]);
        compare(card.scheduleMode, "time");

        request = null;
        enabledSwitch.clicked();
        verify(request !== null);
        compare(request, [
            true,
            true,
            "time",
            20 * 60,
            6 * 60,
            "manual",
            true,
            35.6762,
            139.6503,
            4100,
            6700,
            false
        ]);
        compare(card.nightLightEnabled, false);
        compare(enabledSwitch.checked, false);

        page.nightLightEnabled = true;
        wait(0);
        compare(card.nightLightEnabled, true);
        compare(enabledSwitch.checked, true);
        compare(automaticSwitch.checked, true);
        request = null;
        automaticSwitch.clicked();
        verify(request !== null);
        compare(request[0], true);
        compare(request[1], false);
        compare(request.slice(2), [
            "time",
            20 * 60,
            6 * 60,
            "manual",
            true,
            35.6762,
            139.6503,
            4100,
            6700,
            false
        ]);
        compare(card.automatic, true);
        compare(automaticSwitch.checked, true);

        page.nightLightScheduleMode = "location";
        page.nightLightLocationSource = "geoclue";
        page.nightLightStatus = "disabled";
        page.nightLightNextTransition = "";
        page.nightLightSunrise = "";
        page.nightLightSunset = "";
        wait(0);
        const geoclueStatus = findChild(page, "nightLightGeoClueStatus");
        verify(geoclueStatus !== null);
        verify(geoclueStatus.visible);
        compare(card.solarProjectionAvailable, false);
        verify(String(geoclueStatus.text).indexOf("Waiting") >= 0);
        compare(String(geoclueStatus.text).indexOf("not copied"), -1);

        page.nightLightSunrise = "2035-07-08T09:30:00Z";
        wait(0);
        compare(card.solarProjectionAvailable, true);
        verify(String(geoclueStatus.text).indexOf("not copied") >= 0);
        compare(String(geoclueStatus.text).indexOf("Waiting"), -1);
        compare(String(geoclueStatus.text).indexOf("35.6762"), -1);
        compare(String(geoclueStatus.text).indexOf("139.6503"), -1);
        compare(findChild(page, "nightLightLocateButton"), null);

        page.nightLightStatus = "hyprsunset exited unexpectedly";
        wait(0);
        const statusCard = findChild(page, "nightLightStatusCard");
        const statusLabel = findChild(page, "nightLightStatus");
        verify(statusCard !== null);
        verify(statusLabel !== null);
        compare(card.runtimeFailure, true);
        compare(statusLabel.Accessible.role, Accessible.AlertMessage);

        page.nightLightStatus = "night";
        page.nightLightNextTransition = "2035-07-08T03:04:05Z";
        wait(0);
        compare(card.runtimeFailure, false);
        compare(statusLabel.Accessible.role, Accessible.StaticText);
        const nextTransition = findChild(
            page, "nightLightNextTransition"
        );
        verify(nextTransition !== null);
        compare(nextTransition.text,
                card.displayNextTransition(page.nightLightNextTransition));
        compare(nextTransition.text,
                new Date(page.nightLightNextTransition).toLocaleString(
                    Qt.locale(), Locale.ShortFormat
                ));
        verify(nextTransition.text
            !== card.displayTime(page.nightLightNextTransition));

        page.nightLightSettingsError = "Night Light save failed";
        page.shellAppearanceAutomationError = "";
        wait(0);
        compare(card.errorText, "Night Light save failed");
        const scopedAutomationCard = findChild(
            page, "themeAutomationCard"
        );
        verify(scopedAutomationCard !== null);
        compare(scopedAutomationCard.errorText, "");
        page.nightLightSettingsError = "";

        page.nightLightScheduleMode = "time";
        page.nightLightLocationSource = "manual";
        wait(0);

        const darkStartField = findChild(
            page, "nightLightDarkStartField"
        );
        verify(darkStartField !== null);
        request = null;
        darkStartField.text = "06:00";
        card.commitTime(true);
        compare(request, null);
        verify(card.darkTimeErrorText.indexOf("different") >= 0);
        compare(card.darkStartMinute, 20 * 60);

        darkStartField.text = "21:15";
        card.commitTime(true);
        verify(request !== null);
        compare(request[0], true);
        compare(request[1], true);
        compare(request[2], "time");
        compare(request[3], 21 * 60 + 15);
        compare(request[4], 6 * 60);
        compare(request.slice(5), [
            "manual",
            true,
            35.6762,
            139.6503,
            4100,
            6700,
            false
        ]);
        compare(card.darkStartMinute, 20 * 60);
        compare(darkStartField.text, "20:00");
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
