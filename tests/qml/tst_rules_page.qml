import QtQuick
import QtQuick.Window
import QtTest
import HyprShelld.Client
import "../../src/settings" as Settings

TestCase {
    name: "RulesPage"
    when: windowShown

    Component {
        id: rulesPageComponent

        Window {
            width: 820
            height: 900
            visible: true

            property alias page: rulesPage

            Settings.RulesPage {
                id: rulesPage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: decimalFieldComponent

        Window {
            width: 320
            height: 100
            visible: true

            property alias field: decimalField

            Settings.RuleDecimalField {
                id: decimalField

                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: 12
                }
            }
        }
    }

    Component {
        id: safeIntegerFieldComponent

        Window {
            width: 360
            height: 140
            visible: true

            property alias field: safeIntegerField

            Settings.RuleSafeIntegerField {
                id: safeIntegerField

                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: 12
                }
                controlObjectName: "testSafeIntegerField"
            }
        }
    }

    Component {
        id: mainComponent

        Settings.Main {
            visible: false
        }
    }

    function clone(value) {
        return JSON.parse(JSON.stringify(value));
    }

    function json(value) {
        return JSON.stringify(value);
    }

    function minimalWindowRule(id, name, enabled) {
        return {
            id,
            name,
            enabled: enabled !== false,
            match: { class: "^org\\.example\\.App$" },
            effects: {
                rounding_power: 1.373,
                border_size: 9007199254740991
            }
        };
    }

    function minimalLayerRule(id, name, enabled) {
        return {
            id,
            name,
            enabled: enabled !== false,
            match: { namespace: "^(waybar|launcher)$" },
            effects: {
                ignore_alpha: 0.437,
                order: -9007199254740991
            }
        };
    }

    function mapOfDefaults(page, definitions) {
        const result = {};
        for (const definition of definitions)
            result[definition.key] = page.clone(definition.defaultValue);
        return result;
    }

    function completeWindowRule(page, id, name) {
        return {
            id,
            name,
            enabled: true,
            match: mapOfDefaults(page, page.windowMatcherDefinitions),
            effects: mapOfDefaults(page, page.windowEffectDefinitions)
        };
    }

    function completeLayerRule(page, id, name) {
        return {
            id,
            name,
            enabled: true,
            match: mapOfDefaults(page, page.layerMatcherDefinitions),
            effects: mapOfDefaults(page, page.layerEffectDefinitions)
        };
    }

    function configureRulesPage(page, windows, layers) {
        page.rulesProjectionAvailable = false;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.rulesAvailable = true;
        page.busy = false;
        page.busyOperation = "";
        page.windowRules = windows || [];
        page.layerRules = layers || [];
        page.revisionToken = "7";
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.rulesErrorName = "";
        page.rulesErrorMessage = "";
        page.sharedErrorName = "";
        page.sharedErrorMessage = "";
        page.retryApplyAvailable = false;
        page.recoveryAvailable = false;
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.rulesProjectionAvailable = true;
        page.reviewProjection();
    }

    function compareMinimumTarget(item, name) {
        verify(item !== null, name + " must exist");
        verify(item.implicitHeight >= 44, name + " must be at least 44px");
    }

    function primaryControlNames(definition) {
        const base = definition.controlObjectName;
        switch (definition.kind) {
        case "vector2": return [base + "X", base + "Y"];
        case "target": return [base + "Target", base + "Silent"];
        case "fullscreenState":
            return [base + "Internal", base + "ClientInclude", base + "Client"];
        case "suppressEvents": return [base + "fullscreen", base + "maximize"];
        case "opacity":
            return [base + "Active", base + "InactiveInclude",
                    base + "Inactive", base + "FullscreenInclude",
                    base + "Fullscreen"];
        case "gradient": return [base + "Color0", base + "Angle"];
        default: return [base];
        }
    }

    function test_exactManagedContractAndCompleteTypedFixture() {
        const testWindow = createTemporaryObject(rulesPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        const expectedWindowMatchers = [
            "class", "title", "initial_class", "initial_title", "tag",
            "xwayland", "float", "fullscreen", "pin", "focus", "group",
            "modal", "fullscreen_state_internal", "fullscreen_state_client",
            "workspace", "content", "xdg_tag", "namespace"
        ];
        const expectedWindowEffects = [
            "float", "tile", "fullscreen", "maximize", "center", "pseudo",
            "no_initial_focus", "pin", "fullscreen_state", "move", "size",
            "monitor", "workspace", "suppress_event", "content",
            "no_close_for", "scrolling_width", "rounding", "border_size",
            "rounding_power", "scroll_mouse", "scroll_touchpad", "animation",
            "idle_inhibit", "opacity", "tag", "max_size", "min_size",
            "border_color", "persistent_size", "allows_input", "dim_around",
            "decorate", "focus_on_activate", "keep_aspect_ratio",
            "nearest_neighbor", "no_anim", "no_blur", "no_dim", "no_focus",
            "no_follow_mouse", "no_max_size", "no_shadow",
            "no_shortcuts_inhibit", "opaque", "force_rgbx",
            "sync_fullscreen", "immediate", "xray", "render_unfocused",
            "no_screen_share", "no_vrr", "no_auto_hdr", "stay_focused",
            "confine_pointer", "tonemap"
        ];
        const expectedLayerMatchers = ["namespace"];
        const expectedLayerEffects = [
            "no_anim", "blur", "blur_popups", "ignore_alpha", "dim_around",
            "xray", "animation", "order", "above_lock", "no_screen_share"
        ];

        compare(json(page.windowMatcherKeys), json(expectedWindowMatchers));
        compare(json(page.windowEffectKeys), json(expectedWindowEffects));
        compare(json(page.layerMatcherKeys), json(expectedLayerMatchers));
        compare(json(page.layerEffectKeys), json(expectedLayerEffects));
        compare(page.windowMatcherDefinitions.length, 18);
        compare(page.windowEffectDefinitions.length, 56);
        compare(page.layerMatcherDefinitions.length, 1);
        compare(page.layerEffectDefinitions.length, 10);
        compare(page.definitionsValid, true);

        const allDefinitions = page.windowMatcherDefinitions
            .concat(page.windowEffectDefinitions)
            .concat(page.layerMatcherDefinitions)
            .concat(page.layerEffectDefinitions);
        const includeNames = new Set();
        const controlNames = new Set();
        for (const definition of allDefinitions) {
            verify(definition.key.length > 0);
            verify(definition.controlObjectName.length > 0);
            compare(
                definition.includeObjectName,
                definition.controlObjectName + "Include"
            );
            verify(!includeNames.has(definition.includeObjectName));
            verify(!controlNames.has(definition.controlObjectName));
            includeNames.add(definition.includeObjectName);
            controlNames.add(definition.controlObjectName);
        }
        compare(includeNames.size, 85);
        compare(controlNames.size, 85);
        verify(!page.windowEffectKeys.includes("exec"));
        verify(!page.windowEffectKeys.includes("group"));
        verify(!page.layerEffectKeys.includes("exec"));
        verify(!page.layerEffectKeys.includes("workspace"));

        const windowRule = completeWindowRule(
            page, "window-rule-complete", "Complete window rule"
        );
        const layerRule = completeLayerRule(
            page, "layer-rule-complete", "Complete layer rule"
        );
        compare(page.validateRuleRecord(windowRule, "window"), true);
        compare(page.validateRuleRecord(layerRule, "layer"), true);
        compare(page.validateRuleCollection([windowRule], "window"), true);
        compare(page.validateRuleCollection([layerRule], "layer"), true);
        configureRulesPage(page, [windowRule], [layerRule]);
        tryCompare(page, "projectionInitialized", true);
        compare(page.trustedValuesValid, true);
        compare(page.draftValid, true);
        compare(page.controlsEnabled, true);

        for (const name of [
            "rulesTabs", "windowRulesTab", "layerRulesTab",
            "windowRulesList", "layerRulesList", "addWindowRuleButton",
            "addLayerRuleButton"
        ]) {
            verify(findChild(page, name) !== null, name);
        }
        compareMinimumTarget(findChild(page, "windowRulesTab"), "windowRulesTab");
        compareMinimumTarget(findChild(page, "layerRulesTab"), "layerRulesTab");
        compareMinimumTarget(
            findChild(page, "addWindowRuleButton"), "addWindowRuleButton"
        );
        compareMinimumTarget(
            findChild(page, "addLayerRuleButton"), "addLayerRuleButton"
        );
        verify(findChild(page, "workspaceRulesTab") === null);
        verify(findChild(page, "rulesLayoutPreview") === null);
        verify(findChild(page, "rulesStickyPreview") === null);
    }

    function test_editorInstantiatesEveryManagedFieldAndPreservesExactNumbers() {
        const testWindow = createTemporaryObject(rulesPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        const windowRule = completeWindowRule(
            page, "window-rule-all-fields", "All window fields"
        );
        const layerRule = completeLayerRule(
            page, "layer-rule-all-fields", "All layer fields"
        );
        windowRule.effects.rounding_power = 1.373;
        windowRule.effects.scrolling_width = 0.437;
        windowRule.effects.border_size = 9007199254740991;
        layerRule.effects.ignore_alpha = 0.313;
        layerRule.effects.order = -9007199254740991;
        configureRulesPage(page, [windowRule], [layerRule]);
        tryCompare(page, "projectionInitialized", true);

        page.openRule("window", windowRule.id);
        tryCompare(page, "editorActive", true);
        for (const definition of page.windowMatcherDefinitions
                .concat(page.windowEffectDefinitions)) {
            const include = findChild(page, definition.includeObjectName);
            compareMinimumTarget(include, definition.includeObjectName);
            compare(include.checked, true);
            for (const name of primaryControlNames(definition))
                verify(findChild(page, name) !== null, name);
        }

        const decimal = findChild(page, "windowEffectRoundingPower");
        page.forceActiveFocus();
        decimal.synchronizeText();
        compare(decimal.text, "1.373");
        compare(decimal.inputValid, true);
        decimal.forceActiveFocus();
        decimal.text = "1.337";
        decimal.textEdited();
        compare(
            page.ruleById("window", windowRule.id).effects.rounding_power,
            1.337
        );
        compare(page.draftValid, true);
        decimal.text = "1e0";
        decimal.textEdited();
        compare(decimal.text, "1e0");
        compare(decimal.inputValid, false);
        compare(
            page.ruleById("window", windowRule.id).effects.rounding_power,
            "1e0"
        );
        compare(page.draftValid, false);
        compare(page.saveEnabled, false);
        decimal.text = "1.337";
        decimal.textEdited();
        compare(page.draftValid, true);

        const fullscreenClient = findChild(
            page, "windowEffectFullscreenStateClient"
        );
        const fullscreenClientInclude = findChild(
            page, "windowEffectFullscreenStateClientInclude"
        );
        compare(fullscreenClient.enabled, false);
        fullscreenClientInclude.checked = true;
        fullscreenClientInclude.clicked();
        compare(
            page.ruleById("window", windowRule.id)
                .effects.fullscreen_state.client,
            0
        );
        compare(fullscreenClient.enabled, true);

        const moveX = findChild(page, "windowEffectMoveX");
        moveX.forceActiveFocus();
        moveX.text = "12.375";
        moveX.textEdited();
        compare(
            page.ruleById("window", windowRule.id).effects.move[0],
            12.375
        );
        const monitorTarget = findChild(page, "windowEffectMonitorTarget");
        monitorTarget.text = "DP-1";
        monitorTarget.textEdited();
        const monitorSilent = findChild(page, "windowEffectMonitorSilent");
        monitorSilent.checked = true;
        monitorSilent.clicked();
        compare(
            json(page.ruleById("window", windowRule.id).effects.monitor),
            json({ target: "DP-1", silent: true })
        );

        const suppressMaximize = findChild(
            page, "windowEffectSuppressEventmaximize"
        );
        suppressMaximize.checked = true;
        suppressMaximize.clicked();
        verify(page.ruleById("window", windowRule.id)
            .effects.suppress_event.includes("maximize"));
        const animation = findChild(page, "windowEffectAnimation");
        const animationDirection = findChild(
            page, "windowEffectAnimationDirection"
        );
        animation.currentIndex = 1;
        animation.activated(1);
        compare(
            page.ruleById("window", windowRule.id).effects.animation,
            "slide"
        );
        compare(animationDirection.enabled, true);
        animationDirection.currentIndex = 1;
        animationDirection.activated(1);
        compare(
            page.ruleById("window", windowRule.id).effects.animation,
            "slide top"
        );

        const opacityInactiveInclude = findChild(
            page, "windowEffectOpacityInactiveInclude"
        );
        const opacityInactive = findChild(
            page, "windowEffectOpacityInactive"
        );
        compare(opacityInactive.enabled, false);
        opacityInactiveInclude.checked = true;
        opacityInactiveInclude.clicked();
        compare(opacityInactive.enabled, true);
        opacityInactive.forceActiveFocus();
        opacityInactive.text = "0.619";
        opacityInactive.textEdited();
        compare(
            page.ruleById("window", windowRule.id).effects.opacity.inactive,
            0.619
        );
        const addGradientColor = findChild(
            page, "windowEffectBorderColorAddColor"
        );
        addGradientColor.clicked();
        compare(
            page.ruleById("window", windowRule.id)
                .effects.border_color.colors.length,
            2
        );
        const gradientAngle = findChild(
            page, "windowEffectBorderColorAngle"
        );
        gradientAngle.forceActiveFocus();
        gradientAngle.text = "37.125";
        gradientAngle.textEdited();
        compare(
            page.ruleById("window", windowRule.id).effects.border_color.angle,
            37.125
        );
        compare(page.draftValid, true);

        const borderSize = findChild(page, "windowEffectBorderSize");
        borderSize.forceActiveFocus();
        borderSize.text = "9007199254740991";
        borderSize.textEdited();
        compare(
            page.ruleById("window", windowRule.id).effects.border_size,
            9007199254740991
        );
        for (const invalid of [
            "+1", "01", "-0", "1.0", "1e3", "9007199254740992"
        ]) {
            borderSize.text = invalid;
            borderSize.textEdited();
            compare(
                page.ruleById("window", windowRule.id).effects.border_size,
                invalid
            );
            compare(page.draftValid, false);
        }
        borderSize.text = "-9007199254740991";
        borderSize.textEdited();
        compare(
            page.ruleById("window", windowRule.id).effects.border_size,
            -9007199254740991
        );
        compare(page.draftValid, true);

        page.openRule("layer", layerRule.id);
        wait(0);
        for (const definition of page.layerMatcherDefinitions
                .concat(page.layerEffectDefinitions)) {
            const include = findChild(page, definition.includeObjectName);
            compareMinimumTarget(include, definition.includeObjectName);
            compare(include.checked, true);
            for (const name of primaryControlNames(definition))
                verify(findChild(page, name) !== null, name);
        }
        const ignoreAlpha = findChild(page, "layerEffectIgnoreAlpha");
        page.forceActiveFocus();
        ignoreAlpha.synchronizeText();
        compare(ignoreAlpha.value, 0.313);
        compare(ignoreAlpha.text, "0.313");
        const layerOrder = findChild(page, "layerEffectOrder");
        layerOrder.parent.synchronizeText();
        compare(layerOrder.parent.value, -9007199254740991);
        compare(layerOrder.text, "-9007199254740991");
    }

    function test_localCollectionValidationRejectsDuplicatesEmptyRecordsAnd4097() {
        const testWindow = createTemporaryObject(rulesPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        const first = minimalWindowRule("window-rule-one", "Window one");
        const duplicateId = minimalWindowRule("window-rule-one", "Window two");
        const duplicateName = minimalWindowRule("window-rule-two", "Window one");
        compare(page.validateRuleCollection([first, duplicateId], "window"), false);
        compare(page.validateRuleCollection([first, duplicateName], "window"), false);

        const emptyMatch = clone(first);
        emptyMatch.id = "window-rule-empty-match";
        emptyMatch.name = "Empty match";
        emptyMatch.match = {};
        compare(page.validateRuleRecord(emptyMatch, "window"), false);
        const emptyEffects = clone(first);
        emptyEffects.id = "window-rule-empty-effects";
        emptyEffects.name = "Empty effects";
        emptyEffects.effects = {};
        compare(page.validateRuleRecord(emptyEffects, "window"), false);
        const workspaceRuleShape = {
            id: "workspace-rule-not-supported",
            selector: "1",
            enabled: true,
            monitor: "",
            persistent: false,
            isDefault: false,
            layout: "",
            overrides: {}
        };
        compare(page.validateRuleRecord(workspaceRuleShape, "window"), false);

        const maximum = [];
        for (let index = 0; index < 4096; ++index) {
            maximum.push(minimalWindowRule(
                "window-rule-limit-" + index,
                "Window limit " + index
            ));
        }
        compare(page.validateRuleCollection(maximum, "window"), true);
        maximum.push(minimalWindowRule(
            "window-rule-limit-4096", "Window limit 4096"
        ));
        compare(page.validateRuleCollection(maximum, "window"), false);
    }

    function test_numberAndSafeIntegerRowsUseExactAuthoredTextContracts() {
        const decimalWindow = createTemporaryObject(decimalFieldComponent, this);
        verify(decimalWindow !== null);
        const decimal = decimalWindow.field;
        decimal.minimumValue = -10;
        decimal.maximumValue = 10;
        compare(decimal.parseDecimal("0.373"), 0.373);
        compare(decimal.parseDecimal("-9.999"), -9.999);
        compare(decimal.parseDecimal("10"), 10);
        compare(decimal.parseDecimal(".5"), null);
        compare(decimal.parseDecimal("1."), null);
        compare(decimal.parseDecimal("01"), null);
        compare(decimal.parseDecimal("1e0"), null);
        compare(decimal.parseDecimal(" 1"), null);
        compare(decimal.parseDecimal("10.001"), null);
        decimal.value = "1e0";
        compare(decimal.projectedText, "1e0");
        compare(decimal.inputValid, false);

        const integerWindow = createTemporaryObject(
            safeIntegerFieldComponent, this
        );
        verify(integerWindow !== null);
        const integer = integerWindow.field;
        compare(integer.parseCanonicalInteger("0"), 0);
        compare(
            integer.parseCanonicalInteger("9007199254740991"),
            9007199254740991
        );
        compare(
            integer.parseCanonicalInteger("-9007199254740991"),
            -9007199254740991
        );
        for (const invalid of [
            "", "+1", "01", "-0", "1.0", "1e3", " 1", "1 ",
            "9007199254740992", "-9007199254740992"
        ]) {
            compare(integer.parseCanonicalInteger(invalid), null, invalid);
        }
        integer.value = "1e3";
        compare(integer.projectedText, "1e3");
        compare(integer.inputValid, false);
        compareMinimumTarget(
            findChild(integer, "testSafeIntegerField"),
            "testSafeIntegerField"
        );
    }

    function test_aggregateDraftOrderingAtomicSaveDiscardAndReset() {
        const testWindow = createTemporaryObject(rulesPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        const windows = [
            minimalWindowRule("window-rule-a", "Window A"),
            minimalWindowRule("window-rule-b", "Window B")
        ];
        const layers = [
            minimalLayerRule("layer-rule-a", "Layer A"),
            minimalLayerRule("layer-rule-b", "Layer B")
        ];
        configureRulesPage(page, windows, layers);
        tryCompare(page, "projectionInitialized", true);

        page.moveRule("window", "window-rule-b", -1);
        page.setRuleProperty("window", "window-rule-b", "name", "Window B renamed");
        page.setRuleProperty("window", "window-rule-b", "enabled", false);
        compare(page.draftWindowRules[0].id, "window-rule-b");
        compare(page.draftWindowRules[0].name, "Window B renamed");
        compare(page.draftWindowRules[0].enabled, false);
        page.moveRule("layer", "layer-rule-a", 1);
        compare(page.draftLayerRules[1].id, "layer-rule-a");

        page.addRule("window");
        const generatedWindowId = page.editingRuleId;
        verify(/^window-rule-[1-9][0-9]*$/.test(generatedWindowId));
        compare(page.draftValid, false);
        page.setRuleField(
            "window", generatedWindowId, "match", "class", true, "^new$"
        );
        page.setRuleField(
            "window", generatedWindowId, "effects", "float", true, true
        );
        compare(page.draftValid, true);
        page.closeEditor();

        page.addRule("layer");
        const generatedLayerId = page.editingRuleId;
        verify(/^layer-rule-[1-9][0-9]*$/.test(generatedLayerId));
        page.setRuleField(
            "layer", generatedLayerId, "match", "namespace", true, "^new$"
        );
        page.setRuleField(
            "layer", generatedLayerId, "effects", "blur", true, true
        );
        page.closeEditor();
        compare(page.draftValid, true);
        compare(page.draftDirty, true);

        let saveCalls = 0;
        let savedWindows = null;
        let savedLayers = null;
        page.saveRequested.connect(function(windowRules, layerRules) {
            ++saveCalls;
            savedWindows = clone(windowRules);
            savedLayers = clone(layerRules);
        });
        page.submitDraft();
        compare(saveCalls, 1);
        compare(json(savedWindows), json(page.draftWindowRules));
        compare(json(savedLayers), json(page.draftLayerRules));
        verify(savedWindows.some(rule => rule.id === generatedWindowId));
        verify(savedLayers.some(rule => rule.id === generatedLayerId));

        page.rulesAvailable = false;
        page.windowRules = savedWindows;
        page.layerRules = savedLayers;
        page.revisionToken = "8";
        page.reviewProjection();
        tryCompare(page, "saveSubmitted", false);
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.controlsEnabled, false);

        page.rulesAvailable = true;
        page.setRuleProperty("window", generatedWindowId, "name", "Edited again");
        compare(page.draftDirty, true);
        page.synchronizeDraft();
        compare(page.draftDirty, false);
        compare(
            page.ruleById("window", generatedWindowId).name,
            page.windowRules.find(rule => rule.id === generatedWindowId).name
        );
        page.resetDraftToDefaults();
        compare(page.draftWindowRules.length, 0);
        compare(page.draftLayerRules.length, 0);
        compare(page.draftValid, true);
        compare(page.draftDirty, true);
        compare(page.saveEnabled, true);
    }

    function test_invalidRe2IsSubmittedForTrustedValidationAndDraftIsRetained() {
        const testWindow = createTemporaryObject(rulesPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        const rule = minimalWindowRule("window-rule-regex", "Regex rule");
        configureRulesPage(page, [rule], []);
        tryCompare(page, "projectionInitialized", true);
        page.setRuleField("window", rule.id, "match", "class", true, "[");
        compare(page.draftValid, true);
        compare(page.saveEnabled, true);

        let submitted = null;
        page.saveRequested.connect(function(windowRules, layerRules) {
            submitted = { windows: clone(windowRules), layers: clone(layerRules) };
        });
        page.submitDraft();
        verify(submitted !== null);
        compare(submitted.windows[0].match.class, "[");
        compare(submitted.layers.length, 0);

        page.rulesErrorName = "org.hyprshelld.Client.Compositor.Error.InvalidRules";
        page.rulesErrorMessage = "Window Rule Regex rule has invalid RE2 syntax.";
        page.reviewProjection();
        tryCompare(page, "saveSubmitted", false);
        compare(page.draftWindowRules[0].match.class, "[");
        compare(page.draftDirty, true);
        compare(page.saveEnabled, true);
        verify(String(page.statusMessage).includes("Rules operation failed"));
        verify(String(page.statusMessage).includes("invalid RE2 syntax"));
    }

    function test_conflictAndOwnRetainedApplyHaveDistinctReconciliation() {
        const testWindow = createTemporaryObject(rulesPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        const original = minimalWindowRule("window-rule-conflict", "Original");
        configureRulesPage(page, [original], []);
        tryCompare(page, "projectionInitialized", true);
        page.setRuleProperty("window", original.id, "name", "Local draft");
        page.rulesAvailable = false;
        page.rulesProjectionAvailable = false;
        page.windowRules = [];
        page.revisionToken = "8";
        page.reviewProjection();
        tryCompare(page, "externalChangeWhileEditing", true);
        compare(page.draftWindowRules[0].name, "Local draft");
        const loadCurrent = findChild(page, "loadCurrentRulesButton");
        compare(loadCurrent.visible, true);
        compare(loadCurrent.enabled, false);

        const external = minimalWindowRule(
            "window-rule-external", "External current"
        );
        page.windowRules = [external];
        page.rulesProjectionAvailable = true;
        wait(0);
        compare(loadCurrent.enabled, true);
        loadCurrent.clicked();
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftWindowRules[0].id, external.id);
        compare(page.draftDirty, false);

        page.rulesAvailable = true;
        page.setRuleProperty("window", external.id, "name", "Own saved value");
        let submittedWindows = null;
        page.saveRequested.connect(function(windowRules, layerRules) {
            submittedWindows = clone(windowRules);
        });
        page.submitDraft();
        verify(submittedWindows !== null);
        page.busy = true;
        page.rulesAvailable = false;
        page.windowRules = submittedWindows;
        page.revisionToken = "9";
        page.reviewProjection();
        compare(page.saveSubmitted, true);
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.busy = false;
        page.reviewProjection();
        tryCompare(page, "saveSubmitted", false);
        compare(page.externalChangeWhileEditing, false);
        compare(page.draftDirty, false);
        compare(page.draftWindowRules[0].name, "Own saved value");
        const retry = findChild(page, "retryApplyRulesButton");
        compare(retry.visible, true);
        compare(retry.enabled, true);
    }

    function test_authorityOperationGatesAndRecoveryAreTruthfulAndScoped() {
        const testWindow = createTemporaryObject(rulesPageComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        const rule = minimalWindowRule("window-rule-gates", "Gate rule");
        configureRulesPage(page, [rule], []);
        tryCompare(page, "projectionInitialized", true);
        compare(page.controlsEnabled, true);

        const booleanGates = [
            "serviceAvailable", "writable", "catalogAvailable",
            "rulesAvailable", "rulesProjectionAvailable", "sharedApplySafe"
        ];
        for (const property of booleanGates) {
            const original = page[property];
            page[property] = false;
            compare(page.controlsEnabled, false, property);
            page[property] = original;
        }
        page.busy = true;
        compare(page.controlsEnabled, false);
        page.busy = false;
        page.sharedMutationBusy = true;
        compare(page.controlsEnabled, false);
        page.sharedMutationBusy = false;
        page.confirmationState = "pending";
        compare(page.controlsEnabled, false);
        page.confirmationState = "idle";
        page.managementState = "preview";
        compare(page.controlsEnabled, false);
        page.managementState = "managed";
        page.revisionToken = "01";
        compare(page.controlsEnabled, false);
        page.revisionToken = "7";
        compare(page.controlsEnabled, true);

        page.rulesAvailable = false;
        page.rulesProjectionAvailable = false;
        page.rulesErrorName = "org.hyprshelld.Client.Compositor.Error.Authority";
        page.rulesErrorMessage = "Action and schema verification failed.";
        verify(String(page.statusMessage).includes("authority verification failed"));
        verify(String(page.statusMessage).includes("Action and schema verification failed"));
        verify(!String(page.statusMessage).includes("operation failed"));
        verify(!String(page.statusMessage).includes("service may be restarting"));
        compare(page.statusIsDanger, true);

        page.rulesProjectionAvailable = true;
        page.rulesAvailable = false;
        page.rulesErrorName = "";
        page.rulesErrorMessage = "";
        page.applyState = "retained";
        page.requiredActivation = "reload";
        page.retryApplyAvailable = true;
        page.recoveryAvailable = true;
        page.sharedApplySafe = true;
        const retry = findChild(page, "retryApplyRulesButton");
        const recover = findChild(page, "recoverRulesButton");
        const dialog = findChild(page, "rulesRecoveryDialog");
        const warning = findChild(page, "rulesRecoveryWarning");
        const cancel = findChild(page, "cancelRulesRecoveryButton");
        const confirm = findChild(page, "confirmRulesRecoveryButton");
        for (const item of [retry, recover, cancel, confirm])
            compareMinimumTarget(item, item ? item.objectName : "missing");
        verify(dialog !== null);
        verify(warning !== null);
        compare(retry.visible, true);
        compare(retry.enabled, true);
        compare(recover.visible, true);

        let recoveryCalls = 0;
        page.recoveryRequested.connect(function() { ++recoveryCalls; });
        recover.clicked();
        tryCompare(dialog, "opened", true);
        verify(String(warning.text).includes("every pending compositor setting"));
        confirm.clicked();
        compare(recoveryCalls, 1);
    }

    function test_compactEditorAndListsRemainReachableAt423By480() {
        const testWindow = createTemporaryObject(
            rulesPageComponent, this, { width: 423, height: 480 }
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        const windowRule = completeWindowRule(
            page, "window-rule-compact", "Compact complete rule"
        );
        configureRulesPage(page, [windowRule], []);
        tryCompare(page, "projectionInitialized", true);
        compare(page.compactPage, true);
        page.openRule("window", windowRule.id);
        tryCompare(page, "editorActive", true);
        waitForRendering(page);
        wait(0);
        tryCompare(page, "width", 423);

        const scroll = findChild(page, "rulesEditorScrollView");
        const content = findChild(page, "rulesEditorContent");
        const close = findChild(page, "closeRuleEditorButton");
        const done = findChild(page, "doneEditingRuleButton");
        const remove = findChild(page, "removeEditedRuleButton");
        verify(scroll !== null);
        verify(content !== null);
        compare(scroll.contentWidth, scroll.availableWidth);
        verify(content.width <= scroll.availableWidth + 0.01);
        verify(scroll.contentItem.contentHeight > scroll.contentItem.height);
        for (const item of [close, done, remove])
            compareMinimumTarget(item, item ? item.objectName : "missing");

        const maximumContentY = Math.max(
            0, scroll.contentItem.contentHeight - scroll.contentItem.height
        );
        scroll.contentItem.contentY = maximumContentY;
        tryCompare(scroll.contentItem, "contentY", maximumContentY);
        const donePosition = done.mapToItem(page, 0, 0);
        verify(donePosition.x >= 0);
        verify(
            donePosition.x + done.width <= page.width + 0.01,
            "done x=" + donePosition.x + " width=" + done.width
                + " page=" + page.width
        );
        verify(donePosition.y >= 0);
        verify(donePosition.y + done.height <= page.height + 0.01);
    }

    function test_mainHasOneRulesDestinationBadgeAndExactErrorScope() {
        CompositorClient.clearError();
        const application = createTemporaryObject(mainComponent, this);
        verify(application !== null);
        const page = findChild(application, "rulesPage");
        const workspacesNavigation = findChild(
            application, "workspacesNavigationItem"
        );
        const rulesNavigation = findChild(application, "rulesNavigationItem");
        const componentsNavigation = findChild(
            application, "componentsNavigationItem"
        );
        const badge = findChild(application, "rulesNavigationBadge");
        verify(page !== null);
        verify(workspacesNavigation !== null);
        verify(rulesNavigation !== null);
        verify(componentsNavigation !== null);
        verify(badge !== null);
        verify(rulesNavigation.y > workspacesNavigation.y);
        verify(componentsNavigation.y > rulesNavigation.y);
        verify(rulesNavigation.height >= 44);
        compare(
            rulesNavigation.Accessible.name,
            "Window and Layer Rules settings"
        );

        const rule = minimalWindowRule("window-rule-main", "Main draft");
        configureRulesPage(page, [rule], []);
        tryCompare(page, "projectionInitialized", true);
        page.setRuleProperty("window", rule.id, "name", "Off-page edit");
        compare(application.rulesDraftNavigationState, "dirty");
        compare(badge.visible, true);
        compare(badge.text, "Unsaved");
        verify(String(rulesNavigation.Accessible.description)
            .includes("unsaved changes"));

        page.rulesAvailable = false;
        page.rulesProjectionAvailable = false;
        page.revisionToken = "8";
        page.reviewProjection();
        tryCompare(page, "externalChangeWhileEditing", true);
        compare(application.rulesDraftNavigationState, "conflict");
        compare(badge.text, "Review");
        verify(String(rulesNavigation.Accessible.description)
            .includes("conflicts with a newer compositor revision"));

        for (const operation of [
            "rules-apply", "compositor-apply", "recover"
        ]) {
            compare(application.rulesCompositorError(operation), true);
        }
        for (const operation of [
            "appearance-apply", "input-apply", "windows-apply",
            "workspaces-apply", "adopt", "display-preview",
            "display-confirm", "display-revert", "display-refresh",
            "shared-border-sync", "", "unknown"
        ]) {
            compare(application.rulesCompositorError(operation), false);
        }

        CompositorClient.clearError();
        const filteringApplication = createTemporaryObject(mainComponent, this);
        verify(filteringApplication !== null);
        const filteringPage = findChild(filteringApplication, "rulesPage");
        verify(filteringPage !== null);
        try {
            CompositorClient.previewDisplayConfiguration([], 15);
            compare(CompositorClient.lastErrorOperation, "display-preview");
            compare(filteringPage.sharedErrorName, "");
            compare(filteringPage.sharedErrorMessage, "");
            CompositorClient.applyConfiguration();
            compare(CompositorClient.lastErrorOperation, "compositor-apply");
            tryCompare(
                filteringPage,
                "sharedErrorName",
                CompositorClient.lastErrorName
            );
            compare(
                filteringPage.sharedErrorMessage,
                CompositorClient.lastErrorMessage
            );
        } finally {
            CompositorClient.clearError();
        }
        compare(filteringPage.sharedErrorName, "");
        compare(filteringPage.sharedErrorMessage, "");
    }
}
