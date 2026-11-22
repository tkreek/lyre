import QtQuick
import QtQuick.Shapes

// The lyre from the app icon, drawn as vector strokes in a theme color so it
// sits beside the wordmark and follows theme switches. The paths are the
// icon's (pkgbuild/lyre.svg), cropped to the lyre itself.
Item {
    id: mark
    property color color: theme.accent
    implicitWidth: 32
    implicitHeight: 32

    // The lyre spans roughly 116–396 of the icon's 512 units on both axes.
    readonly property real unit: Math.min(width, height) / 280

    Shape {
        x: -116 * mark.unit
        y: -118 * mark.unit
        width: 512
        height: 512
        scale: mark.unit
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: mark.color
            strokeWidth: 18
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: "M214 384 C 150 360, 132 280, 160 210 C 172 180, 168 150, 140 132 M298 384 C 362 360, 380 280, 352 210 C 340 180, 344 150, 372 132 M168 186 L344 186 M214 384 L298 384" }
        }
        ShapePath {
            strokeColor: mark.color
            // Kept at least a pixel wide, so the strings survive at header size.
            strokeWidth: Math.max(9, 1.1 / mark.unit)
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: "M226 186 L244 384 M256 186 L256 384 M286 186 L268 384" }
        }
    }
}
