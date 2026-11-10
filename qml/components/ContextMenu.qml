import QtQuick
import QtQuick.Controls
import Sylphrena.Blobs

// Bloom context menu — same Sylphrena.Blobs morph as BloomMenu ⋮, seeded at the pointer.
// Host stays laid out (parked off-screen). Open always blooms from seed to a frozen size so
// mid-flight reflows / reopen-while-closing cannot stall the blob halfway.
Item {
    id: menuRoot

    property bool open: false
    property real popupWidth: 288
    property int padding: 14
    property real animDriver: 0
    default property alias content: menuCol.data

    readonly property int corner: 16
    readonly property int seed: 36
    readonly property real measuredWidth: Math.max(menuCol.implicitWidth, popupWidth - padding * 2) + padding * 2
    /// Frozen for the whole open/close cycle — live measured* mid-transition stalls the bloom.
    property real targetW: 240
    property real targetH: 160
    readonly property bool showing: open || animDriver > 0.01

    property real _gx: 0
    property real _gy: 0
    property real originX: 0
    property real originY: 0
    property bool _sawHover: false
    property bool dismissArmed: false
    property bool _openQueued: false

    // Zero footprint in Main; visuals live on the Overlay.
    width: 0
    height: 0

    /// Prefer design extents over layout height (avoids unpolished/zero-height races).
    function extentContentHeight() {
        let h = 0
        let n = 0
        for (let i = 0; i < menuCol.children.length; ++i) {
            const it = menuCol.children[i]
            if (!it || !it.visible)
                continue
            if (n > 0)
                h += menuCol.spacing
            h += (typeof it.menuExtent === "number") ? it.menuExtent : 40
            n++
        }
        return Math.max(h, 40)
    }

    function snapshotSize() {
        targetH = extentContentHeight() + padding * 2
        targetW = Math.max(popupWidth, measuredWidth)
    }

    function resetBloom() {
        open = false
        _openQueued = false
        reopenTimer.stop()
        animDriver = 0
        rect.implicitWidth = seed
        rect.implicitHeight = seed
    }

    function openAt(globalX, globalY) {
        _gx = globalX
        _gy = globalY
        _sawHover = false
        dismissArmed = false
        leaveTimer.stop()
        dismissArmTimer.stop()
        snapshotSize()
        placeOrigin()

        // Reopen or open-while-closing: hard reset to seed, then open next tick
        // so the state machine always runs a full seed→target transition.
        if (open || animDriver > 0.01 || _openQueued) {
            resetBloom()
            placeOrigin()
            _openQueued = true
            reopenTimer.start()
            return
        }

        open = true
        dismissArmTimer.start()
    }

    function placeOrigin() {
        const host = Overlay.overlay
        if (!host)
            return
        const local = host.mapFromGlobal(_gx, _gy)
        const w = targetW
        const h = targetH
        originX = Math.min(Math.max(8, local.x), Math.max(8, host.width - w - 8))
        originY = Math.min(Math.max(8, local.y), Math.max(8, host.height - h - 8))
        if (showing || _openQueued) {
            bloomHost.x = originX
            bloomHost.y = originY
        }
    }

    function close() {
        leaveTimer.stop()
        dismissArmTimer.stop()
        reopenTimer.stop()
        _openQueued = false
        dismissArmed = false
        menuHost.clearHover()
        open = false
    }

    onOpenChanged: {
        if (!open) {
            menuHost.clearHover()
            _sawHover = false
        } else {
            placeOrigin()
        }
    }

    onShowingChanged: {
        if (showing || _openQueued) {
            bloomHost.x = originX
            bloomHost.y = originY
        } else {
            bloomHost.x = -10000
            bloomHost.y = -10000
        }
    }

    Timer {
        id: reopenTimer
        interval: 1
        onTriggered: {
            if (!menuRoot._openQueued)
                return
            menuRoot._openQueued = false
            menuRoot.snapshotSize()
            menuRoot.placeOrigin()
            menuRoot.open = true
            menuRoot.dismissArmTimer.start()
        }
    }

    Timer {
        id: leaveTimer
        interval: 200
        onTriggered: {
            if (!panelMa.containsMouse)
                menuRoot.close()
        }
    }

    Timer {
        id: dismissArmTimer
        interval: 280
        onTriggered: menuRoot.dismissArmed = true
    }

    // --- Overlay dismiss ---
    Item {
        id: dismissMask
        function contains(point) {
            if (!menuRoot.open || !menuRoot.dismissArmed)
                return true
            const p = bloomHost.mapFromItem(dismiss, point.x, point.y)
            const pad = 10
            const w = Math.max(menuRoot.seed, menuRoot.targetW)
            const h = Math.max(menuRoot.seed, menuRoot.targetH)
            const inside = p.x >= -pad && p.y >= -pad
                        && p.x <= w + pad && p.y <= h + pad
            return !inside
        }
    }

    MouseArea {
        id: dismiss
        parent: Overlay.overlay
        anchors.fill: parent
        enabled: menuRoot.open && menuRoot.dismissArmed
        visible: menuRoot.open
        z: 80
        hoverEnabled: false
        containmentMask: dismissMask
        onClicked: menuRoot.close()
    }

    // --- Bloom host: always visible (opacity/park) so menuCol stays laid out ---
    Item {
        id: bloomHost
        parent: Overlay.overlay
        z: 81
        x: -10000
        y: -10000
        width: Math.max(menuRoot.seed, menuRoot.targetW)
        height: Math.max(menuRoot.seed, menuRoot.targetH)
        visible: true
        opacity: menuRoot.showing || menuRoot._openQueued ? 1 : 0
        clip: true

        BlobGroup {
            id: blobGroup
            color: Theme.popup
            smoothing: menuRoot.corner
            cornerFill: false
        }

        BlobRect {
            id: rect
            anchors.left: parent.left
            anchors.top: parent.top
            implicitWidth: menuRoot.seed
            implicitHeight: menuRoot.seed
            group: blobGroup
            radius: menuRoot.corner
            deformScale: 0.00001
            opacity: menuRoot.showing || menuRoot._openQueued ? 1 : 0
            z: 0
            Behavior on opacity { NumberAnimation { duration: 120 } }

            states: State {
                name: "open"
                when: menuRoot.open
                PropertyChanges {
                    rect.implicitWidth: menuRoot.targetW
                    rect.implicitHeight: menuRoot.targetH
                    menuRoot.animDriver: 1
                }
            }

            transitions: Transition {
                NumberAnimation {
                    properties: "implicitWidth"
                    duration: 280
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    properties: "implicitHeight"
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

        Item {
            id: menuHost
            anchors.fill: parent
            anchors.margins: menuRoot.padding
            opacity: menuRoot.animDriver
            visible: true
            z: 1

            property Item hoverItem: null

            function clearHover() { hoverItem = null }

            function rowAtFromPanel(px, py) {
                const p = menuCol.mapFromItem(panelMa, px, py)
                for (let i = 0; i < menuCol.children.length; ++i) {
                    const it = menuCol.children[i]
                    if (!it || typeof it.clicked !== "function")
                        continue
                    if (!it.visible || it.height <= 0)
                        continue
                    if (p.x >= 0 && p.x < menuCol.width
                            && p.y >= it.y && p.y < it.y + it.height)
                        return it
                }
                return null
            }

            function setHoverFromPanel(px, py) {
                const found = rowAtFromPanel(px, py)
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
                menuRoot.close()
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
                Behavior on y { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                Behavior on height { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            Column {
                id: menuCol
                width: parent.width
                spacing: 2
                z: 1
            }
        }

        MouseArea {
            id: panelMa
            anchors.fill: parent
            enabled: menuRoot.open
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            z: 2
            onContainsMouseChanged: {
                if (containsMouse) {
                    menuRoot._sawHover = true
                    leaveTimer.stop()
                } else {
                    menuHost.clearHover()
                    if (menuRoot._sawHover && menuRoot.open)
                        leaveTimer.restart()
                }
            }
            onPositionChanged: (mouse) => menuHost.setHoverFromPanel(mouse.x, mouse.y)
            onEntered: menuHost.setHoverFromPanel(mouseX, mouseY)
            onClicked: (mouse) => menuHost.activateAt(mouse.x, mouse.y)
        }
    }

    Shortcut {
        enabled: menuRoot.open
        sequences: ["Escape"]
        onActivated: menuRoot.close()
    }

    Connections {
        target: Overlay.overlay
        function onWidthChanged() {
            if (menuRoot.open && menuRoot.animDriver >= 1)
                menuRoot.placeOrigin()
        }
        function onHeightChanged() {
            if (menuRoot.open && menuRoot.animDriver >= 1)
                menuRoot.placeOrigin()
        }
    }

    component MenuRow: Item {
        id: row
        property string icon: ""
        property string label: ""
        property bool dangerous: false
        property int menuExtent: 40
        signal clicked()

        width: parent ? parent.width : 212
        height: visible ? menuExtent : 0
        opacity: enabled ? 1 : 0.38
        clip: true

        Row {
            id: labelRow
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10
            visible: row.visible && row.height > 0

            MaterialIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                icon: row.icon
                size: 20
                colour: row.dangerous ? Theme.error : Theme.onSurfaceVariant
            }
            StyledText {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, labelRow.width - 32)
                text: row.label
                color: row.dangerous ? Theme.error : Theme.onSurface
                font.pixelSize: 14
                elide: Text.ElideRight
            }
        }
    }

    component Separator: Item {
        property int menuExtent: 9
        width: parent ? parent.width : 212
        height: visible ? menuExtent : 0
        clip: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            height: 1
            color: Theme.outlineVariant
            opacity: 0.5
            visible: parent.visible && parent.height > 0
        }
    }
}
