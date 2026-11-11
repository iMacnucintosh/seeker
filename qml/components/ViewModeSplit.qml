import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Split pill — current view label + chevron menu (Sylphrena SplitButton shape).
// Hover + mouse wheel cycles grid → list → details.
Item {
    id: root

    property bool expanded: false
    readonly property int gap: 2
    readonly property color pillColor: Theme.surfaceContainerHigh
    readonly property color inkColor: Theme.onSurface

    implicitHeight: 40
    implicitWidth: leftPill.implicitWidth + gap + expandBtn.implicitWidth
    height: implicitHeight
    width: implicitWidth
    z: expanded ? 40 : 0

    function selectMode(mode) {
        Browser.viewMode = mode
        expanded = false
    }

    function cycle(delta) {
        Browser.cycleViewMode(delta)
    }

    Row {
        id: row
        spacing: root.gap
        height: parent.height

        Rectangle {
            id: leftPill
            height: parent.height
            implicitWidth: height
            width: implicitWidth
            radius: height / 2
            topRightRadius: 8
            bottomRightRadius: 8
            color: root.pillColor

            MaterialIcon {
                anchors.centerIn: parent
                icon: Browser.viewModeIcon(Browser.viewMode)
                size: 20
                colour: root.inkColor
                fill: 1
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }
        }

        Rectangle {
            id: expandBtn
            height: parent.height
            implicitWidth: height
            width: height
            radius: height / 2
            topLeftRadius: root.expanded ? height / 2 : 8
            bottomLeftRadius: root.expanded ? height / 2 : 8
            color: root.pillColor

            Behavior on topLeftRadius { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
            Behavior on bottomLeftRadius { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MaterialIcon {
                anchors.centerIn: parent
                icon: "expand_more"
                size: 22
                colour: root.inkColor
                rotation: root.expanded ? 180 : 0
                Behavior on rotation { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }
        }
    }

    HoverHandler {
        id: hover
    }
    WheelHandler {
        enabled: hover.hovered
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            if (event.angleDelta.y > 0)
                root.cycle(-1)
            else if (event.angleDelta.y < 0)
                root.cycle(1)
            event.accepted = true
        }
    }

    // Same open motion as Sylphrena Menu (Idioma y región): opacity + yScale 0.1→1.
    MouseArea {
        id: dismiss
        parent: Overlay.overlay
        anchors.fill: parent
        enabled: root.expanded
        visible: root.expanded || menuHost.opacity > 0.01
        z: 89
        onClicked: root.expanded = false
    }

    Item {
        id: menuHost
        parent: Overlay.overlay
        z: 90
        width: Math.max(188, root.width)
        height: menuCol.implicitHeight + 16
        visible: opacity > 0.01
        opacity: root.expanded ? 1 : 0
        x: {
            if (!Overlay.overlay)
                return 0
            const p = root.mapToItem(Overlay.overlay, 0, root.height + 6)
            return Math.min(Math.max(8, p.x), Overlay.overlay.width - width - 8)
        }
        y: {
            if (!Overlay.overlay)
                return 0
            const p = root.mapToItem(Overlay.overlay, 0, root.height + 6)
            return Math.min(p.y, Overlay.overlay.height - height - 8)
        }

        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }

        transform: Scale {
            yScale: root.expanded ? 1 : 0.1
            origin.x: menuHost.width / 2
            origin.y: 0

            Behavior on yScale {
                NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: 16
            color: Theme.surfaceContainerHigh
        }

        Column {
            id: menuCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            spacing: 2

            component ModeRow: Item {
                id: modeRow
                property string modeId: ""
                property string label: ""
                property string iconName: ""
                readonly property bool active: Browser.viewMode === modeId

                width: menuCol.width
                height: 40

                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: modeRow.active ? Theme.secondaryContainer
                         : (rowMa.containsMouse ? Theme.surfaceContainerHighest : "transparent")
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    MaterialIcon {
                        Layout.alignment: Qt.AlignVCenter
                        icon: modeRow.iconName
                        size: 20
                        colour: modeRow.active ? Theme.onSecondaryContainer : Theme.onSurfaceVariant
                        fill: modeRow.active ? 1 : 0
                    }
                    StyledText {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        text: modeRow.label
                        color: modeRow.active ? Theme.onSecondaryContainer : Theme.onSurface
                        font.pixelSize: 14
                    }
                    MaterialIcon {
                        Layout.alignment: Qt.AlignVCenter
                        visible: modeRow.active
                        icon: "check"
                        size: 18
                        colour: Theme.onSecondaryContainer
                    }
                }

                MouseArea {
                    id: rowMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.selectMode(modeRow.modeId)
                }
            }

            ModeRow { modeId: "grid"; label: qsTr("Grid"); iconName: "grid_view" }
            ModeRow { modeId: "list"; label: qsTr("List"); iconName: "view_list" }
            ModeRow { modeId: "details"; label: qsTr("Details"); iconName: "view_column" }
        }
    }
}
