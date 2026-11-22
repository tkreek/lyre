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
    maximumFlickVelocity: 8000
    WheelScroll { flickable: grid }

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
        required property int discCount
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

        // A set of discs says so on its cover, where a long artist name
        // can't push it out of sight.
        Rectangle {
            visible: card.discCount > 1
            anchors.right: art.right
            anchors.bottom: art.bottom
            anchors.margins: 8
            width: discs.implicitWidth + 12
            height: discs.implicitHeight + 6
            radius: Math.min(4, theme.radius)
            color: Qt.alpha(theme.background, 0.88)
            Label {
                id: discs
                anchors.centerIn: parent
                text: qsTr("%n discs", "", card.discCount)
                color: theme.foreground
                font.pixelSize: theme.fontSize - 3
                font.weight: Font.DemiBold
            }
        }

        MarqueeLabel {
            id: title
            anchors.top: art.bottom
            anchors.topMargin: 8
            x: 8
            width: card.width - 16
            text: card.name
            horizontalAlignment: grid.round ? Text.AlignHCenter : Text.AlignLeft
            color: card.selected ? theme.accent : theme.foreground
            font.weight: Font.Medium
            // A long title scrolls into view under the pointer, or when the
            // keyboard selects the card.
            running: hover.hovered || card.selected
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
            text: {
                if (card.kind === 4)
                    return qsTr("%n tracks", "", card.childCount)
                if (card.kind !== 2)
                    return ""
                const parts = card.artist ? [card.artist] : []
                if (card.year > 0)
                    parts.push(card.year)
                return parts.join(" · ")
            }
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
