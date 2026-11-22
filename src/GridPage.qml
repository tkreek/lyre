import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// One of the library's top-level lists: Albums, Artists, or Playlists.
FocusScope {
    id: page
    property string title
    property var sourceModel
    property bool round: false
    property string emptyText
    property alias currentIndex: grid.currentIndex
    readonly property bool isQueue: false
    signal activated(int index)
    signal retry()

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
                text: page.title
                font.pixelSize: Math.round(theme.fontSize * 1.7)
                font.weight: Font.Bold
                color: theme.foreground
            }
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.alignment: Qt.AlignBaseline
                visible: page.sourceModel.count > 0
                text: page.sourceModel.count.toLocaleString(Qt.locale(), 'f', 0)
                color: theme.muted
            }
        }

        CoverGrid {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            model: page.sourceModel
            round: page.round
            onActivated: (index) => page.activated(index)

            StatusView {
                anchors.fill: parent
                model: page.sourceModel
                emptyText: page.emptyText
                onRetry: page.retry()
            }
        }
    }
}
