import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent
    visible: progress > 0.01
    z: 106

    readonly property bool open: OpenWith.open
    property real progress: open ? 1 : 0
    /// Double-click that opens us would also hit the scrim and dismiss instantly.
    property bool scrimArmed: false

    Behavior on progress {
        NumberAnimation {
            duration: root.open ? 240 : 160
            easing.type: root.open ? Easing.OutBack : Easing.InCubic
            easing.overshoot: 1.12
        }
    }

    onOpenChanged: {
        scrimArmTimer.stop()
        if (open) {
            scrimArmed = false
            scrimArmTimer.start()
        } else {
            scrimArmed = false
        }
    }

    Timer {
        id: scrimArmTimer
        interval: 280
        onTriggered: root.scrimArmed = true
    }

    function close() {
        OpenWith.close()
        if (typeof win !== "undefined" && win.contentItem)
            win.contentItem.forceActiveFocus()
    }

    function launch() {
        if (!OpenWith.canLaunch)
            return
        OpenWith.launchSelected()
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
        enabled: root.open && root.scrimArmed
        onClicked: root.close()
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        anchors.verticalCenterOffset: (1 - root.progress) * 18
        width: Math.min(440, parent.width - 48)
        height: Math.min(520, parent.height - 48)
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
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    StyledText {
                        text: "Abrir con…"
                        color: Theme.onSurface
                        font.pixelSize: 16
                        font.weight: Font.Medium
                    }
                    StyledText {
                        Layout.fillWidth: true
                        text: OpenWith.fileName
                        color: Theme.onSurfaceVariant
                        font.pixelSize: 13
                        elide: Text.ElideMiddle
                    }
                }

                IconButton {
                    icon: "close"
                    onClicked: root.close()
                }
            }

            StyledText {
                Layout.fillWidth: true
                visible: OpenWith.mimeLabel.length > 0
                text: OpenWith.mimeLabel
                color: Theme.onSurfaceVariant
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 14
                color: Theme.surfaceContainerHighest
                clip: true

                ListView {
                    id: appList
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    model: OpenWith.apps
                    spacing: 2
                    currentIndex: OpenWith.selectedIndex
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        height: 48
                        radius: 12
                        color: index === OpenWith.selectedIndex
                               ? Theme.secondaryContainer
                               : (rowMa.containsMouse ? Qt.alpha(Theme.onSurface, 0.06) : "transparent")

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 12

                            Image {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                source: Browser.iconUrl(row.modelData.icon)
                                sourceSize.width: 56
                                sourceSize.height: 56
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                            }

                            StyledText {
                                Layout.fillWidth: true
                                text: row.modelData.name
                                color: index === OpenWith.selectedIndex
                                       ? Theme.onSecondaryContainer
                                       : Theme.onSurface
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            id: rowMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: OpenWith.select(row.index)
                            onDoubleClicked: {
                                OpenWith.select(row.index)
                                root.launch()
                            }
                        }
                    }

                    StyledText {
                        anchors.centerIn: parent
                        visible: appList.count === 0
                        text: "Ninguna aplicación compatible"
                        color: Theme.onSurfaceVariant
                        font.pixelSize: 13
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    radius: 6
                    color: OpenWith.remember ? Theme.primary : Theme.surfaceContainerHighest
                    border.width: OpenWith.remember ? 0 : 1
                    border.color: Theme.outlineVariant

                    MaterialIcon {
                        anchors.centerIn: parent
                        visible: OpenWith.remember
                        icon: "check"
                        size: 16
                        colour: Theme.onPrimary
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: OpenWith.remember = !OpenWith.remember
                    }
                }

                StyledText {
                    Layout.fillWidth: true
                    text: "Usar siempre para este tipo"
                    color: Theme.onSurfaceVariant
                    font.pixelSize: 13
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: OpenWith.remember = !OpenWith.remember
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight
                spacing: 8

                Rectangle {
                    Layout.preferredHeight: 40
                    Layout.preferredWidth: cancelLabel.implicitWidth + 28
                    radius: 20
                    color: cancelMa.containsMouse ? Qt.alpha(Theme.onSurface, 0.08) : "transparent"

                    StyledText {
                        id: cancelLabel
                        anchors.centerIn: parent
                        text: "Cancelar"
                        color: Theme.onSurfaceVariant
                        font.pixelSize: 13
                        font.weight: Font.Medium
                    }
                    MouseArea {
                        id: cancelMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }

                Rectangle {
                    Layout.preferredHeight: 40
                    Layout.preferredWidth: openLabel.implicitWidth + 32
                    radius: 20
                    color: Theme.primary
                    opacity: OpenWith.canLaunch ? 1 : 0.38

                    StyledText {
                        id: openLabel
                        anchors.centerIn: parent
                        text: "Abrir"
                        color: Theme.onPrimary
                        font.pixelSize: 13
                        font.weight: Font.Medium
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: OpenWith.canLaunch
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.launch()
                    }
                }
            }
        }
    }

    Shortcut {
        enabled: root.open
        sequences: ["Escape"]
        onActivated: root.close()
    }
    Shortcut {
        enabled: root.open && OpenWith.canLaunch
        sequences: ["Return", "Enter"]
        onActivated: root.launch()
    }
}
