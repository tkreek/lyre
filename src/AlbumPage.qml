import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

// An album or a playlist: its cover large beside what it is, then the
// tracks. Enter plays from the selected track.
FocusScope {
    id: page
    property string itemId
    property int kind: 2
    property string name
    property string subtitle
    property string imageId
    property string imageTag
    property int year: 0
    property int childCount: 0
    // Owned by this page, so going back frees the track list.
    property var sourceModel: library.tracksOf(itemId, kind)
    property alias currentIndex: list.currentIndex
    readonly property bool isQueue: false
    readonly property bool isPlaylist: kind === 4
    signal activated(int index)

    function shuffle() {
        if (sourceModel.count === 0)
            return
        player.shuffle = true
        player.playFrom(sourceModel, Math.floor(Math.random() * sourceModel.count))
    }

    ListView {
        id: list
        anchors.fill: parent
        focus: true
        clip: true
        model: page.sourceModel
        currentIndex: 0
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

        section.property: !page.isPlaylist && page.sourceModel.count > 0 && page.sourceModel.multiDisc() ? "disc" : ""
        section.delegate: Label {
            required property string section
            width: ListView.view.width - 32
            leftPadding: 12
            topPadding: 16
            bottomPadding: 6
            text: qsTr("Disc %1").arg(section)
            color: theme.muted
            font.weight: Font.DemiBold
        }

        Keys.onPressed: (event) => {
            if (event.modifiers !== Qt.NoModifier)
                return
            switch (event.key) {
            case Qt.Key_J: list.incrementCurrentIndex(); break
            case Qt.Key_K: list.decrementCurrentIndex(); break
            case Qt.Key_Home: list.currentIndex = 0; break
            case Qt.Key_End: list.currentIndex = list.count - 1; break
            case Qt.Key_Return:
            case Qt.Key_Enter: if (list.currentIndex >= 0) page.activated(list.currentIndex); break
            default: return
            }
            event.accepted = true
        }

        header: Item {
            width: list.width - list.leftMargin - list.rightMargin
            height: headerRow.implicitHeight + 40

            RowLayout {
                id: headerRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 24
                anchors.leftMargin: 8
                spacing: 24

                CoverArt {
                    id: cover
                    readonly property int size: Math.round(Math.min(260, Math.max(140, page.width * 0.24)))
                    Layout.preferredWidth: size
                    Layout.preferredHeight: size
                    Layout.alignment: Qt.AlignTop
                    imageId: page.imageId
                    imageTag: page.imageTag
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBottom
                    spacing: 6

                    Label {
                        text: page.isPlaylist ? qsTr("Playlist") : qsTr("Album")
                        color: theme.muted
                        font.pixelSize: theme.fontSize - 2
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: 1
                    }
                    Label {
                        Layout.fillWidth: true
                        text: page.name
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        font.pixelSize: Math.round(theme.fontSize * 2)
                        font.weight: Font.Bold
                        color: theme.foreground
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: page.subtitle.length > 0
                        text: page.subtitle
                        elide: Text.ElideRight
                        color: theme.foreground
                        font.pixelSize: Math.round(theme.fontSize * 1.1)
                    }
                    Label {
                        Layout.fillWidth: true
                        text: {
                            var parts = []
                            if (page.year > 0)
                                parts.push(page.year)
                            if (page.sourceModel.count > 0) {
                                parts.push(qsTr("%n tracks", "", page.sourceModel.count))
                                parts.push(qsTr("%n min", "", Math.round(page.sourceModel.totalDurationMs / 60000)))
                            }
                            return parts.join(" · ")
                        }
                        color: theme.muted
                    }

                    // Wraps rather than clipping in a narrow window or a
                    // language with long words.
                    Flow {
                        Layout.fillWidth: true
                        Layout.topMargin: 10
                        spacing: 8
                        AccentButton {
                            text: qsTr("Play")
                            icon.source: "qrc:/icons/play.svg"
                            enabled: page.sourceModel.count > 0
                            onClicked: player.playFrom(page.sourceModel, 0)
                        }
                        Button {
                            text: qsTr("Shuffle")
                            flat: true
                            icon.source: "qrc:/icons/shuffle.svg"
                            focusPolicy: Qt.NoFocus
                            enabled: page.sourceModel.count > 0
                            onClicked: page.shuffle()
                        }
                        Button {
                            text: qsTr("Add to queue")
                            flat: true
                            icon.source: "qrc:/icons/add-queue.svg"
                            focusPolicy: Qt.NoFocus
                            enabled: page.sourceModel.count > 0
                            onClicked: player.addToQueue(page.sourceModel, -1, false)
                        }
                    }
                }
            }
        }

        delegate: TrackRow {
            required property int index
            required property var model
            width: ListView.view.width - 32
            number: page.isPlaylist || model.trackNumber <= 0 ? (index + 1).toString() : model.trackNumber.toString()
            title: model.name
            duration: model.duration
            // An album's tracks usually share its artist; say it only when
            // a track's differs.
            subtitle: page.isPlaylist || model.artist !== page.subtitle ? model.artist : ""
            current: model.itemId === player.currentId
            selected: ListView.isCurrentItem && list.activeFocus
            showArt: page.isPlaylist
            imageId: page.isPlaylist ? model.imageId : ""
            imageTag: page.isPlaylist ? model.imageTag : ""
            onClicked: { list.currentIndex = index; list.forceActiveFocus() }
            onDoubleClicked: page.activated(index)
        }

        StatusView {
            anchors.fill: parent
            anchors.topMargin: list.headerItem ? list.headerItem.height : 0
            model: page.sourceModel
            emptyText: qsTr("There are no tracks here.")
        }
    }
}
