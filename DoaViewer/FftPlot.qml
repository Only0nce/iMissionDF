import QtQuick 2.15
import QtQuick.Controls 2.15

// ============================================================
// FftPlot.qml (solid colors, NO gradients / NO rgba alpha)
// + Click in FFT => move "offset" (doaOffsetHz) ONLY (NOT change fcHz)
// + Auto adjust offsetMin/offsetMax "window" on click
// + If computed offset puts center == baseFcHz (i.e. offset overlaps center frequency) =>
//      warn + revert to last non-center offset, THEN SEND
// + Otherwise => SEND (requested style):
//      doaClient.doaOffsetHz = off
//      doaClient.doaBwHz     = bw
//      doaClient.applyDoaTone()
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
    property var  freqHz: []
    property var  magDb: []
    property real bandCenterHz: 0      // (optional) display highlight center (fc+offset)
    property real bandBwHz: 0

    // IMPORTANT: base fc (no offset)
    property real baseFcHz: 0

    // Render control
    property int  fftFps: 12
    property bool showGrid: true

    // Y-axis control
    property bool yAuto: false
    property real yMinDb: -150.0
    property real yMaxDb: -50.0

    // Click-to-offset control
    property bool clickOffsetEnabled: true

    // (optional) keep if you still want to block clicking near bandCenterHz
    // BUT requested change is "if overlaps center frequency" => revert+send.
    property real centerGuardHz: 0.0   // set 0 to disable "near bandCenterHz" block

    // ===== Offset allowed range =====
    property real offsetMinHz: NaN
    property real offsetMaxHz: NaN
    property bool offsetRangeAuto: true
    property real offsetRangeSpanHz: 0

    // BW to apply on click (NaN -> bandBwHz -> doaClient.doaBwHz -> 2000)
    property real doaBwToApplyHz: NaN

    // marker
    property bool showOffsetMarker: true
    property real offsetMarkerHz: NaN

    // clickedHz emitted is the actual center after clamp/auto
    signal offsetRequested(real newOffsetHz, real clickedHz)
    signal offsetRangeChanged(real newMinHz, real newMaxHz)

    // =========================
    // Internal scale cache
    // =========================
    property real _fmin: 0
    property real _fmax: 0
    property real _mmin: -150
    property real _mmax: -50
    property int  _n: 0

    // Dirty flags
    property bool _dirtyPlot: true
    property bool _dirtyGrid: true
    property bool _dirtyScale: true

    // Plot padding
    property int padLeft: 64
    property int padRight: 18
    property int padTop: 16
    property int padBottom: 40

    // Style (SOLID only)
    property color cBg0:    "#060B16"
    property color cGrid:   "#142033"
    property color cText:   "#AAB7D1"
    property color cLineA:  "#38BDF8"
    property color cBand:   "#1B3A2A"
    property color cBandLn: "#22C55E"
    property color cBorder: "#24314C"
    property color cMarkLn: "#F59E0B"
    property color cWarnBg: "#2A0B0B"
    property color cWarnBd: "#7F1D1D"
    property color cWarnTx: "#FCA5A5"

    // Axis labels style
    property int axisFontPx: 11
    property color cAxisText: cText
    property color cAxisTick: cGrid

    // =========================
    // Helpers
    // =========================
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
    function _freqFromX(px) {
        var W = _plotW()
        if (W <= 1) return root._fmin
        var t = (px - root.padLeft) / W
        if (t < 0) t = 0
        if (t > 1) t = 1
        return root._fmin + t * (root._fmax - root._fmin)
    }
    function _clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

    // doaClient safe
    function dc() {
        return (typeof(doaClient) !== "undefined" && doaClient !== null) ? doaClient : null
    }

    // =========================
    // Offset range (AUTO window)
    // =========================
    property real _offMinAuto: -500000.0
    property real _offMaxAuto:  500000.0

    // remember last offset that DOES NOT overlap center freq (baseFcHz)
    property real lastNonCenterOffsetHz: NaN

    function _getOffMin() {
        return isFinite(root.offsetMinHz) ? Number(root.offsetMinHz) : Number(root._offMinAuto)
    }
    function _getOffMax() {
        return isFinite(root.offsetMaxHz) ? Number(root.offsetMaxHz) : Number(root._offMaxAuto)
    }

    function _rangeSpanHz(curMin, curMax) {
        var span = Number(root.offsetRangeSpanHz)
        if (isFinite(span) && span > 0) return span
        if (isFinite(root.bandBwHz) && root.bandBwHz > 0) return Number(root.bandBwHz)
        var s2 = Number(curMax) - Number(curMin)
        if (isFinite(s2) && s2 > 0) return s2
        return 1000000.0
    }

    function _setAutoRange(minHz, maxHz) {
        root._offMinAuto = Number(minHz)
        root._offMaxAuto = Number(maxHz)
        root.offsetRangeChanged(root._offMinAuto, root._offMaxAuto)
    }

    // =========================
    // SEND style (requested)
    // =========================
    function _resolveBwToSend() {
        var bw = Number(root.doaBwToApplyHz)
        if (isFinite(bw) && bw > 0) return bw

        bw = Number(root.bandBwHz)
        if (isFinite(bw) && bw > 0) return bw

        var c = dc()
        if (c) {
            var bw2 = Number(c.doaBwHz)
            if (isFinite(bw2) && bw2 > 0) return bw2
        }
        return 2000
    }

    function _sendOffsetBwApply(offHz, bwHz) {
        var c = dc()
        if (!c) return

        var off = Number(offHz)
        var bw  = Number(bwHz)

        if (!isFinite(off)) off = 0
        if (!isFinite(bw) || bw <= 0) bw = 2000

        // exactly your style
        try { c.doaOffsetHz = off } catch(e1) {}
        try { c.doaBwHz     = bw  } catch(e2) {}

        if (c.applyDoaTone !== undefined && typeof c.applyDoaTone === "function") {
            try { c.applyDoaTone() } catch(e3) {}
            return
        }

        // fallback optional
        if (c.sendJson !== undefined && typeof c.sendJson === "function") {
            c.sendJson({ menuID: "applyDoaTone", offset_hz: off, bw_hz: bw })
        }
    }

    function _warn(msg, px, py) {
        warnText.text = msg
        warnBox.x = Math.max(6, Math.min(root.width  - warnBox.width - 6, px - warnBox.width/2))
        warnBox.y = Math.max(6, Math.min(root.height - warnBox.height - 6, py - warnBox.height - 10))
        warnBox.visible = true
        warnTimer.restart()
    }

    // =========================
    // Scale calc
    // =========================
    function _calcScale() {
        var f = root.freqHz
        var m = root.magDb
        if (!_isValidArray(m)) { root._n = 0; return }

        var n = m.length
        if (_isValidArray(f)) n = Math.min(f.length, m.length)
        root._n = n

        var fmin = 0, fmax = 1
        if (_isValidArray(f)) {
            var f0 = Number(f[0])
            var f1 = Number(f[n - 1])
            fmin = Math.min(f0, f1)
            fmax = Math.max(f0, f1)
            if (!isFinite(fmin) || !isFinite(fmax) || fmax <= fmin) {
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

        var mmin, mmax
        if (!root.yAuto) {
            mmin = Number(root.yMinDb); mmax = Number(root.yMaxDb)
            if (!isFinite(mmin)) mmin = -150.0
            if (!isFinite(mmax)) mmax = -50.0
            if (mmax <= mmin) mmax = mmin + 1.0
        } else {
            var mmn = Number(m[0]), mmx = Number(m[0])
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

        root._fmin = fmin; root._fmax = fmax
        root._mmin = mmin; root._mmax = mmax
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
            if (root._dirtyScale) { root._calcScale(); root._dirtyScale = false }
            if (root._dirtyGrid)  { gridCanvas.requestPaint(); root._dirtyGrid = false }
            if (root._dirtyPlot)  { plotCanvas.requestPaint(); root._dirtyPlot = false }
            if (root.showOffsetMarker && isFinite(root.offsetMarkerHz)) markerCanvas.requestPaint()
        }
    }

    // =====================
    // Grid / Background
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
            ctx.globalAlpha = 1.0

            ctx.fillStyle = root.cBg0
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

            ctx.strokeStyle = root.cBorder
            ctx.lineWidth = 1
            ctx.strokeRect(left + 0.5, top + 0.5, W - 1, H - 1)

            if (root.showGrid) {
                ctx.strokeStyle = root.cGrid
                ctx.lineWidth = 1
                for (var gx = 1; gx < 6; gx++) {
                    var x = left + (gx / 6.0) * W
                    ctx.beginPath()
                    ctx.moveTo(x + 0.5, top)
                    ctx.lineTo(x + 0.5, top + H)
                    ctx.stroke()
                }
                for (var gy = 1; gy < 4; gy++) {
                    var y = top + (gy / 4.0) * H
                    ctx.beginPath()
                    ctx.moveTo(left, y + 0.5)
                    ctx.lineTo(left + W, y + 0.5)
                    ctx.stroke()
                }
            }

            // band highlight (visual only)
            if (root.bandBwHz > 0) {
                var b0 = root.bandCenterHz - root.bandBwHz * 0.5
                var b1 = root.bandCenterHz + root.bandBwHz * 0.5
                var inView = (b1 >= root._fmin && b0 <= root._fmax)
                if (inView) {
                    var xb0 = root._xFromFreq(Math.max(b0, root._fmin))
                    var xb1 = root._xFromFreq(Math.min(b1, root._fmax))
                    var xL = Math.min(xb0, xb1)
                    var xR = Math.max(xb0, xb1)

                    ctx.fillStyle = root.cBand
                    ctx.fillRect(xL, top, (xR - xL), H)

                    if (root.bandCenterHz >= root._fmin && root.bandCenterHz <= root._fmax) {
                        var xc = root._xFromFreq(root.bandCenterHz)
                        ctx.strokeStyle = root.cBandLn
                        ctx.beginPath()
                        ctx.moveTo(xc + 0.5, top)
                        ctx.lineTo(xc + 0.5, top + H)
                        ctx.stroke()
                    }
                }
            }

        }
    }

    // =====================
    // FFT Plot
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
            ctx.globalAlpha = 1.0

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

            ctx.strokeStyle = root.cLineA
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
            target: (typeof doaClient !== "undefined") ? doaClient : null

            function onFftChanged() {
                root._markPlotDirty()
                if (root.yAuto) root._markGridDirty()
            }

            // sync marker + remember last NON-center offset
            function onDoaOffsetHzChanged() {
                root._markGridDirty()
                var c = root.dc()
                if (!c) return

                var base = Number(root.baseFcHz)
                var off  = Number(c.doaOffsetHz)

                // "overlap center frequency" means offset == 0 (center == baseFcHz)
                if (isFinite(off) && Math.abs(off) > 1e-9) {
                    root.lastNonCenterOffsetHz = off
                }

                if (isFinite(base) && isFinite(off) && base > 0) {
                    root.offsetMarkerHz = base + off
                    markerCanvas.requestPaint()
                }
            }

            // auto window reset around current offset when BW changes
            function onDoaBwHzChanged() {
                root._markGridDirty()

                if (!root.offsetRangeAuto) return
                if (isFinite(root.offsetMinHz) || isFinite(root.offsetMaxHz)) return

                var c = root.dc()
                if (!c) return
                var off = Number(c.doaOffsetHz)
                if (!isFinite(off)) off = 0

                var span = Number(root.offsetRangeSpanHz)
                if (!(isFinite(span) && span > 0))
                    span = (isFinite(root.bandBwHz) && root.bandBwHz > 0) ? Number(root.bandBwHz) : 1000000.0

                root._setAutoRange(off - span * 0.5, off + span * 0.5)
            }
        }
    }

    // =====================
    // Click area => compute offset + auto range + center-overlap rule
    // =====================
    // MouseArea {
    //     anchors.fill: parent
    //     enabled: root.enabled && root.clickOffsetEnabled
    //     hoverEnabled: true
    //     cursorShape: Qt.PointingHandCursor

    //     function _inPlot(mx, my) {
    //         var left = root.padLeft
    //         var top = root.padTop
    //         var W = root._plotW()
    //         var H = root._plotH()
    //         return (mx >= left && mx <= left + W && my >= top && my <= top + H)
    //     }

    //     onClicked: {
    //         if (!_inPlot(mouse.x, mouse.y)) return
    //         if (root._n < 8) return

    //         if (root._dirtyScale) {
    //             root._calcScale()
    //             root._dirtyScale = false
    //         }

    //         // clicked frequency
    //         var clickedHz = root._freqFromX(mouse.x)

    //         // base fc
    //         var base = Number(root.baseFcHz)
    //         if (!isFinite(base) || base <= 0) {
    //             root._warn("baseFcHz is not ready", mouse.x, mouse.y)
    //             return
    //         }

    //         // raw offset from click
    //         var newOff = clickedHz - base

    //         // OPTIONAL: still block "near bandCenterHz"
    //         if (root.centerGuardHz > 0 && isFinite(root.bandCenterHz)) {
    //             var d = Math.abs(clickedHz - root.bandCenterHz)
    //             if (d <= root.centerGuardHz) {
    //                 root._warn("Clicking near the band center is not allowed\nPlease click further left/right", mouse.x, mouse.y)
    //                 return
    //             }
    //         }

    //         // current allowed window
    //         var curMin = root._getOffMin()
    //         var curMax = root._getOffMax()

    //         // AUTO: if click outside window -> shift window to include newOff
    //         if (root.offsetRangeAuto) {
    //             if (newOff < curMin || newOff > curMax) {
    //                 var span = root._rangeSpanHz(curMin, curMax)
    //                 var half = span * 0.5
    //                 root._setAutoRange(newOff - half, newOff + half)
    //                 curMin = root._getOffMin()
    //                 curMax = root._getOffMax()
    //             }
    //         }

    //         // clamp
    //         newOff = root._clamp(newOff, curMin, curMax)

    //         // BW that will be sent/applied
    //         var bwToSend = root._resolveBwToSend()

    //         // =========================
    //         // RULE: If offset overlaps center frequency (center == baseFcHz) => offset == 0
    //         // warn + revert to lastNonCenterOffsetHz, THEN SEND
    //         // =========================
    //         if (Math.abs(newOff) <= 1e-9) {
    //             root._warn("Offset overlaps the center frequency\nReverting to the last offset", mouse.x, mouse.y)

    //             var revertOff = root.lastNonCenterOffsetHz

    //             // fallback: use current offset from doaClient if non-zero
    //             if (!isFinite(revertOff)) {
    //                 var c2 = root.dc()
    //                 if (c2) {
    //                     var curOff = Number(c2.doaOffsetHz)
    //                     if (isFinite(curOff) && Math.abs(curOff) > 1e-9)
    //                         revertOff = curOff
    //                 }
    //             }

    //             // still none -> do nothing
    //             if (!isFinite(revertOff)) return

    //             // ensure revertOff inside window (or shift window)
    //             if (root.offsetRangeAuto) {
    //                 if (revertOff < curMin || revertOff > curMax) {
    //                     var spanR = root._rangeSpanHz(curMin, curMax)
    //                     var halfR = spanR * 0.5
    //                     root._setAutoRange(revertOff - halfR, revertOff + halfR)
    //                     curMin = root._getOffMin()
    //                     curMax = root._getOffMax()
    //                 }
    //             }
    //             revertOff = root._clamp(revertOff, curMin, curMax)

    //             var actualHzR = base + revertOff
    //             root.offsetMarkerHz = actualHzR
    //             root.offsetRequested(revertOff, actualHzR)

    //             // SEND
    //             root._sendOffsetBwApply(revertOff, bwToSend)
    //             markerCanvas.requestPaint()
    //             return
    //         }

    //         // =========================
    //         // offset != center frequency => SEND
    //         // =========================
    //         var actualCenterHz = base + newOff

    //         // remember last offset that doesn't overlap center
    //         root.lastNonCenterOffsetHz = newOff

    //         root.offsetMarkerHz = actualCenterHz
    //         root.offsetRequested(newOff, actualCenterHz)

    //         // SEND
    //         root._sendOffsetBwApply(newOff, bwToSend)

    //         markerCanvas.requestPaint()
    //     }
    // }
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
    MouseArea {
        anchors.fill: parent
        enabled: root.enabled && root.clickOffsetEnabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        function _inPlot(mx, my) {
            var left = root.padLeft
            var top = root.padTop
            var W = root._plotW()
            var H = root._plotH()
            return (mx >= left && mx <= left + W && my >= top && my <= top + H)
        }

        onClicked: {
            if (!_inPlot(mouse.x, mouse.y)) return
            if (root._n < 8) return

            if (root._dirtyScale) {
                root._calcScale()
                root._dirtyScale = false
            }

            // clicked frequency
            var clickedHz = root._freqFromX(mouse.x)

            // base fc
            var base = Number(root.baseFcHz)
            if (!isFinite(base) || base <= 0) {
                root._warn("baseFcHz is not ready", mouse.x, mouse.y)
                return
            }

            // raw offset from click
            var newOff = clickedHz - base

            // current allowed window
            var curMin = root._getOffMin()
            var curMax = root._getOffMax()

            // AUTO: if click outside window -> shift window to include newOff
            if (root.offsetRangeAuto) {
                if (newOff < curMin || newOff > curMax) {
                    var span = root._rangeSpanHz(curMin, curMax)
                    var half = span * 0.5
                    root._setAutoRange(newOff - half, newOff + half)
                    curMin = root._getOffMin()
                    curMax = root._getOffMax()
                }
            }

            // clamp
            newOff = root._clamp(newOff, curMin, curMax)

            // BW to send
            var bwToSend = root._resolveBwToSend()

            // actual center after clamp
            var actualCenterHz = base + newOff

            // marker + signal
            root.offsetMarkerHz = actualCenterHz
            root.offsetRequested(newOff, actualCenterHz)

            // SEND (always)
            root._sendOffsetBwApply(newOff, bwToSend)

            markerCanvas.requestPaint()
        }
    }


    // =====================
    // Marker overlay
    // =====================
    Canvas {
        id: markerCanvas
        anchors.fill: parent
        visible: root.enabled && root.showOffsetMarker && isFinite(root.offsetMarkerHz)
        antialiasing: false
        renderTarget: Canvas.FramebufferObject

        onPaint: {
            var ctx = getContext("2d")
            ctx.setTransform(1,0,0,1,0,0)
            ctx.clearRect(0,0,width,height)
            ctx.globalAlpha = 1.0

            if (!isFinite(root.offsetMarkerHz)) return
            if (root._n < 8) return

            var top  = root.padTop
            var H = root._plotH()
            if (H < 10) return

            var hz = root.offsetMarkerHz
            if (hz < root._fmin) hz = root._fmin
            if (hz > root._fmax) hz = root._fmax

            var x = root._xFromFreq(hz)

            ctx.strokeStyle = root.cMarkLn
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(x + 0.5, top)
            ctx.lineTo(x + 0.5, top + H)
            ctx.stroke()
        }
    }

    // =====================
    // Warning bubble
    // =====================
    Rectangle {
        id: warnBox
        visible: false
        width: 220
        height: 44
        radius: 10
        color: root.cWarnBg
        border.color: root.cWarnBd
        border.width: 1
        z: 999

        Text {
            id: warnText
            anchors.fill: parent
            anchors.margins: 10
            text: ""
            color: root.cWarnTx
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }

    Timer {
        id: warnTimer
        interval: 1200
        repeat: false
        onTriggered: warnBox.visible = false
    }

    // =====================
    // Disabled overlay
    // =====================
    Item {
        anchors.fill: parent
        visible: !root.enabled
        Rectangle { anchors.fill: parent; color: Qt.rgba(2/255, 6/255, 23/255, 0.55) }
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

        // init auto window if user didn't set min/max
        if (root.offsetRangeAuto && !isFinite(root.offsetMinHz) && !isFinite(root.offsetMaxHz)) {
            var c = root.dc()
            var off0 = (c && isFinite(Number(c.doaOffsetHz))) ? Number(c.doaOffsetHz) : 0

            // last "non-center" offset
            if (isFinite(off0) && Math.abs(off0) > 1e-9) {
                root.lastNonCenterOffsetHz = off0
            }

            var span0 = Number(root.offsetRangeSpanHz)
            if (!(isFinite(span0) && span0 > 0))
                span0 = (isFinite(root.bandBwHz) && root.bandBwHz > 0) ? Number(root.bandBwHz) : 1000000.0

            root._setAutoRange(off0 - span0 * 0.5, off0 + span0 * 0.5)
        }
    }
}
