import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string name: ""
    property string path: ""
    property bool isDir: false
    property string iconName: "text-x-generic"
    property bool selected: false
    signal activated()
    signal contextMenuRequested()

    height: 48
    width: ListView.view ? ListView.view.width : 100

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        radius: 12
        color: root.selected ? Theme.secondaryContainer
             : (ma.containsMouse ? Theme.surfaceContainerHigh : "transparent")
        Behavior on color { ColorAnimation { duration: 100 } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        Image {
            source: Browser.iconUrl(root.iconName)
            sourceSize.width: 28
            sourceSize.height: 28
            width: 28
            height: 28
            asynchronous: true
            fillMode: Image.PreserveAspectFit
        }

        Text {
            Layout.fillWidth: true
            text: root.name
            color: root.selected ? Theme.onSecondaryContainer : Theme.onSurface
            font.pixelSize: 14
            elide: Text.ElideMiddle
        }

        MaterialIcon {
            visible: root.isDir
            icon: "chevron_right"
            size: 18
            color: Theme.onSurfaceVariant
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                root.contextMenuRequested()
                return
            }
            if (mouse.modifiers & Qt.ControlModifier) {
                const cur = Browser.selectedPaths.slice()
                const i = cur.indexOf(root.path)
                if (i >= 0)
                    cur.splice(i, 1)
                else
                    cur.push(root.path)
                Browser.selectedPaths = cur
            } else {
                Browser.selectedPaths = [root.path]
            }
        }
        onDoubleClicked: root.activated()
    }
}
