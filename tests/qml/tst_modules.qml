import QtQuick
import QtQuick.Window
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

    Component {
        id: barWindowComponent

        Window {
            width: 800
            height: 100
            visible: true

            property alias bar: testBar

            Bar {
                id: testBar

                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                }
                barHeight: 40
                currentTime: new Date(2026, 7, 7, 15, 42)
                screenName: "DP-2"
                configurationAvailable: true
            }
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

    function test_barOnlyShowsHealthIndicatorWhenDegraded() {
        const testWindow = createTemporaryObject(barWindowComponent, this);
        verify(testWindow !== null);
        const bar = testWindow.bar;
        verify(bar !== null);
        waitForRendering(bar);

        const configurationIndicator = findChild(
            bar,
            "configurationStatusIndicator"
        );
        const indicator = findChild(bar, "shellHealthIndicator");
        const indicatorGlyph = findChild(
            bar,
            "shellHealthIndicatorGlyph"
        );
        verify(configurationIndicator !== null);
        verify(indicator !== null);
        verify(indicatorGlyph !== null);
        compare(configurationIndicator.visible, true);
        compare(indicator.visible, false);
        compare(indicatorGlyph.Accessible.ignored, true);

        bar.healthSummary = "A HyprShelld component needs attention.";
        bar.shellDegraded = true;
        compare(configurationIndicator.visible, true);
        compare(indicator.visible, true);
        compare(bar.height, 40);

        bar.shellDegraded = false;
        compare(indicator.visible, false);
        compare(bar.height, 40);
    }

    function test_failureNoticeTemporarilyReplacesClock() {
        const testWindow = createTemporaryObject(barWindowComponent, this);
        verify(testWindow !== null);
        const bar = testWindow.bar;
        verify(bar !== null);
        waitForRendering(bar);

        const clock = findChild(bar, "clockLabel");
        const notice = findChild(bar, "failureNotice");
        const noticeLabel = findChild(bar, "failureNoticeLabel");
        verify(clock !== null);
        verify(notice !== null);
        verify(noticeLabel !== null);
        compare(noticeLabel.Accessible.ignored, true);
        compare(clock.visible, true);
        compare(notice.visible, false);

        bar.failureNoticeText = "A HyprShelld component needs attention.";
        bar.failureNoticeVisible = true;
        compare(clock.visible, false);
        compare(notice.visible, true);
        verify(noticeLabel.text.includes(bar.failureNoticeText));
        compare(bar.height, 40);

        bar.failureNoticeVisible = false;
        compare(clock.visible, true);
        compare(notice.visible, false);
        compare(bar.height, 40);
    }
}
