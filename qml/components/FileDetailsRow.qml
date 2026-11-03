import QtQuick
import QtQuick.Effects
import QtQuick.Layouts

// Compact details-view row: icon + name | size | modified.
Item {
    id: root
    property string name: ""
    property string path: ""
    property bool isDir: false
    property string iconName: "text-x-generic"
    property bool isImage: false
    property bool canPreview: false
    property string sizeText: ""
    property string modifiedText: ""
    property bool selected: false
    property string uriPayload: ""
    property bool dragging: false
    property bool dropHover: false
    property real dropGrow: dropHover ? 1 : 0
    readonly property int iconPx: Math.max(16, Math.round(Browser.iconSize * 20 / 56))
    readonly property int badgePx: Math.max(8, Math.round(iconPx * 10 / 20))
    readonly property int thumbEdge: Math.max(32, iconPx * 2)
    readonly property real previewRadius: Math.max(4, Math.round(iconPx * 6 / 20))
    readonly property bool showPreview: !root.isDir && (root.canPreview || root.isImage) && Browser.showImagePreviews
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

    height: Math.max(32, iconPx + 12)
    implicitHeight: height

    Behavior on dropGrow {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
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
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        radius: 8 + root.dropGrow * 6
        color: root.dropHover ? Qt.alpha(Theme.primary, 0.18)
             : root.selected ? Theme.secondaryContainer : "transparent"
        border.width: root.dropGrow > 0.05 ? 1 : 0
        border.color: Qt.alpha(Theme.primary, 0.5 * root.dropGrow)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 12
        spacing: 8

        Item {
            id: iconSlot
            Layout.preferredWidth: root.iconPx
            Layout.preferredHeight: root.iconPx
            Layout.alignment: Qt.AlignVCenter
            opacity: root.dragging ? 0 : 1

            Item {
                anchors.fill: parent
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
                anchors.centerIn: parent
                width: Math.max(14, root.iconPx - 2)
                height: Math.max(14, root.iconPx - 2)
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
                anchors.centerIn: parent
                anchors.verticalCenterOffset: 1
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
            sizePx: 13
        }

        StyledText {
            Layout.preferredWidth: 88
            Layout.alignment: Qt.AlignVCenter
            horizontalAlignment: Text.AlignRight
            text: root.isDir ? "—" : root.sizeText
            color: Theme.onSurfaceVariant
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        StyledText {
            Layout.preferredWidth: 132
            Layout.alignment: Qt.AlignVCenter
            horizontalAlignment: Text.AlignRight
            text: root.modifiedText
            color: Theme.onSurfaceVariant
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                renameTimer.stop()
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
                iconSlot.grabToImage((result) => {
                    root.Drag.imageSource = result.url
                    root.Drag.hotSpot.x = result.width / 2
                    root.Drag.hotSpot.y = result.height / 2
                    root.dragging = true
                }, Qt.size(40, 40))
            } else {
                root.dragging = false
                root.Drag.imageSource = ""
            }
        }
    }
}
