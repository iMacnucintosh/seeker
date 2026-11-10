import QtQuick

// Thin progress bar along the bottom edge of the window, for jobs that take a while (big
// compress / extract). Appears only after a short delay so quick jobs do not flash it.
// progress: 0..1, or negative = unknown (an indeterminate segment slides across).
Item {
    id: root
    property bool busy: false
    property real progress: -1
    property int showAfterMs: 700

    property bool armed: false          // the delay has passed while busy
    readonly property bool active: busy && armed

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    height: 5
    z: 119
    visible: opacity > 0.01
    opacity: active ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 220 } }

    onBusyChanged: {
        if (busy) {
            delay.restart()
        } else {
            delay.stop()
            armed = false
        }
    }
    Timer {
        id: delay
        interval: root.showAfterMs
        onTriggered: root.armed = root.busy
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.popupHighest
    }
    // Known size: fills from the left, smoothly.
    Rectangle {
        visible: root.progress >= 0
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * Math.max(0, Math.min(1, root.progress))
        radius: height / 2
        color: Theme.primary
        Behavior on width { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
    }
    // Unknown size: a segment sweeping back and forth.
    Rectangle {
        id: sweep
        visible: root.progress < 0
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * 0.25
        radius: height / 2
        color: Theme.primary
        SequentialAnimation on x {
            running: root.active && root.progress < 0
            loops: Animation.Infinite
            NumberAnimation { from: 0; to: root.width - sweep.width; duration: 900; easing.type: Easing.InOutCubic }
            NumberAnimation { from: root.width - sweep.width; to: 0; duration: 900; easing.type: Easing.InOutCubic }
        }
    }
}
