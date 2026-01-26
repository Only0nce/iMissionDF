import QtQuick 2.15

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
    radius: 10
    color: "#060B16"
    border.color: "#1F2A44"
    border.width: 1

    // Public API
    property bool enabled: true
    property var  freqHz: []
    property var  magDb: []
    property real bandCenterHz: 0
    property real bandBwHz: 0

    // Render control
    property int  fftFps: 25          // limit paint rate
    property bool showGrid: true

    // Y-axis control
    // - yAuto=true  : scale from incoming FFT data (min/max)
    // - yAuto=false : fixed scale using yMinDb/yMaxDb
    property bool yAuto: false
    property real yMinDb: -150.0
    property real yMaxDb: -50.0

    // Internal scale cache (computed when needed)
    property real _fmin: 0
    property real _fmax: 0
    property real _mmin: -150
    property real _mmax: -50
    property int  _n: 0

    // Dirty flags (avoid repaint storm)
    property bool _dirtyPlot: true
    property bool _dirtyGrid: true

    // Plot area padding
    property int padLeft: 55
    property int padRight: 15
    property int padTop: 15
    property int padBottom: 35

    function _isValidArray(a) {
        return a !== undefined && a !== null && a.length !== undefined && a.length >= 8
    }

    function _calcScale() {
        var f = root.freqHz
        var m = root.magDb
        if (!_isValidArray(f) || !_isValidArray(m)) {
            root._n = 0
            return
        }

        var n = Math.min(f.length, m.length)
        root._n = n

        // freq min/max
        var fmin = Number(f[0])
        var fmax = Number(f[0])
        // mag min/max
        var mmin = Number(m[0])
        var mmax = Number(m[0])

        for (var i = 1; i < n; i++) {
            var fi = Number(f[i])
            var mi = Number(m[i])
            if (fi < fmin) fmin = fi
            if (fi > fmax) fmax = fi
            if (mi < mmin) mmin = mi
            if (mi > mmax) mmax = mi
        }

        // fixed Y range if yAuto is off
        if (!root.yAuto) {
            var lo = Number(root.yMinDb)
            var hi = Number(root.yMaxDb)
            if (isNaN(lo)) lo = mmin
            if (isNaN(hi)) hi = mmax
            if (hi <= lo) hi = lo + 1.0
            mmin = lo
            mmax = hi
        } else {
            // avoid degenerate range
            if (mmax <= mmin) mmax = mmin + 1.0
        }

        // avoid degenerate freq range
        if (fmax <= fmin) fmax = fmin + 1.0

        root._fmin = fmin
        root._fmax = fmax
        root._mmin = mmin
        root._mmax = mmax
    }

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

    // Mark dirty (do NOT paint immediately)
    function _markPlotDirty() { _dirtyPlot = true }
    function _markGridDirty() { _dirtyGrid = true }

    // Data triggers
    onFreqHzChanged: {
        _calcScale()
        _markGridDirty()
        _markPlotDirty()
    }
    onMagDbChanged: {
        _calcScale()
        if (root.yAuto) _markGridDirty()
        _markPlotDirty()
    }
    onBandCenterHzChanged: _markGridDirty()
    onBandBwHzChanged: _markGridDirty()
    onYAutoChanged: {
        _calcScale()
        _markGridDirty()
        _markPlotDirty()
    }
    onYMinDbChanged: {
        if (!root.yAuto) {
            _calcScale()
            _markGridDirty()
            _markPlotDirty()
        }
    }
    onYMaxDbChanged: {
        if (!root.yAuto) {
            _calcScale()
            _markGridDirty()
            _markPlotDirty()
        }
    }

    // When resized, repaint everything (once)
    onWidthChanged:  { _markGridDirty(); _markPlotDirty() }
    onHeightChanged: { _markGridDirty(); _markPlotDirty() }

    // FPS throttler: paints only when dirty
    Timer {
        id: paintTimer
        interval: Math.max(10, Math.floor(1000 / Math.max(1, root.fftFps)))
        running: root.enabled && root.visible
        repeat: true
        onTriggered: {
            if (root._dirtyGrid) {
                gridCanvas.requestPaint()
                root._dirtyGrid = false
            }
            if (root._dirtyPlot) {
                plotCanvas.requestPaint()
                root._dirtyPlot = false
            }
        }
    }

    // =====================
    // Grid / Background layer (cached)
    // =====================
    Canvas {
        id: gridCanvas
        anchors.fill: parent
        visible: root.enabled
        antialiasing: false

        onPaint: {
            var ctx = getContext("2d")

            // reset transform but keep state cheap
            ctx.setTransform(1, 0, 0, 1, 0, 0)
            ctx.clearRect(0, 0, width, height)

            // background
            ctx.fillStyle = "#060B16"
            ctx.fillRect(0, 0, width, height)

            // no data overlay
            if (root._n < 8 || !_isValidArray(root.freqHz) || !_isValidArray(root.magDb)) {
                ctx.fillStyle = "#94A3B8"
                ctx.font = "15px Arial, Helvetica, sans-serif"
                ctx.fillText("No FFT data", 12, 22)
                return
            }

            // plot area
            var left = root.padLeft
            var top = root.padTop
            var W = root._plotW()
            var H = root._plotH()
            if (W < 10 || H < 10) return

            // grid
            if (root.showGrid) {
                ctx.strokeStyle = "rgba(148,163,184,0.12)"
                ctx.lineWidth = 1

                // vertical grid
                for (var gx = 0; gx <= 10; gx++) {
                    var x = left + (gx / 10.0) * W
                    ctx.beginPath()
                    ctx.moveTo(x, top)
                    ctx.lineTo(x, top + H)
                    ctx.stroke()
                }

                // horizontal grid
                for (var gy = 0; gy <= 6; gy++) {
                    var y = top + (gy / 6.0) * H
                    ctx.beginPath()
                    ctx.moveTo(left, y)
                    ctx.lineTo(left + W, y)
                    ctx.stroke()
                }
            }

            // band highlight
            if (root.bandBwHz > 0) {
                var b0 = root.bandCenterHz - root.bandBwHz * 0.5
                var b1 = root.bandCenterHz + root.bandBwHz * 0.5
                var inView = (b1 >= root._fmin && b0 <= root._fmax)
                if (inView) {
                    var xb0 = root._xFromFreq(Math.max(b0, root._fmin))
                    var xb1 = root._xFromFreq(Math.min(b1, root._fmax))

                    ctx.fillStyle = "rgba(34,197,94,0.10)"
                    ctx.fillRect(Math.min(xb0, xb1), top, Math.abs(xb1 - xb0), H)

                    // center line (only if inside view)
                    if (root.bandCenterHz >= root._fmin && root.bandCenterHz <= root._fmax) {
                        ctx.strokeStyle = "rgba(34,197,94,0.55)"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(root._xFromFreq(root.bandCenterHz), top)
                        ctx.lineTo(root._xFromFreq(root.bandCenterHz), top + H)
                        ctx.stroke()
                    }
                }
            }

            // labels
            ctx.fillStyle = "#94A3B8"
            ctx.font = "15px Arial, Helvetica, sans-serif"

            // y labels (min/max)
            ctx.fillText(root._mmax.toFixed(1) + " dB", 10, top + 10)
            ctx.fillText(root._mmin.toFixed(1) + " dB", 10, top + H)

            // x labels (min/max)
            ctx.fillText((root._fmin / 1e6).toFixed(3) + " MHz", left, height - 12)
            ctx.fillText((root._fmax / 1e6).toFixed(3) + " MHz", left + W - 80, height - 12)
        }
    }

    // =====================
    // FFT Plot layer (fast, repainted often)
    // =====================
    Canvas {
        id: plotCanvas
        anchors.fill: parent
        visible: root.enabled
        antialiasing: false

        onPaint: {
            var ctx = getContext("2d")

            // clear only this layer
            ctx.setTransform(1, 0, 0, 1, 0, 0)
            ctx.clearRect(0, 0, width, height)

            if (root._n < 8 || !_isValidArray(root.freqHz) || !_isValidArray(root.magDb)) {
                return
            }

            var left = root.padLeft
            var top = root.padTop
            var W = root._plotW()
            var H = root._plotH()
            if (W < 10 || H < 10) return

            var f = root.freqHz
            var m = root.magDb
            var n = root._n

            // Downsample: draw roughly 1 point per pixel
            var targetPts = Math.max(64, Math.floor(W))
            var step = Math.max(1, Math.floor(n / targetPts))

            ctx.strokeStyle = "#60A5FA"
            ctx.lineWidth = 1.2
            ctx.beginPath()

            // start point
            var i0 = 0
            var x0 = root._xFromFreq(Number(f[i0]))
            var y0 = root._yFromDb(Number(m[i0]))
            ctx.moveTo(x0, y0)

            for (var i = step; i < n; i += step) {
                var xi = root._xFromFreq(Number(f[i]))
                var yi = root._yFromDb(Number(m[i]))

                // clamp (avoid NaN/inf)
                if (!isFinite(xi) || !isFinite(yi)) continue
                if (yi < top) yi = top
                else if (yi > top + H) yi = top + H

                ctx.lineTo(xi, yi)
            }

            // ensure last point
            var il = n - 1
            var xl = root._xFromFreq(Number(f[il]))
            var yl = root._yFromDb(Number(m[il]))
            if (isFinite(xl) && isFinite(yl)) {
                if (yl < top) yl = top
                else if (yl > top + H) yl = top + H
                ctx.lineTo(xl, yl)
            }

            ctx.stroke()
        }

        // Optional external signals: keep compatibility with existing project
        // (Do not crash if doaClient is not present)
        Connections {
            id: doaConn
            target: (typeof doaClient !== "undefined") ? doaClient : null
            function onFftChanged() {
                root._calcScale()
                if (root.yAuto) root._markGridDirty()
                root._markPlotDirty()
            }
            function onDoaOffsetHzChanged() { root._markGridDirty() }
            function onDoaBwHzChanged() { root._markGridDirty() }
        }
    }

    // Disabled overlay
    Item {
        anchors.fill: parent
        visible: !root.enabled
        Text {
            anchors.centerIn: parent
            text: "FFT is OFF"
            color: "#F87171"
            font.pixelSize: 18
        }
    }

    // Initial scale calc
    Component.onCompleted: {
        _calcScale()
        _markGridDirty()
        _markPlotDirty()
    }
}
