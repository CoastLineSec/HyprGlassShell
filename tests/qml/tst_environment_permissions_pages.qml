import QtQuick
import QtQuick.Window
import QtTest
import "../../src/settings" as Settings

TestCase {
    name: "EnvironmentPermissionsPages"
    when: windowShown

    Component {
        id: environmentPageComponent

        Window {
            width: 820
            height: 1000
            visible: true

            property alias page: environmentPage
            property alias saveSpy: environmentSaveSpy

            Settings.EnvironmentVariablesPage {
                id: environmentPage

                anchors.fill: parent
            }

            SignalSpy {
                id: environmentSaveSpy

                target: environmentPage
                signalName: "saveRequested"
            }
        }
    }

    Component {
        id: permissionsPageComponent

        Window {
            width: 820
            height: 1000
            visible: true

            property alias page: permissionsPage
            property alias saveSpy: permissionsSaveSpy

            Settings.PermissionsPage {
                id: permissionsPage

                anchors.fill: parent
            }

            SignalSpy {
                id: permissionsSaveSpy

                target: permissionsPage
                signalName: "saveRequested"
            }
        }
    }

    function json(value) {
        return JSON.stringify(value);
    }

    function environmentRecord(id, name, value, scope) {
        return {
            id: id,
            name: name,
            value: value === undefined ? "" : value,
            scope: scope || "hyprland"
        };
    }

    function permissionRecord(id, binary, type, mode) {
        return {
            id: id,
            binary: binary,
            type: type || "screencopy",
            mode: mode || "ask"
        };
    }

    function configureEnvironment(page, records) {
        page.environmentProjectionAvailable = false;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.environmentAvailable = true;
        page.busy = false;
        page.busyOperation = "";
        page.environmentVariables = records || [];
        page.revisionToken = "7";
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.environmentErrorName = "";
        page.environmentErrorMessage = "";
        page.sharedErrorName = "";
        page.sharedErrorMessage = "";
        page.retryApplyAvailable = false;
        page.recoveryAvailable = false;
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.environmentProjectionAvailable = true;
        page.reviewProjection();
    }

    function configurePermissions(page, records) {
        page.permissionsProjectionAvailable = false;
        page.serviceAvailable = true;
        page.writable = true;
        page.catalogAvailable = true;
        page.permissionsAvailable = true;
        page.busy = false;
        page.busyOperation = "";
        page.permissions = records || [];
        page.revisionToken = "7";
        page.loadState = "normal";
        page.managementState = "managed";
        page.applyState = "current";
        page.requiredActivation = "none";
        page.confirmationState = "idle";
        page.permissionErrorName = "";
        page.permissionErrorMessage = "";
        page.sharedErrorName = "";
        page.sharedErrorMessage = "";
        page.retryApplyAvailable = false;
        page.recoveryAvailable = false;
        page.sharedMutationBusy = false;
        page.sharedApplySafe = true;
        page.permissionsProjectionAvailable = true;
        page.reviewProjection();
    }

    function compareMinimumTarget(item, name) {
        verify(item !== null, name + " must exist");
        verify(item.implicitHeight >= 44, name + " must be at least 44px");
    }

    function test_environmentExactV1ShapeAndBounds() {
        const window = createTemporaryObject(environmentPageComponent, this);
        verify(window !== null);
        const page = window.page;
        const valid = environmentRecord(
            "environment.cursor", "XCURSOR_SIZE", "24", "hyprland"
        );

        compare(page.validateEnvironmentRecord(valid), true);
        compare(page.validateEnvironmentCollection([valid]), true);
        compare(page.validateEnvironmentRecord({
            id: valid.id,
            name: valid.name,
            value: valid.value,
            scope: valid.scope,
            extra: true
        }), false);
        compare(page.validateEnvironmentRecord(environmentRecord(
            "-bad", "XCURSOR_SIZE", "24", "hyprland"
        )), false);
        compare(page.validateEnvironmentRecord(environmentRecord(
            "environment.bad-name", "2BAD", "24", "hyprland"
        )), false);
        compare(page.validateEnvironmentRecord(environmentRecord(
            "environment.bad-value", "GOOD", "bad\u0000value", "hyprland"
        )), false);
        compare(page.validateEnvironmentRecord(environmentRecord(
            "environment.bad-scope", "GOOD", "24", "shell"
        )), false);
        compare(page.validateEnvironmentRecord(environmentRecord(
            "environment.uwsm", "MOZ_ENABLE_WAYLAND", "1", "uwsm"
        )), true);
        compare(page.validateEnvironmentCollection([
            valid,
            environmentRecord("environment.duplicate", "XCURSOR_SIZE", "32")
        ]), false);
        compare(page.validateEnvironmentCollection([
            valid,
            environmentRecord(valid.id, "ANOTHER_NAME", "32")
        ]), false);

        const maximum = [];
        for (let index = 0; index < 512; ++index) {
            maximum.push(environmentRecord(
                "environment.limit-" + index,
                "LIMIT_" + index,
                String(index)
            ));
        }
        compare(page.validateEnvironmentCollection(maximum), true);
        maximum.push(environmentRecord(
            "environment.limit-512", "LIMIT_512", "512"
        ));
        compare(page.validateEnvironmentCollection(maximum), false);
    }

    function test_permissionExactV1ShapeEnumsAndIdentity() {
        const window = createTemporaryObject(permissionsPageComponent, this);
        verify(window !== null);
        const page = window.page;
        const valid = permissionRecord(
            "permission.portal",
            "^/usr/lib/xdg-desktop-portal(?:-[a-z]+)?$",
            "screencopy",
            "ask"
        );

        compare(page.validatePermissionRecord(valid), true);
        compare(page.validatePermissionCollection([valid]), true);
        compare(page.validatePermissionRecord({
            id: valid.id,
            binary: valid.binary,
            type: valid.type,
            mode: valid.mode,
            enabled: true
        }), false);
        compare(page.validatePermissionRecord(permissionRecord(
            "permission.empty", "", "screencopy", "ask"
        )), false);
        compare(page.validatePermissionRecord(permissionRecord(
            "permission.control", "^/usr/bin/a\n$", "screencopy", "ask"
        )), false);
        compare(page.validatePermissionRecord(permissionRecord(
            "permission.bad-type", ".*", "clipboard", "ask"
        )), false);
        compare(page.validatePermissionRecord(permissionRecord(
            "permission.bad-mode", ".*", "plugin", "prompt"
        )), false);
        for (const type of [
            "screencopy", "cursorpos", "plugin", "keyboard", "input-capture"
        ]) {
            compare(page.validatePermissionRecord(permissionRecord(
                "permission." + type, "^/usr/bin/" + type + "$", type, "deny"
            )), true, type);
        }
        for (const mode of ["ask", "allow", "deny"]) {
            compare(page.validatePermissionRecord(permissionRecord(
                "permission." + mode, "^/usr/bin/" + mode + "$",
                "plugin", mode
            )), true, mode);
        }
        compare(page.validatePermissionCollection([
            valid,
            permissionRecord("permission.duplicate", valid.binary,
                valid.type, "deny")
        ]), false);
        compare(page.validatePermissionCollection([
            valid,
            permissionRecord(valid.id, "^/usr/bin/other$", "plugin", "deny")
        ]), false);

        // UI checks the exact persisted string envelope; the trusted backend
        // performs authoritative RE2 compilation before the collection is saved.
        compare(page.validPermissionBinary("["), true);
    }

    function test_environmentDraftOrderEditRemoveResetAndAtomicSave() {
        const window = createTemporaryObject(environmentPageComponent, this);
        verify(window !== null);
        const page = window.page;
        configureEnvironment(page, [
            environmentRecord("environment.a", "A", "one"),
            environmentRecord("environment.b", "B", "two", "uwsm")
        ]);
        tryCompare(page, "projectionInitialized", true);
        compare(page.controlsEnabled, true);
        compare(page.draftHasUnavailableUwsm, true);
        compare(page.saveEnabled, false);

        page.moveVariable("environment.b", -1);
        compare(page.draftEnvironmentVariables[0].id, "environment.b");
        page.setVariableField("environment.b", "name", "B_RENAMED");
        page.setVariableField("environment.b", "value", "updated");
        page.setVariableField("environment.b", "scope", "hyprland");
        compare(page.draftHasUnavailableUwsm, false);
        compare(page.draftEnvironmentVariables[0].name, "B_RENAMED");
        compare(page.environmentExample(page.draftEnvironmentVariables[0]),
            "hl.env(\"B_RENAMED\", \"updated\")");
        page.addVariable();
        verify(/^environment-[1-9][0-9]*$/.test(page.editingVariableId));
        compare(page.draftEnvironmentVariables.length, 3);
        compare(page.validateEnvironmentCollection(
            page.draftEnvironmentVariables), true);
        page.removeVariable("environment.a");
        compare(page.draftEnvironmentVariables.length, 2);
        compare(page.saveEnabled, true);

        page.submitDraft();
        compare(window.saveSpy.count, 1);
        compare(json(window.saveSpy.signalArguments[0][0]),
            json(page.submittedEnvironmentVariables));

        page.saveSubmitted = false;
        page.externalChangeWhileEditing = false;
        page.resetDraftToDefaults();
        compare(page.draftEnvironmentVariables.length, 0);
        compare(page.draftValid, true);
        compare(page.saveEnabled, true);
    }

    function test_permissionDraftOrderEditRemoveResetAndAtomicSave() {
        const window = createTemporaryObject(permissionsPageComponent, this);
        verify(window !== null);
        const page = window.page;
        configurePermissions(page, [
            permissionRecord("permission.a", "^/usr/bin/a$", "plugin", "ask"),
            permissionRecord("permission.b", "^/usr/bin/b$", "keyboard", "deny")
        ]);
        tryCompare(page, "projectionInitialized", true);
        compare(page.controlsEnabled, true);

        page.movePermission("permission.b", -1);
        compare(page.draftPermissions[0].id, "permission.b");
        page.setPermissionField("permission.b", "binary", "^/opt/bin/b$");
        page.setPermissionField("permission.b", "type", "input-capture");
        page.setPermissionField("permission.b", "mode", "allow");
        compare(page.draftPermissions[0].binary, "^/opt/bin/b$");
        verify(page.permissionRisk(page.draftPermissions[0])
            .indexOf("High risk") >= 0);
        compare(page.permissionExample(page.draftPermissions[0]),
            "hl.permission(\"^/opt/bin/b$\", \"input-capture\", \"allow\")");
        page.addPermission();
        verify(/^permission-[1-9][0-9]*$/.test(page.editingPermissionId));
        compare(page.draftPermissions.length, 3);
        compare(page.validatePermissionCollection(page.draftPermissions), true);
        page.removePermission("permission.a");
        compare(page.draftPermissions.length, 2);
        compare(page.saveEnabled, true);

        page.submitDraft();
        compare(window.saveSpy.count, 1);
        compare(json(window.saveSpy.signalArguments[0][0]),
            json(page.submittedPermissions));

        page.saveSubmitted = false;
        page.externalChangeWhileEditing = false;
        page.resetDraftToDefaults();
        compare(page.draftPermissions.length, 0);
        compare(page.draftValid, true);
        compare(page.saveEnabled, true);
    }

    function test_externalRevisionConflictPreservesBothDrafts() {
        const environmentWindow = createTemporaryObject(
            environmentPageComponent, this
        );
        verify(environmentWindow !== null);
        const environmentPage = environmentWindow.page;
        configureEnvironment(environmentPage, [
            environmentRecord("environment.a", "A", "old")
        ]);
        tryCompare(environmentPage, "projectionInitialized", true);
        environmentPage.setVariableField("environment.a", "value", "draft");
        environmentPage.environmentVariables = [
            environmentRecord("environment.a", "A", "external")
        ];
        environmentPage.revisionToken = "8";
        environmentPage.reviewProjection();
        compare(environmentPage.externalChangeWhileEditing, true);
        compare(environmentPage.draftEnvironmentVariables[0].value, "draft");
        compare(environmentPage.controlsEnabled, false);
        environmentPage.synchronizeDraft();
        compare(environmentPage.externalChangeWhileEditing, false);
        compare(environmentPage.draftEnvironmentVariables[0].value, "external");

        const permissionsWindow = createTemporaryObject(
            permissionsPageComponent, this
        );
        verify(permissionsWindow !== null);
        const permissionsPage = permissionsWindow.page;
        configurePermissions(permissionsPage, [
            permissionRecord("permission.a", "^a$", "plugin", "ask")
        ]);
        tryCompare(permissionsPage, "projectionInitialized", true);
        permissionsPage.setPermissionField("permission.a", "mode", "deny");
        permissionsPage.permissions = [
            permissionRecord("permission.a", "^a$", "plugin", "allow")
        ];
        permissionsPage.revisionToken = "8";
        permissionsPage.reviewProjection();
        compare(permissionsPage.externalChangeWhileEditing, true);
        compare(permissionsPage.draftPermissions[0].mode, "deny");
        compare(permissionsPage.controlsEnabled, false);
        permissionsPage.synchronizeDraft();
        compare(permissionsPage.externalChangeWhileEditing, false);
        compare(permissionsPage.draftPermissions[0].mode, "allow");
    }

    function test_visualExplanationsTypedControlsAndTargetSizes() {
        const environmentWindow = createTemporaryObject(
            environmentPageComponent, this
        );
        verify(environmentWindow !== null);
        const environmentPage = environmentWindow.page;
        configureEnvironment(environmentPage, [
            environmentRecord("environment.cursor", "XCURSOR_SIZE", "24")
        ]);
        tryCompare(environmentPage, "projectionInitialized", true);
        environmentPage.openVariable("environment.cursor");
        wait(0);
        for (const name of [
            "environmentOwnershipPreview", "environmentCodeExample",
            "environmentVariableNameField0", "environmentVariableValueField0",
            "environmentVariableScopeSelect0"
        ]) {
            verify(findChild(environmentPage, name) !== null, name);
        }
        for (const name of [
            "refreshEnvironmentButton", "addEnvironmentVariableButton",
            "editEnvironmentVariableButton0", "moveEnvironmentVariableDownButton0",
            "removeEnvironmentVariableButton0", "environmentVariableScopeSelect0",
            "doneEditingEnvironmentVariableButton0", "saveEnvironmentButton"
        ]) {
            compareMinimumTarget(findChild(environmentPage, name), name);
        }
        environmentPage.applyState = "retained";
        environmentPage.requiredActivation = "session";
        verify(environmentPage.statusMessage.indexOf("new session") >= 0);

        const permissionsWindow = createTemporaryObject(
            permissionsPageComponent, this
        );
        verify(permissionsWindow !== null);
        const permissionsPage = permissionsWindow.page;
        configurePermissions(permissionsPage, [
            permissionRecord("permission.portal", "^/usr/bin/portal$",
                "screencopy", "ask")
        ]);
        tryCompare(permissionsPage, "projectionInitialized", true);
        permissionsPage.openPermission("permission.portal");
        wait(0);
        for (const name of [
            "permissionPolicyPreview", "permissionRiskExplanation",
            "permissionCodeExample", "permissionSafetyCard",
            "permissionBinaryField0", "permissionTypeSelect0",
            "permissionModeSelect0"
        ]) {
            verify(findChild(permissionsPage, name) !== null, name);
        }
        for (const name of [
            "refreshPermissionsButton", "addPermissionButton",
            "editPermissionButton0", "movePermissionDownButton0",
            "removePermissionButton0", "permissionTypeSelect0",
            "permissionModeSelect0", "doneEditingPermissionButton0",
            "savePermissionsButton"
        ]) {
            compareMinimumTarget(findChild(permissionsPage, name), name);
        }
        permissionsPage.applyState = "retained";
        permissionsPage.requiredActivation = "restart";
        verify(permissionsPage.statusMessage.indexOf("restarted") >= 0);
    }
}
