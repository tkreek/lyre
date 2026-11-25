import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// An artist and their albums, newest first.
FocusScope {
    id: page
    property string itemId
    property int kind: 3
    property string name
    property string subtitle
    property string imageId
    property string imageTag
    property int year: 0
    property int childCount: 0
    property var members: []
    property var sourceModel: library.albumsBy(itemId)
    property alias currentIndex: grid.currentIndex
    readonly property bool isQueue: false
    signal activated(int index)

    CoverGrid {
        id: grid
        anchors.fill: parent
        focus: true
        model: page.sourceModel
        onActivated: (index) => page.activated(index)

        header: Item {
            width: grid.width - grid.leftMargin - grid.rightMargin
            height: row.implicitHeight + 36

            RowLayout {
                id: row
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 20
                anchors.leftMargin: 8
                spacing: 24

                CoverArt {
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 160
                    round: true
                    imageId: page.imageId
                    imageTag: page.imageTag
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Label {
                        text: qsTr("Artist")
                        color: theme.muted
                        font.pixelSize: theme.fontSize - 2
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: 1
                    }
                    Label {
                        Layout.fillWidth: true
                        text: page.name
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        font.pixelSize: Math.round(theme.fontSize * 2)
                        font.weight: Font.Bold
                        color: theme.foreground
                    }
                    Label {
                        visible: page.sourceModel.count > 0
                        text: qsTr("%n albums", "", page.sourceModel.count)
                        color: theme.muted
                    }
                    // Wraps rather than clipping in a narrow window or a
                    // language with long words.
                    Flow {
                        Layout.fillWidth: true
                        Layout.topMargin: 10
                        spacing: 8
                        ActionButton {
                            accent: true
                            text: qsTr("Play all")
                            icon.source: "qrc:/icons/play.svg"
                            onClicked: player.playContainer(page.itemId, 3, false)
                        }
                        ActionButton {
                            text: qsTr("Shuffle")
                            icon.source: "qrc:/icons/shuffle.svg"
                            onClicked: player.playContainer(page.itemId, 3, true)
                        }
                    }
                }
            }
        }

        StatusView {
            anchors.fill: parent
            anchors.topMargin: grid.headerItem ? grid.headerItem.height : 0
            model: page.sourceModel
            emptyText: qsTr("No albums by this artist.")
        }
    }
}
