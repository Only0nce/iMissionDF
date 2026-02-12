import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: estimations
    width: 1920
    height: 1080

    property var rArray: []
    property var thetaArray: []
    property int headingDeg: 0
    property int offsetX: 200

    property real centerX: width / 2 + offsetX
    property real centerY: height / 2
    property real maxRadius: Math.min(centerX, centerY) * 0.7  // กำหนดขนาดวงกลม

    Rectangle {
        anchors.fill: parent

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#292e49" }
            GradientStop { position: 1.0; color: "#536976" }
        }


        // กริดและตัวเลของศา
        Canvas {
            id: gridCanvas
            anchors.fill: parent

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                const cx = estimations.centerX
                const cy = estimations.centerY
                const radius = estimations.maxRadius

                ctx.strokeStyle = "rgba(255, 255, 255, 0.3)"
                ctx.lineWidth = 2
                for (let r = 0; r <= radius; r += radius / 5) {
                    ctx.beginPath()
                    ctx.arc(cx, cy, r, 0, 2 * Math.PI)
                    ctx.stroke()
                }

                const numLines = 8
                for (let i = 0; i < numLines; i++) {
                    const angle = ((i * 360 / numLines - headingDeg) * Math.PI / 180) - Math.PI / 2
                    const x = cx + radius * Math.cos(angle)
                    const y = cy + radius * Math.sin(angle)

                    ctx.beginPath()
                    ctx.moveTo(cx, cy)
                    ctx.lineTo(x, y)
                    ctx.stroke()
                }

                const degreeMarkers = [0, 45, 90, 135, 180, 225, 270, 315]
                ctx.fillStyle = "#ffffff"
                ctx.font = "bold 14px sans-serif"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                for (let deg of degreeMarkers) {
                    const rotatedDeg = deg - headingDeg
                    const angle = rotatedDeg * Math.PI / 180 - Math.PI / 2
                    const x = cx + (radius + 20) * Math.cos(angle)
                    const y = cy + (radius + 20) * Math.sin(angle)
                    ctx.fillText(`${deg}°`, x, y)
                }
            }

            Component.onCompleted: requestPaint()
        }

        // วาดข้อมูล DOA
        Canvas {
            id: dataCanvas
            anchors.fill: parent

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                const cx = estimations.centerX
                const cy = estimations.centerY
                const radius = estimations.maxRadius

                if (rArray.length > 0 && thetaArray.length === rArray.length) {
                    const minR = Math.min(...rArray)
                    const maxR = Math.max(...rArray)
                    const normalized = rArray.map(r => (r - minR) / (maxR - minR || 1))

                    ctx.fillStyle = "rgba(0, 100, 255, 0.3)"
                    ctx.strokeStyle = "rgba(0, 150, 255, 0.9)"
                    ctx.lineWidth = 2
                    ctx.beginPath()
                    for (let i = 0; i < normalized.length; i++) {
                        const rotated = (thetaArray[i] - headingDeg + 360) % 360
                        const angle = rotated * Math.PI / 180 - Math.PI / 2
                        const x = cx + normalized[i] * radius * Math.cos(angle)
                        const y = cy + normalized[i] * radius * Math.sin(angle)
                        if (i === 0)
                            ctx.moveTo(x, y)
                        else
                            ctx.lineTo(x, y)
                    }
                    ctx.closePath()
                    ctx.fill()
                    ctx.stroke()
                }
            }
        }
    }

    // รับข้อมูลจาก Krakenmapval
    Connections {
        target: Krakenmapval

        function onDoaChanged() {
            rArray = Krakenmapval.doaRArray
            thetaArray = Krakenmapval.doaThetaArray
            dataCanvas.requestPaint()
        }

        function onDegreeChanged() {
            headingDeg = Krakenmapval.degree
            gridCanvas.requestPaint()
            dataCanvas.requestPaint()
        }
    }
}
