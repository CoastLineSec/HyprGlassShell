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

    function test_pluginsLoadFromStagedTree() {
        const control = createTemporaryObject(controlComponent, this);
        verify(control !== null);
        compare(control.value, 40);
        compare(control.minimumValue, 24);
        compare(control.maximumValue, 96);
    }
}
