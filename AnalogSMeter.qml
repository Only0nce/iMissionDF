import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: _item
    width: 250
    height: 75
    property bool darkMode: true
    Rectangle {
        id: rectangle
        color: "transparent"
        radius: 0
        border.width: 0
        anchors.fill: parent
        Canvas {
            id: analogSMeter
            anchors.fill: parent
            anchors.topMargin: 3
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            property real smeterBuffered: smeterLevel

            property int tickCount: 10
            property color needleColor: darkMode ? "#FF4444" : "#D14545"
            property color tickColor: darkMode ? "#D3E1E7" : "#284640"
            property color textColor: darkMode ? "#F4FBFF" : "#102824"
            property color labelUnderlayColor: darkMode ? "rgba(0,0,0,0.92)" : "rgba(255,255,255,0.96)"

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
                ctx.font = "bold 11px monospace";
                ctx.fillStyle = tickColor;

                for (let i = 0; i <= tickCount; ++i) {
                    let db = leftDb + (i / tickCount) * rangeDb;
                    let x = 10 + (i / tickCount) * (w - 20);

                    ctx.beginPath();
                    ctx.moveTo(x, h / 2 - 4);
                    ctx.lineTo(x, h / 2 + 4);
                    ctx.stroke();

                    const tickLabel = Math.round(db).toString();
                    ctx.fillStyle = labelUnderlayColor;
                    ctx.fillText(tickLabel, x - 9, h / 2 + 17);
                    ctx.fillStyle = tickColor;
                    ctx.fillText(tickLabel, x - 10, h / 2 + 16);
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
                const valueStr = smeterBuffered.toFixed(1) + " dBm";
                const labelWidth = ctx.measureText(valueStr).width;
                const safeX = Math.max(0, Math.min(w - labelWidth, needleX - labelWidth / 2));
                ctx.fillStyle = labelUnderlayColor;
                ctx.fillText(valueStr, safeX + 1, h / 2 - 13);
                ctx.fillStyle = textColor;
                ctx.fillText(valueStr, safeX, h / 2 - 14);
            }
        }
    }
}
