import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Asks for a name: renaming the selected item, or a new folder (created only on confirm).
ModalLayer {
    id: root

    // rename | create
    property string mode: "rename"

    function begin(mode, name) {
        root.mode = mode
        field.text = name
        open = true
        Qt.callLater(() => {
            field.forceActiveFocus()
            field.selectAll()
        })
    }

    function close() {
        open = false
        if (typeof win !== "undefined" && win.contentItem)
            win.contentItem.forceActiveFocus()
    }

    function confirm() {
        const name = field.text.trim()
        if (!name.length)
            return
        if (root.mode === "create")
            Browser.createFolder(name)
        else
            Browser.renameSelected(name)
        close()
    }

    overshoot: 1.15
    z: 100
    onCloseRequested: root.close()

    Rectangle {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: (1 - root.progress) * 18
        width: 360
        height: 160
        radius: 20
        color: Theme.popup
        border.width: 1
        border.color: Theme.outlineVariant
        opacity: root.progress
        scale: 0.92 + root.progress * 0.08
        transformOrigin: Item.Center

        MouseArea { anchors.fill: parent; z: -1; onClicked: {} }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            StyledText {
                text: root.mode === "create" ? qsTr("New folder") : qsTr("Rename")
                color: Theme.onSurface
                font.pixelSize: 16
                font.weight: Font.Medium
            }

            TextField {
                id: field
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
                Keys.onReturnPressed: root.confirm()
                Keys.onEnterPressed: root.confirm()
                Keys.onEscapePressed: root.close()
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                IconButton {
                    icon: "close"
                    onClicked: root.close()
                }
                Rectangle {
                    width: 40
                    height: 40
                    radius: 20
                    color: Theme.primary
                    opacity: field.text.trim().length ? 1 : 0.38
                    MaterialIcon {
                        anchors.centerIn: parent
                        icon: root.mode === "create" ? "add" : "check"
                        size: 22
                        colour: Theme.onPrimary
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: field.text.trim().length > 0
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.confirm()
                    }
                }
            }
        }
    }
}
