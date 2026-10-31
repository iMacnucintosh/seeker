import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string symbol: ""
    property string label: ""
    property string path: ""
    property bool pinned: false
    property bool active: false
    property bool hovered: false
    property bool pressed: false
    property bool dropHover: false
    property real dropGrow: dropHover ? 1 : 0
    /// -1 insert above, 0 none, 1 insert below (pinned reorder).
    property int reorderEdge: 0
    property bool dragging: false
    /// White on mid/dark accent pills; dark ink only on very light containers.
    readonly property color accentFg: {
        const c = Theme.primaryContainer
        const lum = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b
        return lum < 0.72 ? "#ffffff" : Theme.onPrimaryContainer
    }
    readonly property color labelColor: (root.active || root.dropHover) ? root.accentFg : Theme.onSurface
    readonly property color iconColor: (root.active || root.dropHover) ? root.accentFg : Theme.onSurfaceVariant
    signal clicked()
    signal contextMenuRequested(real gX, real gY)

    implicitHeight: 44
    implicitWidth: 200
    height: 44
    property real radius: height / 2
    opacity: root.dragging ? 0.4 : 1

    Behavior on dropGrow {
        NumberAnimation {
            duration: 260
            easing.type: Easing.OutBack
            easing.overshoot: 2.2
        }
    }
    Behavior on opacity {
        NumberAnimation { duration: 120 }
    }

    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.MoveAction
    Drag.proposedAction: Qt.MoveAction
    Drag.keys: ["application/x-seeker-pin"]
    Drag.mimeData: { "application/x-seeker-pin": root.path }
    Drag.active: root.dragging

    StateLayer {
        id: layer
        z: 2
        // Pinned rows use pinInteract (click + drag-to-reorder).
        disabled: root.pinned
        radius: root.radius
        shapeMorph: true
        color: root.active ? root.accentFg : Theme.onSurface
        stateOpacity: 0
        onClicked: root.clicked()
        onContainsMouseChanged: {
            if (!root.pinned)
                root.hovered = containsMouse || root.dropHover || root.reorderEdge !== 0
        }
        onPressedChanged: {
            if (!root.pinned)
                root.pressed = pressed
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: Theme.primary
        opacity: root.dropGrow * 0.28
        border.width: root.dropGrow > 0.05 ? 2 : 0
        border.color: Qt.alpha(Theme.primary, 0.65 * root.dropGrow)
        z: 0
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 12
        spacing: 12
        z: 1

        Item {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            Layout.alignment: Qt.AlignVCenter

            MaterialIcon {
                id: iconGlyph
                anchors.centerIn: parent
                icon: root.symbol
                size: 22
                fill: root.active || root.dropHover
                colour: root.iconColor
                scale: 1 + root.dropGrow * 0.18
            }
        }

        StyledText {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: root.label
            color: root.labelColor
            font.pixelSize: 14
            font.weight: Font.Medium
            font.variableAxes: ({ "opsz": 14, "wght": 500, "ROND": 25, "GRAD": 0, "wdth": 100 })
            elide: Text.ElideRight
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        height: 3
        radius: 1.5
        y: root.reorderEdge < 0 ? 0 : parent.height - height
        color: Theme.primary
        visible: root.reorderEdge !== 0
        z: 6
    }

    DropArea {
        id: dropArea
        anchors.fill: parent
        z: 3
        keys: ["text/uri-list", "application/x-seeker-pin"]

        function isPinDrag(drag) {
            return drag.keys.indexOf("application/x-seeker-pin") >= 0
        }

        function pinPathFrom(drag) {
            return drag.getDataAsString("application/x-seeker-pin")
        }

        onEntered: (drag) => {
            if (isPinDrag(drag)) {
                const src = pinPathFrom(drag)
                if (!root.pinned || !src.length || src === root.path) {
                    drag.accepted = false
                    root.reorderEdge = 0
                    return
                }
                drag.accept(Qt.MoveAction)
                root.reorderEdge = drag.y < height / 2 ? -1 : 1
                root.hovered = true
                return
            }
            drag.accept(Qt.MoveAction | Qt.CopyAction)
            root.dropHover = true
            root.hovered = true
            root.reorderEdge = 0
        }
        onPositionChanged: (drag) => {
            if (!isPinDrag(drag) || !root.pinned)
                return
            const src = pinPathFrom(drag)
            if (!src.length || src === root.path) {
                root.reorderEdge = 0
                return
            }
            root.reorderEdge = drag.y < height / 2 ? -1 : 1
        }
        onExited: {
            root.dropHover = false
            root.reorderEdge = 0
            root.hovered = root.pinned ? pinInteract.containsMouse : layer.containsMouse
        }
        onDropped: (drop) => {
            if (isPinDrag(drop)) {
                const src = pinPathFrom(drop)
                const after = root.reorderEdge > 0
                root.reorderEdge = 0
                root.hovered = root.pinned ? pinInteract.containsMouse : layer.containsMouse
                if (root.pinned && src.length && src !== root.path)
                    Browser.reorderPinnedPlace(src, root.path, after)
                drop.acceptProposedAction()
                return
            }
            root.dropHover = false
            root.hovered = root.pinned ? pinInteract.containsMouse : layer.containsMouse
            const urls = []
            for (let i = 0; i < drop.urls.length; ++i)
                urls.push(drop.urls[i].toString())
            if (urls.length && root.path.length)
                Browser.dropUrls(urls, root.path, !!(drop.modifiers & Qt.ControlModifier))
            drop.acceptProposedAction()
        }
    }

    // Click to open + drag-to-reorder for pinned favorites.
    MouseArea {
        id: pinInteract
        anchors.fill: parent
        z: 4
        enabled: root.pinned
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        preventStealing: root.dragging

        property real pressX: 0
        property real pressY: 0
        property bool armed: false

        onContainsMouseChanged: root.hovered = containsMouse || root.dropHover || root.reorderEdge !== 0
        onPressed: (mouse) => {
            if (mouse.button !== Qt.LeftButton)
                return
            pressX = mouse.x
            pressY = mouse.y
            armed = false
            root.pressed = true
        }
        onPositionChanged: (mouse) => {
            if (!pressed || mouse.buttons === Qt.NoButton || root.dragging)
                return
            const dx = mouse.x - pressX
            const dy = mouse.y - pressY
            if (!armed && (dx * dx + dy * dy) >= 36) {
                armed = true
                root.dragging = true
                iconGlyph.grabToImage((result) => {
                    if (!root.dragging)
                        return
                    root.Drag.imageSource = result.url
                    root.Drag.hotSpot.x = result.width / 2
                    root.Drag.hotSpot.y = result.height / 2
                }, Qt.size(48, 48))
            }
        }
        onReleased: (mouse) => {
            root.pressed = false
            if (mouse.button === Qt.RightButton) {
                const g = mapToGlobal(mouse.x, mouse.y)
                root.contextMenuRequested(g.x, g.y)
                return
            }
            if (root.dragging) {
                root.dragging = false
                root.Drag.imageSource = ""
                armed = false
                return
            }
            if (!armed)
                root.clicked()
            armed = false
        }
        onCanceled: {
            root.pressed = false
            root.dragging = false
            root.Drag.imageSource = ""
            armed = false
        }
    }
}
