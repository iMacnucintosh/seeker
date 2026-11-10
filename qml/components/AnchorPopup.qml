import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property bool open: false
    property Item anchor: null
    property real popupWidth: 220
    // Children (MenuRow) must land inside the Popup, not on this root Item.
    default property alias content: body.data

    readonly property Item overlayParent: Overlay.overlay || parent

    function openAt() {
        const host = panel.parent
        if (!anchor || !host)
            return
        const p = anchor.mapToItem(host, anchor.width, anchor.height)
        panel.x = Math.min(Math.max(8, p.x - root.popupWidth), Math.max(8, host.width - root.popupWidth - 8))
        panel.y = Math.min(p.y + 6, Math.max(8, host.height - 220))
    }

    Popup {
        id: panel
        parent: root.overlayParent
        modal: true
        dim: true
        focus: true
        padding: 8
        width: root.popupWidth
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        transformOrigin: Item.TopRight

        Overlay.modal: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.28)
        }

        background: Rectangle {
            radius: 16
            color: Theme.popup
            border.width: 1
            border.color: Theme.outlineVariant
        }

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.72; to: 1; duration: 220; easing.type: Easing.OutBack }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120; easing.type: Easing.InCubic }
                NumberAnimation { property: "scale"; from: 1; to: 0.85; duration: 120; easing.type: Easing.InCubic }
            }
        }

        ColumnLayout {
            id: body
            width: root.popupWidth - 16
            spacing: 0
        }

        onClosed: root.open = false
    }

    onOpenChanged: {
        if (open) {
            // Parent may resolve only after window is ready.
            if (!panel.parent)
                panel.parent = Overlay.overlay || parent
            openAt()
            panel.open()
        } else {
            panel.close()
        }
    }

    component MenuRow: Item {
        id: row
        property string icon: ""
        property string glyph: ""
        property string label: ""
        property bool dangerous: false
        signal clicked()

        height: 44
        Layout.fillWidth: true

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            radius: 12
            color: ma.containsMouse ? (row.dangerous ? Qt.alpha(Theme.error, 0.16) : Theme.surfaceContainerHighest) : "transparent"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12
            clip: true

            MaterialIcon {
                Layout.preferredWidth: 22
                Layout.alignment: Qt.AlignVCenter
                icon: row.icon
                size: 20
                colour: row.dangerous ? Theme.error : Theme.onSurfaceVariant
            }
            StyledText {
                Layout.fillWidth: true
                text: row.label
                color: row.dangerous ? Theme.error : Theme.onSurface
                font.pixelSize: 14
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                row.clicked()
                root.open = false
            }
        }
    }
}
