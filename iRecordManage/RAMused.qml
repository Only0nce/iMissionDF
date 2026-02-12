// ===== RAMused.qml (FULL FILE) =====
import QtQuick 2.12

Item {
    id: rootRAMused
    width: 400
    height: 400

    // -------- inputs --------
    property real valuePct: 0        // 0..100 (สำคัญ!)
    property real usedMB: 0
    property real totalMB: 0

    property string title: "RAM"
    property string updatedText: ""

    property real startDeg: 90

    // -------- theme --------
    property color cCard:   "#2A2F33"
    property color cBorder: "#3D6B7E"
    property color cText:   "#E7F2F7"
    property color cMuted:  "#A9B6BF"

    property color cTrack:  "#8E969D"
    property color cBlue:   "#3E86C8"
    property color cWarn:   "#F2C94C"
    property color cBad:    "#EB5757"

    function clamp(x,a,b){ return Math.max(a, Math.min(b, x)); }
    function clamp01(x){ return clamp(x, 0, 1); }

    function pctColor(p){
        if (p >= 85) return cBad
        if (p >= 60) return cWarn
        return cBlue
    }

    function fmt1(v){ return Number(v).toFixed(1) }

    function autoSubText(){
        if (totalMB <= 0) return ""
        return fmt1(usedMB/1024.0) + " of " +
               fmt1(totalMB/1024.0) + " GiB used"
    }

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: cCard
        border.width: 1
        border.color: cBorder
    }

    Canvas {
        id: ring
        anchors.fill: parent
        anchors.bottomMargin: 123
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var cx = width * 0.5
            var cy = height * 0.43
            var r  = Math.min(width, height) * 0.33
            var thick = Math.max(14, r * 0.18)

            var p = clamp(valuePct, 0, 100)
            var t = p / 100.0

            var startA = startDeg * Math.PI / 180
            var endA   = startA + Math.PI * 2 * t

            // track
            ctx.lineWidth = thick
            ctx.strokeStyle = cTrack
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI*2)
            ctx.stroke()

            // progress
            var pc = pctColor(p)
            ctx.strokeStyle = pc
            ctx.beginPath()
            ctx.arc(cx, cy, r, startA, endA)
            ctx.stroke()

            // knob
            if (t > 0.001) {
                var kx = cx + Math.cos(endA) * r
                var ky = cy + Math.sin(endA) * r
                var kr = thick * 0.28

                ctx.fillStyle = cCard
                ctx.beginPath()
                ctx.arc(kx, ky, kr + 3, 0, Math.PI*2)
                ctx.fill()

                ctx.fillStyle = pc
                ctx.beginPath()
                ctx.arc(kx, ky, kr, 0, Math.PI*2)
                ctx.fill()
            }
        }

        Connections {
            target: rootRAMused
            function onValuePctChanged(){ ring.requestPaint() }
        }
    }

    // -------- text --------
    Column {
        anchors.fill: parent
        anchors.topMargin: 283
        spacing: 10

        Text {
            text: title
            color: cText
            font.pixelSize: 22
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }

        Text {
            text: fmt1(valuePct) + "%"
            color: cText
            font.pixelSize: 64
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignTop
            width: parent.width
        }

        Text {
            text: autoSubText()
            color: cMuted
            font.pixelSize: 18
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }

        Text {
            text: updatedText
            color: cMuted
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
            visible: updatedText !== ""
        }
    }
}
