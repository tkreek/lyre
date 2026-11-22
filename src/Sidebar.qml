import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Search on top, the library's sections below it with their number keys,
// and who's signed in at the bottom.
Rectangle {
    id: side
    property string section
    property alias searchField: search
    signal navigate(string name)
    signal searchMoved()
    signal helpRequested()
    color: theme.surface

    // Searching waits for a pause in typing, so each word doesn't hit the
    // server with every letter.
    Timer {
        id: debounce
        interval: 250
        onTriggered: library.search(search.text)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 2

        Label {
            Layout.leftMargin: 8
            Layout.topMargin: 6
            Layout.bottomMargin: 10
            text: "lyre"
            font.pixelSize: Math.round(theme.fontSize * 1.6)
            font.weight: Font.Bold
            color: theme.accent
        }

        TextField {
            id: search
            Layout.fillWidth: true
            Layout.bottomMargin: 10
            placeholderText: qsTr("Search  /")
            Material.accent: theme.accent
            Material.containerStyle: Material.Outlined
            onTextEdited: {
                debounce.restart()
                if (text.length > 0 && side.section !== "search")
                    side.navigate("search")
            }
            Keys.onDownPressed: side.searchMoved()
            Keys.onReturnPressed: side.searchMoved()
            Keys.onEnterPressed: side.searchMoved()
        }

        Repeater {
            model: [
                { name: "albums", label: qsTr("Albums"), icon: "album", key: "1" },
                { name: "artists", label: qsTr("Artists"), icon: "artist", key: "2" },
                { name: "playlists", label: qsTr("Playlists"), icon: "playlist", key: "3" },
                { name: "queue", label: qsTr("Queue"), icon: "queue", key: "4" },
                { name: "playing", label: qsTr("Now Playing"), icon: "note", key: "5" }
            ]
            delegate: ItemDelegate {
                id: entry
                required property var modelData
                readonly property bool active: side.section === modelData.name
                Layout.fillWidth: true
                implicitHeight: Math.round(theme.fontSize * 2.8)
                text: modelData.label
                icon.source: "qrc:/icons/" + modelData.icon + ".svg"
                icon.color: active ? theme.accent : theme.muted
                icon.width: 20
                icon.height: 20
                focusPolicy: Qt.NoFocus
                Material.foreground: active ? theme.foreground : theme.foreground
                Material.roundedScale: Math.min(4, theme.radius)
                font.weight: active ? Font.DemiBold : Font.Normal
                background: Rectangle {
                    radius: Math.min(4, theme.radius)
                    color: entry.active ? theme.selection : entry.hovered ? Qt.alpha(theme.selection, 0.5) : "transparent"
                }
                onClicked: side.navigate(modelData.name)

                Label {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: entry.modelData.key
                    color: theme.muted
                    font.pixelSize: theme.fontSize - 2
                }
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: 0
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 8
                spacing: 0
                Label {
                    Layout.fillWidth: true
                    text: jellyfin.userName
                    elide: Text.ElideRight
                    color: theme.foreground
                }
                Label {
                    Layout.fillWidth: true
                    text: jellyfin.server.replace(/^https?:\/\//, "")
                    elide: Text.ElideRight
                    color: theme.muted
                    font.pixelSize: theme.fontSize - 3
                }
            }
            IconButton {
                iconName: "help"
                iconSize: 18
                tip: qsTr("Keyboard shortcuts (?)")
                onClicked: side.helpRequested()
            }
            IconButton {
                iconName: "refresh"
                iconSize: 18
                tip: qsTr("Reload the library (Ctrl+R)")
                onClicked: library.refresh()
            }
            IconButton {
                iconName: "logout"
                iconSize: 18
                tip: qsTr("Sign out")
                onClicked: jellyfin.signOut()
            }
        }
    }
}
