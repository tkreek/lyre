import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1200
    height: 780
    minimumWidth: 760
    minimumHeight: 520
    visible: true
    title: player.hasTrack ? qsTr("%1 — Lyre").arg(player.title) : "Lyre"
    color: theme.background
    font.family: theme.fontFamily
    font.pixelSize: theme.fontSize

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.primary: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius

    property string section: "albums"
    readonly property bool signedIn: jellyfin.signedIn
    readonly property var page: stack.currentItem

    // ------------------------------------------------------------ navigation

    function showSection(name, keepFocus) {
        if (!signedIn)
            return
        const pages = { albums: albumsPage, artists: artistsPage, playlists: playlistsPage,
                        queue: queuePage, playing: nowPlayingPage, search: searchPage }
        if (section !== name || stack.depth > 1 || !stack.currentItem)
            stack.replace(null, pages[name], {}, StackView.Immediate)
        section = name
        if (name === "artists") library.loadArtists()
        if (name === "playlists") library.loadPlaylists()
        if (!keepFocus)
            focusPage()
    }

    function focusPage() {
        Qt.callLater(() => { if (stack.currentItem) stack.currentItem.forceActiveFocus() })
    }

    function back() {
        if (stack.depth > 1) {
            stack.pop()
            focusPage()
        }
    }

    // Opens an album, playlist, or artist from any list.
    function openItem(item) {
        const props = { itemId: item.itemId, kind: item.kind, name: item.name, subtitle: item.artist || "",
                        imageId: item.imageId || "", imageTag: item.imageTag || "", year: item.year || 0,
                        childCount: item.childCount || 0 }
        if (item.kind === 3)
            stack.push(artistPage, props)
        else if (item.kind === 2 || item.kind === 4)
            stack.push(albumPage, props)
        focusPage()
    }

    function goToPlaying() {
        if (!player.hasTrack || !player.albumId)
            return
        showSection("albums")
        openItem({ itemId: player.albumId, kind: 2, name: player.album, artist: player.artist,
                   imageId: player.imageId, imageTag: player.imageTag })
    }

    // ------------------------------------------------------------ actions on the selection

    function selected() {
        if (!page || !page.sourceModel || page.currentIndex < 0 || page.currentIndex >= page.sourceModel.count)
            return null
        return page.sourceModel.get(page.currentIndex)
    }

    // Enter: tracks play from there, anything else opens.
    function activate(index) {
        if (page.isQueue) {
            player.playAt(index)
            return
        }
        const item = page.sourceModel.get(index)
        if (item.kind === 1)
            player.playFrom(page.sourceModel, index)
        else
            openItem(item)
    }

    function playSelected() {
        const item = selected()
        if (!item) return
        if (page.isQueue) player.playAt(page.currentIndex)
        else if (item.kind === 1) player.playFrom(page.sourceModel, page.currentIndex)
        else player.playContainer(item.itemId, item.kind, false)
    }

    function queueSelected(next) {
        const item = selected()
        if (!item || page.isQueue) return
        if (item.kind === 1) player.addToQueue(page.sourceModel, page.currentIndex, next)
        else player.addContainer(item.itemId, item.kind, next)
    }

    Connections {
        target: jellyfin
        function onSessionChanged() {
            if (jellyfin.signedIn) {
                sidebar.searchField.text = ""
                win.showSection("albums")
            }
        }
    }
    Connections {
        target: mpris
        function onRaiseRequested() {
            win.show()
            win.raise()
            win.requestActivate()
        }
    }
    Connections {
        target: player
        function onQueued(count) { toast.show(qsTr("Added %n tracks to the queue", "", count)) }
    }

    // ------------------------------------------------------------ keys

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: help.opened ? help.close() : help.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequences: ["Space", "Media Play"]; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: player.playPause() }
    Shortcut { sequence: "N"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: player.next() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: player.previous() }
    Shortcut { sequence: "Shift+Right"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: player.seekBy(10000) }
    Shortcut { sequence: "Shift+Left"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: player.seekBy(-10000) }
    Shortcut { sequences: ["+", "="]; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: player.volume = Math.min(1, player.volume + 0.05) }
    Shortcut { sequence: "-"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: player.volume = Math.max(0, player.volume - 0.05) }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: player.muted = !player.muted }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: player.shuffle = !player.shuffle }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: player.cycleRepeat() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.showSection("albums") }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.showSection("artists") }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.showSection("playlists") }
    Shortcut { sequence: "4"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.showSection("queue") }
    Shortcut { sequence: "5"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.showSection("playing") }
    Shortcut { sequence: "G"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.goToPlaying() }
    Shortcut { sequence: "A"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.queueSelected(false) }
    Shortcut { sequence: "Shift+A"; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.queueSelected(true) }
    Shortcut { sequences: ["Shift+Return", "Shift+Enter"]; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: win.playSelected() }
    Shortcut { sequences: ["Ctrl+R", "F5"]; context: Qt.ApplicationShortcut; enabled: win.signedIn; onActivated: library.refresh() }
    Shortcut {
        sequences: ["/", "Ctrl+F"]
        context: Qt.ApplicationShortcut
        enabled: win.signedIn
        onActivated: {
            sidebar.searchField.forceActiveFocus()
            sidebar.searchField.selectAll()
        }
    }
    Shortcut {
        sequences: ["Esc", "Backspace"]
        context: Qt.ApplicationShortcut
        enabled: win.signedIn || help.opened
        onActivated: {
            if (help.opened) {
                help.close()
            } else if (sidebar.searchField.activeFocus) {
                // Leaving the search box clears it and returns to the list
                // that was showing.
                sidebar.searchField.text = ""
                library.search("")
                win.showSection(win.section === "search" ? "albums" : win.section)
            } else {
                win.back()
            }
        }
    }

    // ------------------------------------------------------------ pages

    Component {
        id: albumsPage
        GridPage {
            title: qsTr("Albums")
            sourceModel: library.albums
            emptyText: qsTr("There are no albums in this library.")
            onActivated: (index) => win.activate(index)
            onRetry: library.loadAlbums(true)
        }
    }
    Component {
        id: artistsPage
        GridPage {
            title: qsTr("Artists")
            sourceModel: library.artists
            round: true
            emptyText: qsTr("There are no artists in this library.")
            onActivated: (index) => win.activate(index)
            onRetry: library.loadArtists(true)
        }
    }
    Component {
        id: playlistsPage
        GridPage {
            title: qsTr("Playlists")
            sourceModel: library.playlists
            emptyText: qsTr("There are no playlists yet.")
            onActivated: (index) => win.activate(index)
            onRetry: library.loadPlaylists(true)
        }
    }
    Component { id: queuePage; QueuePage { onActivated: (index) => win.activate(index) } }
    Component { id: searchPage; SearchPage { onActivated: (index) => win.activate(index) } }
    Component { id: nowPlayingPage; NowPlayingPage {} }
    Component { id: albumPage; AlbumPage { onActivated: (index) => win.activate(index) } }
    Component { id: artistPage; ArtistPage { onActivated: (index) => win.activate(index) } }

    // ------------------------------------------------------------ layout

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        LoginPage {
            anchors.fill: parent
            visible: !win.signedIn
            focus: !win.signedIn
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            visible: win.signedIn

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Sidebar {
                    id: sidebar
                    Layout.fillHeight: true
                    Layout.preferredWidth: Math.round(theme.fontSize * 16)
                    section: win.section
                    onNavigate: (name) => win.showSection(name, name === "search")
                    onSearchMoved: win.focusPage()
                    onHelpRequested: help.open()
                }
                Rectangle {
                    Layout.fillHeight: true
                    implicitWidth: 1
                    color: theme.selection
                }
                StackView {
                    id: stack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    initialItem: albumsPage

                    pushEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
                    pushExit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }
                    popEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
                    popExit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: theme.selection
                visible: nowPlayingBar.visible
            }
            NowPlayingBar {
                id: nowPlayingBar
                Layout.fillWidth: true
                visible: player.hasTrack
                onOpenNowPlaying: win.showSection("playing")
            }
        }

        // A brief confirmation above the transport, for things like adding
        // to the queue that change nothing on screen.
        Rectangle {
            id: toast
            property string text
            function show(message) {
                text = message
                opacity = 1
                toastTimer.restart()
            }
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: (nowPlayingBar.visible ? nowPlayingBar.height : 0) + 20
            width: toastLabel.implicitWidth + 32
            height: toastLabel.implicitHeight + 18
            radius: Math.min(8, theme.radius)
            color: theme.raised
            border.color: theme.selection
            opacity: 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 180 } }
            Label {
                id: toastLabel
                anchors.centerIn: parent
                text: toast.text
                color: theme.foreground
            }
            Timer {
                id: toastTimer
                interval: 2200
                onTriggered: toast.opacity = 0
            }
        }
    }

    HelpOverlay {
        id: help
        onClosed: win.focusPage()
    }

    Component.onCompleted: if (signedIn) focusPage()
}
