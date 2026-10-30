import QtQuick

Text {
    id: root
    property string icon: ""
    property real size: 20
    property bool fill: false

    text: icon
    color: Theme.onSurface
    font.family: "Material Symbols Rounded"
    font.pixelSize: size
    font.variableAxes: {
        "FILL": fill ? 1 : 0,
        "wght": 500,
        "GRAD": 0,
        "opsz": Math.min(48, Math.max(20, size))
    }
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
}
