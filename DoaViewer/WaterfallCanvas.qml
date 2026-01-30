// // WaterfallCanvas.qml (FULL FILE)
// // - Newest row at TOP (flows top->down)
// // - Aligns X scale with FftPlot by using same padLeft/padRight
// // - Scrolls ONLY the plot area (inside padding)
// // - Pull-based Timer (works even if waterfallRowDb has no NOTIFY)
// // - Colors: array of 0xRRGGBB (decimal ok)

// import QtQuick 2.15

// Rectangle {
//     id: root
//     radius: 12
//     color: "#060B16"
//     border.color: "#1F2A44"
//     border.width: 1
//     clip: true

//     // ===== Public API =====
//     property bool enabled: true

//     // 1 row = FFT magnitude dB array (e.g. doaClient.fftMagDb)
//     property var  waterfallRowDb: []

//     // dB scaling (match FFT)
//     property real minDb: -150.0
//     property real maxDb: -50.0

//     // fps + row height
//     property int  wfFps: 25
//     property int  rowHeightPx: 1

//     // colormap: array of decimal colors (0xRRGGBB)
//     property var  waterfallColors: []

//     // ===== IMPORTANT: align with FftPlot padding =====
//     property int padLeft: 64
//     property int padRight: 18
//     property int padTop: 0
//     property int padBottom: 0

//     // show border of plot area
//     property bool showFrame: true

//     // debug overlay
//     property bool showDebug: true

//     // ===== Internal =====
//     property int _frames: 0
//     property int _lastLen: 0
//     property int _sameCount: 0

//     function _isValidArray(a) {
//         return a !== undefined && a !== null && a.length !== undefined && a.length >= 8
//     }
//     function _clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

//     function _plotX0() { return Math.max(0, root.padLeft) }
//     function _plotY0() { return Math.max(0, root.padTop) }
//     function _plotW()  { return Math.max(0, root.width  - root.padLeft - root.padRight) }
//     function _plotH()  { return Math.max(0, root.height - root.padTop  - root.padBottom) }

//     function _colorIndexFromDb(db) {
//         var mn = Number(root.minDb)
//         var mx = Number(root.maxDb)
//         if (!isFinite(mn)) mn = -150
//         if (!isFinite(mx)) mx = -50
//         if (mx <= mn) mx = mn + 1.0

//         var t = (Number(db) - mn) / (mx - mn)
//         if (!isFinite(t)) t = 0
//         t = _clamp(t, 0, 1)

//         // ✅ boost brightness (gamma < 1 => brighter)
//         var gamma = 0.6
//         t = Math.pow(t, gamma)

//         var L = root.waterfallColors.length
//         if (L <= 1) return -1
//         return Math.floor(t * (L - 1) + 0.000001)
//     }


//     function _rgbCssFromColorInt(c) {
//         var r = (c >> 16) & 255
//         var g = (c >> 8) & 255
//         var b = (c) & 255
//         return "rgb(" + r + "," + g + "," + b + ")"
//     }

//     // ===== Canvas (persistent surface) =====
//     Canvas {
//         id: wfCanvas
//         anchors.fill: parent
//         visible: root.enabled
//         antialiasing: false
//         renderTarget: Canvas.FramebufferObject

//         property var _pendingRow: null
//         property bool _inited: false

//         onPaint: {
//             var ctx = getContext("2d")
//             ctx.setTransform(1,0,0,1,0,0)

//             var x0 = root._plotX0()
//             var y0 = root._plotY0()
//             var W  = root._plotW()
//             var Hh = root._plotH()

//             // init / reset
//             if (!wfCanvas._inited) {
//                 ctx.fillStyle = root.color
//                 ctx.fillRect(0, 0, width, height)

//                 // optional plot frame
//                 if (root.showFrame && W > 2 && Hh > 2) {
//                     ctx.strokeStyle = "rgba(36,49,76,0.9)"
//                     ctx.lineWidth = 1
//                     ctx.strokeRect(x0 + 0.5, y0 + 0.5, W - 1, Hh - 1)
//                 }

