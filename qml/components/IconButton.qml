import QtQuick
import QtQuick.Controls

Item {
    id: root
    property string icon: ""
    property bool fill: false
    property color colour: Theme.onSurfaceVariant
    property color bg: "transparent"
    property bool enabled: true
    signal clicked()

    width: 40
    height: 40
    opacity: enabled ? 1 : 0.38

    Rectangle {
        id: disc
        anchors.fill: parent
        radius: height / 2
        color: root.bg
        scale: ma.containsPress ? 0.94 : 1

        Behavior on scale {
            NumberAnimation { duration: 160; easing.type: Easing.OutBack }
        }
    }

    MaterialIcon {
        anchors.centerIn: parent
        icon: root.icon
        size: 22
        fill: root.fill
        color: root.colour
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Theme.onSurface
        opacity: ma.containsMouse && root.enabled ? 0.08 : 0
        Behavior on opacity { NumberAnimation { duration: 120 } }
    }
}
