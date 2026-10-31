import QtQuick

// Chrome icon that does not rely on Material Symbols ligatures (those were
// spilling raw names like "desktop_windows" over the sidebar heading).
Item {
    id: root
    property string name: ""          // freedesktop icon name
    property string glyph: ""         // optional unicode fallback
    property color colour: Theme.onSurfaceVariant
    property real size: 22

    implicitWidth: size + 8
    implicitHeight: size + 8
    width: implicitWidth
    height: implicitHeight

    Image {
        id: img
        anchors.centerIn: parent
        width: root.size
        height: root.size
        source: root.name ? Browser.iconUrl(root.name) : ""
        sourceSize.width: root.size * 2
        sourceSize.height: root.size * 2
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        visible: status === Image.Ready
    }

    StyledText {
        anchors.centerIn: parent
        visible: !img.visible && root.glyph.length
        text: root.glyph
        color: root.colour
        font.pixelSize: root.size
    }
}
