pragma Singleton
pragma ComponentBehavior: Bound
import QtQuick
import Quickshell
import Quickshell.Wayland
import qs.Common

Singleton {
    id: root

    property var controlCenterPopout: null
    property var controlCenterLoader: null
    property var notificationCenterPopout: null
    property var notificationCenterLoader: null
    property var appDrawerPopout: null
    property var appDrawerLoader: null
    property var processListPopout: null
    property var processListPopoutLoader: null
    property var hgsDashPopout: null
    property var hgsDashPopoutLoader: null
    property var batteryPopout: null
    property var batteryPopoutLoader: null
    property var vpnPopout: null
    property var vpnPopoutLoader: null
    property var systemUpdatePopout: null
    property var systemUpdateLoader: null
    property var layoutPopout: null
    property var layoutPopoutLoader: null
    property var clipboardHistoryPopout: null
    property var clipboardHistoryPopoutLoader: null

    property var settingsModal: null
    property var settingsModalLoader: null
    property var clipboardHistoryModal: null
    property var hgsLauncherV2Modal: null
    property var hgsLauncherV2ModalLoader: null
    property var spotlightBarModal: null
    property var spotlightBarModalLoader: null
    property var powerMenuModal: null
    property var processListModal: null
    property var processListModalLoader: null
    property var colorPickerModal: null
    property var notificationModal: null
    property var wifiPasswordModal: null
    property var wifiPasswordModalLoader: null
    property var wifiQRCodeModal: null
    property var wifiQRCodeModalLoader: null
    property var polkitAuthModal: null
    property var polkitAuthModalLoader: null
    property var bluetoothPairingModal: null
    property var networkInfoModal: null
    property var windowRuleModalLoader: null
    property var powerProfileModal: null
    property var powerProfileModalLoader: null

    property var notepadSlideouts: []

    property string pendingThemeInstall: ""
    property string pendingPluginInstall: ""

    function setPosition(popout, x, y, width, section, screen) {
        if (popout && popout.setTriggerPosition && arguments.length >= 6) {
            popout.setTriggerPosition(x, y, width, section, screen);
        }
    }

    function openControlCenter(x, y, width, section, screen) {
        if (controlCenterPopout) {
            setPosition(controlCenterPopout, x, y, width, section, screen);
            controlCenterPopout.open();
        }
    }

    function closeControlCenter() {
        controlCenterPopout?.close();
    }

    function unloadControlCenter() {
        if (!controlCenterLoader)
            return;
        controlCenterPopout = null;
        controlCenterLoader.active = false;
    }

    function toggleControlCenter(x, y, width, section, screen) {
        if (controlCenterPopout) {
            setPosition(controlCenterPopout, x, y, width, section, screen);
            controlCenterPopout.toggle();
        }
    }

    function openNotificationCenter(x, y, width, section, screen) {
        if (notificationCenterPopout) {
            setPosition(notificationCenterPopout, x, y, width, section, screen);
            notificationCenterPopout.open();
        }
    }

    function closeNotificationCenter() {
        notificationCenterPopout?.close();
    }

    function unloadNotificationCenter() {
        if (!notificationCenterLoader)
            return;
        notificationCenterPopout = null;
        notificationCenterLoader.active = false;
    }

    function toggleNotificationCenter(x, y, width, section, screen) {
        if (notificationCenterPopout) {
            setPosition(notificationCenterPopout, x, y, width, section, screen);
            notificationCenterPopout.toggle();
        }
    }

    function openAppDrawer(x, y, width, section, screen) {
        if (appDrawerPopout) {
            setPosition(appDrawerPopout, x, y, width, section, screen);
            appDrawerPopout.open();
        }
    }

    function closeAppDrawer() {
        appDrawerPopout?.close();
    }

    function unloadAppDrawer() {
        if (!appDrawerLoader)
            return;
        appDrawerPopout = null;
        appDrawerLoader.active = false;
    }

    function toggleAppDrawer(x, y, width, section, screen) {
        if (appDrawerPopout) {
            setPosition(appDrawerPopout, x, y, width, section, screen);
            appDrawerPopout.toggle();
        }
    }

    function openProcessList(x, y, width, section, screen) {
        if (processListPopout) {
            setPosition(processListPopout, x, y, width, section, screen);
            processListPopout.open();
        }
    }

    function closeProcessList() {
        processListPopout?.close();
    }

    function unloadProcessListPopout() {
        if (!processListPopoutLoader)
            return;
        processListPopout = null;
        processListPopoutLoader.active = false;
    }

    function toggleProcessList(x, y, width, section, screen) {
        if (processListPopout) {
            setPosition(processListPopout, x, y, width, section, screen);
            processListPopout.toggle();
        }
    }

    property bool _hgsDashWantsOpen: false
    property bool _hgsDashWantsToggle: false
    property int _hgsDashPendingTab: 0
    property real _hgsDashPendingX: 0
    property real _hgsDashPendingY: 0
    property real _hgsDashPendingWidth: 0
    property string _hgsDashPendingSection: ""
    property var _hgsDashPendingScreen: null
    property bool _hgsDashHasPosition: false

    function _storeHGSDashPosition(x, y, width, section, screen, hasPos) {
        _hgsDashPendingX = x;
        _hgsDashPendingY = y;
        _hgsDashPendingWidth = width;
        _hgsDashPendingSection = section;
        _hgsDashPendingScreen = screen;
        _hgsDashHasPosition = hasPos;
    }

    function openHGSDash(tabIndex, x, y, width, section, screen) {
        _hgsDashPendingTab = tabIndex || 0;
        if (hgsDashPopout) {
            if (arguments.length >= 6)
                setPosition(hgsDashPopout, x, y, width, section, screen);
            hgsDashPopout.currentTabIndex = _hgsDashPendingTab;
            hgsDashPopout.dashVisible = true;
            return;
        }
        if (!hgsDashPopoutLoader)
            return;
        _storeHGSDashPosition(x, y, width, section, screen, arguments.length >= 6);
        _hgsDashWantsOpen = true;
        _hgsDashWantsToggle = false;
        hgsDashPopoutLoader.active = true;
    }

    function closeHGSDash() {
        if (hgsDashPopout)
            hgsDashPopout.dashVisible = false;
    }

    function unloadHGSDash() {
        // HGSDash is intentionally kept alive after first use. Destroying this
        // lazy popout during its close signal can invalidate connected overlay
        // bindings while Qt is still unwinding the signal stack.
    }

    function toggleHGSDash(tabIndex, x, y, width, section, screen) {
        _hgsDashPendingTab = tabIndex || 0;
        if (hgsDashPopout) {
            if (arguments.length >= 6)
                setPosition(hgsDashPopout, x, y, width, section, screen);
            if (hgsDashPopout.dashVisible) {
                hgsDashPopout.dashVisible = false;
            } else {
                hgsDashPopout.currentTabIndex = _hgsDashPendingTab;
                hgsDashPopout.dashVisible = true;
            }
            return;
        }
        if (!hgsDashPopoutLoader)
            return;
        _storeHGSDashPosition(x, y, width, section, screen, arguments.length >= 6);
        _hgsDashWantsToggle = true;
        _hgsDashWantsOpen = false;
        hgsDashPopoutLoader.active = true;
    }

    function _onHGSDashPopoutLoaded() {
        if (!hgsDashPopout)
            return;

        if (_hgsDashHasPosition)
            setPosition(hgsDashPopout, _hgsDashPendingX, _hgsDashPendingY, _hgsDashPendingWidth, _hgsDashPendingSection, _hgsDashPendingScreen);

        if (_hgsDashWantsOpen) {
            _hgsDashWantsOpen = false;
            hgsDashPopout.currentTabIndex = _hgsDashPendingTab;
            hgsDashPopout.dashVisible = true;
            return;
        }
        if (_hgsDashWantsToggle) {
            _hgsDashWantsToggle = false;
            if (hgsDashPopout.dashVisible) {
                hgsDashPopout.dashVisible = false;
            } else {
                hgsDashPopout.currentTabIndex = _hgsDashPendingTab;
                hgsDashPopout.dashVisible = true;
            }
        }
    }

    function openBattery(x, y, width, section, screen) {
        if (batteryPopout) {
            setPosition(batteryPopout, x, y, width, section, screen);
            batteryPopout.open();
        }
    }

    function closeBattery() {
        batteryPopout?.close();
    }

    function unloadBattery() {
        if (!batteryPopoutLoader)
            return;
        batteryPopout = null;
        batteryPopoutLoader.active = false;
    }

    function toggleBattery(x, y, width, section, screen) {
        if (batteryPopout) {
            setPosition(batteryPopout, x, y, width, section, screen);
            batteryPopout.toggle();
        }
    }

    function openVpn(x, y, width, section, screen) {
        if (vpnPopout) {
            setPosition(vpnPopout, x, y, width, section, screen);
            vpnPopout.open();
        }
    }

    function closeVpn() {
        vpnPopout?.close();
    }

    function unloadVpn() {
        if (!vpnPopoutLoader)
            return;
        vpnPopout = null;
        vpnPopoutLoader.active = false;
    }

    function toggleVpn(x, y, width, section, screen) {
        if (vpnPopout) {
            setPosition(vpnPopout, x, y, width, section, screen);
            vpnPopout.toggle();
        }
    }

    function openSystemUpdate(x, y, width, section, screen) {
        if (systemUpdatePopout) {
            if (arguments.length >= 5)
                setPosition(systemUpdatePopout, x, y, width, section, screen);
            systemUpdatePopout.open();
        }
    }

    function closeSystemUpdate() {
        systemUpdatePopout?.close();
    }

    function unloadSystemUpdate() {
        if (!systemUpdateLoader)
            return;
        systemUpdatePopout = null;
        systemUpdateLoader.active = false;
    }

    function toggleSystemUpdate(x, y, width, section, screen) {
        if (systemUpdatePopout) {
            if (arguments.length >= 5)
                setPosition(systemUpdatePopout, x, y, width, section, screen);
            systemUpdatePopout.toggle();
        }
    }

    property bool _settingsWantsOpen: false
    property bool _settingsWantsToggle: false

    property string _settingsPendingTab: ""
    property int _settingsPendingTabIndex: -1

    function openSettings() {
        if (settingsModal) {
            settingsModal.show();
        } else if (settingsModalLoader) {
            _settingsWantsOpen = true;
            _settingsWantsToggle = false;
            settingsModalLoader.activeAsync = true;
        }
    }

    function openSettingsWithTab(tabName: string) {
        if (settingsModal) {
            settingsModal.showWithTabName(tabName);
            return;
        }
        if (settingsModalLoader) {
            _settingsPendingTab = tabName;
            _settingsWantsOpen = true;
            _settingsWantsToggle = false;
            settingsModalLoader.activeAsync = true;
        }
    }

    function openSettingsWithTabIndex(tabIndex: int) {
        if (settingsModal) {
            settingsModal.showWithTab(tabIndex);
            return;
        }
        if (settingsModalLoader) {
            _settingsPendingTabIndex = tabIndex;
            _settingsWantsOpen = true;
            _settingsWantsToggle = false;
            settingsModalLoader.activeAsync = true;
        }
    }

    function closeSettings() {
        settingsModal?.close();
    }

    function toggleSettings() {
        if (settingsModal) {
            settingsModal.toggle();
        } else if (settingsModalLoader) {
            _settingsWantsToggle = true;
            _settingsWantsOpen = false;
            settingsModalLoader.activeAsync = true;
        }
    }

    function toggleSettingsWithTab(tabName: string) {
        if (settingsModal) {
            var idx = settingsModal.resolveTabIndex(tabName);
            settingsModal.setTabIndex(idx);
            settingsModal.toggle();
            return;
        }
        if (settingsModalLoader) {
            _settingsPendingTab = tabName;
            _settingsWantsToggle = true;
            _settingsWantsOpen = false;
            settingsModalLoader.activeAsync = true;
        }
    }

    function focusOrToggleSettings() {
        if (settingsModal?.visible) {
            const settingsTitle = I18n.tr("Settings", "settings window title");
            for (const toplevel of ToplevelManager.toplevels.values) {
                if (toplevel.title !== "Settings" && toplevel.title !== settingsTitle)
                    continue;
                if (toplevel.activated) {
                    settingsModal.hide();
                    return;
                }
                toplevel.activate();
                return;
            }
        }
        openSettings();
    }

    function focusOrToggleSettingsWithTab(tabName: string) {
        if (settingsModal?.visible) {
            const settingsTitle = I18n.tr("Settings", "settings window title");
            for (const toplevel of ToplevelManager.toplevels.values) {
                if (toplevel.title !== "Settings" && toplevel.title !== settingsTitle)
                    continue;
                if (toplevel.activated) {
                    settingsModal.hide();
                    return;
                }
                var idx = settingsModal.resolveTabIndex(tabName);
                settingsModal.setTabIndex(idx);
                toplevel.activate();
                return;
            }
        }
        openSettingsWithTab(tabName);
    }

    function unloadSettings() {
        if (settingsModalLoader) {
            settingsModal = null;
            settingsModalLoader.active = false;
        }
    }

    function _onSettingsModalLoaded() {
        if (_settingsWantsOpen) {
            _settingsWantsOpen = false;
            if (_settingsPendingTabIndex >= 0) {
                settingsModal?.showWithTab(_settingsPendingTabIndex);
                _settingsPendingTabIndex = -1;
            } else if (_settingsPendingTab) {
                settingsModal?.showWithTabName(_settingsPendingTab);
                _settingsPendingTab = "";
            } else {
                settingsModal?.show();
            }
            return;
        }
        if (_settingsWantsToggle) {
            _settingsWantsToggle = false;
            if (_settingsPendingTabIndex >= 0) {
                settingsModal?.setTabIndex(_settingsPendingTabIndex);
                _settingsPendingTabIndex = -1;
            } else if (_settingsPendingTab) {
                var idx = settingsModal?.resolveTabIndex(_settingsPendingTab) ?? -1;
                settingsModal?.setTabIndex(idx);
                _settingsPendingTab = "";
            }
            settingsModal?.toggle();
        }
    }

    function openClipboardHistory() {
        clipboardHistoryModal?.show();
    }

    function closeClipboardHistory() {
        clipboardHistoryModal?.close();
    }

    function unloadClipboardHistoryPopout() {
        if (!clipboardHistoryPopoutLoader)
            return;
        clipboardHistoryPopout = null;
        clipboardHistoryPopoutLoader.active = false;
    }

    function unloadLayoutPopout() {
        if (!layoutPopoutLoader)
            return;
        layoutPopout = null;
        layoutPopoutLoader.active = false;
    }

    property bool _hgsLauncherV2WantsOpen: false
    property bool _hgsLauncherV2WantsToggle: false
    property string _hgsLauncherV2PendingQuery: ""
    property string _hgsLauncherV2PendingMode: ""
    property bool _hgsLauncherV2TriggerUsesOverlayLayer: false

    function _setHGSLauncherV2TriggerUsesOverlayLayer(value) {
        _hgsLauncherV2TriggerUsesOverlayLayer = value === true;
        if (hgsLauncherV2Modal)
            hgsLauncherV2Modal.triggerUsesOverlayLayer = _hgsLauncherV2TriggerUsesOverlayLayer;
    }

    function openHGSLauncherV2(triggerUsesOverlayLayer) {
        _setHGSLauncherV2TriggerUsesOverlayLayer(triggerUsesOverlayLayer);
        if (hgsLauncherV2Modal) {
            hgsLauncherV2Modal.show();
        } else if (hgsLauncherV2ModalLoader) {
            _hgsLauncherV2WantsOpen = true;
            _hgsLauncherV2WantsToggle = false;
            hgsLauncherV2ModalLoader.active = true;
        }
    }

    function openHGSLauncherV2WithQuery(query: string, triggerUsesOverlayLayer) {
        _setHGSLauncherV2TriggerUsesOverlayLayer(triggerUsesOverlayLayer);
        if (hgsLauncherV2Modal) {
            hgsLauncherV2Modal.showWithQuery(query);
        } else if (hgsLauncherV2ModalLoader) {
            _hgsLauncherV2PendingQuery = query;
            _hgsLauncherV2WantsOpen = true;
            _hgsLauncherV2WantsToggle = false;
            hgsLauncherV2ModalLoader.active = true;
        }
    }

    function openHGSLauncherV2WithMode(mode: string, triggerUsesOverlayLayer) {
        _setHGSLauncherV2TriggerUsesOverlayLayer(triggerUsesOverlayLayer);
        if (hgsLauncherV2Modal) {
            hgsLauncherV2Modal.showWithMode(mode);
        } else if (hgsLauncherV2ModalLoader) {
            _hgsLauncherV2PendingMode = mode;
            _hgsLauncherV2WantsOpen = true;
            _hgsLauncherV2WantsToggle = false;
            hgsLauncherV2ModalLoader.active = true;
        }
    }

    function closeHGSLauncherV2() {
        hgsLauncherV2Modal?.hide();
    }

    function unloadHGSLauncherV2() {
        if (hgsLauncherV2ModalLoader) {
            hgsLauncherV2Modal = null;
            hgsLauncherV2ModalLoader.active = false;
        }
    }

    function toggleHGSLauncherV2(triggerUsesOverlayLayer) {
        _setHGSLauncherV2TriggerUsesOverlayLayer(triggerUsesOverlayLayer);
        if (hgsLauncherV2Modal) {
            hgsLauncherV2Modal.toggle();
        } else if (hgsLauncherV2ModalLoader) {
            _hgsLauncherV2WantsToggle = true;
            _hgsLauncherV2WantsOpen = false;
            hgsLauncherV2ModalLoader.active = true;
        }
    }

    function toggleHGSLauncherV2WithMode(mode: string, triggerUsesOverlayLayer) {
        _setHGSLauncherV2TriggerUsesOverlayLayer(triggerUsesOverlayLayer);
        if (hgsLauncherV2Modal) {
            hgsLauncherV2Modal.toggleWithMode(mode);
        } else if (hgsLauncherV2ModalLoader) {
            _hgsLauncherV2PendingMode = mode;
            _hgsLauncherV2WantsToggle = true;
            _hgsLauncherV2WantsOpen = false;
            hgsLauncherV2ModalLoader.active = true;
        }
    }

    function toggleHGSLauncherV2WithQuery(query: string, triggerUsesOverlayLayer) {
        _setHGSLauncherV2TriggerUsesOverlayLayer(triggerUsesOverlayLayer);
        if (hgsLauncherV2Modal) {
            hgsLauncherV2Modal.toggleWithQuery(query);
        } else if (hgsLauncherV2ModalLoader) {
            _hgsLauncherV2PendingQuery = query;
            _hgsLauncherV2WantsOpen = true;
            _hgsLauncherV2WantsToggle = false;
            hgsLauncherV2ModalLoader.active = true;
        }
    }

    function _onHGSLauncherV2ModalLoaded() {
        if (hgsLauncherV2Modal)
            hgsLauncherV2Modal.triggerUsesOverlayLayer = _hgsLauncherV2TriggerUsesOverlayLayer;
        if (_hgsLauncherV2WantsOpen) {
            _hgsLauncherV2WantsOpen = false;
            if (_hgsLauncherV2PendingQuery) {
                hgsLauncherV2Modal?.showWithQuery(_hgsLauncherV2PendingQuery);
                _hgsLauncherV2PendingQuery = "";
            } else if (_hgsLauncherV2PendingMode) {
                hgsLauncherV2Modal?.showWithMode(_hgsLauncherV2PendingMode);
                _hgsLauncherV2PendingMode = "";
            } else {
                hgsLauncherV2Modal?.show();
            }
            return;
        }
        if (_hgsLauncherV2WantsToggle) {
            _hgsLauncherV2WantsToggle = false;
            if (_hgsLauncherV2PendingMode) {
                hgsLauncherV2Modal?.toggleWithMode(_hgsLauncherV2PendingMode);
                _hgsLauncherV2PendingMode = "";
            } else {
                hgsLauncherV2Modal?.toggle();
            }
        }
    }

    property bool _spotlightBarWantsOpen: false
    property bool _spotlightBarWantsToggle: false

    function openSpotlightBar() {
        if (spotlightBarModal) {
            spotlightBarModal.show();
        } else if (spotlightBarModalLoader) {
            _spotlightBarWantsOpen = true;
            _spotlightBarWantsToggle = false;
            spotlightBarModalLoader.active = true;
        }
    }

    function closeSpotlightBar() {
        spotlightBarModal?.hide();
    }

    function toggleSpotlightBar() {
        if (spotlightBarModal) {
            spotlightBarModal.toggle();
        } else if (spotlightBarModalLoader) {
            _spotlightBarWantsToggle = true;
            _spotlightBarWantsOpen = false;
            spotlightBarModalLoader.active = true;
        }
    }

    function _onSpotlightBarModalLoaded() {
        if (_spotlightBarWantsOpen) {
            _spotlightBarWantsOpen = false;
            spotlightBarModal?.show();
            return;
        }
        if (_spotlightBarWantsToggle) {
            _spotlightBarWantsToggle = false;
            spotlightBarModal?.toggle();
        }
    }

    function openPowerMenu() {
        powerMenuModal?.openCentered();
    }

    function closePowerMenu() {
        powerMenuModal?.close();
    }

    function togglePowerMenu() {
        if (powerMenuModal) {
            if (powerMenuModal.shouldBeVisible) {
                powerMenuModal.close();
            } else {
                powerMenuModal.openCentered();
            }
        }
    }

    function openPowerProfileModal() {
        if (powerProfileModal) {
            powerProfileModal.openCentered();
        } else if (powerProfileModalLoader) {
            powerProfileModalLoader.active = true;
            Qt.callLater(() => powerProfileModal?.openCentered());
        }
    }

    function closePowerProfileModal() {
        powerProfileModal?.close();
    }

    function togglePowerProfileModal() {
        if (powerProfileModal) {
            if (powerProfileModal.shouldBeVisible) {
                powerProfileModal.close();
            } else {
                powerProfileModal.openCentered();
            }
        } else if (powerProfileModalLoader) {
            powerProfileModalLoader.active = true;
            Qt.callLater(() => {
                if (powerProfileModal) {
                    if (powerProfileModal.shouldBeVisible) {
                        powerProfileModal.close();
                    } else {
                        powerProfileModal.openCentered();
                    }
                }
            });
        }
    }

    function showProcessListModal() {
        if (processListModal) {
            processListModal.show();
        } else if (processListModalLoader) {
            processListModalLoader.active = true;
            Qt.callLater(() => processListModal?.show());
        }
    }

    function hideProcessListModal() {
        processListModal?.hide();
    }

    function unloadProcessListModal() {
        if (processListModalLoader) {
            processListModal = null;
            processListModalLoader.active = false;
        }
    }

    function toggleProcessListModal() {
        if (processListModal) {
            processListModal.toggle();
        } else if (processListModalLoader) {
            processListModalLoader.active = true;
            Qt.callLater(() => processListModal?.show());
        }
    }

    function showColorPicker() {
        colorPickerModal?.show();
    }

    function hideColorPicker() {
        colorPickerModal?.close();
    }

    function showNotificationModal() {
        notificationModal?.show();
    }

    function hideNotificationModal() {
        notificationModal?.close();
    }

    function showWifiPasswordModal(ssid) {
        if (wifiPasswordModalLoader)
            wifiPasswordModalLoader.active = true;
        if (wifiPasswordModal) {
            wifiPasswordModal.show(ssid);
        } else {
            Qt.callLater(() => wifiPasswordModal?.show(ssid));
        }
    }

    function showWifiQRCodeModal(ssid) {
        if (wifiQRCodeModalLoader)
            wifiQRCodeModalLoader.active = true;
        if (wifiQRCodeModal)
            wifiQRCodeModal.show(ssid);
    }

    function showHiddenNetworkModal() {
        if (wifiPasswordModalLoader)
            wifiPasswordModalLoader.active = true;
        if (wifiPasswordModal) {
            wifiPasswordModal.showHidden();
        } else {
            Qt.callLater(() => wifiPasswordModal?.showHidden());
        }
    }

    function hideWifiPasswordModal() {
        wifiPasswordModal?.hide();
    }

    function showNetworkInfoModal() {
        networkInfoModal?.show();
    }

    function hideNetworkInfoModal() {
        networkInfoModal?.close();
    }

    function closeNotepadSlideouts() {
        for (var i = 0; i < notepadSlideouts.length; i++) {
            if (notepadSlideouts[i] && notepadSlideouts[i].isVisible)
                notepadSlideouts[i].hide();
        }
    }

    function openNotepadSlideout() {
        notepadPopout?.hide();
        if (notepadSlideouts.length > 0) {
            notepadSlideouts[0]?.show();
        }
    }

    // Keep the notepad in a single presentation for default modes
    Connections {
        target: SettingsData
        function onNotepadDefaultModeChanged() {
            if (SettingsData.notepadDefaultMode === "popout") {
                var hadSlideout = false;
                for (var i = 0; i < root.notepadSlideouts.length; i++) {
                    if (root.notepadSlideouts[i] && root.notepadSlideouts[i].isVisible) {
                        hadSlideout = true;
                        root.notepadSlideouts[i].hide();
                    }
                }
                if (hadSlideout)
                    root.openNotepadPopout();
            } else if (root.notepadPopout && root.notepadPopout.visible) {
                root.notepadPopout.hide();
                root.openNotepadSlideout();
            }
        }
    }

    function openNotepad() {
        if (SettingsData.notepadDefaultMode === "popout") {
            openNotepadPopout();
            return;
        }
        openNotepadSlideout();
    }

    function closeNotepad() {
        if (SettingsData.notepadDefaultMode === "popout") {
            notepadPopout?.hide();
            return;
        }
        if (notepadSlideouts.length > 0) {
            notepadSlideouts[0]?.hide();
        }
    }

    function toggleNotepad() {
        if (SettingsData.notepadDefaultMode === "popout") {
            toggleNotepadPopout();
            return;
        }
        if (notepadSlideouts.length > 0) {
            notepadSlideouts[0]?.toggle();
        }
    }

    property var notepadPopout: null
    property var notepadPopoutLoader: null
    property bool _notepadPopoutWantsOpen: false

    function openNotepadPopout() {
        closeNotepadSlideouts();
        if (notepadPopout) {
            notepadPopout.show();
        } else if (notepadPopoutLoader) {
            _notepadPopoutWantsOpen = true;
            notepadPopoutLoader.active = true;
        }
    }

    function _onNotepadPopoutLoaded() {
        if (_notepadPopoutWantsOpen && notepadPopout) {
            _notepadPopoutWantsOpen = false;
            notepadPopout.show();
        }
    }

    function toggleNotepadPopout() {
        if (notepadPopout) {
            if (!notepadPopout.visible)
                closeNotepadSlideouts();
            notepadPopout.toggle();
        } else {
            openNotepadPopout();
        }
    }
}
