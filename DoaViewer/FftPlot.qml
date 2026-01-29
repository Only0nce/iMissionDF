import QtQuick 2.15
import QtQuick.Controls 2.15
// ============================================================
// FftPlot.qml (final)
// - Smooth Canvas FFT rendering
//   * FPS throttling (Timer)
//   * Downsample bins to pixel width
//   * Cached grid/background in separate Canvas
//   * antialiasing disabled (CPU saver)
//   * Avoid ctx.reset() per frame
//
// Usage:
//   FftPlot {
//     enabled: true
//     freqHz: yourFreqArray
//     magDb: yourMagArray
//     bandCenterHz: ...
//     bandBwHz: ...
//     yAuto: false
//     yMinDb: -150
//     yMaxDb: -50
//     fftFps: 25
//   }
// ============================================================

Rectangle {
    id: root
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
    property var  magDb: []           // FFT magnitude in dB
    property real bandCenterHz: 0
    property real bandBwHz: 0

    // =========================
    // Render control
    // =========================
    property int  fftFps: 12          // ✅ ลด default ให้ลื่น
    property bool showGrid: true

    // =========================
    // Y-axis control
    // =========================
    property bool yAuto: false
    property real yMinDb: -150.0
    property real yMaxDb: -50.0

    // =========================
    // Internal scale cache
    // =========================
    property real _fmin: 0
    property real _fmax: 0
    property real _mmin: -150
    property real _mmax: -50
    property int  _n: 0

    // =========================
    // Dirty flags
    // =========================
    property bool _dirtyPlot: true
    property bool _dirtyGrid: true
    property bool _dirtyScale: true

    // =========================
    // Plot padding
    // =========================
    property int padLeft: 64
    property int padRight: 18
    property int padTop: 16
    property int padBottom: 40

    // =========================
    // Style
    // =========================
    property color cBg0: "#060B16"
    property color cBg1: "#070F22"
    property color cGrid: "#94A3B8"
    property color cText: "#AAB7D1"
    property color cLineA: "#38BDF8"
    property color cLineB: "#A78BFA"

    // Helpers
    function _isValidArray(a) { return a !== undefined && a !== null && a.length !== undefined && a.length >= 8 }
    function _plotW() { return Math.max(0, width  - padLeft - padRight) }
    function _plotH() { return Math.max(0, height - padTop  - padBottom) }

    function _xFromFreq(hz) {
        var W = _plotW()
        return padLeft + ((hz - _fmin) / (_fmax - _fmin + 1e-12)) * W
    }
    function _yFromDb(db) {
        var H = _plotH()
        return padTop + (1.0 - ((db - _mmin) / (_mmax - _mmin + 1e-12))) * H
    }

    // Optimized scale calc (once/frame)
    function _calcScale() {
        var f = root.freqHz
        var m = root.magDb
        if (!_isValidArray(m)) { root._n = 0; return }

        var n = m.length
        if (_isValidArray(f)) n = Math.min(f.length, m.length)
        root._n = n

        // F range
        var fmin = 0, fmax = 1
        if (_isValidArray(f)) {
            // fast endpoint
            var f0 = Number(f[0])
            var f1 = Number(f[n - 1])
            fmin = Math.min(f0, f1)
            fmax = Math.max(f0, f1)
            if (!isFinite(fmin) || !isFinite(fmax) || fmax <= fmin) {
                // light scan
                fmin = Number(f[0]); fmax = Number(f[0])
                var fstep = Math.max(1, Math.floor(n / 512))
                for (var fi = fstep; fi < n; fi += fstep) {
                    var fv = Number(f[fi])
                    if (fv < fmin) fmin = fv
                    if (fv > fmax) fmax = fv
                }
                if (fmax <= fmin) fmax = fmin + 1.0
            }
        } else {
            fmin = 0
            fmax = Math.max(1, n - 1)
        }

        // M range
        var mmin, mmax
        if (!root.yAuto) {
            mmin = Number(root.yMinDb)
            mmax = Number(root.yMaxDb)
            if (!isFinite(mmin)) mmin = -150.0
            if (!isFinite(mmax)) mmax = -50.0
            if (mmax <= mmin) mmax = mmin + 1.0
        } else {
            var mmn = Number(m[0])
            var mmx = Number(m[0])
            var step = Math.max(1, Math.floor(n / 512))
            for (var i = step; i < n; i += step) {
                var mv = Number(m[i])
                if (mv < mmn) mmn = mv
                if (mv > mmx) mmx = mv
            }
            if (!isFinite(mmn) || !isFinite(mmx)) { mmn = -150.0; mmx = -50.0 }
            if (mmx <= mmn) mmx = mmn + 1.0
            mmin = mmn; mmax = mmx
        }

        root._fmin = fmin
        root._fmax = fmax
        root._mmin = mmin
        root._mmax = mmax
    }

    function _markPlotDirty() { _dirtyPlot = true; _dirtyScale = true }
    function _markGridDirty() { _dirtyGrid = true; _dirtyScale = true }

    onFreqHzChanged:  { _markGridDirty(); _markPlotDirty() }
    onMagDbChanged:   { _markPlotDirty(); if (root.yAuto) _markGridDirty() }
    onBandCenterHzChanged: _markGridDirty()
    onBandBwHzChanged:     _markGridDirty()
    onYAutoChanged:        { _markGridDirty(); _markPlotDirty() }
    onYMinDbChanged:       { if (!root.yAuto) { _markGridDirty(); _markPlotDirty() } }
    onYMaxDbChanged:       { if (!root.yAuto) { _markGridDirty(); _markPlotDirty() } }
    onWidthChanged:        { _markGridDirty(); _markPlotDirty() }
    onHeightChanged:       { _markGridDirty(); _markPlotDirty() }

    // =========================
    // FPS throttler
    // =========================
    Timer {
        id: paintTimer
        interval: Math.max(16, Math.floor(1000 / Math.max(1, root.fftFps)))
        running: root.enabled && root.visible
        repeat: true
        onTriggered: {
            if (!(root._dirtyScale || root._dirtyGrid || root._dirtyPlot)) return

            if (root._dirtyScale) {
                root._calcScale()
                root._dirtyScale = false
            }
            if (root._dirtyGrid) { gridCanvas.requestPaint(); root._dirtyGrid = false }
            if (root._dirtyPlot) { plotCanvas.requestPaint(); root._dirtyPlot = false }
        }
    }

    // =====================
    // Grid / Background (FAST)
    // =====================
    Canvas {
        id: gridCanvas
        anchors.fill: parent
        visible: root.enabled
        antialiasing: false
        renderTarget: Canvas.FramebufferObject

        onPaint: {
            var ctx = getContext("2d")
            ctx.setTransform(1,0,0,1,0,0)
            ctx.clearRect(0,0,width,height)

            // background linear only (fast)
            var g = ctx.createLinearGradient(0, 0, 0, height)
            g.addColorStop(0.0, root.cBg1)
            g.addColorStop(1.0, root.cBg0)
            ctx.fillStyle = g
            ctx.fillRect(0,0,width,height)

            if (root._n < 8 || !_isValidArray(root.magDb)) {
                ctx.fillStyle = "#93A4C7"
                ctx.font = "14px Arial, Helvetica, sans-serif"
                ctx.fillText("No FFT data", 14, 24)
                return
            }

            var left = root.padLeft
            var top = root.padTop
            var W = root._plotW()
            var H = root._plotH()
            if (W < 10 || H < 10) return

            // plot border
            ctx.strokeStyle = "rgba(36,49,76,0.9)"
            ctx.lineWidth = 1
            ctx.strokeRect(left + 0.5, top + 0.5, W - 1, H - 1)

            if (root.showGrid) {
                ctx.strokeStyle = "rgba(148,163,184,0.10)"
                ctx.lineWidth = 1

                // fewer lines (fast)
                for (var gx = 1; gx < 6; gx++) {
                    var x = left + (gx / 6.0) * W
                    ctx.beginPath()
                    ctx.moveTo(x, top)
                    ctx.lineTo(x, top + H)
                    ctx.stroke()
                }
                for (var gy = 1; gy < 4; gy++) {
                    var y = top + (gy / 4.0) * H
                    ctx.beginPath()
                    ctx.moveTo(left, y)
                    ctx.lineTo(left + W, y)
                    ctx.stroke()
                }
            }

            // band highlight (keep but light)
            if (root.bandBwHz > 0) {
                var b0 = root.bandCenterHz - root.bandBwHz * 0.5
                var b1 = root.bandCenterHz + root.bandBwHz * 0.5
                var inView = (b1 >= root._fmin && b0 <= root._fmax)
                if (inView) {
                    var xb0 = root._xFromFreq(Math.max(b0, root._fmin))
                    var xb1 = root._xFromFreq(Math.min(b1, root._fmax))
                    var xL = Math.min(xb0, xb1)
                    var xR = Math.max(xb0, xb1)

                    ctx.fillStyle = "rgba(34,197,94,0.08)"
                    ctx.fillRect(xL, top, (xR - xL), H)

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
        }
    }

    // =====================
    // FFT Plot layer (FAST)
    // =====================
    Canvas {
        id: plotCanvas
        anchors.fill: parent
        visible: root.enabled
        antialiasing: false
        renderTarget: Canvas.FramebufferObject

        onPaint: {
            var ctx = getContext("2d")
            ctx.setTransform(1,0,0,1,0,0)
            ctx.clearRect(0,0,width,height)

            if (root._n < 8 || !_isValidArray(root.magDb)) return

            var left = root.padLeft
            var top = root.padTop
            var W = root._plotW()
            var H = root._plotH()
            if (W < 10 || H < 10) return

            var f = root.freqHz
            var m = root.magDb
            var n = root._n

            var targetPts = Math.max(64, Math.floor(W))
            var step = Math.max(1, Math.floor(n / targetPts))

            function xAt(i) {
                if (_isValidArray(f)) return root._xFromFreq(Number(f[i]))
                return left + (i / (n - 1 + 1e-12)) * W
            }

            // gradient line (no glow)
            var grad = ctx.createLinearGradient(left, top, left + W, top)
            grad.addColorStop(0.0, root.cLineA)
            grad.addColorStop(1.0, root.cLineB)

            ctx.strokeStyle = grad
            ctx.lineWidth = 1.4
            ctx.beginPath()

            var x0 = xAt(0)
            var y0 = root._yFromDb(Number(m[0]))
            if (!isFinite(y0)) y0 = top + H * 0.5
            y0 = Math.max(top, Math.min(top + H, y0))
            ctx.moveTo(x0, y0)

            for (var i = step; i < n; i += step) {
                var xi = xAt(i)
                var yi = root._yFromDb(Number(m[i]))
                if (!isFinite(xi) || !isFinite(yi)) continue
                yi = Math.max(top, Math.min(top + H, yi))
                ctx.lineTo(xi, yi)
            }

            var il = n - 1
            var xl = xAt(il)
            var yl = root._yFromDb(Number(m[il]))
            if (isFinite(xl) && isFinite(yl)) {
                yl = Math.max(top, Math.min(top + H, yl))
                ctx.lineTo(xl, yl)
            }

            ctx.stroke()
        }

        Connections {
            id: doaConn
            target: (typeof doaClient !== "undefined") ? doaClient : null
            function onFftChanged() {
                root._markPlotDirty()
                if (root.yAuto) root._markGridDirty()
            }
            function onDoaOffsetHzChanged() { root._markGridDirty() }
            function onDoaBwHzChanged()     { root._markGridDirty() }
        }
    }

    // =====================
    // Labels as QML Items (FAST)
    // =====================
    function _fmtMHz(x) { return (x / 1e6).toFixed(3) + " MHz" }

    Rectangle {
        id: yMaxPill
        visible: root.enabled && root._n >= 8
        x: 10
        y: root.padTop - 2
        radius: 10
        color: "#0F172A"
        border.color: "#2B3856"
        border.width: 1
        opacity: 0.85
        height: 22
        width: yMaxText.paintedWidth + 16
        Text {
            id: yMaxText
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 8
            text: root._mmax.toFixed(1) + " dB"
            color: root.cText
            font.pixelSize: 12
            font.bold: true
        }
    }

    Rectangle {
        id: yMinPill
        visible: root.enabled && root._n >= 8
        x: 10
        y: root.padTop + root._plotH() - 18
        radius: 10
        color: "#0F172A"
        border.color: "#2B3856"
        border.width: 1
        opacity: 0.85
        height: 22
        width: yMinText.paintedWidth + 16
        Text {
            id: yMinText
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 8
            text: root._mmin.toFixed(1) + " dB"
            color: root.cText
            font.pixelSize: 12
            font.bold: true
        }
    }

    Rectangle {
        id: xMinPill
        visible: root.enabled && root._n >= 8
        x: root.padLeft
        y: root.height - 30
        radius: 10
        color: "#0F172A"
        border.color: "#2B3856"
        border.width: 1
        opacity: 0.85
        height: 22
        width: xMinText.paintedWidth + 16
        Text {
            id: xMinText
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 8
            text: _isValidArray(root.freqHz) ? _fmtMHz(root._fmin) : "bin 0"
            color: root.cText
            font.pixelSize: 12
            font.bold: true
        }
    }

    Rectangle {
        id: xMaxPill
        visible: root.enabled && root._n >= 8
        y: root.height - 30
        radius: 10
        color: "#0F172A"
        border.color: "#2B3856"
        border.width: 1
        opacity: 0.85
        height: 22
        width: xMaxText.paintedWidth + 16
        x: root.padLeft + root._plotW() - width
        Text {
            id: xMaxText
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 8
            text: _isValidArray(root.freqHz) ? _fmtMHz(root._fmax) : ("bin " + (root._n - 1))
            color: root.cText
            font.pixelSize: 12
            font.bold: true
        }
    }

    // Disabled overlay (fix rgba for QML)
    Item {
        anchors.fill: parent
        visible: !root.enabled
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(2/255, 6/255, 23/255, 0.55)  // ✅ QML-compatible
        }
        Text {
            anchors.centerIn: parent
            text: "FFT is OFF"
            color: "#F87171"
            font.pixelSize: 18
            font.bold: true
        }
    }

    Component.onCompleted: {
        _dirtyScale = true
        _markGridDirty()
        _markPlotDirty()
    }
}