//                 wfCanvas._inited = true
//                 return
//             }

//             var row = wfCanvas._pendingRow
//             wfCanvas._pendingRow = null
//             if (!_isValidArray(row)) return
//             if (root.waterfallColors.length < 2) return
//             if (W < 4 || Hh < 4) return

//             var rh = Math.max(1, Math.floor(root.rowHeightPx))
//             if (rh > Hh) rh = Hh

//             // =========================================
//             // ✅ SHIFT ONLY plot-area DOWN by rh pixels
//             // =========================================
//             // src: (x0, y0) .. (x0+W, y0+Hh-rh)
//             // dst: (x0, y0+rh) .. (x0+W, y0+Hh)
//             ctx.drawImage(wfCanvas,
//                           x0, y0, W, Hh - rh,
//                           x0, y0 + rh, W, Hh - rh)

//             // clear TOP band inside plot-area for new row
//             ctx.fillStyle = root.color
//             ctx.fillRect(x0, y0, W, rh)

//             // =========================================
//             // ✅ DRAW NEW ROW AT TOP of plot-area
//             // =========================================
//             var y = y0
//             var n = row.length

//             for (var px = 0; px < W; px++) {
//                 var bi = Math.floor(px * (n - 1) / Math.max(1, (W - 1)))
//                 var db = Number(row[bi])
//                 var ci = root._colorIndexFromDb(db)
//                 if (ci < 0) continue
//                 var c = root.waterfallColors[ci]
//                 ctx.fillStyle = root._rgbCssFromColorInt(c)
//                 ctx.fillRect(x0 + px, y, 1, rh)
//             }

//             // keep plot frame visible (redraw border lightly)
//             if (root.showFrame) {
//                 ctx.strokeStyle = "rgba(36,49,76,0.9)"
//                 ctx.lineWidth = 1
//                 ctx.strokeRect(x0 + 0.5, y0 + 0.5, W - 1, Hh - 1)
//             }

//             root._frames++
//         }
//     }

//     function _pushRow(row) {
//         if (!root.enabled) return
//         if (!_isValidArray(row)) return
//         if (root.waterfallColors.length < 2) return
//         wfCanvas._pendingRow = row
//         wfCanvas.requestPaint()
//     }

//     // ===== Pull-based update =====
//     Timer {
//         id: tick
//         interval: Math.max(16, Math.floor(1000 / Math.max(1, root.wfFps)))
//         running: root.enabled && root.visible
//         repeat: true
//         onTriggered: {
//             var row = root.waterfallRowDb
//             if (!_isValidArray(row)) { root._lastLen = 0; return }

//             var len = row.length
//             var a0  = Number(row[0])
//             var aM  = Number(row[Math.floor(len/2)])
//             var key = "" + len + "|" + a0.toFixed(2) + "|" + aM.toFixed(2)

//             if (key === tick._lastKey) {
//                 root._sameCount++
//                 if (root._sameCount < 3) return
//             } else {
//                 root._sameCount = 0
//                 tick._lastKey = key
//             }

//             root._lastLen = len
//             root._pushRow(row)
//         }
//         property string _lastKey: ""
//     }

//     // ===== Debug overlay =====
//     Rectangle {
//         anchors.left: parent.left
//         anchors.top: parent.top
//         anchors.margins: 8
//         radius: 8
//         color: Qt.rgba(2/255, 6/255, 23/255, 0.65)
//         border.color: "#24314C"
//         border.width: 1
//         visible: root.showDebug
//         width: dbg.paintedWidth + 18
//         height: 26

//         Text {
//             id: dbg
//             anchors.centerIn: parent
//             color: "#E5E7EB"
//             font.pixelSize: 12
//             text: "WF plotW=" + root._plotW()
//                 + " len=" + root._lastLen
//                 + " frames=" + root._frames
//         }
//     }

