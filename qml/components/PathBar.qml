import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Click to edit — browser-style address bar with FS/places suggestions (no visit history).
Rectangle {
    id: root
    property real barHeight: 40
    readonly property bool showRefresh: Browser.showRefreshButton
    readonly property int refreshSlot: showRefresh ? 36 : 0

    implicitHeight: barHeight
    height: barHeight
    radius: barHeight / 2
    color: editing ? Theme.surfaceContainerHighest : Theme.surfaceContainerLowest
    clip: true
    border.width: editing ? 1 : 0
    border.color: Theme.outlineVariant

    property bool editing: false
    property var suggestions: []
    property int highlight: -1
    property bool suppressBlurClose: false

    readonly property bool suggestOpen: editing && suggestions.length > 0
    /// -1 = atrás, +1 = adelante (rueda del ratón sobre la barra).
    signal historyStepRequested(int direction)

    HoverHandler {
        id: barHover
        enabled: !root.editing
    }
    WheelHandler {
        enabled: !root.editing && barHover.hovered
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            const dy = event.angleDelta.y
            const dx = event.angleDelta.x
            // Vertical: up → forward; horizontal: left → forward (trackpad).
            if (dy > 0 || dx < 0)
                root.historyStepRequested(1)
            else if (dy < 0 || dx > 0)
                root.historyStepRequested(-1)
            event.accepted = true
        }
    }

    function beginEdit() {
        editing = true
        field.text = Browser.currentPath
        highlight = -1
        Qt.callLater(() => {
            field.forceActiveFocus()
            field.selectAll()
            root.refreshSuggestions()
        })
    }

    function cancelEdit() {
        blurClose.stop()
        editing = false
        field.text = ""
        suggestions = []
        highlight = -1
    }

    function commitEdit() {
        const raw = field.text.trim()
        cancelEdit()
        if (!raw.length)
            return
        Browser.navigate(raw)
    }

    function refreshSuggestions() {
        if (!editing) {
            suggestions = []
            highlight = -1
            return
        }
        suggestions = Browser.pathSuggestions(field.text, 14)
        if (highlight >= suggestions.length)
            highlight = suggestions.length ? 0 : -1
    }

    function applySuggestion(index, navigateNow) {
        if (index < 0 || index >= suggestions.length)
            return
        const item = suggestions[index]
        const path = item.path || ""
        if (!path.length)
            return
        suppressBlurClose = true
        blurClose.stop()
        if (navigateNow) {
            cancelEdit()
            Browser.navigate(path)
            suppressBlurClose = false
            return
        }
        // Tab / complete: fill and keep editing for deeper paths.
        field.text = path.endsWith("/") ? path : (path + "/")
        field.cursorPosition = field.text.length
        highlight = -1
        refreshSuggestions()
        field.forceActiveFocus()
        Qt.callLater(() => { suppressBlurClose = false })
    }

    Behavior on color {
        ColorAnimation { duration: 120 }
    }

    Timer {
        id: blurClose
        interval: 140
        onTriggered: {
            if (root.suppressBlurClose)
                return
            if (root.editing && !field.activeFocus)
                root.cancelEdit()
        }
    }

    // Display mode — inherit StyledText Medium (same optical weight as bar panel).
    StyledText {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16 + root.refreshSlot
        verticalAlignment: Text.AlignVCenter
        text: Browser.displayPath
        color: Theme.onSurface
        elide: Text.ElideMiddle
        visible: !root.editing
    }

    MouseArea {
        anchors.fill: parent
        anchors.rightMargin: root.refreshSlot
        visible: !root.editing
        cursorShape: Qt.IBeamCursor
        hoverEnabled: true
        onClicked: root.beginEdit()

        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: Theme.onSurface
            opacity: parent.containsMouse ? 0.05 : 0
            Behavior on opacity { NumberAnimation { duration: 80 } }
        }
    }

    TextField {
        id: field
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12 + root.refreshSlot
        visible: root.editing
        color: Theme.onSurface
        selectedTextColor: Theme.onPrimary
        selectionColor: Theme.primary
        font.family: Theme.uiFamily
        font.pixelSize: 14
        font.weight: Font.Medium
        font.variableAxes: ({ "opsz": 14, "wght": 500, "ROND": 25, "GRAD": 0, "wdth": 100 })
        renderType: Text.NativeRendering
        background: Item {}
        leftPadding: 4
        rightPadding: 4
        verticalAlignment: TextInput.AlignVCenter
        inputMethodHints: Qt.ImhNoPredictiveText

        onTextChanged: {
            if (root.editing)
                root.refreshSuggestions()
        }

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Down) {
                if (root.suggestions.length) {
                    root.highlight = Math.min(root.suggestions.length - 1, Math.max(0, root.highlight) + 1)
                    event.accepted = true
                }
            } else if (event.key === Qt.Key_Up) {
                if (root.suggestions.length) {
                    root.highlight = Math.max(0, (root.highlight < 0 ? 0 : root.highlight) - 1)
                    event.accepted = true
                }
            } else if (event.key === Qt.Key_Tab) {
                const i = root.highlight >= 0 ? root.highlight : 0
                if (root.suggestions.length) {
                    root.applySuggestion(i, false)
                    event.accepted = true
                }
            }
        }

        Keys.onReturnPressed: (event) => {
            if (root.highlight >= 0 && root.highlight < root.suggestions.length)
                root.applySuggestion(root.highlight, true)
            else
                root.commitEdit()
            event.accepted = true
        }
        Keys.onEnterPressed: (event) => {
            if (root.highlight >= 0 && root.highlight < root.suggestions.length)
                root.applySuggestion(root.highlight, true)
            else
                root.commitEdit()
            event.accepted = true
        }
        Keys.onEscapePressed: root.cancelEdit()

        onActiveFocusChanged: {
            if (activeFocus) {
                blurClose.stop()
            } else if (root.editing && !root.suppressBlurClose) {
                blurClose.restart()
            }
        }
    }

    IconButton {
        visible: root.showRefresh
        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        z: 3
        size: 36
        iconSize: 20
        icon: "refresh"
        colour: Theme.onSurfaceVariant
        onClicked: Browser.refresh()
    }

    // Suggestions panel (Overlay — PathBar clips its children).
    MouseArea {
        id: suggestDismiss
        parent: Overlay.overlay
        anchors.fill: parent
        enabled: root.suggestOpen
        visible: root.suggestOpen
        z: 88
        onPressed: (mouse) => {
            const p = suggestHost.mapFromItem(suggestDismiss, mouse.x, mouse.y)
            if (p.x >= 0 && p.y >= 0 && p.x <= suggestHost.width && p.y <= suggestHost.height) {
                mouse.accepted = false
                return
            }
            root.cancelEdit()
        }
    }

    Item {
        id: suggestHost
        parent: Overlay.overlay
        z: 91
        width: Math.max(240, root.width)
        height: suggestCol.implicitHeight + 16
        visible: root.suggestOpen
        opacity: root.suggestOpen ? 1 : 0
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

        Rectangle {
            anchors.fill: parent
            radius: 16
            color: Theme.surfaceContainerHigh
        }

        Column {
            id: suggestCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            spacing: 2

            Repeater {
                model: root.suggestions

                delegate: Item {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool active: root.highlight === index

                    width: suggestCol.width
                    height: 40

                    Rectangle {
                        anchors.fill: parent
                        radius: 12
                        color: row.active ? Theme.secondaryContainer
                             : (rowMa.containsMouse ? Theme.surfaceContainerHighest : "transparent")
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        MaterialIcon {
                            Layout.alignment: Qt.AlignVCenter
                            icon: row.modelData.symbol || "folder"
                            size: 20
                            colour: row.active ? Theme.onSecondaryContainer : Theme.onSurfaceVariant
                            fill: row.active ? 1 : 0
                        }
                        Column {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 0
                            StyledText {
                                width: parent.width
                                text: row.modelData.label || ""
                                color: row.active ? Theme.onSecondaryContainer : Theme.onSurface
                                font.pixelSize: 14
                                elide: Text.ElideRight
                            }
                            StyledText {
                                width: parent.width
                                visible: (row.modelData.path || "") !== (row.modelData.label || "")
                                text: row.modelData.path || ""
                                color: row.active ? Theme.onSecondaryContainer : Theme.onSurfaceVariant
                                font.pixelSize: 11
                                opacity: 0.85
                                elide: Text.ElideMiddle
                            }
                        }
                    }

                    MouseArea {
                        id: rowMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: root.highlight = row.index
                        onPressed: root.suppressBlurClose = true
                        onClicked: root.applySuggestion(row.index, true)
                    }
                }
            }
        }
    }

    Connections {
        target: Browser
        function onPathChanged() {
            if (!root.editing)
                return
            root.cancelEdit()
        }
    }
}
