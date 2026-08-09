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
        const scrollView = findChild(page, "barSettingsScrollView");
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

    function test_minimumSizeCanReachWorkspaceReset() {
        const page = createTemporaryObject(pageComponent, this, {
            width: 423,
            height: 480
        });
        verify(page !== null);
        const scrollView = findChild(page, "barSettingsScrollView");
        const reset = findChild(page, "resetWorkspaceSwitcher");
        verify(scrollView !== null);
        verify(reset !== null);
        waitForRendering(page);
        wait(0);
        verify(scrollView.contentItem.contentHeight > scrollView.height);

        scrollView.contentItem.contentY = Math.max(
            0,
            scrollView.contentItem.contentHeight
                - scrollView.contentItem.height
        );
        wait(0);
        const resetPosition = reset.mapToItem(page, 0, 0);
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
