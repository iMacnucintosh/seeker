import QtQuick
import QtQuick.Layouts

// Bottom pill for background work (compress / extract): spinner + text + cancel while busy, then
// the result (check / error) for a few seconds.
Item {
    id: root
    property bool busy: false
    property string busyText: ""
    property string busyDetail: ""
    property string message: ""
    property bool isError: false
    property bool showing: false
    signal cancelRequested()

    readonly property bool active: busy || showing
    // (not readonly: a Behavior cannot animate a read-only property)
    property real shownProgress: active ? 1 : 0

    function show(text, error) {
        message = text
        isError = error
        showing = true
        hideTimer.interval = error ? 7000 : 4000
        hideTimer.restart()
    }

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 26
    width: Math.min(parent.width - 48, 520)
    height: 56
    z: 120
    visible: shownProgress > 0.01
    opacity: shownProgress
    transform: Translate { y: (1 - root.shownProgress) * 18 }
    Behavior on shownProgress { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    Timer {
        id: hideTimer
        onTriggered: root.showing = false
    }
    onBusyChanged: if (busy) { showing = false; hideTimer.stop() }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Theme.popupHighest
        border.width: 1
        border.color: root.isError && !root.busy ? Qt.alpha(Theme.error, 0.6) : Theme.outlineVariant
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 10
        spacing: 12

        MaterialIcon {
            id: stateIcon
            icon: root.busy ? "progress_activity" : (root.isError ? "error" : "check_circle")
            size: 24
            fill: root.busy ? 0 : 1
            colour: root.busy ? Theme.primary : (root.isError ? Theme.error : Theme.primary)
            RotationAnimator on rotation {
                running: root.busy
                loops: Animation.Infinite
                from: 0
                to: 360
                duration: 1100

                // Rotated text is drawn unsmoothed (pixelated): never leave the icon stopped at an angle.
                onRunningChanged: {
                    if (!running)
                        stateIcon.rotation = 0
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            StyledText {
                Layout.fillWidth: true
                text: root.busy ? root.busyText : root.message
                color: Theme.onSurface
                font.pixelSize: 14
                elide: Text.ElideRight
            }
            StyledText {
                Layout.fillWidth: true
                visible: root.busy && root.busyDetail.length > 0
                text: root.busyDetail
                color: Theme.onSurfaceVariant
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }
        }

        // Cancel (busy) / dismiss (result)
        Item {
            Layout.preferredWidth: cancelLabel.implicitWidth + 28
            Layout.preferredHeight: 36
            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: cancelArea.containsMouse ? Qt.alpha(Theme.primary, 0.14) : "transparent"
            }
            StyledText {
                id: cancelLabel
                anchors.centerIn: parent
                text: root.busy ? qsTr("Cancel") : qsTr("Close")
                color: Theme.primary
                font.pixelSize: 13
            }
            MouseArea {
                id: cancelArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.busy ? root.cancelRequested() : (root.showing = false)
            }
        }
    }
}
