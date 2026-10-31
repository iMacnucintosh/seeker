import QtQuick

// File name with extension-preserving elision: "matriz_resp….xlsx"
Text {
    id: root

    property string fileName: ""
    property bool isDir: false
    /// Approx. line budget for wrap (grid = 2, list = 1).
    property int lineBudget: 1
    property int sizePx: 14

    readonly property int dot: {
        const n = root.fileName
        if (root.isDir || n.length < 3)
            return -1
        const i = n.lastIndexOf(".")
        // ".hidden" or no real suffix → keep whole name
        if (i <= 0 || i >= n.length - 1)
            return -1
        return i
    }
    readonly property string stem: dot > 0 ? fileName.substring(0, dot) : fileName
    readonly property string ext: dot > 0 ? fileName.substring(dot) : ""

    color: Theme.onSurface
    font.family: Theme.uiFamily
    font.pixelSize: sizePx
    font.weight: Font.Medium
    font.variableAxes: ({
        "opsz": sizePx,
        "wght": 500,
        "ROND": 25,
        "GRAD": 0,
        "wdth": 100
    })
    renderType: Text.NativeRendering
    textFormat: Text.PlainText
    elide: Text.ElideNone
    wrapMode: lineBudget > 1 ? Text.Wrap : Text.NoWrap
    maximumLineCount: Math.max(1, lineBudget)
    clip: true

    text: {
        const _w = width
        if (!fileName.length)
            return ""
        if (width < 8)
            return fileName

        if (!ext.length) {
            stemMetrics.text = fileName
            stemMetrics.elide = Text.ElideMiddle
            stemMetrics.elideWidth = Math.max(12, width * Math.max(1, lineBudget))
            return stemMetrics.elidedText || fileName
        }

        extMetrics.text = ext
        stemMetrics.text = stem
        stemMetrics.elide = Text.ElideMiddle
        const budget = Math.max(12, width * Math.max(1, lineBudget) - Math.ceil(extMetrics.advanceWidth) - 2)
        stemMetrics.elideWidth = budget
        const elided = stemMetrics.elidedText
        return (elided && elided.length ? elided : stem) + ext
    }

    TextMetrics {
        id: extMetrics
        font: root.font
    }
    TextMetrics {
        id: stemMetrics
        font: root.font
        elide: Text.ElideMiddle
    }
}
