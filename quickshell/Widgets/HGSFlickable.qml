import QtQuick
import QtQuick.Controls
import qs.Widgets
import "ScrollConstants.js" as Scroll

Flickable {
    id: flickable

    property alias verticalScrollBar: vbar
    property real mouseWheelSpeed: Scroll.mouseWheelSpeed
    property real momentumVelocity: 0
    property bool isMomentumActive: false
    property real friction: Scroll.friction
    property bool _scrollBarActive: false
    // When true, the vertical scrollbar never auto-hides (stays visible whenever
    // there is content to scroll). Default on — auto-hiding scrollbars tested poorly.
    property bool persistentScrollBar: true

    flickDeceleration: Scroll.flickDeceleration
    maximumFlickVelocity: Scroll.maximumFlickVelocity
    boundsBehavior: Flickable.StopAtBounds
    boundsMovement: Flickable.FollowBoundsBehavior
    pressDelay: 0
    flickableDirection: Flickable.VerticalFlick
    // Only interactive (drag + native wheel) when there is actually something to
    // scroll. When content fits, going non-interactive lets the wheel pass through to
    // a parent flickable — fixing nested/stacked scroll (e.g. the Network page).
    interactive: flickable.contentHeight > flickable.height + 0.5

    WheelHandler {
        id: wheelHandler

        property real touchpadSpeed: Scroll.touchpadSpeed
        property real momentumRetention: Scroll.momentumRetention
        property real lastWheelTime: 0
        property real momentum: 0
        property var velocitySamples: []
        property bool sessionUsedMouseWheel: false

        function startMomentum() {
            flickable.isMomentumActive = true;
            momentumAnim.running = true;
        }

        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        // Disabled when there's nothing to scroll, so the wheel reaches the parent
        // flickable instead of being trapped here.
        enabled: flickable.contentHeight > flickable.height + 0.5

        onWheel: event => {
            vbar._scrollBarActive = true;
            vbar.hideTimer.restart();

            const currentTime = Date.now();
            const timeDelta = currentTime - lastWheelTime;
            lastWheelTime = currentTime;

            const hasPixel = event.pixelDelta && event.pixelDelta.y !== 0;
            const deltaY = event.angleDelta.y;
            const isTraditionalMouse = !hasPixel && Math.abs(deltaY) >= 120 && (Math.abs(deltaY) % 120) === 0;
            const isHighDpiMouse = !hasPixel && !isTraditionalMouse && deltaY !== 0;
            const isTouchpad = hasPixel;

            if (isTraditionalMouse) {
                sessionUsedMouseWheel = true;
                momentumAnim.running = false;
                flickable.isMomentumActive = false;
                velocitySamples = [];
                momentum = 0;
                flickable.momentumVelocity = 0;

                const lines = Math.round(Math.abs(deltaY) / 120);
                const scrollAmount = (deltaY > 0 ? -lines : lines) * flickable.mouseWheelSpeed;
                let newY = flickable.contentY + scrollAmount;
                newY = Math.max(0, Math.min(flickable.contentHeight - flickable.height, newY));

                if (flickable.flicking) {
                    flickable.cancelFlick();
                }

                flickable.contentY = newY;
            } else if (isHighDpiMouse) {
                sessionUsedMouseWheel = true;
                momentumAnim.running = false;
                flickable.isMomentumActive = false;
                velocitySamples = [];
                momentum = 0;
                flickable.momentumVelocity = 0;

                let delta = deltaY / 8 * touchpadSpeed;
                let newY = flickable.contentY - delta;
                newY = Math.max(0, Math.min(flickable.contentHeight - flickable.height, newY));

                if (flickable.flicking) {
                    flickable.cancelFlick();
                }

                flickable.contentY = newY;
            } else if (isTouchpad) {
                sessionUsedMouseWheel = false;
                momentumAnim.running = false;
                flickable.isMomentumActive = false;

                let delta = event.pixelDelta.y * touchpadSpeed;

                velocitySamples.push({
                    "delta": delta,
                    "time": currentTime
                });
                velocitySamples = velocitySamples.filter(s => currentTime - s.time < Scroll.velocitySampleWindowMs);

                if (velocitySamples.length > 1) {
                    const totalDelta = velocitySamples.reduce((sum, s) => sum + s.delta, 0);
                    const timeSpan = currentTime - velocitySamples[0].time;
                    if (timeSpan > 0) {
                        flickable.momentumVelocity = Math.max(-Scroll.maxMomentumVelocity, Math.min(Scroll.maxMomentumVelocity, totalDelta / timeSpan * 1000));
                    }
                }

                if (timeDelta < Scroll.momentumTimeThreshold) {
                    momentum = momentum * momentumRetention + delta * Scroll.momentumDeltaFactor;
                    delta += momentum;
                } else {
                    momentum = 0;
                }

                let newY = flickable.contentY - delta;
                newY = Math.max(0, Math.min(flickable.contentHeight - flickable.height, newY));

                if (flickable.flicking) {
                    flickable.cancelFlick();
                }

                flickable.contentY = newY;
            }

            event.accepted = true;
        }

        onActiveChanged: {
            if (!active) {
                if (!sessionUsedMouseWheel && Math.abs(flickable.momentumVelocity) >= Scroll.minMomentumVelocity) {
                    startMomentum();
                } else {
                    velocitySamples = [];
                    flickable.momentumVelocity = 0;
                }
            }
        }
    }

    onMovementStarted: {
        vbar._scrollBarActive = true;
        vbar.hideTimer.stop();
    }
    onMovementEnded: vbar.hideTimer.restart()

    FrameAnimation {
        id: momentumAnim
        running: false

        onTriggered: {
            const dt = frameTime;
            const newY = flickable.contentY - flickable.momentumVelocity * dt;
            const maxY = Math.max(0, flickable.contentHeight - flickable.height);

            if (newY < 0 || newY > maxY) {
                flickable.contentY = newY < 0 ? 0 : maxY;
                running = false;
                flickable.isMomentumActive = false;
                flickable.momentumVelocity = 0;
                return;
            }

            flickable.contentY = newY;
            flickable.momentumVelocity *= Math.pow(flickable.friction, dt / 0.016);

            if (Math.abs(flickable.momentumVelocity) < Scroll.momentumStopThreshold) {
                running = false;
                flickable.isMomentumActive = false;
                flickable.momentumVelocity = 0;
            }
        }
    }

    ScrollBar.vertical: HGSScrollbar {
        id: vbar
        alwaysVisible: flickable.persistentScrollBar
    }
}
