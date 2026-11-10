import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"

ApplicationWindow {
    id: win
    visible: true
    width: 1100
    height: 720
    minimumWidth: 720
    minimumHeight: 480
    title: Pick.active
           ? (Pick.title.length ? Pick.title : "Seleccionar")
           : (Browser.displayPath + " — Archivos")
    color: Theme.surface
    font.family: Theme.uiFamily

    property real sidebarWidth: 240
    /// Left chrome over the sidebar: nav (izq) + nueva carpeta (der) + márgenes.
    /// Refresh lives inside the PathBar (browser-style).
    readonly property int sidebarNavMinWidth: {
        const _u = Browser.showUpButton
        const _n = Browser.showNewFolderButton
        const leftN = 2 + (Browser.showUpButton ? 1 : 0)
        const rightN = Browser.showNewFolderButton ? 1 : 0
        const mid = rightN > 0 ? 4 : 0
        return 8 + leftN * 40 + Math.max(0, leftN - 1) * 4
                + mid + rightN * 40 + 8
    }
    onSidebarNavMinWidthChanged: {
        if (sidebarWidth < sidebarNavMinWidth)
            sidebarWidth = sidebarNavMinWidth
    }
    // empty | item
    // empty | item | place (pinned sidebar row)
    property string contextKind: "empty"
    property string placeContextPath: ""
    // rename | create
    property string nameDialogMode: "rename"
    readonly property bool dialogOpen: nameDialog.open || ctxMenu.open || iconPicker.open || emptyTrashDialog.open || Properties.open || OpenWith.open
    /// Folder path under the drag cursor (view DropAreas inside Grid/List often miss DnD).
    property string dropTargetPath: ""
    property bool navBusy: false
    property bool suppressViewAnim: false
    property string pendingNavPath: ""

    function activeFilesView() {
        if (Browser.viewMode === "list")
            return list
        if (Browser.viewMode === "details")
            return details
        return grid
    }

    function folderUnderPoint(mapItem, x, y) {
        const view = activeFilesView()
        if (!view.visible)
            return ""
        const p = view.contentItem.mapFromItem(mapItem, x, y)
        const item = view.itemAt(p.x, p.y)
        if (!item || !item.isDir)
            return ""
        if (Browser.selectedPaths.indexOf(item.path) >= 0)
            return ""
        return item.path
    }

    /// Paths whose delegate rect intersects the marquee (view-local coords).
    function pathsInMarquee(view, mx, my, mw, mh) {
        const paths = []
        if (!view || mw < 1 || mh < 1)
            return paths
        const c0 = view.contentItem.mapFromItem(view, mx, my)
        const c1 = view.contentItem.mapFromItem(view, mx + mw, my + mh)
        const left = Math.min(c0.x, c1.x)
        const right = Math.max(c0.x, c1.x)
        const top = Math.min(c0.y, c1.y)
        const bottom = Math.max(c0.y, c1.y)
        const kids = view.contentItem.children
        for (let i = 0; i < kids.length; ++i) {
            const it = kids[i]
            if (!it || it.path === undefined || it.path === null || it.path === "")
                continue
            if (it.x + it.width <= left || it.x >= right
                    || it.y + it.height <= top || it.y >= bottom)
                continue
            paths.push(it.path)
        }
        return paths
    }

    /// `base` = selection frozen when the marquee started (Ctrl-additive); hit ∪ base each move.
    function applyMarqueeSelection(view, mx, my, mw, mh, base) {
        const hit = pathsInMarquee(view, mx, my, mw, mh)
        if (!base || base.length === 0) {
            Browser.selectedPaths = hit
            return
        }
        const paths = base.slice()
        for (let i = 0; i < hit.length; ++i) {
            if (paths.indexOf(hit[i]) < 0)
                paths.push(hit[i])
        }
        Browser.selectedPaths = paths
    }

    function acceptDropUrls(drop, destPath) {
        const urls = []
        for (let i = 0; i < drop.urls.length; ++i)
            urls.push(drop.urls[i].toString())
        if (!urls.length)
            return
        const copy = (drop.modifiers & Qt.ControlModifier)
                  || drop.proposedAction === Qt.CopyAction
        Browser.dropUrls(urls, destPath, copy)
        drop.acceptProposedAction()
    }

    function showItemContext(gX, gY) {
        placeContextPath = ""
        contextKind = "item"
        ctxMenu.openAt(gX, gY)
    }

    function showEmptyContext(gX, gY) {
        Browser.clearSelection()
        placeContextPath = ""
        contextKind = "empty"
        ctxMenu.openAt(gX, gY)
    }

    function showPlaceContext(path, gX, gY) {
        placeContextPath = path
        contextKind = "place"
        ctxMenu.openAt(gX, gY)
    }

    function dismissMenus() {
        ctxMenu.close()
        moreMenu.open = false
    }

    function beginRename() {
        dismissMenus()
        if (Browser.selectionCount !== 1)
            return
        const paths = Browser.selectedPaths
        if (!paths.length)
            return
        nameDialogMode = "rename"
        nameField.text = paths[0].split("/").pop()
        nameDialog.open = true
        Qt.callLater(() => {
            nameField.forceActiveFocus()
            nameField.selectAll()
        })
    }

    function beginCreateFolder() {
        dismissMenus()
        nameDialogMode = "create"
        nameField.text = "Carpeta nueva"
        nameDialog.open = true
        Qt.callLater(() => {
            nameField.forceActiveFocus()
            nameField.selectAll()
        })
    }

    function confirmNameDialog() {
        const name = nameField.text.trim()
        if (!name.length)
            return
        if (nameDialogMode === "create")
            Browser.createFolder(name)
        else
            Browser.renameSelected(name)
        nameDialog.open = false
        if (win.contentItem)
            win.contentItem.forceActiveFocus()
    }

    /// Double-click / Abrir carpeta: zoom-out of current view, then settle into the new one.
    function enterFolder(path) {
        if (!path || path.length === 0 || win.navBusy)
            return
        if (path === Browser.currentPath)
            return
        win.navBusy = true
        win.pendingNavPath = path
        diveOutAnim.restart()
    }

    property var pendingSoftNav: null
    function softNavigate(call) {
        if (win.navBusy)
            return
        win.navBusy = true
        win.pendingSoftNav = call
        softOutAnim.restart()
    }

    function gridColumns() {
        return Math.max(1, Math.floor(grid.width / grid.cellWidth))
    }

    function ensureSelectionVisible() {
        const idx = Browser.currentIndex()
        if (idx < 0)
            return
        if (Browser.viewMode === "list")
            list.positionViewAtIndex(idx, ListView.Contain)
        else if (Browser.viewMode === "details")
            details.positionViewAtIndex(idx, ListView.Contain)
        else
            grid.positionViewAtIndex(idx, GridView.Contain)
    }

    function moveSelectionByKey(key) {
        const count = Browser.model.count
        if (count <= 0)
            return
        let delta = 0
        if (Browser.viewMode === "grid") {
            const cols = gridColumns()
            if (key === Qt.Key_Left)
                delta = -1
            else if (key === Qt.Key_Right)
                delta = 1
            else if (key === Qt.Key_Up)
                delta = -cols
            else if (key === Qt.Key_Down)
                delta = cols
        } else {
            // list + details
            if (key === Qt.Key_Up || key === Qt.Key_Left)
                delta = -1
            else if (key === Qt.Key_Down || key === Qt.Key_Right)
                delta = 1
        }
        if (delta === 0)
            return
        Browser.moveSelection(delta)
        ensureSelectionVisible()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: toolbar
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme.surfaceContainer
            z: 30

            RowLayout {
                anchors.fill: parent
                anchors.rightMargin: 12
                spacing: 0

                // Same width as the sidebar → PathBar always meets the favorites/content edge.
                Item {
                    id: navCluster
                    Layout.preferredWidth: win.sidebarWidth
                    Layout.minimumWidth: win.sidebarWidth
                    Layout.maximumWidth: win.sidebarWidth
                    Layout.fillHeight: true

                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        IconButton {
                            icon: "arrow_back"
                            enabled: Browser.canGoBack
                            onClicked: win.softNavigate(() => Browser.goBack())
                        }
                        IconButton {
                            icon: "arrow_forward"
                            enabled: Browser.canGoForward
                            onClicked: win.softNavigate(() => Browser.goForward())
                        }
                        IconButton {
                            visible: Browser.showUpButton
                            icon: "arrow_upward"
                            onClicked: win.softNavigate(() => Browser.goUp())
                        }
                    }

                    IconButton {
                        visible: Browser.showNewFolderButton
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        icon: "create_new_folder"
                        onClicked: win.beginCreateFolder()
                    }
                }

                PathBar {
                    id: pathBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    barHeight: 40
                    onHistoryStepRequested: (dir) => {
                        if (dir < 0 && Browser.canGoBack)
                            win.softNavigate(() => Browser.goBack())
                        else if (dir > 0 && Browser.canGoForward)
                            win.softNavigate(() => Browser.goForward())
                    }
                }

                ViewModeSplit {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredHeight: 40
                    Layout.leftMargin: 4
                }

                BloomMenu {
                    id: moreMenu
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    Layout.maximumHeight: 40
                    Layout.leftMargin: 4
                    Layout.alignment: Qt.AlignVCenter

                    BloomMenu.MenuRow {
                        icon: "undo"
                        label: "Deshacer"
                        enabled: Browser.canUndo
                        onClicked: Browser.undo()
                    }
                    BloomMenu.MenuRow {
                        icon: "select_all"
                        label: "Seleccionar todo"
                        onClicked: Browser.selectAll()
                    }
                    BloomMenu.MenuRow {
                        icon: "create_new_folder"
                        label: "Nueva carpeta"
                        onClicked: win.beginCreateFolder()
                    }
                    BloomMenu.MenuRow {
                        icon: Browser.model.showHidden ? "visibility" : "visibility_off"
                        label: "Mostrar ocultos"
                        checkable: true
                        checked: Browser.model.showHidden
                        onClicked: Browser.model.showHidden = !Browser.model.showHidden
                    }
                    BloomMenu.MenuRow {
                        icon: "image"
                        label: "Previsualizar"
                        checkable: true
                        checked: Browser.showImagePreviews
                        onClicked: Browser.showImagePreviews = !Browser.showImagePreviews
                    }
                    BloomMenu.MenuRow {
                        icon: "folder_copy"
                        label: "Vista previa de carpetas"
                        checkable: true
                        checked: Browser.showFolderPreviews
                        onClicked: Browser.showFolderPreviews = !Browser.showFolderPreviews
                    }
                    BloomMenu.MenuSlider {
                        icon: "photo_size_select_large"
                        label: "Tamaño"
                        from: 0
                        to: 4
                        value: Browser.iconSizeStep
                        onValueEdited: (v) => Browser.iconSizeStep = v
                    }
                    BloomMenu.MenuRow {
                        icon: "arrow_upward"
                        label: "Botón subir"
                        checkable: true
                        checked: Browser.showUpButton
                        onClicked: Browser.showUpButton = !Browser.showUpButton
                    }
                    BloomMenu.MenuRow {
                        icon: "refresh"
                        label: "Botón actualizar"
                        checkable: true
                        checked: Browser.showRefreshButton
                        onClicked: Browser.showRefreshButton = !Browser.showRefreshButton
                    }
                    BloomMenu.MenuRow {
                        icon: "create_new_folder"
                        label: "Botón nueva carpeta"
                        checkable: true
                        checked: Browser.showNewFolderButton
                        onClicked: Browser.showNewFolderButton = !Browser.showNewFolderButton
                    }
                    BloomMenu.MenuRow {
                        icon: "volume_up"
                        label: "Sonidos"
                        checkable: true
                        checked: Browser.soundsEnabled
                        onClicked: Browser.soundsEnabled = !Browser.soundsEnabled
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                id: sidebar
                Layout.preferredWidth: win.sidebarWidth
                Layout.minimumWidth: win.sidebarNavMinWidth
                Layout.maximumWidth: 480
                Layout.fillHeight: true
                color: Theme.surfaceContainerLow
                clip: true

                PlacesRail {
                    anchors.fill: parent
                    model: Browser.places
                    onPlaceActivated: (path) => win.softNavigate(() => Browser.navigate(path))
                    onPlaceContextRequested: (path, gX, gY) => win.showPlaceContext(path, gX, gY)
                }
            }

            // Zero-width seam (transparency made the old 8px gap look like a fat dark bar).
            // Hit target + hover pill float over the tonal edge.
            Item {
                id: splitter
                Layout.preferredWidth: 0
                Layout.fillHeight: true
                z: 20

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    width: splitMa.containsMouse || splitMa.pressed ? 3 : 0
                    height: splitMa.pressed ? 56 : 40
                    radius: width / 2
                    color: Theme.primary
                    opacity: splitMa.pressed ? 0.5 : (splitMa.containsMouse ? 0.28 : 0)
                    Behavior on width { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 140 } }
                }

                MouseArea {
                    id: splitMa
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 12
                    cursorShape: Qt.SplitHCursor
                    hoverEnabled: true
                    preventStealing: true

                    property real pressW: 0
                    property real pressX: 0

                    onPressed: (mouse) => {
                        pressW = win.sidebarWidth
                        pressX = mapToItem(win.contentItem, mouse.x, 0).x
                    }
                    onPositionChanged: (mouse) => {
                        if (!pressed)
                            return
                        const x = mapToItem(win.contentItem, mouse.x, 0).x
                        win.sidebarWidth = Math.round(
                            Math.min(480, Math.max(win.sidebarNavMinWidth, pressW + (x - pressX))))
                    }
                }
            }

            // Mat matches the sidebar so the files pane can float as a rounded card.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Frame around the card. When translucent the window itself is the frame: one more layer here
                // would stack up to an almost opaque window.
                Rectangle {
                    anchors.fill: parent
                    color: Theme.transparencyEnabled ? "transparent" : Theme.surfaceContainerLow
                }

                Rectangle {
                    id: contentPane
                    anchors.fill: parent
                    anchors.margins: 12
                    anchors.leftMargin: 8
                    radius: 10 // match Hyprland decoration.rounding
                    color: Theme.panel
                    clip: true
                    focus: true
                    activeFocusOnTab: true

                    // Single DropArea for the view: Grid/List delegate DropAreas often
                    // never see internal Drag.Automatic events, so resolve the folder
                    // under the cursor here and drive cell dropHover from dropTargetPath.
                    DropArea {
                        id: paneDrop
                        anchors.fill: parent
                        z: 0
                        keys: ["text/uri-list"]
                        onEntered: (drag) => {
                            drag.accept(Qt.MoveAction | Qt.CopyAction)
                            win.dropTargetPath = win.folderUnderPoint(paneDrop, drag.x, drag.y)
                        }
                        onPositionChanged: (drag) => {
                            win.dropTargetPath = win.folderUnderPoint(paneDrop, drag.x, drag.y)
                        }
                        onExited: win.dropTargetPath = ""
                        onDropped: (drop) => {
                            const dest = win.dropTargetPath.length ? win.dropTargetPath : Browser.currentPath
                            win.dropTargetPath = ""
                            win.acceptDropUrls(drop, dest)
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: contentPane.radius
                        color: Theme.primaryContainer
                        opacity: paneDrop.containsDrag && !win.dropTargetPath.length ? 0.12 : 0
                        z: 0
                        Behavior on opacity { NumberAnimation { duration: 100 } }
                    }

                // macOS-style trash strip — only while browsing the trash folder.
                Rectangle {
                    id: trashBanner
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: Browser.inTrash ? 44 : 0
                    visible: height > 0
                    z: 2
                    color: Theme.surfaceContainerHigh
                    topLeftRadius: contentPane.radius
                    topRightRadius: contentPane.radius
                    clip: true

                    Behavior on height {
                        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.outlineVariant
                        opacity: 0.55
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 12

                        MaterialIcon {
                            Layout.alignment: Qt.AlignVCenter
                            icon: "delete"
                            size: 20
                            fill: 0
                            colour: Theme.onSurfaceVariant
                        }
                        StyledText {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            text: {
                                if (Browser.trashIsEmpty)
                                    return "La papelera está vacía"
                                const n = Browser.trashCount
                                const items = n === 1 ? "1 elemento" : n + " elementos"
                                const size = Browser.trashStatsBusy && Browser.trashSize === 0
                                    ? "calculando tamaño…" : Browser.formatSize(Browser.trashSize)
                                return items + " · " + size + " · se eliminarán de forma permanente"
                            }
                            color: Theme.onSurfaceVariant
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredHeight: 32
                            Layout.preferredWidth: emptyLabel.implicitWidth + 28
                            radius: 16
                            color: emptyMa.containsMouse ? Theme.error : Qt.alpha(Theme.error, 0.85)
                            opacity: Browser.trashIsEmpty ? 0.38 : 1
                            Behavior on color { ColorAnimation { duration: 80 } }

                            StyledText {
                                id: emptyLabel
                                anchors.centerIn: parent
                                text: "Vaciar papelera"
                                color: Theme.onError
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                            MouseArea {
                                id: emptyMa
                                anchors.fill: parent
                                enabled: !Browser.trashIsEmpty
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: emptyTrashDialog.open = true
                            }
                        }
                    }
                }

                Item {
                    id: viewLayer
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: trashBanner.bottom
                    anchors.bottom: parent.bottom
                    z: 1
                    transformOrigin: Item.Center

                    SequentialAnimation {
                        id: diveOutAnim
                        ParallelAnimation {
                            NumberAnimation {
                                target: viewLayer
                                property: "opacity"
                                to: 0
                                duration: 130
                                easing.type: Easing.InCubic
                            }
                            NumberAnimation {
                                target: viewLayer
                                property: "scale"
                                to: 1.07
                                duration: 150
                                easing.type: Easing.InCubic
                            }
                        }
                        ScriptAction {
                            script: {
                                win.suppressViewAnim = true
                                Browser.openPath(win.pendingNavPath)
                                win.suppressViewAnim = false
                                viewLayer.scale = 0.93
                                diveInAnim.restart()
                            }
                        }
                    }
                    ParallelAnimation {
                        id: diveInAnim
                        NumberAnimation {
                            target: viewLayer
                            property: "opacity"
                            to: 1
                            duration: 200
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: viewLayer
                            property: "scale"
                            to: 1
                            duration: 240
                            easing.type: Easing.OutBack
                            easing.overshoot: 1.2
                        }
                        onFinished: win.navBusy = false
                    }
                    SequentialAnimation {
                        id: softOutAnim
                        ParallelAnimation {
                            NumberAnimation {
                                target: viewLayer
                                property: "opacity"
                                to: 0
                                duration: 100
                                easing.type: Easing.InCubic
                            }
                            NumberAnimation {
                                target: viewLayer
                                property: "scale"
                                to: 0.97
                                duration: 100
                                easing.type: Easing.InCubic
                            }
                        }
                        ScriptAction {
                            script: {
                                win.suppressViewAnim = true
                                if (typeof win.pendingSoftNav === "function")
                                    win.pendingSoftNav()
                                win.pendingSoftNav = null
                                win.suppressViewAnim = false
                                viewLayer.scale = 1.03
                                softInAnim.restart()
                            }
                        }
                    }
                    ParallelAnimation {
                        id: softInAnim
                        NumberAnimation {
                            target: viewLayer
                            property: "opacity"
                            to: 1
                            duration: 160
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: viewLayer
                            property: "scale"
                            to: 1
                            duration: 180
                            easing.type: Easing.OutCubic
                        }
                        onFinished: win.navBusy = false
                    }

                    ListView {
                        id: list
                        anchors.fill: parent
                        anchors.margins: 8
                        visible: Browser.viewMode === "list"
                        clip: true
                        model: Browser.model
                        spacing: 2
                        boundsBehavior: Flickable.StopAtBounds
                        interactive: !listMarquee.selecting

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: (eventPoint) => {
                                const g = list.mapToGlobal(eventPoint.position.x, eventPoint.position.y)
                                win.showEmptyContext(g.x, g.y)
                            }
                        }

                        // Empty-area marquee: press on a row → mouse.accepted=false so FileRow gets it.
                        MouseArea {
                            id: listMarquee
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            preventStealing: selecting
                            z: 1

                            property real ox: 0
                            property real oy: 0
                            property real cx: 0
                            property real cy: 0
                            property bool selecting: false
                            property bool additive: false
                            property bool owned: false
                            property var basePaths: []

                            function syncSelection() {
                                const mx = Math.min(ox, cx)
                                const my = Math.min(oy, cy)
                                const mw = Math.abs(cx - ox)
                                const mh = Math.abs(cy - oy)
                                win.applyMarqueeSelection(list, mx, my, mw, mh, basePaths)
                            }

                            Rectangle {
                                visible: listMarquee.selecting
                                x: Math.min(listMarquee.ox, listMarquee.cx)
                                y: Math.min(listMarquee.oy, listMarquee.cy)
                                width: Math.abs(listMarquee.cx - listMarquee.ox)
                                height: Math.abs(listMarquee.cy - listMarquee.oy)
                                color: Qt.alpha(Theme.primary, 0.18)
                                border.width: 1
                                border.color: Qt.alpha(Theme.primary, 0.55)
                                radius: 4
                                z: 10
                            }

                            onPressed: (mouse) => {
                                const p = list.contentItem.mapFromItem(listMarquee, mouse.x, mouse.y)
                                if (list.itemAt(p.x, p.y)) {
                                    owned = false
                                    mouse.accepted = false
                                    return
                                }
                                owned = true
                                selecting = false
                                additive = !!(mouse.modifiers & Qt.ControlModifier)
                                basePaths = additive ? Browser.selectedPaths.slice() : []
                                ox = cx = mouse.x
                                oy = cy = mouse.y
                            }
                            onPositionChanged: (mouse) => {
                                if (!owned || !pressed)
                                    return
                                cx = mouse.x
                                cy = mouse.y
                                const dx = cx - ox
                                const dy = cy - oy
                                if (!selecting && (dx * dx + dy * dy) >= 16)
                                    selecting = true
                                if (selecting)
                                    syncSelection()
                            }
                            onReleased: (mouse) => {
                                if (!owned) {
                                    selecting = false
                                    return
                                }
                                if (selecting)
                                    syncSelection()
                                else
                                    Browser.clearSelection()
                                selecting = false
                                owned = false
                                basePaths = []
                            }
                            onCanceled: {
                                selecting = false
                                owned = false
                                basePaths = []
                            }
                        }

                        delegate: Item {
                            id: rowWrap
                            required property string name
                            required property string path
                            required property bool isDir
                            required property bool isHidden
                        required property string iconName
                        required property bool isImage
                        required property bool canPreview

                        width: ListView.view.width
                        height: fileRow.height

                        FileRow {
                            id: fileRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            name: rowWrap.name
                            path: rowWrap.path
                            isDir: rowWrap.isDir
                            iconName: rowWrap.iconName
                            isImage: rowWrap.isImage
                            canPreview: rowWrap.canPreview
                            selected: Browser.selectedPaths.indexOf(rowWrap.path) >= 0
                            dropHover: rowWrap.isDir && rowWrap.path === win.dropTargetPath
                            opacity: rowWrap.isHidden ? 0.55 : 1
                            onActivated: {
                                if (rowWrap.isDir) {
                                    win.enterFolder(rowWrap.path)
                                } else if (Pick.active && Pick.mode === "file") {
                                    Pick.confirmPaths([rowWrap.path])
                                } else if (!Pick.active) {
                                    Browser.openPath(rowWrap.path)
                                }
                            }
                            onRenameRequested: {
                                if (!Pick.active)
                                    win.beginRename()
                            }
                            onContextMenuRequested: (gX, gY) => win.showItemContext(gX, gY)
                        }
                    }
                }

                    GridView {
                        id: grid
                        anchors.fill: parent
                        anchors.margins: 12
                        visible: Browser.viewMode === "grid"
                        clip: true
                        model: Browser.model
                        cellWidth: Browser.iconSize * 2
                        cellHeight: Browser.iconSize * 2 + 6
                        boundsBehavior: Flickable.StopAtBounds
                        interactive: !gridMarquee.selecting

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: (eventPoint) => {
                                const g = grid.mapToGlobal(eventPoint.position.x, eventPoint.position.y)
                                win.showEmptyContext(g.x, g.y)
                            }
                        }

                        // Empty-area marquee: press on a cell → mouse.accepted=false so FileCell gets it.
                        MouseArea {
                            id: gridMarquee
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            preventStealing: selecting
                            z: 1

                            property real ox: 0
                            property real oy: 0
                            property real cx: 0
                            property real cy: 0
                            property bool selecting: false
                            property bool additive: false
                            property bool owned: false
                            property var basePaths: []

                            function syncSelection() {
                                const mx = Math.min(ox, cx)
                                const my = Math.min(oy, cy)
                                const mw = Math.abs(cx - ox)
                                const mh = Math.abs(cy - oy)
                                win.applyMarqueeSelection(grid, mx, my, mw, mh, basePaths)
                            }

                            Rectangle {
                                visible: gridMarquee.selecting
                                x: Math.min(gridMarquee.ox, gridMarquee.cx)
                                y: Math.min(gridMarquee.oy, gridMarquee.cy)
                                width: Math.abs(gridMarquee.cx - gridMarquee.ox)
                                height: Math.abs(gridMarquee.cy - gridMarquee.oy)
                                color: Qt.alpha(Theme.primary, 0.18)
                                border.width: 1
                                border.color: Qt.alpha(Theme.primary, 0.55)
                                radius: 4
                                z: 10
                            }

                            onPressed: (mouse) => {
                                const p = grid.contentItem.mapFromItem(gridMarquee, mouse.x, mouse.y)
                                if (grid.itemAt(p.x, p.y)) {
                                    owned = false
                                    mouse.accepted = false
                                    return
                                }
                                owned = true
                                selecting = false
                                additive = !!(mouse.modifiers & Qt.ControlModifier)
                                basePaths = additive ? Browser.selectedPaths.slice() : []
                                ox = cx = mouse.x
                                oy = cy = mouse.y
                            }
                            onPositionChanged: (mouse) => {
                                if (!owned || !pressed)
                                    return
                                cx = mouse.x
                                cy = mouse.y
                                const dx = cx - ox
                                const dy = cy - oy
                                if (!selecting && (dx * dx + dy * dy) >= 16)
                                    selecting = true
                                if (selecting)
                                    syncSelection()
                            }
                            onReleased: (mouse) => {
                                if (!owned) {
                                    selecting = false
                                    return
                                }
                                if (selecting)
                                    syncSelection()
                                else
                                    Browser.clearSelection()
                                selecting = false
                                owned = false
                                basePaths = []
                            }
                            onCanceled: {
                                selecting = false
                                owned = false
                                basePaths = []
                            }
                        }

                        delegate: Item {
                            id: cellWrap
                            required property string name
                            required property string path
                            required property bool isDir
                            required property bool isHidden
                            required property string iconName
                            required property bool isImage
                            required property bool canPreview

                            width: grid.cellWidth
                            height: grid.cellHeight

                            FileCell {
                                anchors.centerIn: parent
                                name: cellWrap.name
                                path: cellWrap.path
                                isDir: cellWrap.isDir
                                iconName: cellWrap.iconName
                                isImage: cellWrap.isImage
                                canPreview: cellWrap.canPreview
                                selected: Browser.selectedPaths.indexOf(cellWrap.path) >= 0
                                dropHover: cellWrap.isDir && cellWrap.path === win.dropTargetPath
                                opacity: cellWrap.isHidden ? 0.55 : 1
                                onActivated: {
                                    if (cellWrap.isDir) {
                                        win.enterFolder(cellWrap.path)
                                    } else if (Pick.active && Pick.mode === "file") {
                                        Pick.confirmPaths([cellWrap.path])
                                    } else if (!Pick.active) {
                                        Browser.openPath(cellWrap.path)
                                    }
                                }
                                onRenameRequested: {
                                    if (!Pick.active)
                                        win.beginRename()
                                }
                                onContextMenuRequested: (gX, gY) => win.showItemContext(gX, gY)
                            }
                        }
                    }

                    Column {
                        id: detailsPane
                        anchors.fill: parent
                        anchors.margins: 8
                        visible: Browser.viewMode === "details"
                        spacing: 0

                        // Column headers
                        Item {
                            width: parent.width
                            height: 28

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 12
                                spacing: 8

                                Item { Layout.preferredWidth: 20 }
                                StyledText {
                                    Layout.fillWidth: true
                                    text: "Nombre"
                                    color: Theme.onSurfaceVariant
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }
                                StyledText {
                                    Layout.preferredWidth: 88
                                    horizontalAlignment: Text.AlignRight
                                    text: "Tamaño"
                                    color: Theme.onSurfaceVariant
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }
                                StyledText {
                                    Layout.preferredWidth: 132
                                    horizontalAlignment: Text.AlignRight
                                    text: "Modificado"
                                    color: Theme.onSurfaceVariant
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                height: 1
                                color: Theme.outlineVariant
                                opacity: 0.45
                            }
                        }

                        ListView {
                            id: details
                            width: parent.width
                            height: parent.height - 28
                            clip: true
                            model: Browser.model
                            spacing: 0
                            boundsBehavior: Flickable.StopAtBounds
                            interactive: !detailsMarquee.selecting

                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: (eventPoint) => {
                                    const g = details.mapToGlobal(eventPoint.position.x, eventPoint.position.y)
                                    win.showEmptyContext(g.x, g.y)
                                }
                            }

                            MouseArea {
                                id: detailsMarquee
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                preventStealing: selecting
                                z: 1

                                property real ox: 0
                                property real oy: 0
                                property real cx: 0
                                property real cy: 0
                                property bool selecting: false
                                property bool additive: false
                                property bool owned: false
                                property var basePaths: []

                                function syncSelection() {
                                    const mx = Math.min(ox, cx)
                                    const my = Math.min(oy, cy)
                                    const mw = Math.abs(cx - ox)
                                    const mh = Math.abs(cy - oy)
                                    win.applyMarqueeSelection(details, mx, my, mw, mh, basePaths)
                                }

                                Rectangle {
                                    visible: detailsMarquee.selecting
                                    x: Math.min(detailsMarquee.ox, detailsMarquee.cx)
                                    y: Math.min(detailsMarquee.oy, detailsMarquee.cy)
                                    width: Math.abs(detailsMarquee.cx - detailsMarquee.ox)
                                    height: Math.abs(detailsMarquee.cy - detailsMarquee.oy)
                                    color: Qt.alpha(Theme.primary, 0.18)
                                    border.width: 1
                                    border.color: Qt.alpha(Theme.primary, 0.55)
                                    radius: 4
                                    z: 10
                                }

                                onPressed: (mouse) => {
                                    const p = details.contentItem.mapFromItem(detailsMarquee, mouse.x, mouse.y)
                                    if (details.itemAt(p.x, p.y)) {
                                        owned = false
                                        mouse.accepted = false
                                        return
                                    }
                                    owned = true
                                    selecting = false
                                    additive = !!(mouse.modifiers & Qt.ControlModifier)
                                    basePaths = additive ? Browser.selectedPaths.slice() : []
                                    ox = cx = mouse.x
                                    oy = cy = mouse.y
                                }
                                onPositionChanged: (mouse) => {
                                    if (!owned || !pressed)
                                        return
                                    cx = mouse.x
                                    cy = mouse.y
                                    const dx = cx - ox
                                    const dy = cy - oy
                                    if (!selecting && (dx * dx + dy * dy) >= 16)
                                        selecting = true
                                    if (selecting)
                                        syncSelection()
                                }
                                onReleased: (mouse) => {
                                    if (!owned) {
                                        selecting = false
                                        return
                                    }
                                    if (selecting)
                                        syncSelection()
                                    else
                                        Browser.clearSelection()
                                    selecting = false
                                    owned = false
                                    basePaths = []
                                }
                                onCanceled: {
                                    selecting = false
                                    owned = false
                                    basePaths = []
                                }
                            }

                            delegate: Item {
                                id: detailsWrap
                                required property string name
                                required property string path
                                required property bool isDir
                                required property bool isHidden
                                required property string iconName
                                required property bool isImage
                                required property bool canPreview
                                required property real size
                                required property string modifiedText

                                width: ListView.view.width
                                height: fileDetails.height

                                FileDetailsRow {
                                    id: fileDetails
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    name: detailsWrap.name
                                    path: detailsWrap.path
                                    isDir: detailsWrap.isDir
                                    iconName: detailsWrap.iconName
                                    isImage: detailsWrap.isImage
                                    canPreview: detailsWrap.canPreview
                                    sizeText: Browser.formatSize(detailsWrap.size)
                                    modifiedText: detailsWrap.modifiedText
                                    selected: Browser.selectedPaths.indexOf(detailsWrap.path) >= 0
                                    dropHover: detailsWrap.isDir && detailsWrap.path === win.dropTargetPath
                                    opacity: detailsWrap.isHidden ? 0.55 : 1
                                    onActivated: {
                                        if (detailsWrap.isDir) {
                                            win.enterFolder(detailsWrap.path)
                                        } else if (Pick.active && Pick.mode === "file") {
                                            Pick.confirmPaths([detailsWrap.path])
                                        } else if (!Pick.active) {
                                            Browser.openPath(detailsWrap.path)
                                        }
                                    }
                                    onRenameRequested: {
                                        if (!Pick.active)
                                            win.beginRename()
                                    }
                                    onContextMenuRequested: (gX, gY) => win.showItemContext(gX, gY)
                                }
                            }
                        }
                    }

                    StyledText {
                        anchors.centerIn: parent
                        visible: {
                            if (Browser.viewMode === "list")
                                return list.count === 0
                            if (Browser.viewMode === "details")
                                return details.count === 0
                            return grid.count === 0
                        }
                        text: "Carpeta vacía"
                        color: Theme.onSurfaceVariant
                        font.pixelSize: 14
                    }
                }
                }
            }
        }

        // Portal / CLI pick chrome
        Rectangle {
            visible: Pick.active
            Layout.fillWidth: true
            Layout.preferredHeight: Pick.mode === "save" ? 72 : 64
            color: Theme.surfaceContainer

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                spacing: 12

                TextField {
                    id: saveNameField
                    visible: Pick.mode === "save"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: Pick.suggestedName
                    placeholderText: "Nombre del archivo"
                    color: Theme.onSurface
                    placeholderTextColor: Theme.onSurfaceVariant
                    selectedTextColor: Theme.onPrimary
                    selectionColor: Theme.primary
                    font.pixelSize: 14
                    leftPadding: 14
                    rightPadding: 14
                    background: Rectangle {
                        radius: 12
                        color: Theme.surfaceContainerHighest
                    }
                    onTextChanged: Pick.suggestedName = text
                    Keys.onReturnPressed: {
                        if (Pick.canConfirm)
                            Pick.confirm()
                    }
                }

                Item {
                    visible: Pick.mode !== "save"
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.preferredWidth: cancelLabel.implicitWidth + 28
                    Layout.preferredHeight: 40
                    radius: 20
                    color: Theme.surfaceContainerHighest

                    Text {
                        id: cancelLabel
                        anchors.centerIn: parent
                        text: "Cancelar"
                        color: Theme.onSurface
                        font.pixelSize: 14
                    }
                    StateLayer {
                        anchors.fill: parent
                        radius: parent.radius
                        onClicked: Pick.cancel()
                    }
                }

                Rectangle {
                    Layout.preferredWidth: Math.max(acceptLabel.implicitWidth + 28, 96)
                    Layout.preferredHeight: 40
                    radius: 20
                    color: Pick.canConfirm ? Theme.primary : Theme.surfaceContainerHighest
                    opacity: Pick.canConfirm ? 1 : 0.55

                    Text {
                        id: acceptLabel
                        anchors.centerIn: parent
                        text: Pick.acceptLabel
                        color: Pick.canConfirm ? Theme.onPrimary : Theme.onSurfaceVariant
                        font.pixelSize: 14
                        font.weight: Font.Medium
                    }
                    StateLayer {
                        anchors.fill: parent
                        radius: parent.radius
                        disabled: !Pick.canConfirm
                        onClicked: Pick.confirm()
                    }
                }
            }
        }
    }

    ContextMenu {
        id: ctxMenu

        // —— empty background ——
        ContextMenu.MenuRow {
            visible: win.contextKind === "empty"
            icon: "undo"
            label: "Deshacer"
            enabled: Browser.canUndo
            onClicked: Browser.undo()
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "empty"
            icon: "create_new_folder"
            label: "Nueva carpeta"
            onClicked: win.beginCreateFolder()
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "empty"
            icon: "terminal"
            label: "Abrir terminal aquí"
            onClicked: Browser.openTerminalHere()
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "empty"
            icon: "content_paste"
            label: "Pegar"
            enabled: Browser.hasClipboard
            onClicked: Browser.paste()
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "empty"
            icon: "select_all"
            label: "Seleccionar todo"
            onClicked: Browser.selectAll()
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "empty"
            icon: "refresh"
            label: "Actualizar"
            onClicked: Browser.refresh()
        }

        // —— selection ——
        ContextMenu.MenuRow {
            visible: win.contextKind === "item"
            icon: "undo"
            label: "Deshacer"
            enabled: Browser.canUndo
            onClicked: Browser.undo()
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item"
            icon: "open_in_new"
            label: Browser.selectionIsDir ? "Abrir" : "Abrir"
            enabled: Browser.selectionCount === 1
            onClicked: {
                if (!Browser.selectedPaths.length)
                    return
                if (Browser.selectionIsDir)
                    win.enterFolder(Browser.selectedPaths[0])
                else
                    Browser.openPath(Browser.selectedPaths[0])
            }
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item" && !Browser.selectionIsDir
            icon: "apps"
            label: "Abrir con…"
            enabled: Browser.selectionCount === 1
            onClicked: {
                if (Browser.selectedPaths.length)
                    OpenWith.openPath(Browser.selectedPaths[0])
            }
        }
        ContextMenu.Separator {
            visible: win.contextKind === "item"
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item"
            icon: "content_copy"
            label: "Copiar"
            enabled: Browser.selectionCount > 0
            onClicked: Browser.copySelection()
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item"
            icon: "content_cut"
            label: "Cortar"
            enabled: Browser.selectionCount > 0
            onClicked: Browser.cutSelection()
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item"
            icon: "content_paste"
            label: "Pegar"
            enabled: Browser.hasClipboard
            onClicked: Browser.paste()
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item"
            icon: "control_point_duplicate"
            label: "Duplicar"
            enabled: Browser.selectionCount === 1
            onClicked: Browser.duplicateSelection()
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item" && Archive.available
            icon: "folder_zip"
            label: "Comprimir en ZIP"
            enabled: Browser.selectionCount > 0 && !Archive.busy
            onClicked: Archive.compress(Browser.selectedPaths)
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item" && Browser.selectionCount === 1
                     && Browser.selectedPaths.length > 0 && Archive.canExtract(Browser.selectedPaths[0])
            icon: "unarchive"
            label: "Extraer aquí"
            enabled: !Archive.busy
            onClicked: Archive.extractHere(Browser.selectedPaths[0])
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item"
            icon: "drive_file_rename_outline"
            label: "Renombrar"
            enabled: Browser.selectionCount === 1
            onClicked: win.beginRename()
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item" && Browser.selectionIsDir
            icon: "category"
            label: "Cambiar icono…"
            enabled: Browser.selectionCount === 1
            onClicked: {
                if (Browser.selectedPaths.length)
                    iconPicker.begin(Browser.selectedPaths[0])
            }
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item"
                     && Browser.selectionIsDir
                     && Browser.selectionCount === 1
                     && Browser.selectedPaths.length
                     && !Browser.isBuiltinPlace(Browser.selectedPaths[0])
            icon: Browser.selectedPaths.length && Browser.isPinned(Browser.selectedPaths[0])
                  ? "bookmark_remove" : "bookmark_add"
            label: Browser.selectedPaths.length && Browser.isPinned(Browser.selectedPaths[0])
                   ? "Quitar de la barra lateral" : "Añadir a la barra lateral"
            onClicked: {
                if (!Browser.selectedPaths.length)
                    return
                const p = Browser.selectedPaths[0]
                if (Browser.isPinned(p))
                    Browser.unpinPlace(p)
                else
                    Browser.pinPlace(p)
            }
        }

        // —— pinned sidebar place ——
        ContextMenu.MenuRow {
            visible: win.contextKind === "place"
            icon: "bookmark_remove"
            label: "Quitar de la barra lateral"
            enabled: win.placeContextPath.length > 0
            onClicked: {
                if (win.placeContextPath.length)
                    Browser.unpinPlace(win.placeContextPath)
            }
        }
        ContextMenu.Separator {
            visible: win.contextKind === "item"
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item"
            icon: "info"
            label: "Detalles"
            enabled: Browser.selectionCount > 0
            onClicked: Properties.openPaths(Browser.selectedPaths)
        }
        ContextMenu.MenuRow {
            visible: win.contextKind === "item"
            icon: "delete"
            label: Browser.inTrash ? "Eliminar definitivamente" : "Mover a la papelera"
            dangerous: true
            enabled: Browser.selectionCount > 0
            onClicked: Browser.trashSelection()
        }
    }

    TaskToast {
        id: taskToast
        busy: Archive.busy
        busyText: Archive.taskText
        busyDetail: Archive.progress >= 0
                    ? Math.round(Archive.progress * 100) + " %" + (Archive.detail.length ? " · " + Archive.detail : "")
                    : Archive.detail
        onCancelRequested: Archive.cancel()
    }
    // Bottom-edge bar for big jobs (shown only if the job lasts a moment)
    ProgressStrip {
        busy: Archive.busy
        progress: Archive.progress
    }
    Connections {
        target: Archive
        function onFinished(ok, message, resultPath) {
            taskToast.show(message, !ok && message !== "Cancelado")
        }
    }

    IconPicker {
        id: iconPicker
    }

    PropertiesDialog {
        id: propertiesDialog
    }

    OpenWithDialog {
        id: openWithDialog
    }

    // Confirm permanent empty-trash.
    Item {
        id: emptyTrashDialog
        property bool open: false
        property real progress: open ? 1 : 0
        anchors.fill: parent
        visible: progress > 0.01
        z: 105

        Behavior on progress {
            NumberAnimation {
                duration: emptyTrashDialog.open ? 220 : 150
                easing.type: emptyTrashDialog.open ? Easing.OutBack : Easing.InCubic
                easing.overshoot: 1.12
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: emptyTrashDialog.progress * 0.45
        }
        MouseArea {
            anchors.fill: parent
            enabled: emptyTrashDialog.open
            onClicked: emptyTrashDialog.open = false
        }

        Rectangle {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: (1 - emptyTrashDialog.progress) * 16
            width: 360
            height: 188
            radius: 20
            color: Theme.popup
            border.width: 1
            border.color: Theme.outlineVariant
            opacity: emptyTrashDialog.progress
            scale: 0.94 + emptyTrashDialog.progress * 0.06

            MouseArea { anchors.fill: parent; z: -1; onClicked: {} }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10

                StyledText {
                    text: "¿Vaciar la papelera?"
                    color: Theme.onSurface
                    font.pixelSize: 16
                    font.weight: Font.Medium
                }
                StyledText {
                    Layout.fillWidth: true
                    text: Browser.trashStatsBusy && Browser.trashSize === 0
                          ? "Se eliminarán definitivamente todos los elementos. Esta acción no se puede deshacer."
                          : "Se eliminarán definitivamente " + (Browser.trashCount === 1 ? "1 elemento" : Browser.trashCount + " elementos")
                            + " (" + Browser.formatSize(Browser.trashSize) + "). Esta acción no se puede deshacer."
                    color: Theme.onSurfaceVariant
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 8
                    IconButton {
                        icon: "close"
                        onClicked: emptyTrashDialog.open = false
                    }
                    Rectangle {
                        width: confirmEmptyLabel.implicitWidth + 28
                        height: 40
                        radius: 20
                        color: Theme.error
                        StyledText {
                            id: confirmEmptyLabel
                            anchors.centerIn: parent
                            text: "Vaciar"
                            color: Theme.onError
                            font.pixelSize: 14
                            font.weight: Font.Medium
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                emptyTrashDialog.open = false
                                Browser.emptyTrash()
                            }
                        }
                    }
                }
            }
        }
    }

    // Name dialog — rename existing OR create folder (only mkdir on confirm).
    Item {
        id: nameDialog
        property bool open: false
        property real progress: open ? 1 : 0
        anchors.fill: parent
        visible: progress > 0.01
        z: 100

        Behavior on progress {
            NumberAnimation {
                duration: nameDialog.open ? 240 : 160
                easing.type: nameDialog.open ? Easing.OutBack : Easing.InCubic
                easing.overshoot: 1.15
            }
        }

        // Scrim
        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: nameDialog.progress * 0.45
        }

        MouseArea {
            anchors.fill: parent
            enabled: nameDialog.open
            onClicked: nameDialog.open = false
        }

        Rectangle {
            id: nameCard
            anchors.centerIn: parent
            anchors.verticalCenterOffset: (1 - nameDialog.progress) * 18
            width: 360
            height: 160
            radius: 20
            color: Theme.popup
            border.width: 1
            border.color: Theme.outlineVariant
            opacity: nameDialog.progress
            scale: 0.92 + nameDialog.progress * 0.08
            transformOrigin: Item.Center

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12

                StyledText {
                    text: win.nameDialogMode === "create" ? "Nueva carpeta" : "Renombrar"
                    color: Theme.onSurface
                    font.pixelSize: 16
                    font.weight: Font.Medium
                }

                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    color: Theme.onSurface
                    selectedTextColor: Theme.onPrimary
                    selectionColor: Theme.primary
                    font.family: Theme.uiFamily
                    font.pixelSize: 14
                    background: Rectangle {
                        radius: 12
                        color: Theme.surfaceContainerHighest
                    }
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 10
                    bottomPadding: 10
                    Keys.onReturnPressed: win.confirmNameDialog()
                    Keys.onEnterPressed: win.confirmNameDialog()
                    Keys.onEscapePressed: nameDialog.open = false
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 8

                    IconButton {
                        icon: "close"
                        onClicked: nameDialog.open = false
                    }
                    Rectangle {
                        width: 40
                        height: 40
                        radius: 20
                        color: Theme.primary
                        opacity: nameField.text.trim().length ? 1 : 0.38
                        MaterialIcon {
                            anchors.centerIn: parent
                            icon: win.nameDialogMode === "create" ? "add" : "check"
                            size: 22
                            colour: Theme.onPrimary
                        }
                        MouseArea {
                            anchors.fill: parent
                            enabled: nameField.text.trim().length > 0
                            cursorShape: Qt.PointingHandCursor
                            onClicked: win.confirmNameDialog()
                        }
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                z: -1
                onClicked: {}
            }
        }
    }

    readonly property bool navEnabled: !nameDialog.open && !iconPicker.open && !emptyTrashDialog.open && !Properties.open && !OpenWith.open && !pathBar.editing && !ctxMenu.open

    Shortcut {
        enabled: win.navEnabled
        sequences: ["Up"]
        onActivated: win.moveSelectionByKey(Qt.Key_Up)
    }
    Shortcut {
        enabled: win.navEnabled
        sequences: ["Down"]
        onActivated: win.moveSelectionByKey(Qt.Key_Down)
    }
    Shortcut {
        enabled: win.navEnabled
        sequences: ["Left"]
        onActivated: win.moveSelectionByKey(Qt.Key_Left)
    }
    Shortcut {
        enabled: win.navEnabled
        sequences: ["Right"]
        onActivated: win.moveSelectionByKey(Qt.Key_Right)
    }
    Shortcut {
        enabled: win.navEnabled
        sequences: ["Home"]
        onActivated: {
            Browser.selectIndex(0)
            win.ensureSelectionVisible()
        }
    }
    Shortcut {
        enabled: win.navEnabled
        sequences: ["End"]
        onActivated: {
            Browser.selectIndex(Math.max(0, Browser.model.count - 1))
            win.ensureSelectionVisible()
        }
    }

    // Ctrl+Z / Delete: also handled in C++ ShellKeyFilter — QML Shortcut flakes on Wayland.
    Shortcut {
        enabled: !nameDialog.open && !iconPicker.open
        sequences: [StandardKey.Copy]
        onActivated: Browser.copySelection()
    }
    Shortcut {
        enabled: !nameDialog.open && !iconPicker.open
        sequences: [StandardKey.Cut]
        onActivated: Browser.cutSelection()
    }
    Shortcut {
        enabled: !nameDialog.open && !iconPicker.open
        sequences: [StandardKey.Paste]
        onActivated: Browser.paste()
    }
    Shortcut {
        enabled: !nameDialog.open && !iconPicker.open
        sequences: [StandardKey.SelectAll]
        onActivated: Browser.selectAll()
    }
    Shortcut {
        // Backup; ShellKeyFilter is the reliable path on Wayland.
        enabled: !nameDialog.open && !iconPicker.open && !pathBar.editing
        context: Qt.ApplicationShortcut
        sequences: [StandardKey.Delete, "Delete"]
        onActivated: {
            if (Browser.selectionCount > 0)
                Browser.trashSelection()
        }
    }
    Shortcut {
        enabled: !nameDialog.open && !win.navBusy
        sequences: ["Backspace"]
        onActivated: win.softNavigate(() => Browser.goUp())
    }
    Shortcut {
        enabled: !nameDialog.open
        sequences: ["F2"]
        onActivated: win.beginRename()
    }
    Shortcut {
        enabled: !nameDialog.open
        sequences: ["Ctrl+Shift+N"]
        onActivated: win.beginCreateFolder()
    }
    Shortcut {
        enabled: !nameDialog.open
        sequences: ["Ctrl+D"]
        onActivated: Browser.duplicateSelection()
    }
    Shortcut {
        enabled: !nameDialog.open
        sequences: ["Ctrl+1"]
        onActivated: Browser.viewMode = "list"
    }
    Shortcut {
        enabled: !nameDialog.open
        sequences: ["Ctrl+2"]
        onActivated: Browser.viewMode = "grid"
    }
    Shortcut {
        enabled: !nameDialog.open && !win.navBusy
        sequences: ["Return", "Enter"]
        onActivated: {
            if (Pick.active) {
                if (Browser.selectionCount === 1 && Browser.selectionIsDir && Pick.mode !== "save")
                    win.enterFolder(Browser.selectedPaths[0])
                else if (Pick.canConfirm)
                    Pick.confirm()
                return
            }
            if (Browser.selectionCount === 1 && Browser.selectionIsDir)
                win.enterFolder(Browser.selectedPaths[0])
            else
                Browser.openSelection()
        }
    }
    Shortcut {
        enabled: Pick.active && !nameDialog.open
        sequences: ["Escape"]
        onActivated: Pick.cancel()
    }

    Component.onCompleted: {
        if (Pick.active && Pick.mode === "save")
            Qt.callLater(() => {
                saveNameField.forceActiveFocus()
                saveNameField.selectAll()
            })
    }

    // Dismiss address-bar edit when pressing outside it (don't steal the click).
    MouseArea {
        anchors.fill: parent
        z: 90
        enabled: pathBar.editing && !nameDialog.open
        visible: enabled
        propagateComposedEvents: true
        onPressed: (mouse) => {
            const p = mapToItem(pathBar, mouse.x, mouse.y)
            const inside = p.x >= 0 && p.y >= 0 && p.x <= pathBar.width && p.y <= pathBar.height
            if (!inside)
                pathBar.cancelEdit()
            mouse.accepted = false
        }
    }
}
