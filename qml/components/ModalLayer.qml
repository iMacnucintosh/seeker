import QtQuick

// The base of Seeker's dialogs: dims the window and eases the dialog in and out. The card is the dialog's own
// child; it uses `progress` (0 → 1) for its offset, scale and opacity. A click on the dimmed area emits
// closeRequested — only `armDelay` ms after opening when the double click that opened it could land there.
Item {
    id: root

    property bool open: false
    property real progress: open ? 1 : 0
    property real overshoot: 1.12
    property int armDelay: 0
    property bool _armed: armDelay === 0

    signal closeRequested()

    anchors.fill: parent
    visible: progress > 0.01

    Behavior on progress {
        NumberAnimation {
            duration: root.open ? 240 : 160
            easing.type: root.open ? Easing.OutBack : Easing.InCubic
            easing.overshoot: root.overshoot
        }
    }

    onOpenChanged: {
        armTimer.stop()
        if (root.armDelay > 0) {
            root._armed = false
            if (root.open)
                armTimer.start()
        }
    }

    Timer {
        id: armTimer
        interval: Math.max(1, root.armDelay)
        onTriggered: root._armed = true
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.progress * 0.45
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.open && root._armed
        onClicked: root.closeRequested()
    }
}
