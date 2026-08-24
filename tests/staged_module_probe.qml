import QtQuick
import QtTest
import HyprShelld.Client
import HyprShelld.UI

TestCase {
    name: "StagedModules"

    Component {
        id: controlComponent

        BarHeightControl {
            value: ConfigClient.barHeight
            minimumValue: ConfigClient.minimumBarHeight
            maximumValue: ConfigClient.maximumBarHeight
            defaultValue: ConfigClient.defaultBarHeight
            busy: ConfigClient.busy
            errorText: ConfigClient.lastErrorMessage
        }
    }

    Component {
        id: runtimeStatusComponent

        ShellRuntimeStatus {
        }
    }

    function test_pluginsLoadFromStagedTree() {
        const control = createTemporaryObject(controlComponent, this);
        verify(control !== null);
        compare(control.value, 40);
        compare(control.minimumValue, 24);
        compare(control.maximumValue, 96);
        compare(CoordinatorClient.healthy, true);
        compare(KeyboardShortcutReferenceModel.available, true);
        compare(KeyboardShortcutReferenceModel.rowCount, 117);
        compare(
            KeyboardShortcutReferenceModel.sourceDigest,
            "47bbde429980d2fa9817c88915cac595ec887573802ed162980613f576b9979d"
        );
        compare(
            KeyboardShortcutReferenceModel.artifactDigest,
            "ee9f5cbe19e4deea91d4640725c14df4153bec6c1e68b11d8770e39c866fc7ba"
        );
        compare(
            KeyboardShortcutReferenceModel.rows[5].chord,
            "SUPER + comma"
        );
        compare(
            KeyboardShortcutReferenceModel.rows[49].action,
            "hl.dsp.focus({ window = \"first\" })"
        );
        compare(
            KeyboardShortcutReferenceModel.rows[104].options.mouse,
            true
        );

        const runtimeStatus = createTemporaryObject(
            runtimeStatusComponent,
            this
        );
        verify(runtimeStatus !== null);
        compare(runtimeStatus.active, false);
        compare(runtimeStatus.available, false);
        compare(runtimeStatus.targetState, "unknown");
    }
}
