import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string icon: ""
    property string label: ""
    property bool active: false
    signal clicked()

    height: 44
    Layout.fillWidth: true

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        radius: 22
        color: root.active ? Theme.secondaryContainer : (ma.containsMouse ? Theme.surfaceContainerHigh : "transparent")
        Behavior on color { ColorAnimation { duration: 140 } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 16
        spacing: 12

        MaterialIcon {
            icon: root.icon
            size: 22
            fill: root.active
            color: root.active ? Theme.onSecondaryContainer : Theme.onSurfaceVariant
        }

        Text {
            Layout.fillWidth: true
            text: root.label
            color: root.active ? Theme.onSecondaryContainer : Theme.onSurface
            font.pixelSize: 14
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
