import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: compass
    width: 900
    height: 900

    // === Properties ===
    property real bearing: 0                // หมุนวงนอกตามแผนที่
    property real innerBearing: 0           // หมุนวงในให้ 0 อยู่ตามเข็มทิศ
    property bool showInnerRing: false      // ✅ ควบคุมการแสดงวงใน
    property color ringColor: "#ffffff"
    property color textColor: "#ff0000"
    property bool showDegreeText: true

    function bearingToDirection(deg) {
        const directions = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
        const index = Math.round(deg / 45) % 8
        return directions[index]
    }

    Text {
        id: bearingLabel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: -30
        font.pixelSize: 24
        font.bold: true
        color: isDarkTheme ? "#ffffff" : "#003344"
        text: bearing.toFixed(1) + "° " + bearingToDirection(bearing)
    }

    Canvas {
        id: compassCanvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            const cx = width / 2
            const cy = height / 2
            const radius = width / 2 - 20
            const scaleFactor = width / 300
            ctx.globalAlpha = 1.0
            // ===== วงนอก =====
            ctx.save()
            ctx.translate(cx, cy)
            ctx.rotate(-bearing * Math.PI / 180)

            for (let deg = 0; deg < 360; deg += 1) {
                const rad = (deg - 90) * Math.PI / 180
                const isMajor = deg % 30 === 0
                const isMedium = deg % 10 === 0

                const tickLength = scaleFactor * (isMajor ? 12 : isMedium ? 6 : 3)
                const lineWidth = isMajor ? 4.5 : (isMedium ? 2.5 : 1.2)

                const x1 = (radius - tickLength) * Math.cos(rad)
                const y1 = (radius - tickLength) * Math.sin(rad)
                const x2 = radius * Math.cos(rad)
                const y2 = radius * Math.sin(rad)

                ctx.strokeStyle = ringColor
                ctx.lineWidth = lineWidth
                ctx.beginPath()
                ctx.moveTo(x1, y1)
                ctx.lineTo(x2, y2)
                ctx.stroke()

                if (isMajor && showDegreeText) {
                    ctx.fillStyle = textColor
                    ctx.font = (7 * scaleFactor) + "px sans-serif"
                    const tx = (radius - 20 * scaleFactor) * Math.cos(rad)
                    const ty = (radius - 20 * scaleFactor) * Math.sin(rad)
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"
                    ctx.fillText(deg.toString(), tx, ty)
                }
            }

            // 🔻 ลูกศรชี้เหนือ (วงนอก)
            ctx.fillStyle = "#ff4444"
            ctx.beginPath()
            ctx.moveTo(0, -radius)
            ctx.lineTo(-6 * scaleFactor, -radius + 16 * scaleFactor)
            ctx.lineTo(6 * scaleFactor, -radius + 16 * scaleFactor)
            ctx.closePath()
            ctx.fill()

            // 🧭 Cardinal directions N/E/S/W
            const cardinal = ["N", "E", "S", "W"]
            const angles = [0, 90, 180, 270]
            ctx.fillStyle = textColor
            ctx.font = (14 * scaleFactor) + "px sans-serif"
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"

            for (let i = 0; i < 4; i++) {
                const rad = (angles[i] - 90) * Math.PI / 180
                const tx = (radius - 30 * scaleFactor) * Math.cos(rad)
                const ty = (radius - 30 * scaleFactor) * Math.sin(rad)
                ctx.fillText(cardinal[i], tx, ty)
            }

            ctx.restore()

            // ===== วงใน (Navigation) =====
            if (showInnerRing) {
                const innerRadius = radius - 40 * scaleFactor

                ctx.save()
                ctx.translate(cx, cy)
                ctx.rotate(-innerBearing * Math.PI / 180)

                for (let deg = 0; deg < 360; deg += 1) {
                    const rad = (deg - 90) * Math.PI / 180
                    const isMajor = deg % 30 === 0
                    const isMedium = deg % 10 === 0

                    const tickLength = scaleFactor * (isMajor ? 10 : isMedium ? 6 : 3)
                    const lineWidth = isMajor ? 1.6 : (isMedium ? 1 : 0.3)

                    const x1 = (innerRadius - tickLength) * Math.cos(rad)
                    const y1 = (innerRadius - tickLength) * Math.sin(rad)
                    const x2 = innerRadius * Math.cos(rad)
                    const y2 = innerRadius * Math.sin(rad)

                    ctx.strokeStyle = ringColor
                    ctx.lineWidth = lineWidth
                    ctx.beginPath()
                    ctx.moveTo(x1, y1)
                    ctx.lineTo(x2, y2)
                    ctx.stroke()

                    if (isMajor && showDegreeText) {
                        ctx.fillStyle = textColor
                        ctx.font = (6 * scaleFactor) + "px sans-serif"
                        const tx = (innerRadius - 18 * scaleFactor) * Math.cos(rad)
                        const ty = (innerRadius - 18 * scaleFactor) * Math.sin(rad)
                        ctx.fillText(deg.toString(), tx, ty)
                    }
                }

                // 🔴 จุดแดงวงใน
                ctx.fillStyle = "#ff4444"
                ctx.beginPath()
                ctx.arc(0, -innerRadius, 5 * scaleFactor, 0, 2 * Math.PI)
                ctx.fill()

                ctx.restore()
            }
        }

        // === รีเฟรช Canvas เมื่อมีการเปลี่ยนแปลง ===
        Connections {
            target: compass
            function onBearingChanged() { compassCanvas.requestPaint() }
            function onInnerBearingChanged() { compassCanvas.requestPaint() }
            function onShowInnerRingChanged() { compassCanvas.requestPaint() }
        }

        Component.onCompleted: requestPaint()
    }
}

