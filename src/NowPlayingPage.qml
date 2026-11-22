import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects
import QtQuick.Layouts

// The playing track's cover as large as the window allows, over a soft blur
// of itself, with what plays next underneath.
FocusScope {
    id: page
    property var sourceModel: null
    property int currentIndex: -1
    readonly property bool isQueue: false
    signal activated(int index)

    readonly property int nextIndex: player.currentIndex + 1 < player.queue.count ? player.currentIndex + 1 : -1

    // The blurred backdrop: the same cover, scaled past the edges and dimmed
    // toward the theme's background so text stays readable in any theme.
    CoverArt {
        id: backdrop
        anchors.centerIn: parent
        width: Math.max(parent.width, parent.height) * 1.2
        height: width
        imageId: player.imageId
        imageTag: player.imageTag
        visible: false
    }
    MultiEffect {
        anchors.fill: backdrop
        source: backdrop
        visible: backdrop.ready
        blurEnabled: true
        blur: 1.0
        blurMax: 64
        saturation: 0.1
        opacity: 0.45
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.alpha(theme.background, 0.6) }
            GradientStop { position: 1.0; color: Qt.alpha(theme.background, 0.9) }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 720)
        spacing: 10
        visible: player.hasTrack

        CoverArt {
            readonly property int size: Math.max(160, Math.min(page.width - 96, page.height - theme.fontSize * 14, 640))
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: size
            Layout.preferredHeight: size
            Layout.bottomMargin: 14
            imageId: player.imageId
            imageTag: player.imageTag
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: player.title
            elide: Text.ElideRight
            font.pixelSize: Math.round(theme.fontSize * 1.8)
            font.weight: Font.Bold
            color: theme.foreground
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: player.album.length > 0 ? player.artist + " — " + player.album : player.artist
            elide: Text.ElideRight
            font.pixelSize: Math.round(theme.fontSize * 1.1)
            color: theme.foreground
            opacity: 0.8
        }
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 8
            horizontalAlignment: Text.AlignHCenter
            visible: page.nextIndex >= 0
            text: page.nextIndex >= 0 ? qsTr("Up next: %1").arg(player.queue.get(page.nextIndex).name) : ""
            elide: Text.ElideRight
            color: theme.muted
        }
    }

    Label {
        anchors.centerIn: parent
        visible: !player.hasTrack
        text: qsTr("Nothing is playing.")
        color: theme.muted
    }
}
