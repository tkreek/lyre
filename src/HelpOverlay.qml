import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every shortcut, shown with ?.
Popup {
    id: help
    anchors.centerIn: Overlay.overlay
    width: Math.min(parent.width - 48, 640)
    modal: true
    focus: true
    // Esc is the window's Go back shortcut, which closes this first.
    closePolicy: Popup.CloseOnPressOutside
    padding: 28
    Material.roundedScale: theme.radius

    Overlay.modal: Rectangle { color: Qt.alpha("black", 0.55) }

    background: Rectangle {
        color: theme.surface
        radius: theme.radius
        border.color: theme.selection
        border.width: 1
    }

    readonly property var shortcuts: [
        [ "Space", qsTr("Play or pause") ],
        [ "N / P", qsTr("Next or previous track") ],
        [ "Shift+← / →", qsTr("Back or forward 10 seconds") ],
        [ "+ / −", qsTr("Volume up or down") ],
        [ "M", qsTr("Mute") ],
        [ "S", qsTr("Shuffle") ],
        [ "R", qsTr("Repeat: off, queue, track") ],
        [ "Enter", qsTr("Open, or play from the selected track") ],
        [ "Shift+Enter", qsTr("Play the selected album without opening it") ],
        [ "A / Shift+A", qsTr("Add to the queue, or play next") ],
        [ "Delete", qsTr("Remove from the queue") ],
        [ "Alt+J / Alt+K", qsTr("Move down or up in the queue") ],
        [ "H J K L", qsTr("Move around, like the arrow keys") ],
        [ "Esc", qsTr("Go back") ],
        [ "1 – 5", qsTr("Albums, Artists, Playlists, Queue, Now Playing") ],
        [ "G", qsTr("Go to the playing album") ],
        [ "/", qsTr("Search") ],
        [ "Ctrl+R", qsTr("Reload the library") ],
        [ "?", qsTr("Show or hide this list") ],
        [ "Q", qsTr("Quit") ]
    ]

    contentItem: ColumnLayout {
        spacing: 14
        Label {
            text: qsTr("Keyboard shortcuts")
            font.pixelSize: Math.round(theme.fontSize * 1.3)
            font.weight: Font.Bold
            color: theme.foreground
        }
        GridLayout {
            columns: 2
            columnSpacing: 24
            rowSpacing: 7
            Repeater {
                model: help.shortcuts
                delegate: Label {
                    required property var modelData
                    required property int index
                    Layout.row: index
                    Layout.column: 0
                    text: modelData[0]
                    color: theme.accent
                    font.weight: Font.DemiBold
                }
            }
            Repeater {
                model: help.shortcuts
                delegate: Label {
                    required property var modelData
                    required property int index
                    Layout.row: index
                    Layout.column: 1
                    Layout.fillWidth: true
                    text: modelData[1]
                    wrapMode: Text.WordWrap
                    color: theme.foreground
                }
            }
        }
    }
}
