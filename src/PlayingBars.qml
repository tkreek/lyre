import QtQuick

// Three bars that bounce while the track plays and rest when it's paused,
// marking the playing row in a list.
Row {
    id: bars
    property bool running: player.playing
    property color color: theme.accent
    spacing: 2
    height: 12

    Repeater {
        model: 3
        Rectangle {
            id: bar
            required property int index
            width: 3
            anchors.bottom: parent.bottom
            color: bars.color
            height: bars.height * 0.4
            SequentialAnimation on height {
                running: bars.running
                loops: Animation.Infinite
                NumberAnimation { to: bars.height; duration: 260 + bar.index * 90; easing.type: Easing.InOutQuad }
                NumberAnimation { to: bars.height * 0.25; duration: 300 + bar.index * 70; easing.type: Easing.InOutQuad }
            }
        }
    }
}
