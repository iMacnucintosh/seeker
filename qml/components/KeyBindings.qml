import QtQuick

// Seeker's keyboard shortcuts, as a table: keys, when they apply and what they do. `win` is the main window.
// Ctrl+Z and Delete are also caught in C++ (ShellKeyFilter): QML shortcuts are unreliable for them on Wayland.
Item {
    id: root

    required property var win
    // Moving the selection: nothing else (a dialog, a menu, the address bar) is taking the keys
    required property bool navEnabled
    // Editing: no name or icon dialog open
    required property bool editEnabled

    readonly property var bindings: [
        { keys: ["Up"], when: "nav", run: () => win.moveSelectionByKey(Qt.Key_Up) },
        { keys: ["Down"], when: "nav", run: () => win.moveSelectionByKey(Qt.Key_Down) },
        { keys: ["Left"], when: "nav", run: () => win.moveSelectionByKey(Qt.Key_Left) },
        { keys: ["Right"], when: "nav", run: () => win.moveSelectionByKey(Qt.Key_Right) },
        { keys: ["Home"], when: "nav", run: () => { Browser.selectIndex(0); win.ensureSelectionVisible() } },
        { keys: ["End"], when: "nav", run: () => { Browser.selectIndex(Math.max(0, Browser.model.count - 1)); win.ensureSelectionVisible() } },
        { keys: [StandardKey.Copy], when: "edit", run: () => Browser.copySelection() },
        { keys: [StandardKey.Cut], when: "edit", run: () => Browser.cutSelection() },
        { keys: [StandardKey.Paste], when: "edit", run: () => Browser.paste() },
        { keys: [StandardKey.SelectAll], when: "edit", run: () => Browser.selectAll() },
        // Backup of ShellKeyFilter, for the whole application; not while typing an address
        { keys: [StandardKey.Delete, "Delete"], when: "delete", app: true, run: () => { if (Browser.selectionCount > 0) Browser.trashSelection() } },
        { keys: ["Backspace"], when: "go", run: () => win.softNavigate(() => Browser.goUp()) },
        { keys: ["F2"], when: "name", run: () => win.beginRename() },
        { keys: ["Ctrl+Shift+N"], when: "name", run: () => win.beginCreateFolder() },
        { keys: ["Ctrl+D"], when: "name", run: () => Browser.duplicateSelection() },
        { keys: ["Ctrl+1"], when: "name", run: () => Browser.viewMode = "list" },
        { keys: ["Ctrl+2"], when: "name", run: () => Browser.viewMode = "grid" },
        { keys: ["Return", "Enter"], when: "go", run: () => root.openSelection() },
        { keys: ["Escape"], when: "pick", run: () => Pick.cancel() },
        // Quick Look, like Finder
        { keys: ["Space"], when: "look", run: () => win.toggleQuickLook() }
    ]

    function allowed(when) {
        const naming = !win.nameDialogOpen
        switch (when) {
        case "nav": return root.navEnabled
        case "edit": return root.editEnabled
        case "delete": return root.editEnabled && !win.addressEditing
        case "go": return naming && !win.navBusy
        case "pick": return Pick.active && naming && !QuickLook.open
        case "look": return root.navEnabled || QuickLook.open
        default: return naming
        }
    }

    // Enter: into a folder, or open the selection (in a file chooser: confirm it)
    function openSelection() {
        const intoFolder = Browser.selectionCount === 1 && Browser.selectionIsDir
        if (Pick.active) {
            if (intoFolder && Pick.mode !== "save")
                win.enterFolder(Browser.selectedPaths[0])
            else if (Pick.canConfirm)
                Pick.confirm()
            return
        }
        if (intoFolder)
            win.enterFolder(Browser.selectedPaths[0])
        else
            Browser.openSelection()
    }

    Repeater {
        model: root.bindings

        Item {
            required property var modelData

            Shortcut {
                enabled: root.allowed(modelData.when)
                sequences: modelData.keys
                context: modelData.app ? Qt.ApplicationShortcut : Qt.WindowShortcut
                onActivated: modelData.run()
            }
        }
    }
}
