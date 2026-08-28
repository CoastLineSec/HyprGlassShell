pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HyprShelld.UI

Frame {
    id: root

    property string source: "desktop"
    property string scheduleMode: "time"
    property int darkStartMinute: 18 * 60
    property int lightStartMinute: 6 * 60
    property string locationSource: "manual"
    property bool hasLocation: false
    property real latitude: 0
    property real longitude: 0
    property string effectiveMode: "dark"
    property string nextTransition: ""
    property string sunrise: ""
    property string sunset: ""
    property string status: ""
    property bool serviceAvailable: false
    property bool busy: false
    property string errorText: ""
    property bool nightLightReady: false

    signal settingsRequested(
        string source,
        string scheduleMode,
        int darkStartMinute,
        int lightStartMinute,
        string locationSource,
        bool hasLocation,
        real latitude,
        real longitude
    )

    readonly property int minimumTargetSize: 44
    readonly property bool compact: width < 560
    readonly property bool canRequest: serviceAvailable && !busy
    readonly property bool scheduleVisible: source === "schedule"
    readonly property bool locationVisible:
        scheduleVisible && scheduleMode === "location"

    property string darkStartDraft: minuteText(darkStartMinute)
    property string lightStartDraft: minuteText(lightStartMinute)
    property string latitudeDraft: hasLocation
        ? coordinateText(latitude) : ""
    property string longitudeDraft: hasLocation
        ? coordinateText(longitude) : ""
    property bool timeValidationRequested: false
    property bool locationValidationRequested: false

    readonly property int parsedDarkStart:
        parseMinuteOfDay(darkStartDraft)
    readonly property int parsedLightStart:
        parseMinuteOfDay(lightStartDraft)
    readonly property real parsedLatitude: Number(latitudeDraft.trim())
    readonly property real parsedLongitude: Number(longitudeDraft.trim())
    readonly property bool coordinatesComplete:
        latitudeDraft.trim().length > 0
        && longitudeDraft.trim().length > 0
    readonly property bool coordinatesValid:
        coordinatesComplete
        && Number.isFinite(parsedLatitude)
        && Number.isFinite(parsedLongitude)
        && parsedLatitude >= -90 && parsedLatitude <= 90
        && parsedLongitude >= -180 && parsedLongitude <= 180
    readonly property string timeValidationText: {
        if (!timeValidationRequested)
            return "";
        if (parsedDarkStart < 0 || parsedLightStart < 0)
            return qsTr("Enter each time as HH:MM on a 24-hour clock.");
        if (parsedDarkStart === parsedLightStart)
            return qsTr("Dark and light transition times must be different.");
        return "";
    }
    readonly property string locationValidationText: {
        if (!locationValidationRequested)
            return "";
        if (!coordinatesComplete)
            return qsTr("Enter both latitude and longitude.");
        if (!Number.isFinite(parsedLatitude)
                || parsedLatitude < -90 || parsedLatitude > 90) {
            return qsTr("Latitude must be between −90 and 90.");
        }
        if (!Number.isFinite(parsedLongitude)
                || parsedLongitude < -180 || parsedLongitude > 180) {
            return qsTr("Longitude must be between −180 and 180.");
        }
        return "";
    }

    readonly property var sourceChoices: [
        {
            value: "desktop",
            label: qsTr("Desktop"),
            symbol: "◐",
            description: qsTr("Follow the desktop appearance preference")
        },
        {
            value: "schedule",
            label: qsTr("Custom schedule"),
            symbol: "◷",
            description: qsTr("Switch by clock or local sun times")
        },
        {
            value: "night-light",
            label: qsTr("Night Light"),
            symbol: "☾",
            description: nightLightReady
                ? qsTr("Follow the Night Light schedule")
                : qsTr("Night Light schedule unavailable")
        }
    ]

    readonly property var methodChoices: [
        {
            value: "time",
            label: qsTr("Time"),
            description: qsTr("Choose exact dark and light times")
        },
        {
            value: "location",
            label: qsTr("Sunrise & sunset"),
            description: qsTr("Use the sun at your location")
        }
    ]
    readonly property var locationSourceChoices: [
        {
            value: "geoclue",
            label: qsTr("GeoClue"),
            description: qsTr("Use the system location service")
        },
        {
            value: "manual",
            label: qsTr("Manual"),
            description: qsTr("Enter latitude and longitude")
        }
    ]

    objectName: "themeAutomationCard"
    // The responsive content changes its column count from the assigned
    // width. A stable preferred width prevents Frame's style implicit width
    // from feeding that decision back into itself inside a Layout.
    implicitWidth: 560
    padding: compact ? 14 : 18

    // Qt Quick Controls otherwise inherit the host toolkit palette, which
    // may disagree with the shell's explicitly selected color mode.
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

    function minuteText(value) {
        const safe = Number.isInteger(value)
            ? Math.max(0, Math.min(1439, value)) : 0;
        const hour = Math.floor(safe / 60);
        const minute = safe % 60;
        return String(hour).padStart(2, "0")
            + ":" + String(minute).padStart(2, "0");
    }

    function parseMinuteOfDay(value) {
        const match = String(value || "").trim()
            .match(/^(\d{1,2})(?::(\d{1,2}))?$/);
        if (!match)
            return -1;
        const hour = Number(match[1]);
        const minute = match[2] === undefined ? 0 : Number(match[2]);
        if (!Number.isInteger(hour) || !Number.isInteger(minute)
                || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
            return -1;
        }
        return hour * 60 + minute;
    }

    function coordinateText(value) {
        if (!Number.isFinite(value))
            return "";
        return Number(value).toFixed(6)
            .replace(/0+$/, "").replace(/\.$/, "");
    }

    function displayTime(value) {
        if (!value)
            return qsTr("Not available");
        const date = new Date(value);
        if (!isNaN(date.getTime()))
            return date.toLocaleTimeString(Qt.locale(), Locale.ShortFormat);
        return String(value);
    }

    function displayNextTransition(value) {
        if (!value)
            return qsTr("No upcoming change");
        const date = new Date(value);
        if (!isNaN(date.getTime()))
            return date.toLocaleString(Qt.locale(), Locale.ShortFormat);
        return String(value);
    }

    function normalizedStatusText() {
        if (!serviceAvailable)
            return qsTr("Automatic color switching is unavailable.");
        if (busy)
            return qsTr("Saving automatic color settings…");
        switch (status) {
        case "desktop":
            return qsTr("Following the desktop appearance preference.");
        case "ready":
            return qsTr("Automatic switching is ready.");
        case "waiting-location":
            return qsTr("Waiting for a usable location.");
        case "no-transition":
            return qsTr("No transition is expected today.");
        case "unavailable":
            return qsTr("The selected automation source is unavailable.");
        default:
            return status || qsTr("Automatic switching is ready.");
        }
    }

    function emitAuthoritativeWith(nextSource, nextMode,
            nextLocationSource) {
        if (!canRequest)
            return;
        settingsRequested(
            nextSource,
            nextMode,
            darkStartMinute,
            lightStartMinute,
            nextLocationSource,
            hasLocation,
            latitude,
            longitude
        );
    }

    function applyTimeDraft() {
        timeValidationRequested = true;
        if (!canRequest || timeValidationText.length > 0)
            return;
        settingsRequested(
            source,
            scheduleMode,
            parsedDarkStart,
            parsedLightStart,
            locationSource,
            hasLocation,
            latitude,
            longitude
        );
    }

    function applyLocationDraft() {
        locationValidationRequested = true;
        if (!canRequest || locationValidationText.length > 0)
            return;
        settingsRequested(
            source,
            scheduleMode,
            darkStartMinute,
            lightStartMinute,
            locationSource,
            true,
            parsedLatitude,
            parsedLongitude
        );
    }

    function clearLocation() {
        if (!canRequest)
            return;
        settingsRequested(
            source,
            scheduleMode,
            darkStartMinute,
            lightStartMinute,
            locationSource,
            false,
            latitude,
            longitude
        );
    }

    function moveMethodFocus(index, offset) {
        const targetIndex = (index + offset + methodRepeater.count)
            % methodRepeater.count;
        const target = methodRepeater.itemAt(targetIndex);
        if (!target)
            return;
        target.forceActiveFocus();
        const choice = root.methodChoices[targetIndex];
        if (target.enabled && choice.value !== root.scheduleMode) {
            root.emitAuthoritativeWith(
                root.source, choice.value, root.locationSource
            );
        }
    }

    function moveLocationSourceFocus(index, offset) {
        const targetIndex = (index + offset
            + locationSourceRepeater.count) % locationSourceRepeater.count;
        const target = locationSourceRepeater.itemAt(targetIndex);
        if (!target)
            return;
        target.forceActiveFocus();
        const choice = root.locationSourceChoices[targetIndex];
        if (target.enabled && choice.value !== root.locationSource) {
            root.emitAuthoritativeWith(
                root.source, root.scheduleMode, choice.value
            );
        }
    }

    onDarkStartMinuteChanged: {
        if (!darkStartField.activeFocus) {
            darkStartDraft = minuteText(darkStartMinute);
            timeValidationRequested = false;
        }
    }
    onLightStartMinuteChanged: {
        if (!lightStartField.activeFocus) {
            lightStartDraft = minuteText(lightStartMinute);
            timeValidationRequested = false;
        }
    }
    onLatitudeChanged: {
        if (!latitudeField.activeFocus && hasLocation) {
            latitudeDraft = coordinateText(latitude);
            locationValidationRequested = false;
        }
    }
    onLongitudeChanged: {
        if (!longitudeField.activeFocus && hasLocation) {
            longitudeDraft = coordinateText(longitude);
            locationValidationRequested = false;
        }
    }
    onHasLocationChanged: {
        if (hasLocation) {
            if (!latitudeField.activeFocus)
                latitudeDraft = coordinateText(latitude);
            if (!longitudeField.activeFocus)
                longitudeDraft = coordinateText(longitude);
        }
        locationValidationRequested = false;
    }

    contentItem: ColumnLayout {
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Automatic switching")
                    color: root.palette.text
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Choose what decides when the shell uses light or dark colors.")
                    color: root.palette.placeholderText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }

            Label {
                text: root.effectiveMode === "light"
                    ? qsTr("Using Light") : qsTr("Using Dark")
                color: root.palette.highlight
                font.pixelSize: 12
                font.weight: Font.DemiBold
                leftPadding: 9
                rightPadding: 9
                topPadding: 5
                bottomPadding: 5
                Accessible.name: text

                background: Rectangle {
                    radius: 10
                    color: ShellTheme.overlay(
                        root.palette.highlight, 0.13, root.palette.base
                    )
                    border.color: root.palette.highlight
                }
            }
        }

        GridLayout {
            id: sourceGrid

            Layout.fillWidth: true
            columns: root.compact ? 1 : 3
            columnSpacing: 10
            rowSpacing: 10

            Repeater {
                id: sourceRepeater
                model: root.sourceChoices

                delegate: AbstractButton {
                    id: sourceButton

                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: root.compact ? 68 : 104
                    implicitHeight: root.compact ? 68 : 104
                    checkable: false
                    checked: root.source === sourceButton.modelData.value
                    enabled: root.canRequest
                        && (sourceButton.modelData.value !== "night-light"
                            || root.nightLightReady || checked)
                    focusPolicy: Qt.StrongFocus
                    hoverEnabled: true
                    objectName: "themeAutomationSource-"
                        + sourceButton.modelData.value

                    Accessible.role: Accessible.RadioButton
                    Accessible.name: sourceButton.modelData.label
                    Accessible.description: sourceButton.modelData.description
                    Accessible.checked: checked

                    onClicked: {
                        if (!checked) {
                            root.emitAuthoritativeWith(
                                sourceButton.modelData.value,
                                root.scheduleMode,
                                root.locationSource
                            );
                        }
                    }

                    function moveFocus(offset) {
                        const count = sourceRepeater.count;
                        for (let step = 1; step <= count; ++step) {
                            const targetIndex = (index + offset * step + count)
                                % count;
                            const target = sourceRepeater.itemAt(targetIndex);
                            if (target && target.enabled) {
                                target.forceActiveFocus();
                                const choice = root.sourceChoices[targetIndex];
                                if (choice.value !== root.source) {
                                    root.emitAuthoritativeWith(
                                        choice.value,
                                        root.scheduleMode,
                                        root.locationSource
                                    );
                                }
                                return;
                            }
                        }
                    }

                    Keys.onLeftPressed: event => {
                        sourceButton.moveFocus(-1);
                        event.accepted = true;
                    }
                    Keys.onRightPressed: event => {
                        sourceButton.moveFocus(1);
                        event.accepted = true;
                    }
                    Keys.onUpPressed: event => {
                        sourceButton.moveFocus(-1);
                        event.accepted = true;
                    }
                    Keys.onDownPressed: event => {
                        sourceButton.moveFocus(1);
                        event.accepted = true;
                    }

                    background: Rectangle {
                        radius: 12
                        color: sourceButton.hovered || sourceButton.down
                            ? ShellTheme.overlay(
                                root.palette.text,
                                sourceButton.down ? 0.14 : 0.07,
                                root.palette.button
                            ) : root.palette.button
                        border.width: sourceButton.activeFocus ? 3
                            : sourceButton.checked ? 2 : 1
                        border.color: sourceButton.activeFocus
                            ? root.palette.text
                            : sourceButton.checked
                                ? root.palette.highlight : root.palette.mid
                    }

                    contentItem: RowLayout {
                        spacing: 10

                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            text: sourceButton.modelData.symbol
                            color: sourceButton.checked
                                ? root.palette.highlight : root.palette.text
                            font.pixelSize: root.compact ? 22 : 28
                            Accessible.ignored: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: sourceButton.modelData.label
                                color: root.palette.text
                                font.pixelSize: 13
                                font.weight: sourceButton.checked
                                    ? Font.DemiBold : Font.Medium
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: sourceButton.modelData.description
                                color: root.palette.placeholderText
                                font.pixelSize: 10
                                wrapMode: root.compact
                                    ? Text.NoWrap : Text.Wrap
                                elide: Text.ElideRight
                                maximumLineCount: root.compact ? 1 : 2
                            }
                        }
                    }
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            visible: root.source === "desktop"
            padding: 12

            background: Rectangle {
                radius: 10
                color: ShellTheme.infoContainer
                border.color: ShellTheme.infoOutline
            }

            Label {
                anchors.fill: parent
                text: qsTr("The desktop appearance preference remains authoritative. Change it in your desktop or portal settings.")
                color: ShellTheme.onInfoContainer
                font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.scheduleVisible
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: qsTr("Custom schedule method")
                color: root.palette.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            GridLayout {
                id: methodGrid

                Layout.fillWidth: true
                columns: root.compact ? 1 : 2
                columnSpacing: 10
                rowSpacing: 10

                Repeater {
                    id: methodRepeater

                    model: root.methodChoices

                    delegate: AbstractButton {
                        id: methodButton

                        required property int index
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        implicitHeight: Math.max(
                            root.minimumTargetSize, methodContent.implicitHeight + 16
                        )
                        checkable: false
                        checked: root.scheduleMode
                            === methodButton.modelData.value
                        enabled: root.canRequest
                        focusPolicy: Qt.StrongFocus
                        hoverEnabled: true
                        objectName: "themeAutomationMethod-"
                            + methodButton.modelData.value

                        Accessible.role: Accessible.RadioButton
                        Accessible.name: methodButton.modelData.label
                        Accessible.description:
                            methodButton.modelData.description
                        Accessible.checked: checked

                        onClicked: {
                            if (!checked) {
                                root.emitAuthoritativeWith(
                                    root.source,
                                    methodButton.modelData.value,
                                    root.locationSource
                                );
                            }
                        }
                        Keys.onLeftPressed: event => {
                            root.moveMethodFocus(methodButton.index, -1);
                            event.accepted = true;
                        }
                        Keys.onRightPressed: event => {
                            root.moveMethodFocus(methodButton.index, 1);
                            event.accepted = true;
                        }
                        Keys.onUpPressed: event => {
                            root.moveMethodFocus(methodButton.index, -1);
                            event.accepted = true;
                        }
                        Keys.onDownPressed: event => {
                            root.moveMethodFocus(methodButton.index, 1);
                            event.accepted = true;
                        }

                        background: Rectangle {
                            radius: 10
                            color: methodButton.hovered
                                ? ShellTheme.surfaceHover
                                : root.palette.button
                            border.width: methodButton.activeFocus ? 3
                                : methodButton.checked ? 2 : 1
                            border.color: methodButton.activeFocus
                                ? root.palette.text
                                : methodButton.checked
                                    ? root.palette.highlight : root.palette.mid
                        }

                        contentItem: ColumnLayout {
                            id: methodContent
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: methodButton.modelData.label
                                color: root.palette.text
                                font.pixelSize: 13
                                font.weight: methodButton.checked
                                    ? Font.DemiBold : Font.Medium
                            }

                            Label {
                                Layout.fillWidth: true
                                text: methodButton.modelData.description
                                color: root.palette.placeholderText
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.scheduleMode === "time"
                spacing: 12

                Frame {
                    Layout.fillWidth: true
                    padding: 12

                    background: Rectangle {
                        radius: 10
                        color: root.palette.alternateBase
                        border.color: root.palette.mid
                    }

                    contentItem: ColumnLayout {
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("A day at a glance")
                            color: root.palette.text
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        Item {
                            id: timelineGraphic

                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            Accessible.ignored: true

                            Rectangle {
                                id: timelineTrack
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    verticalCenter: parent.verticalCenter
                                    leftMargin: 4
                                    rightMargin: 4
                                }
                                height: 12
                                radius: 6
                                color: ShellTheme.warningContainer
                                border.color: root.palette.mid

                                Rectangle {
                                    visible: root.darkStartMinute
                                        < root.lightStartMinute
                                    x: parent.width * root.darkStartMinute / 1440
                                    width: parent.width
                                        * (root.lightStartMinute
                                            - root.darkStartMinute) / 1440
                                    height: parent.height
                                    color: ShellTheme.primaryContainer
                                }

                                Rectangle {
                                    visible: root.darkStartMinute
                                        > root.lightStartMinute
                                    width: parent.width
                                        * root.lightStartMinute / 1440
                                    height: parent.height
                                    color: ShellTheme.primaryContainer
                                }

                                Rectangle {
                                    visible: root.darkStartMinute
                                        > root.lightStartMinute
                                    x: parent.width
                                        * root.darkStartMinute / 1440
                                    width: parent.width - x
                                    height: parent.height
                                    color: ShellTheme.primaryContainer
                                }
                            }

                            Repeater {
                                model: ["00", "06", "12", "18", "24"]

                                Label {
                                    required property int index
                                    required property string modelData
                                    x: Math.max(0, Math.min(
                                        timelineGraphic.width - width,
                                        timelineGraphic.width * index / 4
                                            - width / 2
                                    ))
                                    y: timelineTrack.y + timelineTrack.height + 5
                                    text: modelData
                                    color: root.palette.placeholderText
                                    font.pixelSize: 9
                                }
                            }
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 1 : 2
                    columnSpacing: 12
                    rowSpacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: qsTr("Dark from")
                            color: root.palette.text
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }

                        TextField {
                            id: darkStartField

                            Layout.fillWidth: true
                            implicitHeight: root.minimumTargetSize
                            text: root.darkStartDraft
                            placeholderText: qsTr("18:00")
                            enabled: root.canRequest
                            selectByMouse: true
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            Accessible.name: qsTr("Time to begin dark colors")
                            Accessible.description: qsTr("24-hour time, HH:MM")

                            onTextEdited: root.darkStartDraft = text
                            onAccepted: root.applyTimeDraft()
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: qsTr("Light from")
                            color: root.palette.text
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }

                        TextField {
                            id: lightStartField

                            Layout.fillWidth: true
                            implicitHeight: root.minimumTargetSize
                            text: root.lightStartDraft
                            placeholderText: qsTr("06:00")
                            enabled: root.canRequest
                            selectByMouse: true
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            Accessible.name: qsTr("Time to begin light colors")
                            Accessible.description: qsTr("24-hour time, HH:MM")

                            onTextEdited: root.lightStartDraft = text
                            onAccepted: root.applyTimeDraft()
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.timeValidationText.length > 0
                    text: root.timeValidationText
                    color: ShellTheme.onErrorContainer
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    leftPadding: 9
                    rightPadding: 9
                    topPadding: 7
                    bottomPadding: 7
                    Accessible.role: Accessible.AlertMessage
                    Accessible.name: text

                    background: Rectangle {
                        radius: 7
                        color: ShellTheme.errorContainer
                        border.color: ShellTheme.errorOutline
                    }
                }

                Button {
                    Layout.alignment: Qt.AlignRight
                    implicitHeight: Math.max(
                        root.minimumTargetSize,
                        implicitBackgroundHeight,
                        implicitContentHeight + topPadding + bottomPadding
                    )
                    text: root.busy ? qsTr("Saving…")
                        : qsTr("Apply times")
                    enabled: root.canRequest
                    Accessible.name: qsTr("Apply automatic color transition times")
                    onClicked: root.applyTimeDraft()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.scheduleMode === "location"
                spacing: 12

                Frame {
                    Layout.fillWidth: true
                    padding: 12

                    background: Rectangle {
                        radius: 10
                        color: root.palette.alternateBase
                        border.color: root.palette.mid
                    }

                    contentItem: ColumnLayout {
                        spacing: 8

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: root.effectiveMode === "light" ? "☀" : "☾"
                            color: root.effectiveMode === "light"
                                ? ShellTheme.warning : root.palette.highlight
                            font.pixelSize: 28
                            Accessible.ignored: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 3
                            radius: 2
                            color: root.palette.midlight
                            Accessible.ignored: true
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: qsTr("Sunrise")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 10
                                }
                                Label {
                                    text: root.displayTime(root.sunrise)
                                    color: root.palette.text
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    Layout.alignment: Qt.AlignRight
                                    text: qsTr("Sunset")
                                    color: root.palette.placeholderText
                                    font.pixelSize: 10
                                }
                                Label {
                                    Layout.alignment: Qt.AlignRight
                                    text: root.displayTime(root.sunset)
                                    color: root.palette.text
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Location source")
                    color: root.palette.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 1 : 2
                    columnSpacing: 10
                    rowSpacing: 10

                    Repeater {
                        id: locationSourceRepeater

                        model: root.locationSourceChoices

                        delegate: AbstractButton {
                            id: locationSourceButton

                            required property int index
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                locationSourceContent.implicitHeight + 16
                            )
                            checkable: false
                            checked: root.locationSource
                                === locationSourceButton.modelData.value
                            enabled: root.canRequest
                            focusPolicy: Qt.StrongFocus
                            hoverEnabled: true

                            Accessible.role: Accessible.RadioButton
                            Accessible.name:
                                locationSourceButton.modelData.label
                            Accessible.description:
                                locationSourceButton.modelData.description
                            Accessible.checked: checked

                            onClicked: {
                                if (!checked) {
                                    root.emitAuthoritativeWith(
                                        root.source,
                                        root.scheduleMode,
                                        locationSourceButton.modelData.value
                                    );
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
                                color: locationSourceButton.hovered
                                    ? ShellTheme.surfaceHover
                                    : root.palette.button
                                border.width:
                                    locationSourceButton.activeFocus ? 3
                                    : locationSourceButton.checked ? 2 : 1
                                border.color:
                                    locationSourceButton.activeFocus
                                    ? root.palette.text
                                    : locationSourceButton.checked
                                        ? root.palette.highlight
                                        : root.palette.mid
                            }

                            contentItem: ColumnLayout {
                                id: locationSourceContent
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: locationSourceButton.modelData.label
                                    color: root.palette.text
                                    font.pixelSize: 13
                                    font.weight:
                                        locationSourceButton.checked
                                        ? Font.DemiBold : Font.Medium
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: locationSourceButton
                                        .modelData.description
                                    color: root.palette.placeholderText
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    visible: root.locationSource === "geoclue"
                    padding: 12

                    background: Rectangle {
                        radius: 10
                        color: root.status === "ready"
                                || root.status === "no-transition"
                            ? ShellTheme.successContainer
                            : ShellTheme.infoContainer
                        border.color: root.status === "ready"
                                || root.status === "no-transition"
                            ? ShellTheme.successOutline
                            : ShellTheme.infoOutline
                    }

                    Label {
                        objectName: "themeAutomationGeoClueStatus"
                        anchors.fill: parent
                        text: root.status === "waiting-location"
                            ? qsTr("Waiting for GeoClue. Check location services and permission if this does not resolve.")
                            : qsTr("GeoClue supplies location directly to the local scheduling service. Its coordinates are not copied into the manual fields.")
                        color: root.status === "ready"
                                || root.status === "no-transition"
                            ? ShellTheme.onSuccessContainer
                            : ShellTheme.onInfoContainer
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
                        columnSpacing: 12
                        rowSpacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                text: qsTr("Latitude")
                                color: root.palette.text
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }

                            TextField {
                                id: latitudeField

                                Layout.fillWidth: true
                                implicitHeight: root.minimumTargetSize
                                text: root.latitudeDraft
                                placeholderText: qsTr("40.7128")
                                enabled: root.canRequest
                                selectByMouse: true
                                inputMethodHints:
                                    Qt.ImhFormattedNumbersOnly
                                validator: DoubleValidator {
                                    bottom: -90
                                    top: 90
                                    notation:
                                        DoubleValidator.StandardNotation
                                }
                                Accessible.name: qsTr("Manual latitude")
                                Accessible.description:
                                    qsTr("A number from negative 90 to 90")

                                onTextEdited: root.latitudeDraft = text
                                onAccepted: root.applyLocationDraft()
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                text: qsTr("Longitude")
                                color: root.palette.text
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }

                            TextField {
                                id: longitudeField

                                Layout.fillWidth: true
                                implicitHeight: root.minimumTargetSize
                                text: root.longitudeDraft
                                placeholderText: qsTr("−74.0060")
                                enabled: root.canRequest
                                selectByMouse: true
                                inputMethodHints:
                                    Qt.ImhFormattedNumbersOnly
                                validator: DoubleValidator {
                                    bottom: -180
                                    top: 180
                                    notation:
                                        DoubleValidator.StandardNotation
                                }
                                Accessible.name: qsTr("Manual longitude")
                                Accessible.description:
                                    qsTr("A number from negative 180 to 180")

                                onTextEdited: root.longitudeDraft = text
                                onAccepted: root.applyLocationDraft()
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.locationValidationText.length > 0
                        text: root.locationValidationText
                        color: ShellTheme.onErrorContainer
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        leftPadding: 9
                        rightPadding: 9
                        topPadding: 7
                        bottomPadding: 7
                        Accessible.role: Accessible.AlertMessage
                        Accessible.name: text

                        background: Rectangle {
                            radius: 7
                            color: ShellTheme.errorContainer
                            border.color: ShellTheme.errorOutline
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: childrenRect.height
                        spacing: 10

                        Button {
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight
                                    + topPadding + bottomPadding
                            )
                            text: root.busy ? qsTr("Saving…")
                                : qsTr("Apply location")
                            enabled: root.canRequest
                            Accessible.name:
                                qsTr("Apply manual scheduling location")
                            onClicked: root.applyLocationDraft()
                        }

                        Button {
                            visible: root.hasLocation
                            implicitHeight: Math.max(
                                root.minimumTargetSize,
                                implicitBackgroundHeight,
                                implicitContentHeight
                                    + topPadding + bottomPadding
                            )
                            text: qsTr("Clear location")
                            enabled: root.canRequest
                            Accessible.name:
                                qsTr("Clear the manual scheduling location")
                            onClicked: root.clearLocation()
                        }
                    }
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            visible: root.source === "night-light"
            padding: 12

            background: Rectangle {
                radius: 10
                color: root.nightLightReady
                    ? ShellTheme.infoContainer
                    : ShellTheme.warningContainer
                border.color: root.nightLightReady
                    ? ShellTheme.infoOutline
                    : ShellTheme.warningOutline
            }

            contentItem: ColumnLayout {
                spacing: 9

                Label {
                    Layout.fillWidth: true
                    text: root.nightLightReady
                        ? qsTr("Following Night Light")
                        : qsTr("Night Light schedule unavailable")
                    color: root.nightLightReady
                        ? ShellTheme.onInfoContainer
                        : ShellTheme.onWarningContainer
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 1 : 3
                    columnSpacing: 12
                    rowSpacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            text: qsTr("Light from")
                            color: root.nightLightReady
                                ? ShellTheme.onInfoContainer
                                : ShellTheme.onWarningContainer
                            font.pixelSize: 10
                        }
                        Label {
                            text: root.displayTime(root.sunrise)
                            color: root.nightLightReady
                                ? ShellTheme.onInfoContainer
                                : ShellTheme.onWarningContainer
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            text: qsTr("Dark from")
                            color: root.nightLightReady
                                ? ShellTheme.onInfoContainer
                                : ShellTheme.onWarningContainer
                            font.pixelSize: 10
                        }
                        Label {
                            text: root.displayTime(root.sunset)
                            color: root.nightLightReady
                                ? ShellTheme.onInfoContainer
                                : ShellTheme.onWarningContainer
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            text: qsTr("Next change")
                            color: root.nightLightReady
                                ? ShellTheme.onInfoContainer
                                : ShellTheme.onWarningContainer
                            font.pixelSize: 10
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.displayNextTransition(
                                root.nextTransition
                            )
                            color: root.nightLightReady
                                ? ShellTheme.onInfoContainer
                                : ShellTheme.onWarningContainer
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            padding: 12

            background: Rectangle {
                radius: 10
                color: !root.serviceAvailable
                    ? ShellTheme.warningContainer
                    : root.errorText.length > 0
                        ? ShellTheme.errorContainer
                        : root.palette.alternateBase
                border.color: !root.serviceAvailable
                    ? ShellTheme.warningOutline
                    : root.errorText.length > 0
                        ? ShellTheme.errorOutline : root.palette.mid
            }

            contentItem: RowLayout {
                spacing: 10

                Label {
                    text: root.busy ? "…"
                        : root.effectiveMode === "light" ? "☀" : "☾"
                    color: !root.serviceAvailable
                        ? ShellTheme.onWarningContainer
                        : root.errorText.length > 0
                            ? ShellTheme.onErrorContainer
                            : root.palette.highlight
                    font.pixelSize: 18
                    Accessible.ignored: true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 2

                    Label {
                        objectName: "themeAutomationStatus"
                        Layout.fillWidth: true
                        text: root.errorText.length > 0
                            ? root.errorText : root.normalizedStatusText()
                        color: !root.serviceAvailable
                            ? ShellTheme.onWarningContainer
                            : root.errorText.length > 0
                                ? ShellTheme.onErrorContainer
                                : root.palette.text
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        Accessible.role: root.errorText.length > 0
                            ? Accessible.AlertMessage
                            : Accessible.StaticText
                        Accessible.name: text
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.serviceAvailable
                            && root.errorText.length === 0
                            && root.source !== "desktop"
                        text: qsTr("Next change: %1").arg(
                            root.displayNextTransition(
                                root.nextTransition
                            )
                        )
                        color: root.palette.placeholderText
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
