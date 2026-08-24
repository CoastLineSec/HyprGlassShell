import QtQuick
import QtQuick.Window
import QtTest
import "../../src/settings" as Settings

TestCase {
    name: "DisplaysMonitorFields"
    when: windowShown

    Component {
        id: displaysComponent

        Window {
            width: 860
            height: 900
            visible: true

            property alias page: displaysPage

            Settings.DisplaysPage {
                id: displaysPage

                anchors.fill: parent
            }
        }
    }

    function monitorRecord() {
        return {
            id: "display-DP-1",
            selector: "DP-1",
            enabled: true,
            mode: "maxwidth",
            position: "auto-center-down",
            scale: 1.333333,
            reserved: [1, 2, 3, 4],
            transform: 7,
            mirror: "",
            bitdepth: 10,
            cm: "hdr",
            sdrEotf: "gamma22force",
            sdrBrightness: 1.25,
            sdrSaturation: 0.9,
            vrr: 3,
            icc: "/profiles/display-p3.icc",
            supportsWideColor: 1,
            supportsHdr: 1,
            sdrMinLuminance: 0.1,
            sdrMaxLuminance: 203,
            minLuminance: 0.005,
            maxLuminance: 1000,
            maxAvgLuminance: 400
        };
    }

    function connectedDisplay() {
        return {
            selector: "DP-1",
            description: "Pinned monitor test display",
            make: "Example",
            model: "Panel",
            serial: "DP-1-serial",
            enabled: true,
            width: 1920,
            height: 1080,
            physicalWidthMm: 520,
            physicalHeightMm: 290,
            refreshRate: 60,
            x: 0,
            y: 0,
            scale: 1,
            transform: 0,
            focused: true,
            dpms: true,
            vrrActive: false,
            mirrorOf: "",
            modes: [
                {
                    width: 1920,
                    height: 1080,
                    refreshRate: 60,
                    managedMode: "1920x1080@60"
                }
            ],
            colorManagement: "srgb",
            currentFormat: "XRGB2101010",
            sdrBrightness: 1,
            sdrSaturation: 1,
            sdrMinLuminance: 0.2,
            sdrMaxLuminance: 80
        };
    }

    function configuredPage() {
        const window = createTemporaryObject(displaysComponent, this);
        verify(window !== null);
        const page = window.page;
        page.serviceAvailable = true;
        page.writable = true;
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.revision = 7;
        page.appliedRevision = 7;
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.snapshot = {
            monitors: [monitorRecord()]
        };
        page.connectedDisplays = [connectedDisplay()];
        page.topologyDigest = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        page.synchronizeDraft(false);
        waitForRendering(page);
        wait(0);
        return page;
    }

    function test_allPinnedMonitorFieldsHaveTypedControls() {
        const page = configuredPage();
        compare(page.draftValidationMessage, "");
        const card = findChild(page, "displaySettingsCard");
        const advanced = findChild(page, "displayAdvancedButton");
        verify(card !== null);
        verify(advanced !== null);
        verify(card.availableModes.some(choice => choice.value === "maxwidth"));
        compare(card.automaticPositionChoices.length, 9);
        advanced.checked = true;
        wait(0);

        const expectedControls = ["displayAutomaticPositionComboBox", "displayExactScaleField", "displayReservedTopField", "displayReservedRightField", "displayReservedBottomField", "displayReservedLeftField", "displaySdrEotfComboBox", "displaySdrBrightnessField", "displaySdrSaturationField", "displaySdrMinimumLuminanceField", "displaySdrMaximumLuminanceSpinBox", "displayMinimumLuminanceField", "displayMaximumLuminanceSpinBox", "displayMaximumAverageLuminanceSpinBox"];
        for (const objectName of expectedControls)
            verify(findChild(page, objectName) !== null, objectName);

        compare(findChild(page, "displayAutomaticPositionComboBox").currentValue, "auto-center-down");
        compare(findChild(page, "displaySdrEotfComboBox").currentIndex, 4);
        compare(findChild(page, "displaySdrMaximumLuminanceSpinBox").value, 203);
        compare(findChild(page, "displayMaximumLuminanceSpinBox").value, 1000);
        compare(findChild(page, "displayMaximumAverageLuminanceSpinBox").value, 400);
        compare(findChild(page, "displayReservedTopField").text, "1");
        compare(findChild(page, "displayReservedRightField").text, "2");
        compare(findChild(page, "displayReservedBottomField").text, "3");
        compare(findChild(page, "displayReservedLeftField").text, "4");
    }

    function test_everyNewFieldUpdatesThePreviewPayload() {
        const page = configuredPage();
        const card = findChild(page, "displaySettingsCard");
        verify(card !== null);

        card.updateField("mode", "highrr");
        card.updateField("position", "auto-center-left");
        card.updateField("scale", 1.5);
        card.updateReservedPart(0, 11);
        card.updateReservedPart(1, 12);
        card.updateReservedPart(2, 13);
        card.updateReservedPart(3, 14);
        card.updateField("sdrEotf", "srgb");
        card.updateField("sdrBrightness", 1.5);
        card.updateField("sdrSaturation", 1.1);
        card.updateField("sdrMinLuminance", 0.2);
        card.updateField("sdrMaxLuminance", 250);
        card.updateField("minLuminance", 0.01);
        card.updateField("maxLuminance", 1200);
        card.updateField("maxAvgLuminance", 500);
        wait(0);

        const output = page.draftOutputs[0];
        compare(output.mode, "highrr");
        compare(output.position, "auto-center-left");
        compare(output.scale, 1.5);
        compare(output.reserved, [11, 12, 13, 14]);
        compare(output.sdrEotf, "srgb");
        compare(output.sdrBrightness, 1.5);
        compare(output.sdrSaturation, 1.1);
        compare(output.sdrMinLuminance, 0.2);
        compare(output.sdrMaxLuminance, 250);
        compare(output.minLuminance, 0.01);
        compare(output.maxLuminance, 1200);
        compare(output.maxAvgLuminance, 500);
        compare(page.draftValidationMessage, "");
        compare(page.previewEnabled, true);

        let requestedOutputs = null;
        let requestedTimeout = 0;
        page.previewRequested.connect(function (outputs, timeoutSeconds) {
            requestedOutputs = outputs;
            requestedTimeout = timeoutSeconds;
        });
        const preview = findChild(page, "previewDisplayConfigurationButton");
        verify(preview !== null);
        compare(preview.enabled, true);
        preview.clicked();
        compare(requestedTimeout, 15);
        compare(requestedOutputs.length, 1);
        compare(requestedOutputs[0], output);
    }

    function test_monitorSchemaViolationsBlockPreview() {
        const page = configuredPage();
        const valid = monitorRecord();
        const invalidValues = [
            {
                field: "mode",
                value: "3840x2160@0"
            },
            {
                field: "position",
                value: "1000001x0"
            },
            {
                field: "scale",
                value: 0.24
            },
            {
                field: "reserved",
                value: [0, 0.5, 0, 0]
            },
            {
                field: "sdrEotf",
                value: "pq"
            },
            {
                field: "sdrBrightness",
                value: 10.1
            },
            {
                field: "sdrSaturation",
                value: -0.1
            },
            {
                field: "icc",
                value: "/profiles/hidden\u200bformat.icc"
            },
            {
                field: "sdrMinLuminance",
                value: 10000.1
            },
            {
                field: "sdrMaxLuminance",
                value: 2147483648
            },
            {
                field: "minLuminance",
                value: -1.1
            },
            {
                field: "maxLuminance",
                value: 2147483648
            },
            {
                field: "maxAvgLuminance",
                value: 1.5
            }
        ];

        for (const row of invalidValues) {
            const candidate = page.clone(valid);
            candidate[row.field] = row.value;
            page.replaceOutput(candidate);
            verify(page.draftValidationMessage.length > 0, row.field);
            compare(page.previewEnabled, false, row.field);
        }
    }
}
