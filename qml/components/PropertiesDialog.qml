import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Item {
    id: root
    anchors.fill: parent
    visible: progress > 0.01
    z: 105

    readonly property bool open: Properties.open
    property real progress: open ? 1 : 0
    /// Custom / place Material badge for a single folder (same as FileCell).
    readonly property string folderBadge: {
        const _ = Browser.folderIconsRevision
        if (Properties.isMulti || !Properties.previewPath.length)
            return ""
        if (Properties.iconName !== "folder")
            return ""
        return Browser.folderSymbol(Properties.previewPath)
    }

    Behavior on progress {
        NumberAnimation {
            duration: root.open ? 240 : 160
            easing.type: root.open ? Easing.OutBack : Easing.InCubic
            easing.overshoot: 1.12
        }
    }

    function close() {
        Properties.close()
        if (typeof win !== "undefined" && win.contentItem)
            win.contentItem.forceActiveFocus()
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
        anchors.verticalCenterOffset: (1 - root.progress) * 18
        width: Math.min(420, parent.width - 48)
        implicitHeight: body.implicitHeight + 40
        height: implicitHeight
        radius: 20
        color: Theme.surfaceContainerHigh
        border.width: 1
        border.color: Theme.outlineVariant
        opacity: root.progress
        scale: 0.92 + root.progress * 0.08
        transformOrigin: Item.Center

        MouseArea {
            anchors.fill: parent
            onClicked: {}
        }

        ColumnLayout {
            id: body
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Item {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56

                    Rectangle {
                        anchors.fill: parent
                        radius: 16
                        color: Theme.surfaceContainerHighest
                        visible: !(Properties.isImage && !Properties.isMulti)
                    }

                    Image {
                        id: thumb
                        anchors.fill: parent
                        anchors.margins: 2
                        visible: Properties.isImage && !Properties.isMulti && Properties.previewPath.length
                        source: visible ? Browser.thumbUrl(Properties.previewPath, 112) : ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            maskEnabled: true
                            maskSource: thumbMask
                        }
                    }

                    Item {
                        id: thumbMask
                        anchors.fill: thumb
                        visible: false
                        layer.enabled: true
                        Rectangle {
                            anchors.fill: parent
                            radius: 14
                            color: "#ffffff"
                        }
                    }

                    Image {
                        id: typeIcon
                        anchors.centerIn: parent
                        width: 40
                        height: 40
                        visible: !thumb.visible
                        source: {
                            const _ = Theme.primaryContainer
                            return Browser.iconUrl(Properties.iconName)
                        }
                        sourceSize.width: 80
                        sourceSize.height: 80
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }

                    MaterialIcon {
                        visible: root.folderBadge.length > 0 && !thumb.visible
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: 4
                        icon: root.folderBadge
                        size: 18
                        fill: 0
                        colour: Theme.onPrimaryContainer
                        opacity: 0.92
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    StyledText {
                        Layout.fillWidth: true
                        text: Properties.title
                        color: Theme.onSurface
                        font.pixelSize: 16
                        font.weight: Font.Medium
                        elide: Text.ElideMiddle
                    }
                    StyledText {
                        Layout.fillWidth: true
                        text: Properties.kindLabel
                        color: Theme.onSurfaceVariant
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                }

                IconButton {
                    icon: "close"
                    onClicked: root.close()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                component PropRow: RowLayout {
                    property string label
                    property string value
                    Layout.fillWidth: true
                    spacing: 12

                    StyledText {
                        Layout.preferredWidth: 110
                        text: label
                        color: Theme.onSurfaceVariant
                        font.pixelSize: 13
                    }
                    StyledText {
                        Layout.fillWidth: true
                        text: value
                        color: Theme.onSurface
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        wrapMode: Text.WrapAnywhere
                    }
                }

                PropRow {
                    visible: Properties.showContents
                    label: "Contenido"
                    value: Properties.contentsLabel
                }
                PropRow {
                    label: "Tamaño"
                    value: Properties.sizeLabel
                }
                PropRow {
                    visible: Properties.showModified
                    label: "Modificado"
                    value: Properties.modified
                }
                PropRow {
                    label: "Ubicación"
                    value: Properties.location
                }
                PropRow {
                    visible: Properties.showPermissions
                    label: "Permisos"
                    value: Properties.permissions
                }
            }
        }

    }

    Shortcut {
        enabled: root.open
        sequences: ["Escape"]
        onActivated: root.close()
    }
}
