// ===== RAMused.qml =====
// Phase 6 HMI: presentation-only migration. Input contract is unchanged.
import QtQuick 2.12
import "../ui"

Item {
    id: rootRAMused
    width: 400
    height: 400

    property bool darkMode: true
    property real valuePct: 0
    property real usedMB: 0
    property real totalMB: 0
    property string title: "RAM"
    property string updatedText: ""
    property real startDeg: 90

    Theme { id: hmiTheme; darkMode: rootRAMused.darkMode }

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
    function fmt1(v){ return Number(v).toFixed(1) }
    function autoSubText(){
        if (totalMB <= 0) return ""
        return fmt1(usedMB/1024.0) + " of " + fmt1(totalMB/1024.0) + " GiB used"
    }

    Rectangle {
        anchors.fill: parent
        radius: hmiTheme.radiusLg
        color: cCard
        border.width: 1
        border.color: cBorder
        clip: true
    }

    Canvas {
        id: ring
        anchors.fill: parent
        anchors.bottomMargin: Math.max(104, rootRAMused.height * 0.29)
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            if (!ctx) return
            ctx.reset()
            var cx = width * 0.5
            var cy = height * 0.44
            var r = Math.min(width, height) * 0.31
            var thick = Math.max(14, r * 0.18)
            var p = clamp(valuePct, 0, 100)
            var t = p / 100.0
            var startA = startDeg * Math.PI / 180
            var endA = startA + Math.PI * 2 * t

            ctx.lineWidth = thick
            ctx.strokeStyle = cTrack
            ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI*2); ctx.stroke()

            var pc = pctColor(p)
            ctx.strokeStyle = pc
            ctx.beginPath(); ctx.arc(cx, cy, r, startA, endA); ctx.stroke()

            if (t > 0.001) {
                var kx = cx + Math.cos(endA) * r
                var ky = cy + Math.sin(endA) * r
                var kr = thick * 0.28
                ctx.fillStyle = cCard
                ctx.beginPath(); ctx.arc(kx, ky, kr + 3, 0, Math.PI*2); ctx.fill()
                ctx.fillStyle = pc
                ctx.beginPath(); ctx.arc(kx, ky, kr, 0, Math.PI*2); ctx.fill()
            }
        }

        Connections {
            target: rootRAMused
            function onValuePctChanged(){ ring.requestPaint() }
            function onWidthChanged(){ ring.requestPaint() }
            function onHeightChanged(){ ring.requestPaint() }
            function onDarkModeChanged(){ ring.requestPaint() }
        }
        Component.onCompleted: requestPaint()
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        spacing: 6

        Text { text: title; color: cText; font.pixelSize: 18; font.bold: true; horizontalAlignment: Text.AlignHCenter; width: parent.width }
        Text { text: fmt1(valuePct) + "%"; color: pctColor(valuePct); font.pixelSize: 50; font.bold: true; horizontalAlignment: Text.AlignHCenter; width: parent.width }
        Text { text: autoSubText(); color: cMuted; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; width: parent.width }
        Text { text: updatedText; color: cMuted; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; width: parent.width; visible: updatedText !== "" }
    }
}
