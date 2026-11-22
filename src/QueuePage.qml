import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

// What plays next. Enter jumps to a track, Delete takes it out, and
// Alt+J / Alt+K (or Alt+arrows) move it down and up.
FocusScope {
    id: page
    property var sourceModel: player.queue
    property alias currentIndex: list.currentIndex
    readonly property bool isQueue: true
    signal activated(int index)

    function removeSelected() {
        if (list.currentIndex >= 0)
            player.removeAt(list.currentIndex)
    }

    Component.onCompleted: list.currentIndex = Math.max(0, player.currentIndex)

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 16
            Layout.topMargin: 18
            Layout.bottomMargin: 6
            Label {
                text: qsTr("Queue")
                font.pixelSize: Math.round(theme.fontSize * 1.7)
                font.weight: Font.Bold
                color: theme.foreground
            }
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 8
                visible: player.queue.count > 0
                text: qsTr("%n tracks", "", player.queue.count) + " · " + Format.time(player.queue.totalDurationMs)
                color: theme.muted
            }
            Button {
                text: qsTr("Clear")
                flat: true
                focusPolicy: Qt.NoFocus
                visible: player.queue.count > 0
                onClicked: player.clearQueue()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            clip: true
            model: player.queue
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
                const alt = event.modifiers === Qt.AltModifier
                if (alt && (event.key === Qt.Key_J || event.key === Qt.Key_Down) && list.currentIndex < list.count - 1) {
                    player.moveRow(list.currentIndex, list.currentIndex + 1)
                    list.currentIndex += 1
                } else if (alt && (event.key === Qt.Key_K || event.key === Qt.Key_Up) && list.currentIndex > 0) {
                    player.moveRow(list.currentIndex, list.currentIndex - 1)
                    list.currentIndex -= 1
                } else if (event.modifiers !== Qt.NoModifier) {
                    return
                } else if (event.key === Qt.Key_J) {
                    list.incrementCurrentIndex()
                } else if (event.key === Qt.Key_K) {
                    list.decrementCurrentIndex()
                } else if (event.key === Qt.Key_Delete || event.key === Qt.Key_X) {
                    page.removeSelected()
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    if (list.currentIndex >= 0)
                        page.activated(list.currentIndex)
                } else {
                    return
                }
                event.accepted = true
            }

            delegate: TrackRow {
                required property int index
                required property var model
                width: ListView.view.width - 32
                number: (index + 1).toString()
                title: model.name
                duration: model.duration
                subtitle: model.album.length > 0 ? model.artist + " — " + model.album : model.artist
                current: index === player.currentIndex
                selected: ListView.isCurrentItem && list.activeFocus
                showArt: true
                imageId: model.imageId
                imageTag: model.imageTag
                opacity: index < player.currentIndex ? 0.55 : 1
                onClicked: { list.currentIndex = index; list.forceActiveFocus() }
                onDoubleClicked: page.activated(index)
            }

            StatusView {
                anchors.fill: parent
                model: player.queue
                emptyText: qsTr("The queue is empty. Open an album and press Enter to start playing.")
            }
        }
    }
}
