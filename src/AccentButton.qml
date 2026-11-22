import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The one filled button on a screen, in the theme's accent with text that
// reads on it. Drawn here rather than left to the style, so it looks the
// same in every theme and renderer.
Button {
    id: button
    focusPolicy: Qt.NoFocus
    Material.foreground: theme.accentForeground
    icon.color: theme.accentForeground
    font.weight: Font.DemiBold

    background: Rectangle {
        implicitWidth: 64
        implicitHeight: 40
        radius: Math.min(6, theme.radius)
        color: theme.accent
        opacity: button.enabled ? 1 : 0.4
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: theme.accentForeground
            opacity: button.down ? 0.18 : button.hovered ? 0.08 : 0
        }
    }
}
