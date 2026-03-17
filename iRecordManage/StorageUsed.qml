//StorageUsed.qml
import QtQuick 2.0

Item {
    id: rootStorageUsed
    width: 400
    height: 400

    // -------- inputs --------
    property real valuePct: 0
    property real usedGB: 0
    property real totalGB: 0
    property string title: "STORAGE"
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

    function pctColor(p){
        if (p >= 90) return cBad
        if (p >= 75) return cWarn
        return cBlue
    }

    function fmt1(v){ return Number(v).toFixed(1) }

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

            var p = Math.max(0, Math.min(100, valuePct))
            var t = p / 100.0

            var startA = startDeg * Math.PI / 180
            var endA   = startA + Math.PI * 2 * t

            ctx.lineWidth = thick
            ctx.strokeStyle = cTrack
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI*2)
            ctx.stroke()

            var pc = pctColor(p)
            ctx.strokeStyle = pc
            ctx.beginPath()
            ctx.arc(cx, cy, r, startA, endA)
            ctx.stroke()

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
            target: root
            function onValuePctChanged(){ ring.requestPaint() }
        }
    }

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
            width: parent.width
        }

        Text {
            text: fmt1(usedGB) + " of " + fmt1(totalGB) + " GB used"
            color: cMuted
            font.pixelSize: 18
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }

        Text {
            text: updatedText
            visible: updatedText !== ""
            color: cMuted
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }
    }
}

