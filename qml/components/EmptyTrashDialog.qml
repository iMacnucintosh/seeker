import QtQuick
import QtQuick.Layouts

// Confirms emptying the trash for good.
ModalLayer {
    id: root

    function close() {
        open = false
        if (typeof win !== "undefined" && win.contentItem)
            win.contentItem.forceActiveFocus()
    }

    z: 105
    onCloseRequested: root.close()

    Rectangle {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: (1 - root.progress) * 16
        width: 360
        height: 188
        radius: 20
        color: Theme.popup
        border.width: 1
        border.color: Theme.outlineVariant
        opacity: root.progress
        scale: 0.94 + root.progress * 0.06

        MouseArea { anchors.fill: parent; z: -1; onClicked: {} }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10

            StyledText {
                text: qsTr("Empty the trash?")
                color: Theme.onSurface
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            StyledText {
                Layout.fillWidth: true
                text: Browser.trashStatsBusy && Browser.trashSize === 0
                      ? qsTr("All the items will be deleted permanently. This cannot be undone.")
                      : qsTr("%1 (%2) will be deleted permanently. This cannot be undone.")
                            .arg(Browser.trashCount === 1 ? qsTr("1 item") : qsTr("%1 items").arg(Browser.trashCount))
                            .arg(Browser.formatSize(Browser.trashSize))
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
                    onClicked: root.close()
                }
                Rectangle {
                    width: confirmLabel.implicitWidth + 28
                    height: 40
                    radius: 20
                    color: Theme.error
                    StyledText {
                        id: confirmLabel
                        anchors.centerIn: parent
                        text: qsTr("Empty trash")
                        color: Theme.onError
                        font.pixelSize: 14
                        font.weight: Font.Medium
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.close()
                            Browser.emptyTrash()
                        }
                    }
                }
            }
        }
    }
}
