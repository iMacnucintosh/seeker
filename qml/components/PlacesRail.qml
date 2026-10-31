import QtQuick
import QtQuick.Layouts

// Places rail — floating full-width pill (like Sylphrena filedialog Sidebar).
// MaterialShape stays on square chrome (⋮, orbs); unit-square shapes stretch
// into blobs when forced into a wide nav row.
// Drop folders onto empty rail chrome to pin; drop onto a SidePlace still moves into it.
Item {
    id: root
    property var model: []
    property bool pinDropHover: false
    signal placeActivated(string path)
    signal placeContextRequested(string path, real gX, real gY)

    implicitWidth: 240
    clip: true

    // Whole-rail pin target (SidePlace DropAreas sit above and keep move-into-folder).
    DropArea {
        anchors.fill: parent
        z: 0
        keys: ["text/uri-list"]
        onEntered: (drag) => {
            drag.accept(Qt.CopyAction | Qt.MoveAction)
            root.pinDropHover = true
        }
        onExited: root.pinDropHover = false
        onDropped: (drop) => {
            root.pinDropHover = false
            const urls = []
            for (let i = 0; i < drop.urls.length; ++i)
                urls.push(drop.urls[i].toString())
            if (urls.length)
                Browser.pinPlacesFromUrls(urls)
            drop.acceptProposedAction()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.primary
        opacity: root.pinDropHover ? 0.10 : 0
        z: 0
        Behavior on opacity { NumberAnimation { duration: 120 } }
    }

    Item {
        id: listHost
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        z: 1

        property Item activeItem: null
        property Item hoverItem: null
        property real targetY: 0
        property real targetH: 44

        function syncActive() {
            let found = null
            for (let i = 0; i < placesRep.count; ++i) {
                const it = placesRep.itemAt(i)
                if (it && it.active) {
                    found = it
                    break
                }
            }
            activeItem = found
            if (found) {
                targetY = found.y - flick.contentY
                targetH = found.height
            }
        }

        function syncHover() {
            let found = null
            for (let i = 0; i < placesRep.count; ++i) {
                const it = placesRep.itemAt(i)
                if (it && it.hovered && !it.active) {
                    found = it
                    break
                }
            }
            hoverItem = found
            if (found) {
                hoverPill.y = found.y - flick.contentY
                hoverPill.height = found.height
            }
        }

        Rectangle {
            id: hoverPill
            width: parent.width
            height: 44
            radius: height / 2
            color: Theme.surfaceContainerHigh
            opacity: listHost.hoverItem ? 1 : 0
            z: 0

            Behavior on y {
                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
            }
            Behavior on opacity {
                NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
            }
        }

        Rectangle {
            id: selection
            y: listHost.targetY
            width: parent.width
            height: listHost.targetH
            radius: (listHost.activeItem && listHost.activeItem.pressed)
                    ? Math.min(12, height / 2)
                    : height / 2
            color: Theme.primaryContainer
            opacity: listHost.activeItem ? 1 : 0
            scale: (listHost.activeItem && listHost.activeItem.pressed) ? 1.04 : 1
            z: 1

            Behavior on y {
                NumberAnimation { duration: 320; easing.type: Easing.OutCubic }
            }
            Behavior on height {
                NumberAnimation { duration: 280; easing.type: Easing.OutCubic }
            }
            Behavior on opacity {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
            Behavior on radius {
                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
            }
            Behavior on scale {
                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
            }
        }

        Flickable {
            id: flick
            anchors.fill: parent
            clip: true
            contentWidth: width
            contentHeight: placesCol.height
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            z: 2

            onContentYChanged: {
                listHost.syncActive()
                listHost.syncHover()
            }

            Column {
                id: placesCol
                width: flick.width
                spacing: 2

                Repeater {
                    id: placesRep
                    model: root.model

                    delegate: SidePlace {
                        required property var modelData
                        width: placesCol.width
                        symbol: modelData.symbol ?? "folder"
                        label: modelData.label ?? ""
                        path: modelData.path ?? ""
                        pinned: !!(modelData.pinned)
                        active: Browser.currentPath === modelData.path
                        onClicked: root.placeActivated(modelData.path)
                        onContextMenuRequested: (gX, gY) => root.placeContextRequested(modelData.path, gX, gY)
                        onActiveChanged: listHost.syncActive()
                        onHoveredChanged: listHost.syncHover()
                        Component.onCompleted: {
                            if (active)
                                listHost.syncActive()
                        }
                    }
                }
            }
        }

        Connections {
            target: Browser
            function onPathChanged() {
                Qt.callLater(listHost.syncActive)
            }
            function onPlacesChanged() {
                Qt.callLater(listHost.syncActive)
            }
        }

        Component.onCompleted: Qt.callLater(syncActive)
    }
}
