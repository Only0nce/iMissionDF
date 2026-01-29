import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    width: 640
    height: 220
    radius: 12
    color: "#060B16"
    border.color: "#1F2A44"
    border.width: 1
    clip: true

    // =========================
    // Public API
    // =========================
    property bool enabled: true
    property var  freqHz: []          // optional
    property var  magDb: []           // required
    property int  fps: 20

    // dB range (fixed)
    property real minDb: -150
    property real maxDb: -50

    // Optional band overlay
    property real bandCenterHz: 0
    property real bandBwHz: 0
    property bool showGrid: true

    // Plot padding
    property int padLeft: 64
    property int padRight: 12
    property int padTop: 10
    property int padBottom: 22

    // Waterfall resolution
    property int rowHeightPx: 1

    // Palette (0xRRGGBB)
    // (You can override from outside: colorLut: waterfallColors)
    property var colorLut: [
        0x30123b,0x311542,0x32184a,0x331b52,0x341e5a,0x352163,0x36246b,0x372773,
        0x382a7c,0x392d84,0x3a308c,0x3b3395,0x3c369d,0x3d39a5,0x3e3cae,0x3f3fb6,
        0x4042be,0x4145c7,0x4248cf,0x434bd7,0x444edf,0x4551e8,0x4654f0,0x4757f8,
        0x465bff,0x4060ff,0x3766ff,0x2d6cff,0x2472ff,0x1b78ff,0x137eff,0x0c84ff,
        0x078aff,0x0590ff,0x0896ff,0x109cff,0x19a2ff,0x23a8ff,0x2daeff,0x37b4ff,
        0x41baff,0x4bc0ff,0x55c6ff,0x5fccff,0x69d2ff,0x73d8ff,0x7ddfff,0x87e5ff,
        0x91ebff,0x9bf1ff,0xa5f7ff,0xaffdff,0xb7fffb,0xbfffef,0xc7ffe3,0xcfffd7,
        0xd7ffcb,0xdfffbe,0xe7ffb2,0xefffa6,0xf7ff9a,0xfffd8e,0xfff282,0xffe776,
        0xffdc6b,0xffd160,0xffc655,0xffbb4b,0xffb041,0xffa538,0xff9a30,0xff8f28,
        0xff8421,0xff791b,0xff6e15,0xff6310,0xff580c,0xff4d08,0xff4205,0xff3702
    ]

    // =========================
    // Internal
    // =========================
    property bool _dirtyGrid: true
    property int  _n: 0
    property real _fmin: 0
    property real _fmax: 1

    function _isValidArray(a) {
        return a !== undefined && a !== null && a.length !== undefined && a.length >= 8
    }

    function _plotW() { return Math.max(0, width  - padLeft - padRight) }
    function _plotH() { return Math.max(0, height - padTop  - padBottom) }

    function _calcXScale() {
        var f = root.freqHz
        var m = root.magDb
        if (!_isValidArray(m)) { root._n = 0; return }

        var n = m.length
        if (_isValidArray(f)) n = Math.min(f.length, m.length)
        root._n = n

        if (_isValidArray(f)) {
            var f0 = Number(f[0])
            var f1 = Number(f[n - 1])
            var lo = Math.min(f0, f1)
            var hi = Math.max(f0, f1)
            if (!isFinite(lo) || !isFinite(hi) || hi <= lo) {
                lo = 0
                hi = Math.max(1, n - 1)
            }
            root._fmin = lo
            root._fmax = hi
        } else {
            root._fmin = 0
            root._fmax = Math.max(1, n - 1)
        }
    }

    function _xFromFreq(hz) {
        var W = _plotW()
        return padLeft + ((hz - _fmin) / (_fmax - _fmin + 1e-12)) * W
    }

    function _clamp01(x) {
        if (x < 0) return 0
        if (x > 1) return 1
        return x
    }

    // Map dB -> LUT color (0xRRGGBB)
    function _colorOfDb(db) {
        var lo = Number(root.minDb)
        var hi = Number(root.maxDb)
        if (!isFinite(lo)) lo = -150
        if (!isFinite(hi)) hi = -50
        if (hi <= lo) hi = lo + 1

        var t = (Number(db) - lo) / (hi - lo)
        t = _clamp01(t)

        var lut = root.colorLut
        if (!lut || lut.length < 2) return 0x000000

        var idx = Math.floor(t * (lut.length - 1))
        if (idx < 0) idx = 0
        if (idx >= lut.length) idx = lut.length - 1
        return lut[idx] >>> 0
    }

    function _markGridDirty() { _dirtyGrid = true }

    onFreqHzChanged:  { _calcXScale(); _markGridDirty() }
    onMagDbChanged:   { _calcXScale() }             // (waterfall draws per-tick)
    onMinDbChanged:   { }                           // (waterfall draws per-tick)
    onMaxDbChanged:   { }                           // (waterfall draws per-tick)
    onBandCenterHzChanged: _markGridDirty()
    onBandBwHzChanged:     _markGridDirty()
    onWidthChanged:  { _markGridDirty() }
    onHeightChanged: { _markGridDirty() }

    // ✅ Timer: always scroll+draw when data is present (no reliance on onMagDbChanged)
    Timer {
        id: tick
        interval: Math.max(16, Math.floor(1000 / Math.max(1, root.fps)))
        running: root.visible
        repeat: true
        onTriggered: {
            // Keep n in sync even if backend mutates same array reference
            root._calcXScale()

            if (root._dirtyGrid) {
                gridCanvas.requestPaint()
                root._dirtyGrid = false
            }

            // draw 1 row continuously when enabled + has data
            if (root.enabled && root._n >= 8 && root.magDb && root.magDb.length >= 8) {
                wfCanvas.requestPaint()
            }
        }
    }

    // =========================
    // GRID / LABEL LAYER
    // =========================
    Canvas {
        id: gridCanvas
        z: 2
        anchors.fill: parent
        antialiasing: false
        visible: true
        renderTarget: Canvas.Image

        onPaint: {
            var ctx = getContext("2d")
            ctx.setTransform(1,0,0,1,0,0)
            ctx.clearRect(0,0,width,height)

            // background
            ctx.fillStyle = "#060B16"
            ctx.fillRect(0,0,width,height)

            var left = root.padLeft
            var top = root.padTop
            var W = root._plotW()
            var H = root._plotH()
            if (W < 10 || H < 10) return

            // plot border
            ctx.strokeStyle = "rgba(36,49,76,0.9)"
            ctx.lineWidth = 1
            ctx.strokeRect(left + 0.5, top + 0.5, W - 1, H - 1)

            // grid
            if (root.showGrid) {
                ctx.strokeStyle = "rgba(148,163,184,0.10)"
                ctx.lineWidth = 1
                for (var gx = 1; gx < 6; gx++) {
                    var x = left + (gx / 6.0) * W
                    ctx.beginPath()
                    ctx.moveTo(x, top)
                    ctx.lineTo(x, top + H)
                    ctx.stroke()
                }
            }

            // band overlay
            if (root.bandBwHz > 0 && root._n >= 8) {
                var b0 = root.bandCenterHz - root.bandBwHz * 0.5
                var b1 = root.bandCenterHz + root.bandBwHz * 0.5
                var inView = (b1 >= root._fmin && b0 <= root._fmax)
                if (inView) {
                    var xb0 = root._xFromFreq(Math.max(b0, root._fmin))
                    var xb1 = root._xFromFreq(Math.min(b1, root._fmax))
                    var xL = Math.min(xb0, xb1)
                    var xR = Math.max(xb0, xb1)
                    ctx.fillStyle = "rgba(34,197,94,0.07)"
                    ctx.fillRect(xL, top, xR - xL, H)

                    if (root.bandCenterHz >= root._fmin && root.bandCenterHz <= root._fmax) {
                        var xc = root._xFromFreq(root.bandCenterHz)
                        ctx.strokeStyle = "rgba(34,197,94,0.55)"
                        ctx.beginPath()
                        ctx.moveTo(xc, top)
                        ctx.lineTo(xc, top + H)
                        ctx.stroke()
                    }
                }
            }

            // labels
            ctx.fillStyle = "#AAB7D1"
            ctx.font = "12px Arial, Helvetica, sans-serif"
            ctx.fillText(root.maxDb.toFixed(0) + " dB", 10, top + 12)
            ctx.fillText(root.minDb.toFixed(0) + " dB", 10, top + H)
        }
    }

    // =========================
    // WATERFALL LAYER (FAST SCROLL)
    // =========================
    Canvas {
        id: wfCanvas
        z: 1
        anchors.fill: parent
        antialiasing: false
        visible: true
        opacity: root.enabled ? 1.0 : 0.25
        renderTarget: Canvas.Image

        // internal ImageData cache (W x H of plot area)
        property var _img: null
        property int _imgW: 0
        property int _imgH: 0

        function _ensureBuffer() {
            var W = Math.floor(root._plotW())
            var H = Math.floor(root._plotH())
            if (W < 10 || H < 10) return false

            if (_img === null || _imgW !== W || _imgH !== H) {
                _imgW = W
                _imgH = H
                var ctx = getContext("2d")
                _img = ctx.createImageData(W, H)

                // clear to black
                for (var i = 0; i < _img.data.length; i += 4) {
                    _img.data[i] = 0
                    _img.data[i+1] = 0
                    _img.data[i+2] = 0
                    _img.data[i+3] = 255
                }
            }
            return true
        }

        onPaint: {
            var ctx = getContext("2d")
            ctx.setTransform(1,0,0,1,0,0)
            ctx.clearRect(0,0,width,height)

            if (root._n < 8 || !_isValidArray(root.magDb)) return
            if (!_ensureBuffer()) return

            var left = root.padLeft
            var top = root.padTop
            var W = _imgW
            var H = _imgH

            // 1) scroll DOWN by dy rows
            var dy = Math.max(1, root.rowHeightPx)
            if (dy >= H) dy = 1

            var data = _img.data
            var rowBytes = W * 4

            // copy backward to avoid overwrite
            for (var y = H - 1; y >= dy; y--) {
                var dst = y * rowBytes
                var src = (y - dy) * rowBytes
                for (var b = 0; b < rowBytes; b++) data[dst + b] = data[src + b]
            }

            // 2) write new top rows
            var m = root.magDb
            var n = root._n

            for (var yy = 0; yy < dy; yy++) {
                var base = yy * rowBytes
                for (var x = 0; x < W; x++) {
                    var iBin = Math.floor((x / (W - 1 + 1e-12)) * (n - 1))
                    var rgb = root._colorOfDb(m[iBin])

                    data[base + x*4 + 0] = (rgb >> 16) & 255
                    data[base + x*4 + 1] = (rgb >>  8) & 255
                    data[base + x*4 + 2] = (rgb >>  0) & 255
                    data[base + x*4 + 3] = 255
                }
            }

            // 3) draw to plot area
            ctx.putImageData(_img, left, top)

            // thin line for newest row
            ctx.strokeStyle = "rgba(255,255,255,0.06)"
            ctx.beginPath()
            ctx.moveTo(left, top + 0.5)
            ctx.lineTo(left + W, top + 0.5)
            ctx.stroke()
        }
    }

    // =========================
    // DEBUG OVERLAY (always visible)
    // =========================
    Rectangle {
        z: 10
        visible: true
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        radius: 8
        color: "#0F172A"
        border.color: "#2B3856"
        border.width: 1
        opacity: 0.85
        height: 24
        width: debugText.paintedWidth + 16

        Text {
            id: debugText
            anchors.centerIn: parent
            color: "#E5E7EB"
            font.pixelSize: 12
            text: "wf enabled=" + root.enabled
                  + "  n=" + root._n
                  + "  magLen=" + ((root.magDb && root.magDb.length !== undefined) ? root.magDb.length : "NA")
                  + "  fps=" + root.fps
        }
    }

    Component.onCompleted: {
        _calcXScale()
        _dirtyGrid = true
        gridCanvas.requestPaint()
    }
}
