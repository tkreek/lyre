import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A toolbar button drawing one of the bundled icons in the theme's colors.
// It never takes focus, so the keyboard stays on the list being browsed.
ToolButton {
    id: button
    property string iconName
    property bool active: false
    property string tip
    property int iconSize: 22

    icon.source: "qrc:/icons/" + iconName + ".svg"
    icon.color: active ? theme.accent : (enabled ? theme.foreground : theme.muted)
    icon.width: iconSize
    icon.height: iconSize
    focusPolicy: Qt.NoFocus
    Material.foreground: theme.foreground

    ToolTip.visible: hovered && tip.length > 0
    ToolTip.text: tip
    ToolTip.delay: 600
}
