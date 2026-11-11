import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Modal Material Symbols picker — search + grid, same badge style as place watermarks.
Item {
    id: root
    property bool open: false
    property string targetPath: ""
    property real progress: open ? 1 : 0
    property string query: ""
    property var symbols: []

    anchors.fill: parent
    visible: progress > 0.01
    z: 110

    Behavior on progress {
        NumberAnimation {
            duration: root.open ? 240 : 160
            easing.type: root.open ? Easing.OutBack : Easing.InCubic
            easing.overshoot: 1.12
        }
    }

    function begin(path) {
        targetPath = path
        query = ""
        searchField.text = ""
        refreshSymbols()
        open = true
        Qt.callLater(() => {
            searchField.forceActiveFocus()
            searchField.selectAll()
        })
    }

    function close() {
        open = false
        if (typeof win !== "undefined" && win.contentItem)
            win.contentItem.forceActiveFocus()
    }

    function refreshSymbols() {
        symbols = Browser.filterMaterialSymbols(query, 420)
    }

    function pick(symbol) {
        if (!targetPath.length || !symbol.length)
            return
        Browser.setFolderIcon(targetPath, symbol)
        close()
    }

    function clearCustom() {
        if (!targetPath.length)
            return
        Browser.clearFolderIcon(targetPath)
        close()
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.progress * 0.45
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.open
        onClicked: root.close()
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        anchors.verticalCenterOffset: (1 - root.progress) * 22
        width: Math.min(560, parent.width - 48)
        height: Math.min(520, parent.height - 48)
        radius: 24
        color: Theme.popup
        border.width: 1
        border.color: Theme.outlineVariant
        opacity: root.progress
        scale: 0.94 + root.progress * 0.06
        transformOrigin: Item.Center
        clip: true

        // Swallow clicks on the card.
        MouseArea {
            anchors.fill: parent
            z: -1
            onClicked: {}
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                StyledText {
                    Layout.fillWidth: true
                    text: qsTr("Folder icon")
                    color: Theme.onSurface
                    font.pixelSize: 18
                    font.weight: Font.Medium
                }
                IconButton {
                    icon: "close"
                    onClicked: root.close()
                }
            }

            StyledText {
                Layout.fillWidth: true
                text: root.targetPath.length
                    ? qsTr("Watermark for “%1”").arg(root.targetPath.split("/").pop())
                    : ""
                color: Theme.onSurfaceVariant
                font.pixelSize: 13
                elide: Text.ElideMiddle
            }

            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("Search Material icons…")
                color: Theme.onSurface
                placeholderTextColor: Theme.onSurfaceVariant
                selectedTextColor: Theme.onPrimary
                selectionColor: Theme.primary
                font.family: Theme.uiFamily
                font.pixelSize: 14
                background: Rectangle {
                    radius: 14
                    color: Theme.surfaceContainerHighest
                }
                leftPadding: 14
                rightPadding: 14
                topPadding: 10
                bottomPadding: 10
                onTextChanged: {
                    root.query = text
                    debounce.restart()
                }
                Keys.onEscapePressed: root.close()
            }

            Timer {
                id: debounce
                interval: 80
                onTriggered: root.refreshSymbols()
            }

            GridView {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                cellWidth: 72
                cellHeight: 78
                model: root.symbols
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    width: grid.cellWidth
                    height: grid.cellHeight

                    property string symbolName: modelData

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: 14
                        color: cellMa.containsMouse ? Theme.secondaryContainer : "transparent"
                        Behavior on color { ColorAnimation { duration: 80 } }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        MaterialIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            icon: symbolName
                            size: 28
                            fill: 0
                            colour: Theme.onSurface
                        }
                        Text {
                            width: 64
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: symbolName
                            color: Theme.onSurfaceVariant
                            font.pixelSize: 9
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    MouseArea {
                        id: cellMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.pick(symbolName)
                    }
                }

                StyledText {
                    anchors.centerIn: parent
                    visible: grid.count === 0
                    text: qsTr("No results")
                    color: Theme.onSurfaceVariant
                    font.pixelSize: 14
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                IconButton {
                    icon: "restart_alt"
                    enabled: {
                        const _ = Browser.folderIconsRevision
                        return root.targetPath.length > 0 && Browser.hasCustomFolderIcon(root.targetPath)
                    }
                    onClicked: root.clearCustom()
                }
                StyledText {
                    Layout.fillWidth: true
                    text: {
                        const _ = Browser.folderIconsRevision
                        return Browser.hasCustomFolderIcon(root.targetPath)
                            ? qsTr("Remove custom icon")
                            : qsTr("Click an icon to apply it")
                    }
                    color: Theme.onSurfaceVariant
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }
    }
}
