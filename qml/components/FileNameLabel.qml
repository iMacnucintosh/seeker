import QtQuick

// File name with extension-preserving elision: "Captura de pa…013054.png"
// Never middle-elide into the suffix — the painted string always ends with the real extension.
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

        // Prefer the full name when it wraps cleanly inside the line budget
        // (each wrapped line must fit `width`, or the right edge clips the extension).
        if (lineBudget > 1 && fitsWrapped(fileName, width, lineBudget))
            return fileName

        if (!ext.length) {
            stemMetrics.text = fileName
            stemMetrics.elide = Text.ElideMiddle
            // One cell-width — never width*lines (that overflows and clip eats the end).
            stemMetrics.elideWidth = Math.max(12, Math.floor(width))
            return stemMetrics.elidedText || fileName
        }

        extMetrics.text = ext
        const extW = Math.ceil(extMetrics.advanceWidth)
        // Keep the whole suffix; only the stem may shrink.
        let budget = Math.max(12, Math.floor(width) - extW - 2)
        if (lineBudget > 1) {
            // Two lines: first line can take a full width of stem, second holds the rest + ext.
            // Cap total stem so stem+ext still wraps without a line wider than `width`.
            const probe = elideStem(stem, Math.max(12, Math.floor(width) * lineBudget - extW - 2)) + ext
            if (fitsWrapped(probe, width, lineBudget))
                return probe
        }

        return elideStem(stem, budget) + ext
    }

    function elideStem(s, budget) {
        stemMetrics.text = s
        stemMetrics.elide = Text.ElideMiddle
        stemMetrics.elideWidth = Math.max(12, budget)
        const elided = stemMetrics.elidedText
        return (elided && elided.length) ? elided : s
    }

    function fitsWrapped(s, w, lines) {
        wrapProbe.width = Math.max(1, w)
        wrapProbe.text = s
        if (wrapProbe.lineCount > lines)
            return false
        // contentWidth is the longest line when wrap is on
        return wrapProbe.contentWidth <= w + 1
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

    // Off-tree probe for wrap/line measurements (not visible).
    Text {
        id: wrapProbe
        visible: false
        font: root.font
        wrapMode: Text.Wrap
        maximumLineCount: 8
        textFormat: Text.PlainText
    }
}