//     // Disabled overlay
//     Item {
//         anchors.fill: parent
//         visible: !root.enabled
//         Rectangle { anchors.fill: parent; color: Qt.rgba(2/255, 6/255, 23/255, 0.55) }
//         Text {
//             anchors.centerIn: parent
//             text: "WATERFALL OFF"
//             color: "#F87171"
//             font.pixelSize: 16
//             font.bold: true
//         }
//     }

//     function _reset() {
//         wfCanvas._inited = false
//         wfCanvas.requestPaint()
//     }

//     onWidthChanged:  _reset()
//     onHeightChanged: _reset()
//     onEnabledChanged: if (enabled) _reset()

//     Component.onCompleted: _reset()
// }
// WaterfallCanvas.qml (FULL FILE)
// - Pull-based (ไม่ต้องพึ่ง NOTIFY)
// - New row goes to TOP, history shifts DOWN  ✅ (ไหลบนลงล่าง)
// - Supports padLeft/padRight to align with FFT plot
// - Uses colormap array of 0xRRGGBB ints
// ---------------------------------------------------------------

import QtQuick 2.15

Rectangle {
    id: root
    radius: 12
    color: "#060B16"
    border.color: "#1F2A44"
    border.width: 1
    clip: true

    // ===== Public API =====
    property bool enabled: true

    // 1 row = FFT magnitude dB array (e.g. doaClient.fftMagDb)
    property var  waterfallRowDb: []

    // shared with FFT
    property bool autoDb: false
    property real minDb: -150.0
    property real maxDb: -50.0

    // fps + row height
    property int  wfFps: 25
    property int  rowHeightPx: 1

    // align with FFT plot
    property int padLeft: 0
    property int padRight: 0

    // colormap
    property var  waterfallColors: []

    // debug
    property bool showDebug: true

    // internal
    property int _frames: 0
    property int _lastLen: 0
    property int _sameCount: 0

    function _isValidArray(a) {
        return a !== undefined && a !== null && a.length !== undefined && a.length >= 8
    }

    function _clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

    // map db -> color index
    function _colorIndexFromDb(db) {
        var mn = Number(root.minDb)
        var mx = Number(root.maxDb)
        if (!isFinite(mn)) mn = -150
        if (!isFinite(mx)) mx = -50
        if (mx <= mn) mx = mn + 1.0

        var t = (Number(db) - mn) / (mx - mn)
        if (!isFinite(t)) t = 0
        t = _clamp(t, 0, 1)

        var L = root.waterfallColors.length
        if (L <= 0) return -1
        return Math.floor(t * (L - 1) + 0.000001)
    }

    function _rgbCssFromColorInt(c) {
        var r = (c >> 16) & 255
        var g = (c >> 8) & 255
        var b = (c) & 255
        return "rgb(" + r + "," + g + "," + b + ")"
    }

    function _pushRow(row) {
        if (!root.enabled) return
        if (!_isValidArray(row)) return
        if (root.width < 4 || root.height < 4) return
        if (root.waterfallColors.length < 2) return

        var H = Math.max(1, Math.floor(root.rowHeightPx))
        if (H > root.height) H = root.height

        // 1) shift history DOWN by H (make room at TOP)
        scrollCanvas._rowH = H
        scrollCanvas.requestPaint()

        // 2) draw new row at TOP
        drawCanvas._pendingRow = row
        drawCanvas._rowH = H
        drawCanvas.requestPaint()

        root._frames++
    }

    // ============ Pull-based update (NO NOTIFY needed) ============
    Timer {
        id: tick
        interval: Math.max(16, Math.floor(1000 / Math.max(1, root.wfFps)))
        running: root.enabled && root.visible
        repeat: true
        onTriggered: {
            var row = root.waterfallRowDb
            if (!_isValidArray(row)) {
                root._lastLen = 0
                return
            }

            var len = row.length
            var a0  = Number(row[0])
            var aM  = Number(row[Math.floor(len/2)])
            var key = "" + len + "|" + a0.toFixed(2) + "|" + aM.toFixed(2)

            if (key === tick._lastKey) {
                root._sameCount++
                if (root._sameCount < 3) return
            } else {
                root._sameCount = 0
                tick._lastKey = key
            }

            root._lastLen = len
            root._pushRow(row)
        }
        property string _lastKey: ""
    }

    // ============ Canvas 1: keep history image ============
    Canvas {
        id: scrollCanvas
        anchors.fill: parent
        visible: root.enabled
        antialiasing: false
        renderTarget: Canvas.FramebufferObject

        property bool _init: false
        property int  _rowH: 1

        onPaint: {
            var ctx = getContext("2d")
            ctx.setTransform(1,0,0,1,0,0)

            if (scrollCanvas._init !== true) {
                ctx.fillStyle = root.color
                ctx.fillRect(0,0,width,height)
                scrollCanvas._init = true
                return
            }

            var H = Math.max(1, Math.floor(scrollCanvas._rowH))
            if (H > height) H = height

            // shift DOWN by H:
            // copy (0..height-H) -> (H..height)
            ctx.drawImage(scrollCanvas,
                          0, 0, width, height - H,
                          0, H, width, height - H)

            // clear TOP band for new row
            ctx.fillStyle = root.color
            ctx.fillRect(0, 0, width, H)
        }
    }

    // ============ Canvas 2: draw new row at TOP then composite into scrollCanvas ============
    Canvas {
        id: drawCanvas
        anchors.fill: parent
        visible: root.enabled
        antialiasing: false
        renderTarget: Canvas.FramebufferObject

        property var _pendingRow: null
        property int _rowH: 1

        onPaint: {
            var row = drawCanvas._pendingRow
            if (!_isValidArray(row)) return
            if (root.waterfallColors.length < 2) return

            var ctx = getContext("2d")
            ctx.setTransform(1,0,0,1,0,0)
            ctx.clearRect(0,0,width,height)

            var H = Math.max(1, drawCanvas._rowH)
            if (H > height) H = height

            var yTop = 0

            // plot x range aligned with FFT padding
            var left = Math.max(0, root.padLeft)
            var rightPad = Math.max(0, root.padRight)
            var xR = Math.max(left, Math.floor(width - rightPad))
            var plotW = Math.max(1, xR - left)

            // clear top band fully (including padding) to avoid leftover pixels
            ctx.fillStyle = root.color
            ctx.fillRect(0, yTop, width, H)

            var n = row.length

            // draw only inside plot area (left..xR)
            for (var px = 0; px < plotW; px++) {
                var x = left + px
                var bi = Math.floor(px * (n - 1) / Math.max(1, (plotW - 1)))
                var db = Number(row[bi])
                var ci = root._colorIndexFromDb(db)
                if (ci < 0) continue
                var c = root.waterfallColors[ci]
                ctx.fillStyle = root._rgbCssFromColorInt(c)
                ctx.fillRect(x, yTop, 1, H)
            }

            // composite into scrollCanvas
            var sctx = scrollCanvas.getContext("2d")
            sctx.drawImage(drawCanvas, 0, 0, width, height, 0, 0, width, height)
        }
    }

    // ============ Debug overlay ============
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        radius: 8
        color: Qt.rgba(2/255, 6/255, 23/255, 0.65)
        border.color: "#24314C"
        border.width: 1
        visible: root.showDebug
        width: dbg.paintedWidth + 18
        height: 26

        Text {
            id: dbg
            anchors.centerIn: parent
            color: "#E5E7EB"
            font.pixelSize: 12
            text: "WF len=" + root._lastLen + "  frames=" + root._frames + "  enabled=" + root.enabled
        }
    }

    // Disabled overlay
    Item {
        anchors.fill: parent
        visible: !root.enabled
        Rectangle { anchors.fill: parent; color: Qt.rgba(2/255, 6/255, 23/255, 0.55) }
        Text {
            anchors.centerIn: parent
            text: "WATERFALL OFF"
            color: "#F87171"
            font.pixelSize: 16
            font.bold: true
        }
    }

    Component.onCompleted: {
        scrollCanvas._init = false
        scrollCanvas.requestPaint()
    }
}
