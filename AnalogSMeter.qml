import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: _item
    width: 250
    height: 75
    Rectangle {
        id: rectangle
        color: "#A0000000"
        radius: 5
        anchors.fill: parent
        Canvas {
            id: analogSMeter
            anchors.fill: parent
            anchors.topMargin: 3
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            property real smeterBuffered: smeterLevel

            property int tickCount: 10
            property color needleColor: "#FF4444"
            property color tickColor: "#AAAAAA"
            property color textColor: "#FFFFFF"

            Timer {
                interval: 100
                running: true
                repeat: true
                onTriggered: {
                    smeterBuffered = smeterLevel
                    analogSMeter.requestPaint();
                }
            }

            onPaint: {
                const ctx = getContext("2d");
                const w = width;
                const h = height;
                ctx.clearRect(0, 0, w, h);

                const leftDb = waterfallMinDb;
                const rightDb = waterfallMaxDb;
                const rangeDb = rightDb - leftDb;

                // --- Background line ---
                ctx.strokeStyle = tickColor;
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.moveTo(10, h / 2);
                ctx.lineTo(w - 10, h / 2);
                ctx.stroke();

                // --- Tick marks ---
                ctx.font = "10px monospace";
                ctx.fillStyle = tickColor;

                for (let i = 0; i <= tickCount; ++i) {
                    let db = leftDb + (i / tickCount) * rangeDb;
                    let x = 10 + (i / tickCount) * (w - 20);

                    ctx.beginPath();
                    ctx.moveTo(x, h / 2 - 4);
                    ctx.lineTo(x, h / 2 + 4);
                    ctx.stroke();

                    ctx.fillText(Math.round(db), x - 10, h / 2 + 16);
                }

                // --- Needle ---
                const norm = Math.max(0, Math.min(1, (smeterBuffered - leftDb) / rangeDb));
                const needleX = 10 + norm * (w - 20);

                ctx.strokeStyle = needleColor;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(needleX, h / 2 - 10);
                ctx.lineTo(needleX, h / 2 + 10);
                ctx.stroke();

                // --- Value Label ---
                ctx.fillStyle = textColor;
                const valueStr = smeterBuffered.toFixed(1) + " dBm";
                const labelWidth = ctx.measureText(valueStr).width;
                const safeX = Math.max(0, Math.min(w - labelWidth, needleX - labelWidth / 2));
                ctx.fillText(valueStr, safeX, h / 2 - 14);
            }
        }
    }
}
