// WaterfallCanvas.qml (FULL FILE)
// - Pull-based (ไม่ต้องพึ่ง NOTIFY)
// - New row goes to TOP, history shifts DOWN  ✅ (ไหลบนลงล่าง)
// - Supports padLeft/padRight to align with FFT plot
// - Uses colormap array of 0xRRGGBB ints
// ---------------------------------------------------------------

import QtQuick 2.15
import iScan.Display 1.0

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

    // DOA-VIEWER1.6: native GPU texture renderer path. When enabled, rows
    // are colorized into an event-driven ring texture and presented by Qt
    // SceneGraph/GPU, avoiding QML Canvas and QPainter waterfall paints.
    property bool nativeRenderEnabled: false
    // DOA-VIEWER1.7: use the existing runtime CUDA plugin for row peak-pooling
    // and colorization when available. Falls back to local CPU row processing.
    property bool cudaWaterfallEnabled: true

    // Sequence-gated live history. A new waterfall row is appended only when
    // the producer publishes a new display frame. This avoids fake scrolling
    // and saves Canvas work when the FFT frame has not changed.
    property int frameSequence: 0
    property string sourceKey: ""

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
    property bool showDebug: false

    // internal
    property int _frames: 0
    property int _lastLen: 0
    property int _sameCount: 0
    property int _lastFrameSequence: -1
    property string _lastSourceKey: ""
    property var _cssColors: []
    property bool _nativeSubmitQueued: false

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

    function _rebuildColorCache() {
        var out = []
        if (root.waterfallColors) {
            for (var i = 0; i < root.waterfallColors.length; ++i)
                out.push(root._rgbCssFromColorInt(root.waterfallColors[i]))
        }
        root._cssColors = out
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

    // ============ Native GPU waterfall texture surface ============
    FftWaterfallTextureItem {
        id: nativeWaterfallItem
        x: Math.max(0, root.padLeft)
        y: 0
        width: Math.max(1, root.width - Math.max(0, root.padLeft) - Math.max(0, root.padRight))
        height: root.height
        z: 4
        visible: root.enabled && root.nativeRenderEnabled
        renderEnabled: root.enabled && root.visible && root.nativeRenderEnabled
        targetFps: Math.max(1, root.wfFps)
        rowHeightPx: Math.max(1, root.rowHeightPx)
        minDb: root.minDb
        maxDb: root.maxDb
        palette: root.waterfallColors
        backgroundColor: root.color
        cudaProcessingEnabled: root.cudaWaterfallEnabled
    }

    function _submitNativeRow() {
        if (!root.nativeRenderEnabled || !root.enabled || !root.visible) return false
        if (!_isValidArray(root.waterfallRowDb)) return false
        root._lastLen = root.waterfallRowDb.length
        var ok = nativeWaterfallItem.submitExternalFrame(root.waterfallRowDb)
        if (ok) {
            root._frames++
            root._lastFrameSequence = root.frameSequence
        }
        return ok
    }

    function _requestNativeSubmit(reason) {
        if (!root.nativeRenderEnabled || !root.enabled || !root.visible) return
        if (root._nativeSubmitQueued) return
        root._nativeSubmitQueued = true
        // Let QML bindings settle first. The parent updates displayFftMagDb and
        // frameSequence in the same function; onFrameSequenceChanged can run
        // before waterfallRowDb has delivered the new array to this child. A
        // deferred submit keeps Spectrum and Waterfall on the same visible row.
        Qt.callLater(function() {
            root._nativeSubmitQueued = false
            root._submitNativeRow()
        })
    }

    onFrameSequenceChanged: {
        if (root.nativeRenderEnabled) {
            if (root.sourceKey !== root._lastSourceKey) {
                root._lastSourceKey = root.sourceKey
                root._resetHistory()
            }
            if (root.frameSequence !== root._lastFrameSequence)
                root._requestNativeSubmit("frame-sequence")
        }
    }

    onWaterfallRowDbChanged: {
        if (root.nativeRenderEnabled && root.frameSequence !== root._lastFrameSequence)
            root._requestNativeSubmit("row-db")
    }

    onNativeRenderEnabledChanged: {
        root._resetHistory()
        if (root.nativeRenderEnabled)
            root._requestNativeSubmit("native-enabled")
    }

    // ============ Pull-based update (NO NOTIFY needed) ============
    Timer {
        id: tick
        interval: Math.max(16, Math.floor(1000 / Math.max(1, root.wfFps)))
        running: root.enabled && root.visible && !root.nativeRenderEnabled
        repeat: true
        onTriggered: {
            if (root.sourceKey !== root._lastSourceKey) {
                root._lastSourceKey = root.sourceKey
                root._lastFrameSequence = -1
                root._sameCount = 0
                tick._lastKey = ""
                root._resetHistory()
            }

            var row = root.waterfallRowDb
            if (!_isValidArray(row)) {
                root._lastLen = 0
                return
            }

            // Preferred path: use explicit producer sequence. This means the
            // waterfall is a truthful acquisition/display history: one new
            // visible FFT frame produces at most one waterfall row.
            if (root.frameSequence > 0) {
                if (root.frameSequence === root._lastFrameSequence)
                    return
                root._lastFrameSequence = root.frameSequence
                root._sameCount = 0
                root._lastLen = row.length
                root._pushRow(row)
                return
            }

            // Compatibility fallback for callers that do not provide a sequence:
            // append only on actual content change; never keep scrolling the
            // same row forever.
            var len = row.length
            var a0  = Number(row[0])
            var aM  = Number(row[Math.floor(len/2)])
            var key = "" + len + "|" + a0.toFixed(2) + "|" + aM.toFixed(2)
            if (key === tick._lastKey) return
            tick._lastKey = key
            root._lastLen = len
            root._pushRow(row)
        }
        property string _lastKey: ""
    }

    // ============ Canvas 1: keep history image ============
    Canvas {
        id: scrollCanvas
        anchors.fill: parent
        visible: root.enabled && !root.nativeRenderEnabled
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
        visible: root.enabled && !root.nativeRenderEnabled
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
            var colors = root._cssColors
            if (!colors || colors.length !== root.waterfallColors.length)
                root._rebuildColorCache()
            colors = root._cssColors

            // Draw only inside plot area (left..xR). Group equal-color pixels
            // into runs so a row uses far fewer fillStyle/fillRect calls than
            // one rectangle per pixel. This matters on Jetson Qt Canvas.
            var runColor = ""
            var runStart = left
            var runLen = 0
            for (var px = 0; px < plotW; px++) {
                var bi = Math.floor(px * (n - 1) / Math.max(1, (plotW - 1)))
                var db = Number(row[bi])
                var ci = root._colorIndexFromDb(db)
                var css = (ci >= 0 && ci < colors.length) ? colors[ci] : root.color
                if (px === 0) {
                    runColor = css
                    runStart = left
                    runLen = 1
                } else if (css === runColor) {
                    runLen++
                } else {
                    ctx.fillStyle = runColor
                    ctx.fillRect(runStart, yTop, runLen, H)
                    runColor = css
                    runStart = left + px
                    runLen = 1
                }
            }
            if (runLen > 0) {
                ctx.fillStyle = runColor
                ctx.fillRect(runStart, yTop, runLen, H)
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

    function _resetHistory() {
        scrollCanvas._init = false
        root._frames = 0
        root._lastLen = 0
        root._lastFrameSequence = -1
        root._nativeSubmitQueued = false
        drawCanvas._pendingRow = null
        if (nativeWaterfallItem) nativeWaterfallItem.clearHistory()
        scrollCanvas.requestPaint()
        if (root.nativeRenderEnabled) root._requestNativeSubmit("reset")
    }

    // In native SceneGraph mode, geometry changes are handled by the C++ item.
    // Avoid clearing history from QML on every Layout width/height jitter; that
    // made the panel look permanently blank on StackView/ColumnLayout resize.
    onWidthChanged: {
        if (root.nativeRenderEnabled) root._requestNativeSubmit("width")
        else root._resetHistory()
    }
    onHeightChanged: {
        if (root.nativeRenderEnabled) root._requestNativeSubmit("height")
        else root._resetHistory()
    }
    onEnabledChanged: if (enabled) _resetHistory()
    onWaterfallColorsChanged: root._rebuildColorCache()

    Component.onCompleted: {
        root._rebuildColorCache()
        root._resetHistory()
    }
}
