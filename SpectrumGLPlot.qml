// SpectrumGLPlot.qml  (FULL FILE)
// ✅ Fix4: Cached grid (works on x86 + Jetson)
// ✅ Fix CPU: remove paint-loop, add 30fps throttle
// ✅ Fix labels: X axis labels on TOP (no overlap)
// ✅ IMPORTANT: This file MUST NOT instantiate SpectrumGLPlot inside itself (prevents recursive instantiation)

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.4
import QtQuick.Layouts 1.0
import "ui"

Item {
    id: root
    anchors.fill: parent

    property real priStart: 0
    property real priStop: 0

    property real plotWidth: root.width
    property real dataCount : 0
    property real smeterLevel: -100
    property var  spectrumData: []
    property var  waterfallBuffer: []
    property var  waterfallColorMap: []
    property real smeterBuffered: smeterLevel

    signal spectrumUpdated(var spectrum)
    signal waterfallUpdated(var line)
    signal waterfallColorUpdate(var colors)

    property var  maxHoldKept: []
    property var  peakVal: []
    property bool rfScannerInterlock: false

    property real centerFreq: mainWindows.center_freq()    // Hz
    property int  sampRate:   mainWindows.samp_rate()      // Hz

    property real waterfallMinDb: -100
    property real waterfallMaxDb: 0
    property real waterfallMax: 0
    property real waterfallMin: -100
    property alias autoScaleTimer: autoScaleTimer

    property real offsetFrequency: 0      // Hz
    property int  bandwidth: high_cut - low_cut
    property int  low_cut:  bwModel.get(scanBwSelected).low_cut
    property int  high_cut: bwModel.get(scanBwSelected).high_cut
    property int  offsetSnapStep: 100     // Hz

    property bool autoScaleEnabled: false

    property int  sampRateMin: 50000
    property int  sampRateMax: 24.576e6
    property real zoomStep: root.width
    property string start_mod: ""
    property real xPos: 0
    property real setCenterFreq: centerFreq
    property bool initFreq: false

    property int offsetStart: -1600000
    property int offsetStop:   1600000
    property int offsetStep:   1000
    property int dwellMs:      500
    property int currentOffset: offsetStart
    property bool scanning: false

    property int keptStart: 0
    property int profileViewIndex: 0

    Theme { id: theme }

    // ============================================================
    // ✅ FIX CPU: throttle spectrum paint (30fps)
    // ============================================================
    Timer {
        id: spectrumPaintTimer
        interval: 33
        repeat: false
        onTriggered: spectrumCanvas.requestPaint()
    }

    function scheduleSpectrumPaint() {
        if (!spectrumPaintTimer.running)
            spectrumPaintTimer.start()
    }

    // ===== Timer ที่ใช้แทน for loop =====
    Timer {
        id: scanTimer
        interval: 5
        repeat: true
        running: false
        onTriggered: {
            offsetFrequency = currentOffset
            currentOffset += offsetStep
            if (currentOffset > offsetStop) stopScan()
        }
    }

    function startScan() {
        if (scanning) return
        currentOffset = offsetStart
        scanning = true
        scanTimer.start()
    }

    function stopScan() {
        scanning = false
        scanTimer.stop()
    }

    onBandwidthChanged: {
        console.log("Bandwidth:", bandwidth, low_cut, high_cut)
    }

    onStart_modChanged: {
        let idx = getReceiverIndex(start_mod);
        if (idx !== -1) scanReceiverModeSelected = idx;
        currentModIndex = scanReceiverModeSelected
    }

    onOffsetFrequencyChanged: {
        if (((centerFreq + offsetFrequency) > (centerFreq + (sampRate / 2))) ||
            ((centerFreq + offsetFrequency) < (centerFreq - (sampRate / 2)))) {
            centerFreq = centerFreq + offsetFrequency
            offsetFrequency = 0
            mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":' + centerFreq + ',"key":"memagic"}}')
            updateFrequency()
        } else {
            mainWindows.sendmessage('{"type": "dspcontrol","params": {"offset_freq": ' + offsetFrequency + '}}')
            freqScan = centerFreq + offsetFrequency
            updateFrequency()
        }
        mainWindows.updateCurrentOffsetFreq(offsetFrequency, centerFreq)
    }

    onCenterFreqChanged: {
        spectrumCanvas.clearPeakTimer.start()
        if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
    }

    onSetCenterFreqChanged: {
        if (setCenterFreq !== centerFreq) {
            mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":' + setCenterFreq + ',"key":"memagic"}}')
        }
        spectrumCanvas.clearPeakTimer.start()
        if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
    }

    onSampRateChanged: {
        scheduleSpectrumPaint()
        waterfallCanvas.requestPaint()
        overlayCanvas.requestPaint()
        if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
    }

    onWaterfallMinDbChanged: { if (spectrumGridCanvas) spectrumGridCanvas.invalidate() }
    onWaterfallMaxDbChanged: { if (spectrumGridCanvas) spectrumGridCanvas.invalidate() }

    Component.onCompleted: {
        mainWindows.updateCenterFreq.connect(updateCenterFreq)

        mainWindows.spectrumUpdated.connect(spectrumUpdated)
        mainWindows.waterfallUpdated.connect(waterfallUpdated)
        mainWindows.waterfallColorUpdate.connect(waterfallColorUpdate)

        mainWindows.findBandsWithProfile.connect(findBandsWithProfile)

        mainWindows.smeterValueUpdated.connect(function(smeter) {
            smeterValueUpdated(smeter)
        })

        if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
    }

    function setOffset(freqOffset) {
        offsetFrequency = freqOffset;
    }

    function setManualOffset(freq) {
        let freqOffset = freq - centerFreq
        offsetFrequency = freqOffset;
        overlayCanvas.requestPaint()
        scheduleSpectrumPaint()
        waterfallCanvas.requestPaint()
    }

    function zoomIn() {
        plotWidth += zoomStep
        if (plotWidth > root.width * 20) plotWidth = root.width * 20
        applyZoom()
    }

    function zoomOut() {
        plotWidth -= zoomStep
        if (plotWidth < root.width) plotWidth = root.width
        applyZoom()
    }

    function applyZoom() {
        scheduleSpectrumPaint()
        waterfallCanvas.requestPaint()
        overlayCanvas.requestPaint()
        zoomNav.rectangle.opacity = 1
        if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
    }

    function smeterValueUpdated(smeter) { smeterLevel = smeter }

    function updateWaterfallLevels(minDb, maxDb) {
        waterfallMinDb = minDb;
        waterfallMaxDb = maxDb;
        waterfallScaleControl.waterfallMinDb = waterfallMinDb
        waterfallScaleControl.waterfallMaxDb = waterfallMaxDb
    }

    function updateCenterFreq() {
        sampRate = mainWindows.samp_rate()
        centerFreq = mainWindows.center_freq()
        sampRateMax = mainWindows.samp_rate()
        start_mod = mainWindows.start_mod()
        freqScan = centerFreq
        updateFrequency()
        resetCenterFreqTimer.start()
        if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
    }

    function autoScaleWaterfallColor() {
        if (waterfallBuffer.length === 0) return;
        var latestLine = waterfallBuffer[waterfallBuffer.length - 1];
        if (!latestLine || latestLine.length < 2) return;

        var minVal = latestLine[0];
        var maxVal = latestLine[0];
        for (var i = 1; i < latestLine.length; ++i) {
            var v = latestLine[i];
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
        }

        waterfallMin = Math.max(minVal - 5, -120);
        waterfallMax = Math.min(maxVal + 5, 0);

        waterfallMinDb = waterfallMin < waterfallMinDb ? waterfallMin : waterfallMinDb
        waterfallMaxDb = waterfallMax > waterfallMaxDb ? waterfallMax : waterfallMaxDb

        if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
    }

    function resetCenterFreq() {
        if (initFreq === false) {
            if (centerFreq !== 0) {
                let centerFrequency = centerFreq;
                mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":' + 30000000 + ',"key":"memagic"}}')
                mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":' + centerFrequency + ',"key":"memagic"}}')
                mainWindows.sendmessage('{"type": "dspcontrol","params": {"offset_freq": ' + offsetFrequency + '}}')
                initFreq = true
            }
        }
    }

    Timer {
        id: resetCenterFreqTimer
        interval: 1000
        repeat: false
        running: false
        onTriggered: resetCenterFreq()
    }

    Timer {
        id: autoScaleTimer
        interval: 500
        repeat: true
        running: autoScaleEnabled
        onTriggered: autoScaleWaterfallColor()
    }

    /* ============================================================
       ✅ Fix4: Cached grid canvas (X axis on TOP, works x86+Jetson)
       - DO NOT set visible:false (x86 often won't render => drawImage blank)
       ============================================================ */
    Canvas {
        id: spectrumGridCanvas
        width: spectrumCanvas.width
        height: spectrumCanvas.height

        visible: true
        opacity: 0.0
        z: -1000

        renderTarget: Canvas.FramebufferObject
        renderStrategy: Canvas.Immediate
        antialiasing: false
        smooth: false

        property bool ready: false

        function invalidate() {
            ready = false
            requestPaint()
        }

        onWidthChanged:  invalidate()
        onHeightChanged: invalidate()

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            ctx.clearRect(0, 0, w, h)
            if (w < 2 || h < 2) return

            const minDb = root.waterfallMinDb
            const maxDb = root.waterfallMaxDb
            const rangeDb = Math.max(1e-6, (maxDb - minDb))

            // ===== Y grid + dB labels =====
            ctx.strokeStyle = theme.gridLine
            ctx.lineWidth = 1
            ctx.font = "11px monospace"
            ctx.fillStyle = theme.axisText

            for (var db = minDb; db <= maxDb; db += 10) {
                let y = h - ((db - minDb) / rangeDb) * h
                ctx.beginPath()
                ctx.moveTo(0, y)
                ctx.lineTo(w, y)
                ctx.stroke()
                ctx.fillText(db.toFixed(0) + " dBm ", 4, y - 2)
            }

            // ===== X axis (TOP) + freq labels =====
            const xAxisH   = 18
            const yAxisBot = xAxisH

            let startFreq = root.centerFreq - root.sampRate / 2
            let stopFreq  = root.centerFreq + root.sampRate / 2
            let freqRange = Math.max(1, (stopFreq - startFreq))

            // baseline (top axis line)
            ctx.strokeStyle = theme.gridLine
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(0, yAxisBot)
            ctx.lineTo(w, yAxisBot)
            ctx.stroke()

            // label step by pixel spacing
            ctx.font = "11px monospace"
            let pixelsPerHz = w / Math.max(1, root.sampRate)
            let minLabelSpacingPx = 160
            let rawStep = minLabelSpacingPx / Math.max(1e-12, pixelsPerHz)
            let pow10 = Math.pow(10, Math.floor(Math.log10(rawStep)))
            let freqStep = pow10
            if (rawStep / pow10 >= 5) freqStep = 5 * pow10
            else if (rawStep / pow10 >= 2) freqStep = 2 * pow10

            ctx.strokeStyle = theme.gridLine
            ctx.fillStyle = theme.axisText
            ctx.lineWidth = 1

            // ✅ anti-overlap: purely by X spacing
            let lastX = -1e9

            for (var f = Math.ceil(startFreq / freqStep) * freqStep; f <= stopFreq; f += freqStep) {
                let x = ((f - startFreq) / freqRange) * w
                if (!isFinite(x)) continue
                if (x < 0) x = 0
                if (x > w) x = w

                // vertical grid line (start below top axis so it won't touch label)
                ctx.beginPath()
                ctx.moveTo(x, yAxisBot)
                ctx.lineTo(x, h)
                ctx.stroke()

                // tick
                ctx.beginPath()
                ctx.moveTo(x, yAxisBot)
                ctx.lineTo(x, yAxisBot + 6)
                ctx.stroke()

                // label
                if (x - lastX < minLabelSpacingPx) continue
                lastX = x

                let label = (f / 1e6).toFixed(2) + " MHz"
                let tw = ctx.measureText(label).width
                if (!isFinite(tw) || tw <= 0) tw = label.length * 7

                let lx = x - tw / 2
                if (lx < 0) lx = 0
                if (lx > w - tw) lx = w - tw

                ctx.fillText(label, lx, 12)
            }

            ready = true

            // ✅ IMPORTANT: grid finished -> paint spectrum once (no loop)
            root.scheduleSpectrumPaint()
        }
    }

    Canvas {
        id: spectrumCanvas
        width: plotWidth
        height: parent.height / 4
        renderTarget: Canvas.FramebufferObject
        renderStrategy: Canvas.Cooperative
        antialiasing: false
        smooth: false
        x: plotWidth > root.width ? zoomNav.rectangle.x * (plotWidth / root.width) * (-1) : 0

        property alias clearPeakTimer: clearPeakTimer
        property var maxHold: []
        property bool showMaxHold: true

        function clearPeaks() {
            maxHold = []
            maxHoldKept = []
        }

        Timer {
            id: clearPeakTimer
            interval: 1000
            repeat: false
            onTriggered: spectrumCanvas.clearPeaks()
        }

        onWidthChanged:  { if (spectrumGridCanvas) spectrumGridCanvas.invalidate() }
        onHeightChanged: { if (spectrumGridCanvas) spectrumGridCanvas.invalidate() }

        onPaint: {
            var ctx = getContext("2d");
            var w = width;
            var h = height;

            ctx.clearRect(0, 0, w, h);

            if (!spectrumData || spectrumData.length < 2)
                return;

            // ✅ draw cached grid ONLY if ready (no requestPaint here -> avoids CPU loop)
            if (spectrumGridCanvas && spectrumGridCanvas.ready) {
                ctx.drawImage(spectrumGridCanvas, 0, 0, w, h)
            }

            const minDb = root.waterfallMinDb;
            const maxDb = root.waterfallMaxDb;
            const rangeDb = Math.max(1e-6, (maxDb - minDb));

            const xAxisH = 18
            const plotH = h - xAxisH

            function yOf(v) { return plotH - ((v - minDb) / rangeDb) * plotH; }

            // Spectrum line
            ctx.beginPath();
            ctx.strokeStyle = "#00FF00";
            ctx.lineWidth = 1;
            ctx.moveTo(0, yOf(spectrumData[0]));

            // (optional) speed: sample step by pixel width
            let step = Math.max(1, Math.floor(spectrumData.length / Math.max(1, w)))
            for (var i = step; i < spectrumData.length; i += step) {
                let x = i / (spectrumData.length - 1) * w;
                let y = yOf(spectrumData[i]);
                ctx.lineTo(x, y);
            }
            ctx.stroke();

            // Max hold
            for (var j = 0; j < spectrumData.length; ++j) {
                if (maxHold.length < spectrumData.length)
                    maxHold.push(spectrumData[j]);
                else if (spectrumData[j] > maxHold[j])
                    maxHold[j] = spectrumData[j];
            }
            maxHoldKept = maxHold

            if (showMaxHold && maxHold.length === spectrumData.length) {
                ctx.beginPath();
                ctx.strokeStyle = theme.maxHoldLine;
                ctx.lineWidth = 1;
                ctx.moveTo(0, yOf(maxHold[0]));

                let step2 = Math.max(1, Math.floor(maxHold.length / Math.max(1, w)))
                for (var k = step2; k < maxHold.length; k += step2) {
                    let x2 = k / (maxHold.length - 1) * w;
                    let y2 = yOf(maxHold[k]);
                    ctx.lineTo(x2, y2);
                }
                ctx.stroke();
            }
        }

        Connections {
            target: root
            function onSpectrumUpdated(spectrum) {
                spectrumData = spectrum
                root.scheduleSpectrumPaint()   // ✅ throttle (fix CPU)
            }
        }
    }

    Canvas {
        id: waterfallCanvas
        y: spectrumCanvas.height
        x: spectrumCanvas.x
        width: spectrumCanvas.width
        height: parent.height / 4

        onPaint: {
            const minDb = waterfallMinDb;
            const maxDb = waterfallMaxDb;
            const rangeDb = maxDb - minDb;
            var ctx = getContext("2d");

            // Scroll down by 1px
            ctx.drawImage(waterfallCanvas, 0, 0, width, height - 1, 0, 1, width, height - 1);

            var line = waterfallBuffer[waterfallBuffer.length - 1];
            if (!line || typeof line.length === "undefined") return;

            const canvasWidth = width;
            const bins = line.length;

            for (let i = 0; i < bins; ++i) {
                const x = Math.floor(i / (bins - 1) * canvasWidth);

                const dB = Math.max(minDb, Math.min(maxDb, line[i]));
                const norm = (dB - minDb) / Math.max(1e-9, rangeDb);
                const colorIndex = Math.floor(norm * (waterfallColorMap.length - 1));

                const rgb = waterfallColorMap[colorIndex] || 0;
                const r = (rgb >> 16) & 0xFF;
                const g = (rgb >> 8) & 0xFF;
                const b = rgb & 0xFF;

                ctx.fillStyle = `rgb(${r},${g},${b})`;
                ctx.fillRect(x, 0, 1, 1);
            }
        }

        Connections {
            target: root
            function onWaterfallColorUpdate(colors) { waterfallColorMap = colors }
        }

        Connections {
            target: root
            function onWaterfallUpdated(line) {
                if (waterfallBuffer.length >= waterfallCanvas.height)
                    waterfallBuffer.shift();
                waterfallBuffer.push(line);
                waterfallCanvas.requestPaint();
            }
        }
    }

    Canvas {
        id: overlayCanvas
        z: 10
        antialiasing: false

        anchors.left:   spectrumCanvas.left
        anchors.right:  spectrumCanvas.right
        anchors.top:    spectrumCanvas.top
        anchors.bottom: waterfallCanvas.bottom

        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            let ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            let startFreq = centerFreq - sampRate / 2;
            let freqRange = sampRate;
            let canvasWidth = width;
            let canvasHeight = height;

            let offsetFreqAbs = centerFreq + offsetFrequency;
            let freqLeft = offsetFreqAbs + low_cut;
            let freqRight = offsetFreqAbs + high_cut;

            let x1 = ((freqLeft - startFreq) / freqRange) * canvasWidth;
            let x2 = ((freqRight - startFreq) / freqRange) * canvasWidth;
            let xCenter = ((offsetFreqAbs - startFreq) / freqRange) * canvasWidth;

            ctx.fillStyle = theme.selectionFillCss;
            ctx.fillRect(x1, 0, x2 - x1, canvasHeight);

            ctx.strokeStyle = theme.cursorLine;
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(xCenter, 0);
            ctx.lineTo(xCenter, canvasHeight);
            ctx.stroke();

            xPos = xCenter;
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            property bool dragging: false
            property real dragX: 0
            property real leftMargin: 0

            onPressed: {
                let x = mouse.x;
                let canvasWidth = overlayCanvas.width;
                let startFreq = centerFreq - sampRate / 2;
                let freqRange = sampRate;

                let offsetFreqAbs = centerFreq + offsetFrequency;
                let targetX = ((offsetFreqAbs - startFreq) / freqRange) * canvasWidth;

                if (Math.abs(x - targetX) < 10) {
                    dragging = true;
                    dragX = x;
                } else {
                    let clickedFreq = startFreq + ((x - leftMargin) / (canvasWidth - leftMargin)) * freqRange;
                    let newOffset = clickedFreq - centerFreq;

                    newOffset = Math.round(newOffset / offsetSnapStep) * offsetSnapStep;
                    offsetFrequency = Math.max(-sampRate / 2, Math.min(sampRate / 2, newOffset));

                    overlayCanvas.requestPaint();
                    root.scheduleSpectrumPaint()
                    waterfallCanvas.requestPaint();

                    if (mainWindows.setOffsetFrequency)
                        mainWindows.setOffsetFrequency(Math.round(offsetFrequency));
                }
            }

            onReleased: dragging = false

            onPositionChanged: {
                if (!dragging) return;

                let x = mouse.x;
                let deltaX = x - dragX;
                dragX = x;

                let deltaFreq = (deltaX / overlayCanvas.width) * sampRate;
                let newOffset = offsetFrequency + deltaFreq;

                newOffset = Math.round(newOffset / offsetSnapStep) * offsetSnapStep;
                offsetFrequency = Math.max(-sampRate / 2, Math.min(sampRate / 2, newOffset));

                overlayCanvas.requestPaint();
                root.scheduleSpectrumPaint()
                waterfallCanvas.requestPaint();

                if (mainWindows.setOffsetFrequency)
                    mainWindows.setOffsetFrequency(Math.round(offsetFrequency));
            }
        }

        Connections {
            target: root
            function onSpectrumUpdated(_)  { overlayCanvas.requestPaint() }
            function onWaterfallUpdated(_) { overlayCanvas.requestPaint() }
        }
    }

    Item {
        id: zoomNav
        x: 0
        y: 113
        height: 20
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z:97
        visible: spectrumCanvas.width > root.width
        property alias rectangle: rectangle
        Rectangle {
            id: rectangle
            width: root.width*(root.width/spectrumCanvas.width)
            color: theme.zoomViewportCss
            radius: 2
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 0

            Behavior on opacity {
                NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
            }

            MouseArea {
                id: mouseArea1
                anchors.fill: parent
                drag.target: parent
                drag.axis: Drag.XAxis
                drag.minimumX: 0
                drag.maximumX: zoomNav.width - parent.width

                onReleased: {
                    // Optional: calculate new offsetFrequency or view based on parent.x
                    const ratio = parent.x / (zoomNav.width - parent.width);
                    console.log("Slider moved to %:", Math.round(ratio * 100));
                    // You could adjust offsetFrequency or trigger repaint here
                }
                onClicked:
                    zoomNavTimer.restart()

            }
            onXChanged: {
                zoomNavTimer.restart()
                rectangle.opacity = 1
            }

        }
        Timer {
            id: zoomNavTimer
            repeat: false
            running: true
            interval: 10000
            onTriggered: {
                rectangle.opacity = 0.5
            }
            onRunningChanged: {
                if (running)
                    rectangle.opacity = 1
            }
        }
    }

    Zoom {
        id:zoom
        x: 1210
        width: 80
        height: 180
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 10
        anchors.topMargin: 40
        property int btnSize: 60   // <<< ขนาดปุ่มใหม่

        buttonIn.width: btnSize
        buttonIn.height: btnSize

        buttonOut.width: btnSize
        buttonOut.height: btnSize

        buttonReset.width: btnSize
        buttonReset.height: btnSize

        buttonClear.width: btnSize
        buttonClear.height: btnSize
        z:96
        opacity: zoomTimer.running ? 1 : 0.2

        Behavior on opacity {
            NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
        }

        // ===== พื้นหลังโปร่งบาง =====
        Rectangle {
            id: bg
            anchors.fill: parent
            radius: 12
            color: "#000000"
            opacity: 0.18           // <<< ปรับตรงนี้ (0.12–0.25 กำลังสวย)
            z: -1                   // อยู่หลังปุ่ม
        }

        Timer {
            id: zoomTimer
            repeat: false
            running: true
            interval: 10000
            onTriggered: {
                zoom.opacity = 0.2
            }
            onRunningChanged: {
                if(running)
                    zoom.opacity = 1
            }
        }
        buttonIn.visible: plotWidth/root.width <= 10
        buttonClear.visible: plotWidth/root.width <= 10
        buttonOut.opacity: plotWidth > root.width ? 1 : 0.2
        // buttonOut.enabled: plotWidth > root.width ? true : false
        buttonReset.opacity: plotWidth > root.width ? 1 : 0.2
        // buttonReset.enabled: plotWidth > root.width ? true : false

        buttonIn.onClicked: {
            zoomTimer.restart()
            zoomIn()
        }
        buttonOut.onClicked: {
            zoomTimer.restart()
            zoomOut()
        }
        buttonReset.onClicked: {
            zoomTimer.restart()
            plotWidth = root.width
        }
        buttonClear.onClicked: {
            zoomTimer.restart()
            spectrumCanvas.clearPeaks()
        }

    }

    RowLayout {
        x: 10
        y: 230
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 10
        anchors.bottomMargin: 25
        AnalogSMeter {
            id: smeterOverlay
            Layout.preferredWidth: 300
            Layout.preferredHeight: 60
        }

        WaterfallScaleControl {
            id: waterfallScaleControl
            z: 99
            waterfallMinDb: root.waterfallMinDb
            waterfallMaxDb: root.waterfallMaxDb
            onWaterfallMinDbChanged: {
                root.waterfallMinDb = waterfallMinDb
            }
            onWaterfallMaxDbChanged: {
                root.waterfallMaxDb = waterfallMaxDb
            }
            Layout.preferredWidth: 300
            Layout.preferredHeight: 75
        }
    }

    RowLayout {
        y: 380
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.bottomMargin: 25
        BandwidthScaleControl {
            id: bandwidthScaleControl
            visible: receiverMode.get(scanReceiverModeSelected).mode === "Analog"
            z: 99
            // bwRangeSlider.to: receiverMode.get(scanReceiverModeSelected).name === "WFM" ? 250e3 : 50e3
            // bwRangeSlider.from: receiverMode.get(scanReceiverModeSelected).name === "WFM" ? -250e3 : -50e3
            Layout.preferredWidth: 300
            Layout.preferredHeight: 75
        }

        CheckBox {
            text: "Show Max Hold"
            Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
            onCheckedChanged: {
                // spectrumCanvas.clearPeaks()
                spectrumCanvas.showMaxHold = checked
            }
            checked: spectrumCanvas.showMaxHold
        }
    }

    /* === จุดยึดกลาง: ขนาด “เต็มกรอบ” ตามที่ต้องการ === */
    Item {
        id: memorySlot
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: parent.height / 1.9      // <<< ใช้ตำแหน่งเดียวกับของเดิม
        height: parent.height / 2
        z: 50
    }

    NewMemoryAddEdit {
        id: newMemoryAddEdit
        anchors.fill: memorySlot
        visible: !widgetView
        z: 51
        radioMemLists: radioMemList    // ✅ ส่ง ListModel id: radioMemList เข้าไป
    }

    LogDeviceScanner {
        id: logDeviceScanner
        visible: widgetView
        anchors.fill: memorySlot
        z: 52
    }

    /* ============================================================
       Peak-scan Engine v2 (Per-Window Multi-Mode)
       - ทำครบทุกโหมด (wide/narrow/อื่นๆ) ต่อ "หน้าต่าง" เดียวกัน
       - ใช้ snapshot max-hold เดียว แล้ววิเคราะห์ทุกรูปแบบ
       - ต่อคิวได้ (หลายช่วง/หลายเซตโหมด), ไม่ต้องหยุดก่อน
       - ใช้ตัวแปรเดิมของคุณ: centerFreq, sampRate, mainWindows, profileCards, maxHoldKept
       ============================================================ */

    Item {
        id: peakScan

        /* ===== CONFIG / RUNTIME PROPS ===== */
        property bool   running: false
        property real   startHz: 0
        property real   stopHz: 0
        property real   spanHz: 0
        property real   stepHz: 0
        property int    windowIndex: 0
        property int    totalWindows: 0
        property real   overlap: 0.15     // 0..0.9
        property int    dwellMs: 900
        property int    settleMs: 250

        // โหมดหลายค่า/เรียงลำดับ (เช่น ["wide","narrow"])
        property var    modes: ["wide"]   // จะตั้งใหม่จาก msg
        // เก็บผลรวมข้ามหน้าต่าง (กันซ้ำ)
        property var    foundBands: []    // [{startHz,endHz,centerHz,bandwidthHz,mode}...]

        // คิวงาน: [{sHz, eHz, opt:{modes,overlap,dwellMs,settleMs}}]
        property var    jobQueue: []

        property int lowCutNow: radioScanner.spectrumGLPlot.low_cut
        property int highCutNow: radioScanner.spectrumGLPlot.high_cut


        /* ===== Profiles per mode ===== */
        function profileParams(m) {
            const profiles = {
                narrow: { trim:0.15, deltaHi:7.5, deltaLo:3.0, smoothBins:5,  mergeGapHz:1.3e3, minWidthHz:4e3  },
                wide:   { trim:0.18, deltaHi:8.0, deltaLo:3.0, smoothBins:13, mergeGapHz:20e3,  minWidthHz:50e3 }
            }
            return profiles[m] || profiles.wide
        }

        /* ===== Tune to center (offset = 0) ===== */
        function tuneToCenter(hz) {
            centerFreq = Math.round(hz)
            offsetFrequency = 0
            if (spectrumCanvas && spectrumCanvas.clearPeaks) {spectrumCanvas.clearPeaks(); console.log("spectrumCanvas.clearPeaks");}
            mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":'+ centerFreq +',"key":"memagic"}}')
            mainWindows.sendmessage('{"type":"dspcontrol","params":{"offset_freq":0}}')
            updateFrequency()
            mainWindows.updateCurrentOffsetFreq(0, centerFreq)

            // ✅ Fix4: grid depends on center/sampRate
            if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
        }

        /* ===== Analyze a window with a given mode ===== */
        function analyzeWindow(maxHold, centerHzNow, sampRateNow, modeNow) {
            const N = maxHold.length
            if (!N || sampRateNow <= 0) return []
            const binWidth = sampRateNow / N
            const leftEdge = centerHzNow - sampRateNow/2
            const opt = profileParams(modeNow)

            // Smooth
            const k = Math.max(0, Math.floor(opt.smoothBins||0))
            const smoothed = (function(){
                if (k<=0) return maxHold.slice()
                const out = new Array(N)
                for (let i=0;i<N;i++){
                    let s=0,c=0,a=Math.max(0,i-k),b=Math.min(N-1,i+k)
                    for (let j=a;j<=b;j++){ s+=maxHold[j]; c++ }
                    out[i]=s/c
                }
                return out
            })()

            // Noise (trimmed mean)
            const sorted = smoothed.slice().sort((a,b)=>a-b)
            const aidx = Math.floor(N*(opt.trim||0)), bidx = Math.ceil(N*(1-(opt.trim||0)))
            let noise=0; for (let i=aidx;i<bidx;i++) noise+=sorted[i]
            noise /= Math.max(1,bidx-aidx)
            const thrHi = noise + (opt.deltaHi||0)
            const thrLo = noise + (opt.deltaLo||0)

            // Hysteresis detect
            let ranges=[], inBand=false, st=-1
            for (let i=0;i<N;i++){
                const v=smoothed[i]
                if (!inBand) { if (v>thrHi){ inBand=true; st=i } }
                else { if (v<=thrLo){ ranges.push({start:st,end:i-1}); inBand=false } }
            }
            if (inBand) ranges.push({start:st,end:N-1})
            if (!ranges.length) return []

            // Merge by gap
            const mergeGapBins = Math.floor(((opt.mergeGapHz||0))/binWidth)
            let merged=[], cur=ranges[0]
            for (let k2=1;k2<ranges.length;k2++){
                const r=ranges[k2], gap=r.start-cur.end-1
                if (gap>=0 && gap<=mergeGapBins) cur.end=r.end
                else { merged.push(cur); cur=r }
            }
            merged.push(cur)

            // Keep >= minWidthHz
            const bands=[]
            for (const r of merged){
                const bwHz = (r.end-r.start+1)*binWidth
                if (bwHz < (opt.minWidthHz||0)) continue
                const startHz = leftEdge +  r.start    *binWidth
                const endHz   = leftEdge + (r.end + 1)*binWidth
                bands.push({ startHz, endHz, centerHz:0.5*(startHz+endHz), bandwidthHz:bwHz, mode:modeNow })
            }
            return bands
        }

        /* ===== Push & de-dup across windows ===== */
        function pushBands(bands) {
            for (const b of bands) {
                let dup = false
                for (const e of foundBands) {
                    if (b.centerHz >= e.startHz && b.centerHz <= e.endHz) {
                        dup = true
                        break
                    }
                }
                if (dup)
                    continue

                foundBands.push(b)

                const centerMHz = parseFloat((b.centerHz / 1e6).toFixed(6))
                const startMHz  = parseFloat((b.startHz  / 1e6).toFixed(6))
                const endMHz    = parseFloat((b.endHz    / 1e6).toFixed(6))

                // ใช้ค่าจาก UI ตอนนี้
                const lowCut  = peakScan.lowCutNow
                const highCut = peakScan.highCutNow

                if(centerMHz >= parseFloat((priStart    / 1e6).toFixed(6)) && centerMHz <= parseFloat((priStop    / 1e6).toFixed(6)) ){
                    console.log("startMHz:",priStart," centerMHz:",centerMHz," endMHz:",priStop)
                    foundCards.append({
                          "index":   profileCards.count,
                          "freq":    centerMHz,
                          "unit":    "MHz",
                          "bw":      `${(b.bandwidthHz / 1e3).toFixed(0)} kHz`,
                          "startHz": startMHz,
                          "endHz":   endMHz,
                          "mode":    b.mode,
                          "low_cut": lowCut,
                          "high_cut": highCut
                      })
                    profileCards.append({
                        "index":   profileCards.count,
                        "freq":    centerMHz,
                        "unit":    "MHz",
                        "bw":      `${(b.bandwidthHz / 1e3).toFixed(0)} kHz`,
                        "startHz": startMHz,
                        "endHz":   endMHz,
                        "mode":    b.mode,
                        "low_cut": lowCut,
                        "high_cut": highCut
                    })
                }
            }

            if (profileCards.count)
                rfScannerInterlock = true
        }

        /* ===== Stop current range (เรียกจาก action: "stop") ===== */
        function stopRange() {
            if (!running)
                return

            console.log("[PeakScan] stopRange() manual stop")

            // หยุด state การสแกนตอนนี้
            running = false

            // ไม่ให้คิวเก่าไปรันต่อ
            jobQueue = []

            // หยุด timer ทั้งคู่
            if (typeof settleTimer !== "undefined") settleTimer.stop()
            if (typeof dwellTimer  !== "undefined") dwellTimer.stop()

            // ❌ ไม่เคลียร์ foundBands / profileCards

            centerFreq = Math.round(keptStart)
            offsetFrequency = 0
            if (spectrumCanvas && spectrumCanvas.clearPeaks)
                spectrumCanvas.clearPeaks()

            mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":'
                                    + centerFreq + ',"key":"memagic"}}')
            mainWindows.sendmessage('{"type":"dspcontrol","params":{"offset_freq":0}}')
            updateFrequency()
            mainWindows.updateCurrentOffsetFreq(0, centerFreq)

            // ✅ Fix4: rebuild cached grid
            if (spectrumGridCanvas) spectrumGridCanvas.invalidate()

            if (profileCards.count > 0)
                widgetView = true

            trigerScan = true
            if (typeof signalProfileCards === "function")
                signalProfileCards()
        }

        /* ===== Start (queue-aware) ===== */
        function startRange(sHz, eHz, opts) {
            console.log("sHz:",sHz," eHz:",eHz," opts:",opts)
            if (running) {
                jobQueue.push({ sHz: sHz, eHz: eHz, opt: (opts||{}) })
                return
            }

            startHz = Math.min(sHz, eHz)
            stopHz  = Math.max(sHz, eHz)

            // === โหมดหลายค่า ===
            if (opts && Array.isArray(opts.modes) && opts.modes.length) {
                modes = opts.modes.slice()
            } else if (opts && typeof opts.mode === "string") {
                modes = [opts.mode]
            } else {
                // ถ้าไม่ได้ระบุ: ให้ทำครบสองโหมดเป็นดีฟอลต์
                modes = ["wide","narrow"]
            }

            overlap = (opts && typeof opts.overlap==="number") ? Math.max(0,Math.min(0.9,opts.overlap)) : overlap
            dwellMs = (opts && opts.dwellMs) || dwellMs
            settleMs= (opts && opts.settleMs)|| settleMs

            foundBands = []

            // กำหนดหน้าต่าง
            spanHz = sampRate / 2
            stepHz = Math.max(1, spanHz * (1 - overlap))
            totalWindows = Math.max(1, Math.ceil((stopHz - startHz - spanHz) / stepHz) + 1)
            windowIndex = 0

            running = true

            // หน้าต่างแรก
            const firstLeft  = startHz
            const firstRight = Math.min(stopHz, firstLeft + spanHz)
            const firstCenter= 0.5*(firstLeft + firstRight)

            tuneToCenter(firstCenter)
            settleTimer.interval = settleMs
            settleTimer.start()
        }

        /* ===== Finish → run next from queue ===== */
        function finishRangeAndRunNext() {
            running = false
            if (jobQueue.length > 0) {
                const job = jobQueue.shift()
                startRange(job.sHz, job.eHz, job.opt)
            } else {
                centerFreq = Math.round(keptStart)
                offsetFrequency = 0
                if (spectrumCanvas && spectrumCanvas.clearPeaks) spectrumCanvas.clearPeaks()
                mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":'+ centerFreq +',"key":"memagic"}}')
                mainWindows.sendmessage('{"type":"dspcontrol","params":{"offset_freq":0}}')
                updateFrequency()
                mainWindows.updateCurrentOffsetFreq(0, centerFreq)

                // ✅ Fix4: rebuild cached grid
                if (spectrumGridCanvas) spectrumGridCanvas.invalidate()

                if(profileCards.count > 0){
                    widgetView = true
                }
                console.log("[PeakScan] all done")
                trigerScan = true
                if (typeof signalProfileCards === "function") signalProfileCards()
            }
        }

        /* ===== Timers ===== */
        Timer { // รอ settle หลังจูน
            id: settleTimer
            repeat: false
            onTriggered: dwellTimer.start()
        }

        Timer { // เก็บ max-hold แล้ว "วิเคราะห์ครบทุกโหมด" ภายในหน้าต่างนี้
            id: dwellTimer
            repeat: false
            interval: peakScan.dwellMs
            onTriggered: {
                const data = (root.maxHoldKept && root.maxHoldKept.length) ? root.maxHoldKept.slice() : []
                if (data.length) {
                    for (var i = 0; i < peakScan.modes.length; ++i) {
                        const m = peakScan.modes[i]
                        const bands = peakScan.analyzeWindow(data, root.centerFreq, root.sampRate, m)
                        peakScan.pushBands(bands)
                    }
                }
                peakScan.nextWindow()
            }
        }

        /* ===== Next window ===== */
        function nextWindow() {
            if (!running) return
            windowIndex++
            if (windowIndex >= totalWindows) {
                finishRangeAndRunNext()
                return
            }
            const left  = startHz + windowIndex*stepHz
            const right = left + spanHz
            const center = (right <= stopHz)
                ? 0.5*(left+right)
                : 0.5*(Math.max(startHz, stopHz - spanHz) + stopHz)

            tuneToCenter(center)
            settleTimer.interval = settleMs
            settleTimer.start()
        }
    }

    /* ============================================================
       Handler: เรียกจาก mainWindows.findBandsWithProfile(msg)
       ============================================================ */
    function findBandsWithProfile(msg) {
        trigerScan = false
        var obj = {}
        try {
            obj = JSON.parse(msg)
        } catch(e) {
            console.warn("Bad JSON:", e)
            return
        }

        if (obj.objectName !== "Scan")
            return

        console.log("obj.action",msg)
        // ====== กรณี STOP: แค่หยุดสแกน แต่ไม่ล้างผล ======
        if (obj.action === "stop") {
            console.log("[findBandsWithProfile] received STOP")
            peakScan.stopRange()
            return
        }

        // ====== กรณี START/QUEUE งานสแกนใหม่ ======
        if (!obj.frequency) {
            console.warn("[findBandsWithProfile] no frequency field for Scan")
            return
        }

        // เคลียร์สถานะเฉพาะรอบนี้ (เฉพาะตอนเริ่มงานใหม่)
        offsetFrequency      = 0
        root.maxHoldKept     = []
        rfScannerInterlock   = false

        // อ่านช่วง
        var startPoint = Number(obj.frequency.start) || 0
        var stopPoint  = Number(obj.frequency.stop)  || 0

        priStart = startPoint
        priStop = stopPoint
        keptStart = startPoint

        // จัด modes
        var modesOpt = []
        if (Array.isArray(obj.modes) && obj.modes.length) {
            modesOpt = obj.modes.slice()
        } else if (typeof obj.mode === "string") {
            modesOpt = [obj.mode]
        } else {
            modesOpt = ["wide","narrow"]
        }

        var options = {
            modes:   modesOpt,
            overlap: (typeof obj.overlap === "number") ? obj.overlap : 0.15,
            dwellMs:(typeof obj.dwellMs  === "number") ? obj.dwellMs : 900,
            settleMs:(typeof obj.settleMs=== "number") ? obj.settleMs: 250
        }

        console.log("default:",stopPoint,root.sampRate)
        stopPoint = stopPoint + (root.sampRate / 2)

        foundCards.clear()
        console.log("[findBandsWithProfile] enqueue:",
                    startPoint, "→", stopPoint,
                    "modes:", JSON.stringify(options.modes))

        peakScan.startRange(startPoint, stopPoint, options)

        // ✅ Fix4: rebuild cached grid (range start usually changes center soon)
        if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
    }
}
