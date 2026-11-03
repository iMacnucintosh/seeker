import QtQuick
import QtQuick.Effects
import QtQuick.Layouts

Item {
    id: root
    property string name: ""
    property string path: ""
    property bool isDir: false
    property string iconName: "text-x-generic"
    property bool isImage: false
    property bool selected: false
    property string uriPayload: ""
    property bool dragging: false
    property bool dropHover: false
    property real dropGrow: dropHover ? 1 : 0
    /// M3 shape.corner.medium — list thumbs are smaller.
    readonly property int iconPx: Math.max(20, Math.round(Browser.iconSize * 28 / 56))
    readonly property int badgePx: Math.max(10, Math.round(iconPx * 12 / 28))
    readonly property int thumbEdge: Math.max(48, iconPx * 2)
    readonly property real previewRadius: Math.max(8, Math.round(iconPx * 12 / 28))
    readonly property bool showPreview: !root.isDir && root.isImage && Browser.showImagePreviews
    readonly property string placeSymbol: {
        const _ = Browser.folderIconsRevision
        return root.isDir ? Browser.folderSymbol(root.path) : ""
    }
    signal activated()
    signal renameRequested()
    signal contextMenuRequested(real gX, real gY)

    Timer {
        id: renameTimer
        interval: Qt.styleHints.mouseDoubleClickInterval
        onTriggered: root.renameRequested()
    }

    height: Math.max(48, iconPx + 20)
    implicitHeight: height

    Behavior on dropGrow {
        NumberAnimation {
            duration: 260
            easing.type: Easing.OutBack
            easing.overshoot: 2.2
        }
    }

    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.MoveAction | Qt.CopyAction
    Drag.proposedAction: Qt.MoveAction
    Drag.keys: ["text/uri-list"]
    Drag.mimeData: { "text/uri-list": root.uriPayload }
    Drag.active: root.dragging

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        radius: 12 + root.dropGrow * 12
        color: root.dropHover ? Qt.alpha(Theme.primary, 0.22)
             : root.selected ? Theme.secondaryContainer : "transparent"
        border.width: root.dropGrow > 0.05 ? 2 : 0
        border.color: Qt.alpha(Theme.primary, 0.55 * root.dropGrow)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12
        Item {
            id: iconSlot
            Layout.preferredWidth: root.iconPx
            Layout.preferredHeight: root.iconPx
            Layout.alignment: Qt.AlignVCenter
            opacity: root.dragging ? 0 : 1
            scale: 1 + root.dropGrow * 0.2
            Item {
                anchors.fill: parent
                visible: root.showPreview

                Rectangle {
                    anchors.fill: parent
                    radius: root.previewRadius
                    color: Theme.surfaceContainerHighest
                }

                Image {
                    id: previewImage
                    anchors.fill: parent
                    source: root.showPreview ? Browser.thumbUrl(root.path, root.thumbEdge) : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: true
                    sourceSize.width: root.thumbEdge
                    sourceSize.height: root.thumbEdge
                    visible: status === Image.Ready
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskSource: previewMask
                        maskSpreadAtMin: 1
                        maskThresholdMin: 0.5
                    }
                }

                Item {
                    id: previewMask
                    anchors.fill: parent
                    layer.enabled: true
                    visible: false
                    Rectangle {
                        anchors.fill: parent
                        radius: root.previewRadius
                        color: "#ffffff"
                    }
                }
            }
            Image {
                id: iconImage
                anchors.centerIn: parent
                width: root.iconPx
                height: root.iconPx
                visible: !root.showPreview || previewImage.status !== Image.Ready
                source: {
                    const _ = Theme.primaryContainer
                    return root.iconName ? Browser.iconUrl(root.iconName) : ""
                }
                sourceSize.width: root.iconPx * 2
                sourceSize.height: root.iconPx * 2
            }
            MaterialIcon {
                visible: root.placeSymbol.length > 0 && !root.showPreview
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 2
                icon: root.placeSymbol
                size: root.badgePx
                fill: 0
                colour: Theme.onPrimaryContainer
                opacity: 0.92
            }
        }
        FileNameLabel {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            fileName: root.name
            isDir: root.isDir
            lineBudget: 1
            sizePx: 14
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                renameTimer.stop()
                // Keep multi-selection when right-clicking an already-selected item.
                if (Browser.selectedPaths.indexOf(root.path) < 0)
                    Browser.selectedPaths = [root.path]
                const g = mapToGlobal(mouse.x, mouse.y)
                root.contextMenuRequested(g.x, g.y)
                return
            }
            if (mouse.modifiers & Qt.ControlModifier) {
                renameTimer.stop()
                const paths = Browser.selectedPaths.slice()
                const idx = paths.indexOf(root.path)
                if (idx >= 0)
                    paths.splice(idx, 1)
                else
                    paths.push(root.path)
                Browser.selectedPaths = paths
                return
            }
            const already = Browser.selectedPaths.length === 1
                && Browser.selectedPaths[0] === root.path
            if (already) {
                renameTimer.restart()
            } else {
                renameTimer.stop()
                Browser.selectedPaths = [root.path]
            }
        }
        onDoubleClicked: {
            renameTimer.stop()
            root.activated()
        }
    }

    DragHandler {
        acceptedButtons: Qt.LeftButton
        target: null
        onActiveChanged: {
            if (active) {
                renameTimer.stop()
                root.uriPayload = Browser.uriList(Browser.selectedPaths.indexOf(root.path) >= 0
                    ? Browser.selectedPaths : [root.path])
                if (Browser.selectedPaths.indexOf(root.path) < 0)
                    Browser.selectedPaths = [root.path]
                iconSlot.grabToImage((result) => {
                    root.Drag.imageSource = result.url
                    root.Drag.hotSpot.x = result.width / 2
                    root.Drag.hotSpot.y = result.height / 2
                    root.dragging = true
                }, Qt.size(56, 56))
            } else {
                root.dragging = false
                root.Drag.imageSource = ""
            }
        }
    }
}
