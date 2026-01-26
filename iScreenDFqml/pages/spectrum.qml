import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.4
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

Item {
    id: spectrumView
    width: 1920
    height: 1080

    property var spectrumX: []
    property var spectrumY: []
    property int maxWaterfallRows: 300
    property var waterfallRows: []

    property real centerFreq: 141.5e6
    property real sampRate: 2e6

    property real waterfallMinDb: -90
    property real waterfallMaxDb: 10
    property int sliderMin: -130
    property int sliderMax: 14

    property var waterfallColors: [3150395, 3216706, 3348554, 3414865, 3481176, 3547487, 3613798, 3679852,
        3746163, 3812473, 3878784, 3945094, 4011403, 4077713, 4078231, 4144540, 4210850, 4211623, 4277932,
        4343985, 4344758, 4411066, 4411839, 4477891, 4478663, 4544971, 4545487, 4546259, 4547031, 4613082,
        4613853, 4614625, 4615140, 4615911, 4616681, 4617196, 4617966, 4618481, 4619251, 4619765, 4620535,
        4621049, 4621818, 4556796, 4557565, 4492542, 4493310, 4428287, 4363519, 4298495, 4168191, 4103167,
        4038398, 3908093, 3843069, 3712763, 3582202, 3517433, 3386871, 3256566, 3126004, 2995698, 2930672,
        2800110, 2669804, 2539242, 2474215, 2343909, 2213346, 2148320, 2083293, 1952987, 1887960, 1822933,
        1757907, 1692880, 1627853, 1628363, 1628872, 1563589, 1564099, 1564608, 1630398, 1630907, 1696697,
        1762743, 1828532, 1960114, 2025903, 2157229, 2288810, 2420135, 2616996, 2748321, 2945182, 3142043,
        3338904, 3535764, 3732625, 3995022, 4191883, 4454279, 4651140, 4913536, 5175933, 5372537, 5634934,
        5897331, 6159471, 6421868, 6684009, 6946405, 7208546, 7470687, 7733084, 7995225, 8257366, 8453971,
        8716112, 8978254, 9174859, 9437001, 9633606, 9895748, 10092354, 10288704, 10485310, 10681661, 10812731,
        11009082, 11205689, 11336504, 11532855, 11663670, 11860021, 12056629, 12187188, 12383540, 12579892,
        12710708, 12907059, 13037619, 13233972, 13364788, 13560884, 13691700, 13822260, 14018613, 14149173,
        14279733, 14410294, 14541110, 14737207, 14867767, 14998328, 15128888, 15193912, 15324473, 15455033,
        15585593, 15650618, 15781178, 15846202, 15976762, 16041786, 16106810, 16237370, 16302394, 16367417,
        16432441, 16431672, 16496696, 16561719, 16561206, 16625973, 16625461, 16690228, 16689459, 16688946,
        16688177, 16752943, 16752174, 16751661, 16750892, 16684587, 16683818, 16683048, 16682279, 16615974,
        16615204, 16548899, 16548130, 16481824, 16481055, 16414750, 16348444, 16282139, 16281370, 16215065,
        16148759, 16082454, 16016149, 15949844, 15883539, 15751953, 15685648, 15619343, 15553294, 15421454,
        15355405, 15289100, 15157515, 15091466, 14959882, 14893577, 14761993, 14630408, 14564359, 14432775,
        14301190, 14169606, 14038021, 13906437, 13775109, 13643524, 13511940, 13380355, 13248771, 13117443,
        12985858, 12788738, 12657410, 12525826, 12328705, 12197377, 12000257, 11803393, 11671809, 11474945,
        11277825, 11146497, 10949633, 10752513, 10555649, 10358785, 10161921, 9964801, 9767937, 9571073,
        9308673, 9111809, 8914945, 8718081, 8455682, 8258818, 7996418]

    property var vfoList: []
    Rectangle {
        anchors.fill: parent
        color: "black"
        z: -1
    }

    Canvas {
        id: spectrumCanvas
        anchors.top: parent.top
        anchors.topMargin: 60
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * 0.35
        z: 0
        property var peakHoldY: []

        onPaint: {
            const ctx = getContext("2d");
            const w = width;
            const h = height;
            ctx.fillStyle = "black";
            ctx.fillRect(0, 0, w, h);

            if (!spectrumY || spectrumY.length < 2 || !spectrumX || spectrumX.length < 2)
                return;

            const minDb = waterfallMinDb;
            const maxDb = waterfallMaxDb;
            const rangeDb = maxDb - minDb;

            if (peakHoldY.length !== spectrumY.length) {
                peakHoldY = spectrumY.slice();
            } else {
                for (let i = 0; i < spectrumY.length; ++i)
                    peakHoldY[i] = Math.max(peakHoldY[i], spectrumY[i]);
            }

            // Draw dB horizontal lines
            ctx.strokeStyle = "#333";
            ctx.fillStyle = "white";
            ctx.lineWidth = 0.5;
            ctx.font = "12px sans-serif";
            for (let db = minDb; db <= maxDb; db += 10) {
                const y = Math.round(h - ((db - minDb) / rangeDb) * h);
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(w, y);
                ctx.stroke();
                ctx.fillText(db.toFixed(0) + " dB", 4, y - 2);
            }

            // Draw frequency vertical lines
            const xStartHz = spectrumX[0];
            const xEndHz = spectrumX[spectrumX.length - 1];
            const hzPerBin = (xEndHz - xStartHz) / (spectrumX.length - 1);
            const stepHz = 500000;
            const startHz = Math.ceil(xStartHz / stepHz) * stepHz;

            ctx.strokeStyle = "#444";
            ctx.lineWidth = 1;
            for (let freqHz = startHz; freqHz <= xEndHz; freqHz += stepHz) {
                const binIndex = Math.round((freqHz - xStartHz) / hzPerBin);
                const x = (binIndex / (spectrumX.length - 1)) * w;
                ctx.beginPath();
                ctx.moveTo(x, 0);
                ctx.lineTo(x, h);
                ctx.stroke();
                ctx.fillText((freqHz / 1e6).toFixed(1) + "M", x + 2, h - 4);
            }

            ctx.save();
            ctx.beginPath();
            ctx.rect(0, 0, w, h);
            ctx.clip();

            // Spectrum (green)
            ctx.beginPath();
            ctx.strokeStyle = "#00FF00";
            ctx.lineWidth = 1;
            for (let i = 0; i < spectrumY.length; ++i) {
                const x = (i / (spectrumY.length - 1)) * w;
                const y = h - ((spectrumY[i] - minDb) / rangeDb) * h;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.stroke();

            // Peak Hold (yellow)
            ctx.beginPath();
            ctx.strokeStyle = "yellow";
            ctx.lineWidth = 1;
            for (let i = 0; i < peakHoldY.length; ++i) {
                const x = (i / (peakHoldY.length - 1)) * w;
                const y = h - ((peakHoldY[i] - minDb) / rangeDb) * h;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.stroke();

            // Peak dot
            let peakIndex = 0;
            let peakValue = spectrumY[0];
            for (let i = 1; i < spectrumY.length; ++i) {
                if (spectrumY[i] > peakValue) {
                    peakValue = spectrumY[i];
                    peakIndex = i;
                }
            }
            const peakX = (peakIndex / (spectrumY.length - 1)) * w;
            const peakY = h - ((peakValue - minDb) / rangeDb) * h;
            ctx.strokeStyle = "red";
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            ctx.arc(peakX, peakY, 4, 0, 2 * Math.PI);
            ctx.fillStyle = "red";
            ctx.fill();
            ctx.fillStyle = "red";
            ctx.font = "bold 12px sans-serif";
            ctx.fillText(`Peak: ${spectrumX[peakIndex].toFixed(1)} Hz / ${peakValue.toFixed(1)} dB`, peakX + 6, peakY - 6);

            // VFO Markers
            for (let i = 0; i < vfoList.length; ++i) {
                const vfo = vfoList[i];
                const freq = vfo.freqHz;
                if (freq == null || freq < xStartHz || freq > xEndHz) continue;

                const x = ((freq - xStartHz) / (xEndHz - xStartHz)) * w;
                const markerWidth = 6;

                ctx.fillStyle = "rgba(255,0,0,0.4)"; // ใช้สีเดียวกันหมด

                ctx.fillRect(x - markerWidth / 2, 0, markerWidth, h);

                // Label
                ctx.fillStyle = "white";
                ctx.font = "bold 11px sans-serif";
                ctx.textAlign = "center";
                ctx.fillText(vfo.name, x, 14);

                // Info
                // ctx.font = "10px sans-serif";
                // if (vfo.angle != null) {
                //     ctx.fillText(`${vfo.angle.toFixed(2)}°`, x, 28);
                // } else if (vfo.squelchDb != null && vfo.squelchFreqs && vfo.squelchFreqs.length > 0) {
                //     const avgFreq = vfo.squelchFreqs.reduce((a, b) => a + b, 0) / vfo.squelchFreqs.length;
                //     ctx.fillText(`${(avgFreq / 1e6).toFixed(3)} MHz / ${vfo.squelchDb.toFixed(1)} dB`, x, 28);
                // }

                // Squelch line
                if (vfo.squelchDb != null) {
                    const y = h - ((vfo.squelchDb - minDb) / rangeDb) * h;
                    ctx.strokeStyle = "#FFA500";
                    ctx.setLineDash([4, 2]);
                    ctx.beginPath();
                    ctx.moveTo(x - 25, y);
                    ctx.lineTo(x + 25, y);
                    ctx.stroke();
                    ctx.setLineDash([]);
                }
            }
            ctx.restore();
        }
    }

    Canvas {
        id: waterfallCanvas
        anchors.top: spectrumCanvas.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 0

        onPaint: {
            const ctx = getContext("2d");
            const w = width;
            const h = height;

            if (!waterfallRows.length || !spectrumX || spectrumX.length < 2)
                return;

            const minDb = waterfallMinDb;
            const maxDb = waterfallMaxDb;
            const rangeDb = maxDb - minDb;

            ctx.drawImage(waterfallCanvas, 0, 0, w, h - 1, 0, 1, w, h - 1);

            const row = waterfallRows[0];
            const colWidth = w / row.length;
            for (let i = 0; i < row.length; i++) {
                const x = Math.round(i * colWidth);
                const dB = Math.max(minDb, Math.min(maxDb, row[i]));
                const norm = (dB - minDb) / rangeDb;
                const colorIndex = Math.floor(norm * (waterfallColors.length - 1));
                const rgb = waterfallColors[colorIndex] || 0;
                const r = (rgb >> 16) & 0xFF;
                const g = (rgb >> 8) & 0xFF;
                const b = rgb & 0xFF;
                ctx.fillStyle = `rgb(${r},${g},${b})`;
                ctx.fillRect(x, 0, Math.ceil(colWidth), 1);
            }
        }
    }

    Column {
        width: 60
        spacing: 8
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 600
        anchors.rightMargin: 30

        Rectangle {
            id: rangeSliderArea
            width: 40
            height: 420
            radius: 10
            color: "transparent"
            border.color: "#444"
            anchors.horizontalCenter: parent.horizontalCenter

            Rectangle {
                id: backgroundBlur
                anchors.fill: parent
                radius: 10
                color: "#20232a88"
                z: -2
            }

            FastBlur {
                anchors.fill: backgroundBlur
                source: backgroundBlur
                radius: 32
                transparentBorder: true
                z: -1
            }

            Rectangle {
                id: rangeFill
                width: 10
                radius: 5
                color: "#00bfff"
                opacity: 0.2
                anchors.horizontalCenter: parent.horizontalCenter
                y: maxKnob.y + maxKnob.height / 2
                height: (minKnob.y + minKnob.height / 2) - (maxKnob.y + maxKnob.height / 2)
            }

            Rectangle {
                id: maxKnob
                width: 36
                height: 22
                radius: 5
                opacity: 0.4
                color: "#111"
                border.color: "#00e5ff"
                border.width: 2
                x: 2
                y: (1 - (waterfallMaxDb - sliderMin) / (sliderMax - sliderMin)) * rangeSliderArea.height - height / 2

                Text {
                    anchors.centerIn: parent
                    text: waterfallMaxDb.toFixed(0)
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    drag.target: parent
                    drag.axis: Drag.YAxis
                    drag.minimumY: 0
                    drag.maximumY: minKnob.y - height
                    onMouseYChanged: {
                        const ratio = 1 - (maxKnob.y + maxKnob.height / 2) / rangeSliderArea.height;
                        const val = sliderMin + ratio * (sliderMax - sliderMin);
                        waterfallMaxDb = Math.min(sliderMax, Math.max(waterfallMinDb + 1, Math.round(val))); // ✅ limit
                        spectrumCanvas.requestPaint();
                        waterfallCanvas.requestPaint();
                    }
                }
            }

            Rectangle {
                id: minKnob
                width: 36
                height: 22
                radius: 5
                opacity: 0.4
                color: "#111"
                border.color: "#ffa500"
                border.width: 2
                x: 2
                y: (1 - (waterfallMinDb - sliderMin) / (sliderMax - sliderMin)) * rangeSliderArea.height - height / 2

                Text {
                    anchors.centerIn: parent
                    text: waterfallMinDb.toFixed(0)
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    drag.target: parent
                    drag.axis: Drag.YAxis
                    drag.minimumY: maxKnob.y + height
                    drag.maximumY: rangeSliderArea.height - height
                    onMouseYChanged: {
                        const ratio = 1 - (minKnob.y + minKnob.height / 2) / rangeSliderArea.height;
                        const val = sliderMin + ratio * (sliderMax - sliderMin);
                        waterfallMinDb = Math.min(waterfallMaxDb - 1, Math.round(val));
                        spectrumCanvas.requestPaint();
                        waterfallCanvas.requestPaint();
                    }
                }
            }
        }

        Text {
            text: waterfallMinDb.toFixed(0) + " → " + waterfallMaxDb.toFixed(0) + " dB"
            color: "#eeeeee"
            opacity: 0.9
            font.pixelSize: 12
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    Button {
        id: clearButton
        text: "Clear Data"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 70
        anchors.rightMargin: 30
        background: Rectangle {
            color: "#00CCCC"
            radius: 5
            opacity: 0.4
        }
        contentItem: Text {
            text: clearButton.text
            font.bold: true
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        onClicked: {
            spectrumX = []
            spectrumY = []
            waterfallRows = []
            spectrumCanvas.peakHoldY = []
            spectrumCanvas.requestPaint()
        }
    }

    Connections {
        target: mainWindow
        function onUpdatespectrumData(name, channel, xData, yData) {
            if (name === "spectrum") {
                spectrumX = xData;
                spectrumY = yData;

                waterfallRows.unshift(yData);
                if (waterfallRows.length > maxWaterfallRows)
                    waterfallRows.pop();

                spectrumCanvas.requestPaint();
                waterfallCanvas.requestPaint();
                return;
            }

            // ---- จัดการ VFO และ VFO Squelch ----
            if (name.startsWith("VFO")) {
                const parts = name.split(" ");
                const vfoName = parts[0];  // เช่น "VFO1"
                const isSquelch = parts.length > 1 && parts[1] === "Squelch";

                let found = false;
                for (let i = 0; i < vfoList.length; ++i) {
                    if (vfoList[i].name === vfoName) {
                        if (isSquelch) {
                            vfoList[i].squelchDb = average(yData);
                            vfoList[i].squelchFreqs = xData;

                            // หาก freqHz ยังไม่ถูกตั้งจาก VFO มาก่อน ลองใช้ค่าเฉลี่ยจาก squelchFreqs
                            if (vfoList[i].freqHz == null && xData && xData.length > 0) {
                                vfoList[i].freqHz = average(xData);
                            }
                        } else {
                            vfoList[i].freqHz = findPeakFreq(xData, yData);
                            vfoList[i].angle = estimateAngleFromPeak(yData);
                        }
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    let newVfo = {
                        name: vfoName,
                        freqHz: isSquelch ? (xData.length > 0 ? average(xData) : null) : findPeakFreq(xData, yData),
                        angle: isSquelch ? null : estimateAngleFromPeak(yData),
                        squelchDb: isSquelch ? average(yData) : null,
                        squelchFreqs: isSquelch ? xData : null
                    };
                    vfoList.push(newVfo);
                }

                // console.log(`qml: VFO Update: ${name} isSquelch: ${isSquelch}`);
                spectrumCanvas.requestPaint();
            }
        }

        function findPeakFreq(x, y) {
            let max = y[0];
            let idx = 0;
            for (let i = 1; i < y.length; ++i) {
                if (y[i] > max) {
                    max = y[i];
                    idx = i;
                }
            }
            return x[idx];
        }

        function estimateAngleFromPeak(y) {
            return Math.random() * 360; // สามารถแก้ให้ใช้จาก backend ได้
        }

        function average(arr) {
            if (!arr || arr.length === 0) return 0;
            return arr.reduce((a, b) => a + b, 0) / arr.length;
        }
    }
    onWaterfallMinDbChanged: {
        if (waterfallMaxDb > sliderMax)
            waterfallMaxDb = sliderMax;
        spectrumCanvas.requestPaint();
        waterfallCanvas.requestPaint();
    }
    onWaterfallMaxDbChanged: {
        spectrumCanvas.requestPaint()
        waterfallCanvas.requestPaint()
    }
}
