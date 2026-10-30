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
    title: Browser.displayPath + " — Archivos"
    color: Theme.surface

    header: Rectangle {
        height: 64
        color: Theme.surfaceContainer

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 4

            IconButton {
                icon: "arrow_back"
                enabled: Browser.canGoBack
                onClicked: Browser.goBack()
            }
            IconButton {
                icon: "arrow_forward"
                enabled: Browser.canGoForward
                onClicked: Browser.goForward()
            }
            IconButton {
                icon: "arrow_upward"
                onClicked: Browser.goUp()
            }
            IconButton {
                icon: "refresh"
                onClicked: Browser.refresh()
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                radius: 20
                color: Theme.surfaceContainerLowest

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    verticalAlignment: Text.AlignVCenter
                    text: Browser.displayPath
                    color: Theme.onSurface
                    font.pixelSize: 14
                    elide: Text.ElideMiddle
                }
            }

            IconButton {
                id: moreBtn
                icon: "more_vert"
                onClicked: actionsPopup.open = !actionsPopup.open
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            color: Theme.surfaceContainerLow

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 12
                anchors.bottomMargin: 12
                spacing: 2

                Text {
                    Layout.leftMargin: 24
                    Layout.bottomMargin: 8
                    text: "Lugares"
                    color: Theme.onSurfaceVariant
                    font.pixelSize: 12
                    font.bold: true
                }

                Repeater {
                    model: Browser.places
                    delegate: SidePlace {
                        required property var modelData
                        icon: modelData.icon
                        label: modelData.label
                        active: Browser.currentPath === modelData.path
                            || (modelData.id === "home" && Browser.currentPath.indexOf(modelData.path) === 0
                                && Browser.currentPath === modelData.path)
                        onClicked: Browser.navigate(modelData.path)
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                model: Browser.model
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds

                delegate: FileRow {
                    required property string name
                    required property string path
                    required property bool isDir
                    required property string iconName
                    selected: Browser.selectedPaths.indexOf(path) >= 0
                    onActivated: Browser.openPath(path)
                    onContextMenuRequested: {
                        Browser.selectedPaths = [path]
                        actionsPopup.anchor = moreBtn
                        actionsPopup.open = true
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    text: "Carpeta vacía"
                    color: Theme.onSurfaceVariant
                    font.pixelSize: 14
                }
            }
        }
    }

    // Invisible helper for future cursor-anchored menus
    Item { id: ctxAnchor; width: 1; height: 1 }

    AnchorPopup {
        id: actionsPopup
        anchor: moreBtn
        popupWidth: 240

        AnchorPopup.MenuRow {
            icon: "content_copy"
            label: "Copiar"
            onClicked: Browser.copySelection()
        }
        AnchorPopup.MenuRow {
            icon: "content_cut"
            label: "Cortar"
            onClicked: Browser.cutSelection()
        }
        AnchorPopup.MenuRow {
            icon: "content_paste"
            label: "Pegar"
            onClicked: Browser.paste()
        }
        AnchorPopup.MenuRow {
            icon: "select_all"
            label: "Seleccionar todo"
            onClicked: Browser.selectAll()
        }
        AnchorPopup.MenuRow {
            icon: "delete"
            label: "Mover a la papelera"
            dangerous: true
            onClicked: Browser.trashSelection()
        }
    }

    Shortcut {
        sequences: [StandardKey.Copy]
        onActivated: Browser.copySelection()
    }
    Shortcut {
        sequences: [StandardKey.Cut]
        onActivated: Browser.cutSelection()
    }
    Shortcut {
        sequences: [StandardKey.Paste]
        onActivated: Browser.paste()
    }
    Shortcut {
        sequences: [StandardKey.SelectAll]
        onActivated: Browser.selectAll()
    }
    Shortcut {
        sequences: [StandardKey.Delete, "Delete"]
        onActivated: Browser.trashSelection()
    }
    Shortcut {
        sequences: ["Backspace"]
        onActivated: Browser.goUp()
    }
}
