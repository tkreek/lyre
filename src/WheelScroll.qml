import QtQuick

// Scrolling that keeps up with the rest of the desktop. Qt's lists move by
// exactly the pixels the compositor reports and stop dead when the fingers
// lift, which feels sluggish next to browsers and GTK apps: Hyprland scales
// touchpad scrolling down (Omarchy sets scroll_factor = 0.4), and those apps
// scale it back up and coast afterwards. This does the same for the list it's
// declared in: touchpad deltas are multiplied, lifting the fingers flicks
// with the speed the content was moving, and a mouse wheel notch moves a
// few rows.
WheelHandler {
    id: wheel
    required property Flickable flickable
    // How much faster than the compositor's pixels a touchpad scrolls.
    property real touchpadSpeed: 2.5
    // How far one mouse-wheel notch scrolls.
    property real notchStep: Math.round(theme.fontSize * 9)

    property real velocity: 0
    property double lastEvent: 0
    property bool touchpad: false

    target: null
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

    function scrollBy(dy) {
        const f = flickable
        const top = f.originY - f.topMargin
        const bottom = Math.max(top, f.originY + f.contentHeight + f.bottomMargin - f.height)
        f.contentY = Math.max(top, Math.min(bottom, f.contentY - dy))
    }

    onWheel: (event) => {
        const f = flickable
        const now = Date.now()
        const pixels = event.pixelDelta.y !== 0 || event.angleDelta.y === 0
        f.cancelFlick()
        if (pixels) {
            const dy = event.pixelDelta.y * touchpadSpeed
            scrollBy(dy)
            // Smoothed speed of the last few events, in pixels per second,
            // for the coast when the fingers lift.
            const dt = Math.max(1, now - lastEvent)
            const instant = dt > 100 ? 0 : dy * 1000 / dt
            velocity = velocity * 0.4 + instant * 0.6
            touchpad = true
            coast.restart()
        } else {
            touchpad = false
            velocity = 0
            scrollBy(event.angleDelta.y / 120 * notchStep)
        }
        lastEvent = now
    }

    // Touchpads send no event when the fingers lift, only silence; a short
    // gap means they've lifted, so carry on at the speed they left off.
    property Timer coast: Timer {
        interval: 45
        onTriggered: {
            if (wheel.touchpad && Math.abs(wheel.velocity) > 150)
                wheel.flickable.flick(0, Math.max(-8000, Math.min(8000, wheel.velocity)))
            wheel.velocity = 0
        }
    }
}
