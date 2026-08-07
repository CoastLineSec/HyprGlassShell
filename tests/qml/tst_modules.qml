import QtQuick
import QtTest
import HyprShelld.Client
import HyprShelld.UI

TestCase {
    name: "SharedQmlModules"

    Component {
        id: controlComponent

        BarHeightControl {
            value: 48
            minimumValue: ConfigClient.minimumBarHeight
            maximumValue: ConfigClient.maximumBarHeight
            defaultValue: ConfigClient.defaultBarHeight
        }
    }

    function test_clientConstants() {
        compare(ConfigClient.minimumBarHeight, 32);
        compare(ConfigClient.maximumBarHeight, 96);
        compare(ConfigClient.defaultBarHeight, 48);
    }

    function test_controlInstantiation() {
        const control = createTemporaryObject(controlComponent, this);
        verify(control !== null);
        compare(control.value, 48);
        compare(control.minimumValue, 32);
        compare(control.maximumValue, 96);
        compare(control.defaultValue, 48);
        compare(control.busy, false);
        compare(control.errorText, "");
    }
}
