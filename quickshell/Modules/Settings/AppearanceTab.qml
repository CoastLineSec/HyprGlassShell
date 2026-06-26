import QtCore
import QtQuick
import QtQuick.Effects
import Quickshell
import qs.Common
import qs.Modals.FileBrowser
import qs.Services
import qs.Widgets
import qs.Modules.Settings.Widgets
import "../../Common/ConfigIncludeResolve.js" as ConfigIncludeResolve

// macOS-style "Appearance" pane. Owns System Appearance (Light/Dark/Auto),
// Theme Color, Icon Theme and Cursor Theme — moved here from Theme & Colors.
Item {
    id: appearanceTab

    property var parentModal: null
    property var cachedIconThemes: SettingsData.availableIconThemes
    property var cachedCursorThemes: SettingsData.availableCursorThemes
    property var installedRegistryThemes: []

    property var cursorIncludeStatus: ({
            "exists": false,
            "included": false,
            "configFormat": "",
            "readOnly": false
        })
    readonly property bool cursorReadOnly: cursorIncludeStatus.readOnly === true
    property bool checkingCursorInclude: false
    property bool fixingCursorInclude: false

    property var cachedFontFamilies: []
    property var cachedMonoFamilies: []
    property bool fontsEnumerated: false

    // "" = main pane; otherwise an open sub-page id (e.g. "advancedFonts").
    property string sub: ""

    onVisibleChanged: {
        if (!visible)
            sub = "";
    }

    // Icon-theme preview: applied snapshot (drives the Restart notice) + sample
    // icon file paths loaded directly off disk, so even an unapplied theme previews.
    property string appliedIconTheme: ""
    property var iconPreviewMain: []
    property var iconPreviewSymbolic: []

    function refreshIconPreview() {
        var t = SettingsData.iconTheme === "System Default" ? SettingsData.systemDefaultIconTheme : SettingsData.iconTheme;
        if (!t) {
            iconPreviewMain = [];
            iconPreviewSymbolic = [];
            return;
        }
        var script = `theme="$1"
nl="
"
AWK='function sz(p, n,a,i){n=split(p,a,"/");for(i=n;i>=1;i--){if(a[i]=="scalable")return 48;if(a[i]~/^[0-9]+$/)return a[i]+0;if(a[i]~/^[0-9]+x[0-9]+$/){split(a[i],b,"x");return b[1]+0}}return 9999}{s=sz($0);dd=(s>=48)?(s-48):(48-s)*2;if(NR==1||dd<bd){bd=dd;bp=$0}}END{if(bp)print bp}'
themedir() { for b in /usr/share/icons /usr/local/share/icons "$HOME/.local/share/icons" "$HOME/.icons"; do [ -d "$b/$1" ] && { printf '%s' "$b/$1"; return 0; }; done; return 1; }
order=""
add() { [ -n "$1" ] && order="$order$1$nl"; }
d0=$(themedir "$theme") && add "$d0"
if [ -n "$d0" ] && [ -f "$d0/index.theme" ]; then
  inh=$(grep -m1 '^Inherits=' "$d0/index.theme" | cut -d= -f2)
  oifs=$IFS; IFS=','
  for p in $inh; do IFS=$oifs; p=$(printf '%s' "$p" | sed 's/^ *//;s/ *$//'); pd=$(themedir "$p") && add "$pd"; IFS=','; done
  IFS=$oifs
fi
for fb in hicolor Adwaita; do fd=$(themedir "$fb") && add "$fd"; done
resolve() {
  name="$1"; oifs=$IFS; IFS=$nl
  for d in $order; do
    IFS=$oifs
    if [ -n "$d" ]; then
      best=$(find "$d" -name "$name.svg" 2>/dev/null | awk "$AWK")
      [ -z "$best" ] && best=$(find "$d" -name "$name.png" 2>/dev/null | awk "$AWK")
      [ -n "$best" ] && { printf '%s' "$best"; return 0; }
    fi
    IFS=$nl
  done
  IFS=$oifs
}
for n in user-home user-desktop folder folder-remote user-trash x-office-document application-x-executable image-x-generic package-x-generic emblem-mail utilities-terminal chromium firefox gimp; do
  printf 'M|%s|%s\n' "$n" "$(resolve "$n")"
done
for n in network-wired-symbolic network-wireless-symbolic bluetooth-active-symbolic computer-symbolic audio-volume-high-symbolic battery-low-charging-symbolic display-brightness-medium-symbolic; do
  printf 'S|%s|%s\n' "$n" "$(resolve "$n")"
done`;
        Proc.runCommand("icon-preview", ["sh", "-c", script, "iconpreview", t], (output, code) => {
            if (code !== 0 || !output) {
                appearanceTab.iconPreviewMain = [];
                appearanceTab.iconPreviewSymbolic = [];
                return;
            }
            var mainList = [];
            var symList = [];
            var rows = output.trim().split("\n");
            for (var i = 0; i < rows.length; i++) {
                var parts = rows[i].split("|");
                if (parts.length < 3 || !parts[2])
                    continue;
                if (parts[0] === "M")
                    mainList.push(parts[2]);
                else if (parts[0] === "S")
                    symList.push(parts[2]);
            }
            appearanceTab.iconPreviewMain = mainList;
            appearanceTab.iconPreviewSymbolic = symList;
        });
    }

    // Which appearance tile is active, derived from the live session state.
    function currentMode() {
        if (SessionData.themeModeAutoEnabled)
            return "auto";
        return SessionData.isLightMode ? "light" : "dark";
    }

    function selectMode(mode) {
        if (mode === "auto") {
            SessionData.setThemeModeAutoEnabled(true);
            return;
        }
        if (SessionData.themeModeAutoEnabled)
            SessionData.setThemeModeAutoEnabled(false);
        Theme.screenTransition();
        Theme.setLightMode(mode === "light");
    }

    // Static custom-color picker — pick any hex, seed the theme from it (matugen).
    function openStaticColorPicker() {
        if (!PopoutService.colorPickerModal)
            return;
        var seed = (Theme.currentTheme && typeof Theme.currentTheme === "string" && Theme.currentTheme.startsWith("#")) ? Theme.currentTheme : Theme.primary;
        PopoutService.colorPickerModal.selectedColor = seed;
        PopoutService.colorPickerModal.pickerTitle = I18n.tr("Theme Color");
        PopoutService.colorPickerModal.onColorSelectedCallback = function (color) {
            var hex = "#" + Math.round(color.r * 255).toString(16).padStart(2, "0") + Math.round(color.g * 255).toString(16).padStart(2, "0") + Math.round(color.b * 255).toString(16).padStart(2, "0");
            Theme.switchTheme(hex);
        };
        PopoutService.colorPickerModal.show();
    }

    function enumerateFonts() {
        var fonts = [];
        var availableFonts = Qt.fontFamilies();

        for (var i = 0; i < availableFonts.length; i++) {
            var fontName = availableFonts[i];
            if (fontName.startsWith("."))
                continue;
            fonts.push(fontName);
        }
        fonts.sort();
        fonts.unshift("Default");
        cachedFontFamilies = fonts;
        cachedMonoFamilies = fonts;
    }

    function getCursorConfigPaths() {
        const configDir = Paths.strip(StandardPaths.writableLocation(StandardPaths.ConfigLocation));
        return {
            "configFile": configDir + "/hypr/hyprland.lua",
            "cursorFile": configDir + "/hypr/hgs/cursor.lua",
            "grepPattern": "hgs.cursor",
            "includeLine": "require(\"hgs.cursor\")"
        };
    }

    function checkCursorIncludeStatus() {
        checkingCursorInclude = true;
        Proc.runCommand("check-cursor-include", ["hgs", "config", "resolve-include", "hyprland", "cursor.lua"], (output, exitCode) => {
            checkingCursorInclude = false;
            if (exitCode !== 0) {
                cursorIncludeStatus = {
                    "exists": false,
                    "included": false,
                    "configFormat": "",
                    "readOnly": false
                };
                return;
            }
            try {
                cursorIncludeStatus = JSON.parse(output.trim());
            } catch (e) {
                cursorIncludeStatus = {
                    "exists": false,
                    "included": false,
                    "configFormat": "",
                    "readOnly": false
                };
            }
        });
    }

    function fixCursorInclude() {
        if (cursorReadOnly) {
            ToastService.showWarning(I18n.tr("Hyprland conf mode"), I18n.tr("This install is still using hyprland.conf. Run hgs setup to migrate before editing cursor settings."), "hgs setup", "hyprland-migration");
            return;
        }
        const paths = getCursorConfigPaths();
        if (!paths)
            return;
        fixingCursorInclude = true;
        const unixTime = Math.floor(Date.now() / 1000);
        const backupFile = paths.configFile + ".backup" + unixTime;
        const script = ConfigIncludeResolve.buildRepairScript({
            configFile: paths.configFile,
            backupFile: backupFile,
            fragmentFile: paths.cursorFile,
            grepPattern: paths.grepPattern,
            includeLine: paths.includeLine
        });
        Proc.runCommand("fix-cursor-include", ["sh", "-c", script], (output, exitCode) => {
            fixingCursorInclude = false;
            if (exitCode !== 0)
                return;
            checkCursorIncludeStatus();
            SettingsData.updateCompositorCursor();
        });
    }
    function formatThemeAutoTime(isoString) {
        if (!isoString)
            return "";
        try {
            const date = new Date(isoString);
            if (isNaN(date.getTime()))
                return "";
            return date.toLocaleTimeString(Qt.locale(), "HH:mm");
        } catch (e) {
            return "";
        }
    }

    property var cursorPreviewPaths: []
    property bool xcur2pngMissing: false

    function refreshCursorPreview() {
        var ct = (SettingsData.cursorSettings && SettingsData.cursorSettings.theme) ? SettingsData.cursorSettings.theme : "";
        if (!ct)
            ct = "default";
        var script = `theme="$1"
command -v xcur2png >/dev/null 2>&1 || { echo "NO_XCUR2PNG"; exit 0; }
nl="
"
themedir() { for b in /usr/share/icons /usr/local/share/icons "$HOME/.local/share/icons" "$HOME/.icons"; do [ -d "$b/$1" ] && { printf '%s' "$b/$1"; return 0; }; done; return 1; }
order=""
add() { [ -n "$1" ] && order="$order$1$nl"; }
d0=$(themedir "$theme") && add "$d0"
if [ -n "$d0" ] && [ -f "$d0/index.theme" ]; then
  inh=$(grep -m1 '^Inherits=' "$d0/index.theme" | cut -d= -f2)
  oifs=$IFS; IFS=','
  for p in $inh; do IFS=$oifs; p=$(printf '%s' "$p" | sed 's/^ *//;s/ *$//'); pd=$(themedir "$p") && add "$pd"; IFS=','; done
  IFS=$oifs
fi
ad=$(themedir Adwaita) && add "$ad"
base="$XDG_RUNTIME_DIR"
[ -z "$base" ] && base=/tmp
base="$base/hgs-cursor-preview"
mkdir -p "$base"
rm -rf "$base"/* 2>/dev/null
tmp="$base/$$"
mkdir -p "$tmp"
findcursor() { oifs=$IFS; IFS=$nl; for d in $order; do IFS=$oifs; if [ -n "$d" ] && [ -f "$d/cursors/$1" ]; then printf '%s' "$d/cursors/$1"; return 0; fi; IFS=$nl; done; IFS=$oifs; }
for name in left_ptr hand2 sb_v_double_arrow fleur xterm left_side top_left_corner h_double_arrow; do
  cf=$(findcursor "$name")
  [ -z "$cf" ] && continue
  xcur2png "$cf" -d "$tmp" -c "$tmp" -q >/dev/null 2>&1
  png="$tmp/$name""_000.png"
  [ -f "$png" ] && printf '%s\n' "$png"
done`;
        Proc.runCommand("cursor-preview", ["sh", "-c", script, "cursorpreview", ct], (output, code) => {
            if (code !== 0 || !output) {
                appearanceTab.cursorPreviewPaths = [];
                return;
            }
            var trimmed = output.trim();
            if (trimmed === "NO_XCUR2PNG") {
                appearanceTab.xcur2pngMissing = true;
                appearanceTab.cursorPreviewPaths = [];
                return;
            }
            appearanceTab.xcur2pngMissing = false;
            appearanceTab.cursorPreviewPaths = trimmed.split("\n").filter(p => p);
        });
    }

    Component.onCompleted: {
        SettingsData.detectAvailableIconThemes();
        SettingsData.detectAvailableCursorThemes();
        if (HGSService.hgsAvailable)
            HGSService.listInstalledThemes();
        if (PopoutService.pendingThemeInstall)
            Qt.callLater(() => showThemeBrowser());
        checkCursorIncludeStatus();
        fontEnumerationTimer.start();
        appliedIconTheme = SettingsData.iconTheme;
        refreshIconPreview();
        refreshCursorPreview();
    }

    Timer {
        id: fontEnumerationTimer
        interval: 50
        running: false
        onTriggered: {
            if (fontsEnumerated)
                return;
            enumerateFonts();
            fontsEnumerated = true;
        }
    }

    Connections {
        target: HGSService
        function onInstalledThemesReceived(themes) {
            appearanceTab.installedRegistryThemes = themes;
        }
    }

    Connections {
        target: PopoutService
        function onPendingThemeInstallChanged() {
            if (PopoutService.pendingThemeInstall)
                showThemeBrowser();
        }
    }

    HGSFlickable {
        anchors.fill: parent
        visible: appearanceTab.sub === ""
        clip: true
        contentHeight: mainColumn.height + Theme.spacingXL
        contentWidth: width

        Column {
            id: mainColumn
            topPadding: 4

            width: Math.min(550, parent.width - Theme.spacingL * 2)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingXL

            // ===== System Appearance (Light / Dark / Auto) =====
            SettingsCard {
                title: I18n.tr("System Appearance")
                iconName: "brightness_6"

                Column {
                    width: parent.width
                    spacing: Theme.spacingL

                    Row {
                        width: parent.width
                        spacing: Theme.spacingM

                        Repeater {
                            model: [
                                {
                                    "mode": "auto",
                                    "label": I18n.tr("Automatic"),
                                    "icon": "brightness_auto"
                                },
                                {
                                    "mode": "light",
                                    "label": I18n.tr("Light"),
                                    "icon": "light_mode"
                                },
                                {
                                    "mode": "dark",
                                    "label": I18n.tr("Dark"),
                                    "icon": "dark_mode"
                                }
                            ]

                            delegate: Rectangle {
                                width: (parent.width - Theme.spacingM * 2) / 3
                                height: 120
                                radius: Theme.cornerRadius
                                color: Theme.surfaceContainerHigh
                                border.width: appearanceTab.currentMode() === modelData.mode ? 2 : 1
                                border.color: appearanceTab.currentMode() === modelData.mode ? Theme.primary : Theme.outline

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingS
                                    spacing: Theme.spacingS

                                    // Placeholder preview — example image to be added later.
                                    Rectangle {
                                        width: parent.width
                                        height: parent.height - previewLabel.height - Theme.spacingS
                                        radius: Theme.cornerRadius
                                        color: Theme.surfaceContainerHighest
                                        clip: true

                                        HGSIcon {
                                            anchors.centerIn: parent
                                            name: modelData.icon
                                            size: 32
                                            color: Theme.surfaceVariantText
                                        }
                                    }

                                    StyledText {
                                        id: previewLabel
                                        width: parent.width
                                        text: modelData.label
                                        horizontalAlignment: Text.AlignHCenter
                                        font.pixelSize: Theme.fontSizeMedium
                                        font.weight: appearanceTab.currentMode() === modelData.mode ? Font.Bold : Font.Medium
                                        color: Theme.surfaceText
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: appearanceTab.selectMode(modelData.mode)
                                }
                            }
                        }
                    }

                    // Automatic color-mode configuration (visible when Auto is selected).
                    Column {
                        width: parent.width
                        spacing: Theme.spacingM
                        visible: SessionData.themeModeAutoEnabled

                        HGSToggle {
                            width: parent.width
                            text: I18n.tr("Share Gamma Control Settings")
                            checked: SessionData.themeModeShareGammaSettings
                            onToggled: checked => {
                                SessionData.setThemeModeShareGammaSettings(checked);
                            }
                        }

                        Item {
                            width: parent.width
                            height: 45 + Theme.spacingM

                            HGSTabBar {
                                id: themeModeTabBar
                                width: 200
                                height: 45
                                anchors.horizontalCenter: parent.horizontalCenter
                                model: [
                                    {
                                        "text": I18n.tr("Time", "theme auto mode tab"),
                                        "icon": "access_time"
                                    },
                                    {
                                        "text": I18n.tr("Location", "theme auto mode tab"),
                                        "icon": "place"
                                    }
                                ]

                                Component.onCompleted: {
                                    currentIndex = SessionData.themeModeAutoMode === "location" ? 1 : 0;
                                    Qt.callLater(updateIndicator);
                                }

                                onTabClicked: index => {
                                    SessionData.setThemeModeAutoMode(index === 1 ? "location" : "time");
                                    currentIndex = index;
                                }

                                Connections {
                                    target: SessionData
                                    function onThemeModeAutoModeChanged() {
                                        themeModeTabBar.currentIndex = SessionData.themeModeAutoMode === "location" ? 1 : 0;
                                        Qt.callLater(themeModeTabBar.updateIndicator);
                                    }
                                }
                            }
                        }

                        Column {
                            width: parent.width
                            spacing: Theme.spacingM
                            visible: SessionData.themeModeAutoMode === "time" && !SessionData.themeModeShareGammaSettings

                            Column {
                                spacing: Theme.spacingXS
                                anchors.horizontalCenter: parent.horizontalCenter

                                Row {
                                    spacing: Theme.spacingM

                                    StyledText {
                                        text: ""
                                        width: 50
                                        height: 20
                                    }

                                    StyledText {
                                        text: I18n.tr("Hour")
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.surfaceVariantText
                                        width: 70
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    StyledText {
                                        text: I18n.tr("Minute")
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.surfaceVariantText
                                        width: 70
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }

                                Row {
                                    spacing: Theme.spacingM

                                    StyledText {
                                        text: I18n.tr("Start")
                                        font.pixelSize: Theme.fontSizeMedium
                                        color: Theme.surfaceText
                                        width: 50
                                        height: 40
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    HGSDropdown {
                                        dropdownWidth: 70
                                        currentValue: SessionData.themeModeStartHour.toString()
                                        options: {
                                            var hours = [];
                                            for (var i = 0; i < 24; i++)
                                                hours.push(i.toString());
                                            return hours;
                                        }
                                        onValueChanged: value => {
                                            SessionData.setThemeModeStartHour(parseInt(value));
                                        }
                                    }

                                    HGSDropdown {
                                        dropdownWidth: 70
                                        currentValue: SessionData.themeModeStartMinute.toString().padStart(2, '0')
                                        options: {
                                            var minutes = [];
                                            for (var i = 0; i < 60; i += 5) {
                                                minutes.push(i.toString().padStart(2, '0'));
                                            }
                                            return minutes;
                                        }
                                        onValueChanged: value => {
                                            SessionData.setThemeModeStartMinute(parseInt(value));
                                        }
                                    }
                                }

                                Row {
                                    spacing: Theme.spacingM

                                    StyledText {
                                        text: I18n.tr("End")
                                        font.pixelSize: Theme.fontSizeMedium
                                        color: Theme.surfaceText
                                        width: 50
                                        height: 40
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    HGSDropdown {
                                        dropdownWidth: 70
                                        currentValue: SessionData.themeModeEndHour.toString()
                                        options: {
                                            var hours = [];
                                            for (var i = 0; i < 24; i++)
                                                hours.push(i.toString());
                                            return hours;
                                        }
                                        onValueChanged: value => {
                                            SessionData.setThemeModeEndHour(parseInt(value));
                                        }
                                    }

                                    HGSDropdown {
                                        dropdownWidth: 70
                                        currentValue: SessionData.themeModeEndMinute.toString().padStart(2, '0')
                                        options: {
                                            var minutes = [];
                                            for (var i = 0; i < 60; i += 5) {
                                                minutes.push(i.toString().padStart(2, '0'));
                                            }
                                            return minutes;
                                        }
                                        onValueChanged: value => {
                                            SessionData.setThemeModeEndMinute(parseInt(value));
                                        }
                                    }
                                }
                            }
                        }

                        Column {
                            width: parent.width
                            spacing: Theme.spacingM
                            visible: SessionData.themeModeAutoMode === "location" && !SessionData.themeModeShareGammaSettings

                            HGSToggle {
                                id: themeModeIpLocationToggle
                                width: parent.width
                                text: I18n.tr("Use IP Location")
                                checked: SessionData.nightModeUseIPLocation || false
                                onToggled: checked => {
                                    SessionData.setNightModeUseIPLocation(checked);
                                }

                                Connections {
                                    target: SessionData
                                    function onNightModeUseIPLocationChanged() {
                                        themeModeIpLocationToggle.checked = SessionData.nightModeUseIPLocation;
                                    }
                                }
                            }

                            Column {
                                width: parent.width
                                spacing: Theme.spacingM
                                visible: !SessionData.nightModeUseIPLocation

                                StyledText {
                                    text: I18n.tr("Manual Coordinates")
                                    font.pixelSize: Theme.fontSizeMedium
                                    color: Theme.surfaceText
                                    horizontalAlignment: Text.AlignHCenter
                                    width: parent.width
                                }

                                Row {
                                    spacing: Theme.spacingL
                                    anchors.horizontalCenter: parent.horizontalCenter

                                    Column {
                                        spacing: Theme.spacingXS

                                        StyledText {
                                            text: I18n.tr("Latitude")
                                            font.pixelSize: Theme.fontSizeSmall
                                            color: Theme.surfaceVariantText
                                        }

                                        HGSTextField {
                                            width: 120
                                            height: 40
                                            text: SessionData.latitude.toString()
                                            placeholderText: "0.0"
                                            onEditingFinished: {
                                                const lat = parseFloat(text);
                                                if (!isNaN(lat) && lat >= -90 && lat <= 90 && lat !== SessionData.latitude) {
                                                    SessionData.setLatitude(lat);
                                                }
                                            }
                                        }
                                    }

                                    Column {
                                        spacing: Theme.spacingXS

                                        StyledText {
                                            text: I18n.tr("Longitude")
                                            font.pixelSize: Theme.fontSizeSmall
                                            color: Theme.surfaceVariantText
                                        }

                                        HGSTextField {
                                            width: 120
                                            height: 40
                                            text: SessionData.longitude.toString()
                                            placeholderText: "0.0"
                                            onEditingFinished: {
                                                const lon = parseFloat(text);
                                                if (!isNaN(lon) && lon >= -180 && lon <= 180 && lon !== SessionData.longitude) {
                                                    SessionData.setLongitude(lon);
                                                }
                                            }
                                        }
                                    }
                                }

                                StyledText {
                                    text: I18n.tr("Uses sunrise/sunset times based on your location.")
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: Theme.surfaceVariantText
                                    width: parent.width
                                    wrapMode: Text.WordWrap
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                        }

                        StyledText {
                            width: parent.width
                            text: I18n.tr("Using shared settings from Gamma Control")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.primary
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            visible: SessionData.themeModeShareGammaSettings
                        }

                        Rectangle {
                            width: parent.width
                            height: statusRow.implicitHeight + Theme.spacingM * 2
                            radius: Theme.cornerRadius
                            color: Theme.surfaceContainerHigh

                            Row {
                                id: statusRow
                                anchors.centerIn: parent
                                spacing: Theme.spacingL
                                width: parent.width - Theme.spacingM * 2

                                Column {
                                    spacing: 2
                                    width: (parent.width - Theme.spacingL * 2) / 3
                                    anchors.verticalCenter: parent.verticalCenter

                                    Row {
                                        spacing: Theme.spacingS
                                        anchors.horizontalCenter: parent.horizontalCenter

                                        Rectangle {
                                            width: 8
                                            height: 8
                                            radius: 4
                                            color: SessionData.themeModeAutoEnabled ? Theme.success : Theme.error
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        StyledText {
                                            text: I18n.tr("Automation")
                                            font.pixelSize: Theme.fontSizeMedium
                                            font.weight: Font.Medium
                                            color: Theme.surfaceText
                                        }
                                    }

                                    StyledText {
                                        text: SessionData.themeModeAutoEnabled ? I18n.tr("Enabled") : I18n.tr("Disabled")
                                        font.pixelSize: Theme.fontSizeMedium
                                        font.weight: Font.Medium
                                        color: Theme.surfaceText
                                        horizontalAlignment: Text.AlignHCenter
                                        width: parent.width
                                    }
                                }

                                Column {
                                    spacing: 2
                                    width: (parent.width - Theme.spacingL * 2) / 3
                                    anchors.verticalCenter: parent.verticalCenter

                                    Row {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        spacing: Theme.spacingS

                                        HGSIcon {
                                            name: SessionData.isLightMode ? "light_mode" : "dark_mode"
                                            size: Theme.iconSize
                                            color: SessionData.isLightMode ? "#FFA726" : "#7E57C2"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        StyledText {
                                            text: SessionData.isLightMode ? I18n.tr("Light Mode") : I18n.tr("Dark Mode")
                                            font.pixelSize: Theme.fontSizeMedium
                                            font.weight: Font.Bold
                                            color: Theme.surfaceText
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    StyledText {
                                        text: I18n.tr("Active")
                                        font.pixelSize: Theme.fontSizeMedium
                                        font.weight: Font.Medium
                                        color: Theme.surfaceText
                                        horizontalAlignment: Text.AlignHCenter
                                        width: parent.width
                                    }
                                }

                                Column {
                                    spacing: 2
                                    width: (parent.width - Theme.spacingL * 2) / 3
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: SessionData.themeModeAutoEnabled && SessionData.themeModeNextTransition

                                    Row {
                                        spacing: Theme.spacingS
                                        anchors.horizontalCenter: parent.horizontalCenter

                                        HGSIcon {
                                            name: "schedule"
                                            size: Theme.iconSize
                                            color: Theme.primary
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        StyledText {
                                            text: I18n.tr("Next Transition")
                                            font.pixelSize: Theme.fontSizeMedium
                                            font.weight: Font.Medium
                                            color: Theme.surfaceText
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    StyledText {
                                        text: appearanceTab.formatThemeAutoTime(SessionData.themeModeNextTransition)
                                        font.pixelSize: Theme.fontSizeMedium
                                        font.weight: Font.Medium
                                        color: Theme.surfaceText
                                        horizontalAlignment: Text.AlignHCenter
                                        width: parent.width
                                    }
                                }
                            }
                        }
                    }
                }
            }

            StyledText {
                text: I18n.tr("Theme")
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.Bold
                color: Theme.surfaceText
            }

            SettingsCard {
                tab: "theme"
                tags: ["color", "palette", "theme", "appearance"]
                title: I18n.tr("Theme Color")
                settingKey: "themeColor"
                iconName: "palette"

                Column {
                    width: parent.width
                    spacing: Theme.spacingS

                    StyledText {
                        property string registryThemeName: {
                            if (Theme.currentThemeCategory !== "registry")
                                return "";
                            for (var i = 0; i < appearanceTab.installedRegistryThemes.length; i++) {
                                var t = appearanceTab.installedRegistryThemes[i];
                                if (SettingsData.customThemeFile && SettingsData.customThemeFile.endsWith((t.sourceDir || t.id) + "/theme.json"))
                                    return t.name;
                            }
                            return "";
                        }
                        text: {
                            if (Theme.currentTheme === Theme.dynamic)
                                return I18n.tr("Current Theme: %1", "current theme label").arg(I18n.tr("Dynamic", "dynamic theme name"));
                            if (Theme.currentThemeCategory === "registry" && registryThemeName)
                                return I18n.tr("Current Theme: %1", "current theme label").arg(registryThemeName);
                            return I18n.tr("Current Theme: %1", "current theme label").arg(Theme.getThemeColors(Theme.currentThemeName).name);
                        }
                        font.pixelSize: Theme.fontSizeMedium
                        color: Theme.surfaceText
                        font.weight: Font.Medium
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    StyledText {
                        text: {
                            if (Theme.currentTheme === Theme.dynamic)
                                return I18n.tr("Material colors generated from wallpaper", "dynamic theme description");
                            if (Theme.currentThemeCategory === "registry")
                                return I18n.tr("Color theme from HGS registry", "registry theme description");
                            if (Theme.currentTheme === Theme.custom)
                                return I18n.tr("Custom theme loaded from JSON file", "custom theme description");
                            return I18n.tr("Material Design inspired color themes", "generic theme description");
                        }
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.surfaceVariantText
                        anchors.horizontalCenter: parent.horizontalCenter
                        wrapMode: Text.WordWrap
                        width: Math.min(parent.width, 400)
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Column {
                    id: themeCategoryColumn
                    spacing: Theme.spacingM
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width

                    Item {
                        width: parent.width
                        height: themeCategoryGroup.implicitHeight
                        clip: true

                        HGSButtonGroup {
                            id: themeCategoryGroup
                            anchors.horizontalCenter: parent.horizontalCenter
                            buttonPadding: parent.width < 420 ? Theme.spacingS : Theme.spacingL
                            minButtonWidth: parent.width < 420 ? 44 : 64
                            textSize: parent.width < 420 ? Theme.fontSizeSmall : Theme.fontSizeMedium
                            property bool isRegistryTheme: Theme.currentThemeCategory === "registry"
                            property int pendingIndex: -1
                            property int computedIndex: {
                                if (Theme.currentTheme === Theme.dynamic)
                                    return 1;
                                return 0;
                            }

                            model: [I18n.tr("Static", "theme category option"), I18n.tr("Dynamic", "theme category option")]
                            currentIndex: pendingIndex >= 0 ? pendingIndex : computedIndex
                            selectionMode: "single"
                            onSelectionChanged: (index, selected) => {
                                if (!selected)
                                    return;
                                pendingIndex = index;
                            }
                            onAnimationCompleted: {
                                if (pendingIndex < 0)
                                    return;
                                const idx = pendingIndex;
                                pendingIndex = -1;
                                switch (idx) {
                                case 0:
                                    Theme.switchThemeCategory("generic", "blue");
                                    break;
                                case 1:
                                    if (ToastService.wallpaperErrorStatus === "matugen_missing")
                                        ToastService.showError(I18n.tr("matugen not found - install matugen package for dynamic theming", "matugen error"));
                                    else if (ToastService.wallpaperErrorStatus === "error")
                                        ToastService.showError(I18n.tr("Wallpaper processing failed - check wallpaper path", "wallpaper error"));
                                    else
                                        Theme.switchThemeCategory("dynamic", Theme.dynamic);
                                    break;
                                }
                            }
                        }
                    }

                    Item {
                        width: parent.width
                        height: genericColorGrid.implicitHeight + Math.ceil(genericColorGrid.dotSize * 0.05)
                        visible: Theme.currentThemeCategory === "generic" && Theme.currentTheme !== Theme.dynamic && Theme.currentThemeName !== "custom"

                        Grid {
                            id: genericColorGrid
                            property var colorList: ["blue", "purple", "pink", "red", "orange", "amber", "green", "monochrome"]
                            property int dotSize: parent.width < 300 ? 28 : 32
                            columns: colorList.length + 2
                            rowSpacing: Theme.spacingS
                            columnSpacing: Theme.spacingS
                            anchors.horizontalCenter: parent.horizontalCenter

                            Rectangle {
                                id: multiSwatch
                                width: genericColorGrid.dotSize
                                height: genericColorGrid.dotSize
                                radius: width / 2
                                property bool isActive: Theme.currentTheme && typeof Theme.currentTheme === "string" && Theme.currentTheme.startsWith("#")
                                color: isActive ? Theme.currentTheme : "transparent"
                                border.color: Theme.outline
                                border.width: isActive ? 2 : 1
                                scale: isActive ? 1.1 : 1

                                Rectangle {
                                    anchors.fill: parent
                                    radius: parent.radius
                                    visible: !multiSwatch.isActive
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop {
                                            position: 0.0
                                            color: "#ff3b30"
                                        }
                                        GradientStop {
                                            position: 0.2
                                            color: "#ff9500"
                                        }
                                        GradientStop {
                                            position: 0.4
                                            color: "#ffcc00"
                                        }
                                        GradientStop {
                                            position: 0.6
                                            color: "#34c759"
                                        }
                                        GradientStop {
                                            position: 0.8
                                            color: "#007aff"
                                        }
                                        GradientStop {
                                            position: 1.0
                                            color: "#af52de"
                                        }
                                    }
                                }

                                Rectangle {
                                    width: nameTextMulti.contentWidth + Theme.spacingS * 2
                                    height: nameTextMulti.contentHeight + Theme.spacingXS * 2
                                    color: Theme.surfaceContainer
                                    radius: Theme.cornerRadius
                                    anchors.bottom: parent.top
                                    anchors.bottomMargin: Theme.spacingXS
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    visible: multiMouse.containsMouse

                                    StyledText {
                                        id: nameTextMulti
                                        text: I18n.tr("Custom Color")
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.surfaceText
                                        anchors.centerIn: parent
                                    }
                                }

                                MouseArea {
                                    id: multiMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: appearanceTab.openStaticColorPicker()
                                }

                                Behavior on scale {
                                    NumberAnimation {
                                        duration: Theme.shortDuration
                                        easing.type: Theme.emphasizedEasing
                                    }
                                }
                            }

                            Item {
                                width: 1
                                height: genericColorGrid.dotSize

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 1
                                    height: genericColorGrid.dotSize * 0.7
                                    radius: 1
                                    color: Theme.outline
                                    opacity: 0.6
                                }
                            }

                            Repeater {
                                model: genericColorGrid.colorList

                                Rectangle {
                                    required property string modelData
                                    property string themeName: modelData
                                    width: genericColorGrid.dotSize
                                    height: genericColorGrid.dotSize
                                    radius: width / 2
                                    color: Theme.getThemeColors(themeName).primary
                                    border.color: Theme.outline
                                    border.width: (Theme.currentThemeName === themeName && Theme.currentTheme !== Theme.dynamic) ? 2 : 1
                                    scale: (Theme.currentThemeName === themeName && Theme.currentTheme !== Theme.dynamic) ? 1.1 : 1

                                    Rectangle {
                                        width: nameText.contentWidth + Theme.spacingS * 2
                                        height: nameText.contentHeight + Theme.spacingXS * 2
                                        color: Theme.surfaceContainer
                                        radius: Theme.cornerRadius
                                        anchors.bottom: parent.top
                                        anchors.bottomMargin: Theme.spacingXS
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        visible: mouseArea.containsMouse

                                        StyledText {
                                            id: nameText
                                            text: Theme.getThemeColors(parent.parent.themeName).name
                                            font.pixelSize: Theme.fontSizeSmall
                                            color: Theme.surfaceText
                                            anchors.centerIn: parent
                                        }
                                    }

                                    MouseArea {
                                        id: mouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Theme.switchTheme(parent.themeName)
                                    }

                                    Behavior on scale {
                                        NumberAnimation {
                                            duration: Theme.shortDuration
                                            easing.type: Theme.emphasizedEasing
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spacingM
                        visible: Theme.currentTheme === Theme.dynamic && Theme.currentThemeCategory !== "registry"

                        StyledRect {
                            width: parent.width
                            height: 150
                            radius: Theme.cornerRadius
                            color: Theme.surfaceVariant

                            Image {
                                anchors.fill: parent
                                anchors.margins: 1
                                source: {
                                    var wp = Theme.wallpaperPath;
                                    if (!wp || wp === "" || wp.startsWith("#"))
                                        return "";
                                    if (wp.startsWith("file://"))
                                        wp = wp.substring(7);
                                    return "file://" + wp.split('/').map(s => encodeURIComponent(s)).join('/');
                                }
                                fillMode: Image.PreserveAspectCrop
                                visible: Theme.wallpaperPath && !Theme.wallpaperPath.startsWith("#")
                                sourceSize.width: 700
                                sourceSize.height: 300
                                asynchronous: true
                                layer.enabled: true
                                layer.effect: MultiEffect {
                                    maskEnabled: true
                                    maskSource: autoWallpaperMask
                                    maskThresholdMin: 0.5
                                    maskSpreadAtMin: 1
                                }
                            }

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                radius: Theme.cornerRadius - 1
                                color: Theme.wallpaperPath && Theme.wallpaperPath.startsWith("#") ? Theme.wallpaperPath : "transparent"
                                visible: Theme.wallpaperPath && Theme.wallpaperPath.startsWith("#")
                            }

                            Rectangle {
                                id: autoWallpaperMask
                                anchors.fill: parent
                                anchors.margins: 1
                                radius: Theme.cornerRadius - 1
                                color: "black"
                                visible: false
                                layer.enabled: true
                            }

                            HGSIcon {
                                anchors.centerIn: parent
                                name: (ToastService.wallpaperErrorStatus === "error" || ToastService.wallpaperErrorStatus === "matugen_missing") ? "error" : "palette"
                                size: Theme.iconSizeLarge
                                color: (ToastService.wallpaperErrorStatus === "error" || ToastService.wallpaperErrorStatus === "matugen_missing") ? Theme.error : Theme.surfaceVariantText
                                visible: !Theme.wallpaperPath
                            }
                        }

                        SettingsDropdownRow {
                            tab: "theme"
                            tags: ["matugen", "palette", "algorithm", "dynamic"]
                            settingKey: "matugenScheme"
                            text: I18n.tr("Matugen Palette")
                            description: I18n.tr("Select the palette algorithm used for wallpaper-based colors")
                            options: cachedMatugenSchemes
                            currentValue: Theme.getMatugenScheme(SettingsData.matugenScheme).label
                            enabled: Theme.matugenAvailable
                            opacity: enabled ? 1 : 0.4
                            onValueChanged: value => {
                                for (var i = 0; i < Theme.availableMatugenSchemes.length; i++) {
                                    var option = Theme.availableMatugenSchemes[i];
                                    if (option.label === value) {
                                        SettingsData.setMatugenScheme(option.value);
                                        break;
                                    }
                                }
                            }
                        }

                        StyledText {
                            text: {
                                var scheme = Theme.getMatugenScheme(SettingsData.matugenScheme);
                                return scheme.description + " (" + scheme.value + ")";
                            }
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            wrapMode: Text.WordWrap
                            width: parent.width - Theme.spacingM * 2
                            x: Theme.spacingM
                        }

                        SettingsSliderRow {
                            tab: "theme"
                            tags: ["matugen", "contrast", "dynamic"]
                            settingKey: "matugenContrast"
                            text: I18n.tr("Matugen Contrast")
                            description: I18n.tr("Adjusts contrast of generated colors (-100 = minimum, 0 = standard, 100 = maximum)")
                            value: Math.round(SettingsData.matugenContrast * 100)
                            minimum: -100
                            maximum: 100
                            unit: "%"
                            defaultValue: 0
                            enabled: Theme.matugenAvailable
                            opacity: enabled ? 1 : 0.4
                            onSliderDragFinished: finalValue => SettingsData.setMatugenContrast(finalValue / 100)
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spacingM
                        visible: Theme.currentThemeName === "custom" && Theme.currentThemeCategory !== "registry"

                        Row {
                            width: parent.width
                            spacing: Theme.spacingM

                            HGSActionButton {
                                buttonSize: 48
                                iconName: "folder_open"
                                iconSize: Theme.iconSize
                                backgroundColor: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.12)
                                iconColor: Theme.primary
                                onClicked: fileBrowserModal.open()
                            }

                            Column {
                                width: parent.width - 48 - Theme.spacingM
                                spacing: Theme.spacingXS
                                anchors.verticalCenter: parent.verticalCenter

                                StyledText {
                                    text: SettingsData.customThemeFile ? SettingsData.customThemeFile.split('/').pop() : I18n.tr("No custom theme file", "no custom theme file status")
                                    font.pixelSize: Theme.fontSizeLarge
                                    color: Theme.surfaceText
                                    elide: Text.ElideMiddle
                                    maximumLineCount: 1
                                    width: parent.width
                                }

                                StyledText {
                                    text: SettingsData.customThemeFile || I18n.tr("Click to select a custom theme JSON file", "custom theme file hint")
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: Theme.surfaceVariantText
                                    elide: Text.ElideMiddle
                                    maximumLineCount: 1
                                    width: parent.width
                                }
                            }
                        }
                    }

                    Column {
                        id: registrySection
                        width: parent.width
                        spacing: Theme.spacingM
                        visible: Theme.currentThemeCategory === "registry"

                        Grid {
                            id: themeGrid
                            property int cardWidth: registrySection.width < 350 ? 100 : 140
                            property int cardHeight: registrySection.width < 350 ? 72 : 100
                            columns: Math.max(1, Math.floor((registrySection.width + spacing) / (cardWidth + spacing)))
                            spacing: Theme.spacingS
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: appearanceTab.installedRegistryThemes.length > 0

                            Repeater {
                                model: appearanceTab.installedRegistryThemes

                                Rectangle {
                                    id: themeCard
                                    property bool isActive: Theme.currentThemeCategory === "registry" && Theme.currentThemeName === "custom" && SettingsData.customThemeFile && SettingsData.customThemeFile.endsWith((modelData.sourceDir || modelData.id) + "/theme.json")
                                    property bool hasVariants: modelData.hasVariants || false
                                    property var variants: modelData.variants || null
                                    property string selectedVariant: hasVariants ? SettingsData.getRegistryThemeVariant(modelData.id, variants?.default || "") : ""
                                    property string previewPath: {
                                        const baseDir = Quickshell.env("HOME") + "/.config/HyprGlassShell/themes/" + (modelData.sourceDir || modelData.id);
                                        const mode = Theme.isLightMode ? "light" : "dark";
                                        if (hasVariants && selectedVariant)
                                            return baseDir + "/preview-" + selectedVariant + "-" + mode + ".svg";
                                        return baseDir + "/preview-" + mode + ".svg";
                                    }
                                    width: themeGrid.cardWidth
                                    height: themeGrid.cardHeight
                                    radius: Theme.cornerRadius
                                    color: Theme.surfaceVariant
                                    border.color: isActive ? Theme.primary : Theme.outline
                                    border.width: isActive ? 2 : 1
                                    scale: isActive ? 1.03 : 1

                                    Behavior on scale {
                                        NumberAnimation {
                                            duration: Theme.shortDuration
                                            easing.type: Theme.emphasizedEasing
                                        }
                                    }

                                    Image {
                                        id: previewImage
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        source: "file://" + themeCard.previewPath
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        mipmap: true
                                    }

                                    HGSIcon {
                                        anchors.centerIn: parent
                                        name: "palette"
                                        size: themeGrid.cardWidth < 120 ? 24 : 32
                                        color: Theme.primary
                                        visible: previewImage.status === Image.Error || previewImage.status === Image.Null
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: themeGrid.cardWidth < 120 ? 18 : 22
                                        radius: Theme.cornerRadius
                                        color: Qt.rgba(0, 0, 0, 0.6)

                                        StyledText {
                                            anchors.centerIn: parent
                                            text: modelData.name
                                            font.pixelSize: themeGrid.cardWidth < 120 ? Theme.fontSizeSmall - 2 : Theme.fontSizeSmall
                                            color: "white"
                                            font.weight: Font.Medium
                                            elide: Text.ElideRight
                                            width: parent.width - Theme.spacingXS * 2
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }

                                    Rectangle {
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        anchors.margins: themeGrid.cardWidth < 120 ? 2 : 4
                                        width: themeGrid.cardWidth < 120 ? 16 : 20
                                        height: width
                                        radius: width / 2
                                        color: Theme.primary
                                        visible: themeCard.isActive

                                        HGSIcon {
                                            anchors.centerIn: parent
                                            name: "check"
                                            size: themeGrid.cardWidth < 120 ? 10 : 14
                                            color: Theme.surface
                                        }
                                    }

                                    Rectangle {
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.margins: themeGrid.cardWidth < 120 ? 2 : 4
                                        width: themeGrid.cardWidth < 120 ? 16 : 20
                                        height: width
                                        radius: width / 2
                                        color: Theme.secondary
                                        visible: themeCard.hasVariants && !deleteButton.visible

                                        StyledText {
                                            anchors.centerIn: parent
                                            text: {
                                                if (themeCard.variants?.type === "multi")
                                                    return themeCard.variants?.accents?.length || 0;
                                                return themeCard.variants?.options?.length || 0;
                                            }
                                            font.pixelSize: themeGrid.cardWidth < 120 ? Theme.fontSizeSmall - 4 : Theme.fontSizeSmall - 2
                                            color: Theme.surface
                                            font.weight: Font.Bold
                                        }
                                    }

                                    MouseArea {
                                        id: cardMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            const themesDir = Quickshell.env("HOME") + "/.config/HyprGlassShell/themes";
                                            const themePath = themesDir + "/" + (modelData.sourceDir || modelData.id) + "/theme.json";
                                            SettingsData.set("customThemeFile", themePath);
                                            Theme.switchTheme("custom", true, true);
                                        }
                                    }

                                    Rectangle {
                                        id: deleteButton
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.margins: themeGrid.cardWidth < 120 ? 2 : 4
                                        width: themeGrid.cardWidth < 120 ? 18 : 24
                                        height: width
                                        radius: width / 2
                                        color: deleteMouseArea.containsMouse ? Theme.error : Qt.rgba(0, 0, 0, 0.6)
                                        opacity: cardMouseArea.containsMouse || deleteMouseArea.containsMouse ? 1 : 0
                                        visible: opacity > 0

                                        Behavior on opacity {
                                            NumberAnimation {
                                                duration: Theme.shortDuration
                                            }
                                        }

                                        HGSIcon {
                                            anchors.centerIn: parent
                                            name: "close"
                                            size: themeGrid.cardWidth < 120 ? 10 : 14
                                            color: "white"
                                        }

                                        MouseArea {
                                            id: deleteMouseArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                ToastService.showInfo(I18n.tr("Uninstalling: %1", "uninstallation progress").arg(modelData.name));
                                                HGSService.uninstallTheme(modelData.id, response => {
                                                    if (response.error) {
                                                        ToastService.showError(I18n.tr("Uninstall failed: %1", "uninstallation error").arg(response.error));
                                                        return;
                                                    }
                                                    ToastService.showInfo(I18n.tr("Uninstalled: %1", "uninstallation success").arg(modelData.name));
                                                    HGSService.listInstalledThemes();
                                                });
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        StyledText {
                            text: I18n.tr("No themes installed. Browse themes to install from the registry.", "no registry themes installed hint")
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.surfaceVariantText
                            wrapMode: Text.WordWrap
                            width: parent.width
                            visible: appearanceTab.installedRegistryThemes.length === 0
                            horizontalAlignment: Text.AlignHCenter
                        }

                        HGSButton {
                            text: I18n.tr("Browse Themes", "browse themes button")
                            iconName: "store"
                            anchors.horizontalCenter: parent.horizontalCenter
                            onClicked: showThemeBrowser()
                        }
                    }

                    Column {
                        id: variantSelector
                        width: parent.width
                        spacing: Theme.spacingS
                        visible: activeThemeId !== "" && activeThemeVariants !== null && (isMultiVariant || (activeThemeVariants.options && activeThemeVariants.options.length > 0))

                        property string activeThemeId: {
                            switch (Theme.currentThemeCategory) {
                            case "registry":
                                if (Theme.currentTheme !== "custom")
                                    return "";
                                for (var i = 0; i < appearanceTab.installedRegistryThemes.length; i++) {
                                    var t = appearanceTab.installedRegistryThemes[i];
                                    if (SettingsData.customThemeFile && SettingsData.customThemeFile.endsWith((t.sourceDir || t.id) + "/theme.json"))
                                        return t.id;
                                }
                                return "";
                            case "custom":
                                return Theme.currentThemeId || "";
                            default:
                                return "";
                            }
                        }
                        property var activeThemeVariants: {
                            if (!activeThemeId)
                                return null;
                            switch (Theme.currentThemeCategory) {
                            case "registry":
                                for (var i = 0; i < appearanceTab.installedRegistryThemes.length; i++) {
                                    var t = appearanceTab.installedRegistryThemes[i];
                                    if (t.id === activeThemeId && t.hasVariants)
                                        return t.variants;
                                }
                                return null;
                            case "custom":
                                return Theme.currentThemeVariants || null;
                            default:
                                return null;
                            }
                        }
                        property bool isMultiVariant: activeThemeVariants?.type === "multi"
                        property string colorMode: Theme.isLightMode ? "light" : "dark"
                        property var multiDefaults: {
                            if (!isMultiVariant || !activeThemeVariants?.defaults)
                                return {};
                            return activeThemeVariants.defaults[colorMode] || activeThemeVariants.defaults.dark || {};
                        }
                        property var storedMulti: activeThemeId ? SettingsData.getRegistryThemeMultiVariant(activeThemeId, multiDefaults, colorMode) : multiDefaults
                        property string selectedFlavor: {
                            var sf = storedMulti.flavor || multiDefaults.flavor || "";
                            for (var i = 0; i < flavorOptions.length; i++) {
                                if (flavorOptions[i].id === sf)
                                    return sf;
                            }
                            if (flavorOptions.length > 0)
                                return flavorOptions[0].id;
                            return sf;
                        }
                        property string selectedAccent: storedMulti.accent || multiDefaults.accent || ""
                        property var flavorOptions: {
                            if (!isMultiVariant || !activeThemeVariants?.flavors)
                                return [];
                            return activeThemeVariants.flavors.filter(f => {
                                if (f.mode)
                                    return f.mode === colorMode || f.mode === "both";
                                return !!f[colorMode];
                            });
                        }
                        property var flavorNames: flavorOptions.map(f => f.name)
                        property int flavorIndex: {
                            for (var i = 0; i < flavorOptions.length; i++) {
                                if (flavorOptions[i].id === selectedFlavor)
                                    return i;
                            }
                            return 0;
                        }
                        property string selectedVariant: activeThemeId ? SettingsData.getRegistryThemeVariant(activeThemeId, activeThemeVariants?.default || "") : ""
                        property var variantNames: {
                            if (!activeThemeVariants?.options)
                                return [];
                            return activeThemeVariants.options.map(v => v.name);
                        }
                        property int selectedIndex: {
                            if (!activeThemeVariants?.options || !selectedVariant)
                                return 0;
                            for (var i = 0; i < activeThemeVariants.options.length; i++) {
                                if (activeThemeVariants.options[i].id === selectedVariant)
                                    return i;
                            }
                            return 0;
                        }

                        Item {
                            width: parent.width
                            height: flavorButtonGroup.implicitHeight
                            clip: true
                            visible: variantSelector.isMultiVariant && variantSelector.flavorOptions.length > 1

                            HGSButtonGroup {
                                id: flavorButtonGroup
                                anchors.horizontalCenter: parent.horizontalCenter
                                property int _count: variantSelector.flavorNames.length
                                property real _maxPerItem: _count > 1 ? (parent.width - (_count - 1) * spacing) / _count : parent.width
                                buttonPadding: _maxPerItem < 55 ? Theme.spacingXS : (_maxPerItem < 75 ? Theme.spacingS : Theme.spacingL)
                                minButtonWidth: Math.min(_maxPerItem < 55 ? 28 : (_maxPerItem < 75 ? 44 : 64), Math.max(28, Math.floor(_maxPerItem)))
                                textSize: _maxPerItem < 55 ? Theme.fontSizeSmall - 2 : (_maxPerItem < 75 ? Theme.fontSizeSmall : Theme.fontSizeMedium)
                                checkEnabled: _maxPerItem >= 55
                                property int pendingIndex: -1
                                model: variantSelector.flavorNames
                                currentIndex: pendingIndex >= 0 ? pendingIndex : variantSelector.flavorIndex
                                selectionMode: "single"
                                onSelectionChanged: (index, selected) => {
                                    if (!selected)
                                        return;
                                    pendingIndex = index;
                                }
                                onAnimationCompleted: {
                                    if (pendingIndex < 0 || pendingIndex >= variantSelector.flavorOptions.length)
                                        return;
                                    const flavorId = variantSelector.flavorOptions[pendingIndex]?.id;
                                    const idx = pendingIndex;
                                    pendingIndex = -1;
                                    if (!flavorId || flavorId === variantSelector.selectedFlavor)
                                        return;
                                    Theme.screenTransition();
                                    SettingsData.setRegistryThemeMultiVariant(variantSelector.activeThemeId, flavorId, variantSelector.selectedAccent, variantSelector.colorMode);
                                }
                            }
                        }

                        Item {
                            width: parent.width
                            height: accentColorsGrid.implicitHeight
                            visible: variantSelector.isMultiVariant && variantSelector.activeThemeVariants?.accents?.length > 0

                            Grid {
                                id: accentColorsGrid
                                property int accentCount: variantSelector.activeThemeVariants?.accents?.length ?? 0
                                property int dotSize: parent.width < 300 ? 28 : 32
                                columns: accentCount > 0 ? Math.ceil(accentCount / 2) : 1
                                rowSpacing: Theme.spacingS
                                columnSpacing: Theme.spacingS
                                anchors.horizontalCenter: parent.horizontalCenter

                                Repeater {
                                    model: variantSelector.activeThemeVariants?.accents || []

                                    Rectangle {
                                        required property var modelData
                                        required property int index
                                        property string accentId: modelData.id
                                        property bool isSelected: accentId === variantSelector.selectedAccent
                                        width: accentColorsGrid.dotSize
                                        height: accentColorsGrid.dotSize
                                        radius: width / 2
                                        color: modelData.color || modelData[variantSelector.selectedFlavor]?.primary || Theme.primary
                                        border.color: Theme.outline
                                        border.width: isSelected ? 2 : 1
                                        scale: isSelected ? 1.1 : 1

                                        Rectangle {
                                            width: accentNameText.contentWidth + Theme.spacingS * 2
                                            height: accentNameText.contentHeight + Theme.spacingXS * 2
                                            color: Theme.surfaceContainer
                                            radius: Theme.cornerRadius
                                            anchors.bottom: parent.top
                                            anchors.bottomMargin: Theme.spacingXS
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            visible: accentMouseArea.containsMouse

                                            StyledText {
                                                id: accentNameText
                                                text: modelData.name
                                                font.pixelSize: Theme.fontSizeSmall
                                                color: Theme.surfaceText
                                                anchors.centerIn: parent
                                            }
                                        }

                                        MouseArea {
                                            id: accentMouseArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (parent.isSelected)
                                                    return;
                                                Theme.screenTransition();
                                                SettingsData.setRegistryThemeMultiVariant(variantSelector.activeThemeId, variantSelector.selectedFlavor, parent.accentId, variantSelector.colorMode);
                                            }
                                        }

                                        Behavior on scale {
                                            NumberAnimation {
                                                duration: Theme.shortDuration
                                                easing.type: Theme.emphasizedEasing
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            width: parent.width
                            height: variantButtonGroup.implicitHeight
                            clip: true
                            visible: !variantSelector.isMultiVariant && variantSelector.variantNames.length > 0

                            HGSButtonGroup {
                                id: variantButtonGroup
                                anchors.horizontalCenter: parent.horizontalCenter
                                property int _count: variantSelector.variantNames.length
                                property real _maxPerItem: _count > 1 ? (parent.width - (_count - 1) * spacing) / _count : parent.width
                                buttonPadding: _maxPerItem < 55 ? Theme.spacingXS : (_maxPerItem < 75 ? Theme.spacingS : Theme.spacingL)
                                minButtonWidth: Math.min(_maxPerItem < 55 ? 28 : (_maxPerItem < 75 ? 44 : 64), Math.max(28, Math.floor(_maxPerItem)))
                                textSize: _maxPerItem < 55 ? Theme.fontSizeSmall - 2 : (_maxPerItem < 75 ? Theme.fontSizeSmall : Theme.fontSizeMedium)
                                checkEnabled: _maxPerItem >= 55
                                property int pendingIndex: -1
                                model: variantSelector.variantNames
                                currentIndex: pendingIndex >= 0 ? pendingIndex : variantSelector.selectedIndex
                                selectionMode: "single"
                                onSelectionChanged: (index, selected) => {
                                    if (!selected)
                                        return;
                                    pendingIndex = index;
                                }
                                onAnimationCompleted: {
                                    if (pendingIndex < 0 || !variantSelector.activeThemeVariants?.options)
                                        return;
                                    const variantId = variantSelector.activeThemeVariants.options[pendingIndex]?.id;
                                    const idx = pendingIndex;
                                    pendingIndex = -1;
                                    if (!variantId || variantId === variantSelector.selectedVariant)
                                        return;
                                    Theme.screenTransition();
                                    SettingsData.setRegistryThemeVariant(variantSelector.activeThemeId, variantId);
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.outline
                    opacity: 0.15
                }

                SettingsSliderRow {
                    tab: "theme"
                    tags: ["surface", "tint", "neutral", "chroma", "gray", "macos"]
                    settingKey: "surfaceTintStrength"
                    text: I18n.tr("Surface Tint")
                    description: I18n.tr("How much accent hue bleeds into backgrounds — 0% = neutral grays, 100% = full Material tint")
                    minimum: 0
                    maximum: 100
                    value: Math.round(SettingsData.surfaceTintStrength * 100)
                    unit: "%"
                    defaultValue: 0
                    onSliderValueChanged: newValue => SettingsData.setSurfaceTintStrength(newValue / 100)
                }
            }
            SettingsCard {
                tab: "theme"
                tags: ["icon", "theme", "system"]
                title: I18n.tr("Icon Theme")
                settingKey: "iconTheme"
                iconName: "interests"

                SettingsDropdownRow {
                    tab: "theme"
                    tags: ["icon", "theme", "system"]
                    settingKey: "iconTheme"
                    text: I18n.tr("Icon Theme")
                    description: I18n.tr("System & HGS icons")
                    currentValue: SettingsData.iconTheme
                    enableFuzzySearch: true
                    popupWidthOffset: 100
                    maxPopupHeight: 236
                    options: cachedIconThemes
                    onValueChanged: value => {
                        SettingsData.setIconTheme(value);
                        appearanceTab.refreshIconPreview();
                        if (Quickshell.env("QT_QPA_PLATFORMTHEME") != "gtk3" && Quickshell.env("QT_QPA_PLATFORMTHEME") != "qt6ct" && Quickshell.env("QT_QPA_PLATFORMTHEME_QT6") != "qt6ct") {
                            ToastService.showError(I18n.tr("Missing Environment Variables", "qt theme env error title"), I18n.tr("You need to set either:\nQT_QPA_PLATFORMTHEME=gtk3 OR\nQT_QPA_PLATFORMTHEME=qt6ct\nas environment variables, and then restart the shell.\n\nqt6ct requires qt6ct-kde to be installed.", "qt theme env error body"));
                        }
                    }
                }

                // Restart Required — shown only after the selection changes.
                Row {
                    x: Theme.spacingM
                    width: parent.width - Theme.spacingM * 2
                    spacing: Theme.spacingS
                    visible: appearanceTab.appliedIconTheme !== "" && SettingsData.iconTheme !== appearanceTab.appliedIconTheme

                    HGSIcon {
                        name: "restart_alt"
                        size: Theme.iconSize - 2
                        color: Theme.warning
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    StyledText {
                        text: I18n.tr("Restart Required for the new icon theme to take effect")
                        color: Theme.warning
                        font.pixelSize: Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                        width: parent.width - Theme.iconSize - Theme.spacingS
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // Preview — framed icon grid mirroring nwg-look.
                Rectangle {
                    x: Theme.spacingM
                    width: parent.width - Theme.spacingM * 2
                    visible: appearanceTab.iconPreviewMain.length > 0 || appearanceTab.iconPreviewSymbolic.length > 0
                    height: previewCol.height + Theme.spacingL * 2
                    radius: Theme.cornerRadius
                    color: Theme.surfaceContainerHighest
                    border.color: Theme.outline
                    border.width: 1

                    Column {
                        id: previewCol
                        anchors.top: parent.top
                        anchors.topMargin: Theme.spacingL
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width - Theme.spacingL * 2
                        spacing: Theme.spacingM

                        StyledText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: I18n.tr("Preview")
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.Medium
                            color: Theme.surfaceVariantText
                        }

                        Grid {
                            anchors.horizontalCenter: parent.horizontalCenter
                            columns: 7
                            columnSpacing: Theme.spacingM
                            rowSpacing: Theme.spacingM

                            Repeater {
                                model: appearanceTab.iconPreviewMain

                                delegate: Image {
                                    source: "file://" + modelData
                                    sourceSize.width: 48
                                    sourceSize.height: 48
                                    width: 48
                                    height: 48
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    asynchronous: true
                                }
                            }
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: Theme.spacingL
                            visible: appearanceTab.iconPreviewSymbolic.length > 0

                            Repeater {
                                model: appearanceTab.iconPreviewSymbolic

                                delegate: Item {
                                    width: 16
                                    height: 16

                                    Image {
                                        id: symSrc
                                        anchors.fill: parent
                                        source: "file://" + modelData
                                        sourceSize.width: 16
                                        sourceSize.height: 16
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        asynchronous: true
                                        visible: false
                                    }

                                    MultiEffect {
                                        anchors.fill: symSrc
                                        source: symSrc
                                        colorization: 1.0
                                        colorizationColor: Theme.surfaceText
                                    }
                                }
                            }
                        }
                    }
                }
            }
            SettingsCard {
                tab: "theme"
                tags: ["cursor", "mouse", "pointer", "theme", "size"]
                title: I18n.tr("Cursor")
                settingKey: "cursorTheme"
                iconName: "mouse"
                Column {
                    width: parent.width
                    spacing: Theme.spacingM

                    StyledRect {
                        id: cursorWarningBox
                        width: parent.width
                        height: cursorWarningContent.implicitHeight + Theme.spacingM * 2
                        radius: Theme.cornerRadius

                        readonly property bool showError: appearanceTab.cursorIncludeStatus.exists && !appearanceTab.cursorIncludeStatus.included
                        readonly property bool showSetup: !appearanceTab.cursorIncludeStatus.exists && !appearanceTab.cursorIncludeStatus.included

                        color: (showError || showSetup) ? Theme.withAlpha(Theme.warning, 0.15) : "transparent"
                        border.color: (showError || showSetup) ? Theme.withAlpha(Theme.warning, 0.3) : "transparent"
                        border.width: 1
                        visible: (showError || showSetup) && !appearanceTab.checkingCursorInclude

                        Row {
                            id: cursorWarningContent
                            anchors.fill: parent
                            anchors.margins: Theme.spacingM
                            spacing: Theme.spacingM

                            HGSIcon {
                                name: "warning"
                                size: Theme.iconSize
                                color: Theme.warning
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Column {
                                width: parent.width - Theme.iconSize - (cursorFixButton.visible ? cursorFixButton.width + Theme.spacingM : 0) - Theme.spacingM
                                spacing: Theme.spacingXS
                                anchors.verticalCenter: parent.verticalCenter

                                StyledText {
                                    text: cursorWarningBox.showSetup ? I18n.tr("Cursor Config Not Configured") : I18n.tr("Cursor Include Missing")
                                    font.pixelSize: Theme.fontSizeMedium
                                    font.weight: Font.Medium
                                    color: Theme.warning
                                }

                                StyledText {
                                    text: cursorWarningBox.showSetup ? I18n.tr("Click 'Setup' to create cursor config and add include to your compositor config.") : I18n.tr("hgs/cursor config exists but is not included. Cursor settings won't apply.")
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: Theme.surfaceVariantText
                                    wrapMode: Text.WordWrap
                                    width: parent.width
                                }
                            }

                            HGSButton {
                                id: cursorFixButton
                                visible: cursorWarningBox.showError || cursorWarningBox.showSetup
                                text: appearanceTab.fixingCursorInclude ? I18n.tr("Fixing...") : (cursorWarningBox.showSetup ? I18n.tr("Setup") : I18n.tr("Fix Now"))
                                backgroundColor: Theme.warning
                                textColor: Theme.background
                                enabled: !appearanceTab.fixingCursorInclude
                                anchors.verticalCenter: parent.verticalCenter
                                onClicked: appearanceTab.fixCursorInclude()
                            }
                        }
                    }

                    SettingsDropdownRow {
                        tab: "theme"
                        tags: ["cursor", "mouse", "pointer", "theme"]
                        settingKey: "cursorTheme"
                        text: I18n.tr("Cursor Theme")
                        description: I18n.tr("Mouse pointer appearance")
                        currentValue: SettingsData.cursorSettings.theme
                        enableFuzzySearch: true
                        popupWidthOffset: 100
                        maxPopupHeight: 236
                        options: cachedCursorThemes
                        onValueChanged: value => {
                            SettingsData.setCursorTheme(value);
                            appearanceTab.refreshCursorPreview();
                        }
                    }

                    SettingsSliderRow {
                        tab: "theme"
                        tags: ["cursor", "mouse", "pointer", "size"]
                        settingKey: "cursorSize"
                        text: I18n.tr("Cursor Size")
                        description: I18n.tr("Mouse pointer size in pixels")
                        value: SettingsData.cursorSettings.size
                        minimum: 12
                        maximum: 128
                        unit: "px"
                        defaultValue: 24
                        onSliderValueChanged: newValue => SettingsData.setCursorSize(newValue)
                    }

                    // Cursor preview — extracted via xcur2png, mirroring nwg-look.
                    Rectangle {
                        width: parent.width
                        visible: appearanceTab.cursorPreviewPaths.length > 0 || appearanceTab.xcur2pngMissing
                        height: cursorPrevCol.height + Theme.spacingL * 2
                        radius: Theme.cornerRadius
                        color: Theme.surfaceContainerHighest
                        border.color: Theme.outline
                        border.width: 1

                        Column {
                            id: cursorPrevCol
                            anchors.top: parent.top
                            anchors.topMargin: Theme.spacingL
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - Theme.spacingL * 2
                            spacing: Theme.spacingM

                            StyledText {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: I18n.tr("Preview")
                                font.pixelSize: Theme.fontSizeSmall
                                font.weight: Font.Medium
                                color: Theme.surfaceVariantText
                            }

                            StyledText {
                                visible: appearanceTab.xcur2pngMissing
                                width: parent.width
                                text: I18n.tr("Install the “xcur2png” package to preview cursors")
                                color: Theme.warning
                                font.pixelSize: Theme.fontSizeSmall
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: Theme.spacingL
                                visible: appearanceTab.cursorPreviewPaths.length > 0

                                Repeater {
                                    model: appearanceTab.cursorPreviewPaths

                                    delegate: Image {
                                        source: "file://" + modelData
                                        sourceSize.width: 28
                                        sourceSize.height: 28
                                        width: 28
                                        height: 28
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        asynchronous: true
                                        cache: false
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.outline
                        opacity: 0.15
                    }

                    // Advanced Settings — opens the Advanced Cursor Settings sub-page.
                    Item {
                        x: Theme.spacingM
                        width: parent.width - Theme.spacingM * 2
                        height: cursorAdvLink.implicitHeight

                        StyledText {
                            id: cursorAdvLink
                            anchors.right: parent.right
                            text: I18n.tr("Advanced Settings")
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.Medium
                            color: Theme.primary

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: appearanceTab.sub = "advancedCursor"
                            }
                        }
                    }
                }
            }
            SettingsCard {
                tab: "typography"
                tags: ["font", "family", "text", "system"]
                title: I18n.tr("Font")
                settingKey: "fonts"
                iconName: "text_fields"

                SettingsDropdownRow {
                    tab: "typography"
                    tags: ["font", "family", "system", "ui"]
                    settingKey: "fontFamily"
                    previewOptionFont: true
                    text: I18n.tr("System Font")
                    description: I18n.tr("Used across the entire interface")
                    options: appearanceTab.fontsEnumerated ? appearanceTab.cachedFontFamilies : ["Default"]
                    currentValue: SettingsData.fontFamily === Theme.defaultFontFamily ? "Default" : (SettingsData.fontFamily || "Default")
                    enableFuzzySearch: true
                    popupWidthOffset: 100
                    maxPopupHeight: 400
                    onValueChanged: value => {
                        if (value === "Default")
                            SettingsData.set("fontFamily", Theme.defaultFontFamily);
                        else
                            SettingsData.set("fontFamily", value);
                    }
                }

                SettingsDropdownRow {
                    tab: "typography"
                    tags: ["font", "monospace", "code", "terminal"]
                    settingKey: "monoFontFamily"
                    previewOptionFont: true
                    text: I18n.tr("Monospace Font")
                    description: I18n.tr("Used for process lists and technical displays")
                    options: appearanceTab.fontsEnumerated ? appearanceTab.cachedMonoFamilies : ["Default"]
                    currentValue: SettingsData.monoFontFamily === Theme.defaultMonoFontFamily ? "Default" : (SettingsData.monoFontFamily || "Default")
                    enableFuzzySearch: true
                    popupWidthOffset: 100
                    maxPopupHeight: 400
                    onValueChanged: value => {
                        if (value === "Default")
                            SettingsData.set("monoFontFamily", Theme.defaultMonoFontFamily);
                        else
                            SettingsData.set("monoFontFamily", value);
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.outline
                    opacity: 0.15
                }

                SettingsDropdownRow {
                    tab: "typography"
                    tags: ["font", "weight", "bold", "light"]
                    settingKey: "fontWeight"
                    text: I18n.tr("Font Weight")
                    description: I18n.tr("Lighter or bolder interface text")
                    options: [I18n.tr("Thin", "font weight"), I18n.tr("Extra Light", "font weight"), I18n.tr("Light", "font weight"), I18n.tr("Regular", "font weight"), I18n.tr("Medium", "font weight"), I18n.tr("Demi Bold", "font weight"), I18n.tr("Bold", "font weight"), I18n.tr("Extra Bold", "font weight"), I18n.tr("Black", "font weight")]
                    currentValue: {
                        switch (SettingsData.fontWeight) {
                        case Font.Thin:
                            return I18n.tr("Thin", "font weight");
                        case Font.ExtraLight:
                            return I18n.tr("Extra Light", "font weight");
                        case Font.Light:
                            return I18n.tr("Light", "font weight");
                        case Font.Normal:
                            return I18n.tr("Regular", "font weight");
                        case Font.Medium:
                            return I18n.tr("Medium", "font weight");
                        case Font.DemiBold:
                            return I18n.tr("Demi Bold", "font weight");
                        case Font.Bold:
                            return I18n.tr("Bold", "font weight");
                        case Font.ExtraBold:
                            return I18n.tr("Extra Bold", "font weight");
                        case Font.Black:
                            return I18n.tr("Black", "font weight");
                        default:
                            return I18n.tr("Regular", "font weight");
                        }
                    }
                    onValueChanged: value => {
                        var weight;
                        switch (value) {
                        case I18n.tr("Thin", "font weight"):
                            weight = Font.Thin;
                            break;
                        case I18n.tr("Extra Light", "font weight"):
                            weight = Font.ExtraLight;
                            break;
                        case I18n.tr("Light", "font weight"):
                            weight = Font.Light;
                            break;
                        case I18n.tr("Regular", "font weight"):
                            weight = Font.Normal;
                            break;
                        case I18n.tr("Medium", "font weight"):
                            weight = Font.Medium;
                            break;
                        case I18n.tr("Demi Bold", "font weight"):
                            weight = Font.DemiBold;
                            break;
                        case I18n.tr("Bold", "font weight"):
                            weight = Font.Bold;
                            break;
                        case I18n.tr("Extra Bold", "font weight"):
                            weight = Font.ExtraBold;
                            break;
                        case I18n.tr("Black", "font weight"):
                            weight = Font.Black;
                            break;
                        default:
                            weight = Font.Normal;
                            break;
                        }
                        SettingsData.set("fontWeight", weight);
                    }
                }

                SettingsSliderRow {
                    tab: "typography"
                    tags: ["font", "scale", "size", "zoom"]
                    settingKey: "fontScale"
                    text: I18n.tr("Font Scale")
                    description: I18n.tr("Resize all fonts throughout HGS")
                    minimum: 75
                    maximum: 150
                    value: Math.round(SettingsData.fontScale * 100)
                    unit: "%"
                    defaultValue: 100
                    onSliderValueChanged: newValue => SettingsData.set("fontScale", newValue / 100)
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.outline
                    opacity: 0.15
                }

                // Advanced Settings — opens the Text Rendering sub-page.
                Item {
                    x: Theme.spacingM
                    width: parent.width - Theme.spacingM * 2
                    height: advLink.implicitHeight

                    StyledText {
                        id: advLink
                        anchors.right: parent.right
                        text: I18n.tr("Advanced Settings")
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight: Font.Medium
                        color: Theme.primary

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appearanceTab.sub = "advancedFonts"
                        }
                    }
                }
            }
        }
    }


    // ===== Advanced Font Settings sub-page (drill-in from the Fonts card) =====
    Item {
        anchors.fill: parent
        visible: appearanceTab.sub === "advancedFonts"

        Item {
            id: advBackBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 44

            HGSActionButton {
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingM
                anchors.verticalCenter: parent.verticalCenter
                circular: false
                iconName: "arrow_back"
                iconColor: Theme.surfaceText
                onClicked: appearanceTab.sub = ""
            }

            StyledText {
                anchors.centerIn: parent
                text: I18n.tr("Advanced Font Settings")
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.Medium
                color: Theme.surfaceText
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.outline
                opacity: 0.12
            }
        }

        Item {
            anchors.top: advBackBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.topMargin: Theme.spacingS
            clip: true

            HGSFlickable {
                anchors.fill: parent
                clip: true
                contentHeight: advColumn.height + Theme.spacingXL
                contentWidth: width

                Column {
                    id: advColumn
                    topPadding: 4
                    width: Math.min(550, parent.width - Theme.spacingL * 2)
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Theme.spacingXL

            SettingsCard {
                tab: "typography"
                tags: ["text", "render", "rendering", "quality", "anti-aliasing", "freetype", "distance", "field"]
                title: I18n.tr("Text Rendering")
                settingKey: "textRenderType"
                iconName: "text_format"

                Item {
                    width: parent.width
                    height: renderTypeGroup.implicitHeight
                    clip: true

                    HGSButtonGroup {
                        id: renderTypeGroup
                        anchors.horizontalCenter: parent.horizontalCenter
                        buttonPadding: parent.width < 480 ? Theme.spacingS : Theme.spacingL
                        minButtonWidth: parent.width < 480 ? 64 : 96
                        textSize: parent.width < 480 ? Theme.fontSizeSmall : Theme.fontSizeMedium
                        model: [I18n.tr("Native"), I18n.tr("Qt"), I18n.tr("Curve")]
                        selectionMode: "single"
                        currentIndex: {
                            switch (SettingsData.textRenderType) {
                            case SettingsData.TextRenderType.Qt:
                                return 1;
                            case SettingsData.TextRenderType.Curve:
                                return 2;
                            default:
                                return 0;
                            }
                        }
                        onSelectionChanged: (index, selected) => {
                            if (!selected)
                                return;
                            switch (index) {
                            case 1:
                                SettingsData.set("textRenderType", SettingsData.TextRenderType.Qt);
                                break;
                            case 2:
                                SettingsData.set("textRenderType", SettingsData.TextRenderType.Curve);
                                break;
                            default:
                                SettingsData.set("textRenderType", SettingsData.TextRenderType.Native);
                                break;
                            }
                        }

                        Connections {
                            target: SettingsData
                            function onTextRenderTypeChanged() {
                                switch (SettingsData.textRenderType) {
                                case SettingsData.TextRenderType.Qt:
                                    renderTypeGroup.currentIndex = 1;
                                    break;
                                case SettingsData.TextRenderType.Curve:
                                    renderTypeGroup.currentIndex = 2;
                                    break;
                                default:
                                    renderTypeGroup.currentIndex = 0;
                                    break;
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.outline
                    opacity: 0.15
                }

                Item {
                    width: parent.width
                    height: renderTypeDescription.implicitHeight + Theme.spacingS * 2

                    StyledText {
                        id: renderTypeDescription
                        x: Theme.spacingM
                        y: Theme.spacingS
                        width: parent.width - Theme.spacingM * 2
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.surfaceVariantText
                        wrapMode: Text.WordWrap
                        text: {
                            switch (SettingsData.textRenderType) {
                            case SettingsData.TextRenderType.Qt:
                                return I18n.tr("Qt: distance-field renderer.");
                            case SettingsData.TextRenderType.Curve:
                                return I18n.tr("Curve: curve rasterizer.");
                            default:
                                return I18n.tr("Native: platform renderer (FreeType).");
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.outline
                    opacity: 0.15
                }

                Item {
                    width: parent.width
                    height: qualityGroup.implicitHeight + qualityLabel.implicitHeight + Theme.spacingS
                    clip: true

                    StyledText {
                        id: qualityLabel
                        x: Theme.spacingM
                        text: I18n.tr("Quality")
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight: Font.Medium
                        color: Theme.surfaceText
                    }

                    HGSButtonGroup {
                        id: qualityGroup
                        anchors.top: qualityLabel.bottom
                        anchors.topMargin: Theme.spacingS
                        anchors.horizontalCenter: parent.horizontalCenter
                        buttonPadding: parent.width < 480 ? Theme.spacingXS : Theme.spacingS
                        minButtonWidth: parent.width < 480 ? 40 : 56
                        textSize: parent.width < 480 ? Theme.fontSizeSmall : Theme.fontSizeMedium
                        model: [I18n.tr("Default"), I18n.tr("Low"), I18n.tr("Normal"), I18n.tr("High"), I18n.tr("Very High")]
                        selectionMode: "single"
                        currentIndex: SettingsData.textRenderQuality
                        onSelectionChanged: (index, selected) => {
                            if (!selected)
                                return;
                            SettingsData.set("textRenderQuality", index);
                        }

                        Connections {
                            target: SettingsData
                            function onTextRenderQualityChanged() {
                                qualityGroup.currentIndex = SettingsData.textRenderQuality;
                            }
                        }
                    }
                }
            }

                }
            }
        }
    }


    // ===== Advanced Cursor Settings sub-page (drill-in from the Cursor card) =====
    Item {
        anchors.fill: parent
        visible: appearanceTab.sub === "advancedCursor"

        Item {
            id: cursorAdvBackBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 44

            HGSActionButton {
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingM
                anchors.verticalCenter: parent.verticalCenter
                circular: false
                iconName: "arrow_back"
                iconColor: Theme.surfaceText
                onClicked: appearanceTab.sub = ""
            }

            StyledText {
                anchors.centerIn: parent
                text: I18n.tr("Advanced Cursor Settings")
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.Medium
                color: Theme.surfaceText
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.outline
                opacity: 0.12
            }
        }

        Item {
            anchors.top: cursorAdvBackBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.topMargin: Theme.spacingS
            clip: true

            HGSFlickable {
                anchors.fill: parent
                clip: true
                contentHeight: cursorAdvColumn.height + Theme.spacingXL
                contentWidth: width

                Column {
                    id: cursorAdvColumn
                    topPadding: 4
                    width: Math.min(550, parent.width - Theme.spacingL * 2)
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Theme.spacingXL

                    SettingsCard {
                        tab: "theme"
                        tags: ["cursor", "hide", "visibility", "auto"]
                        title: I18n.tr("Cursor Visibility")
                        settingKey: "cursorVisibility"
                        iconName: "visibility_off"

                    SettingsToggleRow {
                        tab: "theme"
                        tags: ["cursor", "hide", "typing"]
                        settingKey: "cursorHideWhenTyping"
                        text: I18n.tr("Hide When Typing")
                        description: I18n.tr("Hide cursor when pressing keyboard keys")
                        checked: {
                            return SettingsData.cursorSettings.hyprland?.hideOnKeyPress || false;
                        }
                        onToggled: checked => {
                            const updated = JSON.parse(JSON.stringify(SettingsData.cursorSettings));
                            if (!updated.hyprland)
                                updated.hyprland = {};
                            updated.hyprland.hideOnKeyPress = checked;
                            SettingsData.set("cursorSettings", updated);
                        }
                    }

                    SettingsToggleRow {
                        tab: "theme"
                        tags: ["cursor", "hide", "touch"]
                        settingKey: "cursorHideOnTouch"
                        text: I18n.tr("Hide on Touch")
                        description: I18n.tr("Hide cursor when using touch input")
                        checked: SettingsData.cursorSettings.hyprland?.hideOnTouch || false
                        onToggled: checked => {
                            const updated = JSON.parse(JSON.stringify(SettingsData.cursorSettings));
                            if (!updated.hyprland)
                                updated.hyprland = {};
                            updated.hyprland.hideOnTouch = checked;
                            SettingsData.set("cursorSettings", updated);
                        }
                    }

                    SettingsSliderRow {
                        tab: "theme"
                        tags: ["cursor", "hide", "timeout", "inactive"]
                        settingKey: "cursorHideAfterInactive"
                        text: I18n.tr("Auto-Hide Timeout")
                        description: I18n.tr("Hide cursor after inactivity (0 = disabled)")
                        value: {
                            return SettingsData.cursorSettings.hyprland?.inactiveTimeout || 0;
                        }
                        minimum: 0
                        maximum: 10
                        unit: "s"
                        defaultValue: 0
                        onSliderValueChanged: newValue => {
                            const updated = JSON.parse(JSON.stringify(SettingsData.cursorSettings));
                            if (!updated.hyprland)
                                updated.hyprland = {};
                            updated.hyprland.inactiveTimeout = newValue;
                            SettingsData.set("cursorSettings", updated);
                        }
                    }
                    }
                }
            }
        }
    }

    FileBrowserModal {
        id: fileBrowserModal
        browserTitle: I18n.tr("Select Custom Theme", "custom theme file browser title")
        filterExtensions: ["*.json"]
        showHiddenFiles: true

        function selectCustomTheme() {
            shouldBeVisible = true;
        }

        onFileSelected: function (filePath) {
            if (filePath.endsWith(".json")) {
                SettingsData.set("customThemeFile", filePath);
                Theme.switchTheme("custom");
                close();
            }
        }
    }
    LazyLoader {
        id: themeBrowserLoader
        active: false

        ThemeBrowser {
            id: themeBrowserItem
            parentModal: appearanceTab.parentModal
        }
    }
    function showThemeBrowser() {
        themeBrowserLoader.active = true;
        if (themeBrowserLoader.item)
            themeBrowserLoader.item.show();
    }
}
