// ===== CPUused.qml (FULL FILE) =====
// Donut gauge style (like your example): grey ring + colored progress + end knob
import QtQuick 2.12

Item {
    id: rootCPUused
    width: 400
    height: 400

    // -------- inputs --------
    property real valuePct: 0            // 0..100
    property string title: "CPU"
    property string subText: ""          // e.g. "1 of 12 used"
    property string updatedText: ""      // optional timestamp

    // rotate where 0% begins (degrees). 0° = right, -90° = top, 180° = left
    // For the example vibe: start near bottom-left.
    property real startDeg: 90

    // -------- theme --------
    property color cCard:   "#2A2F33"    // dark grey card like sample
    property color cBorder: "#3D6B7E"    // keep your border tone
    property color cText:   "#E7F2F7"
    property color cMuted:  "#A9B6BF"

    property color cTrack:  "#8E969D"    // grey ring
    property color cBlue:   "#3E86C8"    // normal
    property color cWarn:   "#F2C94C"    // >= 60
    property color cBad:    "#EB5757"    // >= 85

    function clamp(x,a,b){ return Math.max(a, Math.min(b, x)); }
    function clamp01(x){ return clamp(x, 0, 1); }

    function pctColor(p){
        if (p >= 85) return cBad
        if (p >= 60) return cWarn
        return cBlue
    }

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: rootCPUused.cCard
        border.width: 1
        border.color: rootCPUused.cBorder
        clip: true
    }

    Canvas {
        id: ring
        anchors.fill: parent
        anchors.bottomMargin: 123
        antialiasing: true
        renderTarget: Canvas.Image

        function repaint(){ requestPaint(); }

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var w = width
            var h = height

            // center a bit high for text below
            var cx = w * 0.5
            var cy = h * 0.43

            // ring sizing
            var outerR = Math.min(w, h) * 0.33
            var thick  = Math.max(14, outerR * 0.18)
            var r      = outerR

            var p = rootCPUused.clamp(rootCPUused.valuePct, 0, 100)
            var t = rootCPUused.clamp01(p / 100.0)

            function deg2rad(d){ return d * Math.PI / 180.0 }
            var startA = deg2rad(rootCPUused.startDeg)
            var endA   = startA + (Math.PI * 2 * t)

            ctx.lineCap = "butt" // like sample (flat)
            ctx.lineWidth = thick

            // -------- track (full ring) --------
            ctx.strokeStyle = rootCPUused.cTrack
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2, false)
            ctx.stroke()

            // -------- progress arc --------
            var pc = rootCPUused.pctColor(p)
            ctx.strokeStyle = pc
            ctx.beginPath()
            ctx.arc(cx, cy, r, startA, endA, false)
            ctx.stroke()

            // -------- end knob (small circle at arc end) --------
            if (t > 0.001) {
                var kx = cx + Math.cos(endA) * r
                var ky = cy + Math.sin(endA) * r
                var kr = thick * 0.28

                // outer ring of knob
                ctx.beginPath()
                ctx.fillStyle = rootCPUused.cCard
                ctx.arc(kx, ky, kr + 3, 0, Math.PI * 2, false)
                ctx.fill()

                // knob
                ctx.beginPath()
                ctx.fillStyle = pc
                ctx.arc(kx, ky, kr, 0, Math.PI * 2, false)
                ctx.fill()

                // knob outline
                ctx.beginPath()
                ctx.strokeStyle = "#0B0F12"
                ctx.lineWidth = 2
                ctx.arc(kx, ky, kr, 0, Math.PI * 2, false)
                ctx.stroke()
            }

            // -------- start marker (optional small dot) --------
            // (like sample has a small marker/hinge)
            var sx = cx + Math.cos(startA) * r
            var sy = cy + Math.sin(startA) * r
            ctx.beginPath()
            ctx.fillStyle = "#1B2227"
            ctx.arc(sx, sy, thick*0.18, 0, Math.PI*2, false)
            ctx.fill()
            ctx.beginPath()
            ctx.strokeStyle = "#0B0F12"
            ctx.lineWidth = 2
            ctx.arc(sx, sy, thick*0.18, 0, Math.PI*2, false)
            ctx.stroke()
        }

        Connections {
            target: rootCPUused
            function onValuePctChanged(){ ring.repaint() }
            function onWidthChanged(){ ring.repaint() }
            function onHeightChanged(){ ring.repaint() }
            function onStartDegChanged(){ ring.repaint() }
        }
        Component.onCompleted: repaint()
    }

    // -------- center text --------
    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 7
        anchors.topMargin: 283
        anchors.rightMargin: 0
        anchors.leftMargin: 0
        spacing: 10


        Text {
            text: rootCPUused.title
            color: rootCPUused.cText
            font.pixelSize: 22
            font.bold: false
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
            opacity: 0.95
        }

        Text {
            text: Math.round(rootCPUused.valuePct).toString() + "%"
            color: rootCPUused.cText
            font.pixelSize: 64
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }


        Text {
            text: rootCPUused.subText
            visible: rootCPUused.subText !== ""
            color: rootCPUused.cMuted
            font.pixelSize: 18
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }

        Text {
            text: rootCPUused.updatedText
            visible: rootCPUused.updatedText !== ""
            color: rootCPUused.cMuted
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }
    }
}
