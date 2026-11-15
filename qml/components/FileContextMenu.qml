import QtQuick

// The right-click menu of the files: on the background, on a selection, or on a pinned place of the sidebar
// (win.contextKind). `win` is the main window.
ContextMenu {
    id: root

    required property var win
    required property var iconPicker

    // —— the background and a selection ——
    ContextMenu.MenuRow {
        visible: root.win.contextKind !== "place"
        icon: "undo"
        label: qsTr("Undo")
        enabled: Browser.canUndo
        onClicked: Browser.undo()
    }

    // —— the background ——
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "empty"
        icon: "create_new_folder"
        label: qsTr("New folder")
        onClicked: root.win.beginCreateFolder()
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "empty"
        icon: "terminal"
        label: qsTr("Open terminal here")
        onClicked: Browser.openTerminalHere()
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "empty"
        icon: "content_paste"
        label: qsTr("Paste")
        enabled: Browser.hasClipboard
        onClicked: Browser.paste()
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "empty"
        icon: "select_all"
        label: qsTr("Select all")
        onClicked: Browser.selectAll()
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "empty"
        icon: "refresh"
        label: qsTr("Refresh")
        onClicked: Browser.refresh()
    }

    // —— a selection ——
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item"
        icon: "open_in_new"
        label: qsTr("Open")
        enabled: Browser.selectionCount === 1
        onClicked: {
            if (!Browser.selectedPaths.length)
                return
            if (Browser.selectionIsDir)
                root.win.enterFolder(Browser.selectedPaths[0])
            else
                Browser.openPath(Browser.selectedPaths[0])
        }
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item" && !Browser.selectionIsDir
        icon: "apps"
        label: qsTr("Open with")
        enabled: Browser.selectionCount === 1
        onClicked: {
            if (Browser.selectedPaths.length)
                OpenWith.openPath(Browser.selectedPaths[0])
        }
    }
    ContextMenu.Separator {
        visible: root.win.contextKind === "item"
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item"
        icon: "content_copy"
        label: qsTr("Copy")
        enabled: Browser.selectionCount > 0
        onClicked: Browser.copySelection()
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item"
        icon: "content_cut"
        label: qsTr("Cut")
        enabled: Browser.selectionCount > 0
        onClicked: Browser.cutSelection()
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item"
        icon: "content_paste"
        label: qsTr("Paste")
        enabled: Browser.hasClipboard
        onClicked: Browser.paste()
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item"
        icon: "control_point_duplicate"
        label: qsTr("Duplicate")
        enabled: Browser.selectionCount === 1
        onClicked: Browser.duplicateSelection()
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item" && Archive.available
        icon: "folder_zip"
        label: qsTr("Compress to ZIP")
        enabled: Browser.selectionCount > 0 && !Archive.busy
        onClicked: Archive.compress(Browser.selectedPaths)
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item" && Browser.selectionCount === 1
                 && Browser.selectedPaths.length > 0 && Archive.canExtract(Browser.selectedPaths[0])
        icon: "unarchive"
        label: qsTr("Extract here")
        enabled: !Archive.busy
        onClicked: Archive.extractHere(Browser.selectedPaths[0])
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item"
        icon: "drive_file_rename_outline"
        label: qsTr("Rename")
        enabled: Browser.selectionCount === 1
        onClicked: root.win.beginRename()
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item" && Browser.selectionIsDir
        icon: "category"
        label: qsTr("Change icon")
        enabled: Browser.selectionCount === 1
        onClicked: {
            if (Browser.selectedPaths.length)
                root.iconPicker.begin(Browser.selectedPaths[0])
        }
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item"
                 && Browser.selectionIsDir
                 && Browser.selectionCount === 1
                 && Browser.selectedPaths.length
                 && !Browser.isBuiltinPlace(Browser.selectedPaths[0])
        icon: Browser.selectedPaths.length && Browser.isPinned(Browser.selectedPaths[0])
              ? "bookmark_remove" : "bookmark_add"
        label: Browser.selectedPaths.length && Browser.isPinned(Browser.selectedPaths[0])
               ? qsTr("Remove from the sidebar") : qsTr("Add to the sidebar")
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
        visible: root.win.contextKind === "place"
        icon: "bookmark_remove"
        label: qsTr("Remove from the sidebar")
        enabled: root.win.placeContextPath.length > 0
        onClicked: {
            if (root.win.placeContextPath.length)
                Browser.unpinPlace(root.win.placeContextPath)
        }
    }
    ContextMenu.Separator {
        visible: root.win.contextKind === "item"
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item"
        icon: "info"
        label: qsTr("Details")
        enabled: Browser.selectionCount > 0
        onClicked: Properties.openPaths(Browser.selectedPaths)
    }
    ContextMenu.MenuRow {
        visible: root.win.contextKind === "item"
        icon: "delete"
        label: Browser.inTrash ? qsTr("Delete permanently") : qsTr("Move to the trash")
        dangerous: true
        enabled: Browser.selectionCount > 0
        onClicked: Browser.trashSelection()
    }
}
