import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property bool open: false
    property Item anchor: null
    property alias content: body.data
    property real popupWidth: 220

    // Morph from the anchor button: scale + fade from its center.
    function openAt() {
        if (!anchor)
            return
        const p = anchor.mapToItem(overlayParent, anchor.width / 2, anchor.height / 2)
        panel.x = Math.min(Math.max(8, p.x - panel.width + anchor.width / 2), overlayParent.width - panel.width - 8)
        panel.y = Math.min(p.y + anchor.height / 2 + 6, overlayParent.height - panel.height - 8)
        open = true
    }

    property Item overlayParent: Overlay.overlay

    Popup {
        id: panel
        parent: root.overlayParent
        modal: true
        dim: true
        focus: true
        padding: 0
        width: root.popupWidth
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Overlay.modal: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.28)
        }

        background: Rectangle {
            radius: 16
            color: Theme.surfaceContainerHigh
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

        transformOrigin: Item.TopRight

        ColumnLayout {
            id: body
            width: root.popupWidth
            spacing: 0
        }

        onClosed: root.open = false
    }

    onOpenChanged: {
        if (open) {
            openAt()
            panel.open()
        } else {
            panel.close()
        }
    }

    component MenuRow: Item {
        id: row
        property string icon: ""
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

            MaterialIcon {
                icon: row.icon
                size: 20
                color: row.dangerous ? Theme.error : Theme.onSurfaceVariant
            }
            Text {
                Layout.fillWidth: true
                text: row.label
                color: row.dangerous ? Theme.error : Theme.onSurface
                font.pixelSize: 14
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
