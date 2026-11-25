import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The row of actions under an album's or artist's header: one filled in the
// accent (Play), the rest plain. A notch smaller than a dialog's buttons, so
// they sit under the title rather than compete with it.
Button {
    id: button
    property bool accent: false

    focusPolicy: Qt.NoFocus
    font.pixelSize: theme.fontSize - 1
    font.weight: accent ? Font.DemiBold : Font.Normal
    icon.width: 16
    icon.height: 16
    icon.color: accent ? theme.accentForeground : theme.foreground
    Material.foreground: accent ? theme.accentForeground : theme.foreground
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    topPadding: 6
    bottomPadding: 6
    leftPadding: 12
    rightPadding: 14
    spacing: 6

    background: Rectangle {
        implicitWidth: 56
        implicitHeight: 32
        radius: Math.min(6, theme.radius)
        color: button.accent ? theme.accent : "transparent"
        opacity: button.enabled ? 1 : 0.4
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: button.accent ? theme.accentForeground : theme.foreground
            opacity: button.down ? 0.18 : button.hovered ? 0.08 : 0
        }
    }
}
