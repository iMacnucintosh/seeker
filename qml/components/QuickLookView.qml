import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtMultimedia

// Quick Look (Space): the selected file and nothing else. Images as they are, video and audio with a minimal bar
// (play/pause and progress; the volume is the system's), PDF one page at a time, text as its first lines. It grows
// out of the file's icon (`origin`, in this item's coordinates) and goes back into it.
Item {
    id: root

    property rect origin: Qt.rect(width / 2 - 24, height / 2 - 24, 48, 48)
    property real progress: QuickLook.open ? 1 : 0
    readonly property string kind: QuickLook.kind
    readonly property real maxW: width - 120
    readonly property real maxH: height - 140

    anchors.fill: parent
    z: 110
    visible: progress > 0.001

    Behavior on progress {
        NumberAnimation {
            duration: QuickLook.open ? 260 : 170
            easing.type: QuickLook.open ? Easing.OutCubic : Easing.InCubic
        }
    }

    // file:// URL of a path, with #, ? and spaces escaped
    function fileUrl(path) {
        if (!path)
            return ""
        return "file://" + path.split("/").map(encodeURIComponent).join("/")
    }

    function formatTime(ms) {
        const s = Math.max(0, Math.floor(ms / 1000))
        const m = Math.floor(s / 60)
        const rest = s % 60
        return m + ":" + (rest < 10 ? "0" : "") + rest
    }

    function close() {
        QuickLook.close()
        if (typeof win !== "undefined" && win.contentItem)
            win.contentItem.forceActiveFocus()
    }

    // Media plays only while the preview shows it: moving on or closing stops it
    readonly property bool playsMedia: QuickLook.open && (kind === "video" || kind === "audio")

    // The video's own size, from its metadata (VideoOutput's own rect depends on the box: a binding loop)
    property size videoSize: Qt.size(0, 0)

    MediaPlayer {
        id: player
        onMetaDataChanged: {
            const r = metaData.value(MediaMetaData.Resolution)
            root.videoSize = r && r.width > 0 ? Qt.size(r.width, r.height) : Qt.size(0, 0)
        }
        source: root.playsMedia ? root.fileUrl(QuickLook.path) : ""
        audioOutput: AudioOutput {}
        videoOutput: videoOut
        onSourceChanged: {
            if (source.toString().length)
                play()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.progress * 0.62
    }

    MouseArea {
        anchors.fill: parent
        enabled: QuickLook.open
        onClicked: root.close()
    }

    Shortcut {
        enabled: QuickLook.open
        sequences: ["Escape"]
        onActivated: root.close()
    }
    Shortcut {
        enabled: QuickLook.open && root.kind === "pdf"
        sequences: ["PgDown"]
        onActivated: QuickLook.setPage(QuickLook.page + 1)
    }
    Shortcut {
        enabled: QuickLook.open && root.kind === "pdf"
        sequences: ["PgUp"]
        onActivated: QuickLook.setPage(QuickLook.page - 1)
    }

    // The box of the preview: the content's own size, fitted to the window, never enlarged
    Item {
        id: stage

        readonly property size natural: {
            switch (root.kind) {
            case "image":
                return img.status === Image.Ready ? Qt.size(img.implicitWidth, img.implicitHeight) : Qt.size(480, 320)
            case "video":
                return root.videoSize.width > 0 ? root.videoSize : Qt.size(960, 540)
            case "audio":
                return Qt.size(380, 360)
            case "pdf":
                return pageImg.status === Image.Ready ? Qt.size(pageImg.implicitWidth, pageImg.implicitHeight) : stage.lastPage
            case "text":
                return Qt.size(Math.min(900, root.maxW), Math.min(root.maxH, textBody.implicitHeight + 48))
            default:
                return Qt.size(300, 260)
            }
        }
        // While the next page renders the box keeps the size of the last one (A4 before the first)
        property size lastPage: Qt.size(1600, 2263)
        readonly property real fit: Math.min(1, root.maxW / Math.max(1, natural.width), root.maxH / Math.max(1, natural.height))
        readonly property bool isCard: root.kind === "audio" || root.kind === "text" || root.kind === "other"

        width: Math.round(natural.width * fit)
        height: Math.round(natural.height * fit)
        x: (root.width - width) / 2
        y: (root.height - height) / 2 - (pagePill.visible ? 18 : 0)
        opacity: Math.min(1, root.progress * 1.6)

        Behavior on width {
            enabled: QuickLook.open && root.progress === 1
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }
        Behavior on height {
            enabled: QuickLook.open && root.progress === 1
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }

        // From the file's icon to the centre
        transform: [
            Scale {
                origin.x: stage.width / 2
                origin.y: stage.height / 2
                readonly property real from: Math.max(0.05, Math.min(root.origin.width / Math.max(1, stage.width), root.origin.height / Math.max(1, stage.height)))
                xScale: from + (1 - from) * root.progress
                yScale: xScale
            },
            Translate {
                x: (root.origin.x + root.origin.width / 2 - root.width / 2) * (1 - root.progress)
                y: (root.origin.y + root.origin.height / 2 - root.height / 2) * (1 - root.progress)
            }
        ]

        // Clicks on the preview itself do not close it
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onPositionChanged: controls.poke()
            onWheel: (wheel) => {
                if (root.kind !== "pdf")
                    return
                QuickLook.setPage(QuickLook.page + (wheel.angleDelta.y < 0 ? 1 : -1))
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: 20
            visible: stage.isCard
            color: Theme.popup
            border.width: 1
            border.color: Theme.outlineVariant
        }

        // Rounded corners for pictures, video and pages
        Item {
            id: media
            anchors.fill: parent
            visible: !stage.isCard
            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: mask
                maskSpreadAtMin: 1
                maskThresholdMin: 0.5
            }

            AnimatedImage {
                id: img
                anchors.fill: parent
                visible: root.kind === "image"
                source: root.kind === "image" ? root.fileUrl(QuickLook.path) : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                autoTransform: true
                smooth: true
                mipmap: true
                cache: false
            }

            Rectangle {
                anchors.fill: parent
                visible: root.kind === "video"
                color: "#000000"
            }

            VideoOutput {
                id: videoOut
                anchors.fill: parent
                visible: root.kind === "video"
                fillMode: VideoOutput.PreserveAspectFit
            }

            Rectangle {
                anchors.fill: parent
                visible: root.kind === "pdf"
                color: "#ffffff"
            }

            Image {
                id: pageImg
                anchors.fill: parent
                visible: root.kind === "pdf"
                source: root.kind === "pdf" && QuickLook.pageImage ? root.fileUrl(QuickLook.pageImage) : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                mipmap: true
                cache: false
                onStatusChanged: {
                    if (status === Image.Ready)
                        stage.lastPage = Qt.size(implicitWidth, implicitHeight)
                }
            }
        }

        Item {
            id: mask
            anchors.fill: parent
            layer.enabled: true
            visible: false
            Rectangle {
                anchors.fill: parent
                radius: 20
                color: "#ffffff"
            }
        }

        // Audio: a music note, the name, and the bar
        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - 48
            visible: root.kind === "audio"
            spacing: 18

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 132
                Layout.preferredHeight: 132
                radius: 36
                color: Theme.primaryContainer

                MaterialIcon {
                    anchors.centerIn: parent
                    icon: "music_note"
                    size: 72
                    fill: 1
                    colour: Theme.onPrimaryContainer
                }
            }

            StyledText {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: QuickLook.name
                elide: Text.ElideMiddle
                color: Theme.onSurface
                font.pixelSize: 15
                font.weight: Font.Medium
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
            }
        }

        // Text: the first lines, scrollable
        Flickable {
            id: textView
            anchors.fill: parent
            anchors.margins: 24
            visible: root.kind === "text"
            clip: true
            contentWidth: width
            contentHeight: textBody.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            Text {
                id: textBody
                width: textView.width
                text: root.kind === "text" ? QuickLook.text : ""
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                color: Theme.onSurface
                font.family: "monospace"
                font.pixelSize: 13
                lineHeight: 1.15
            }
        }

        // Anything else (only when moving to it with the arrows): its icon and name
        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - 40
            visible: root.kind === "other"
            spacing: 14

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 112
                Layout.preferredHeight: 112
                source: root.kind === "other" && QuickLook.iconName ? Browser.iconUrl(QuickLook.iconName) : ""
                sourceSize: Qt.size(224, 224)
                fillMode: Image.PreserveAspectFit
            }

            StyledText {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: QuickLook.name
                elide: Text.ElideMiddle
                color: Theme.onSurface
                font.pixelSize: 15
                font.weight: Font.Medium
            }
        }

        // Video and audio bar: play/pause, progress and time. On video it hides while the pointer rests.
        Rectangle {
            id: controls

            property bool awake: true

            function poke() {
                awake = true
                sleepTimer.restart()
            }

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            height: 44
            radius: 22
            visible: root.kind === "video" || root.kind === "audio"
            color: root.kind === "video" ? Qt.rgba(0, 0, 0, 0.55) : "transparent"
            opacity: root.kind === "audio" || awake || player.playbackState !== MediaPlayer.PlayingState ? 1 : 0

            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }

            Timer {
                id: sleepTimer
                interval: 1800
                running: root.playsMedia
                onTriggered: controls.awake = false
            }

            readonly property color ink: root.kind === "video" ? "#ffffff" : Theme.onSurface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 14
                spacing: 10

                IconButton {
                    size: 36
                    iconSize: 24
                    icon: player.playbackState === MediaPlayer.PlayingState ? "pause" : "play_arrow"
                    colour: controls.ink
                    onClicked: {
                        controls.poke()
                        if (player.playbackState === MediaPlayer.PlayingState)
                            player.pause()
                        else
                            player.play()
                    }
                }

                Item {
                    id: track
                    Layout.fillWidth: true
                    Layout.preferredHeight: 20

                    readonly property real ratio: player.duration > 0 ? player.position / player.duration : 0

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: 4
                        radius: 2
                        color: Qt.alpha(controls.ink, 0.25)
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * track.ratio
                        height: 4
                        radius: 2
                        color: root.kind === "video" ? "#ffffff" : Theme.primary
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        function seek(x) {
                            if (player.duration > 0)
                                player.position = Math.max(0, Math.min(1, x / width)) * player.duration
                        }
                        onPressed: (mouse) => { controls.poke(); seek(mouse.x) }
                        onPositionChanged: (mouse) => { if (pressed) seek(mouse.x) }
                    }
                }

                StyledText {
                    text: root.formatTime(player.position) + " / " + root.formatTime(player.duration)
                    color: controls.ink
                    font.pixelSize: 12
                    font.features: { "tnum": 1 }
                }
            }
        }
    }

    // PDF: which page, and the way to the others
    Rectangle {
        id: pagePill
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: stage.bottom
        anchors.topMargin: 14
        visible: root.kind === "pdf" && QuickLook.pageCount > 1
        opacity: root.progress
        height: 36
        width: pageRow.implicitWidth + 12
        radius: 18
        color: Theme.popup

        MouseArea {
            anchors.fill: parent
        }

        RowLayout {
            id: pageRow
            anchors.centerIn: parent
            spacing: 4

            IconButton {
                size: 30
                iconSize: 20
                icon: "chevron_left"
                enabled: QuickLook.page > 1
                onClicked: QuickLook.setPage(QuickLook.page - 1)
            }
            StyledText {
                // TRANSLATORS: page %1 of %2
                text: qsTr("%1 of %2").arg(QuickLook.page).arg(QuickLook.pageCount)
                color: Theme.onSurface
                font.pixelSize: 13
                font.features: { "tnum": 1 }
            }
            IconButton {
                size: 30
                iconSize: 20
                icon: "chevron_right"
                enabled: QuickLook.page < QuickLook.pageCount
                onClicked: QuickLook.setPage(QuickLook.page + 1)
            }
        }
    }
}
