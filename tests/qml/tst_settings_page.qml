import QtQuick
import QtQuick.Window
import QtTest
import HyprShelld.Client
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
            shellBorderEnabled: true
            shellBorderWidth: 1
            shellBorderRadius: 15
            syncHyprlandWindowBorders: true
            workspaceShowIdentifiers: true
            workspaceShowNames: false
            workspaceShowApplications: false
            workspaceMaximumApplications: 3
            workspaceOccupiedOnly: false
            workspaceScrollMode: "disabled"
            workspaceInstanceAvailable: true
        }
    }

    Component {
        id: mainComponent

        Settings.Main {
            visible: false
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

    function enableCoreSettings(page) {
        page.coreServiceAvailable = true;
    }

    function enableWorkspaceSettings(page) {
        page.componentServiceAvailable = true;
        page.componentCatalogAvailable = true;
        page.componentWritable = true;
        page.workspaceInstanceAvailable = true;
    }

    Component {
        id: workspaceSettingsComponent

        Window {
            width: 520
            height: 900
            visible: true

            property alias settings: workspaceSettings

            Settings.WorkspaceSwitcherSettings {
                id: workspaceSettings

                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: 12
                }
                showIdentifiers: true
                showNames: false
                showApplications: false
                maximumApplications: 3
                occupiedOnly: false
                scrollMode: "disabled"
                controlsEnabled: true
            }
        }
    }

    Component {
        id: componentsPageComponent

        Window {
            width: 760
            height: 720
            visible: true

            property alias page: componentsPage

            Settings.ComponentsPage {
                id: componentsPage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: displaysPageComponent

        Window {
            width: 820
            height: 900
            visible: true

            property alias page: displaysPage

            Settings.DisplaysPage {
                id: displaysPage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: appearancePageComponent

        Window {
            width: 820
            height: 900
            visible: true

            property alias page: appearancePage

            Settings.AppearancePage {
                id: appearancePage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: appearancePreviewComponent

        Window {
            width: 720
            height: 420
            visible: true

            property alias preview: appearancePreview

            Settings.AppearancePreview {
                id: appearancePreview

                anchors.fill: parent
            }
        }
    }

    function appearanceDefinitions() {
        return [
            {
                id: "hyprland.general.border_size",
                type: "integer",
                control: "spinBox",
                defaultValue: 1,
                min: 0,
                max: 20
            },
            {
                id: "hyprland.decoration.rounding",
                type: "integer",
                control: "spinBox",
                defaultValue: 0,
                min: 0,
                max: 20
            },
            {
                id: "hyprland.decoration.blur.enabled",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.decoration.shadow.enabled",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.animations.enabled",
                type: "boolean",
                control: "toggle",
                defaultValue: true
            },
            {
                id: "hyprland.general.layout",
                type: "enum",
                control: "select",
                defaultValue: "dwindle",
                choices: [
                    { label: "dwindle", value: "dwindle" },
                    { label: "master", value: "master" },
                    { label: "scrolling", value: "scrolling" },
                    { label: "monocle", value: "monocle" }
                ]
            },
            {
                id: "hyprland.general.resize_on_border",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            },
            {
                id: "hyprland.general.snap.enabled",
                type: "boolean",
                control: "toggle",
                defaultValue: false
            }
        ];
    }

    function appearanceDefaults() {
        return {
            "hyprland.general.border_size": 1,
            "hyprland.decoration.rounding": 0,
            "hyprland.decoration.blur.enabled": true,
            "hyprland.decoration.shadow.enabled": true,
            "hyprland.animations.enabled": true,
            "hyprland.general.layout": "dwindle",
            "hyprland.general.resize_on_border": false,
            "hyprland.general.snap.enabled": false
        };
    }

    function configureAppearancePage(page, values, windowBorderSynced) {
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.appearanceOptions = appearanceDefinitions();
        page.appearanceValues = values || appearanceDefaults();
        page.sharedBorderAvailable = true;
        page.sharedBorderBusy = false;
        page.windowBorderSynced = windowBorderSynced === true;
        page.sharedBorderSyncState = windowBorderSynced === true
            ? "current" : "override";
        page.sharedBorderSyncError = "";
        page.sharedBorderClientError = "";
        page.sharedBorderConfigRevisionToken = "11";
        page.sharedBorderVerifiedRevisionToken = "11";
        page.revisionToken = "7";
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.appearanceAvailable = true;
        page.reviewProjection();
    }

    function displayRecord(id, selector, enabled, mirror, vrr) {
        return {
            id: id,
            selector: selector,
            enabled: enabled,
            mode: "1920x1080@60",
            position: "0x0",
            scale: 1,
            reserved: [0, 0, 0, 0],
            transform: 0,
            mirror: mirror || "",
            bitdepth: 8,
            cm: "auto",
            sdrEotf: "default",
            sdrBrightness: 1,
            sdrSaturation: 1,
            vrr: vrr === undefined ? -1 : vrr,
            icc: "",
            supportsWideColor: -1,
            supportsHdr: -1,
            sdrMinLuminance: 0.2,
            sdrMaxLuminance: 80,
            minLuminance: -1,
            maxLuminance: -1,
            maxAvgLuminance: -1
        };
    }

    function connectedDisplay(selector, enabled, x, format, sdrMinimum) {
        return {
            selector: selector,
            description: selector + " test display",
            make: "Example",
            model: "Panel",
            serial: selector + "-serial",
            enabled: enabled,
            width: 1920,
            height: 1080,
            physicalWidthMm: 520,
            physicalHeightMm: 290,
            refreshRate: 60,
            x: x || 0,
            y: 0,
            scale: 1,
            transform: 0,
            focused: x === 0,
            dpms: true,
            vrrActive: false,
            mirrorOf: "",
            modes: [{
                width: 1920,
                height: 1080,
                refreshRate: 60,
                managedMode: "1920x1080@60"
            }],
            colorManagement: "srgb",
            currentFormat: format || "XRGB8888",
            sdrBrightness: 1,
            sdrSaturation: 1,
            sdrMinLuminance: sdrMinimum === undefined ? 0.2 : sdrMinimum,
            sdrMaxLuminance: 80
        };
    }

    function configureDisplaysPage(page, records, topology) {
        page.serviceAvailable = true;
        page.writable = true;
        page.revision = 7;
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.snapshot = { monitors: records };
        page.connectedDisplays = topology;
        page.topologyDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.synchronizeDraft(false);
    }

    function workspaceCatalogRecord() {
        return {
            id: "io.github.coastlinesec.hyprshelld.workspace-switcher",
            type: "bar-widget",
            version: "0.2.0",
            name: "Workspace Switcher",
            description: "Shows and activates workspaces on each display.",
            authors: [{ name: "CoastLineSec", email: "", homepage: "" }],
            license: "LicenseRef-HyprShelld",
            packageDigest:
                "4887e8c9e981ce892d39382e696de83d5b2dee4236e83db6da84780064aeaf54",
            origin: "system",
            removable: false,
            hasSettings: true,
            activationSupported: true,
            compatibilityReason: "",
            settingsDefinitions: [],
            requestedCapabilities: []
        };
    }

    function thirdPartyServiceRecord() {
        return {
            id: "org.example.local-service",
            type: "shell-service",
            version: "1.2.3",
            name: "Local Service",
            description: "A locally installed test service.",
            authors: [{ name: "Example Author" }],
            license: "MIT",
            packageDigest:
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            origin: "user",
            removable: true,
            hasSettings: true,
            activationSupported: false,
            compatibilityReason:
                "Runtime activation for third-party components is not available yet.",
            settingsDefinitions: [
                {
                    key: "logging",
                    scope: "component",
                    type: "boolean",
                    label: "Enable logging",
                    description: "Write diagnostic messages.",
                    group: "behavior",
                    order: 10,
                    defaultValue: false,
                    options: []
                },
                {
                    key: "mode",
                    scope: "component",
                    type: "enum",
                    label: "Logging mode",
                    description: "Choose how much information is recorded.",
                    group: "behavior",
                    order: 20,
                    defaultValue: "quiet",
                    options: [
                        { value: "quiet", label: "Quiet" },
                        {
                            value: "verbose",
                            label: "Verbose <img src=https://example.invalid/x>"
                        }
                    ],
                    visibleWhen: { key: "logging", equals: true }
                },
                {
                    key: "instanceTitle",
                    scope: "instance",
                    type: "string",
                    label: "Instance title",
                    description: "A title for one placement.",
                    group: "appearance",
                    order: 10,
                    defaultValue: "Widget",
                    minimumLength: 1,
                    maximumLength: 64,
                    options: []
                }
            ],
            requestedCapabilities: [
                { id: "example.read", reason: "Read example state." }
            ],
            dependencies: []
        };
    }

    function thirdPartyApplicationRecord() {
        const record = thirdPartyServiceRecord();
        record.id = "org.example.local-application";
        record.type = "shell-application";
        record.name = "Local Application";
        record.packageDigest =
            "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
        return record;
    }

    function thirdPartyDeclarativeWidgetRecord() {
        return {
            id: "org.example.clock-widget",
            type: "bar-widget",
            version: "1.0.0",
            name: "Clock Widget",
            description: "A data-only local clock label.",
            authors: [{ name: "Example Author" }],
            license: "MIT",
            packageDigest:
                "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
            origin: "user",
            removable: true,
            hasSettings: true,
            activationSupported: true,
            compatibilityReason: "",
            runtime: {
                kind: "declarative-v1",
                entrypoint: "payload/widget.json"
            },
            settingsDefinitions: [{
                key: "label",
                scope: "component",
                type: "string",
                label: "Label",
                description: "Text shown in the bar.",
                group: "appearance",
                order: 10,
                defaultValue: "Clock",
                minimumLength: 1,
                maximumLength: 32,
                options: []
            }],
            requestedCapabilities: [],
            dependencies: []
        };
    }

    function configureSnapshotForComponent(component, enabled) {
        const components = {};
        components[component.id] = {
            packageDigest: component.packageDigest,
            enabled: enabled,
            grantedCapabilities: [],
            settings: {}
        };
        return {
            formatVersion: 1,
            revision: "4",
            components: components,
            instances: {},
            layouts: { bars: {}, desktops: {} }
        };
    }

    function configureSnapshotForComponents(components, enabled) {
        const records = {};
        for (const component of components) {
            records[component.id] = {
                packageDigest: component.packageDigest,
                enabled: enabled,
                grantedCapabilities: [],
                settings: component.origin === "user"
                    ? { logging: false, mode: "quiet" } : {}
            };
        }
        return {
            formatVersion: 1,
            revision: "4",
            components: records,
            instances: {},
            layouts: { bars: {}, desktops: {} }
        };
    }

    function configureComponentsPage(page) {
        const component = workspaceCatalogRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [component];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        page.configSnapshot = configureSnapshotForComponent(component, true);
    }

    function test_serviceAvailability() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);

        const coreWarning = findChild(page, "coreServiceWarning");
        const componentWarning = findChild(
            page,
            "componentServiceWarning"
        );
        const control = findChild(page, "barHeightControl");
        const preview = findChild(page, "barPreview");
        const workspaceSettings = findChild(
            page,
            "workspaceSwitcherSettingsCard"
        );
        verify(coreWarning !== null);
        verify(componentWarning !== null);
        verify(control !== null);
        verify(preview !== null);
        verify(workspaceSettings !== null);
        compare(page.coreServiceWarningVisible, true);
        compare(page.componentServiceWarningVisible, true);
        compare(page.coreControlsEnabled, false);
        compare(page.workspaceControlsEnabled, false);
        compare(control.busy, true);
        compare(preview.barHeight, 40);
        compare(preview.configurationAvailable, false);
        compare(workspaceSettings.controlsEnabled, false);

        enableCoreSettings(page);
        compare(page.coreServiceWarningVisible, false);
        compare(page.componentServiceWarningVisible, true);
        compare(page.coreControlsEnabled, true);
        compare(page.workspaceControlsEnabled, false);
        compare(control.busy, false);
        compare(preview.configurationAvailable, true);

        enableWorkspaceSettings(page);
        compare(page.componentServiceWarningVisible, false);
        compare(page.workspaceControlsEnabled, true);
        compare(workspaceSettings.controlsEnabled, true);

        page.coreServiceAvailable = false;
        compare(page.coreControlsEnabled, false);
        compare(page.workspaceControlsEnabled, true);
        compare(control.enabled, false);
        compare(workspaceSettings.controlsEnabled, true);

        page.coreServiceAvailable = true;
        page.componentCatalogAvailable = false;
        compare(page.coreControlsEnabled, true);
        compare(page.workspaceControlsEnabled, false);
        compare(control.enabled, true);
        compare(workspaceSettings.controlsEnabled, false);
        verify(page.componentWarningMessage.includes("catalog"));

        page.componentRecoveryState = "unsupported";
        page.componentServiceAvailable = false;
        verify(page.componentWarningMessage.includes("cannot read"));
        compare(page.coreControlsEnabled, true);
        page.componentServiceAvailable = true;
        verify(page.componentWarningMessage.includes("recovery copy"));
        compare(page.workspaceControlsEnabled, false);
    }

    function test_recoveryMessages() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);

        const coreWarning = findChild(page, "coreRecoveryWarning");
        const componentWarning = findChild(
            page,
            "componentRecoveryWarning"
        );
        verify(coreWarning !== null);
        verify(componentWarning !== null);
        compare(page.coreRecoveryWarningVisible, false);
        compare(page.componentRecoveryWarningVisible, false);

        page.coreRecoveryState = "recovered";
        compare(page.coreRecoveryWarningVisible, true);
        verify(page.coreRecoveryMessage.includes("last known good"));
        compare(page.componentRecoveryWarningVisible, false);

        page.coreRecoveryState = "normal";
        page.componentRecoveryState = "defaulted";
        compare(page.coreRecoveryWarningVisible, false);
        compare(page.componentRecoveryWarningVisible, true);
        verify(page.componentRecoveryMessage.includes("safe defaults"));

        page.componentRecoveryState = "normal";
        compare(page.componentRecoveryWarningVisible, false);
    }

    function test_componentsPageUsesFixedCategoriesAndProtectedBuiltinRow() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureComponentsPage(page);
        waitForRendering(page);
        wait(0);

        const categories = [
            findChild(page, "componentCategory-bar-widget"),
            findChild(page, "componentCategory-desktop-widget"),
            findChild(page, "componentCategory-shell-service"),
            findChild(page, "componentCategory-shell-application")
        ];
        for (const category of categories)
            verify(category !== null);
        compare(categories.map(category => category.text), [
            "Bar Widgets",
            "Desktop Widgets",
            "Services",
            "Shell Applications"
        ]);
        for (let index = 1; index < categories.length; ++index) {
            verify(categories[index].mapToItem(page, 0, 0).y
                > categories[index - 1].mapToItem(page, 0, 0).y);
        }

        const componentId = workspaceCatalogRecord().id;
        const pill = findChild(page, "componentPill-" + componentId);
        const origin = findChild(page, "componentOrigin-" + componentId);
        const toggle = findChild(page, "componentEnabled-" + componentId);
        verify(pill !== null);
        verify(origin !== null);
        verify(toggle !== null);
        compare(origin.text, "Built-in");
        compare(toggle.checked, true);
        compare(toggle.enabled, true);
        compare(
            findChild(page, "componentSettings-" + componentId).visible,
            false
        );
        compare(
            findChild(page, "componentRemove-" + componentId).visible,
            false
        );
        const install = findChild(page, "installComponent");
        verify(install !== null);
        compare(install.enabled, true);

        let requested = [];
        let requestCount = 0;
        page.componentEnabledRequested.connect(function(
            id,
            packageDigest,
            enabled
        ) {
            ++requestCount;
            requested = [id, packageDigest, enabled];
        });
        toggle.checked = false;
        toggle.toggled();
        compare(requested, [
            componentId,
            workspaceCatalogRecord().packageDigest,
            false
        ]);
        compare(requestCount, 1);

        page.lastErrorComponentId = componentId;
        page.configError = "The package digest no longer matches.";
        wait(0);
        compare(toggle.checked, true);
        compare(requestCount, 1);

        page.lastErrorComponentId = "";
        page.configError = "";
        toggle.checked = false;
        toggle.toggled();
        compare(requestCount, 2);
        page.pendingComponentId = componentId;
        page.configBusy = true;
        wait(0);
        compare(toggle.checked, false);
        page.lastErrorComponentId = componentId;
        page.configError = "The change could not be saved.";
        page.pendingComponentId = "";
        page.configBusy = false;
        wait(0);
        compare(toggle.checked, true);
        compare(requestCount, 2);
    }

    function test_componentsToggleRequiresLiveDigestBoundState() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureComponentsPage(page);
        waitForRendering(page);
        wait(0);

        const component = workspaceCatalogRecord();
        const toggle = findChild(page, "componentEnabled-" + component.id);
        const status = findChild(page, "componentStatus-" + component.id);
        const warning = findChild(page, "componentsAvailabilityWarning");
        verify(toggle !== null);
        verify(status !== null);
        verify(warning !== null);
        compare(toggle.enabled, true);
        compare(warning.visible, false);

        page.managerBusy = true;
        compare(toggle.enabled, false);
        page.managerBusy = false;
        page.configBusy = true;
        compare(toggle.enabled, false);
        page.configBusy = false;

        page.configCatalogDigest =
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        compare(toggle.enabled, false);
        compare(warning.visible, true);
        verify(page.availabilityMessage.includes("both services agree"));

        page.configCatalogDigest = page.managerCatalogDigest;
        const mismatched = JSON.parse(JSON.stringify(page.configSnapshot));
        mismatched.components[component.id].packageDigest =
            "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
        page.configSnapshot = mismatched;
        compare(toggle.enabled, false);
        compare(status.text,
            "Configuration does not match the installed package.");

        page.configSnapshot = configureSnapshotForComponent(component, true);
        page.lastErrorComponentId = component.id;
        page.configError = "The enablement change could not be saved.";
        compare(status.text, "The enablement change could not be saved.");
        compare(status.Accessible.role, Accessible.AlertMessage);

        page.lastErrorComponentId = "org.example.some-other-component";
        compare(status.text, "Enabled");
    }

    function test_localPackagePickerAndReviewAreExplicit() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureComponentsPage(page);
        waitForRendering(page);
        wait(0);

        const install = findChild(page, "installComponent");
        verify(install !== null);

        let selectedUrl = "";
        page.inspectPackageRequested.connect(function(packageUrl) {
            selectedUrl = String(packageUrl);
        });
        page.inspectSelectedPackage(
            "file:///tmp/example.hyprshelld-component"
        );
        compare(
            selectedUrl,
            "file:///tmp/example.hyprshelld-component"
        );

        page.inspectionToken = "0123456789abcdef0123456789abcdef";
        wait(0);

        const review = findChild(page, "componentReviewDialog");
        verify(review !== null);
        compare(review.opened, false);

        page.inspectionReview = {
            operation: "install",
            id: "org.example.local-service",
            name: "Local Service",
            description: "A locally selected component.",
            version: "1.2.3",
            type: "shell-service",
            authors: [{ name: "Example Author" }],
            license: "MIT",
            runtime: { kind: "process-v1" },
            packageDigest:
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            activationSupported: false,
            compatibilityReason: "Activation is not available yet.",
            requestedCapabilities: [
                { id: "example.read", reason: "Read example state." }
            ],
            dependencies: []
        };
        wait(0);

        const warning = findChild(review, "componentUnverifiedWarning");
        const name = findChild(review, "componentReviewName");
        const capabilities = findChild(
            review,
            "componentReviewCapabilities"
        );
        const activation = findChild(
            review,
            "componentActivationNoticeText"
        );
        const confirm = findChild(
            review,
            "confirmComponentInstallation"
        );
        const cancel = findChild(
            review,
            "cancelComponentInstallation"
        );
        verify(review !== null);
        verify(warning !== null);
        verify(name !== null);
        verify(capabilities !== null);
        verify(activation !== null);
        verify(confirm !== null);
        verify(cancel !== null);
        compare(review.opened, true);
        compare(name.text, "Local Service");
        verify(capabilities.text.includes("example.read"));
        verify(activation.text.includes("cannot activate"));
        verify(activation.text.includes("does not change saved state"));
        const supportedReview = Object.assign({}, page.inspectionReview, {
            activationSupported: true,
            compatibilityReason: ""
        });
        page.inspectionReview = supportedReview;
        wait(0);
        verify(activation.text.includes("does not change saved enablement"));
        verify(activation.text.includes("exact version"));

        let installRequests = 0;
        page.installInspectedPackageRequested.connect(function() {
            ++installRequests;
            page.packageOperationBusy = true;
        });
        confirm.clicked();
        compare(installRequests, 1);
        compare(review.opened, true);
        compare(confirm.enabled, false);

        page.packageOperationBusy = false;
        page.inspectionToken = "";
        wait(0);
        compare(review.opened, false);

        page.inspectionToken = "0123456789abcdef0123456789abcdef";
        wait(0);
        compare(review.opened, true);

        let cancelRequests = 0;
        page.cancelInspectionRequested.connect(function() {
            ++cancelRequests;
        });
        cancel.clicked();
        compare(cancelRequests, 1);
    }

    function test_thirdPartyRowsExposeTrustedActionsOnly() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const service = thirdPartyServiceRecord();
        const application = thirdPartyApplicationRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [workspaceCatalogRecord(), service, application];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        page.configSnapshot = configureSnapshotForComponents(
            page.components,
            false
        );
        waitForRendering(page);
        wait(0);

        const trust = findChild(page, "componentTrust-" + service.id);
        const status = findChild(page, "componentStatus-" + service.id);
        const toggle = findChild(page, "componentEnabled-" + service.id);
        const configure = findChild(
            page,
            "componentSettings-" + service.id
        );
        const remove = findChild(page, "componentRemove-" + service.id);
        verify(trust !== null);
        verify(status !== null);
        verify(toggle !== null);
        verify(configure !== null);
        verify(remove !== null);
        compare(trust.text, "Unverified third-party code");
        verify(status.text.includes("Installed disabled"));
        verify(status.text.includes(service.compatibilityReason));
        compare(page.toggleAvailable(service), false);
        compare(toggle.enabled, false);
        compare(configure.visible, true);
        compare(remove.visible, true);
        compare(remove.enabled, true);
        page.managerAvailable = false;
        wait(0);
        compare(remove.enabled, false);
        page.managerAvailable = true;
        wait(0);
        compare(remove.enabled, true);
        compare(
            findChild(page, "componentSettings-" + application.id).visible,
            false
        );

        let removed = [];
        page.packageRemovalRequested.connect(function(
            componentId,
            packageDigest,
            catalogDigest
        ) {
            removed = [componentId, packageDigest, catalogDigest];
        });
        remove.clicked();
        wait(0);
        const removalDialog = findChild(page, "componentRemovalDialog");
        verify(removalDialog !== null);
        const confirmRemoval = findChild(
            removalDialog,
            "confirmComponentRemoval"
        );
        verify(confirmRemoval !== null);
        compare(removalDialog.opened, true);
        compare(removed.length, 0);
        confirmRemoval.clicked();
        compare(removed, [
            service.id,
            service.packageDigest,
            page.managerCatalogDigest
        ]);
        page.packageRemovalCompleted(service.id);
        wait(0);
        compare(removalDialog.opened, false);
    }

    function test_unsupportedComponentStaysDisabledAfterSettingsRecord() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const service = thirdPartyServiceRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [service];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        page.configSnapshot = {
            formatVersion: 1,
            revision: "4",
            components: {},
            instances: {},
            layouts: { bars: {}, desktops: {} }
        };
        waitForRendering(page);
        wait(0);

        const configure = findChild(
            page,
            "componentSettings-" + service.id
        );
        const toggle = findChild(
            page,
            "componentEnabled-" + service.id
        );
        const status = findChild(
            page,
            "componentStatus-" + service.id
        );
        verify(configure !== null);
        verify(toggle !== null);
        verify(status !== null);
        compare(configure.enabled, true);
        compare(page.configRecord(service), null);
        compare(page.toggleAvailable(service), false);
        compare(toggle.enabled, false);

        configure.clicked();
        wait(0);
        const form = findChild(page, "genericComponentSettings");
        verify(form !== null);
        const save = findChild(form, "saveGenericComponentSettings");
        const logging = findChild(
            form,
            "componentSettingBoolean-logging"
        );
        verify(save !== null);
        verify(logging !== null);
        logging.checked = true;
        logging.clicked();
        wait(0);
        compare(save.enabled, true);
        save.clicked();

        page.configSnapshot = configureSnapshotForComponent(service, false);
        wait(0);
        verify(page.configRecord(service) !== null);
        compare(page.toggleAvailable(service), false);
        compare(toggle.enabled, false);
        verify(status.text.includes("Installed disabled"));
        verify(status.text.includes(service.compatibilityReason));
    }

    function test_genericSettingsRenderTrustedComponentScope() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const service = thirdPartyServiceRecord();
        service.activationSupported = true;
        service.compatibilityReason = "";
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [service];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        page.configSnapshot = configureSnapshotForComponents([service], false);
        waitForRendering(page);
        wait(0);

        const configure = findChild(
            page,
            "componentSettings-" + service.id
        );
        verify(configure !== null);
        configure.clicked();
        wait(0);

        const dialog = findChild(page, "componentSettingsDialog");
        const form = findChild(page, "genericComponentSettings");
        verify(dialog !== null);
        verify(form !== null);
        const logging = findChild(
            form,
            "componentSettingBoolean-logging"
        );
        const modeRow = findChild(form, "componentSetting-mode");
        const mode = findChild(form, "componentSettingEnum-mode");
        const instanceNotice = findChild(
            form,
            "componentInstanceSettingsNotice"
        );
        const save = findChild(form, "saveGenericComponentSettings");
        verify(logging !== null);
        verify(modeRow !== null);
        verify(mode !== null);
        verify(instanceNotice !== null);
        verify(save !== null);
        compare(dialog.opened, true);
        compare(logging.checked, false);
        compare(modeRow.visible, false);
        compare(instanceNotice.visible, true);
        compare(save.enabled, false);
        compare(mode.contentItem.textFormat, Text.PlainText);

        logging.checked = true;
        logging.clicked();
        wait(0);
        compare(modeRow.visible, true);
        mode.popup.open();
        wait(0);
        const unsafeOption = mode.popup.contentItem.itemAtIndex(1);
        verify(unsafeOption !== null);
        compare(
            unsafeOption.text,
            "Verbose <img src=https://example.invalid/x>"
        );
        compare(unsafeOption.Accessible.name, unsafeOption.text);
        compare(unsafeOption.contentItem.textFormat, Text.PlainText);
        compare(unsafeOption.contentItem.Accessible.ignored, true);
        compare(
            unsafeOption.contentItem.text,
            "Verbose <img src=https://example.invalid/x>"
        );
        mode.popup.close();
        mode.activated(1);
        compare(save.enabled, true);

        let saved = [];
        page.componentSettingsRequested.connect(function(
            componentId,
            packageDigest,
            settings
        ) {
            saved = [componentId, packageDigest, settings];
        });
        save.clicked();
        compare(saved[0], service.id);
        compare(saved[1], service.packageDigest);
        compare(saved[2].logging, true);
        compare(saved[2].mode, "verbose");
        verify(!Object.prototype.hasOwnProperty.call(
            saved[2],
            "instanceTitle"
        ));
    }

    function test_compatibleWidgetUsesAtomicAddToBarThenGlobalToggle() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const widget = thirdPartyDeclarativeWidgetRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [widget];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        const instanceId = "d9a61b25-670b-44cb-a824-9e52772e79f1";
        page.configSnapshot = {
            formatVersion: 1,
            revision: "4",
            components: {},
            instances: {},
            layouts: { bars: {}, desktops: {} }
        };
        waitForRendering(page);
        wait(0);

        const add = findChild(page, "componentAddToBar-" + widget.id);
        const toggle = findChild(page, "componentEnabled-" + widget.id);
        const status = findChild(page, "componentStatus-" + widget.id);
        verify(add !== null);
        verify(toggle !== null);
        verify(status !== null);
        compare(add.visible, true);
        compare(add.enabled, true);
        compare(toggle.visible, false);
        verify(status.text.includes("Add it to the bar"));

        const configuredComponents = {};
        configuredComponents[widget.id] = {
            packageDigest: widget.packageDigest,
            enabled: false,
            grantedCapabilities: [],
            settings: { label: "Clock" }
        };
        const unplacedInstances = {};
        unplacedInstances[instanceId] = {
            componentId: widget.id,
            enabled: false,
            settings: {}
        };
        page.configSnapshot = {
            formatVersion: 1,
            revision: "5",
            components: configuredComponents,
            instances: unplacedInstances,
            layouts: { bars: {}, desktops: {} }
        };
        wait(0);
        compare(add.visible, true);
        compare(toggle.visible, false);
        compare(status.text, "Configured but not on the bar.");

        page.configSnapshot = {
            formatVersion: 1,
            revision: "6",
            components: configuredComponents,
            instances: unplacedInstances,
            layouts: {
                bars: {
                    secondary: {
                        outputs: { mode: "all" },
                        regions: {
                            start: [],
                            center: [],
                            end: [instanceId]
                        }
                    }
                },
                desktops: {}
            }
        };
        wait(0);
        compare(add.visible, true);
        compare(toggle.visible, false);
        compare(status.text, "Configured but not on the bar.");

        page.configSnapshot = {
            formatVersion: 1,
            revision: "7",
            components: configuredComponents,
            instances: unplacedInstances,
            layouts: {
                bars: {
                    main: {
                        outputs: { mode: "all" },
                        regions: {
                            start: [], center: [], end: [instanceId]
                        }
                    }
                },
                desktops: {}
            }
        };
        wait(0);
        compare(add.visible, true);
        compare(toggle.visible, false);
        compare(status.text, "Configured but not on the bar.");

        let request = [];
        page.componentAddToBarRequested.connect(function(
            componentId,
            packageDigest,
            settings
        ) {
            request = [componentId, packageDigest, settings];
        });
        add.clicked();
        compare(request[0], widget.id);
        compare(request[1], widget.packageDigest);
        compare(request[2].label, "Clock");

        const components = {};
        components[widget.id] = {
            packageDigest: widget.packageDigest,
            enabled: true,
            grantedCapabilities: [],
            settings: { label: "Clock" }
        };
        const instances = {};
        instances[instanceId] = {
            componentId: widget.id,
            enabled: true,
            settings: {}
        };
        page.configSnapshot = {
            formatVersion: 1,
            revision: "5",
            components: components,
            instances: instances,
            layouts: {
                bars: {
                    main: {
                        outputs: { mode: "all" },
                        regions: { start: [], center: [], end: [instanceId] }
                    }
                },
                desktops: {}
            }
        };
        wait(0);
        compare(add.visible, false);
        compare(toggle.visible, true);
        compare(toggle.checked, true);
        compare(status.text, "Enabled");

        const disabled = JSON.parse(JSON.stringify(page.configSnapshot));
        disabled.components[widget.id].enabled = false;
        page.configSnapshot = disabled;
        wait(0);
        compare(add.visible, false);
        compare(toggle.visible, true);
        compare(toggle.checked, false);
        compare(status.text, "Installed disabled. Review it before enabling.");
    }

    function test_updatedSchemaFreeWidgetRequiresExplicitInertAdoption() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const widget = thirdPartyDeclarativeWidgetRecord();
        widget.settingsDefinitions = [];
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [widget];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        const instanceId = "d9a61b25-670b-44cb-a824-9e52772e79f1";
        const oldDigest =
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        const components = {};
        components[widget.id] = {
            packageDigest: oldDigest,
            enabled: true,
            grantedCapabilities: ["old.permission"],
            settings: { oldValue: "preserved only until adoption" }
        };
        const instances = {};
        instances[instanceId] = {
            componentId: widget.id,
            enabled: true,
            settings: {}
        };
        page.configSnapshot = {
            formatVersion: 1,
            revision: "7",
            components: components,
            instances: instances,
            layouts: {
                bars: {
                    main: {
                        outputs: { mode: "all" },
                        regions: {
                            start: [], center: [], end: [instanceId]
                        }
                    }
                },
                desktops: {}
            }
        };
        waitForRendering(page);
        wait(0);

        const adopt = findChild(
            page,
            "componentAdoptPackage-" + widget.id
        );
        const add = findChild(page, "componentAddToBar-" + widget.id);
        const toggle = findChild(page, "componentEnabled-" + widget.id);
        const configure = findChild(
            page,
            "componentSettings-" + widget.id
        );
        const status = findChild(page, "componentStatus-" + widget.id);
        verify(adopt !== null);
        verify(add !== null);
        verify(toggle !== null);
        verify(configure !== null);
        verify(status !== null);
        compare(adopt.visible, true);
        compare(adopt.enabled, true);
        compare(adopt.text, "Use Installed Version");
        compare(add.visible, false);
        compare(toggle.visible, false);
        compare(configure.visible, false);
        verify(status.text.includes("different package version"));

        let adoption = [];
        page.componentAdoptionRequested.connect(function(
            componentId,
            packageDigest,
            settings
        ) {
            adoption = [componentId, packageDigest, settings];
        });
        adopt.clicked();
        compare(adoption[0], widget.id);
        compare(adoption[1], widget.packageDigest);
        compare(Object.keys(adoption[2]).length, 0);

        const adopted = JSON.parse(JSON.stringify(page.configSnapshot));
        adopted.revision = "8";
        adopted.components[widget.id] = {
            packageDigest: widget.packageDigest,
            enabled: false,
            grantedCapabilities: [],
            settings: {}
        };
        page.configSnapshot = adopted;
        wait(0);
        compare(adopt.visible, false);
        compare(add.visible, false);
        compare(toggle.visible, true);
        compare(toggle.checked, false);
        compare(status.text, "Installed disabled. Review it before enabling.");
    }

    function test_quarantinedWidgetOffersDigestBoundRetry() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const widget = thirdPartyDeclarativeWidgetRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [widget];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        const instanceId = "d9a61b25-670b-44cb-a824-9e52772e79f1";
        const placed = configureSnapshotForComponent(widget, true);
        placed.instances[instanceId] = {
            componentId: widget.id,
            enabled: true,
            settings: {}
        };
        placed.layouts.bars.main = {
            outputs: { mode: "all" },
            regions: { start: [], center: [], end: [instanceId] }
        };
        page.configSnapshot = placed;
        page.runtimeAvailable = true;
        page.runtimeStates = [{
            componentId: widget.id,
            packageDigest: widget.packageDigest,
            state: "quarantined",
            reason: "timeout",
            failureCount: 1
        }];
        waitForRendering(page);
        wait(0);

        const retry = findChild(page, "componentRetry-" + widget.id);
        const add = findChild(page, "componentAddToBar-" + widget.id);
        const status = findChild(page, "componentStatus-" + widget.id);
        verify(retry !== null);
        verify(add !== null);
        verify(status !== null);
        compare(add.visible, false);
        compare(retry.visible, true);
        compare(retry.enabled, true);
        verify(status.text.includes("activation did not complete"));
        verify(status.text.includes("did not stabilize"));

        let request = [];
        page.componentRetryRequested.connect(function(
            componentId,
            packageDigest
        ) {
            request = [componentId, packageDigest];
        });
        retry.clicked();
        compare(request, [widget.id, widget.packageDigest]);
        page.runtimeRetryBusyComponentId = widget.id;
        compare(retry.enabled, false);
    }

    function test_thirdPartyRuntimeSafeModeWarnsAndLocksActivation() {
        const testWindow = createTemporaryObject(
            componentsPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const widget = thirdPartyDeclarativeWidgetRecord();
        page.managerAvailable = true;
        page.managerCatalogDigest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.components = [widget];
        page.configAvailable = true;
        page.configCatalogAvailable = true;
        page.configWritable = true;
        page.configCatalogDigest = page.managerCatalogDigest;
        page.configSnapshot = configureSnapshotForComponent(widget, true);
        page.runtimeAvailable = true;
        page.thirdPartySafeMode = true;
        waitForRendering(page);
        wait(0);

        const warning = findChild(page, "componentRuntimeSafeModeWarning");
        const add = findChild(page, "componentAddToBar-" + widget.id);
        const toggle = findChild(page, "componentEnabled-" + widget.id);
        const retry = findChild(page, "componentRetry-" + widget.id);
        const status = findChild(page, "componentStatus-" + widget.id);
        verify(warning !== null);
        verify(add !== null);
        verify(toggle !== null);
        verify(retry !== null);
        verify(status !== null);
        compare(warning.visible, true);
        compare(add.visible, true);
        compare(add.enabled, false);
        compare(toggle.visible, false);
        compare(retry.visible, false);
        verify(status.text.includes("runtime safe mode"));

        const instanceId = "d9a61b25-670b-44cb-a824-9e52772e79f1";
        const placed = configureSnapshotForComponent(widget, true);
        placed.instances[instanceId] = {
            componentId: widget.id,
            enabled: true,
            settings: {}
        };
        placed.layouts.bars.main = {
            outputs: { mode: "all" },
            regions: { start: [], center: [], end: [instanceId] }
        };
        page.configSnapshot = placed;
        page.runtimeStates = [{
            componentId: widget.id,
            packageDigest: widget.packageDigest,
            state: "quarantined",
            reason: "Recovery data could not be trusted.",
            failureCount: 1
        }];
        wait(0);
        compare(add.visible, false);
        compare(toggle.visible, true);
        compare(toggle.enabled, false);
        compare(retry.visible, true);
        compare(retry.enabled, false);
        verify(status.text.includes("runtime safe mode"));

        page.thirdPartySafeMode = false;
        compare(warning.visible, false);
        compare(toggle.enabled, true);
        compare(retry.enabled, true);
    }

    function test_requestsAreForwardedOnce() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableCoreSettings(page);

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

    function test_sharedBorderControlsUseOneAtomicRequestAndRealPreview() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableCoreSettings(page);

        const enabledControl = findChild(page, "shellBorderEnabled");
        const widthControl = findChild(page, "shellBorderWidth");
        const radiusControl = findChild(page, "shellBorderRadius");
        const syncControl = findChild(
            page,
            "syncHyprlandWindowBorders"
        );
        const authorityMessage = findChild(
            page,
            "sharedBorderAuthorityMessage"
        );
        const enabledDescription = findChild(
            page,
            "shellBorderEnabledDescription"
        );
        const reset = findChild(page, "resetSharedBorder");
        const previewBar = findChild(page, "previewBarVisual");
        verify(enabledControl !== null);
        verify(widthControl !== null);
        verify(radiusControl !== null);
        verify(syncControl !== null);
        verify(authorityMessage !== null);
        verify(enabledDescription !== null);
        verify(reset !== null);
        verify(previewBar !== null);

        compare(enabledControl.checked, true);
        compare(widthControl.from, 0);
        compare(widthControl.to, 20);
        compare(widthControl.value, 1);
        compare(radiusControl.from, 0);
        compare(radiusControl.to, 20);
        compare(radiusControl.value, 15);
        compare(syncControl.checked, true);
        compare(reset.enabled, false);
        compare(
            enabledControl.Accessible.name,
            "Show shared border on the bar and synchronized windows"
        );
        verify(enabledDescription.text.includes(
            "while synced"
        ));
        verify(enabledDescription.text.includes(
            "kept when hidden"
        ));
        verify(authorityMessage.text.includes("HyprShelld controls"));
        verify(authorityMessage.text.includes("read-only"));
        compare(previewBar.shellBorderEnabled, true);
        compare(previewBar.shellBorderWidth, 1);
        compare(previewBar.shellBorderRadius, 15);

        let requestCount = 0;
        let request = [];
        let resetCount = 0;
        page.sharedBorderRequested.connect(function(
            enabled,
            width,
            radius,
            sync
        ) {
            ++requestCount;
            request = [enabled, width, radius, sync];
        });
        page.resetSharedBorderRequested.connect(function() {
            ++resetCount;
        });

        page.requestSharedBorder(true, 7, 15, true);
        compare(requestCount, 1);
        compare(request, [true, 7, 15, true]);
        compare(previewBar.shellBorderWidth, 7);

        page.requestSharedBorder(true, 7, 11, true);
        compare(requestCount, 2);
        compare(request, [true, 7, 11, true]);
        compare(previewBar.shellBorderRadius, 11);

        page.requestSharedBorder(false, 7, 11, true);
        compare(requestCount, 3);
        compare(request, [false, 7, 11, true]);
        compare(previewBar.shellBorderEnabled, false);
        compare(previewBar.renderedBorderWidth, 0);

        page.requestSharedBorder(false, 7, 11, false);
        compare(requestCount, 4);
        compare(request, [false, 7, 11, false]);
        verify(authorityMessage.text.includes("own window border override"));
        compare(reset.enabled, true);

        reset.clicked();
        compare(resetCount, 1);
        compare(requestCount, 4);
        compare(previewBar.shellBorderEnabled, true);
        compare(previewBar.shellBorderWidth, 1);
        compare(previewBar.shellBorderRadius, 15);
        compare(syncControl.checked, true);
        compare(reset.enabled, false);
    }

    function test_sharedBorderControlsFollowCoreAvailability() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);

        const enabledControl = findChild(page, "shellBorderEnabled");
        const widthControl = findChild(page, "shellBorderWidth");
        const radiusControl = findChild(page, "shellBorderRadius");
        const syncControl = findChild(
            page,
            "syncHyprlandWindowBorders"
        );
        verify(enabledControl !== null);
        verify(widthControl !== null);
        verify(radiusControl !== null);
        verify(syncControl !== null);

        compare(enabledControl.enabled, false);
        compare(widthControl.enabled, false);
        compare(radiusControl.enabled, false);
        compare(syncControl.enabled, false);

        enableCoreSettings(page);
        compare(enabledControl.enabled, true);
        compare(widthControl.enabled, true);
        compare(radiusControl.enabled, true);
        compare(syncControl.enabled, true);

        page.coreBusy = true;
        compare(enabledControl.enabled, false);
        compare(widthControl.enabled, false);
        compare(radiusControl.enabled, false);
        compare(syncControl.enabled, false);
    }

    function test_busyAndErrorsAreWired() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableCoreSettings(page);
        enableWorkspaceSettings(page);

        const control = findChild(page, "barHeightControl");
        const workspaceSettings = findChild(
            page,
            "workspaceSwitcherSettingsCard"
        );
        const coreError = findChild(page, "coreConfigurationError");
        const coreErrorLabel = findChild(
            page,
            "coreConfigurationErrorLabel"
        );
        const componentError = findChild(
            page,
            "componentConfigurationError"
        );
        const componentErrorLabel = findChild(
            page,
            "componentConfigurationErrorLabel"
        );
        verify(control !== null);
        verify(workspaceSettings !== null);
        verify(coreError !== null);
        verify(coreErrorLabel !== null);
        verify(componentError !== null);
        verify(componentErrorLabel !== null);
        compare(control.busy, false);
        compare(control.errorText, "");
        compare(workspaceSettings.controlsEnabled, true);

        page.coreBusy = true;
        page.coreErrorText = "Could not save the bar size.";
        wait(0);
        compare(control.busy, true);
        compare(control.errorText, "");
        compare(workspaceSettings.controlsEnabled, true);
        compare(page.coreConfigurationErrorVisible, true);
        compare(page.componentConfigurationErrorVisible, false);
        verify(coreErrorLabel.text.includes(
            "Could not save the bar size."
        ));
        compare(
            coreErrorLabel.Accessible.role,
            Accessible.AlertMessage
        );

        page.coreBusy = false;
        page.componentBusy = true;
        page.componentErrorText = "Could not save workspace settings.";
        wait(0);
        compare(control.busy, false);
        compare(workspaceSettings.controlsEnabled, false);
        compare(page.coreConfigurationErrorVisible, true);
        compare(page.componentConfigurationErrorVisible, true);
        verify(componentErrorLabel.text.includes(
            "Could not save workspace settings."
        ));
        compare(
            componentErrorLabel.Accessible.role,
            Accessible.AlertMessage
        );
    }

    function test_workspaceControlsExposeCurrentSettings() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);

        const showIdentifiers = findChild(
            page,
            "workspaceShowIdentifiers"
        );
        const showNames = findChild(page, "workspaceShowNames");
        const showApplications = findChild(
            page,
            "workspaceShowApplications"
        );
        const maximumRow = findChild(
            page,
            "workspaceMaximumApplicationsRow"
        );
        const maximumApplications = findChild(
            page,
            "workspaceMaximumApplications"
        );
        const occupiedOnly = findChild(page, "workspaceOccupiedOnly");
        const scrollMode = findChild(page, "workspaceScrollMode");
        const reset = findChild(page, "resetWorkspaceSwitcher");
        verify(showIdentifiers !== null);
        verify(showNames !== null);
        verify(showApplications !== null);
        verify(maximumRow !== null);
        verify(maximumApplications !== null);
        verify(occupiedOnly !== null);
        verify(scrollMode !== null);
        verify(reset !== null);

        compare(showIdentifiers.checked, true);
        compare(showNames.checked, false);
        compare(
            showIdentifiers.Accessible.name,
            "Show workspace identifiers"
        );
        compare(showNames.Accessible.name, "Show workspace names");
        verify(showIdentifiers.mapToItem(page, 0, 0).y
            < showNames.mapToItem(page, 0, 0).y);
        verify(showNames.mapToItem(page, 0, 0).y
            < showApplications.mapToItem(page, 0, 0).y);
        compare(showApplications.checked, false);
        compare(maximumRow.visible, false);
        compare(maximumApplications.from, 1);
        compare(maximumApplications.to, 5);
        compare(maximumApplications.value, 3);
        compare(occupiedOnly.checked, false);
        compare(scrollMode.currentText, "Off");
        compare(reset.enabled, false);

        enableWorkspaceSettings(page);
        page.workspaceShowIdentifiers = false;
        page.workspaceShowNames = true;
        page.workspaceShowApplications = true;
        page.workspaceMaximumApplications = 5;
        page.workspaceOccupiedOnly = true;
        page.workspaceScrollMode = "reversed";
        const scrollView = findChild(page, "barOptionsScrollView");
        verify(scrollView !== null);
        scrollView.contentItem.contentY = scrollView.contentItem.contentHeight
            - scrollView.contentItem.height;
        wait(0);
        compare(showIdentifiers.checked, false);
        compare(showNames.checked, true);
        compare(showApplications.checked, true);
        compare(maximumApplications.value, 5);
        compare(occupiedOnly.checked, true);
        compare(scrollMode.currentText, "Reversed");
        compare(reset.enabled, true);

        page.componentBusy = true;
        compare(showIdentifiers.enabled, false);
        compare(showNames.enabled, false);
        compare(showApplications.enabled, false);
        compare(maximumApplications.enabled, false);
        compare(occupiedOnly.enabled, false);
        compare(scrollMode.enabled, false);
        compare(reset.enabled, false);
    }

    function test_disabledWorkspaceGreysNaturalSettingsAndOmitsPreview() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableCoreSettings(page);
        enableWorkspaceSettings(page);

        const message = findChild(page, "workspaceFeatureDisabledMessage");
        const messageLabel = findChild(
            page,
            "workspaceFeatureDisabledMessageLabel"
        );
        const controls = findChild(page, "workspaceSettingsControls");
        const showIdentifiers = findChild(
            page,
            "workspaceShowIdentifiers"
        );
        const showNames = findChild(page, "workspaceShowNames");
        const reset = findChild(page, "resetWorkspaceSwitcher");
        const heightControl = findChild(page, "barHeightControl");
        verify(message !== null);
        verify(messageLabel !== null);
        verify(controls !== null);
        verify(showIdentifiers !== null);
        verify(showNames !== null);
        verify(reset !== null);
        verify(heightControl !== null);

        page.workspaceFeatureAvailable = true;
        page.workspaceFeatureEnabled = false;
        page.workspacePreviewEnabled = false;
        wait(0);
        const settingsCard = findChild(
            page,
            "workspaceSwitcherSettingsCard"
        );
        verify(settingsCard !== null);
        compare(page.workspaceFeatureAvailable, true);
        compare(page.workspaceFeatureEnabled, false);
        compare(settingsCard.featureAvailable, true);
        compare(settingsCard.featureEnabled, false);
        compare(settingsCard.disabledMessageVisible, true);
        compare(
            messageLabel.text,
            "This feature has been disabled. Enable it from Components → Bar Widgets to change these settings."
        );
        compare(messageLabel.opacity, 1);
        compare(controls.opacity, 0.42);
        compare(showIdentifiers.enabled, false);
        compare(showNames.enabled, false);
        compare(reset.enabled, false);
        compare(heightControl.enabled, true);
        compare(findChild(page, "workspaceSwitcher"), null);

        page.workspaceShowIdentifiers = false;
        page.workspaceShowNames = true;
        page.workspaceShowApplications = true;
        page.workspaceMaximumApplications = 5;
        page.workspaceOccupiedOnly = true;
        page.workspaceScrollMode = "reversed";
        page.workspaceFeatureEnabled = true;
        page.workspacePreviewEnabled = true;
        wait(0);
        const switcher = findChild(page, "workspaceSwitcher");
        verify(switcher !== null);
        compare(settingsCard.disabledMessageVisible, false);
        compare(controls.opacity, 1);
        compare(showIdentifiers.checked, false);
        compare(showNames.checked, true);
        compare(switcher.showIdentifiers, false);
        compare(switcher.showNames, true);
        compare(switcher.showApplications, true);
        compare(switcher.maximumApplications, 5);
        compare(switcher.scrollMode, "reversed");
    }

    function test_unavailableWorkspaceIsNotPresentedAsDisabled() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableCoreSettings(page);
        enableWorkspaceSettings(page);

        page.workspaceFeatureAvailable = false;
        page.workspaceFeatureEnabled = true;
        page.workspacePreviewEnabled = true;
        wait(0);
        const message = findChild(page, "workspaceFeatureDisabledMessage");
        const switcher = findChild(page, "workspaceSwitcher");
        verify(message !== null);
        verify(switcher !== null);
        compare(message.visible, false);
        compare(page.componentServiceWarningVisible, true);
        verify(page.componentWarningMessage.includes("placement"));
        compare(page.workspaceControlsEnabled, false);
        compare(switcher.showIdentifiers, true);
        compare(switcher.showNames, false);
    }

    function test_workspaceAuthorityDistinguishesGlobalAndInstanceState() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const definition = workspaceCatalogRecord();
        const digest =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        const instanceId = application.workspaceInstanceId;
        const instances = {};
        instances[instanceId] = {
            componentId: definition.id,
            enabled: true,
            settings: {
                showIdentifiers: true,
                showNames: false,
                showApplications: false,
                maximumApplications: 3,
                occupiedOnly: false,
                scrollMode: "disabled"
            }
        };
        const snapshot = configureSnapshotForComponent(definition, true);
        snapshot.instances = instances;

        let state = application.workspaceComponentStateFromServices(
            true,
            digest,
            [definition],
            true,
            true,
            digest,
            snapshot
        );
        compare(state.available, true);
        compare(state.desiredEnabled, true);
        compare(state.instanceEnabled, true);
        compare(state.previewEnabled, true);

        state = application.workspaceComponentStateFromServices(
            false,
            digest,
            [definition],
            true,
            true,
            digest,
            snapshot
        );
        compare(state.available, false);
        compare(state.desiredEnabled, true);
        compare(state.instanceEnabled, true);
        compare(state.previewEnabled, true);

        snapshot.components[definition.id].enabled = false;
        snapshot.instances[instanceId].enabled = false;
        state = application.workspaceComponentStateFromServices(
            true, digest, [definition], true, true, digest, snapshot
        );
        compare(state.available, true);
        compare(state.desiredEnabled, false);
        compare(state.instanceEnabled, false);
        compare(state.previewEnabled, false);
        compare(application.workspaceNaturalSettingsAvailable(state), true);

        snapshot.components[definition.id].enabled = true;
        state = application.workspaceComponentStateFromServices(
            true, digest, [definition], true, true, digest, snapshot
        );
        compare(state.available, true);
        compare(state.desiredEnabled, true);
        compare(state.instanceEnabled, false);
        compare(state.previewEnabled, false);
        compare(application.workspaceNaturalSettingsAvailable(state), false);

        state = application.workspaceComponentStateFromServices(
            true,
            digest,
            [definition],
            true,
            true,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            snapshot
        );
        compare(state.available, false);
        compare(state.previewEnabled, false);
    }

    function test_workspaceConditionalRowIsVisible() {
        const testWindow = createTemporaryObject(
            workspaceSettingsComponent,
            this
        );
        verify(testWindow !== null);
        const settings = testWindow.settings;
        verify(settings !== null);
        const maximumRow = findChild(
            settings,
            "workspaceMaximumApplicationsRow"
        );
        verify(maximumRow !== null);
        compare(maximumRow.visible, false);

        settings.showApplications = true;
        waitForRendering(settings);
        compare(maximumRow.visible, true);

        settings.featureEnabled = false;
        waitForRendering(settings);
        const disabledMessage = findChild(
            settings,
            "workspaceFeatureDisabledMessage"
        );
        verify(disabledMessage !== null);
        compare(settings.disabledMessageVisible, true);
        compare(disabledMessage.visible, true);
    }

    function test_workspaceRequestsCarryOneAtomicSnapshot() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableWorkspaceSettings(page);

        const settings = findChild(
            page,
            "workspaceSwitcherSettingsCard"
        );
        const reset = findChild(page, "resetWorkspaceSwitcher");
        verify(settings !== null);
        verify(reset !== null);

        let requestCount = 0;
        let request = [];
        let resetCount = 0;
        page.workspaceSwitcherRequested.connect(function(
            showIdentifiers,
            showNames,
            showApplications,
            maximumApplications,
            occupiedOnly,
            scrollMode
        ) {
            ++requestCount;
            request = [
                showIdentifiers,
                showNames,
                showApplications,
                maximumApplications,
                occupiedOnly,
                scrollMode
            ];
        });
        page.resetWorkspaceSwitcherRequested.connect(function() {
            ++resetCount;
        });

        settings.requestSnapshot(
            true, false, false, 3, false, "disabled"
        );
        compare(requestCount, 0);

        settings.requestSnapshot(
            false, false, false, 5, false, "disabled"
        );
        compare(requestCount, 1);
        compare(request, [
            false, false, false, 5, false, "disabled"
        ]);

        page.workspaceShowIdentifiers = false;
        page.workspaceMaximumApplications = 5;
        page.workspaceOccupiedOnly = true;
        settings.requestSnapshot(
            false, true, false, 5, true, "reversed"
        );
        compare(requestCount, 2);
        compare(request, [
            false, true, false, 5, true, "reversed"
        ]);

        page.componentBusy = true;
        settings.requestSnapshot(
            true, true, false, 5, true, "reversed"
        );
        compare(requestCount, 2);

        page.componentBusy = false;
        reset.clicked();
        compare(resetCount, 1);
    }

    function test_workspaceControlsReconcileAfterRejectedAsyncSave() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        enableWorkspaceSettings(page);

        const showIdentifiers = findChild(
            page,
            "workspaceShowIdentifiers"
        );
        const maximumApplications = findChild(
            page,
            "workspaceMaximumApplications"
        );
        const scrollMode = findChild(page, "workspaceScrollMode");
        verify(showIdentifiers !== null);
        verify(maximumApplications !== null);
        verify(scrollMode !== null);

        let requestCount = 0;
        page.workspaceSwitcherRequested.connect(function() {
            ++requestCount;
            page.componentBusy = true;
        });

        showIdentifiers.checked = false;
        showIdentifiers.toggled();
        compare(requestCount, 1);
        compare(page.componentBusy, true);
        compare(showIdentifiers.checked, false);

        page.componentErrorText = "The change could not be saved.";
        page.componentBusy = false;
        wait(0);
        compare(showIdentifiers.checked, true);

        page.componentErrorText = "";
        page.workspaceShowApplications = true;
        wait(0);
        maximumApplications.value = 5;
        maximumApplications.valueModified();
        compare(requestCount, 2);
        compare(page.componentBusy, true);
        compare(maximumApplications.value, 5);

        page.componentErrorText = "The change could not be saved.";
        page.componentBusy = false;
        wait(0);
        compare(maximumApplications.value, 3);

        page.componentErrorText = "";
        scrollMode.currentIndex = 2;
        scrollMode.activated(2);
        compare(requestCount, 3);
        compare(page.componentBusy, true);
        compare(scrollMode.currentIndex, 2);

        page.componentErrorText = "The change could not be saved.";
        page.componentBusy = false;
        wait(0);
        compare(scrollMode.currentIndex, 0);
        compare(scrollMode.currentText, "Off");
    }

    function test_workspaceSnapshotReplacementIsWholeAndAtomic() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);

        const workspaceId = application.workspaceInstanceId;
        const workspaceComponentId = application.workspaceComponentId;
        const dormantId = "c89b6683-33a8-4d63-a573-b89b99fd0dd0";
        const instances = {};
        instances[workspaceId] = {
            componentId: workspaceComponentId,
            enabled: true,
            settings: {
                showIdentifiers: true,
                showNames: false,
                showApplications: false,
                maximumApplications: 3,
                occupiedOnly: false,
                scrollMode: "disabled"
            }
        };
        instances[dormantId] = {
            componentId: "org.example.dormant-widget",
            enabled: false,
            settings: { preserved: "exactly" }
        };
        const snapshot = {
            formatVersion: 1,
            revision: "42",
            components: {
                "org.example.dormant-widget": {
                    packageDigest:
                        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                    enabled: false,
                    grantedCapabilities: ["example.read"],
                    settings: { retained: true }
                }
            },
            instances: instances,
            layouts: {
                bars: {
                    main: {
                        outputs: { mode: "all" },
                        regions: {
                            start: [workspaceId],
                            center: [],
                            end: [dormantId]
                        }
                    }
                },
                desktops: {}
            }
        };

        const extracted = application.workspaceSettingsFromSnapshot(
            snapshot
        );
        compare(extracted.valid, true);
        compare(extracted.showIdentifiers, true);
        compare(extracted.showNames, false);

        const replacement = application.workspaceSnapshotWithSettings(
            snapshot,
            false,
            true,
            true,
            5,
            true,
            "reversed"
        );
        verify(replacement !== null);
        verify(replacement !== snapshot);
        compare(
            snapshot.instances[workspaceId].settings.showIdentifiers,
            true
        );
        compare(snapshot.instances[workspaceId].settings.showNames, false);
        compare(replacement.instances[workspaceId].settings, {
            showIdentifiers: false,
            showNames: true,
            showApplications: true,
            maximumApplications: 5,
            occupiedOnly: true,
            scrollMode: "reversed"
        });
        compare(
            replacement.instances[dormantId],
            snapshot.instances[dormantId]
        );
        compare(replacement.components, snapshot.components);
        compare(replacement.layouts, snapshot.layouts);
        compare(replacement.revision, "42");

        const reset = application.workspaceSnapshotWithSettings(
            replacement,
            true,
            false,
            false,
            3,
            false,
            "disabled"
        );
        compare(
            reset.instances[workspaceId].settings,
            application.workspaceDefaults
        );

        compare(application.workspaceSnapshotWithSettings(
            snapshot,
            true,
            false,
            false,
            6,
            false,
            "disabled"
        ), null);
        snapshot.instances[workspaceId].componentId =
            "org.example.wrong-component";
        compare(
            application.workspaceSettingsFromSnapshot(snapshot).valid,
            false
        );
        compare(application.workspaceSnapshotWithSettings(
            snapshot,
            true,
            false,
            false,
            3,
            false,
            "disabled"
        ), null);
    }

    function test_previewDemonstratesWorkspaceSettings() {
        const page = createTemporaryObject(pageComponent, this);
        verify(page !== null);
        const preview = findChild(page, "barPreview");
        const bar = findChild(page, "previewBarVisual");
        const startSlot = findChild(page, "barStartComponentSlot");
        const switcher = findChild(page, "workspaceSwitcher");
        verify(preview !== null);
        verify(bar !== null);
        verify(startSlot !== null);
        verify(switcher !== null);

        compare(bar.currentTime.getFullYear(), 1991);
        compare(bar.currentTime.getMonth(), 8);
        compare(bar.currentTime.getDate(), 17);
        compare(bar.currentTime.getDay(), 2);
        compare(bar.currentTime.getHours(), 15);
        compare(bar.currentTime.getMinutes(), 42);

        compare(preview.previewWorkspaces.length, 3);
        compare(switcher.showIdentifiers, true);
        compare(switcher.showNames, false);
        compare(Boolean(preview.previewWorkspaces[0].placeholder), false);
        compare(preview.previewWorkspaces[0].occupied, false);
        compare(preview.previewWorkspaces[0].active, false);
        compare(preview.previewWorkspaces[1].active, true);
        compare(preview.previewWorkspaces[1].occupied, true);
        compare(preview.previewWorkspaces[1].applications.length, 5);
        compare(preview.previewWorkspaces[1].applications[0].iconSource, "");
        compare(preview.previewWorkspaces[1].applications[0].fallbackInitial, "E");
        compare(preview.previewWorkspaces[1].applications[0].active, true);
        compare(preview.previewWorkspaces[1].applications[0].count, 1);
        compare(preview.previewWorkspaces[1].applications[0].activatable, false);
        verify(preview.previewWorkspaces[1].applications[1].iconSource.length > 0);
        compare(preview.previewWorkspaces[2].urgent, true);
        compare(preview.previewWorkspaces[2].occupied, true);
        compare(preview.previewWorkspaces[2].applications.length, 1);
        compare(preview.previewWorkspaces[2].applications[0].active, false);
        compare(preview.previewWorkspaces[2].applications[0].count, 3);
        compare(preview.previewWorkspaces[2].applications[0].activatable, false);
        compare(findChild(page, "workspacePlaceholder-0"), null);

        const numericIndicator = findChild(page, "workspaceIndicator-1");
        const namedIndicator = findChild(page, "workspaceIndicator-2");
        verify(numericIndicator !== null);
        verify(namedIndicator !== null);
        compare(numericIndicator.circleIdentifier, "1");
        compare(numericIndicator.nameLabel, "");
        compare(namedIndicator.circleIdentifier, "2");
        compare(namedIndicator.nameLabel, "");

        page.workspaceShowIdentifiers = false;
        wait(0);
        compare(namedIndicator.circleIdentifier, "");
        compare(namedIndicator.nameLabel, "");

        page.workspaceShowIdentifiers = true;
        page.workspaceShowNames = true;
        wait(0);
        compare(numericIndicator.circleIdentifier, "1");
        compare(numericIndicator.nameLabel, "");
        compare(namedIndicator.circleIdentifier, "2");
        compare(namedIndicator.nameLabel, "writing");

        page.workspaceShowIdentifiers = false;
        wait(0);
        compare(numericIndicator.circleIdentifier, "");
        compare(numericIndicator.nameLabel, "");
        compare(namedIndicator.circleIdentifier, "");
        compare(namedIndicator.nameLabel, "writing");

        page.workspaceShowApplications = true;
        page.workspaceMaximumApplications = 1;
        wait(0);
        const currentIndicator = findChild(page, "workspaceIndicator-2");
        verify(currentIndicator !== null);
        compare(currentIndicator.visibleApplications.length, 1);
        compare(currentIndicator.visibleApplications[0].active, true);
        compare(currentIndicator.applicationOverflow, 4);

        page.workspaceMaximumApplications = 3;
        page.workspaceScrollMode = "normal";
        compare(switcher.showIdentifiers, false);
        compare(switcher.showNames, true);
        compare(switcher.showApplications, true);
        compare(switcher.maximumApplications, 3);
        compare(switcher.scrollMode, "normal");

        page.workspaceOccupiedOnly = true;
        compare(preview.previewWorkspaces.length, 2);
        compare(preview.previewWorkspaces[0].active, true);
        compare(preview.previewWorkspaces[1].occupied, true);

        page.workspaceOccupiedOnly = false;
        compare(preview.previewWorkspaces.length, 3);
        compare(preview.previewWorkspaces[0].workspaceId, 1);
        compare(preview.previewWorkspaces[2].workspaceId, 4);
    }

    function test_barPreviewStaysStickyWhileOptionsScroll() {
        const page = createTemporaryObject(pageComponent, this, {
            width: 820,
            height: 900
        });
        verify(page !== null);
        enableCoreSettings(page);
        enableWorkspaceSettings(page);
        waitForRendering(page);
        wait(0);

        const sticky = findChild(page, "barStickyPreview");
        const scroll = findChild(page, "barOptionsScrollView");
        const content = findChild(page, "barOptionsContent");
        const preview = findChild(page, "barPreview");
        const heightSlider = findChild(page, "barHeightSlider");
        verify(sticky !== null);
        verify(scroll !== null);
        verify(content !== null);
        verify(preview !== null);
        verify(heightSlider !== null);
        compare(page.compactPreview, false);
        compare(preview.height, 286);
        compare(preview.implicitHeight, 286);
        compare(preview.scale, 1);
        verify(Math.abs(preview.width - sticky.width) <= 0.01);
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width <= scroll.contentWidth + 0.01);

        const maximumContentY = scroll.contentItem.contentHeight
            - scroll.contentItem.height;
        verify(maximumContentY > 0);
        const stickyBefore = sticky.mapToItem(page, 0, 0);
        const previewBefore = preview.mapToItem(page, 0, 0);
        const contentBefore = content.mapToItem(page, 0, 0);
        const targetContentY = Math.min(180, maximumContentY);
        verify(targetContentY > 0);

        scroll.contentItem.contentY = targetContentY;
        tryCompare(scroll.contentItem, "contentY", targetContentY);
        const stickyAfter = sticky.mapToItem(page, 0, 0);
        const previewAfter = preview.mapToItem(page, 0, 0);
        const contentAfter = content.mapToItem(page, 0, 0);
        verify(Math.abs(stickyAfter.x - stickyBefore.x) <= 0.01);
        verify(Math.abs(stickyAfter.y - stickyBefore.y) <= 0.01);
        verify(Math.abs(previewAfter.x - previewBefore.x) <= 0.01);
        verify(Math.abs(previewAfter.y - previewBefore.y) <= 0.01);
        verify(contentAfter.y < contentBefore.y);

        heightSlider.value = 64;
        wait(0);
        compare(preview.barHeight, 64);
    }

    function test_minimumSizeCanReachWorkspaceReset() {
        // Main's 620-pixel minimum width leaves 423 pixels for this page
        // after the fixed sidebar and separator.
        const page = createTemporaryObject(pageComponent, this, {
            width: 423,
            height: 480
        });
        verify(page !== null);
        const sticky = findChild(page, "barStickyPreview");
        const scrollView = findChild(page, "barOptionsScrollView");
        const content = findChild(page, "barOptionsContent");
        const preview = findChild(page, "barPreview");
        const borderCard = findChild(page, "sharedBorderSettingsCard");
        const borderEnabled = findChild(page, "shellBorderEnabled");
        const borderWidth = findChild(page, "shellBorderWidth");
        const borderRadius = findChild(page, "shellBorderRadius");
        const borderSync = findChild(page, "syncHyprlandWindowBorders");
        const borderReset = findChild(page, "resetSharedBorder");
        const reset = findChild(page, "resetWorkspaceSwitcher");
        verify(sticky !== null);
        verify(scrollView !== null);
        verify(content !== null);
        verify(preview !== null);
        verify(borderCard !== null);
        verify(borderEnabled !== null);
        verify(borderWidth !== null);
        verify(borderRadius !== null);
        verify(borderSync !== null);
        verify(borderReset !== null);
        verify(reset !== null);
        waitForRendering(page);
        wait(0);
        compare(page.compactPreview, true);
        compare(preview.height, 286);
        compare(preview.implicitHeight, 286);
        compare(preview.scale, 1);
        verify(Math.abs(preview.width - sticky.width) <= 0.01);
        verify(scrollView.height >= 100);
        verify(scrollView.contentItem.contentHeight > scrollView.height);
        verify(scrollView.contentWidth
            <= scrollView.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width
            <= scrollView.contentWidth + 0.01);
        for (const control of [
            borderEnabled,
            borderWidth,
            borderRadius,
            borderSync,
            borderReset
        ]) {
            verify(control.height >= 44);
            const position = control.mapToItem(page, 0, 0);
            verify(position.x >= 0);
            verify(position.x + control.width <= page.width + 0.01);
        }
        const borderCardPosition = borderCard.mapToItem(content, 0, 0);
        verify(borderCardPosition.x >= 0);
        verify(borderCardPosition.x + borderCard.width
            <= content.width + 0.01);

        const stickyBefore = sticky.mapToItem(page, 0, 0);
        const previewBefore = preview.mapToItem(page, 0, 0);
        const contentBefore = content.mapToItem(page, 0, 0);
        verify(stickyBefore.x >= 0);
        verify(stickyBefore.x + sticky.width <= page.width + 0.01);
        verify(stickyBefore.y >= 0);
        verify(stickyBefore.y + sticky.height <= page.height + 0.01);

        const maximumContentY = Math.max(
            0,
            scrollView.contentItem.contentHeight
                - scrollView.contentItem.height
        );
        verify(maximumContentY > 0);
        scrollView.contentItem.contentY = maximumContentY;
        tryCompare(scrollView.contentItem, "contentY", maximumContentY);
        const stickyAfter = sticky.mapToItem(page, 0, 0);
        const previewAfter = preview.mapToItem(page, 0, 0);
        const contentAfter = content.mapToItem(page, 0, 0);
        verify(Math.abs(stickyAfter.x - stickyBefore.x) <= 0.01);
        verify(Math.abs(stickyAfter.y - stickyBefore.y) <= 0.01);
        verify(Math.abs(previewAfter.x - previewBefore.x) <= 0.01);
        verify(Math.abs(previewAfter.y - previewBefore.y) <= 0.01);
        verify(contentAfter.y < contentBefore.y);
        const resetPosition = reset.mapToItem(page, 0, 0);
        verify(resetPosition.x >= 0);
        verify(resetPosition.x + reset.width <= page.width);
        verify(resetPosition.y >= 0);
        verify(resetPosition.y + reset.height <= page.height);
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
        const switcher = findChild(page, "workspaceSwitcher");
        const firstWorkspace = findChild(page, "workspaceIndicator-1");
        const activeWorkspace = findChild(page, "workspaceIndicator-2");
        const urgentWorkspace = findChild(page, "workspaceIndicator-4");
        verify(preview !== null);
        verify(frame !== null);
        verify(bar !== null);
        verify(reservedLabel !== null);
        verify(switcher !== null);
        verify(firstWorkspace !== null);
        verify(activeWorkspace !== null);
        verify(urgentWorkspace !== null);
        verify(preview.previewScale < 1);
        verify(frame.width * frame.scale <= preview.width);
        verify(reservedLabel.y >= frame.y + bar.height * frame.scale);
        compare(switcher.interactive, false);
        compare(firstWorkspace.workspaceActive, false);
        compare(activeWorkspace.workspaceActive, true);
        compare(urgentWorkspace.workspaceUrgent, true);
        compare(findChild(page, "workspaceIndicator-3"), null);
    }

    function test_displayDraftRequiresAChangeAndSafeMirrorGraph() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const outputs = [
            displayRecord("display-a", "DP-1", true, "", -1),
            displayRecord("display-b", "DP-2", false, "", -1),
            displayRecord("display-c", "DP-3", true, "DP-1", -1),
            displayRecord("display-d", "DP-4", true, "", -1)
        ];
        const topology = [
            connectedDisplay("DP-1", true, 0),
            connectedDisplay("DP-2", false, 1920),
            connectedDisplay("DP-3", true, 3840),
            connectedDisplay("DP-4", true, 5760)
        ];
        configureDisplaysPage(page, outputs, topology);
        waitForRendering(page);
        wait(0);

        compare(page.draftDirty, false);
        compare(page.previewEnabled, false);

        const card = findChild(page, "displaySettingsCard");
        verify(card !== null);
        compare(card.availableMirrors.length, 2);
        compare(card.availableMirrors[0].value, "");
        compare(card.availableMirrors[1].value, "DP-4");

        let changed = page.clone(page.outputById("display-a"));
        changed.scale = 1.25;
        page.replaceOutput(changed);
        compare(page.draftDirty, true);
        compare(page.draftValidationMessage, "");
        compare(page.previewEnabled, true);

        changed = page.clone(page.outputById("display-a"));
        changed.scale = 1;
        page.replaceOutput(changed);
        compare(page.draftDirty, false);
        compare(page.previewEnabled, false);

        changed = page.clone(page.outputById("display-a"));
        changed.mirror = "DP-2";
        page.replaceOutput(changed);
        verify(page.draftValidationMessage.includes("disabled"));
        compare(page.previewEnabled, false);

        changed = page.clone(page.outputById("display-a"));
        changed.mirror = "DP-1";
        page.replaceOutput(changed);
        verify(page.draftValidationMessage.includes("itself"));
        compare(page.previewEnabled, false);

        changed = page.clone(page.outputById("display-a"));
        changed.mirror = "DP-4";
        page.replaceOutput(changed);
        verify(page.draftValidationMessage.includes("chains"));
        compare(page.previewEnabled, false);

        let directTarget = page.clone(page.outputById("display-d"));
        directTarget.mirror = "DP-1";
        page.replaceOutput(directTarget);
        verify(page.draftValidationMessage.includes("not a mirror"));
        compare(page.previewEnabled, false);
    }

    function test_displayMirrorTracksTargetPosition() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const primary = displayRecord("display-a", "DP-1", true, "", -1);
        const secondary = displayRecord("display-b", "DP-2", true, "", -1);
        primary.position = "0x0";
        secondary.position = "1920x0";
        configureDisplaysPage(
            page,
            [primary, secondary],
            [
                connectedDisplay("DP-1", true, 0),
                connectedDisplay("DP-2", true, 1920)
            ]
        );
        waitForRendering(page);
        wait(0);

        let mirrored = page.clone(page.outputById("display-b"));
        mirrored.mirror = "DP-1";
        page.replaceOutput(mirrored);
        compare(page.outputById("display-b").position, "0x0");
        compare(page.draftValidationMessage, "");

        let movedTarget = page.clone(page.outputById("display-a"));
        movedTarget.position = "320x180";
        page.replaceOutput(movedTarget);
        compare(page.outputById("display-a").position, "320x180");
        compare(page.outputById("display-b").position, "320x180");
        compare(page.draftValidationMessage, "");
        compare(page.previewEnabled, true);
    }

    function test_displayAdvancedValuesAndNewConnectorSeeds() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", 3)],
            [
                connectedDisplay("DP-1", true, 0),
                connectedDisplay(
                    "DP-2",
                    true,
                    1920,
                    "XRGB2101010",
                    0.2
                ),
                connectedDisplay("DP-3", true, 3840, "XRGB8888", -20)
            ]
        );
        waitForRendering(page);
        wait(0);

        const vrr = findChild(page, "displayVrrComboBox");
        verify(vrr !== null);
        compare(vrr.currentValue, 3);

        const tenBit = page.outputById("display-DP-2");
        verify(tenBit !== null);
        compare(tenBit.bitdepth, 10);
        compare(tenBit.cm, "srgb");
        compare(tenBit.sdrMinLuminance, 0.2);

        const invalidMinimum = page.outputById("display-DP-3");
        verify(invalidMinimum !== null);
        compare(invalidMinimum.bitdepth, 8);
        compare(invalidMinimum.sdrMinLuminance, 0.2);
    }

    function test_displayDraftUsesTheWinningExactConnectorRule() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const first = displayRecord(
            "older-display-rule",
            "DP-1",
            true,
            "",
            0
        );
        const winning = displayRecord(
            "winning-display-rule",
            "DP-1",
            true,
            "",
            3
        );
        winning.icc = "/profiles/winning.icc";
        configureDisplaysPage(
            page,
            [first, winning],
            [connectedDisplay("DP-1", true, 0)]
        );
        waitForRendering(page);
        wait(0);

        compare(page.draftOutputs.length, 1);
        compare(page.draftOutputs[0].id, "winning-display-rule");
        compare(page.draftOutputs[0].vrr, 3);
        compare(page.draftOutputs[0].icc, "/profiles/winning.icc");
    }

    function test_displayHotplugRefreshesAndInvalidatesTheDraft() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        waitForRendering(page);
        wait(0);

        const changed = page.clone(page.outputById("display-a"));
        changed.scale = 1.25;
        page.replaceOutput(changed);
        compare(page.draftDirty, true);
        compare(page.previewEnabled, true);

        page.connectedDisplays = [
            connectedDisplay("DP-1", true, 0),
            connectedDisplay("DP-2", true, 1920)
        ];
        page.topologyDigest =
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        wait(0);

        compare(page.draftDirty, false);
        compare(page.previewEnabled, false);
        compare(page.inventoryChangedWhileEditing, true);
        compare(page.draftOutputs.length, 2);
        compare(page.synchronizedTopologyDigest, page.topologyDigest);
    }

    function test_displaySavedRulesArePreservedWithoutOfflineClaim() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const descriptionRule = displayRecord(
            "saved-projector",
            "desc:Example Projector",
            true,
            "",
            -1
        );
        configureDisplaysPage(
            page,
            [
                displayRecord("display-a", "DP-1", true, "", -1),
                descriptionRule
            ],
            [connectedDisplay("DP-1", true, 0)]
        );
        waitForRendering(page);
        wait(0);

        compare(page.offlineRecords.length, 1);
        compare(page.offlineRecords[0].selector, "desc:Example Projector");
        const label = findChild(page, "savedDisplayRulesLabel");
        verify(label !== null);
        compare(label.visible, true);
        verify(String(label.text).includes("saved display rule"));
        verify(!/offline/i.test(String(label.text)));
    }

    function test_displayOwnerLossProjectionUnlocksUnavailablePage() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        page.managementState = "preview";
        page.confirmationState = "awaiting-confirmation";
        page.confirmationRevision = 8;
        // DeadlineMs is an informational UTC-epoch projection. The daemon's
        // monotonic timer, rather than this countdown, owns automatic revert.
        page.countdownNowMs = 100000;
        page.confirmationDeadlineMs = 115000;
        page.confirmationGeneration =
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        page.confirmationOwned = true;
        waitForRendering(page);
        wait(0);

        const overlay = findChild(page, "displayConfirmationOverlay");
        const keep = findChild(page, "confirmDisplayConfigurationButton");
        const title = findChild(page, "displayConfirmationTitle");
        const message = findChild(page, "displayConfirmationMessage");
        verify(overlay !== null);
        verify(keep !== null);
        verify(title !== null);
        verify(message !== null);
        compare(overlay.visible, true);
        compare(keep.visible, true);
        compare(keep.enabled, true);
        compare(page.confirmationSecondsRemaining, 15);

        // The UTC projection drives presentation only. A wall-clock jump must
        // not locally decide whether the daemon still accepts confirmation.
        page.countdownNowMs = 120000;
        compare(page.confirmationSecondsRemaining, 0);
        compare(keep.enabled, true);

        page.confirmationOwned = false;
        wait(0);
        compare(keep.visible, false);

        page.confirmationState = "failed";
        wait(0);
        compare(title.text, "Display recovery needs attention");
        verify(String(message.text).includes(
            "If the desktop health warning reports Compositor settings"
        ));
        verify(String(message.text).includes(
            "preserve the current state and seek recovery guidance"
        ));
        compare(message.Accessible.role, Accessible.AlertMessage);

        page.serviceAvailable = false;
        page.errorMessage = "Injected private recovery failure";
        wait(0);
        compare(title.text, "Display confirmation is unavailable");

        // This is the projection CompositorClient publishes when its service
        // owner disappears; stale global preview state must not pin the page.
        page.managementState = "unmanaged";
        page.confirmationState = "idle";
        page.confirmationRevision = 0;
        page.confirmationDeadlineMs = 0;
        page.confirmationGeneration = "";
        wait(0);
        compare(overlay.visible, false);
        compare(page.confirmationRevision, 0);
        compare(page.confirmationDeadlineMs, 0);
        compare(page.confirmationGeneration, "");
    }

    function test_displayAdoptionRequiresExplicitConfirmation() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        page.managementState = "unmanaged";
        page.applyState = "unavailable";
        page.errorName = "org.hyprshelld.Error.OperationFailed";
        page.errorMessage = "The previous takeover attempt was rejected.";
        waitForRendering(page);
        wait(0);

        const takeControl = findChild(page, "adoptCompositorButton");
        const status = findChild(page, "displayStatusMessage");
        const dialog = findChild(page, "displayAdoptionDialog");
        const explanation = findChild(
            dialog,
            "displayAdoptionExplanation"
        );
        const cancel = findChild(
            dialog,
            "cancelDisplayAdoptionButton"
        );
        const confirm = findChild(
            dialog,
            "confirmDisplayAdoptionButton"
        );
        verify(takeControl !== null);
        verify(status !== null);
        verify(dialog !== null);
        verify(explanation !== null);
        verify(cancel !== null);
        verify(confirm !== null);
        compare(page.serviceAvailable, true);
        compare(page.writable, true);
        compare(page.busy, false);
        compare(page.confirmationState, "idle");
        compare(dialog.opened, false);
        compare(dialog.modal, true);
        compare(takeControl.visible, true);
        compare(takeControl.enabled, true);
        verify(String(status.text).includes("display operation failed"));
        verify(String(status.text).includes(
            "previous takeover attempt was rejected"
        ));
        compare(takeControl.Accessible.name,
            "Review compositor management takeover");
        compare(cancel.Accessible.name,
            "Cancel without changing Hyprland");
        compare(confirm.Accessible.name,
            "Confirm and let HyprShelld manage Hyprland");
        compare(explanation.Accessible.name, explanation.text);
        verify(takeControl.implicitHeight >= 44);
        verify(cancel.implicitHeight >= 44);
        verify(confirm.implicitHeight >= 44);

        let adoptionRequests = 0;
        page.adoptionRequested.connect(function() {
            ++adoptionRequests;
        });

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        compare(adoptionRequests, 0);
        tryCompare(cancel, "activeFocus", true);

        const copy = String(explanation.text).toLowerCase();
        verify(copy.includes("replace the active hyprland entrypoint"));
        verify(copy.includes("reload hyprland"));
        verify(copy.includes("not imported"));
        verify(copy.includes("if hyprland.lua exists"));
        verify(copy.includes("exact original"));
        verify(copy.includes("preserved privately for recovery"));
        verify(copy.includes("if it does not exist"));
        verify(copy.includes("that absence is recorded"));
        verify(copy.includes("legacy configuration files stay where they are"));
        verify(copy.includes("no longer selects them"));
        verify(copy.includes("preserves an existing user-custom.lua"));
        verify(copy.includes("if that file is absent"));
        verify(copy.includes("creates it"));
        verify(copy.includes("loaded last"));
        verify(copy.includes("no user-facing action to stop managing"));
        verify(copy.includes("canceling leaves your files"));
        verify(copy.includes("running compositor unchanged"));

        cancel.clicked();
        tryCompare(dialog, "opened", false);
        compare(adoptionRequests, 0);

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        keyClick(Qt.Key_Escape);
        tryCompare(dialog, "opened", false);
        compare(adoptionRequests, 0);

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        confirm.clicked();
        tryCompare(dialog, "opened", false);
        compare(adoptionRequests, 1);
        compare(confirm.enabled, false);

        // A stale callback after the dialog closes cannot submit twice.
        dialog.confirmAdoption();
        compare(adoptionRequests, 1);
    }

    function test_displayPendingBaselineActionSurvivesTransientError() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.appliedRevision = 6;
        page.errorName = "org.hyprshelld.Error.OperationFailed";
        page.errorMessage = "The previous apply attempt was interrupted.";
        waitForRendering(page);
        wait(0);

        const apply = findChild(
            page,
            "applyCompositorBaselineButton"
        );
        const status = findChild(page, "displayStatusMessage");
        verify(apply !== null);
        verify(status !== null);
        compare(page.managementState, "managed");
        compare(page.baselineCurrent, false);
        compare(page.busy, false);
        compare(page.confirmationState, "idle");
        compare(apply.visible, true);
        compare(apply.enabled, true);
        verify(String(status.text).includes("display operation failed"));
        verify(String(status.text).includes(
            "previous apply attempt was interrupted"
        ));

        let applyRequests = 0;
        page.applyRequested.connect(function() { ++applyRequests; });
        apply.clicked();
        compare(applyRequests, 1);

        page.busy = true;
        compare(apply.visible, true);
        compare(apply.enabled, false);
        compare(applyRequests, 1);

        page.busy = false;
        page.confirmationState = "awaiting-confirmation";
        compare(apply.visible, true);
        compare(apply.enabled, false);
        compare(applyRequests, 1);
    }

    function test_displayAuthorityOverridesStaleOperationError() {
        const rows = [
            {
                serviceAvailable: false,
                writable: true,
                managementState: "unmanaged",
                expectedStatus: "settings are unavailable"
            },
            {
                serviceAvailable: true,
                writable: false,
                managementState: "unmanaged",
                expectedStatus: "read-only"
            },
            {
                serviceAvailable: true,
                writable: true,
                managementState: "conflict",
                expectedStatus: "changed unexpectedly"
            }
        ];
        const staleDetail = "Stale transient operation detail.";

        for (const row of rows) {
            const testWindow = createTemporaryObject(
                displaysPageComponent,
                this
            );
            verify(testWindow !== null);
            const page = testWindow.page;
            configureDisplaysPage(
                page,
                [displayRecord("display-a", "DP-1", true, "", -1)],
                [connectedDisplay("DP-1", true, 0)]
            );
            page.serviceAvailable = row.serviceAvailable;
            page.writable = row.writable;
            page.managementState = row.managementState;
            page.applyState = "retained";
            page.requiredActivation = "reload";
            page.errorName = "org.hyprshelld.Error.OperationFailed";
            page.errorMessage = staleDetail;
            waitForRendering(page);
            wait(0);

            const takeControl = findChild(
                page,
                "adoptCompositorButton"
            );
            const apply = findChild(
                page,
                "applyCompositorBaselineButton"
            );
            const card = findChild(page, "displayStatusCard");
            const status = findChild(page, "displayStatusMessage");
            verify(takeControl !== null);
            verify(apply !== null);
            verify(card !== null);
            verify(status !== null);
            compare(takeControl.visible, false);
            compare(apply.visible, false);
            compare(card.visible, true);
            verify(String(status.text).includes(row.expectedStatus));
            verify(!String(status.text).includes(staleDetail));
        }
    }

    function test_displayAdoptionDialogFitsTheMinimumWindow() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this,
            { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        page.managementState = "unmanaged";
        page.applyState = "unavailable";
        waitForRendering(page);
        wait(0);

        const takeControl = findChild(page, "adoptCompositorButton");
        const dialog = findChild(page, "displayAdoptionDialog");
        const scroll = findChild(page, "displayAdoptionScrollView");
        const content = findChild(page, "displayAdoptionContent");
        const explanation = findChild(
            page,
            "displayAdoptionExplanation"
        );
        const cancel = findChild(
            page,
            "cancelDisplayAdoptionButton"
        );
        const confirm = findChild(
            page,
            "confirmDisplayAdoptionButton"
        );
        verify(takeControl !== null);
        verify(dialog !== null);
        verify(scroll !== null);
        verify(content !== null);
        verify(explanation !== null);
        verify(cancel !== null);
        verify(confirm !== null);

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        verify(dialog.width <= page.width);
        verify(dialog.height <= page.height);
        verify(scroll.width > 0);
        verify(scroll.height > 0);
        verify(content.width <= scroll.availableWidth + 1);

        // A shorter temporary viewport forces the bounded dialog onto its
        // scrolling path without moving the confirmation actions offscreen.
        testWindow.height = 360;
        wait(0);
        verify(dialog.height <= page.height);
        verify(scroll.contentItem.contentHeight
            > scroll.contentItem.height);

        const maximumContentY = scroll.contentItem.contentHeight
            - scroll.contentItem.height;
        verify(maximumContentY > 0);
        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);

        const explanationPosition = explanation.mapToItem(scroll, 0, 0);
        verify(explanationPosition.y + explanation.height
            <= scroll.height + 1);

        const cancelPosition = cancel.mapToItem(page, 0, 0);
        const confirmPosition = confirm.mapToItem(page, 0, 0);
        verify(cancelPosition.x >= 0);
        verify(cancelPosition.x + cancel.width <= page.width);
        verify(cancelPosition.y >= 0);
        verify(cancelPosition.y + cancel.height <= page.height);
        verify(confirmPosition.x >= 0);
        verify(confirmPosition.x + confirm.width <= page.width);
        verify(confirmPosition.y >= 0);
        verify(confirmPosition.y + confirm.height <= page.height);
    }

    function test_displayAdoptionDialogClosesWhenEligibilityIsLost() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        page.managementState = "unmanaged";
        page.applyState = "unavailable";
        waitForRendering(page);
        wait(0);

        const takeControl = findChild(page, "adoptCompositorButton");
        const dialog = findChild(page, "displayAdoptionDialog");
        const confirm = findChild(
            dialog,
            "confirmDisplayAdoptionButton"
        );
        verify(takeControl !== null);
        verify(dialog !== null);
        verify(confirm !== null);

        let adoptionRequests = 0;
        page.adoptionRequested.connect(function() {
            ++adoptionRequests;
        });

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        page.busy = true;
        tryCompare(dialog, "opened", false);
        compare(confirm.enabled, false);
        compare(adoptionRequests, 0);
        page.busy = false;

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        page.serviceAvailable = false;
        tryCompare(dialog, "opened", false);
        compare(confirm.enabled, false);
        compare(adoptionRequests, 0);
        page.serviceAvailable = true;

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        page.writable = false;
        tryCompare(dialog, "opened", false);
        compare(confirm.enabled, false);
        compare(adoptionRequests, 0);
        page.writable = true;

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        page.managementState = "conflict";
        tryCompare(dialog, "opened", false);
        compare(confirm.enabled, false);
        compare(adoptionRequests, 0);
        page.managementState = "unmanaged";

        takeControl.clicked();
        tryCompare(dialog, "opened", true);
        page.confirmationState = "awaiting-confirmation";
        tryCompare(dialog, "opened", false);
        compare(confirm.enabled, false);
        compare(adoptionRequests, 0);
    }

    function test_displayStatusMessagesDescribeAuthoritativeState() {
        const testWindow = createTemporaryObject(
            displaysPageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureDisplaysPage(
            page,
            [displayRecord("display-a", "DP-1", true, "", -1)],
            [connectedDisplay("DP-1", true, 0)]
        );
        waitForRendering(page);
        wait(0);

        const card = findChild(page, "displayStatusCard");
        const status = findChild(page, "displayStatusMessage");
        verify(card !== null);
        verify(status !== null);
        compare(status.Accessible.role, Accessible.AlertMessage);
        compare(card.visible, false);

        page.loadState = "recovered";
        wait(0);
        compare(card.visible, true);
        verify(String(status.text).includes("last known good"));
        verify(String(status.text).includes("Review"));
        compare(status.Accessible.name, status.text);

        page.loadState = "defaulted";
        wait(0);
        verify(String(status.text).includes("safe defaults"));
        verify(String(status.text).includes("Review"));
        compare(status.Accessible.name, status.text);

        page.loadState = "normal";
        page.managementState = "unmanaged";
        wait(0);
        verify(String(status.text).includes("not managing"));
        verify(String(status.text).includes("does not import"));
        verify(String(status.text).includes("If hyprland.lua exists"));
        verify(String(status.text).includes("exact original"));
        verify(String(status.text).includes("preserved privately for recovery"));
        verify(String(status.text).includes("absence is recorded"));
        compare(status.Accessible.name, status.text);

        page.managementState = "conflict";
        wait(0);
        verify(String(status.text).includes("changed unexpectedly"));
        verify(String(status.text).includes("locked"));
        verify(String(status.text).includes(
            "ownership state"
        ));
        verify(String(status.text).includes(
            "If the desktop health warning reports Compositor settings"
        ));
        verify(String(status.text).includes(
            "preserve the unexpected files and seek recovery guidance"
        ));
        compare(status.Accessible.name, status.text);
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

    function test_coordinatorRestartAcceptsEnterKeys() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorHealthy = false;
        warning.coordinatorFailedUnits = ["hyprshelld-configd.service"];
        waitForRendering(warning);

        const restartButton = findChild(
            warning,
            "restartButton-hyprshelld-configd.service"
        );
        verify(restartButton !== null);

        let requestedUnit = "";
        let requestCount = 0;
        warning.restartRequested.connect(function(unitName) {
            requestedUnit = unitName;
            ++requestCount;
        });

        testWindow.requestActivate();
        restartButton.forceActiveFocus();
        tryCompare(restartButton, "activeFocus", true);

        keyClick(Qt.Key_Return);
        compare(requestedUnit, "hyprshelld-configd.service");
        compare(requestCount, 1);

        keyClick(Qt.Key_Enter);
        compare(requestCount, 2);
    }

    function test_componentManagerFailureHasDedicatedRow() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorHealthy = false;
        warning.coordinatorFailedUnits = [
            "hyprshelld-componentd.service"
        ];
        waitForRendering(warning);

        compare(warning.failedComponentCount, 1);
        compare(
            warning.friendlyName("hyprshelld-componentd.service"),
            "Component manager"
        );
        verify(findChild(
            warning,
            "restartButton-hyprshelld-componentd.service"
        ) !== null);
    }

    function test_compositorAuthorityFailureHasDedicatedRow() {
        const testWindow = createTemporaryObject(healthWarningComponent, this);
        verify(testWindow !== null);
        const warning = testWindow.warning;
        verify(warning !== null);
        warning.coordinatorHealthy = false;
        warning.coordinatorFailedUnits = [
            "hyprshelld-compositord.service"
        ];
        waitForRendering(warning);

        compare(warning.failedComponentCount, 1);
        compare(
            warning.friendlyName("hyprshelld-compositord.service"),
            "Compositor settings"
        );
        verify(findChild(
            warning,
            "restartButton-hyprshelld-compositord.service"
        ) !== null);
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
        warning.componentManagerState = "failed";
        warning.compositorState = "failed";
        warning.surfaceState = "active";
        waitForRendering(warning);
        compare(warning.warningVisible, true);
        compare(warning.failedComponentCount, 3);
        compare(warning.friendlyName("hyprshelld.service"), "Shell health");
        compare(
            warning.friendlyName("hyprshelld-componentd.service"),
            "Component manager"
        );
        compare(
            warning.friendlyName("hyprshelld-compositord.service"),
            "Compositor settings"
        );

        const restartButton = findChild(
            warning,
            "restartButton-hyprshelld.service"
        );
        verify(restartButton !== null);
        compare(restartButton.visible, false);
        const componentRestartButton = findChild(
            warning,
            "restartButton-hyprshelld-componentd.service"
        );
        verify(componentRestartButton !== null);
        compare(componentRestartButton.visible, false);
        const compositorRestartButton = findChild(
            warning,
            "restartButton-hyprshelld-compositord.service"
        );
        verify(compositorRestartButton !== null);
        compare(compositorRestartButton.visible, false);
        verify(warning.warningDescription.includes("directly from systemd"));
    }

    function test_appearanceUsesExactTrustedDefinitions() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        compare(page.trustedDefinitionsValid, true);
        compare(page.trustedValuesValid, true);
        compare(page.controlsEnabled, true);

        const border = findChild(page, "appearanceBorderSize");
        const rounding = findChild(page, "appearanceRounding");
        const blur = findChild(page, "appearanceBlurEnabled");
        const shadow = findChild(page, "appearanceShadowEnabled");
        const animations = findChild(page, "appearanceAnimationsEnabled");
        const layout = findChild(page, "appearanceLayout");
        const resize = findChild(page, "appearanceResizeOnBorder");
        const snap = findChild(page, "appearanceSnapEnabled");
        const preview = findChild(page, "appearancePreview");
        const previewStage = findChild(page, "appearancePreviewStage");
        const summary = findChild(page, "appearancePreviewSummary");
        const motionToggle = findChild(
            page,
            "toggleAppearanceMotionButton"
        );
        verify(border !== null);
        verify(rounding !== null);
        verify(blur !== null);
        verify(shadow !== null);
        verify(animations !== null);
        verify(layout !== null);
        verify(resize !== null);
        verify(snap !== null);
        verify(preview !== null);
        verify(previewStage !== null);
        verify(summary !== null);
        verify(motionToggle !== null);

        compare(border.from, 0);
        compare(border.to, 20);
        compare(border.value, 1);
        compare(rounding.from, 0);
        compare(rounding.to, 20);
        compare(rounding.value, 0);
        compare(blur.checked, true);
        compare(shadow.checked, true);
        compare(animations.checked, true);
        compare(layout.currentText, "Dwindle");
        compare(resize.checked, false);
        compare(snap.checked, false);
        compare(previewStage.Accessible.ignored, true);
        compare(summary.Accessible.name, summary.text);
        verify(String(summary.text).includes("Dwindle layout"));
        compare(motionToggle.enabled, true);
        compare(motionToggle.text, "Pause motion");
        compare(
            motionToggle.Accessible.name,
            "Pause the illustrative window motion"
        );
        compare(preview.motionRunning, true);
        compare(preview.motionPaused, false);
        compare(preview.motionStory, "dwindle-split");
        verify(String(summary.text).includes("Motion playing"));

        const targetIds = [
            "refreshAppearanceButton",
            "appearanceOpenDisplaysButton",
            "loadCurrentAppearanceButton",
            "retryApplyAppearanceButton",
            "recoverAppearanceButton",
            "windowBorderSourceButton",
            "appearanceBorderSize",
            "appearanceRounding",
            "appearanceBlurEnabled",
            "appearanceShadowEnabled",
            "appearanceAnimationsEnabled",
            "appearanceLayout",
            "appearanceResizeOnBorder",
            "appearanceSnapEnabled",
            "discardAppearanceDraftButton",
            "resetAppearanceDefaultsButton",
            "saveAppearanceButton",
            "toggleAppearanceMotionButton",
            "cancelAppearanceRecoveryButton",
            "confirmAppearanceRecoveryButton"
        ];
        for (const objectName of targetIds) {
            const target = findChild(page, objectName);
            verify(target !== null, "Missing target " + objectName);
            verify(
                target.implicitHeight >= 44,
                objectName + " must provide a 44px interaction target"
            );
        }

        page.setDraftValue(page.animationsId, false);
        page.setDraftValue(page.layoutId, "master");
        page.setDraftValue(page.snapId, true);
        wait(0);
        compare(motionToggle.enabled, false);
        compare(motionToggle.text, "Motion off");
        compare(
            motionToggle.Accessible.name,
            "Illustrative window motion is off"
        );
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "off");
        verify(String(summary.text).includes("Master layout"));
        verify(String(summary.text).includes("Animations off"));
        compare(
            findChild(page, "appearancePreviewSnapGuide").visible,
            true
        );
    }

    function test_syncedWindowBorderPairIsReadOnlyAndChangesSourceAtomically() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 7;
        resolvedValues[page.roundingId] = 13;
        configureAppearancePage(page, resolvedValues, true);
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        const border = findChild(page, "appearanceBorderSize");
        const rounding = findChild(page, "appearanceRounding");
        const blur = findChild(page, "appearanceBlurEnabled");
        const source = findChild(page, "windowBorderSourceButton");
        const message = findChild(page, "windowBorderAuthorityMessage");
        verify(border !== null);
        verify(rounding !== null);
        verify(blur !== null);
        verify(source !== null);
        verify(message !== null);

        compare(border.value, 7);
        compare(rounding.value, 13);
        compare(border.enabled, false);
        compare(rounding.enabled, false);
        compare(blur.enabled, true);
        compare(source.text, "Override window borders");
        compare(source.enabled, true);
        verify(source.implicitHeight >= 44);
        verify(String(message.text).includes("Controlled by HyprShelld"));
        verify(String(message.text).includes("Bar page"));

        let requestCount = 0;
        let requests = [];
        page.windowBorderSyncRequested.connect(function(sync) {
            ++requestCount;
            requests.push(sync);
        });

        source.clicked();
        compare(requestCount, 1);
        compare(requests, [false]);

        // Model the single authoritative Config1 update that follows the
        // request. Only the synchronized pair changes editability.
        page.sharedBorderBusy = true;
        page.sharedBorderConfigRevisionToken = "12";
        page.windowBorderSynced = false;
        page.sharedBorderSyncState = "override";
        page.sharedBorderVerifiedRevisionToken = "12";
        page.sharedBorderBusy = false;
        wait(0);
        compare(border.enabled, true);
        compare(rounding.enabled, true);
        compare(blur.enabled, true);
        compare(source.text, "Sync with HyprShelld");

        source.clicked();
        compare(requestCount, 2);
        compare(requests, [false, true]);
    }

    function test_unavailableProjectionKeepsSourceActionReversible() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const overrideValues = appearanceDefaults();
        overrideValues[page.borderSizeId] = 4;
        overrideValues[page.roundingId] = 5;
        configureAppearancePage(page, overrideValues);
        waitForRendering(page);
        wait(0);

        const source = findChild(page, "windowBorderSourceButton");
        const save = findChild(page, "saveAppearanceButton");
        const retryApply = findChild(
            page,
            "retryApplyAppearanceButton"
        );
        verify(source !== null);
        verify(save !== null);
        verify(retryApply !== null);

        const requests = [];
        page.windowBorderSyncRequested.connect(function(sync) {
            requests.push(sync);
        });
        source.clicked();
        compare(requests, [true]);

        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 8;
        resolvedValues[page.roundingId] = 12;
        page.sharedBorderBusy = true;
        page.sharedBorderConfigRevisionToken = "12";
        page.windowBorderSynced = true;
        page.appearanceValues = resolvedValues;
        page.sharedBorderSyncState = "unavailable";
        page.sharedBorderBusy = false;
        wait(0);
        page.reviewProjection();

        compare(page.sharedBorderSourceRequestPending, false);
        compare(page.sharedBorderProjectionPending, true);
        compare(page.sharedBorderRevisionVerified, false);
        compare(page.draftValue(page.borderSizeId), 8);
        compare(page.draftValue(page.roundingId), 12);
        compare(page.draftDirty, false);
        compare(source.enabled, true);
        compare(save.enabled, false);
        page.retryApplyAvailable = true;
        compare(retryApply.enabled, false);

        source.clicked();
        compare(requests, [true, false]);
        page.sharedBorderBusy = true;
        page.sharedBorderConfigRevisionToken = "13";
        page.windowBorderSynced = false;
        page.appearanceValues = overrideValues;
        page.sharedBorderBusy = false;
        wait(0);
        page.reviewProjection();

        compare(page.sharedBorderSourceRequestPending, false);
        compare(page.sharedBorderProjectionPending, true);
        compare(page.sharedBorderRevisionVerified, false);
        compare(page.draftValue(page.borderSizeId), 4);
        compare(page.draftValue(page.roundingId), 5);
        compare(page.draftDirty, false);
        compare(source.enabled, true);
        compare(save.enabled, false);
        compare(retryApply.enabled, false);
        compare(requests, [true, false]);
    }

    function test_dirtyAppearanceDraftBlocksWindowBorderSourceChanges() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const source = findChild(page, "windowBorderSourceButton");
        verify(source !== null);
        compare(source.enabled, true);

        page.setDraftValue(page.blurId, false);
        compare(page.draftDirty, true);
        compare(source.enabled, false);

        page.synchronizeDraft();
        compare(page.draftDirty, false);
        compare(source.enabled, true);

        page.saveSubmitted = true;
        compare(source.enabled, false);
        page.saveSubmitted = false;
        compare(source.enabled, true);
    }

    function test_sharedBorderBusyPreservesAndLocksAppearanceDraft() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.blurId, false);
        const save = findChild(page, "saveAppearanceButton");
        const source = findChild(page, "windowBorderSourceButton");
        verify(save !== null);
        verify(source !== null);
        compare(page.draftDirty, true);
        compare(save.enabled, true);

        let saveCount = 0;
        page.saveRequested.connect(function() { ++saveCount; });
        page.sharedBorderBusy = true;
        wait(0);
        compare(page.controlsEnabled, false);
        compare(save.enabled, false);
        compare(source.enabled, false);

        page.setDraftValue(page.shadowId, false);
        page.resetDraftToDefaults();
        page.synchronizeDraft();
        page.submitDraft();
        compare(page.draftValue(page.blurId), false);
        compare(page.draftValue(page.shadowId), true);
        compare(page.draftDirty, true);
        compare(saveCount, 0);

        page.sharedBorderBusy = false;
        wait(0);
        compare(page.controlsEnabled, true);
        compare(page.draftValue(page.blurId), false);
        compare(save.enabled, true);
        page.setDraftValue(page.shadowId, false);
        compare(page.draftValue(page.shadowId), false);
    }

    function test_appearanceApplyActionsRequireSettledSharedBorderAuthority() {
        const rows = [
            {
                label: "synchronized current",
                synced: true,
                state: "current",
                sourceBusy: false,
                safe: true,
                controlsEnabled: true
            },
            {
                label: "synchronized saved",
                synced: true,
                state: "saved",
                sourceBusy: false,
                safe: true,
                controlsEnabled: true
            },
            {
                label: "synchronized pending",
                synced: true,
                state: "pending",
                sourceBusy: false,
                safe: false,
                controlsEnabled: false
            },
            {
                label: "synchronized unavailable",
                synced: true,
                state: "unavailable",
                sourceBusy: false,
                safe: false,
                controlsEnabled: true
            },
            {
                label: "synchronized failed",
                synced: true,
                state: "failed",
                sourceBusy: false,
                safe: false,
                controlsEnabled: true
            },
            {
                label: "explicit override",
                synced: false,
                state: "override",
                sourceBusy: false,
                safe: true,
                controlsEnabled: true
            },
            {
                label: "override source busy",
                synced: false,
                state: "override",
                sourceBusy: true,
                safe: false,
                controlsEnabled: false
            },
            {
                label: "incoherent override state",
                synced: false,
                state: "current",
                sourceBusy: false,
                safe: false,
                controlsEnabled: true
            }
        ];

        for (const row of rows) {
            const testWindow = createTemporaryObject(
                appearancePageComponent,
                this
            );
            verify(testWindow !== null, row.label);
            const page = testWindow.page;
            configureAppearancePage(page, undefined, row.synced);
            waitForRendering(page);
            wait(0);

            page.setDraftValue(page.blurId, false);
            compare(page.draftDirty, true, row.label);
            page.retryApplyAvailable = true;
            page.sharedBorderSyncState = row.state;
            page.sharedBorderBusy = row.sourceBusy;
            wait(0);

            const save = findChild(page, "saveAppearanceButton");
            const retry = findChild(page, "retryApplyAppearanceButton");
            verify(save !== null, row.label);
            verify(retry !== null, row.label);
            compare(page.sharedBorderApplySafe, row.safe, row.label);
            compare(page.controlsEnabled, row.controlsEnabled, row.label);
            compare(save.enabled, row.safe, row.label);
            compare(retry.enabled, row.safe, row.label);
            compare(page.draftValue(page.blurId), false, row.label);
            testWindow.destroy();
        }
    }

    function test_retryApplyLocksForCompleteSourceTransition() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        page.retryApplyAvailable = true;
        waitForRendering(page);
        wait(0);

        const retry = findChild(page, "retryApplyAppearanceButton");
        const source = findChild(page, "windowBorderSourceButton");
        verify(retry !== null);
        verify(source !== null);
        compare(page.sharedBorderApplySafe, true);
        compare(retry.enabled, true);

        let sourceRequests = 0;
        page.windowBorderSyncRequested.connect(function(sync) {
            ++sourceRequests;
            compare(sync, true);
        });
        source.clicked();
        compare(sourceRequests, 1);
        compare(page.sharedBorderSourceRequestPending, true);
        compare(retry.enabled, false);

        page.sharedBorderBusy = true;
        page.sharedBorderConfigRevisionToken = "12";
        page.sharedBorderSyncState = "pending";
        page.windowBorderSynced = true;
        page.sharedBorderBusy = false;
        wait(0);
        compare(page.sharedBorderSourceRequestPending, false);
        compare(page.sharedBorderProjectionPending, true);
        compare(retry.enabled, false);

        page.sharedBorderVerifiedRevisionToken = "12";
        page.sharedBorderSyncState = "saved";
        wait(0);
        page.reviewProjection();
        compare(page.sharedBorderProjectionPending, false);
        compare(page.sharedBorderApplySafe, true);
        compare(retry.enabled, true);
        compare(sourceRequests, 1);
    }

    function test_sharedBorderRevisionCorrelationIsLosslessAndPolicyAware() {
        const terminalStates = ["current", "saved"];
        const configRevision = "9007199254740993";
        const olderRevision = "9007199254740992";

        for (const terminalState of terminalStates) {
            const testWindow = createTemporaryObject(
                appearancePageComponent,
                this
            );
            verify(testWindow !== null, terminalState);
            const page = testWindow.page;
            configureAppearancePage(page);
            page.retryApplyAvailable = true;
            waitForRendering(page);
            wait(0);

            const source = findChild(page, "windowBorderSourceButton");
            const retry = findChild(page, "retryApplyAppearanceButton");
            verify(source !== null, terminalState);
            verify(retry !== null, terminalState);

            const resolvedValues = appearanceDefaults();
            resolvedValues[page.borderSizeId] = 6;
            resolvedValues[page.roundingId] = 10;
            page.sharedBorderBusy = true;
            page.sharedBorderConfigRevisionToken = configRevision;
            page.sharedBorderVerifiedRevisionToken = olderRevision;
            page.windowBorderSynced = true;
            page.appearanceValues = resolvedValues;
            page.sharedBorderSyncState = terminalState;
            page.sharedBorderBusy = false;
            wait(0);
            page.reviewProjection();

            // No transient pending state was observed. The already-terminal
            // projection remains gated until the exact source revision lands.
            compare(
                page.sharedBorderConfigRevisionToken,
                configRevision,
                terminalState
            );
            compare(
                page.sharedBorderVerifiedRevisionToken,
                olderRevision,
                terminalState
            );
            compare(page.sharedBorderRevisionVerified, false, terminalState);
            compare(
                page.sharedBorderProjectionPending,
                true,
                terminalState
            );
            compare(page.sharedBorderApplySafe, false, terminalState);
            compare(retry.enabled, false, terminalState);
            compare(source.enabled, true, terminalState);

            page.sharedBorderVerifiedRevisionToken =
                "0" + configRevision;
            wait(0);
            compare(page.sharedBorderRevisionVerified, false, terminalState);
            compare(
                page.sharedBorderProjectionPending,
                true,
                terminalState
            );

            page.sharedBorderVerifiedRevisionToken = configRevision;
            wait(0);
            page.reviewProjection();
            compare(page.sharedBorderRevisionVerified, true, terminalState);
            compare(
                page.sharedBorderProjectionPending,
                false,
                terminalState
            );
            compare(page.sharedBorderApplySafe, true, terminalState);
            compare(retry.enabled, true, terminalState);
            compare(page.draftValue(page.borderSizeId), 6, terminalState);
            compare(page.draftValue(page.roundingId), 10, terminalState);
            testWindow.destroy();
        }

        const terminalFailureWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(terminalFailureWindow !== null);
        const failurePage = terminalFailureWindow.page;
        configureAppearancePage(failurePage);
        failurePage.sharedBorderBusy = true;
        failurePage.sharedBorderConfigRevisionToken = "12";
        failurePage.sharedBorderVerifiedRevisionToken = "12";
        failurePage.windowBorderSynced = true;
        failurePage.sharedBorderSyncState = "failed";
        failurePage.sharedBorderBusy = false;
        wait(0);
        failurePage.reviewProjection();
        compare(failurePage.sharedBorderProjectionPending, false);
        compare(failurePage.controlsEnabled, true);
        compare(failurePage.sharedBorderApplySafe, false);

        const unavailableWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(unavailableWindow !== null);
        const unavailablePage = unavailableWindow.page;
        configureAppearancePage(unavailablePage);
        unavailablePage.sharedBorderBusy = true;
        unavailablePage.sharedBorderConfigRevisionToken = "0";
        unavailablePage.sharedBorderVerifiedRevisionToken = "0";
        unavailablePage.windowBorderSynced = true;
        unavailablePage.sharedBorderSyncState = "unavailable";
        unavailablePage.sharedBorderBusy = false;
        wait(0);
        unavailablePage.reviewProjection();
        compare(unavailablePage.sharedBorderRevisionVerified, true);
        compare(unavailablePage.sharedBorderProjectionVerified, false);
        compare(unavailablePage.sharedBorderProjectionPending, true);
        compare(unavailablePage.sharedBorderApplySafe, false);

        unavailablePage.sharedBorderConfigRevisionToken = "12";
        unavailablePage.sharedBorderVerifiedRevisionToken = "12";
        wait(0);
        unavailablePage.reviewProjection();
        compare(unavailablePage.sharedBorderProjectionVerified, true);
        compare(unavailablePage.sharedBorderProjectionPending, false);
        compare(unavailablePage.controlsEnabled, true);
        compare(unavailablePage.sharedBorderApplySafe, false);
    }

    function test_settledSourceTransitionProjectsOneCoherentBorderPair() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const overrideValues = appearanceDefaults();
        overrideValues[page.borderSizeId] = 4;
        overrideValues[page.roundingId] = 5;
        configureAppearancePage(page, overrideValues);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.blurId, false);
        compare(page.draftDirty, true);

        page.sharedBorderBusy = true;
        page.sharedBorderConfigRevisionToken = "12";
        page.sharedBorderSyncState = "pending";
        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 9;
        resolvedValues[page.roundingId] = 14;
        page.appearanceValues = resolvedValues;
        page.revisionToken = "8";
        page.windowBorderSynced = true;
        wait(0);

        compare(page.controlsEnabled, false);
        compare(page.draftValue(page.borderSizeId), 4);
        compare(page.draftValue(page.roundingId), 5);
        compare(findChild(page, "saveAppearanceButton").enabled, false);

        page.sharedBorderBusy = false;
        wait(0);
        compare(page.controlsEnabled, false);
        page.sharedBorderVerifiedRevisionToken = "12";
        page.sharedBorderSyncState = "current";
        wait(0);
        page.reviewProjection();

        const border = findChild(page, "appearanceBorderSize");
        const rounding = findChild(page, "appearanceRounding");
        const source = findChild(page, "windowBorderSourceButton");
        const save = findChild(page, "saveAppearanceButton");
        verify(border !== null);
        verify(rounding !== null);
        verify(source !== null);
        verify(save !== null);
        compare(page.draftValue(page.borderSizeId), 9);
        compare(page.draftValue(page.roundingId), 14);
        compare(page.synchronizedValues[page.borderSizeId], 9);
        compare(page.synchronizedValues[page.roundingId], 14);
        compare(page.draftValue(page.blurId), false);
        compare(page.synchronizedValues[page.blurId], true);
        compare(page.synchronizedRevisionToken, "8");
        compare(page.externalChangeWhileEditing, false);
        compare(page.sharedBorderProjectionPending, false);
        compare(page.draftDirty, true);
        compare(border.value, 9);
        compare(rounding.value, 14);
        compare(border.enabled, false);
        compare(rounding.enabled, false);
        compare(source.enabled, false);
        compare(save.enabled, true);
    }

    function test_sharedBorderHydrationWaitsForAuthoritativeRevision() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const overrideValues = appearanceDefaults();
        overrideValues[page.borderSizeId] = 4;
        overrideValues[page.roundingId] = 5;
        configureAppearancePage(page, overrideValues);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.blurId, false);
        compare(page.draftDirty, true);
        compare(page.synchronizedRevisionToken, "7");

        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 9;
        resolvedValues[page.roundingId] = 14;

        // A reconnect can deliver the terminal shared-border tuple while the
        // compositor client still exposes its old, unavailable snapshot.
        page.serviceAvailable = false;
        page.sharedBorderConfigRevisionToken = "12";
        page.sharedBorderVerifiedRevisionToken = "12";
        page.windowBorderSynced = true;
        page.appearanceValues = resolvedValues;
        page.sharedBorderSyncState = "current";
        wait(0);
        page.reviewProjection();

        compare(page.sharedBorderProjectionPending, true);
        compare(page.draftValue(page.borderSizeId), 4);
        compare(page.draftValue(page.roundingId), 5);
        compare(page.draftValue(page.blurId), false);
        compare(page.synchronizedValues[page.blurId], true);
        compare(page.synchronizedRevisionToken, "7");
        compare(page.externalChangeWhileEditing, false);

        // The refreshed revision can arrive while availability is still
        // false. Raising availability must schedule the authoritative review.
        page.revisionToken = "8";
        wait(0);
        compare(page.synchronizedRevisionToken, "7");
        compare(page.externalChangeWhileEditing, false);

        page.serviceAvailable = true;
        wait(0);

        compare(page.sharedBorderProjectionPending, false);
        compare(page.draftValue(page.borderSizeId), 9);
        compare(page.draftValue(page.roundingId), 14);
        compare(page.draftValue(page.blurId), false);
        compare(page.synchronizedValues[page.borderSizeId], 9);
        compare(page.synchronizedValues[page.roundingId], 14);
        compare(page.synchronizedValues[page.blurId], true);
        compare(page.synchronizedRevisionToken, "8");
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, true);
        compare(findChild(page, "saveAppearanceButton").enabled, true);
    }

    function test_syncedAppearanceResetPreservesResolvedBorderPair() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 6;
        resolvedValues[page.roundingId] = 12;
        configureAppearancePage(page, resolvedValues, true);
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        // The synchronized pair rejects Appearance draft writes, while an
        // unrelated setting remains independently editable.
        page.setDraftValue(page.borderSizeId, 3);
        page.setDraftValue(page.roundingId, 4);
        compare(page.draftValue(page.borderSizeId), 6);
        compare(page.draftValue(page.roundingId), 12);
        const reset = findChild(page, "resetAppearanceDefaultsButton");
        verify(reset !== null);
        compare(reset.enabled, false);

        page.setDraftValue(page.blurId, false);
        compare(page.draftValue(page.blurId), false);
        compare(page.draftDirty, true);
        compare(reset.enabled, true);
        reset.clicked();
        compare(page.draftValue(page.borderSizeId), 6);
        compare(page.draftValue(page.roundingId), 12);
        compare(page.draftValue(page.blurId), true);
        compare(page.draftValue(page.layoutId), "dwindle");
        compare(page.draftDirty, false);
        compare(reset.enabled, false);
    }

    function test_unavailableSharedBorderSourceFailsClosedLocally() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const resolvedValues = appearanceDefaults();
        resolvedValues[page.borderSizeId] = 5;
        resolvedValues[page.roundingId] = 9;
        configureAppearancePage(page, resolvedValues, true);
        page.sharedBorderAvailable = false;
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        const border = findChild(page, "appearanceBorderSize");
        const rounding = findChild(page, "appearanceRounding");
        const blur = findChild(page, "appearanceBlurEnabled");
        const source = findChild(page, "windowBorderSourceButton");
        const message = findChild(page, "windowBorderAuthorityMessage");
        verify(border !== null);
        verify(rounding !== null);
        verify(blur !== null);
        verify(source !== null);
        verify(message !== null);

        compare(border.enabled, false);
        compare(rounding.enabled, false);
        compare(source.enabled, false);
        verify(String(message.text).includes("service is unavailable"));
        verify(String(message.text).includes("read-only"));
        compare(blur.enabled, true);
        page.setDraftValue(page.blurId, false);
        compare(page.draftValue(page.blurId), false);
        compare(page.draftDirty, true);
    }

    function test_failedSharedBorderSyncOffersOneExplicitRetry() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page, undefined, true);
        page.sharedBorderSyncState = "failed";
        page.sharedBorderSyncError = "Injected synchronization failure.";
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        const message = findChild(page, "windowBorderAuthorityMessage");
        const retry = findChild(page, "retrySharedBorderSyncButton");
        verify(message !== null);
        verify(retry !== null);
        verify(String(message.text).includes("Controlled by HyprShelld"));
        verify(String(message.text).includes(
            "Injected synchronization failure."
        ));
        compare(retry.visible, true);
        compare(retry.enabled, true);
        verify(retry.implicitHeight >= 44);

        let retryCount = 0;
        page.retrySharedBorderSyncRequested.connect(function() {
            ++retryCount;
        });
        retry.clicked();
        compare(retryCount, 1);

        page.busy = true;
        compare(retry.enabled, false);
    }

    function test_unavailableSharedBorderSyncOffersBoundedRetry() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page, undefined, true);
        page.sharedBorderSyncState = "unavailable";
        page.sharedBorderSyncError = "Config1 GetAll failed transiently.";
        page.reviewProjection();
        waitForRendering(page);
        wait(0);

        const retry = findChild(page, "retrySharedBorderSyncButton");
        verify(retry !== null);
        compare(page.sharedBorderAvailable, true);
        compare(page.serviceAvailable, true);
        compare(retry.visible, true);
        compare(retry.enabled, true);
        verify(retry.implicitHeight >= 44);

        let retryCount = 0;
        page.retrySharedBorderSyncRequested.connect(function() {
            ++retryCount;
        });
        retry.clicked();
        compare(retryCount, 1);

        page.sharedBorderBusy = true;
        compare(retry.enabled, false);
        page.sharedBorderBusy = false;
        page.busy = true;
        compare(retry.enabled, false);
        page.busy = false;
        page.serviceAvailable = false;
        compare(retry.visible, false);
    }

    function test_sharedBorderMutationFailureIsDistinctAndRetryClearsIt() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const source = findChild(page, "windowBorderSourceButton");
        const error = findChild(
            page,
            "sharedBorderMutationErrorMessage"
        );
        verify(source !== null);
        verify(error !== null);
        compare(error.visible, false);

        // A retained error from another Config1 operation is not evidence
        // that this page's source action failed.
        page.sharedBorderClientError =
            "Retained Bar height mutation failure.";
        wait(0);
        compare(error.visible, false);

        let requestCount = 0;
        let requestedSync = false;
        page.windowBorderSyncRequested.connect(function(sync) {
            ++requestCount;
            requestedSync = sync;
        });
        source.clicked();
        compare(requestCount, 1);
        compare(requestedSync, true);

        // ConfigClient clears its retained error before beginning a mutation.
        page.sharedBorderClientError = "";
        page.sharedBorderBusy = true;
        const failurePrefix =
            "Injected Config1 source mutation failure.";
        const oversizedFailure = failurePrefix
            + new Array(2049).join("x");
        page.sharedBorderClientError = oversizedFailure;
        page.sharedBorderBusy = false;
        wait(0);
        compare(error.visible, true);
        verify(String(error.text).includes(
            "shared border source could not be changed"
        ));
        verify(String(error.text).includes(
            failurePrefix
        ));
        compare(
            page.sharedBorderSourceActionError,
            oversizedFailure.slice(
                0,
                page.maximumSharedBorderSourceErrorLength
            )
        );
        compare(
            page.sharedBorderSourceActionError.length,
            page.maximumSharedBorderSourceErrorLength
        );
        compare(error.textFormat, Text.PlainText);
        compare(error.Accessible.role, Accessible.AlertMessage);
        compare(error.Accessible.name, error.text);
        compare(source.enabled, true);

        source.clicked();
        compare(requestCount, 2);
        compare(requestedSync, true);
        compare(error.visible, false);
        page.sharedBorderClientError = "";
        page.sharedBorderBusy = true;
        compare(source.enabled, false);
        page.sharedBorderConfigRevisionToken = "12";
        page.sharedBorderSyncState = "pending";
        page.windowBorderSynced = true;
        page.sharedBorderVerifiedRevisionToken = "12";
        page.sharedBorderBusy = false;
        page.sharedBorderSyncState = "current";
        wait(0);
        compare(error.visible, false);
        compare(page.sharedBorderSourceRequestPending, false);
        compare(source.enabled, true);
    }

    function test_mainProjectsSharedBorderIntoAppearance() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const appearance = findChild(application, "appearancePage");
        verify(appearance !== null);

        const input = appearanceDefaults();
        input[appearance.borderSizeId] = 19;
        input[appearance.roundingId] = 2;
        const resolved = application.appearanceValuesWithSharedBorder(input);
        verify(resolved !== null);

        if (ConfigClient.syncHyprlandWindowBorders) {
            compare(
                resolved[appearance.borderSizeId],
                ConfigClient.shellBorderEnabled
                    ? ConfigClient.shellBorderWidth : 0
            );
            compare(
                resolved[appearance.roundingId],
                ConfigClient.shellBorderRadius
            );
            compare(input[appearance.borderSizeId], 19);
            compare(input[appearance.roundingId], 2);
        } else {
            compare(resolved, input);
        }
        compare(
            appearance.windowBorderSynced,
            ConfigClient.syncHyprlandWindowBorders
        );
        compare(
            appearance.sharedBorderClientError,
            ConfigClient.lastErrorMessage
        );
        compare(
            appearance.sharedBorderConfigRevisionToken,
            ConfigClient.revisionToken
        );
        compare(
            appearance.sharedBorderVerifiedRevisionToken,
            CompositorClient.sharedBorderSourceRevisionToken
        );
    }

    function test_appearanceMotionAutoRunsAndTogglesDeterministically() {
        const testWindow = createTemporaryObject(
            appearancePreviewComponent,
            this
        );
        verify(testWindow !== null);
        const preview = testWindow.preview;
        waitForRendering(preview);
        wait(0);

        const toggle = findChild(
            preview,
            "toggleAppearanceMotionButton"
        );
        const summary = findChild(preview, "appearancePreviewSummary");
        verify(toggle !== null);
        verify(summary !== null);

        compare(preview.animationsEnabled, true);
        compare(preview.motionPaused, false);
        compare(preview.motionRunning, true);
        compare(preview.motionStatus, "playing");
        verify(preview.motionPhase !== "off");
        compare(toggle.enabled, true);
        compare(toggle.text, "Pause motion");
        compare(
            toggle.Accessible.name,
            "Pause the illustrative window motion"
        );
        verify(toggle.implicitHeight >= 44);
        compare(summary.Accessible.name, summary.text);
        verify(String(summary.text).includes("Motion playing"));

        toggle.clicked();
        compare(preview.motionPaused, true);
        compare(preview.motionRunning, false);
        compare(preview.motionStatus, "paused");
        compare(toggle.text, "Play motion");
        compare(
            toggle.Accessible.name,
            "Play the illustrative window motion"
        );
        verify(String(summary.text).includes("Motion paused"));

        const pausedPhase = preview.motionPhase;
        const pausedProgress = preview.motionProgress;
        preview.synchronizeMotion(false);
        wait(0);
        compare(preview.motionRunning, false);
        compare(preview.motionPhase, pausedPhase);
        compare(preview.motionProgress, pausedProgress);

        toggle.clicked();
        compare(preview.motionPaused, false);
        compare(preview.motionRunning, true);
        compare(preview.motionStatus, "playing");
        compare(toggle.text, "Pause motion");
        verify(preview.motionPhase !== "off");
        verify(String(summary.text).includes("Motion playing"));
    }

    function test_appearanceMotionStoriesUseDistinctGeometry() {
        const testWindow = createTemporaryObject(
            appearancePreviewComponent,
            this
        );
        verify(testWindow !== null);
        const preview = testWindow.preview;
        waitForRendering(preview);
        wait(0);

        const stage = findChild(preview, "appearancePreviewStage");
        const active = findChild(
            preview,
            "appearancePreviewActiveWindow"
        );
        const secondary = findChild(
            preview,
            "appearancePreviewSecondaryWindow"
        );
        const spawned = findChild(
            preview,
            "appearancePreviewSpawnedWindow"
        );
        verify(stage !== null);
        verify(active !== null);
        verify(secondary !== null);
        verify(spawned !== null);

        preview.motionPaused = true;
        const layouts = [
            { mode: "dwindle", story: "dwindle-split" },
            { mode: "master", story: "master-stack" },
            { mode: "scrolling", story: "scrolling-strip" },
            { mode: "monocle", story: "monocle-replace" }
        ];
        const stories = [];

        for (const layout of layouts) {
            preview.layoutMode = layout.mode;
            preview.synchronizeMotion(true);
            wait(0);

            compare(preview.motionRunning, false);
            compare(preview.motionPhase, "resting");
            compare(preview.motionProgress, 0);
            compare(preview.motionStory, layout.story);
            compare(stories.indexOf(preview.motionStory), -1);
            stories.push(preview.motionStory);

            const before = {
                activeX: active.x,
                activeY: active.y,
                activeWidth: active.width,
                activeHeight: active.height,
                activeOpacity: active.opacity,
                secondaryX: secondary.x,
                secondaryY: secondary.y,
                secondaryWidth: secondary.width,
                secondaryHeight: secondary.height,
                spawnedX: spawned.x,
                spawnedY: spawned.y,
                spawnedWidth: spawned.width,
                spawnedHeight: spawned.height,
                spawnedOpacity: spawned.opacity
            };

            preview.motionProgress = 1;
            wait(0);
            const after = {
                activeX: active.x,
                activeY: active.y,
                activeWidth: active.width,
                activeHeight: active.height,
                activeOpacity: active.opacity,
                secondaryX: secondary.x,
                secondaryY: secondary.y,
                secondaryWidth: secondary.width,
                secondaryHeight: secondary.height,
                spawnedX: spawned.x,
                spawnedY: spawned.y,
                spawnedWidth: spawned.width,
                spawnedHeight: spawned.height,
                spawnedOpacity: spawned.opacity,
                spawnedScale: spawned.scale
            };

            compare(before.spawnedOpacity, 0);
            compare(after.spawnedOpacity, 1);
            const tolerance = 0.01;

            if (layout.mode === "dwindle") {
                compare(secondary.visible, false);
                verify(Math.abs(
                    stage.dwindleAreaLeft - stage.width * 0.07
                ) <= tolerance);
                verify(Math.abs(
                    stage.dwindleAreaRight - stage.width * 0.93
                ) <= tolerance);
                compare(
                    stage.dwindleGap,
                    Math.max(8, Math.round(stage.width * 0.02))
                );
                verify(Math.abs(
                    before.activeX - stage.dwindleAreaLeft
                ) <= tolerance);
                verify(Math.abs(
                    before.activeWidth - stage.dwindleAreaWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.activeX + before.activeWidth
                        - stage.dwindleAreaRight
                ) <= tolerance);
                verify(Math.abs(
                    before.spawnedX - before.activeX
                        - before.activeWidth - stage.dwindleGap
                ) <= tolerance);

                verify(Math.abs(
                    after.activeX - before.activeX
                ) <= tolerance);
                verify(Math.abs(
                    after.activeY - before.activeY
                ) <= tolerance);
                verify(Math.abs(
                    after.activeHeight - before.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.activeWidth - stage.dwindleTileWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedWidth - stage.dwindleTileWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.activeWidth - after.spawnedWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedY - after.activeY
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedHeight - after.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX - after.activeX
                        - after.activeWidth - stage.dwindleGap
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX + after.spawnedWidth
                        - stage.dwindleAreaRight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX + after.spawnedWidth
                        - before.activeX - before.activeWidth
                ) <= tolerance);
            } else if (layout.mode === "master") {
                compare(secondary.visible, true);
                verify(Math.abs(
                    stage.masterWidth
                        - (stage.windowAreaWidth - stage.windowGap) * 0.55
                ) <= tolerance);
                verify(Math.abs(
                    stage.masterStackWidth
                        - (stage.windowAreaWidth - stage.windowGap) * 0.45
                ) <= tolerance);

                verify(Math.abs(
                    before.activeX - stage.windowAreaLeft
                ) <= tolerance);
                verify(Math.abs(
                    before.activeY - stage.windowAreaTop
                ) <= tolerance);
                verify(Math.abs(
                    before.activeWidth - stage.masterWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.activeHeight - stage.windowAreaHeight
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryX - before.activeX
                        - before.activeWidth - stage.windowGap
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryY - stage.windowAreaTop
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryWidth - stage.masterStackWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryHeight - stage.windowAreaHeight
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryX + before.secondaryWidth
                        - stage.windowAreaRight
                ) <= tolerance);

                verify(Math.abs(
                    after.activeX - before.activeX
                ) <= tolerance);
                verify(Math.abs(
                    after.activeY - before.activeY
                ) <= tolerance);
                verify(Math.abs(
                    after.activeWidth - before.activeWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.activeHeight - before.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryX - before.secondaryX
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryY - before.secondaryY
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryWidth - before.secondaryWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX - after.secondaryX
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedWidth - after.secondaryWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryHeight - stage.masterStackTileHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedHeight - after.secondaryHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedY - after.secondaryY
                        - after.secondaryHeight - stage.windowGap
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedY + after.spawnedHeight
                        - before.secondaryY - before.secondaryHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX + after.spawnedWidth
                        - before.secondaryX - before.secondaryWidth
                ) <= tolerance);
            } else if (layout.mode === "scrolling") {
                compare(secondary.visible, true);
                verify(Math.abs(
                    stage.scrollingTravel
                        - stage.halfWindowWidth - stage.windowGap
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryX - before.activeX
                        - stage.scrollingTravel
                ) <= tolerance);
                verify(Math.abs(
                    before.spawnedX - before.secondaryX
                        - stage.scrollingTravel
                ) <= tolerance);
                verify(Math.abs(
                    before.activeWidth - stage.halfWindowWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryWidth - before.activeWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.spawnedWidth - before.activeWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.activeY - stage.scrollingAreaTop
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryY - before.activeY
                ) <= tolerance);
                verify(Math.abs(
                    before.spawnedY - before.activeY
                ) <= tolerance);
                verify(Math.abs(
                    before.activeHeight - stage.scrollingAreaHeight
                ) <= tolerance);
                verify(Math.abs(
                    before.secondaryHeight - before.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    before.spawnedHeight - before.activeHeight
                ) <= tolerance);

                verify(Math.abs(
                    after.activeX
                        - (before.activeX - stage.scrollingTravel)
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryX - before.activeX
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX - before.secondaryX
                ) <= tolerance);
                verify(Math.abs(
                    after.activeWidth - stage.halfWindowWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryWidth - after.activeWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedWidth - after.activeWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryY - after.activeY
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedY - after.activeY
                ) <= tolerance);
                verify(Math.abs(
                    after.secondaryHeight - after.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedHeight - after.activeHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedX + after.spawnedWidth
                        - stage.windowAreaRight
                ) <= tolerance);
                verify(after.spawnedX >= stage.windowAreaLeft - tolerance);
                verify(after.spawnedX + after.spawnedWidth
                    <= stage.windowAreaRight + tolerance);
                compare(after.activeOpacity, 0);
            } else {
                compare(secondary.visible, false);
                verify(Math.abs(
                    before.activeX - before.spawnedX
                ) <= tolerance);
                verify(Math.abs(
                    before.activeY - before.spawnedY
                ) <= tolerance);
                verify(Math.abs(
                    before.activeWidth - before.spawnedWidth
                ) <= tolerance);
                verify(Math.abs(
                    before.activeHeight - before.spawnedHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.activeX - after.spawnedX
                ) <= tolerance);
                verify(Math.abs(
                    after.activeY - after.spawnedY
                ) <= tolerance);
                verify(Math.abs(
                    after.activeWidth - after.spawnedWidth
                ) <= tolerance);
                verify(Math.abs(
                    after.activeHeight - after.spawnedHeight
                ) <= tolerance);
                verify(Math.abs(
                    after.spawnedScale - 1
                ) <= tolerance);
                compare(after.activeOpacity, 0);
                compare(after.spawnedOpacity, 1);
            }
        }

        compare(stories.length, 4);
    }

    function test_appearanceMotionResetsAcrossStateAndLifecycleChanges() {
        const testWindow = createTemporaryObject(
            appearancePreviewComponent,
            this
        );
        verify(testWindow !== null);
        const preview = testWindow.preview;
        waitForRendering(preview);
        wait(0);

        const toggle = findChild(
            preview,
            "toggleAppearanceMotionButton"
        );
        const summary = findChild(preview, "appearancePreviewSummary");
        verify(toggle !== null);
        verify(summary !== null);
        compare(preview.motionRunning, true);

        preview.motionProgress = 0.72;
        preview.motionPhase = "settled";
        preview.animationsEnabled = false;
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "off");
        compare(preview.motionStatus, "off");
        compare(toggle.enabled, false);
        compare(toggle.text, "Motion off");
        compare(
            toggle.Accessible.name,
            "Illustrative window motion is off"
        );
        verify(String(summary.text).includes("Animations off"));
        verify(String(summary.text).includes("Motion off"));

        preview.synchronizeMotion(false);
        wait(0);
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "off");

        preview.animationsEnabled = true;
        compare(preview.motionRunning, true);
        verify(preview.motionPhase !== "off");

        toggle.clicked();
        compare(preview.motionPaused, true);
        compare(preview.motionRunning, false);
        preview.motionProgress = 0.64;
        preview.motionPhase = "closing";
        preview.layoutMode = "master";
        compare(preview.motionStory, "master-stack");
        compare(preview.motionPaused, true);
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "resting");

        toggle.clicked();
        compare(preview.motionPaused, false);
        compare(preview.motionRunning, true);
        verify(preview.motionPhase !== "closing");

        preview.visible = false;
        compare(preview.motionPaused, false);
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "resting");

        preview.visible = true;
        compare(preview.motionPaused, false);
        compare(preview.motionRunning, true);
        verify(preview.motionPhase !== "off");

        toggle.clicked();
        compare(preview.motionPaused, true);
        preview.motionProgress = 0.48;
        preview.motionPhase = "settled";
        preview.visible = false;
        compare(preview.motionPaused, true);
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "resting");

        preview.visible = true;
        compare(preview.motionPaused, true);
        compare(preview.motionRunning, false);
        compare(preview.motionProgress, 0);
        compare(preview.motionPhase, "resting");
        compare(toggle.text, "Play motion");
        verify(String(summary.text).includes("Motion paused"));
    }

    function test_appearanceCatalogMismatchFailsClosed() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const definitions = appearanceDefinitions();
        definitions[0].max = 21;
        page.appearanceOptions = definitions;
        wait(0);

        compare(page.catalogAvailable, true);
        compare(page.trustedDefinitionsValid, false);
        compare(page.controlsEnabled, false);
        compare(findChild(page, "appearanceBorderSize").enabled, false);
        compare(findChild(page, "saveAppearanceButton").enabled, false);
        const status = findChild(page, "appearanceStatusMessage");
        verify(status !== null);
        compare(status.visible, true);
        verify(String(status.text).includes(
            "trusted Appearance contract does not match"
        ));

        page.appearanceOptions = appearanceDefinitions();
        const invalidValues = appearanceDefaults();
        invalidValues["hyprland.general.border_size"] = 21;
        page.appearanceValues = invalidValues;
        wait(0);
        compare(page.trustedDefinitionsValid, true);
        compare(page.trustedValuesValid, false);
        compare(page.controlsEnabled, false);

        page.appearanceValues = appearanceDefaults();
        page.catalogAvailable = false;
        wait(0);
        compare(page.controlsEnabled, false);
        verify(String(status.text).includes("catalog is unavailable"));
    }

    function test_appearanceDraftActionsAreExplicitAndExact() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const baseline = appearanceDefaults();
        baseline["hyprland.general.border_size"] = 3;
        baseline["hyprland.general.layout"] = "master";
        configureAppearancePage(page, baseline);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.borderSizeId, 7);
        page.setDraftValue(page.roundingId, 5);
        page.setDraftValue(page.blurId, false);
        compare(page.draftDirty, true);

        let saveCount = 0;
        let submitted = null;
        page.saveRequested.connect(function(values) {
            ++saveCount;
            submitted = values;
        });
        const save = findChild(page, "saveAppearanceButton");
        verify(save !== null);
        compare(save.enabled, true);
        save.clicked();
        compare(saveCount, 1);
        verify(submitted !== null);
        compare(Object.keys(submitted).length, 8);
        compare(submitted[page.borderSizeId], 7);
        compare(submitted[page.roundingId], 5);
        compare(submitted[page.blurId], false);
        compare(submitted[page.layoutId], "master");

        // A second submission is blocked until the first request resolves.
        save.clicked();
        compare(saveCount, 1);

        // A fresh page proves Discard and catalog-default reset are local.
        const secondWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(secondWindow !== null);
        const secondPage = secondWindow.page;
        configureAppearancePage(secondPage, baseline);
        waitForRendering(secondPage);
        wait(0);
        secondPage.setDraftValue(secondPage.borderSizeId, 9);
        compare(secondPage.draftDirty, true);
        findChild(secondPage, "discardAppearanceDraftButton").clicked();
        compare(secondPage.draftDirty, false);
        compare(secondPage.draftValue(secondPage.borderSizeId), 3);

        findChild(secondPage, "resetAppearanceDefaultsButton").clicked();
        compare(secondPage.draftDirty, true);
        compare(secondPage.draftValue(secondPage.borderSizeId), 1);
        compare(secondPage.draftValue(secondPage.layoutId), "dwindle");
        compare(secondPage.draftValue(secondPage.blurId), true);
    }

    function test_appearanceExternalRevisionPreservesDraft() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.borderSizeId, 8);
        compare(page.draftDirty, true);
        compare(page.draftValue(page.borderSizeId), 8);

        const newerValues = appearanceDefaults();
        newerValues["hyprland.general.border_size"] = 2;
        newerValues["hyprland.decoration.rounding"] = 4;
        page.appearanceValues = newerValues;
        page.revisionToken = "8";
        page.appliedRevision = 8;
        wait(0);

        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.borderSizeId), 8);
        compare(page.synchronizedRevisionToken, "7");
        compare(findChild(page, "saveAppearanceButton").enabled, false);
        const status = findChild(page, "appearanceStatusMessage");
        verify(String(status.text).includes("draft is preserved"));

        const loadCurrent = findChild(
            page,
            "loadCurrentAppearanceButton"
        );
        verify(loadCurrent !== null);
        compare(loadCurrent.visible, true);
        loadCurrent.clicked();
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.draftValue(page.borderSizeId), 2);
        compare(page.draftValue(page.roundingId), 4);
        compare(page.synchronizedRevisionToken, "8");
    }

    function test_appearanceRevisionTokenRemainsExactAboveJsIntegerLimit() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        page.revisionToken = "9007199254740992";
        page.reviewProjection();
        waitForRendering(page);
        wait(0);
        compare(
            page.synchronizedRevisionToken,
            "9007199254740992"
        );

        page.setDraftValue(page.borderSizeId, 8);
        compare(page.draftDirty, true);

        // An unrelated option changes authority N -> N+1 while the eight
        // projected Appearance values remain byte-for-byte equivalent. These
        // adjacent revisions collapse to one JavaScript Number, so conflict
        // detection must use the client's exact canonical string token.
        page.appearanceValues = appearanceDefaults();
        page.revisionToken = "9007199254740993";
        wait(0);

        compare(page.externalChangeWhileEditing, true);
        compare(page.draftValue(page.borderSizeId), 8);
        compare(
            page.synchronizedRevisionToken,
            "9007199254740992"
        );
        compare(findChild(page, "saveAppearanceButton").enabled, false);
    }

    function test_appearanceSynchronousRejectionKeepsRetryableDraft() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        page.setDraftValue(page.borderSizeId, 6);
        let requestCount = 0;
        page.saveRequested.connect(function() {
            ++requestCount;
            // Model a same-turn client authorization failure: no busy state
            // is entered and the authoritative projection does not change.
            page.errorName = "org.hyprshelld.Error.StaleRevision";
            page.errorMessage = "The compositor revision changed.";
        });

        const save = findChild(page, "saveAppearanceButton");
        verify(save !== null);
        save.clicked();
        compare(requestCount, 1);
        wait(0);
        compare(page.saveSubmitted, false);
        compare(page.draftDirty, true);
        compare(page.draftValue(page.borderSizeId), 6);
        compare(save.enabled, true);
    }

    function test_appearanceRetainedRevisionHasBoundedActions() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        page.appearanceAvailable = false;
        page.appliedRevision = 6;
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.recoveryAvailable = true;
        wait(0);

        let retryCount = 0;
        let recoveryCount = 0;
        page.retryApplyRequested.connect(function() { ++retryCount; });
        page.recoveryRequested.connect(function() { ++recoveryCount; });

        const retry = findChild(page, "retryApplyAppearanceButton");
        const recover = findChild(page, "recoverAppearanceButton");
        const dialog = findChild(page, "appearanceRecoveryDialog");
        const cancel = findChild(page, "cancelAppearanceRecoveryButton");
        const confirm = findChild(page, "confirmAppearanceRecoveryButton");
        const warning = findChild(page, "appearanceRecoveryWarning");
        verify(retry !== null);
        verify(recover !== null);
        verify(dialog !== null);
        verify(cancel !== null);
        verify(confirm !== null);
        verify(warning !== null);
        compare(retry.visible, true);
        compare(recover.visible, true);
        page.busyOperation = "appearance-apply";
        page.busy = true;
        wait(0);
        verify(String(findChild(page, "appearanceStatusMessage").text)
            .includes("Applying and verifying"));
        compare(retry.enabled, false);
        compare(recover.enabled, false);
        page.busy = false;
        page.busyOperation = "";
        wait(0);
        retry.clicked();
        compare(retryCount, 1);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        tryCompare(cancel, "activeFocus", true);
        compare(recoveryCount, 0);
        verify(String(warning.text).includes(
            "not limited to Appearance"
        ));
        verify(String(warning.text).includes("every pending compositor"));
        keyClick(Qt.Key_Escape);
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 0);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        cancel.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 0);

        recover.clicked();
        tryCompare(dialog, "opened", true);
        confirm.clicked();
        tryCompare(dialog, "opened", false);
        compare(recoveryCount, 1);
        compare(confirm.enabled, false);

        // A stale signal cannot bypass the final live eligibility check.
        confirm.clicked();
        compare(recoveryCount, 1);
    }

    function test_appearanceStatusMatrixLocksUnsafeStates() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);
        const status = findChild(page, "appearanceStatusMessage");
        verify(status !== null);

        page.appearanceAvailable = false;
        wait(0);
        compare(status.visible, true);
        verify(String(status.text).includes(
            "waiting for a current, verified compositor baseline"
        ));

        page.managementState = "unmanaged";
        wait(0);
        verify(String(status.text).includes("takeover from Displays"));
        compare(page.controlsEnabled, false);
        const openDisplays = findChild(
            page,
            "appearanceOpenDisplaysButton"
        );
        verify(openDisplays !== null);
        compare(openDisplays.visible, true);
        let routeCount = 0;
        page.openDisplaysRequested.connect(function() { ++routeCount; });
        openDisplays.clicked();
        compare(routeCount, 1);

        page.managementState = "managed";
        page.confirmationState = "awaiting-confirmation";
        wait(0);
        verify(String(status.text).includes("display test is active"));

        page.confirmationState = "idle";
        page.loadState = "recovered";
        page.appearanceAvailable = true;
        wait(0);
        verify(String(status.text).includes("last known good"));
        compare(page.controlsEnabled, true);

        page.loadState = "defaulted";
        wait(0);
        verify(String(status.text).includes("safe desired-state defaults"));

        page.loadState = "normal";
        page.revisionToken = "";
        page.appearanceAvailable = true;
        wait(0);
        verify(String(status.text).includes("exact compositor revision token"));
        compare(page.controlsEnabled, false);

        page.revisionToken = "7";
        page.managementState = "conflict";
        page.appearanceAvailable = false;
        wait(0);
        verify(String(status.text).includes("changed unexpectedly"));
        compare(page.statusIsDanger, true);
    }

    function test_appearanceDangerRevealsWithoutResettingOrdinaryScroll() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const sticky = findChild(page, "appearanceStickyPreview");
        const scroll = findChild(page, "appearanceOptionsScrollView");
        const statusCard = findChild(page, "appearanceStatusCard");
        const status = findChild(page, "appearanceStatusMessage");
        verify(sticky !== null);
        verify(scroll !== null);
        verify(statusCard !== null);
        verify(status !== null);
        compare(page.statusIsDanger, false);

        const maximumContentY = scroll.contentItem.contentHeight
            - scroll.contentItem.height;
        verify(maximumContentY > 0);
        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        const stickyBefore = sticky.mapToItem(page, 0, 0);

        page.setDraftValue(page.borderSizeId, 4);
        wait(0);
        compare(page.draftDirty, true);
        compare(page.statusIsDanger, false);
        compare(scroll.contentItem.contentY, maximumContentY);

        page.loadState = "recovered";
        wait(0);
        compare(page.statusVisible, true);
        compare(page.statusIsDanger, false);
        compare(scroll.contentItem.contentY, maximumContentY);

        page.loadState = "normal";
        wait(0);
        compare(page.statusIsDanger, false);
        compare(scroll.contentItem.contentY, maximumContentY);

        page.managementState = "conflict";
        tryCompare(scroll.contentItem, "contentY", 0);
        compare(page.statusIsDanger, true);
        compare(statusCard.visible, true);
        verify(String(status.text).includes("changed unexpectedly"));
        const statusPosition = statusCard.mapToItem(scroll, 0, 0);
        verify(statusPosition.y >= 0);
        verify(statusPosition.y + statusCard.height <= scroll.height);
        const stickyAfter = sticky.mapToItem(page, 0, 0);
        verify(Math.abs(stickyAfter.x - stickyBefore.x) <= 0.01);
        verify(Math.abs(stickyAfter.y - stickyBefore.y) <= 0.01);

        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        page.managementState = "managed";
        wait(0);
        compare(page.statusIsDanger, false);
        compare(scroll.contentItem.contentY, maximumContentY);

        page.managementState = "conflict";
        page.managementState = "managed";
        wait(0);
        compare(page.statusIsDanger, false);
        compare(scroll.contentItem.contentY, maximumContentY);
    }

    function test_appearancePreviewStaysStickyWhileOptionsScroll() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page);
        waitForRendering(page);
        wait(0);

        const sticky = findChild(page, "appearanceStickyPreview");
        const scroll = findChild(page, "appearanceOptionsScrollView");
        const content = findChild(page, "appearanceOptionsContent");
        const preview = findChild(page, "appearancePreview");
        const motionToggle = findChild(
            page,
            "toggleAppearanceMotionButton"
        );
        verify(sticky !== null);
        verify(scroll !== null);
        verify(content !== null);
        verify(preview !== null);
        verify(motionToggle !== null);
        compare(page.compactPreview, false);
        compare(preview.scale, 1);
        verify(Math.abs(
            preview.width - sticky.availableWidth
        ) <= 0.01);
        verify(Math.abs(
            preview.height - preview.implicitHeight
        ) <= 0.01);
        verify(motionToggle.height >= page.minimumTargetSize);

        const maximumContentY = scroll.contentItem.contentHeight
            - scroll.contentItem.height;
        verify(maximumContentY > 0);
        const stickyBefore = sticky.mapToItem(page, 0, 0);
        const contentBefore = content.mapToItem(page, 0, 0);
        const targetContentY = Math.min(180, maximumContentY);
        verify(targetContentY > 0);

        scroll.contentItem.contentY = targetContentY;
        tryCompare(scroll.contentItem, "contentY", targetContentY);
        const stickyAfter = sticky.mapToItem(page, 0, 0);
        const contentAfter = content.mapToItem(page, 0, 0);
        verify(Math.abs(stickyAfter.x - stickyBefore.x) <= 0.01);
        verify(Math.abs(stickyAfter.y - stickyBefore.y) <= 0.01);
        verify(contentAfter.y < contentBefore.y);
    }

    function test_appearanceActionsReachableAtMinimumWindow() {
        const testWindow = createTemporaryObject(
            appearancePageComponent,
            this,
            { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        configureAppearancePage(page, undefined, true);
        page.setDraftValue(page.blurId, false);
        waitForRendering(page);
        wait(0);

        const sticky = findChild(page, "appearanceStickyPreview");
        const scroll = findChild(page, "appearanceOptionsScrollView");
        const content = findChild(page, "appearanceOptionsContent");
        const save = findChild(page, "saveAppearanceButton");
        const preview = findChild(page, "appearancePreview");
        const refresh = findChild(page, "refreshAppearanceButton");
        const source = findChild(page, "windowBorderSourceButton");
        const resetDefaults = findChild(
            page,
            "resetAppearanceDefaultsButton"
        );
        const motionToggle = findChild(
            page,
            "toggleAppearanceMotionButton"
        );
        verify(sticky !== null);
        verify(scroll !== null);
        verify(content !== null);
        verify(save !== null);
        verify(preview !== null);
        verify(refresh !== null);
        verify(source !== null);
        verify(resetDefaults !== null);
        verify(motionToggle !== null);
        compare(page.compactPreview, true);
        verify(scroll.contentItem.contentHeight > scroll.height);
        verify(scroll.height >= 100);
        verify(scroll.contentWidth <= scroll.availableWidth + 0.01);
        verify(content.x >= 0);
        verify(content.x + content.width <= scroll.contentWidth + 0.01);
        verify(preview.width > 0);
        compare(preview.scale, 1);
        verify(Math.abs(
            preview.width - sticky.availableWidth
        ) <= 0.01);
        verify(Math.abs(
            preview.height - preview.implicitHeight
        ) <= 0.01);
        verify(motionToggle.height >= page.minimumTargetSize);
        verify(source.height >= page.minimumTargetSize);
        verify(resetDefaults.height >= page.minimumTargetSize);

        const stickyBefore = sticky.mapToItem(page, 0, 0);
        const contentBefore = content.mapToItem(page, 0, 0);
        verify(stickyBefore.x >= 0);
        verify(stickyBefore.x + sticky.width <= page.width + 0.01);
        verify(stickyBefore.y >= 0);
        verify(stickyBefore.y + sticky.height <= page.height + 0.01);
        const refreshPosition = refresh.mapToItem(page, 0, 0);
        verify(refreshPosition.x >= 0);
        verify(refreshPosition.x + refresh.width <= page.width);

        const maximumContentY = Math.max(
            0,
            scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        verify(maximumContentY > 0);
        const sourceInContent = source.mapToItem(content, 0, 0);
        const sourceContentY = Math.min(
            maximumContentY,
            Math.max(0, sourceInContent.y - 8)
        );
        scroll.contentItem.contentY = sourceContentY;
        tryCompare(scroll.contentItem, "contentY", sourceContentY);
        const scrollPosition = scroll.mapToItem(page, 0, 0);
        const sourcePosition = source.mapToItem(page, 0, 0);
        verify(sourcePosition.x >= 0);
        verify(sourcePosition.x + source.width <= page.width);
        verify(sourcePosition.y >= scrollPosition.y);
        verify(sourcePosition.y + source.height
            <= scrollPosition.y + scroll.height + 0.01);

        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        const stickyAfter = sticky.mapToItem(page, 0, 0);
        const contentAfter = content.mapToItem(page, 0, 0);
        verify(Math.abs(stickyAfter.x - stickyBefore.x) <= 0.01);
        verify(Math.abs(stickyAfter.y - stickyBefore.y) <= 0.01);
        verify(contentAfter.y < contentBefore.y);
        const savePosition = save.mapToItem(page, 0, 0);
        verify(savePosition.x >= 0);
        verify(savePosition.x + save.width <= page.width);
        verify(savePosition.y >= 0);
        verify(savePosition.y + save.height <= page.height);
    }

    function test_mainNavigationIncludesAppearance() {
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const navigation = findChild(
            application,
            "appearanceNavigationItem"
        );
        const page = findChild(application, "appearancePage");
        verify(navigation !== null);
        verify(page !== null);
        compare(navigation.Accessible.name, "Appearance settings");
        navigation.clicked();
        compare(application.currentPage, "appearance");
        compare(navigation.checked, true);
        compare(page.visible, true);

        // Appearance offers only navigation into the existing confirmed
        // takeover workflow. It never forwards an adoption mutation itself.
        page.openDisplaysRequested();
        compare(application.currentPage, "displays");
        compare(findChild(application, "displaysPage").visible, true);
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
