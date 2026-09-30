// ===== CPUused.qml =====
// Phase 6 HMI: presentation-only migration. Input contract is unchanged.
import QtQuick 2.12
import "../ui"

Item {
    id: rootCPUused
    width: 400
    height: 400

    property bool darkMode: true
    property real valuePct: 0
    property string title: "CPU"
    property string subText: ""
    property string updatedText: ""
    property real startDeg: 90

    Theme { id: hmiTheme; darkMode: rootCPUused.darkMode }

    property color cCard:   hmiTheme.card
    property color cBorder: hmiTheme.line
    property color cText:   hmiTheme.text
    property color cMuted:  hmiTheme.muted
    property color cTrack:  hmiTheme.lineStrong
    property color cBlue:   hmiTheme.info
    property color cWarn:   hmiTheme.warning
    property color cBad:    hmiTheme.danger

    function clamp(x,a,b){ return Math.max(a, Math.min(b, x)); }
    function clamp01(x){ return clamp(x, 0, 1); }
    function pctColor(p){
        if (p >= 85) return cBad
        if (p >= 60) return cWarn
        return cBlue
    }

    Rectangle {
        anchors.fill: parent
        radius: hmiTheme.radiusLg
        color: rootCPUused.cCard
        border.width: 1
        border.color: rootCPUused.cBorder
        clip: true
    }

    Canvas {
        id: ring
        anchors.fill: parent
        anchors.bottomMargin: Math.max(104, rootCPUused.height * 0.29)
        antialiasing: true
        renderTarget: Canvas.Image

        function repaint(){ requestPaint(); }

        onPaint: {
            var ctx = getContext("2d")
            if (!ctx) return
            ctx.reset()

            var w = width
            var h = height
            var cx = w * 0.5
            var cy = h * 0.44
            var outerR = Math.min(w, h) * 0.31
            var thick = Math.max(14, outerR * 0.18)
            var r = outerR
            var p = rootCPUused.clamp(rootCPUused.valuePct, 0, 100)
            var t = rootCPUused.clamp01(p / 100.0)

            function deg2rad(d){ return d * Math.PI / 180.0 }
            var startA = deg2rad(rootCPUused.startDeg)
            var endA = startA + (Math.PI * 2 * t)

            ctx.lineCap = "butt"
            ctx.lineWidth = thick
            ctx.strokeStyle = rootCPUused.cTrack
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2, false)
            ctx.stroke()

            var pc = rootCPUused.pctColor(p)
            ctx.strokeStyle = pc
            ctx.beginPath()
            ctx.arc(cx, cy, r, startA, endA, false)
            ctx.stroke()

            if (t > 0.001) {
                var kx = cx + Math.cos(endA) * r
                var ky = cy + Math.sin(endA) * r
                var kr = thick * 0.28
                ctx.beginPath()
                ctx.fillStyle = rootCPUused.cCard
                ctx.arc(kx, ky, kr + 3, 0, Math.PI * 2, false)
                ctx.fill()
                ctx.beginPath()
                ctx.fillStyle = pc
                ctx.arc(kx, ky, kr, 0, Math.PI * 2, false)
                ctx.fill()
                ctx.beginPath()
                ctx.strokeStyle = hmiTheme.plot
                ctx.lineWidth = 2
                ctx.arc(kx, ky, kr, 0, Math.PI * 2, false)
                ctx.stroke()
            }

            var sx = cx + Math.cos(startA) * r
            var sy = cy + Math.sin(startA) * r
            ctx.beginPath()
            ctx.fillStyle = hmiTheme.input
            ctx.arc(sx, sy, thick * 0.18, 0, Math.PI * 2, false)
            ctx.fill()
        }

        Connections {
            target: rootCPUused
            function onValuePctChanged(){ ring.repaint() }
            function onWidthChanged(){ ring.repaint() }
            function onHeightChanged(){ ring.repaint() }
            function onStartDegChanged(){ ring.repaint() }
            function onDarkModeChanged(){ ring.repaint() }
        }
        Component.onCompleted: repaint()
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        spacing: 6

        Text {
            text: rootCPUused.title
            color: rootCPUused.cText
            font.pixelSize: 18
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }

        Text {
            text: Math.round(rootCPUused.valuePct).toString() + "%"
            color: rootCPUused.pctColor(rootCPUused.valuePct)
            font.pixelSize: 50
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }

        Text {
            text: rootCPUused.subText
            visible: rootCPUused.subText !== ""
            color: rootCPUused.cMuted
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }

        Text {
            text: rootCPUused.updatedText
            visible: rootCPUused.updatedText !== ""
            color: rootCPUused.cMuted
            font.pixelSize: 11
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }
    }
}
