import QtQuick 2.15

Rectangle {
    id: root
    radius: 10
    color: "#060B16"
    border.color: "#1F2A44"
    border.width: 1

    property bool enabled: true
    property var theta: []
    property var spectrum: []
    property real peakDeg: 0
    property real conf: 0

    // gate info from doaClient
    property bool signalPresent: false
    property real sigPower: 0.0
    property real bandPeakDb: -200.0
    property real gateThDb: -65.0

    // ===== repaint triggers (NO Connections needed) =====
    onThetaChanged:        c.requestPaint()
    onSpectrumChanged:     c.requestPaint()
    onPeakDegChanged:      c.requestPaint()
    onConfChanged:         c.requestPaint()
    onSignalPresentChanged:c.requestPaint()
    onSigPowerChanged:     c.requestPaint()
    onBandPeakDbChanged:   c.requestPaint()
    onGateThDbChanged:     c.requestPaint()
    onEnabledChanged:      c.requestPaint()
    onWidthChanged:        c.requestPaint()
    onHeightChanged:       c.requestPaint()

    // ---- badge ----
    Rectangle {
        id: sigBadge
        width: 200
        height: 30
        radius: 8
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.rightMargin: 8
        z: 10

        color: root.signalPresent ? "#052e16" : "#3f1d1d"
        border.color: root.signalPresent ? "#16a34a" : "#ef4444"
        border.width: 1

        Row {
            anchors.centerIn: parent
            spacing: 10

            Text {
                text: root.signalPresent ? "SIGNAL" : "NO SIGNAL"
                color: root.signalPresent ? "#22c55e" : "#f87171"
                font.pixelSize: 12
                font.bold: true
            }

            Text {
                text: "Band " + root.bandPeakDb.toFixed(1) + " dB  (th " + root.gateThDb.toFixed(1) + ")"
                color: "#93c5fd"
                font.pixelSize: 10
            }
        }
    }

    Canvas {
        id: c
        anchors.fill: parent
        visible: root.enabled
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)

            var cx = width * 0.5
            var cy = height * 0.55
            var R  = Math.min(width, height) * 0.42

            ctx.fillStyle = "#060B16"
            ctx.fillRect(0, 0, width, height)

            function thToRad(deg) { return (deg - 90) * Math.PI / 180.0 }
            function isFiniteNumber(v) { return (v === v) && isFinite(v) } // v===v => not NaN
            function normDeg(d) {
                var x = Number(d)
                if (!isFiniteNumber(x)) return NaN
                x = x % 360
                if (x < 0) x += 360
                return x
            }

            // grid circles
            ctx.lineWidth = 1
            ctx.strokeStyle = "#142033"
            for (var i = 1; i <= 4; i++) {
                ctx.beginPath()
                ctx.arc(cx, cy, R * i / 4.0, 0, Math.PI * 2)
                ctx.stroke()
            }

            // cross
            ctx.beginPath(); ctx.moveTo(cx - R, cy); ctx.lineTo(cx + R, cy); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(cx, cy - R); ctx.lineTo(cx, cy + R); ctx.stroke()

            // ticks
            ctx.fillStyle = "#94A3B8"
            ctx.font = "12px sans-serif"
            for (var a = 0; a < 360; a += 30) {
                var rad = thToRad(a)
                ctx.fillText(String(a),
                             cx + (R + 14) * Math.cos(rad) - 8,
                             cy + (R + 14) * Math.sin(rad) + 4)
            }

            // gated
            if (!root.signalPresent) {
                ctx.fillStyle = "#F87171"
                ctx.font = "16px sans-serif"
                ctx.fillText("NO SIGNAL (DOA gated)", cx - 105, cy)
                ctx.fillStyle = "#94A3B8"
                ctx.font = "12px sans-serif"
                ctx.fillText("Select offset/BW to hit the tone peak.", cx - 140, cy + 20)
                return
            }

            // gated
            if (!root.signalPresent) {
                ctx.fillStyle = "#F87171"
                ctx.font = "16px Sans"
                ctx.fillText("NO SIGNAL (DOA gated)", cx - 105, cy)
                ctx.fillStyle = "#94A3B8"
                ctx.font = "12px Sans"
                ctx.fillText("Select offset/BW to hit the tone peak.", cx - 140, cy + 20)
                return
            }

            // inputs
            var th = root.theta
            var sp = root.spectrum
            var hasSpectrum = (th && sp && th.length >= 10 && sp.length >= 10)

            // peak can be used even without spectrum (ESPRIT)
            var peak = normDeg(root.peakDeg)
            var hasPeak = isFiniteNumber(peak)

            // ---- If no spectrum: still draw direction if peak is valid ----
            if (!hasSpectrum) {
                if (hasPeak) {
                    var pang = thToRad(peak)

                    // peak line (green for peak-only mode)
                    ctx.lineWidth = 2.4
                    ctx.strokeStyle = "#22c55e"
                    ctx.beginPath()
                    ctx.moveTo(cx, cy)
                    ctx.lineTo(cx + R * Math.cos(pang), cy + R * Math.sin(pang))
                    ctx.stroke()

                    // peak dot
                    ctx.fillStyle = "#22c55e"
                    ctx.beginPath()
                    ctx.arc(cx + (R * 0.98) * Math.cos(pang),
                            cy + (R * 0.98) * Math.sin(pang),
                            5, 0, Math.PI * 2)
                    ctx.fill()

                    // center dot
                    ctx.fillStyle = "#E5E7EB"
                    ctx.beginPath(); ctx.arc(cx, cy, 3, 0, Math.PI * 2); ctx.fill()

                    // info
                    ctx.fillStyle = "#BBF7D0"
                    ctx.font = "12px sans-serif"
                    ctx.fillText("Peak: " + peak.toFixed(1) + "°  Conf: " + root.conf.toFixed(2),
                                 12, height - 10)

                    // hint label
                    ctx.fillStyle = "#94A3B8"
                    ctx.font = "13px sans-serif"
                    ctx.fillText("Peak-only mode (ESPRIT)", 12, 22)
                    return
                } else {
                    ctx.fillStyle = "#94A3B8"
                    ctx.font = "14px sans-serif"
                    ctx.fillText("No DOA data", 12, 22)
                    return
                }
            }

            // =============================
            // MUSIC spectrum mode (hasSpectrum)
            // =============================

            // spectrum polyline
            ctx.lineWidth = 1.6
            ctx.strokeStyle = "#60A5FA"
            ctx.beginPath()
            var x0 = cx + (sp[0] * R) * Math.cos(thToRad(th[0]))
            var y0 = cy + (sp[0] * R) * Math.sin(thToRad(th[0]))
            ctx.moveTo(x0, y0)
            for (var k = 1; k < th.length && k < sp.length; k++) {
                var rr = sp[k] * R
                var ang = thToRad(th[k])
                ctx.lineTo(cx + rr * Math.cos(ang), cy + rr * Math.sin(ang))
            }
            ctx.closePath()
            ctx.stroke()

            // peak line (yellow)
            if (hasPeak) {
                var pang2 = thToRad(peak)
                ctx.lineWidth = 2.0
                ctx.strokeStyle = "#FBBF24"
                ctx.beginPath()
                ctx.moveTo(cx, cy)
                ctx.lineTo(cx + R * Math.cos(pang2), cy + R * Math.sin(pang2))
                ctx.stroke()

                // peak dot
                ctx.fillStyle = "#FBBF24"
                ctx.beginPath()
                ctx.arc(cx + (R * 0.98) * Math.cos(pang2),
                        cy + (R * 0.98) * Math.sin(pang2),
                        4, 0, Math.PI * 2)
                ctx.fill()
            }

            // center dot
            ctx.fillStyle = "#E5E7EB"
            ctx.beginPath(); ctx.arc(cx, cy, 3, 0, Math.PI * 2); ctx.fill()

            // info
            ctx.fillStyle = "#FDE68A"
            ctx.font = "12px sans-serif"
            ctx.fillText("Peak: " + (hasPeak ? peak.toFixed(1) : "NaN") + "°  Conf: " + root.conf.toFixed(2),
                         12, height - 10)
        }
    }

    Item {
        anchors.fill: parent
        visible: !root.enabled
        Text { anchors.centerIn: parent; text: "DOA is OFF"; color: "#F87171"; font.pixelSize: 18 }
    }
}
