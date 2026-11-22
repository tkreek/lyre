import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Artists, albums, playlists, and tracks matching what's typed in the
// sidebar. Enter opens what's selected, or plays it if it's a track.
FocusScope {
    id: page
    property var sourceModel: library.searchResults
    property alias currentIndex: list.currentIndex
    readonly property bool isQueue: false
    signal activated(int index)

    function kindLabel(kind, artist, album, discs) {
        switch (kind) {
        case 3: return qsTr("Artist")
        case 2: {
            const label = artist ? qsTr("Album · %1").arg(artist) : qsTr("Album")
            return discs > 1 ? label + " · " + qsTr("%n discs", "", discs) : label
        }
        case 4: return qsTr("Playlist")
        default: return album ? artist + " — " + album : artist
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 16
            Layout.topMargin: 18
            Layout.bottomMargin: 6
            text: library.searchTerm.length > 0 ? qsTr("Results for “%1”").arg(library.searchTerm) : qsTr("Search")
            elide: Text.ElideRight
            font.pixelSize: Math.round(theme.fontSize * 1.7)
            font.weight: Font.Bold
            color: theme.foreground
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            clip: true
            model: library.searchResults
            keyNavigationEnabled: true
            boundsBehavior: Flickable.StopAtBounds
            leftMargin: 16
            rightMargin: 16
            bottomMargin: 24
            highlightMoveDuration: 0
            ScrollBar.vertical: ScrollBar {}
            maximumFlickVelocity: 8000
            WheelScroll { flickable: list }
            // Rows usually arrive after the view opens; start on the first one so
            // Enter works straight away.
            onCountChanged: if (currentIndex < 0 && count > 0) currentIndex = 0

            Keys.onPressed: (event) => {
                if (event.modifiers !== Qt.NoModifier)
                    return
                switch (event.key) {
                case Qt.Key_J: list.incrementCurrentIndex(); break
                case Qt.Key_K: list.decrementCurrentIndex(); break
                case Qt.Key_Return:
                case Qt.Key_Enter: if (list.currentIndex >= 0) page.activated(list.currentIndex); break
                default: return
                }
                event.accepted = true
            }

            delegate: TrackRow {
                required property int index
                required property var model
                width: ListView.view.width - 32
                number: ""
                title: model.name
                subtitle: page.kindLabel(model.kind, model.artist, model.album, model.discCount)
                duration: model.kind === 1 ? model.duration : 0
                current: model.kind === 1 && model.itemId === player.currentId
                selected: ListView.isCurrentItem && list.activeFocus
                showArt: true
                imageId: model.imageId
                imageTag: model.imageTag
                onClicked: { list.currentIndex = index; list.forceActiveFocus() }
                onDoubleClicked: page.activated(index)
            }

            StatusView {
                anchors.fill: parent
                model: library.searchResults
                emptyText: library.searchTerm.length > 0 ? qsTr("Nothing matches “%1”.").arg(library.searchTerm)
                                                         : qsTr("Type in the search box to find artists, albums, and tracks.")
            }
        }
    }
}
