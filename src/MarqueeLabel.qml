import QtQuick
import QtQuick.Controls

// One line of text that elides like a normal label, and while `running`
// slides sideways to show what doesn't fit: a pause, a slow scroll to the
// end, a longer pause, back to the start, again. Text that fits stays put.
Item {
    id: root
    property alias text: label.text
    property alias color: label.color
    property alias font: label.font
    property int horizontalAlignment: Text.AlignLeft
    property bool running: false
    // Pixels per second; slow enough to read while it moves.
    property real speed: 45

    readonly property real overflow: Math.max(0, label.implicitWidth - width)
    readonly property bool scrolling: running && overflow > 1

    implicitHeight: label.implicitHeight
    clip: true

    Label {
        id: label
        width: root.scrolling ? implicitWidth : root.width
        elide: root.scrolling ? Text.ElideNone : Text.ElideRight
        horizontalAlignment: root.scrolling ? Text.AlignLeft : root.horizontalAlignment
    }

    SequentialAnimation {
        running: root.scrolling
        loops: Animation.Infinite
        PauseAnimation { duration: 600 }
        NumberAnimation {
            target: label
            property: "x"
            from: 0
            to: -root.overflow
            duration: Math.max(400, root.overflow / root.speed * 1000)
            easing.type: Easing.InOutSine
        }
        PauseAnimation { duration: 1200 }
        NumberAnimation { target: label; property: "x"; to: 0; duration: 300; easing.type: Easing.OutCubic }
    }

    onScrollingChanged: if (!scrolling) label.x = 0
}
