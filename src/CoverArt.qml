import QtQuick
import QtQuick.Effects
import QtQuick.Window
import "Format.js" as Format

// An album, artist, or playlist image from the server, scaled there to the
// size it's shown at, with a quiet placeholder for items without art.
Item {
    id: root
    property string imageId
    property string imageTag
    property bool round: false
    property bool highlighted: false
    // Rounds with the windows, unless it's an artist's circle.
    readonly property real radius: round ? width / 2 : theme.radius
    readonly property int requestSize: Format.imageSize(Math.max(width, height), Screen.devicePixelRatio)
    readonly property bool ready: image.status === Image.Ready

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: theme.raised

        Text {
            anchors.centerIn: parent
            visible: !root.ready
            text: "♪"
            color: theme.muted
            font.family: theme.fontFamily
            font.pixelSize: Math.max(8, parent.height * 0.36)
        }
    }

    Image {
        id: image
        anchors.fill: parent
        source: root.imageTag && root.width > 0 ? jellyfin.image(root.imageId, root.imageTag, root.requestSize) : ""
        sourceSize.width: root.requestSize
        sourceSize.height: root.requestSize
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        smooth: true
        mipmap: true
        opacity: root.ready ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 160 } }

        layer.enabled: root.radius > 0
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: mask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
    }

    Item {
        id: mask
        anchors.fill: parent
        layer.enabled: true
        visible: false
        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: "black"
            antialiasing: true
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: root.highlighted ? 3 : 0
        border.color: theme.accent
    }
}
