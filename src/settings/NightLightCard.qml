pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Frame {
    id: root

    // `enabled` belongs to Item and disables the whole visual subtree. Keep
    // the persisted Night Light switch separate so an off filter can still
    // be turned back on.
    property bool nightLightEnabled: false
    property bool automatic: false
    property string scheduleMode: "time"
    property int darkStartMinute: 18 * 60
    property int lightStartMinute: 6 * 60
    property string locationSource: "manual"
    property bool hasLocation: false
    property real latitude: 0
    property real longitude: 0
    property int nightTemperature: 3500
    property int dayTemperature: 6500
    property bool gradual: false

    property string status: ""
    property int currentTemperature: 0
    property string nextTransition: ""
    property string sunrise: ""
    property string sunset: ""

    property bool hyprsunsetAvailable: false
    property bool serviceAvailable: false
    property bool busy: false
    property string errorText: ""

    signal settingsRequested(
        bool nightLightEnabled,
        bool automatic,
        string scheduleMode,
        int darkStartMinute,
        int lightStartMinute,
        string locationSource,
        bool hasLocation,
        real latitude,
        real longitude,
        int nightTemperature,
        int dayTemperature,
        bool gradual
    )

    readonly property int minimumTargetSize: 44
    readonly property bool compact: width < 560
    readonly property bool requestAvailable:
        root.enabled && root.serviceAvailable
        && root.hyprsunsetAvailable && !root.busy
    readonly property bool settingsAvailable:
        root.requestAvailable && root.nightLightEnabled
    readonly property bool scheduleAvailable: root.requestAvailable
    readonly property string normalizedStatus:
        root.status.trim().toLowerCase()
    readonly property bool runtimeFailure: {
        const known = [
            "", "off", "disabled", "day", "daytime", "night",
            "applying", "pending", "waiting-location", "external-daemon",
            "ready", "active", "no-transition"
        ];
        return root.nightLightEnabled
            && (root.normalizedStatus === "failed"
                || root.normalizedStatus === "unavailable"
                || !known.includes(root.normalizedStatus));
    }
    readonly property bool runtimeWarning:
        root.normalizedStatus === "waiting-location"
        || root.normalizedStatus === "external-daemon"
    readonly property bool solarProjectionAvailable:
        root.nextTransition.trim().length > 0
        || root.sunrise.trim().length > 0
        || root.sunset.trim().length > 0
    readonly property var scheduleModes: [
        {
            value: "time",
            label: qsTr("By time"),
            description: qsTr("Use fixed light and dark times")
        },
        {
            value: "location",
            label: qsTr("Sunrise & sunset"),
            description: qsTr("Follow the sun at your location")
        }
    ]
    readonly property var locationSources: [
        {
            value: "geoclue",
            label: qsTr("Current location"),
            description: qsTr("Use the system location service")
        },
        {
            value: "manual",
            label: qsTr("Manual coordinates"),
            description: qsTr("Enter latitude and longitude")
        }
    ]

    property string darkTimeErrorText: ""
    property string lightTimeErrorText: ""
    property string latitudeErrorText: ""
    property string longitudeErrorText: ""
    property string actionErrorText: ""
    property bool locationDraftDirty: false

    readonly property bool authoritativeTimesValid:
        root.validMinuteOfDay(root.darkStartMinute)
        && root.validMinuteOfDay(root.lightStartMinute)
        && root.darkStartMinute !== root.lightStartMinute
    readonly property bool authoritativeTemperaturesValid:
        root.validTemperaturePair(
            root.nightTemperature, root.dayTemperature
        )
    readonly property bool authoritativeLocationValid:
        !root.hasLocation
        || (root.validLatitude(root.latitude)
            && root.validLongitude(root.longitude))
    readonly property string projectionErrorText: {
        if (root.scheduleMode !== "time"
                && root.scheduleMode !== "location") {
            return qsTr("The saved schedule mode is not valid.");
        }
        if (root.locationSource !== "manual"
                && root.locationSource !== "geoclue") {
            return qsTr("The saved location source is not valid.");
        }
        if (!root.authoritativeTimesValid) {
            return qsTr("The saved light and dark times must be different valid times.");
        }
        if (!root.authoritativeTemperaturesValid) {
            return qsTr("The saved temperatures are outside the supported range.");
        }
        if (!root.authoritativeLocationValid) {
            return qsTr("The saved location is not valid.");
        }
        return "";
    }
    readonly property string availabilityErrorText: {
        if (root.errorText.length > 0)
            return root.errorText;
        if (!root.serviceAvailable) {
            return qsTr("The settings service is unavailable. Night Light settings cannot be changed right now.");
        }
        if (!root.hyprsunsetAvailable) {
            return qsTr("hyprsunset is not available. Install it to use Night Light.");
        }
        if (root.actionErrorText.length > 0)
            return root.actionErrorText;
        return root.projectionErrorText;
    }

    objectName: "nightLightCard"
    implicitWidth: 560
    Layout.fillWidth: true
    padding: 18

    // Keep native controls synchronized with the shell mode even when the
    // platform toolkit palette has a different light/dark preference.
    palette.window: ShellTheme.card
    palette.windowText: ShellTheme.onSurface
    palette.base: ShellTheme.floating
    palette.alternateBase: ShellTheme.card
    palette.text: ShellTheme.onSurface
    palette.placeholderText: ShellTheme.onSurfaceMuted
    palette.button: ShellTheme.floating
    palette.buttonText: ShellTheme.onSurface
    palette.light: ShellTheme.outlineStrong
    palette.midlight: ShellTheme.track
    palette.mid: ShellTheme.outline
    palette.dark: ShellTheme.outlineStrong
    palette.shadow: ShellTheme.shadow
    palette.highlight: ShellTheme.primary
    palette.highlightedText: ShellTheme.onPrimary
    palette.toolTipBase: ShellTheme.floating
    palette.toolTipText: ShellTheme.onSurface

    background: Rectangle {
        color: ShellTheme.card
        radius: 16
        border.color: ShellTheme.outline
    }

    function validMinuteOfDay(value) {
        return Number.isInteger(value) && value >= 0 && value < 24 * 60;
    }

    function validLatitude(value) {
        return Number.isFinite(value) && value >= -90 && value <= 90;
    }

    function validLongitude(value) {
        return Number.isFinite(value) && value >= -180 && value <= 180;
    }

    function validTemperaturePair(nightValue, dayValue) {
        return Number.isInteger(nightValue)
            && Number.isInteger(dayValue)
            && nightValue >= 2500 && nightValue <= 6000
            && dayValue >= 2500 && dayValue <= 10000
            && nightValue <= dayValue;
    }

    function minuteText(value) {
        if (!root.validMinuteOfDay(value))
            return "";
        const hours = Math.floor(value / 60);
        const minutes = value % 60;
        return (hours < 10 ? "0" : "") + hours
            + ":" + (minutes < 10 ? "0" : "") + minutes;
    }

    function parseClock(rawText) {
        const text = String(rawText || "").trim();
        if (text.length === 0) {
            return {
                valid: false,
                error: qsTr("Enter a time.")
            };
        }
        const match = /^(\d{1,2})(?::(\d{1,2}))?$/.exec(text);
        if (!match) {
            return {
                valid: false,
                error: qsTr("Use a 24-hour time such as 18:30.")
            };
        }
        const hour = Number(match[1]);
        const minute = match[2] === undefined ? 0 : Number(match[2]);
        if (!Number.isInteger(hour) || !Number.isInteger(minute)
                || hour < 0 || hour > 23
                || minute < 0 || minute > 59) {
            return {
                valid: false,
                error: qsTr("Enter a time from 00:00 through 23:59.")
            };
        }
        return {
            valid: true,
            minute: hour * 60 + minute,
            error: ""
        };
    }

    function parseCoordinate(rawText, latitudeValue) {
        const text = String(rawText || "").trim();
        if (text.length === 0) {
            return {
                valid: false,
                error: latitudeValue
                    ? qsTr("Enter a latitude.")
                    : qsTr("Enter a longitude.")
            };
        }
        const value = Number(text);
        if (!Number.isFinite(value)) {
            return {
                valid: false,
                error: qsTr("Enter a finite number.")
            };
        }
        if (latitudeValue && !root.validLatitude(value)) {
            return {
                valid: false,
                error: qsTr("Latitude must be between −90 and 90.")
            };
        }
        if (!latitudeValue && !root.validLongitude(value)) {
            return {
                valid: false,
                error: qsTr("Longitude must be between −180 and 180.")
            };
        }
        return {
            valid: true,
            value: value,
            error: ""
        };
    }

    function coordinateText(value) {
        if (!Number.isFinite(value))
            return "";
        return Number(value).toFixed(5).replace(/\.?0+$/, "");
    }

    function changedValue(changes, name, currentValue) {
        return changes && changes[name] !== undefined
            ? changes[name] : currentValue;
    }

    function requestSettings(changes) {
        if (!root.requestAvailable)
            return;

        const requestedEnabled = Boolean(root.changedValue(
            changes, "nightLightEnabled", root.nightLightEnabled
        ));
        const requestedAutomatic = Boolean(root.changedValue(
            changes, "automatic", root.automatic
        ));
        const requestedScheduleMode = String(root.changedValue(
            changes, "scheduleMode", root.scheduleMode
        ));
        const requestedDarkStart = Number(root.changedValue(
            changes, "darkStartMinute", root.darkStartMinute
        ));
        const requestedLightStart = Number(root.changedValue(
            changes, "lightStartMinute", root.lightStartMinute
        ));
        const requestedLocationSource = String(root.changedValue(
            changes, "locationSource", root.locationSource
        ));
        const requestedHasLocation = Boolean(root.changedValue(
            changes, "hasLocation", root.hasLocation
        ));
        const requestedLatitude = Number(root.changedValue(
            changes, "latitude", root.latitude
        ));
        const requestedLongitude = Number(root.changedValue(
            changes, "longitude", root.longitude
        ));
        const requestedNightTemperature = Number(root.changedValue(
            changes, "nightTemperature", root.nightTemperature
        ));
        const requestedDayTemperature = Number(root.changedValue(
            changes, "dayTemperature", root.dayTemperature
        ));
        const requestedGradual = Boolean(root.changedValue(
            changes, "gradual", root.gradual
        ));

        if (requestedScheduleMode !== "time"
                && requestedScheduleMode !== "location") {
            root.actionErrorText = qsTr("Choose a valid schedule mode.");
            return;
        }
        if (requestedLocationSource !== "manual"
                && requestedLocationSource !== "geoclue") {
            root.actionErrorText = qsTr("Choose a valid location source.");
            return;
        }
        if (!root.validMinuteOfDay(requestedDarkStart)
                || !root.validMinuteOfDay(requestedLightStart)
                || requestedDarkStart === requestedLightStart) {
            root.actionErrorText = qsTr("Light and dark times must be different valid times.");
            return;
        }
        if (!root.validTemperaturePair(
                requestedNightTemperature, requestedDayTemperature)) {
            root.actionErrorText = qsTr("Choose valid day and night temperatures.");
            return;
        }
        if (requestedHasLocation
                && (!root.validLatitude(requestedLatitude)
                    || !root.validLongitude(requestedLongitude))) {
            root.actionErrorText = qsTr("Choose a valid location.");
            return;
        }

        root.actionErrorText = "";
        root.settingsRequested(
            requestedEnabled,
            requestedAutomatic,
            requestedScheduleMode,
            requestedDarkStart,
            requestedLightStart,
            requestedLocationSource,
            requestedHasLocation,
            requestedLatitude,
            requestedLongitude,
            requestedNightTemperature,
            requestedDayTemperature,
            requestedGradual
        );
    }

    function validateTimeDraft(darkField) {
        const field = darkField ? darkStartField : lightStartField;
        const result = root.parseClock(field.text);
        let error = result.error;
        if (result.valid) {
            const otherValue = darkField
                ? root.lightStartMinute : root.darkStartMinute;
            if (result.minute === otherValue) {
                error = qsTr("Light and dark times must be different.");
            }
        }
        if (darkField)
            root.darkTimeErrorText = error;
        else
            root.lightTimeErrorText = error;
        return result.valid && error.length === 0 ? result : null;
    }

    function commitTime(darkField) {
        const result = root.validateTimeDraft(darkField);
        if (!result)
            return;
        const field = darkField ? darkStartField : lightStartField;
        const currentValue = darkField
            ? root.darkStartMinute : root.lightStartMinute;
        if (result.minute !== currentValue) {
            const changes = {};
            changes[darkField
                ? "darkStartMinute" : "lightStartMinute"] = result.minute;
            root.requestSettings(changes);
        }
        field.text = root.minuteText(currentValue);
    }

    function validateLocationDraft() {
        const parsedLatitude = root.parseCoordinate(
            latitudeField.text, true
        );
        const parsedLongitude = root.parseCoordinate(
            longitudeField.text, false
        );
        root.latitudeErrorText = parsedLatitude.error;
        root.longitudeErrorText = parsedLongitude.error;
        return parsedLatitude.valid && parsedLongitude.valid
            ? {
                valid: true,
                latitude: parsedLatitude.value,
                longitude: parsedLongitude.value
            }
            : null;
    }

    function commitLocation() {
        const result = root.validateLocationDraft();
        if (!result)
            return;
        root.requestSettings({
            hasLocation: true,
            latitude: result.latitude,
            longitude: result.longitude
        });
        root.locationDraftDirty = false;
        latitudeField.text = root.coordinateText(root.latitude);
        longitudeField.text = root.coordinateText(root.longitude);
    }

    function displayTime(value) {
        const text = String(value || "").trim();
        if (text.length === 0)
            return "";
        const date = new Date(text);
        if (!Number.isNaN(date.getTime()))
            return date.toLocaleTimeString(Qt.locale(), Locale.ShortFormat);
        return text;
    }

    function displayNextTransition(value) {
        const text = String(value || "").trim();
        if (text.length === 0)
            return "";
        const date = new Date(text);
        if (!Number.isNaN(date.getTime()))
            return date.toLocaleString(Qt.locale(), Locale.ShortFormat);
        return text;
    }

    function statusText() {
        switch (root.normalizedStatus) {
        case "off":
        case "disabled":
            return qsTr("Off");
        case "day":
        case "daytime":
            return qsTr("Daytime");
        case "night":
            return qsTr("Night");
        case "applying":
        case "pending":
            return qsTr("Applying…");
        case "waiting-location":
            return qsTr("Waiting for a usable location");
        case "external-daemon":
            return qsTr("Managed by another hyprsunset session");
        case "failed":
            return qsTr("Could not apply Night Light");
        case "unavailable":
            return qsTr("Unavailable");
        case "no-transition":
            return qsTr("No solar transition today");
        default:
            if (root.status.trim().length > 0)
                return root.status.trim();
            return root.nightLightEnabled ? qsTr("Active") : qsTr("Off");
        }
    }

    function moveScheduleFocus(index, offset) {
        const targetIndex = (index + offset
            + scheduleModeRepeater.count) % scheduleModeRepeater.count;
        const target = scheduleModeRepeater.itemAt(targetIndex);
        if (!target)
            return;
        target.forceActiveFocus();
        const choice = root.scheduleModes[targetIndex];
        if (target.enabled
                && choice.value !== root.scheduleMode) {
            root.requestSettings({
                scheduleMode: choice.value
            });
        }
    }

    function moveLocationSourceFocus(index, offset) {
        const targetIndex = (index + offset
            + locationSourceRepeater.count) % locationSourceRepeater.count;
        const target = locationSourceRepeater.itemAt(targetIndex);
        if (!target)
            return;
        target.forceActiveFocus();
        const choice = root.locationSources[targetIndex];
        if (target.enabled
                && choice.value !== root.locationSource) {
            root.requestSettings({
                locationSource: choice.value
            });
        }
    }

    onDarkStartMinuteChanged: {
        if (!darkStartField.activeFocus)
            darkStartField.text = root.minuteText(root.darkStartMinute);
        root.darkTimeErrorText = "";
    }
    onLightStartMinuteChanged: {
        if (!lightStartField.activeFocus)
            lightStartField.text = root.minuteText(root.lightStartMinute);
        root.lightTimeErrorText = "";
    }
    onLatitudeChanged: {
        if (!root.locationDraftDirty && !latitudeField.activeFocus)
            latitudeField.text = root.coordinateText(root.latitude);
    }
    onLongitudeChanged: {
        if (!root.locationDraftDirty && !longitudeField.activeFocus)
            longitudeField.text = root.coordinateText(root.longitude);
    }

    contentItem: ColumnLayout {
        spacing: 16

        GridLayout {
            Layout.fillWidth: true
            columns: root.compact ? 1 : 2
            columnSpacing: 16
            rowSpacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Night Light")
                    color: ShellTheme.onSurface
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Warm the display after dark to reduce cool blue light.")
                    color: ShellTheme.onSurfaceMuted
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            Switch {
                objectName: "nightLightEnabledSwitch"
                Layout.alignment: root.compact
                    ? Qt.AlignLeft : Qt.AlignRight | Qt.AlignVCenter
                implicitHeight: root.minimumTargetSize
                text: root.nightLightEnabled
                    ? qsTr("Enabled") : qsTr("Disabled")
                checked: root.nightLightEnabled
                checkable: false
                enabled: root.requestAvailable
                palette.text: ShellTheme.onSurface
                palette.windowText: ShellTheme.onSurface
                focusPolicy: Qt.StrongFocus
                Accessible.name: qsTr("Night Light")
                Accessible.description: root.nightLightEnabled
                    ? qsTr("Night Light is enabled.")
                    : qsTr("Night Light is disabled.")
                Accessible.checked: root.nightLightEnabled

                onClicked: root.requestSettings({
                    nightLightEnabled: !root.nightLightEnabled
                })
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.busy
            spacing: 10

            BusyIndicator {
                implicitWidth: 24
                implicitHeight: 24
                running: parent.visible
                Accessible.ignored: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Applying Night Light settings…")
                color: ShellTheme.onSurfaceMuted
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Accessible.name: text
            }
        }

        Label {
            objectName: "nightLightError"
            Layout.fillWidth: true
            visible: root.availabilityErrorText.length > 0
            text: root.availabilityErrorText
            color: ShellTheme.onErrorContainer
            font.pixelSize: 12
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            leftPadding: 10
            rightPadding: 10
            topPadding: 8
            bottomPadding: 8
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text

            background: Rectangle {
                radius: 8
                color: ShellTheme.errorContainer
                border.color: ShellTheme.errorOutline
            }
        }

        Frame {
            Layout.fillWidth: true
            padding: 14
            enabled: root.settingsAvailable
            opacity: enabled ? 1 : 0.62

            background: Rectangle {
                color: ShellTheme.floating
                radius: 12
                border.color: ShellTheme.outline
            }

            contentItem: ColumnLayout {
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Temperature")
                    color: ShellTheme.onSurface
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                GridLayout {
                    id: temperatureGrid

                    Layout.fillWidth: true
                    columns: root.compact ? 1 : 2
                    columnSpacing: 18
                    rowSpacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.columnSpan: root.automatic
                            ? 1 : temperatureGrid.columns
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: root.automatic
                                    ? qsTr("Night") : qsTr("Filter")
                                color: ShellTheme.onSurface
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }

                            Label {
                                objectName: "nightLightNightTemperatureValue"
                                text: qsTr("%1 K").arg(
                                    nightTemperatureSlider.pressed
                                        ? nightTemperatureSlider.draftValue
                                        : root.nightTemperature
                                )
                                color: ShellTheme.onSurfaceMuted
                                font.pixelSize: 12
                                Accessible.ignored: true
                            }
                        }

                        Slider {
                            id: nightTemperatureSlider

                            property int draftValue: root.nightTemperature

                            objectName: "nightLightNightTemperatureSlider"
                            Layout.fillWidth: true
                            implicitHeight: root.minimumTargetSize
                            from: 2500
                            to: 6000
                            stepSize: 100
                            snapMode: Slider.SnapAlways
                            enabled: root.authoritativeTemperaturesValid
                            Accessible.name: root.automatic
                                ? qsTr("Night temperature")
                                : qsTr("Filter temperature")
                            Accessible.description: qsTr("Lower values are warmer.")

                            Binding {
                                target: nightTemperatureSlider
                                property: "value"
                                value: root.nightTemperature
                                when: !nightTemperatureSlider.pressed
                                restoreMode: Binding.RestoreNone
                            }

                            onMoved: {
                                draftValue = Math.round(value / 100) * 100;
                                if (!pressed && draftValue
                                        !== root.nightTemperature) {
                                    root.requestSettings({
                                        nightTemperature: draftValue,
                                        dayTemperature: Math.max(
                                            root.dayTemperature, draftValue
                                        )
                                    });
                                }
                            }
                            onPressedChanged: {
                                if (pressed) {
                                    draftValue = root.nightTemperature;
                                } else if (draftValue
                                        !== root.nightTemperature) {
                                    root.requestSettings({
                                        nightTemperature: draftValue,
                                        dayTemperature: Math.max(
                                            root.dayTemperature, draftValue
                                        )
                                    });
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Lower is warmer")
                            color: ShellTheme.onSurfaceMuted
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        visible: root.automatic
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Day")
                                color: ShellTheme.onSurface
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }

                            Label {
                                objectName: "nightLightDayTemperatureValue"
                                text: qsTr("%1 K").arg(
                                    dayTemperatureSlider.pressed
                                        ? dayTemperatureSlider.draftValue
                                        : root.dayTemperature
                                )
                                color: ShellTheme.onSurfaceMuted
                                font.pixelSize: 12
                                Accessible.ignored: true
                            }
                        }

                        Slider {
                            id: dayTemperatureSlider

                            property int draftValue: root.dayTemperature

                            objectName: "nightLightDayTemperatureSlider"
                            Layout.fillWidth: true
                            implicitHeight: root.minimumTargetSize
                            from: Math.max(2500, root.nightTemperature)
                            to: 10000
                            stepSize: 100
                            snapMode: Slider.SnapAlways
                            enabled: root.authoritativeTemperaturesValid
                            Accessible.name: qsTr("Day temperature")
                            Accessible.description: qsTr("Applied outside night hours.")

                            Binding {
                                target: dayTemperatureSlider
                                property: "value"
                                value: root.dayTemperature
                                when: !dayTemperatureSlider.pressed
                                restoreMode: Binding.RestoreNone
                            }

                            onMoved: {
                                draftValue = Math.round(value / 100) * 100;
                                if (!pressed && draftValue
                                        !== root.dayTemperature) {
                                    root.requestSettings({
                                        dayTemperature: draftValue
                                    });
                                }
                            }
                            onPressedChanged: {
                                if (pressed) {
                                    draftValue = root.dayTemperature;
                                } else if (draftValue
                                        !== root.dayTemperature) {
                                    root.requestSettings({
                                        dayTemperature: draftValue
                                    });
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Applied outside night hours")
                            color: ShellTheme.onSurfaceMuted
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            padding: 14
            enabled: root.scheduleAvailable
            opacity: enabled ? 1 : 0.62

            background: Rectangle {
                color: ShellTheme.floating
                radius: 12
                border.color: ShellTheme.outline
            }

            contentItem: ColumnLayout {
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Automatic schedule")
                            color: ShellTheme.onSurface
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Change temperature by time of day or the sun.")
                            color: ShellTheme.onSurfaceMuted
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }

                    Switch {
                        objectName: "nightLightAutomaticSwitch"
                        implicitHeight: root.minimumTargetSize
                        checked: root.automatic
                        checkable: false
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: qsTr("Automatic schedule")
                        Accessible.checked: root.automatic

                        onClicked: root.requestSettings({
                            automatic: !root.automatic
                        })
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    visible: root.automatic
                    columns: root.compact ? 1 : 2
                    columnSpacing: 10
                    rowSpacing: 10

                    Repeater {
                        id: scheduleModeRepeater
                        model: root.scheduleModes

                        delegate: AbstractButton {
                            id: scheduleModeButton

                            required property int index
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            implicitHeight: Math.max(
                                64, root.minimumTargetSize
                            )
                            checkable: false
                            checked: root.scheduleMode
                                === scheduleModeButton.modelData.value
                            focusPolicy: Qt.StrongFocus
                            hoverEnabled: true
                            objectName: "nightLightScheduleMode-"
                                + scheduleModeButton.modelData.value
                            Accessible.role: Accessible.RadioButton
                            Accessible.name:
                                scheduleModeButton.modelData.label
                            Accessible.description:
                                scheduleModeButton.modelData.description
                            Accessible.checked: checked

                            onClicked: {
                                if (!checked) {
                                    root.requestSettings({
                                        scheduleMode:
                                            scheduleModeButton.modelData.value
                                    });
                                }
                            }
                            Keys.onLeftPressed: event => {
                                root.moveScheduleFocus(
                                    scheduleModeButton.index, -1
                                );
                                event.accepted = true;
                            }
                            Keys.onRightPressed: event => {
                                root.moveScheduleFocus(
                                    scheduleModeButton.index, 1
                                );
                                event.accepted = true;
                            }
                            Keys.onUpPressed: event => {
                                root.moveScheduleFocus(
                                    scheduleModeButton.index, -1
                                );
                                event.accepted = true;
                            }
                            Keys.onDownPressed: event => {
                                root.moveScheduleFocus(
                                    scheduleModeButton.index, 1
                                );
                                event.accepted = true;
                            }

                            background: Rectangle {
                                radius: 10
                                color: scheduleModeButton.down
                                    ? ShellTheme.surfacePressed
                                    : scheduleModeButton.hovered
                                        ? ShellTheme.surfaceHover
                                        : ShellTheme.card
                                border.width:
                                    scheduleModeButton.activeFocus ? 3
                                    : scheduleModeButton.checked ? 2 : 1
                                border.color:
                                    scheduleModeButton.activeFocus
                                    ? ShellTheme.onSurface
                                    : scheduleModeButton.checked
                                        ? ShellTheme.primary
                                        : ShellTheme.outline
                            }

                            contentItem: ColumnLayout {
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: scheduleModeButton.modelData.label
                                    color: ShellTheme.onSurface
                                    font.pixelSize: 13
                                    font.weight:
                                        scheduleModeButton.checked
                                        ? Font.DemiBold : Font.Medium
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text:
                                        scheduleModeButton.modelData.description
                                    color: ShellTheme.onSurfaceMuted
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    visible: root.automatic
                        && root.scheduleMode === "time"
                    columns: root.compact ? 1 : 2
                    columnSpacing: 14
                    rowSpacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 5

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Dark from")
                            color: ShellTheme.onSurface
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }

                        TextField {
                            id: darkStartField

                            objectName: "nightLightDarkStartField"
                            Layout.fillWidth: true
                            implicitHeight: root.minimumTargetSize
                            text: root.minuteText(root.darkStartMinute)
                            placeholderText: qsTr("18:00")
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            selectByMouse: true
                            Accessible.name: qsTr("Dark from")
                            Accessible.description:
                                qsTr("24-hour time, for example 18:30.")

                            onTextEdited: root.validateTimeDraft(true)
                            onAccepted: root.commitTime(true)
                            onEditingFinished: root.commitTime(true)
                            Keys.onEscapePressed: event => {
                                text = root.minuteText(
                                    root.darkStartMinute
                                );
                                root.darkTimeErrorText = "";
                                event.accepted = true;
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.darkTimeErrorText.length > 0
                            text: root.darkTimeErrorText
                            color: ShellTheme.error
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: text
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 5

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Light from")
                            color: ShellTheme.onSurface
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }

                        TextField {
                            id: lightStartField

                            objectName: "nightLightLightStartField"
                            Layout.fillWidth: true
                            implicitHeight: root.minimumTargetSize
                            text: root.minuteText(root.lightStartMinute)
                            placeholderText: qsTr("06:00")
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            selectByMouse: true
                            Accessible.name: qsTr("Light from")
                            Accessible.description:
                                qsTr("24-hour time, for example 06:30.")

                            onTextEdited: root.validateTimeDraft(false)
                            onAccepted: root.commitTime(false)
                            onEditingFinished: root.commitTime(false)
                            Keys.onEscapePressed: event => {
                                text = root.minuteText(
                                    root.lightStartMinute
                                );
                                root.lightTimeErrorText = "";
                                event.accepted = true;
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.lightTimeErrorText.length > 0
                            text: root.lightTimeErrorText
                            color: ShellTheme.error
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: text
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.automatic
                        && root.scheduleMode === "location"
                    spacing: 12

                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.compact ? 1 : 2
                        columnSpacing: 10
                        rowSpacing: 10

                        Repeater {
                            id: locationSourceRepeater
                            model: root.locationSources

                            delegate: AbstractButton {
                                id: locationSourceButton

                                required property int index
                                required property var modelData

                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                implicitHeight: Math.max(
                                    64, root.minimumTargetSize
                                )
                                checkable: false
                                checked: root.locationSource
                                    === locationSourceButton.modelData.value
                                focusPolicy: Qt.StrongFocus
                                hoverEnabled: true
                                objectName: "nightLightLocationSource-"
                                    + locationSourceButton.modelData.value
                                Accessible.role: Accessible.RadioButton
                                Accessible.name:
                                    locationSourceButton.modelData.label
                                Accessible.description:
                                    locationSourceButton.modelData.description
                                Accessible.checked: checked

                                onClicked: {
                                    if (!checked) {
                                        root.requestSettings({
                                            locationSource:
                                                locationSourceButton
                                                    .modelData.value
                                        });
                                    }
                                }
                                Keys.onLeftPressed: event => {
                                    root.moveLocationSourceFocus(
                                        locationSourceButton.index, -1
                                    );
                                    event.accepted = true;
                                }
                                Keys.onRightPressed: event => {
                                    root.moveLocationSourceFocus(
                                        locationSourceButton.index, 1
                                    );
                                    event.accepted = true;
                                }
                                Keys.onUpPressed: event => {
                                    root.moveLocationSourceFocus(
                                        locationSourceButton.index, -1
                                    );
                                    event.accepted = true;
                                }
                                Keys.onDownPressed: event => {
                                    root.moveLocationSourceFocus(
                                        locationSourceButton.index, 1
                                    );
                                    event.accepted = true;
                                }

                                background: Rectangle {
                                    radius: 10
                                    color: locationSourceButton.down
                                        ? ShellTheme.surfacePressed
                                        : locationSourceButton.hovered
                                            ? ShellTheme.surfaceHover
                                            : ShellTheme.card
                                    border.width:
                                        locationSourceButton.activeFocus ? 3
                                        : locationSourceButton.checked
                                            ? 2 : 1
                                    border.color:
                                        locationSourceButton.activeFocus
                                        ? ShellTheme.onSurface
                                        : locationSourceButton.checked
                                            ? ShellTheme.primary
                                            : ShellTheme.outline
                                }

                                contentItem: ColumnLayout {
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text:
                                            locationSourceButton.modelData.label
                                        color: ShellTheme.onSurface
                                        font.pixelSize: 13
                                        font.weight:
                                            locationSourceButton.checked
                                            ? Font.DemiBold : Font.Medium
                                        wrapMode: Text.Wrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: locationSourceButton
                                            .modelData.description
                                        color: ShellTheme.onSurfaceMuted
                                        font.pixelSize: 11
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.locationSource === "geoclue"
                        spacing: 8

                        Label {
                            objectName: "nightLightGeoClueStatus"
                            Layout.fillWidth: true
                            text: root.solarProjectionAvailable
                                ? qsTr("GeoClue supplies location directly to the local scheduling service. Its coordinates are not copied into the manual fields.")
                                : qsTr("Waiting for the system location service. Check GeoClue permission if this does not resolve.")
                            color: ShellTheme.onSurfaceMuted
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.locationSource === "manual"
                        spacing: 10

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.compact ? 1 : 2
                            columnSpacing: 14
                            rowSpacing: 10

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 5

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Latitude")
                                    color: ShellTheme.onSurface
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                }

                                TextField {
                                    id: latitudeField

                                    objectName: "nightLightLatitudeField"
                                    Layout.fillWidth: true
                                    implicitHeight: root.minimumTargetSize
                                    text: root.coordinateText(root.latitude)
                                    placeholderText: qsTr("40.7128")
                                    inputMethodHints:
                                        Qt.ImhFormattedNumbersOnly
                                    selectByMouse: true
                                    Accessible.name: qsTr("Latitude")
                                    Accessible.description:
                                        qsTr("A number from minus 90 through 90.")

                                    onTextEdited: {
                                        root.locationDraftDirty = true;
                                        root.latitudeErrorText = root
                                            .parseCoordinate(text, true).error;
                                    }
                                    onAccepted: root.commitLocation()
                                    Keys.onEscapePressed: event => {
                                        text = root.coordinateText(
                                            root.latitude
                                        );
                                        root.latitudeErrorText = "";
                                        root.locationDraftDirty = false;
                                        event.accepted = true;
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible:
                                        root.latitudeErrorText.length > 0
                                    text: root.latitudeErrorText
                                    color: ShellTheme.error
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    Accessible.role:
                                        Accessible.AlertMessage
                                    Accessible.name: text
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 5

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Longitude")
                                    color: ShellTheme.onSurface
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                }

                                TextField {
                                    id: longitudeField

                                    objectName: "nightLightLongitudeField"
                                    Layout.fillWidth: true
                                    implicitHeight: root.minimumTargetSize
                                    text: root.coordinateText(root.longitude)
                                    placeholderText: qsTr("−74.0060")
                                    inputMethodHints:
                                        Qt.ImhFormattedNumbersOnly
                                    selectByMouse: true
                                    Accessible.name: qsTr("Longitude")
                                    Accessible.description:
                                        qsTr("A number from minus 180 through 180.")

                                    onTextEdited: {
                                        root.locationDraftDirty = true;
                                        root.longitudeErrorText = root
                                            .parseCoordinate(text, false).error;
                                    }
                                    onAccepted: root.commitLocation()
                                    Keys.onEscapePressed: event => {
                                        text = root.coordinateText(
                                            root.longitude
                                        );
                                        root.longitudeErrorText = "";
                                        root.locationDraftDirty = false;
                                        event.accepted = true;
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible:
                                        root.longitudeErrorText.length > 0
                                    text: root.longitudeErrorText
                                    color: ShellTheme.error
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    Accessible.role:
                                        Accessible.AlertMessage
                                    Accessible.name: text
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                objectName: "nightLightUseCoordinatesButton"
                                implicitHeight: root.minimumTargetSize
                                text: qsTr("Use coordinates")
                                enabled: root.locationDraftDirty
                                    || !root.hasLocation
                                onClicked: root.commitLocation()
                            }

                            Button {
                                objectName: "nightLightClearLocationButton"
                                visible: root.hasLocation
                                implicitHeight: root.minimumTargetSize
                                text: qsTr("Clear")
                                onClicked: root.requestSettings({
                                    hasLocation: false,
                                    latitude: 0,
                                    longitude: 0
                                })
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.hasLocation
                            text: qsTr("Saved location: %1, %2")
                                .arg(root.coordinateText(root.latitude))
                                .arg(root.coordinateText(root.longitude))
                            color: ShellTheme.onSurfaceMuted
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.scheduleMode === "location"
                        spacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Gradual sunset")
                                color: ShellTheme.onSurface
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Blend the temperature through twilight instead of switching at once.")
                                color: ShellTheme.onSurfaceMuted
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }
                        }

                        Switch {
                            objectName: "nightLightGradualSwitch"
                            implicitHeight: root.minimumTargetSize
                            checked: root.gradual
                            checkable: false
                            focusPolicy: Qt.StrongFocus
                            Accessible.name: qsTr("Gradual sunset")
                            Accessible.checked: root.gradual

                            onClicked: root.requestSettings({
                                gradual: !root.gradual
                            })
                        }
                    }
                }
            }
        }

        Frame {
            objectName: "nightLightStatusCard"
            Layout.fillWidth: true
            padding: 14

            background: Rectangle {
                color: root.runtimeFailure
                    ? ShellTheme.errorContainer
                    : root.runtimeWarning
                        ? ShellTheme.warningContainer
                        : ShellTheme.floating
                radius: 12
                border.color: root.runtimeFailure
                    ? ShellTheme.errorOutline
                    : root.runtimeWarning
                        ? ShellTheme.warningOutline
                        : ShellTheme.outline
            }

            contentItem: ColumnLayout {
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Status")
                    color: ShellTheme.onSurface
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 1 : 2
                    columnSpacing: 18
                    rowSpacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 1

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("State")
                            color: ShellTheme.onSurfaceMuted
                            font.pixelSize: 11
                        }

                        Label {
                            objectName: "nightLightStatus"
                            Layout.fillWidth: true
                            text: root.statusText()
                            color: root.runtimeFailure
                                ? ShellTheme.onErrorContainer
                                : root.runtimeWarning
                                    ? ShellTheme.onWarningContainer
                                    : ShellTheme.onSurface
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                            Accessible.name:
                                qsTr("State: %1").arg(text)
                            Accessible.role: root.runtimeFailure
                                ? Accessible.AlertMessage
                                : Accessible.StaticText
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 1

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Filter")
                            color: ShellTheme.onSurfaceMuted
                            font.pixelSize: 11
                        }

                        Label {
                            objectName: "nightLightCurrentTemperature"
                            Layout.fillWidth: true
                            text: root.currentTemperature > 0
                                ? qsTr("%1 K").arg(
                                    root.currentTemperature
                                )
                                : qsTr("Off")
                            color: ShellTheme.onSurface
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                            Accessible.name:
                                qsTr("Filter: %1").arg(text)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        visible: root.displayTime(root.sunrise).length > 0
                        spacing: 1

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Sunrise")
                            color: ShellTheme.onSurfaceMuted
                            font.pixelSize: 11
                        }

                        Label {
                            objectName: "nightLightSunrise"
                            Layout.fillWidth: true
                            text: root.displayTime(root.sunrise)
                            color: ShellTheme.onSurface
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                            Accessible.name:
                                qsTr("Sunrise: %1").arg(text)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        visible: root.displayTime(root.sunset).length > 0
                        spacing: 1

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Sunset")
                            color: ShellTheme.onSurfaceMuted
                            font.pixelSize: 11
                        }

                        Label {
                            objectName: "nightLightSunset"
                            Layout.fillWidth: true
                            text: root.displayTime(root.sunset)
                            color: ShellTheme.onSurface
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                            Accessible.name:
                                qsTr("Sunset: %1").arg(text)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.columnSpan: root.compact ? 1 : 2
                        visible:
                            root.displayTime(root.nextTransition).length > 0
                        spacing: 1

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Next change")
                            color: ShellTheme.onSurfaceMuted
                            font.pixelSize: 11
                        }

                        Label {
                            objectName: "nightLightNextTransition"
                            Layout.fillWidth: true
                            text: root.displayNextTransition(
                                root.nextTransition
                            )
                            color: ShellTheme.onSurface
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                            Accessible.name:
                                qsTr("Next change: %1").arg(text)
                        }
                    }
                }
            }
        }
    }
}
