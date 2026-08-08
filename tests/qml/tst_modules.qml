import QtQuick
import QtTest
import HyprShelld.Client
import HyprShelld.UI

TestCase {
    name: "SharedQmlModules"
    when: windowShown

    Component {
        id: controlComponent

        BarHeightControl {
            value: 40
            minimumValue: ConfigClient.minimumBarHeight
            maximumValue: ConfigClient.maximumBarHeight
            defaultValue: ConfigClient.defaultBarHeight
        }
    }

    Component {
        id: barComponent

        Bar {
            width: 800
            barHeight: 40
            currentTime: new Date(2026, 7, 7, 15, 42)
            screenName: "DP-2"
            configurationAvailable: true
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
        compare(control.previewValue, 40);
        compare(control.adjusting, false);

        const slider = findChild(control, "barHeightSlider");
        verify(slider !== null);
        slider.value = 64;
        compare(control.previewValue, 64);
    }

    function test_barFollowsConfiguredHeight() {
        const bar = createTemporaryObject(barComponent, this);
        verify(bar !== null);
        compare(bar.height, 40);
        compare(bar.cornerRadius, 15);

        bar.barHeight = 24;
        compare(bar.height, 24);
        compare(bar.cornerRadius, 9);

        bar.barHeight = 96;
        compare(bar.height, 96);
        compare(bar.cornerRadius, 16);
    }

}
