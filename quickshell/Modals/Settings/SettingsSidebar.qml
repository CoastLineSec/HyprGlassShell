pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import qs.Common
import qs.Modals.Settings
import qs.Services
import qs.Widgets

Rectangle {
    id: root

    LayoutMirroring.enabled: I18n.isRtl
    LayoutMirroring.childrenInherit: true

    property int currentIndex: 0
    property var parentModal: null

    signal tabChangeRequested(int tabIndex)
    property string _expandedIds: ","
    property string _collapsedIds: ","
    property string _autoExpandedIds: ","
    property bool searchActive: searchField.text.length > 0
    property int searchSelectedIndex: 0
    property int keyboardHighlightIndex: -1
    readonly property int navigationStateDuration: Theme.currentAnimationSpeed === SettingsData.AnimationSpeed.None ? 0 : Anims.settingsNavigationStateDuration

    function focusSearch() {
        searchField.forceActiveFocus();
    }

    function clearSearchFocus() {
        // Drop the field's focus directly (and park focus on the sink) so it returns
        // to the inactive "Search..." placeholder state.
        searchField.setFocus(false);
        searchFocusSink.forceActiveFocus();
    }

    // Monitor geometry for fluid-glass wallpaper sampling on the glass boxes.
    readonly property string _glassScreenName: parentModal && parentModal.screen ? parentModal.screen.name : ""
    readonly property real _glassScreenW: parentModal && parentModal.screen ? parentModal.screen.width : 0
    readonly property real _glassScreenH: parentModal && parentModal.screen ? parentModal.screen.height : 0

    function highlightNext() {
        var flatItems = getFlatNavigableItems();
        if (flatItems.length === 0)
            return;
        var currentPos = flatItems.findIndex(item => item.tabIndex === keyboardHighlightIndex);
        if (currentPos === -1) {
            currentPos = flatItems.findIndex(item => item.tabIndex === currentIndex);
        }
        var nextPos = (currentPos + 1) % flatItems.length;
        keyboardHighlightIndex = flatItems[nextPos].tabIndex;
        autoExpandForTab(keyboardHighlightIndex);
    }

    function highlightPrevious() {
        var flatItems = getFlatNavigableItems();
        if (flatItems.length === 0)
            return;
        var currentPos = flatItems.findIndex(item => item.tabIndex === keyboardHighlightIndex);
        if (currentPos === -1) {
            currentPos = flatItems.findIndex(item => item.tabIndex === currentIndex);
        }
        var prevPos = (currentPos - 1 + flatItems.length) % flatItems.length;
        keyboardHighlightIndex = flatItems[prevPos].tabIndex;
        autoExpandForTab(keyboardHighlightIndex);
    }

    function selectHighlighted() {
        if (keyboardHighlightIndex < 0)
            return;
        var oldIndex = currentIndex;
        var newIndex = keyboardHighlightIndex;
        tabChangeRequested(newIndex);
        autoCollapseIfNeeded(oldIndex, newIndex);
        keyboardHighlightIndex = -1;
        Qt.callLater(searchField.forceActiveFocus);
    }

    readonly property var categoryStructure: [
        {
            "id": "wifi",
            "text": I18n.tr("WiFi"),
            "icon": "wifi",
            "hgsOnly": true,
            "tabIndex": 40
        },
        {
            "id": "bluetooth",
            "text": I18n.tr("Bluetooth"),
            "icon": "bluetooth",
            "tabIndex": 42
        },
        {
            "id": "network",
            "text": I18n.tr("Network"),
            "icon": "public",
            "hgsOnly": true,
            "tabIndex": 43
        },
        {
            "id": "battery",
            "text": I18n.tr("Battery"),
            "icon": "battery_full",
            "batteryOnly": true,
            "tabIndex": 44
        },
        {
            "id": "general_separator",
            "separator": true
        },
        {
            "id": "general",
            "text": I18n.tr("General"),
            "icon": "settings",
            "tabIndex": 45
        },
        {
            "id": "accessibility",
            "text": I18n.tr("Accessibility"),
            "icon": "accessibility_new",
            "tabIndex": 46
        },
        {
            "id": "appearance",
            "text": I18n.tr("Appearance"),
            "icon": "brightness_6",
            "tabIndex": 47
        },
        {
            "id": "hyprglass",
            "text": I18n.tr("HyprGlass"),
            "icon": "blur_on",
            "tabIndex": 48
        },
        {
            "id": "personalization",
            "text": I18n.tr("Personalization"),
            "icon": "palette",
            "children": [
                {
                    "id": "wallpaper",
                    "text": I18n.tr("Wallpaper"),
                    "icon": "wallpaper",
                    "tabIndex": 0
                },
                {
                    "id": "theme",
                    "text": I18n.tr("Theme & Colors"),
                    "icon": "format_paint",
                    "tabIndex": 10
                },
                {
                    "id": "typography",
                    "text": I18n.tr("Typography & Motion"),
                    "icon": "text_fields",
                    "tabIndex": 14
                },
                {
                    "id": "time_weather",
                    "text": I18n.tr("Time & Weather"),
                    "icon": "schedule",
                    "tabIndex": 1
                },
                {
                    "id": "sounds",
                    "text": I18n.tr("Sounds"),
                    "icon": "volume_up",
                    "tabIndex": 15,
                    "soundsOnly": true
                },
                {
                    "id": "compositor_layout",
                    "text": "Hyprland",
                    "icon": "layers",
                    "tabIndex": 37
                }
            ]
        },
        {
            "id": "hgsbar",
            "text": I18n.tr("HGS Bar"),
            "icon": "toolbar",
            "children": [
                {
                    "id": "hgsbar_appearance",
                    "text": I18n.tr("Appearance"),
                    "icon": "palette",
                    "tabIndex": 6
                },
                {
                    "id": "hgsbar_settings",
                    "text": I18n.tr("Settings"),
                    "icon": "tune",
                    "tabIndex": 3
                },
                {
                    "id": "hgsbar_widgets",
                    "text": I18n.tr("Widgets"),
                    "icon": "widgets",
                    "tabIndex": 22
                },
                {
                    "id": "workspaces",
                    "text": I18n.tr("Workspaces"),
                    "icon": "view_module",
                    "tabIndex": 4
                },
                {
                    "id": "frame",
                    "text": I18n.tr("Frame"),
                    "icon": "frame_source",
                    "tabIndex": 33
                }
            ]
        },
        {
            "id": "workspaces_widgets",
            "text": I18n.tr("Widgets & Notifications"),
            "icon": "dashboard",
            "collapsedByDefault": true,
            "children": [
                {
                    "id": "media_player",
                    "text": I18n.tr("Media Player"),
                    "icon": "music_note",
                    "tabIndex": 16
                },
                {
                    "id": "notifications",
                    "text": I18n.tr("Notifications"),
                    "icon": "notifications",
                    "tabIndex": 17
                },
                {
                    "id": "osd",
                    "text": I18n.tr("On-screen Displays"),
                    "icon": "tune",
                    "tabIndex": 18
                },
                {
                    "id": "desktop_widgets",
                    "text": I18n.tr("Desktop Widgets"),
                    "icon": "widgets",
                    "tabIndex": 27
                }
            ]
        },
        {
            "id": "dock_launcher",
            "text": I18n.tr("Dock & Launcher"),
            "icon": "shelf_auto_hide",
            "collapsedByDefault": true,
            "children": [
                {
                    "id": "dock",
                    "text": I18n.tr("Dock"),
                    "icon": "dock_to_bottom",
                    "tabIndex": 5
                },
                {
                    "id": "launcher",
                    "text": I18n.tr("Launcher"),
                    "icon": "grid_view",
                    "tabIndex": 9
                }
            ]
        },
        {
            "id": "keybinds",
            "text": I18n.tr("Keyboard Shortcuts"),
            "icon": "keyboard",
            "tabIndex": 2,
            "shortcutsOnly": true
        },
        {
            "id": "displays",
            "text": I18n.tr("Displays"),
            "icon": "monitor",
            "collapsedByDefault": true,
            "children": [
                {
                    "id": "display_config",
                    "text": I18n.tr("Configuration"),
                    "icon": "display_settings",
                    "tabIndex": 24
                },
                {
                    "id": "display_gamma",
                    "text": I18n.tr("Gamma Control"),
                    "icon": "brightness_6",
                    "tabIndex": 25
                },
                {
                    "id": "display_widgets",
                    "text": I18n.tr("Widgets", "settings_displays"),
                    "icon": "widgets",
                    "tabIndex": 26
                }
            ]
        },
        // (Network is now a top-level selectable field — WiFi / Bluetooth / Network —
        //  with Status/Ethernet/VPN living inside the Network page.)
        {
            "id": "applications",
            "text": I18n.tr("Applications"),
            "icon": "apps",
            "collapsedByDefault": true,
            "children": [
                {
                    "id": "default_apps",
                    "text": I18n.tr("Default Apps"),
                    "icon": "star",
                    "tabIndex": 34
                },
                {
                    "id": "running_apps",
                    "text": I18n.tr("Running Apps"),
                    "icon": "app_registration",
                    "tabIndex": 19
                },
                {
                    "id": "autostart",
                    "text": I18n.tr("Autostart Apps"),
                    "icon": "line_start",
                    "tabIndex": 36,
                    "autostartOnly": true
                },
                {
                    "id": "window_rules",
                    "text": I18n.tr("Window Rules"),
                    "icon": "select_window",
                    "tabIndex": 38
                }
            ]
        },
        {
            "id": "system",
            "text": I18n.tr("System"),
            "icon": "memory",
            "collapsedByDefault": true,
            "children": [
                {
                    "id": "audio",
                    "text": I18n.tr("Audio"),
                    "icon": "headphones",
                    "tabIndex": 29
                },
                {
                    "id": "clipboard",
                    "text": I18n.tr("Clipboard"),
                    "icon": "content_paste",
                    "tabIndex": 23,
                    "clipboardOnly": true
                },
                {
                    "id": "printers",
                    "text": I18n.tr("Printers"),
                    "icon": "print",
                    "tabIndex": 8,
                    "cupsOnly": true
                },
                {
                    "id": "multiplexers",
                    "text": I18n.tr("Multiplexers"),
                    "icon": "terminal",
                    "tabIndex": 32
                },
                {
                    "id": "users",
                    "text": I18n.tr("Users"),
                    "icon": "manage_accounts",
                    "tabIndex": 35
                }
            ]
        },
        {
            "id": "power_security",
            "text": I18n.tr("Power & Security"),
            "icon": "security",
            "collapsedByDefault": true,
            "children": [
                {
                    "id": "lock_screen",
                    "text": I18n.tr("Lock Screen"),
                    "icon": "lock",
                    "tabIndex": 11
                },
                {
                    "id": "power_sleep",
                    "text": I18n.tr("Power & Sleep"),
                    "icon": "power_settings_new",
                    "tabIndex": 21
                }
            ]
        }
    ]

    function isItemVisible(item) {
        if (item.hgsOnly && NetworkService.usingLegacy)
            return false;
        if (item.cupsOnly && !CupsService.cupsAvailable)
            return false;
        if (item.shortcutsOnly && !KeybindsService.available)
            return false;
        if (item.soundsOnly && !AudioService.soundsAvailable)
            return false;
        if (item.clipboardOnly && (!HGSService.isConnected || HGSService.apiVersion < 23))
            return false;
        if (item.updaterOnly && !SystemUpdateService.sysupdateAvailable)
            return false;
        if (item.autostartOnly && !DesktopService.autostartAvailable)
            return false;
        if (item.batteryOnly && !BatteryService.batteryAvailable)
            return false;
        return true;
    }

    function hasVisibleChildren(category) {
        if (!category.children)
            return false;
        return category.children.some(child => isItemVisible(child));
    }

    function isCategoryVisible(category) {
        if (category.separator)
            return true;
        if (!isItemVisible(category))
            return false;
        if (category.children && !hasVisibleChildren(category))
            return false;
        return true;
    }

    function _setExpanded(id, expanded) {
        var marker = "," + id + ",";
        if (expanded) {
            if (_expandedIds.indexOf(marker) < 0)
                _expandedIds = _expandedIds + id + ",";
            _collapsedIds = _collapsedIds.replace(marker, ",");
        } else {
            _expandedIds = _expandedIds.replace(marker, ",");
            if (_collapsedIds.indexOf(marker) < 0)
                _collapsedIds = _collapsedIds + id + ",";
        }
        SessionData.setSettingsSidebarState(_expandedIds, _collapsedIds);
    }

    function _setAutoExpanded(id, value) {
        var marker = "," + id + ",";
        if (value) {
            if (_autoExpandedIds.indexOf(marker) < 0)
                _autoExpandedIds = _autoExpandedIds + id + ",";
        } else {
            _autoExpandedIds = _autoExpandedIds.replace(marker, ",");
        }
    }

    function _isAutoExpanded(id) {
        return _autoExpandedIds.indexOf("," + id + ",") >= 0;
    }

    function toggleCategory(categoryId) {
        _setExpanded(categoryId, !isCategoryExpanded(categoryId));
        _setAutoExpanded(categoryId, false);
    }

    function isCategoryExpanded(categoryId) {
        if (_collapsedIds.indexOf("," + categoryId + ",") >= 0)
            return false;
        if (_expandedIds.indexOf("," + categoryId + ",") >= 0)
            return true;
        var category = categoryStructure.find(cat => cat.id === categoryId);
        if (category && category.collapsedByDefault)
            return false;
        return true;
    }

    function isChildActive(category) {
        if (!category.children)
            return false;
        return category.children.some(child => child.tabIndex === currentIndex);
    }

    function findParentCategory(tabIndex) {
        for (var i = 0; i < categoryStructure.length; i++) {
            var cat = categoryStructure[i];
            if (cat.children) {
                for (var j = 0; j < cat.children.length; j++) {
                    if (cat.children[j].tabIndex === tabIndex) {
                        return cat;
                    }
                }
            }
        }
        return null;
    }

    function autoExpandForTab(tabIndex) {
        var parent = findParentCategory(tabIndex);
        if (!parent)
            return;

        if (!isCategoryExpanded(parent.id)) {
            _setExpanded(parent.id, true);
            _setAutoExpanded(parent.id, true);
        }
    }

    function autoCollapseIfNeeded(oldTabIndex, newTabIndex) {
        var oldParent = findParentCategory(oldTabIndex);
        var newParent = findParentCategory(newTabIndex);

        if (oldParent && oldParent !== newParent && _isAutoExpanded(oldParent.id)) {
            _setExpanded(oldParent.id, false);
            _setAutoExpanded(oldParent.id, false);
        }
    }

    function navigateNext() {
        var flatItems = getFlatNavigableItems();
        var currentPos = flatItems.findIndex(item => item.tabIndex === currentIndex);
        var oldIndex = currentIndex;
        var newIndex;
        if (currentPos === -1) {
            newIndex = flatItems[0]?.tabIndex ?? 0;
        } else {
            var nextPos = (currentPos + 1) % flatItems.length;
            newIndex = flatItems[nextPos].tabIndex;
        }
        tabChangeRequested(newIndex);
        autoCollapseIfNeeded(oldIndex, newIndex);
        autoExpandForTab(newIndex);
    }

    function navigatePrevious() {
        var flatItems = getFlatNavigableItems();
        var currentPos = flatItems.findIndex(item => item.tabIndex === currentIndex);
        var oldIndex = currentIndex;
        var newIndex;
        if (currentPos === -1) {
            newIndex = flatItems[0]?.tabIndex ?? 0;
        } else {
            var prevPos = (currentPos - 1 + flatItems.length) % flatItems.length;
            newIndex = flatItems[prevPos].tabIndex;
        }
        tabChangeRequested(newIndex);
        autoCollapseIfNeeded(oldIndex, newIndex);
        autoExpandForTab(newIndex);
    }

    function getFlatNavigableItems() {
        var items = [];
        for (var i = 0; i < categoryStructure.length; i++) {
            var cat = categoryStructure[i];
            if (cat.separator || !isCategoryVisible(cat))
                continue;

            if (cat.tabIndex !== undefined && !cat.children) {
                items.push(cat);
            }

            if (cat.children) {
                for (var j = 0; j < cat.children.length; j++) {
                    var child = cat.children[j];
                    if (isItemVisible(child)) {
                        items.push(child);
                    }
                }
            }
        }
        return items;
    }

    function resolveTabIndex(name: string): int {
        if (!name)
            return -1;

        var normalized = name.toLowerCase().replace(/[_\-\s]/g, "");
        if (normalized === "compositor")
            normalized = "workspaces";

        for (var i = 0; i < categoryStructure.length; i++) {
            var cat = categoryStructure[i];
            if (cat.separator)
                continue;

            var catId = (cat.id || "").toLowerCase().replace(/[_\-\s]/g, "");
            if (catId === normalized) {
                if (cat.tabIndex !== undefined)
                    return cat.tabIndex;
                if (cat.children && cat.children.length > 0)
                    return cat.children[0].tabIndex;
            }

            if (cat.children) {
                for (var j = 0; j < cat.children.length; j++) {
                    var child = cat.children[j];
                    var childId = (child.id || "").toLowerCase().replace(/[_\-\s]/g, "");
                    if (childId === normalized)
                        return child.tabIndex;
                }
            }
        }
        return -1;
    }

    property real __maxTextWidth: Math.max(__m1.advanceWidth, __m2.advanceWidth, __m3.advanceWidth, __m4.advanceWidth, __m5.advanceWidth, __m6.advanceWidth)
    // +7px over the text-fit width to host the scrollbar lane (5px left + 2px right).
    property real __calculatedWidth: Math.max(270, __maxTextWidth + Theme.iconSize * 2 + Theme.spacingM * 4 + Theme.spacingS * 2) + 7

    implicitWidth: __calculatedWidth
    width: __calculatedWidth
    height: parent.height
    color: Theme.surfaceContainer
    radius: Theme.cornerRadius

    Component.onCompleted: {
        root._expandedIds = SessionData.settingsSidebarExpandedIds;
        root._collapsedIds = SessionData.settingsSidebarCollapsedIds;
    }

    StyledTextMetrics {
        id: __m1
        font.pixelSize: Theme.fontSizeMedium
        font.weight: Font.Medium
        text: I18n.tr("Widgets & Notifications")
    }
    StyledTextMetrics {
        id: __m2
        font.pixelSize: Theme.fontSizeMedium
        font.weight: Font.Medium
        text: I18n.tr("Typography & Motion")
    }
    StyledTextMetrics {
        id: __m3
        font.pixelSize: Theme.fontSizeMedium
        font.weight: Font.Medium
        text: I18n.tr("Keyboard Shortcuts")
    }
    StyledTextMetrics {
        id: __m4
        font.pixelSize: Theme.fontSizeMedium
        font.weight: Font.Medium
        text: I18n.tr("Power & Security")
    }
    StyledTextMetrics {
        id: __m5
        font.pixelSize: Theme.fontSizeMedium
        font.weight: Font.Medium
        text: I18n.tr("Dock & Launcher")
    }
    StyledTextMetrics {
        id: __m6
        font.pixelSize: Theme.fontSizeMedium
        font.weight: Font.Medium
        text: I18n.tr("Personalization")
    }

    function selectSearchResult(result) {
        if (!result)
            return;
        if (result.section) {
            SettingsSearchService.navigateToSection(result.section);
        }
        var oldIndex = root.currentIndex;
        tabChangeRequested(result.tabIndex);
        autoCollapseIfNeeded(oldIndex, result.tabIndex);
        autoExpandForTab(result.tabIndex);
        searchField.text = "";
        SettingsSearchService.clear();
        searchSelectedIndex = 0;
        keyboardHighlightIndex = -1;
        Qt.callLater(searchField.forceActiveFocus);
    }

    function navigateSearchResults(delta) {
        if (SettingsSearchService.results.length === 0)
            return;
        searchSelectedIndex = Math.max(0, Math.min(searchSelectedIndex + delta, SettingsSearchService.results.length - 1));
    }

    // Focus sink: clicking anywhere outside the search field hands focus here,
    // deactivating the search (back to the "Search..." placeholder).
    Item {
        id: searchFocusSink
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        acceptedButtons: Qt.AllButtons
        onPressed: mouse => {
            root.clearSearchFocus();
            mouse.accepted = false;
        }
    }

    Column {
        id: layoutColumn
        anchors.fill: parent
        anchors.margins: Theme.spacingS
        spacing: Theme.spacingS

        // Static box: search field + user info, with fluid glass behind them.
        Rectangle {
            id: staticBox
            width: parent.width
            radius: Theme.cornerRadius
            color: "transparent"
            border.width: 1
            border.color: Theme.withAlpha(Theme.outline, 0.18)
            clip: true
            height: staticInner.height + Theme.spacingS * 2

            FluidGlass {
                anchors.fill: parent
                cornerRadius: staticBox.radius
                backdropColor: Theme.surfaceContainer
                level: FluidGlassStyle.level
                tintHue: FluidGlassStyle.tintHue
                noTint: FluidGlassStyle.noTint
            }

            Column {
                id: staticInner
                x: Theme.spacingS
                y: Theme.spacingS
                width: parent.width - Theme.spacingS * 2
                spacing: Theme.spacingS

                // Pinned search field — stays above the ribbon and never scrolls.
                HGSTextField {
                    id: searchField
                    width: parent.width
            placeholderText: I18n.tr("Search...")
            normalBorderColor: Theme.outlineMedium
            focusedBorderColor: Theme.primary
            leftIconName: "search"
            leftIconSize: Theme.iconSize - 4
            showClearButton: text.length > 0
            usePopupTransparency: false
            onTextChanged: {
                SettingsSearchService.search(text);
                root.searchSelectedIndex = 0;
            }
            keyForwardTargets: [keyHandler]

            Item {
                id: keyHandler
                function navNext() {
                    if (root.searchActive) {
                        root.navigateSearchResults(1);
                    } else {
                        root.highlightNext();
                    }
                }
                function navPrev() {
                    if (root.searchActive) {
                        root.navigateSearchResults(-1);
                    } else {
                        root.highlightPrevious();
                    }
                }
                function navSelect() {
                    if (root.searchActive && SettingsSearchService.results.length > 0) {
                        root.selectSearchResult(SettingsSearchService.results[root.searchSelectedIndex]);
                    } else if (root.keyboardHighlightIndex >= 0) {
                        root.selectHighlighted();
                    }
                }
                Keys.onDownPressed: event => {
                    navNext();
                    event.accepted = true;
                }
                Keys.onUpPressed: event => {
                    navPrev();
                    event.accepted = true;
                }
                Keys.onTabPressed: event => {
                    navNext();
                    event.accepted = true;
                }
                Keys.onBacktabPressed: event => {
                    navPrev();
                    event.accepted = true;
                }
                Keys.onReturnPressed: event => {
                    navSelect();
                    event.accepted = true;
                }
                Keys.onEscapePressed: event => {
                    if (root.searchActive) {
                        searchField.text = "";
                        SettingsSearchService.clear();
                    } else {
                        root.keyboardHighlightIndex = -1;
                    }
                    event.accepted = true;
                }
            }
        }

                // User info (photo / username / system name) — static, below the search.
                ProfileSection {
                    id: profileSection
                    width: parent.width
                    parentModal: root.parentModal
                }
            }
        }

        // Clearly-bounded, rounded scroll area with a persistent scrollbar (glass behind).
        Rectangle {
            id: scrollBox
            width: parent.width
            height: parent.height - staticBox.height - layoutColumn.spacing
            radius: Theme.cornerRadius
            color: "transparent"
            border.width: 1
            border.color: Theme.withAlpha(Theme.outline, 0.18)
            clip: true

            FluidGlass {
                anchors.fill: parent
                cornerRadius: scrollBox.radius
                backdropColor: Theme.surfaceContainer
                level: FluidGlassStyle.level
                tintHue: FluidGlassStyle.tintHue
                noTint: FluidGlassStyle.noTint
            }

            HGSFlickable {
                anchors.fill: parent
                anchors.topMargin: Theme.spacingS
                anchors.bottomMargin: Theme.spacingS
                anchors.leftMargin: 2
                // 5px gap between the scrollbar and the glass box's right edge.
                anchors.rightMargin: 5
                clip: true
                contentHeight: sidebarColumn.height
                persistentScrollBar: true

                Column {
                    id: sidebarColumn
            width: parent.width
            leftPadding: Theme.spacingS
            // Extra right padding reserves the scrollbar lane: ~5px clear space between
            // the selected-item highlight and the scrollbar.
            rightPadding: Theme.spacingS + 11
            bottomPadding: Theme.spacingL
            topPadding: Theme.spacingM + 2
            spacing: 2

            // Profile (user info) + search are now pinned above this scroll area.

            Column {
                id: searchResultsColumn
                width: parent.width - parent.leftPadding - parent.rightPadding
                spacing: 2
                visible: root.searchActive

                Item {
                    width: parent.width
                    height: Theme.spacingS
                }

                Repeater {
                    model: ScriptModel {
                        values: SettingsSearchService.results
                    }

                    Rectangle {
                        id: resultDelegate
                        required property int index
                        required property var modelData

                        width: searchResultsColumn.width
                        height: resultContent.height + Theme.spacingM * 2
                        radius: Theme.cornerRadius
                        color: {
                            if (root.searchSelectedIndex === index)
                                return Theme.buttonBg;
                            if (resultMouseArea.containsMouse)
                                return Theme.surfaceHover;
                            return "transparent";
                        }

                        HGSRipple {
                            id: resultRipple
                            rippleColor: root.searchSelectedIndex === resultDelegate.index ? Theme.buttonText : Theme.surfaceText
                            cornerRadius: resultDelegate.radius
                            animationDuration: Anims.settingsNavigationRippleDuration
                        }

                        Row {
                            id: resultContent
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.spacingM
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.spacingM
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.spacingM

                            HGSIcon {
                                name: resultDelegate.modelData.icon || "settings"
                                size: Theme.iconSize - 2
                                color: root.searchSelectedIndex === resultDelegate.index ? Theme.buttonText : Theme.surfaceText
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Column {
                                width: parent.width - Theme.iconSize - Theme.spacingM
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2

                                StyledText {
                                    text: resultDelegate.modelData.label
                                    font.pixelSize: Theme.fontSizeMedium
                                    font.weight: Font.Medium
                                    color: root.searchSelectedIndex === resultDelegate.index ? Theme.buttonText : Theme.surfaceText
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignLeft
                                }

                                StyledText {
                                    text: resultDelegate.modelData.category
                                    font.pixelSize: Theme.fontSizeSmall - 1
                                    color: root.searchSelectedIndex === resultDelegate.index ? Theme.withAlpha(Theme.buttonText, 0.7) : Theme.surfaceVariantText
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignLeft
                                }
                            }
                        }

                        MouseArea {
                            id: resultMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onPressed: mouse => resultRipple.trigger(mouse.x, mouse.y)
                            onClicked: root.selectSearchResult(resultDelegate.modelData)
                        }

                        Behavior on color {
                            ColorAnimation {
                                duration: root.navigationStateDuration
                                easing.type: Easing.BezierSpline
                                easing.bezierCurve: Anims.expressiveEffects
                            }
                        }
                    }
                }

                StyledText {
                    width: parent.width
                    text: I18n.tr("No matches")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.surfaceVariantText
                    horizontalAlignment: Text.AlignHCenter
                    visible: searchField.text.length > 0 && SettingsSearchService.results.length === 0
                    topPadding: Theme.spacingM
                }
            }

            Item {
                width: parent.width - parent.leftPadding - parent.rightPadding
                height: Theme.spacingXS
                visible: !root.searchActive
            }

            Repeater {
                model: root.categoryStructure

                delegate: Column {
                    id: categoryDelegate
                    required property int index
                    required property var modelData

                    width: parent.width - parent.leftPadding - parent.rightPadding
                    visible: !root.searchActive && root.isCategoryVisible(modelData)
                    spacing: 2

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.outline
                        opacity: 0.15
                        visible: categoryDelegate.modelData.separator === true
                    }

                    Item {
                        width: parent.width
                        height: Theme.spacingS
                        visible: categoryDelegate.modelData.separator === true
                    }

                    Rectangle {
                        id: categoryRow
                        width: parent.width
                        height: Math.max(Theme.iconSize, Theme.fontSizeMedium) + Theme.spacingS * 2
                        radius: Theme.cornerRadius
                        visible: categoryDelegate.modelData.separator !== true

                        readonly property bool hasTab: categoryDelegate.modelData.tabIndex !== undefined && !categoryDelegate.modelData.children
                        readonly property bool isActive: hasTab && root.currentIndex === categoryDelegate.modelData.tabIndex
                        readonly property bool isHighlighted: hasTab && root.keyboardHighlightIndex === categoryDelegate.modelData.tabIndex

                        color: {
                            if (isActive)
                                return Theme.buttonBg;
                            if (isHighlighted)
                                return Theme.buttonHover;
                            if (categoryMouseArea.containsMouse)
                                return Theme.surfaceHover;
                            return "transparent";
                        }

                        HGSRipple {
                            id: categoryRipple
                            rippleColor: categoryRow.isActive ? Theme.buttonText : Theme.surfaceText
                            cornerRadius: categoryRow.radius
                            animationDuration: Anims.settingsNavigationRippleDuration
                        }

                        Row {
                            id: categoryRowContent
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.spacingM
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.spacingM

                            HGSIcon {
                                name: categoryDelegate.modelData.icon || ""
                                size: Theme.iconSize - 2
                                color: categoryRow.isActive ? Theme.buttonText : Theme.surfaceText
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            StyledText {
                                text: categoryDelegate.modelData.text || ""
                                font.pixelSize: Theme.fontSizeMedium
                                font.weight: (categoryRow.isActive || root.isChildActive(categoryDelegate.modelData)) ? Font.Medium : Font.Normal
                                color: categoryRow.isActive ? Theme.buttonText : Theme.surfaceText
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        HGSIcon {
                            id: expandIcon
                            name: root.isCategoryExpanded(categoryDelegate.modelData.id) ? "expand_less" : "expand_more"
                            size: Theme.iconSize - 4
                            color: Theme.surfaceVariantText
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.spacingM
                            anchors.verticalCenter: parent.verticalCenter
                            visible: categoryDelegate.modelData.children !== undefined && categoryDelegate.modelData.children.length > 0
                        }

                        MouseArea {
                            id: categoryMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onPressed: mouse => categoryRipple.trigger(mouse.x, mouse.y)
                            onClicked: {
                                root.keyboardHighlightIndex = -1;
                                if (categoryDelegate.modelData.children) {
                                    root.toggleCategory(categoryDelegate.modelData.id);
                                } else if (categoryDelegate.modelData.tabIndex !== undefined) {
                                    root.tabChangeRequested(categoryDelegate.modelData.tabIndex);
                                }
                                root.clearSearchFocus();
                            }
                        }

                        Behavior on color {
                            ColorAnimation {
                                duration: root.navigationStateDuration
                                easing.type: Easing.BezierSpline
                                easing.bezierCurve: Anims.expressiveEffects
                            }
                        }
                    }

                    Column {
                        id: childrenColumn
                        width: parent.width
                        spacing: 2
                        visible: categoryDelegate.modelData.children !== undefined && root.isCategoryExpanded(categoryDelegate.modelData.id)
                        clip: true

                        Repeater {
                            model: categoryDelegate.modelData.children || []

                            delegate: Rectangle {
                                id: childDelegate
                                required property int index
                                required property var modelData

                                readonly property bool isActive: root.currentIndex === modelData.tabIndex
                                readonly property bool isHighlighted: root.keyboardHighlightIndex === modelData.tabIndex

                                width: childrenColumn.width
                                height: Math.max(Theme.iconSize - 4, Theme.fontSizeSmall + 1) + Theme.spacingS * 2
                                radius: Theme.cornerRadius
                                visible: root.isItemVisible(modelData)
                                color: {
                                    if (isActive)
                                        return Theme.buttonBg;
                                    if (isHighlighted)
                                        return Theme.buttonHover;
                                    if (childMouseArea.containsMouse)
                                        return Theme.surfaceHover;
                                    return "transparent";
                                }

                                HGSRipple {
                                    id: childRipple
                                    rippleColor: childDelegate.isActive ? Theme.buttonText : Theme.surfaceText
                                    cornerRadius: childDelegate.radius
                                    animationDuration: Anims.settingsNavigationRippleDuration
                                }

                                Row {
                                    id: childRowContent
                                    anchors.left: parent.left
                                    anchors.leftMargin: Theme.spacingL + Theme.spacingM
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: Theme.spacingM

                                    HGSIcon {
                                        name: childDelegate.modelData.icon || ""
                                        size: Theme.iconSize - 4
                                        color: childDelegate.isActive ? Theme.buttonText : Theme.surfaceVariantText
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    StyledText {
                                        text: childDelegate.modelData.text || ""
                                        font.pixelSize: Theme.fontSizeSmall + 1
                                        font.weight: childDelegate.isActive ? Font.Medium : Font.Normal
                                        color: childDelegate.isActive ? Theme.buttonText : Theme.surfaceText
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                MouseArea {
                                    id: childMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onPressed: mouse => childRipple.trigger(mouse.x, mouse.y)
                                    onClicked: {
                                        root.keyboardHighlightIndex = -1;
                                        root.tabChangeRequested(childDelegate.modelData.tabIndex);
                                        root.clearSearchFocus();
                                    }
                                }

                                Behavior on color {
                                    ColorAnimation {
                                        duration: root.navigationStateDuration
                                        easing.type: Easing.BezierSpline
                                        easing.bezierCurve: Anims.expressiveEffects
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
        }
    }
}
