import QtQuick
import QtTest
import HyprShelld.Client
import HyprShelld.UI

TestCase {
    name: "SharedQmlModules"

    Component {
        id: controlComponent

        BarHeightControl {
            value: 40
            minimumValue: ConfigClient.minimumBarHeight
            maximumValue: ConfigClient.maximumBarHeight
            defaultValue: ConfigClient.defaultBarHeight
        }
    }

    function test_clientConstants() {
        compare(ConfigClient.minimumBarHeight, 24);
        compare(ConfigClient.maximumBarHeight, 96);
        compare(ConfigClient.defaultBarHeight, 40);
    }

    function test_controlInstantiation() {
        const control = createTemporaryObject(controlComponent, this);
        verify(control !== null);
        compare(control.value, 40);
        compare(control.minimumValue, 24);
        compare(control.maximumValue, 96);
        compare(control.defaultValue, 40);
        compare(control.busy, false);
        compare(control.errorText, "");
    }
}
