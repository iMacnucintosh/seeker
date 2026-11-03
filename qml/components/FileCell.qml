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
    property bool canPreview: false
    property bool selected: false
    property string uriPayload: ""
    property bool dragging: false
    /// Set from Main via dropTargetPath (delegate DropAreas miss internal drags).
    property bool dropHover: false
    property real dropGrow: dropHover ? 1 : 0
    readonly property int iconPx: Browser.iconSize
    readonly property int mosaicPx: Math.max(14, Math.round(iconPx * 22 / 56))
    readonly property int badgePx: Math.max(12, Math.round(iconPx * 20 / 56))
    readonly property int thumbEdge: Math.max(96, Math.round(iconPx * 160 / 56))
    /// M3 shape.corner.large — matches Material image thumbnails.
    readonly property real previewRadius: Math.max(10, Math.round(iconPx * 16 / 56))
    readonly property bool showPreview: !root.isDir && (root.canPreview || root.isImage) && Browser.showImagePreviews
    readonly property var folderPreviewList: {
        const _ = Browser.folderPreviewsRevision
        if (!root.isDir || !Browser.showFolderPreviews)
            return []
        return Browser.folderPreviewPaths(root.path, 4)
    }
    readonly property bool hasFolderMosaic: folderPreviewList.length > 0
    // 1–2 tiles = one row; slight nudge into the folder pocket (not the bottom edge).
    readonly property int mosaicYOffset: {
        const n = folderPreviewList.length
        if (n <= 0)
            return 0
        if (n <= 2)
            return Math.max(4, Math.round(iconPx * 6 / 56))
        return Math.max(2, Math.round(iconPx * 3 / 56))
    }
    readonly property string placeSymbol: {
        const _ = Browser.folderIconsRevision
        return root.isDir ? Browser.folderSymbol(root.path) : ""
    }
    signal activated()
    signal renameRequested()
    signal contextMenuRequested(real gX, real gY)

    // Second click on an already-selected item schedules rename; cancelled by double-click.
    Timer {
        id: renameTimer
        interval: Qt.styleHints.mouseDoubleClickInterval
        onTriggered: root.renameRequested()
    }

    width: iconPx * 2
    height: iconPx * 2 + 6

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
        anchors.margins: 4
        radius: 16 + root.dropGrow * 14
        color: root.dropHover ? Qt.alpha(Theme.primary, 0.22)
             : root.selected ? Theme.secondaryContainer : "transparent"
        border.width: root.dropGrow > 0.05 ? 2 : 0
        border.color: Qt.alpha(Theme.primary, 0.55 * root.dropGrow)
    }

    // Fixed icon slot so 1- vs 2-line names don't shift icons vertically.
    Item {
        id: iconSlot
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Math.max(6, Math.round(iconPx * 10 / 56))
        width: root.iconPx
        height: root.iconPx
        opacity: root.dragging ? 0 : 1
        scale: 1 + root.dropGrow * 0.18
        Item {
            anchors.fill: parent
            // Keep the loader alive while we try; only paint plate+image on success
            // so a failed .blend thumb doesn't leave a grey shape behind the MIME icon.
            visible: root.showPreview

            Rectangle {
                anchors.fill: parent
                radius: root.previewRadius
                color: Theme.surfaceContainerHighest
                visible: previewImage.status === Image.Ready
            }

            Image {
                id: previewImage
                anchors.fill: parent
                // Disk-backed thumbs via image://thumb (see ThumbnailProvider).
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

            // Mask must be layered; Rectangle.clip ignores radius.
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
                const _ = Theme.primaryContainer // refresh when scheme changes
                return root.iconName ? Browser.iconUrl(root.iconName) : ""
            }
            sourceSize.width: root.iconPx * 2
            sourceSize.height: root.iconPx * 2
        }
        // Dolphin-style 2×2 image mosaic over the folder icon (no dimming of the face).
        Grid {
            id: folderMosaic
            visible: root.hasFolderMosaic
            anchors.centerIn: parent
            anchors.verticalCenterOffset: root.mosaicYOffset
            columns: 2
            rowSpacing: 2
            columnSpacing: 2
            Repeater {
                model: root.folderPreviewList
                delegate: Item {
                    required property string modelData
                    width: root.mosaicPx
                    height: root.mosaicPx

                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: Theme.surfaceContainerHighest
                        visible: tile.status === Image.Ready
                    }

                    Image {
                        id: tile
                        anchors.fill: parent
                        source: Browser.thumbUrl(modelData, Math.max(48, root.mosaicPx * 3))
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: true
                        sourceSize.width: Math.max(48, root.mosaicPx * 3)
                        sourceSize.height: Math.max(48, root.mosaicPx * 3)
                        visible: status === Image.Ready
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            maskEnabled: true
                            maskSource: tileMask
                            maskSpreadAtMin: 1
                            maskThresholdMin: 0.5
                        }
                    }

                    Item {
                        id: tileMask
                        anchors.fill: parent
                        layer.enabled: true
                        visible: false
                        Rectangle {
                            anchors.fill: parent
                            radius: 4
                            color: "#ffffff"
                        }
                    }
                }
            }
        }
        // Place watermark (same Material symbol as the sidebar) on the folder face.
        MaterialIcon {
            visible: root.placeSymbol.length > 0 && !root.showPreview && !root.hasFolderMosaic
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: Math.max(3, Math.round(root.iconPx * 5 / 56))
            icon: root.placeSymbol
            size: root.badgePx
            fill: 0
            colour: Theme.onPrimaryContainer
            opacity: 0.92
        }
    }
    FileNameLabel {
        anchors.top: iconSlot.bottom
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        height: 32
        fileName: root.name
        isDir: root.isDir
        lineBudget: 2
        sizePx: 12
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignTop
        scale: 1 + root.dropGrow * 0.04
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
                if (Browser.selectedPaths.indexOf(root.path) < 0)
                    Browser.selectedPaths = [root.path]
                root.uriPayload = Browser.uriList(Browser.selectedPaths)
                const grab = Math.max(80, root.iconPx * 2)
                iconSlot.grabToImage((result) => {
                    root.Drag.imageSource = result.url
                    root.Drag.hotSpot.x = result.width / 2
                    root.Drag.hotSpot.y = result.height / 2
                    root.dragging = true
                }, Qt.size(grab, grab))
            } else {
                root.dragging = false
                root.Drag.imageSource = ""
            }
        }
    }
}
