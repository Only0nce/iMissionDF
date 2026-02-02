import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.4
import QtQuick.Layouts 1.0
// import QtGraphicalEffects 1.15
import "ui"

Item {
    id: root
    anchors.fill: parent

    property real priStart: 0
    property real priStop: 0

    property real plotWidth: root.width
    property real dataCount : 0
    property real smeterLevel: -100
    property var spectrumData: []
    property var waterfallBuffer: []
    property var waterfallColorMap: []
    property real smeterBuffered: smeterLevel

    signal spectrumUpdated(var spectrum)
    signal waterfallUpdated(var line)
    signal waterfallColorUpdate(var colors)

    property var maxHoldKept: []
    property var peakVal: []
    property bool rfScannerInterlock: false

    property real centerFreq: mainWindows.center_freq()    // in Hz
    property int sampRate: mainWindows.samp_rate()        // in Hz

    property real waterfallMinDb: -100
    property real waterfallMaxDb: 0
    property real waterfallMax: 0
    property real waterfallMin: -100
    property alias autoScaleTimer: autoScaleTimer

    property real offsetFrequency: 0      // in Hz
    property int bandwidth: high_cut-low_cut         // in Hz
    property int low_cut: bwModel.get(scanBwSelected).low_cut
    property int high_cut: bwModel.get(scanBwSelected).high_cut
    property int offsetSnapStep: 100   // in Hz (e.g. 12.5 kHz)

    property bool autoScaleEnabled: false

    // property int sampRate: 2500000  // initial span in Hz
    property int sampRateMin: 50000
    property int sampRateMax: 24.576e6
    property real zoomStep: root.width
    property string start_mod: ""
    property real xPos: 0
    property real setCenterFreq: centerFreq
    property bool initFreq: false

    property int offsetStart: -1600000
    property int offsetStop:  1600000
    property int offsetStep:  1000
    property int dwellMs:     500
    property int currentOffset: offsetStart
    property bool scanning: false

    property int keptStart: 0
    property int profileViewIndex: 0

    Theme{
        id: theme
    }

    // ===== Timer ที่ใช้แทน for loop =====
    Timer {
        id: scanTimer
        interval: 5
        repeat: true
        running: false
        onTriggered: {
            // ตั้งค่า offset ใหม่
            offsetFrequency = currentOffset      // <- ค่านี้คุณส่งไป backend ได้ เช่น mainWindows.sendOffset(offsetFrequency)
            console.log("offset =", offsetFrequency)

            // เพิ่มค่า
            currentOffset += offsetStep

            // ถึงขอบบนแล้วหยุด
            if (currentOffset > offsetStop) {
                stopScan()
            }
        }
    }

    function startScan() {
        if (scanning)
            return
        currentOffset = offsetStart
        scanning = true
        scanTimer.start()
        console.log("scan started")
    }

    function stopScan() {
        scanning = false
        scanTimer.stop()
        console.log("scan stopped")
    }


    onBandwidthChanged: {
        console.log("Bandwidth:",bandwidth,low_cut,high_cut)
    }

    onStart_modChanged: {
        let idx = getReceiverIndex(start_mod);
        console.log("onStart_modChanged index:", idx);
        if (idx !== -1) {
            scanReceiverModeSelected = idx;
            console.log("Selected index:", idx);
        }
        currentModIndex = scanReceiverModeSelected
    }

    onOffsetFrequencyChanged: {
        // console.log("onOffsetFrequencyChanged")
        if (((centerFreq+offsetFrequency) > (centerFreq+(sampRate/2))) || ((centerFreq+offsetFrequency) < (centerFreq-(sampRate/2)))){
            centerFreq = centerFreq+offsetFrequency
            offsetFrequency = 0
            mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":'+ centerFreq +',"key":"memagic"}}')
            updateFrequency()
        }
        else
        {
            mainWindows.sendmessage('{"type": "dspcontrol","params": {"offset_freq": '+offsetFrequency+'}}')
            freqScan = centerFreq+offsetFrequency
            // console.log("freqScan" , freqScan)
            updateFrequency()
        }
        mainWindows.updateCurrentOffsetFreq(offsetFrequency,centerFreq)
    }


    onCenterFreqChanged:
    {
        // console.log("onCenterFreqChanged")
        spectrumCanvas.clearPeakTimer.start()
    }

    onSetCenterFreqChanged:
    {
        // console.log("onSetCenterFreqChanged")
        if (setCenterFreq != centerFreq)
        {
            mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":'+ setCenterFreq +',"key":"memagic"}}')
        }
        spectrumCanvas.clearPeakTimer.start()
    }


    onSampRateChanged: {
        spectrumCanvas.requestPaint();
        waterfallCanvas.requestPaint();
        overlayCanvas.requestPaint();
        // if (initFreq == false){
        //     if (centerFreq != 0){
        //         mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":'+ centerFreq +',"key":"memagic"}}')
        //         mainWindows.sendmessage('{"type": "dspcontrol","params": {"offset_freq": '+offsetFrequency+'}}')
        //         initFreq = true
        //     }
        // }
    }
    Component.onCompleted: {
        mainWindows.updateCenterFreq.connect(updateCenterFreq)

        mainWindows.spectrumUpdated.connect(spectrumUpdated)
        mainWindows.waterfallUpdated.connect(waterfallUpdated)
        mainWindows.waterfallColorUpdate.connect(waterfallColorUpdate)

        mainWindows.findBandsWithProfile.connect(findBandsWithProfile)
        // mainWindows.waterfallLevelsChanged.connect(function(levelsmin, levelsmax) {
        //     console.log("Updated waterfall range:",levelsmin,levelsmax)
        //     updateWaterfallLevels(levelsmin, levelsmax)
        // })


        mainWindows.smeterValueUpdated.connect(function(smeter) {
            smeterValueUpdated(smeter)
        })

    }

    function temperature(value)
    {

    }

    function setOffset(freqOffset) {
        offsetFrequency = freqOffset;
        // if (offsetFrequency > sampRate/2){
        //     setCenterFreq = centerFreq+(sampRate/2);
        //     offsetFrequency = 0;
        // }
        // if (offsetFrequency < sampRate/(-2)) {
        //     setCenterFreq = centerFreq-(sampRate/2);
        //     offsetFrequency = 0;
        // }
        // overlayCanvas.requestPaint();       // if you show offset visually
        // spectrumCanvas.requestPaint();      // update spectrum
        // waterfallCanvas.requestPaint();     // update waterfall
    }

    function setManualOffset(freq)
    {
        let freqOffset = freq-centerFreq
        offsetFrequency = freqOffset;
        console.log("setManualOffset",freq, " freqOffset",freqOffset)
        overlayCanvas.requestPaint();       // if you show offset visually
        spectrumCanvas.requestPaint();      // update spectrum
        waterfallCanvas.requestPaint();     // update waterfall
    }

    function zoomIn() {
        plotWidth += zoomStep
        if (plotWidth > root.width*20)
            plotWidth = root.width*20

        applyZoom()
    }

    function zoomOut() {
        plotWidth -= zoomStep
        if (plotWidth < root.width)
            plotWidth = root.width

        applyZoom()
    }

    function applyZoom() {
        spectrumCanvas.requestPaint();
        waterfallCanvas.requestPaint();
        overlayCanvas.requestPaint();
        zoomNav.rectangle.opacity = 1
        console.log("applyZoom",plotWidth,"xPos",xPos)
    }

    function smeterValueUpdated(smeter)
    {
        smeterLevel = smeter
    }

    function updateWaterfallLevels(minDb, maxDb) {
        waterfallMinDb = minDb;
        waterfallMaxDb = maxDb;
        waterfallScaleControl.waterfallMinDb = waterfallMinDb
        waterfallScaleControl.waterfallMaxDb = waterfallMaxDb
    }

    function updateCenterFreq()
    {
        sampRate = mainWindows.samp_rate()
        centerFreq = mainWindows.center_freq()
        sampRateMax = mainWindows.samp_rate()
        start_mod = mainWindows.start_mod()
        console.log("mainWindows.start_mod()",mainWindows.start_mod()," start_mod",start_mod)
        freqScan = centerFreq
        updateFrequency()
        resetCenterFreqTimer.start();
    }

    function autoScaleWaterfallColor() {
        if (waterfallBuffer.length === 0)
            return;

        var latestLine = waterfallBuffer[waterfallBuffer.length - 1];
        if (!latestLine || latestLine.length < 2)
            return;

        var minVal = latestLine[0];
        var maxVal = latestLine[0];

        for (var i = 1; i < latestLine.length; ++i) {
            var v = latestLine[i];
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
        }

        // Clamp to avoid over-scaling
        waterfallMin = Math.max(minVal - 5, -120);
        waterfallMax = Math.min(maxVal + 5, 0);

        waterfallMinDb = waterfallMin < waterfallMinDb ? waterfallMin : waterfallMinDb
        waterfallMaxDb = waterfallMax > waterfallMaxDb ? waterfallMax : waterfallMaxDb

        console.log("Auto-scaled waterfall:", waterfallMinDb, waterfallMaxDb);
    }

    function resetCenterFreq()
    {
        if (initFreq == false){
            if (centerFreq != 0){
                let centerFrequency = centerFreq;
                mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":'+ 30000000 +',"key":"memagic"}}')
                mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":'+ centerFrequency +',"key":"memagic"}}')
                mainWindows.sendmessage('{"type": "dspcontrol","params": {"offset_freq": '+offsetFrequency+'}}')
                initFreq = true
            }
        }
    }

    Timer {
        id: resetCenterFreqTimer
        interval: 1000  // ms
        repeat: false
        running: false
        onTriggered: resetCenterFreq()
    }

    Timer {
        id: autoScaleTimer
        interval: 500  // ms
        repeat: true
        running: autoScaleEnabled
        onTriggered: autoScaleWaterfallColor()
    }

    Canvas {
        id: spectrumCanvas
        width: plotWidth
        // height: rfScannerInterlock ? parent.height / 4 : parent.height / 2
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

        onPaint: {
            var ctx = getContext("2d");
            var w = spectrumCanvas.width;
            var h = spectrumCanvas.height;

            ctx.clearRect(0, 0, w, h);

            if (spectrumData.length < 2)
                return;

            const minDb = waterfallMinDb;
            const maxDb = waterfallMaxDb;
            const rangeDb = maxDb - minDb;

            let startFreq = centerFreq - sampRate / 2;
            let stopFreq = centerFreq + sampRate / 2;
            let freqRange = stopFreq - startFreq;

            let pixelsPerFreqHz = plotWidth / sampRate;
            let minLabelSpacing = 200;

            let rawStep = minLabelSpacing / pixelsPerFreqHz;
            let pow10 = Math.pow(10, Math.floor(Math.log10(rawStep)));
            let freqStep = pow10;

            if (rawStep / pow10 >= 5) freqStep = 5 * pow10;
            else if (rawStep / pow10 >= 2) freqStep = 2 * pow10;

            // --- spectrumCanvas.onPaint: grid + labels ---
            ctx.strokeStyle = theme.gridLine;
            ctx.lineWidth = 1;
            ctx.font = "11px monospace";
            ctx.fillStyle = theme.axisText;

            for (var db = minDb; db <= maxDb; db += 10) {
                let y = h - ((db - minDb) / rangeDb) * h;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(w, y);
                ctx.stroke();
                let label = db.toFixed(0) + " dBm ";
                ctx.fillText(label, 4, y - 2);
            }

            ctx.strokeStyle = theme.spectrumLine;
            ctx.lineWidth = 1;
            ctx.font = "11px monospace";
            ctx.fillStyle = "#aaa";

            for (var freq = startFreq; freq <= stopFreq; freq += freqStep) {
                let x = ((freq - startFreq) / freqRange) * plotWidth;
                ctx.beginPath();
                ctx.moveTo(x, 0);
                ctx.lineTo(x, 8);
                ctx.stroke();
                let label = (freq / 1e6).toFixed(1) + " MHz";
                ctx.fillText(label, x, 12);
            }

            ctx.beginPath();
            ctx.strokeStyle = "#00FF00";
            ctx.moveTo(0, h - ((spectrumData[0] - minDb) / rangeDb) * h);

            for (var i = 1; i < spectrumData.length; ++i) {
                let x = i / (spectrumData.length - 1) * w;
                let y = h - ((spectrumData[i] - minDb) / rangeDb) * h;
                ctx.lineTo(x, y);
            }
            ctx.stroke();

            // Max hold logic
            for (var i = 0; i < spectrumData.length; ++i) {
                if (maxHold.length < spectrumData.length)
                    maxHold.push(spectrumData[i]);
                else if (spectrumData[i] > maxHold[i])
                    maxHold[i] = spectrumData[i];
            }
            maxHoldKept = maxHold
            // console.log("maxHoldKept already kept data!")
            if (showMaxHold && maxHold.length === spectrumData.length) {
                ctx.beginPath();
                ctx.strokeStyle = theme.maxHoldLine;
                ctx.moveTo(0, h - ((maxHold[0] - minDb) / rangeDb) * h);
                for (var i = 1; i < maxHold.length; ++i) {
                    let x = i / (maxHold.length - 1) * w;
                    let y = h - ((maxHold[i] - minDb) / rangeDb) * h;
                    ctx.lineTo(x, y);
                }
                ctx.stroke();
            }
        }

        Connections {
            target: root
            function onSpectrumUpdated(spectrum) {
                spectrumData = spectrum;
                spectrumCanvas.requestPaint();
            }
        }
    }

    Canvas {
        id: waterfallCanvas
        y: spectrumCanvas.height
        x: spectrumCanvas.x
        width: spectrumCanvas.width
        // height: rfScannerInterlock ? parent.height / 4 : parent.height/2
        height: parent.height / 4
        onPaint: {
            const minDb = waterfallMinDb;
            const maxDb = waterfallMaxDb;
            const rangeDb = maxDb - minDb;
            const colorCount = waterfallColorMap.length;
            var ctx = getContext("2d");

            // Scroll existing content down by 1 pixel
            ctx.drawImage(waterfallCanvas, 0, 0, width, height - 1, 0, 1, width, height - 1);

            // Get the newest row (line) to draw at the top
            var line = waterfallBuffer[waterfallBuffer.length - 1];
            if (!line || typeof line.length === "undefined") return;

            const canvasWidth = width;
            const bins = line.length;

            for (let i = 0; i < bins; ++i) {
                // x position of this bin on canvas
                const x = Math.floor(i / (bins - 1) * canvasWidth);

                const dB = Math.max(minDb, Math.min(maxDb, line[i]));
                const norm = (dB - minDb) / rangeDb;
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
            function onWaterfallColorUpdate(colors)
            {
                waterfallColorMap = colors
            }
        }

        Connections {
            target: root
            function onWaterfallUpdated(line)
            {
                dataCount++;
                if (waterfallBuffer.length >= waterfallCanvas.height)
                {
                    waterfallBuffer.shift();
                }

                waterfallBuffer.push(line);
                waterfallCanvas.requestPaint();
                dataCount=0;
            }
        }
    }

    Canvas {
        id: overlayCanvas
        z: 10
        antialiasing: false

        // ผูกครอบตั้งแต่บน spectrum จนถึงล่าง waterfall
        anchors.left:   spectrumCanvas.left
        anchors.right:  spectrumCanvas.right
        anchors.top:    spectrumCanvas.top
        anchors.bottom: waterfallCanvas.bottom
        // ถ้าอยากเว้นล่าง 100px ให้ใช้บรรทัดนี้ (และอย่าใส่ใน MouseArea)
        // anchors.bottomMargin: 100

        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            let ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            let startFreq = centerFreq - sampRate / 2;
            let freqRange = sampRate;
            let canvasWidth = overlayCanvas.width;
            let canvasHeight = overlayCanvas.height;

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
            anchors.fill: parent        // ⬅️ กินเต็มผืน
            // anchors.bottomMargin: 100 // ⛔️ ไม่ต้องใส่ที่นี่
            // width: plotWidth          // ⛔️ เอาออก

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
                    spectrumCanvas.requestPaint();
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
                spectrumCanvas.requestPaint();
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

    /* === MemoryAddEdit ยังคงอยู่ แต่ซ่อนเมื่อเปิด widgetView === */
    // MemoryAddEdit {
    //     id: memoryAddEdit
    //     anchors.fill: memorySlot
    //     visible: !widgetView
    //     z: 51
    // }

    /* === MemoryAddEdit ยังคงอยู่ แต่ซ่อนเมื่อเปิด widgetView === */


    NewMemoryAddEdit {
        id: newMemoryAddEdit
        anchors.fill: memorySlot
        visible: !widgetView
        z: 51
        radioMemLists: radioMemList    // ✅ ส่ง ListModel id: radioMemList เข้าไป
    }

    /* === MemoryAddEdit ยังคงอยู่ แต่ซ่อนเมื่อเปิด widgetView === */


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

                // console.log("lowCut:",lowCut," highCut:",highCut," peakScan.lowCutNow:",peakScan.lowCutNow," peakScan.highCutNow:",peakScan.highCutNow)

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
            //    -> ผลช่องความถี่ที่เจอแล้วจะยังอยู่ให้ user ดู/เลือก

            // ถ้าอยากคืนความถี่กลับต้นทางก็ทำได้ (ตามที่คุณใช้ keptStart อยู่)
            centerFreq = Math.round(keptStart)
            offsetFrequency = 0
            if (spectrumCanvas && spectrumCanvas.clearPeaks)
                spectrumCanvas.clearPeaks()

            mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":'
                                    + centerFreq + ',"key":"memagic"}}')
            mainWindows.sendmessage('{"type":"dspcontrol","params":{"offset_freq":0}}')
            updateFrequency()
            mainWindows.updateCurrentOffsetFreq(0, centerFreq)

            // ถ้าสแกนเจออะไรแล้วก็โชว์ widget ได้เลย
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
                // console.log("[PeakScan] queued:", sHz, "→", eHz, "modes:", (opts && opts.modes) || "[wide]")
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

            // เคลียร์ list เมื่อเป็นงานแรก และมี "wide" อยู่ใน modes
            // if (modes.indexOf("wide") !== -1) profileCards.clear()
            foundBands = []

            // กำหนดหน้าต่าง
            spanHz = sampRate / 2
            stepHz = Math.max(1, spanHz * (1 - overlap))
            totalWindows = Math.max(1, Math.ceil((stopHz - startHz - spanHz) / stepHz) + 1)
            windowIndex = 0

            running = true
            // console.log("[PeakScan] start:", startHz, "→", stopHz, "win:", totalWindows, "span:", spanHz, "step:", stepHz, "modes:", JSON.stringify(modes))

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
                // console.log("[PeakScan] dequeue →", job.sHz, "→", job.eHz, "modes:", (job.opt && job.opt.modes) || (job.opt && job.opt.mode) || "[wide]")
                startRange(job.sHz, job.eHz, job.opt)
            } else {
                centerFreq = Math.round(keptStart)
                offsetFrequency = 0
                if (spectrumCanvas && spectrumCanvas.clearPeaks) spectrumCanvas.clearPeaks()
                mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":'+ centerFreq +',"key":"memagic"}}')
                mainWindows.sendmessage('{"type":"dspcontrol","params":{"offset_freq":0}}')
                updateFrequency()
                mainWindows.updateCurrentOffsetFreq(0, centerFreq)

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
                    // วิเคราะห์ทุกรูปแบบโหมดด้วย snapshot เดียว
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
       - งานใหม่จะเข้าคิว ถ้ากำลังก
       - รองรับ modes หลายค่าในครั้งเดียว (ทำครบทุกโหมดต่อหน้าต่าง)
       JSON ตัวอย่าง:
       {
         "objectName": "Scan",
         "frequency": { "start": 88e6, "stop": 108e6 },
         "modes": ["wide","narrow"],     // หรือ "mode": "wide"
         "overlap": 0.15,
         "dwellMs": 900,
         "settleMs": 250
       }
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
            // ถ้าไม่ส่ง mode/modes → ทำครบสองโหมดเป็นดีฟอลต์
            modesOpt = ["wide","narrow"]
        }

        var options = {
            modes:   modesOpt,
            overlap: (typeof obj.overlap === "number") ? obj.overlap : 0.15,
            dwellMs:(typeof obj.dwellMs  === "number") ? obj.dwellMs : 900,
            settleMs:(typeof obj.settleMs=== "number") ? obj.settleMs: 250
        }

        console.log("default:",stopPoint,root.sampRate)
        // ปรับ stopPoint ตาม logic เดิม
        stopPoint = stopPoint + (root.sampRate / 2)

        // เริ่มงานใหม่ → ถ้าต้องการล้างการ์ดเก่าออก ค่อย clear ตรงนี้
        // profileCards.clear()
        foundCards.clear()
        console.log("[findBandsWithProfile] enqueue:",
                    startPoint, "→", stopPoint,
                    "modes:", JSON.stringify(options.modes))

        peakScan.startRange(startPoint, stopPoint, options)  // ถ้ากำลังรัน → เข้าคิว
    }


}
