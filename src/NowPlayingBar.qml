import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

// The transport along the bottom: what's playing, the controls, where it
// is in the track, and the volume.
Rectangle {
    id: bar
    signal openNowPlaying()
    color: theme.surface
    implicitHeight: Math.round(theme.fontSize * 6.4)

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 16
        spacing: 16

        // What's playing; clicking it opens the big view.
        RowLayout {
            Layout.preferredWidth: Math.max(220, bar.width * 0.28)
            Layout.maximumWidth: Math.max(220, bar.width * 0.28)
            spacing: 12

            CoverArt {
                Layout.preferredWidth: bar.height - 22
                Layout.preferredHeight: bar.height - 22
                imageId: player.imageId
                imageTag: player.imageTag
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: player.title
                    elide: Text.ElideRight
                    font.weight: Font.DemiBold
                    color: theme.foreground
                }
                Label {
                    Layout.fillWidth: true
                    text: player.error.length > 0 ? player.error : player.artist
                    elide: Text.ElideRight
                    color: player.error.length > 0 ? theme.danger : theme.muted
                    font.pixelSize: theme.fontSize - 2
                }
            }
            TapHandler { onTapped: bar.openNowPlaying() }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }

        // The controls over the progress line.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 4
                IconButton {
                    iconName: "shuffle"
                    iconSize: 18
                    active: player.shuffle
                    tip: qsTr("Shuffle (S)")
                    onClicked: player.shuffle = !player.shuffle
                }
                IconButton {
                    iconName: "previous"
                    tip: qsTr("Previous (P)")
                    onClicked: player.previous()
                }
                RoundButton {
                    id: playButton
                    implicitWidth: Math.round(theme.fontSize * 3.2)
                    implicitHeight: implicitWidth
                    focusPolicy: Qt.NoFocus
                    background: Rectangle {
                        radius: width / 2
                        color: theme.accent
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            color: theme.accentForeground
                            opacity: playButton.down ? 0.18 : playButton.hovered ? 0.08 : 0
                        }
                    }
                    icon.source: player.playing ? "qrc:/icons/pause.svg" : "qrc:/icons/play.svg"
                    icon.color: theme.accentForeground
                    icon.width: 22
                    icon.height: 22
                    onClicked: player.playPause()
                    BusyIndicator {
                        anchors.fill: parent
                        anchors.margins: -4
                        running: player.buffering && player.playing
                        visible: running
                        Material.accent: theme.foreground
                    }
                }
                IconButton {
                    iconName: "next"
                    enabled: player.canNext
                    tip: qsTr("Next (N)")
                    onClicked: player.next()
                }
                IconButton {
                    iconName: player.repeat === 2 ? "repeat-one" : "repeat"
                    iconSize: 18
                    active: player.repeat !== 0
                    tip: player.repeat === 2 ? qsTr("Repeat track (R)") : player.repeat === 1 ? qsTr("Repeat queue (R)") : qsTr("Repeat off (R)")
                    onClicked: player.cycleRepeat()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 760
                Layout.alignment: Qt.AlignHCenter
                spacing: 10
                Label {
                    text: Format.time(progress.pressed ? progress.value : player.position)
                    color: theme.muted
                    font.pixelSize: theme.fontSize - 2
                    font.features: { "tnum": 1 }
                }
                Slider {
                    id: progress
                    Layout.fillWidth: true
                    from: 0
                    to: Math.max(1, player.duration)
                    focusPolicy: Qt.NoFocus
                    Material.accent: theme.accent
                    Binding on value {
                        when: !progress.pressed
                        value: player.position
                        restoreMode: Binding.RestoreNone
                    }
                    onPressedChanged: if (!pressed) player.seek(value)
                }
                Label {
                    text: Format.time(player.duration)
                    color: theme.muted
                    font.pixelSize: theme.fontSize - 2
                    font.features: { "tnum": 1 }
                }
            }
        }

        // Volume.
        RowLayout {
            Layout.preferredWidth: Math.max(160, bar.width * 0.16)
            spacing: 2
            IconButton {
                iconName: player.muted || player.volume === 0 ? "muted" : "volume"
                tip: qsTr("Mute (M)")
                onClicked: player.muted = !player.muted
            }
            Slider {
                Layout.fillWidth: true
                from: 0
                to: 1
                value: player.volume
                focusPolicy: Qt.NoFocus
                Material.accent: theme.accent
                onMoved: {
                    player.muted = false
                    player.volume = value
                }
            }
        }
    }
}
