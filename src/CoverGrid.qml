import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Albums, artists, or playlists as a grid of covers that fills the width.
// Arrows or h j k l move, Enter opens.
GridView {
    id: grid
    property bool round: false
    signal activated(int index)

    focus: true
    clip: true
    keyNavigationEnabled: true
    boundsBehavior: Flickable.StopAtBounds
    leftMargin: 16
    rightMargin: 16
    topMargin: 8
    bottomMargin: 24
    readonly property int columns: Math.max(1, Math.floor((width - leftMargin - rightMargin) / (theme.fontSize * 13)))
    cellWidth: Math.floor((width - leftMargin - rightMargin) / columns)
    cellHeight: cellWidth + Math.round(theme.fontSize * 3.6)
    cacheBuffer: cellHeight * 3
    reuseItems: true
    highlightFollowsCurrentItem: false
    ScrollBar.vertical: ScrollBar {}

    // Rows usually arrive after the view opens; start on the first one so
    // Enter works straight away.
    onCountChanged: if (currentIndex < 0 && count > 0) currentIndex = 0

    Keys.onPressed: (event) => {
        if (event.modifiers !== Qt.NoModifier)
            return
        switch (event.key) {
        case Qt.Key_H: grid.moveCurrentIndexLeft(); break
        case Qt.Key_J: grid.moveCurrentIndexDown(); break
        case Qt.Key_K: grid.moveCurrentIndexUp(); break
        case Qt.Key_L: grid.moveCurrentIndexRight(); break
        case Qt.Key_Home: grid.currentIndex = 0; break
        case Qt.Key_End: grid.currentIndex = grid.count - 1; break
        case Qt.Key_Return:
        case Qt.Key_Enter: if (grid.currentIndex >= 0) grid.activated(grid.currentIndex); break
        default: return
        }
        event.accepted = true
    }

    delegate: Item {
        id: card
        required property int index
        required property string name
        required property string artist
        required property string imageId
        required property string imageTag
        required property int kind
        required property int childCount
        required property int year
        width: grid.cellWidth
        height: grid.cellHeight
        readonly property bool selected: GridView.isCurrentItem && grid.activeFocus

        CoverArt {
            id: art
            x: 8
            y: 8
            width: card.width - 16
            height: width
            imageId: card.imageId
            imageTag: card.imageTag
            round: grid.round
            highlighted: card.selected || hover.hovered
            scale: hover.hovered ? 1.015 : 1
            Behavior on scale { NumberAnimation { duration: 120 } }
        }

        Label {
            id: title
            anchors.top: art.bottom
            anchors.topMargin: 8
            x: 8
            width: card.width - 16
            text: card.name
            elide: Text.ElideRight
            horizontalAlignment: grid.round ? Text.AlignHCenter : Text.AlignLeft
            color: card.selected ? theme.accent : theme.foreground
            font.weight: Font.Medium
        }
        Label {
            anchors.top: title.bottom
            anchors.topMargin: 2
            x: 8
            width: card.width - 16
            elide: Text.ElideRight
            horizontalAlignment: title.horizontalAlignment
            color: theme.muted
            font.pixelSize: theme.fontSize - 2
            text: card.kind === 4 ? qsTr("%n tracks", "", card.childCount)
                : card.kind === 2 ? (card.year > 0 && card.artist ? card.artist + " · " + card.year : card.artist)
                : ""
        }

        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: {
                grid.currentIndex = card.index
                grid.forceActiveFocus()
                grid.activated(card.index)
            }
        }
    }
}
