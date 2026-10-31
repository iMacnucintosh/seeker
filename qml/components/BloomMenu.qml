import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sylphrena.Blobs

// Bloom ⋮ menu — visual = Sylphrena.Blobs; interaction on Overlay.
// One panel MouseArea owns hover+click (per-row MouseAreas stole containsMouse
// in the gaps, which closed the bloom / dropped the blob background mid-travel).
Item {
    id: root

    property bool open: false
    property int padding: 14
    property real animDriver: 0
    default property alias content: menuCol.data

    readonly property int corner: 16
    readonly property int spaceSm: 8
    readonly property int chrome: 40
    readonly property real openWidth: Math.max(menuCol.implicitWidth, 268) + padding * 2
    readonly property real openHeight: menuCol.implicitHeight + padding * 2

    // Only the ⋮ button + the full overlay panel. Gaps between rows stay inside panelMa.
    readonly property bool hovering: btn.containsMouse || panelMa.containsMouse
    property bool suppressHover: false

    implicitWidth: chrome
    implicitHeight: chrome
    width: chrome
    height: chrome
    clip: false
    z: open ? 40 : 0

    function syncOverlay() {
        if (!Overlay.overlay)
            return

        const b = root.mapToItem(Overlay.overlay, 0, 0)
        const expanded = root.open || root.animDriver > 0.01

        const contentRight = expanded ? (b.x + root.spaceSm) : (b.x + root.width)
        const contentLeft = expanded ? (contentRight - root.openWidth) : b.x
        const contentTop = b.y
        const contentBottom = expanded ? (b.y + root.openHeight) : (b.y + root.height)

        const left = Math.min(contentLeft, b.x)
        const top = Math.min(contentTop, b.y)
        const right = Math.max(contentRight, b.x + root.width)
        const bottom = Math.max(contentBottom, b.y + root.height)

        const pad = 6
        hitLayer.x = left - pad
        hitLayer.y = top - pad
        hitLayer.width = Math.max(0, right - left) + pad * 2
        hitLayer.height = Math.max(0, bottom - top) + pad * 2

        menuHost.anchors.leftMargin = root.padding + (contentLeft - hitLayer.x)
        menuHost.anchors.topMargin = root.padding + (contentTop - hitLayer.y)
        menuHost.anchors.rightMargin = root.padding + ((hitLayer.x + hitLayer.width) - contentRight)
        menuHost.anchors.bottomMargin = root.padding + ((hitLayer.y + hitLayer.height) - contentBottom)
    }

    function syncHoverOpen() {
        if (!hovering) {
            menuHost.clearHover()
            suppressHover = false
            if (open)
                closeTimer.restart()
            return
        }
        closeTimer.stop()
        if (suppressHover)
            return
        if (!open) {
            open = true
            syncOverlay()
        }
    }

    function closeMenu() {
        closeTimer.stop()
        menuHost.clearHover()
        open = false
    }

    onHoveringChanged: syncHoverOpen()
    onOpenChanged: {
        if (open) {
            syncOverlay()
        } else {
            menuHost.clearHover()
            Qt.callLater(syncOverlay)
        }
    }
    onOpenWidthChanged: if (open) syncOverlay()
    onOpenHeightChanged: if (open) syncOverlay()

    Timer {
        id: closeTimer
        interval: 280
        onTriggered: {
            if (!root.hovering)
                root.closeMenu()
        }
    }

    // --- Visual bloom (toolbar-local) ---
    BlobGroup {
        id: blobGroup
        color: Theme.surfaceContainerHigh
        smoothing: root.corner
        cornerFill: false
    }

    Rectangle {
        id: crispBtn
        anchors.fill: parent
        radius: root.corner
        color: Theme.surfaceContainerHigh
        opacity: root.open || root.animDriver > 0 ? 0 : 1
        z: 0
        Behavior on opacity { NumberAnimation { duration: 120 } }
    }

    BlobRect {
        anchors.fill: parent
        group: blobGroup
        radius: root.corner
        opacity: root.open || root.animDriver > 0 ? 1 : 0
        z: 0
        Behavior on opacity { NumberAnimation { duration: 120 } }
    }

    BlobRect {
        id: rect
        anchors.right: parent.right
        anchors.top: parent.top
        implicitWidth: parent.width
        implicitHeight: parent.height
        group: blobGroup
        radius: root.corner
        deformScale: 0.00001
        opacity: root.open || root.animDriver > 0 ? 1 : 0
        z: 1
        Behavior on opacity { NumberAnimation { duration: 120 } }

        states: State {
            name: "open"
            when: root.open
            PropertyChanges {
                rect.anchors.rightMargin: root.width - root.spaceSm
                rect.anchors.topMargin: 0
                rect.implicitWidth: root.openWidth
                rect.implicitHeight: root.openHeight
                root.animDriver: 1
            }
        }

        transitions: Transition {
            NumberAnimation {
                properties: "rightMargin,implicitWidth"
                duration: 280
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                properties: "topMargin,implicitHeight"
                duration: 350
                easing.type: Easing.BezierSpline
                easing.bezierCurve: [0.42, 1.67, 0.21, 0.9, 1, 1]
            }
            NumberAnimation {
                properties: "animDriver"
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
    }

    MouseArea {
        id: btn
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        cursorShape: Qt.PointingHandCursor
        z: 2
        onContainsMouseChanged: root.syncHoverOpen()

        MaterialIcon {
            anchors.centerIn: parent
            icon: "more_vert"
            size: 22
            colour: Theme.onSurface
        }
    }

    // --- Overlay: dismiss outside + interactive panel ---
    Item {
        id: dismissMask
        function contains(point) {
            if (!root.open || hitLayer.width <= 0)
                return true
            const p = hitLayer.mapFromItem(dismiss, point.x, point.y)
            const pad = 10
            const inside = p.x >= -pad && p.y >= -pad
                        && p.x <= hitLayer.width + pad && p.y <= hitLayer.height + pad
            return !inside
        }
    }

    MouseArea {
        id: dismiss
        parent: Overlay.overlay
        anchors.fill: parent
        enabled: root.open
        visible: root.open
        z: 0
        hoverEnabled: false
        containmentMask: dismissMask
        onClicked: root.closeMenu()
    }

    Item {
        id: hitLayer
        parent: Overlay.overlay
        z: 1
        visible: root.open || root.animDriver > 0
        clip: true

        Item {
            id: menuHost
            anchors.fill: parent
            anchors.leftMargin: root.padding
            anchors.rightMargin: root.padding
            anchors.topMargin: root.padding
            anchors.bottomMargin: root.padding
            // Follow the bloom driver so labels don't sit at full size while the
            // blob is still growing/shrinking (hitLayer uses final geometry).
            // panelMa is a sibling on top, so fade does not block hover/click.
            opacity: root.animDriver
            visible: root.animDriver > 0.01
            z: 0

            property Item hoverItem: null

            function clearHover() {
                hoverItem = null
            }

            function rowAtFromPanel(px, py) {
                const p = menuCol.mapFromItem(panelMa, px, py)
                for (let i = 0; i < menuCol.children.length; ++i) {
                    const it = menuCol.children[i]
                    if (!it || typeof it.clicked !== "function")
                        continue
                    if (p.x >= 0 && p.x < menuCol.width
                            && p.y >= it.y && p.y < it.y + it.height)
                        return it
                }
                return null
            }

            function setHoverFromPanel(px, py) {
                const found = rowAtFromPanel(px, py)
                // No hover chrome on disabled rows.
                const target = (found && found.enabled) ? found : null
                if (hoverItem === target)
                    return
                hoverItem = target
                if (target) {
                    hoverPill.y = target.y
                    hoverPill.height = target.height
                }
            }

            function activateAt(px, py) {
                const found = rowAtFromPanel(px, py)
                if (!found || !found.enabled)
                    return
                found.clicked()
                // Keep open for toggles so the switch state is visible.
                if (found.checkable)
                    return
                root.suppressHover = true
                root.closeMenu()
            }

            Rectangle {
                id: hoverPill
                width: parent.width
                height: 40
                radius: 12
                color: {
                    if (!menuHost.hoverItem)
                        return Theme.surfaceContainerHighest
                    if (menuHost.hoverItem.dangerous)
                        return Qt.alpha(Theme.error, 0.16)
                    return Theme.surfaceContainerHighest
                }
                opacity: menuHost.hoverItem ? 1 : 0
                z: 0

                Behavior on y {
                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                }
                Behavior on height {
                    NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                }
                Behavior on opacity {
                    NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
                }
                Behavior on color {
                    ColorAnimation { duration: 120 }
                }
            }

            Column {
                id: menuCol
                width: parent.width
                spacing: 2
                z: 1
            }
        }

        // Single hit target on top — never loses containsMouse between rows.
        MouseArea {
            id: panelMa
            anchors.fill: parent
            enabled: root.open
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            z: 2
            onContainsMouseChanged: {
                if (!containsMouse)
                    menuHost.clearHover()
                root.syncHoverOpen()
            }
            onPositionChanged: (mouse) => menuHost.setHoverFromPanel(mouse.x, mouse.y)
            onEntered: menuHost.setHoverFromPanel(mouseX, mouseY)
            onClicked: (mouse) => menuHost.activateAt(mouse.x, mouse.y)
        }
    }

    Connections {
        target: Overlay.overlay
        function onWidthChanged() { if (root.open) root.syncOverlay() }
        function onHeightChanged() { if (root.open) root.syncOverlay() }
    }

    component MenuRow: Item {
        id: row
        property string icon: ""
        property string label: ""
        property bool dangerous: false
        property bool checkable: false
        property bool checked: false
        // Item.enabled is the source of truth; keep opacity in sync.
        signal clicked()

        width: parent ? parent.width : 268
        height: 40
        opacity: enabled ? 1 : 0.38

        Row {
            id: labelRow
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10

            MaterialIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                icon: row.icon
                size: 20
                colour: row.dangerous ? Theme.error : Theme.onSurfaceVariant
            }
            StyledText {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, labelRow.width - 32 - (row.checkable ? 28 : 0))
                text: row.label
                color: row.dangerous ? Theme.error : Theme.onSurface
                font.pixelSize: 14
                elide: Text.ElideRight
            }
            MaterialIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: row.checkable
                width: 22
                icon: row.checked ? "toggle_on" : "toggle_off"
                size: 22
                fill: row.checked ? 1 : 0
                colour: row.checked ? Theme.primary : Theme.onSurfaceVariant
            }
        }
    }
}
