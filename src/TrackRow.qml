import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

// One track in a list: number, title, artist, length. The playing track is
// drawn in the accent with bouncing bars in place of its number.
Rectangle {
    id: row
    property string number
    property string title
    property string subtitle
    property real duration: 0
    property bool current: false
    property bool selected: false
    property bool showArt: false
    property string imageId
    property string imageTag
    signal clicked()
    signal doubleClicked()

    height: Math.round(theme.fontSize * (showArt ? 3.7 : 2.9))
    radius: Math.min(4, theme.radius)
    color: selected ? theme.selection : mouse.containsMouse ? Qt.alpha(theme.selection, 0.5) : "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 14
        spacing: 12

        Item {
            Layout.preferredWidth: theme.fontSize * 2.4
            Layout.fillHeight: true
            Label {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: !row.current
                text: row.number
                color: theme.muted
                font.features: { "tnum": 1 }
            }
            PlayingBars {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: row.current
            }
        }

        CoverArt {
            visible: row.showArt
            Layout.preferredWidth: row.height - 12
            Layout.preferredHeight: row.height - 12
            imageId: row.imageId
            imageTag: row.imageTag
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Label {
                Layout.fillWidth: true
                text: row.title
                elide: Text.ElideRight
                color: row.current ? theme.accent : theme.foreground
                font.weight: row.current ? Font.DemiBold : Font.Normal
            }
            Label {
                Layout.fillWidth: true
                visible: row.subtitle.length > 0
                text: row.subtitle
                elide: Text.ElideRight
                color: theme.muted
                font.pixelSize: theme.fontSize - 2
            }
        }

        Label {
            visible: row.duration > 0
            text: Format.time(row.duration)
            color: theme.muted
            font.features: { "tnum": 1 }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: row.clicked()
        onDoubleClicked: row.doubleClicked()
    }
}
