import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.settings 1.1

Rectangle {
    id: root
    // StackView sizes pages itself. Do not anchor the root page to parent;
    // anchors on a StackView-managed item can race transitions and produce
    // QQuickWindow UpdateRequest crashes on embedded Qt 5.15 render loops.
    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080
    color: "#000000"

    // =========================
    // Logical display-source routing
    // CH1 = AstraRX/Home RX spectrum+waterfall data
    // CH2..CH6 = existing DF physical ADC CH1..CH5
    // Polar/MUSIC remains the coherent five-channel DF result in all modes.
    // =========================
    property int displayChannel: 1
    readonly property bool rxDisplaySelected: displayChannel === 1
    property var rxFftFreqHz: []
    property var rxFftMagDb: []
    property int rxAxisBins: 0
    property real rxAxisCenterHz: 0
    property real rxAxisSampleRate: 0

    // DOA-VIEWER1.9: final production performance governor.  The heavy
    // render paths are already native/GPU; this governor keeps the page smooth
    // under real Jetson load by lowering only display cadence when the page
    // cannot meet its requested FPS, then raising quality back after recovery.
    property bool adaptiveQualityEnabled: true
    // 0 = high, 1 = balanced/default, 2 = economy/recovery
    property int renderQualityLevel: 1
    property int _adaptiveOverloadStreak: 0
    property int _adaptiveRecoveryStreak: 0

    function _qualityName() {
        if (root.renderQualityLevel <= 0) return "high"
        if (root.renderQualityLevel >= 2) return "economy"
        return "balanced"
    }

    function _schedulerIntervalMs() {
        if (root.rxDisplaySelected) {
            if (root.renderQualityLevel <= 0) return 33
            if (root.renderQualityLevel >= 2) return 66
            return 40
        }
        if (root.renderQualityLevel <= 0) return 40
        if (root.renderQualityLevel >= 2) return 80
        return 50
    }

    function _spectrumFps() {
        if (root.rxDisplaySelected) {
            if (root.renderQualityLevel <= 0) return 30
            if (root.renderQualityLevel >= 2) return 15
            return 24
        }
        if (root.renderQualityLevel <= 0) return 24
        if (root.renderQualityLevel >= 2) return 12
        return 20
    }

    function _waterfallFps() {
        if (root.rxDisplaySelected) {
            if (root.renderQualityLevel <= 0) return 22
            if (root.renderQualityLevel >= 2) return 10
            return 18
        }
        if (root.renderQualityLevel <= 0) return 18
        if (root.renderQualityLevel >= 2) return 8
        return 15
    }

    function _polarFps() {
        if (root.renderQualityLevel <= 0) return 15
        if (root.renderQualityLevel >= 2) return 8
        return 12
    }

    function _setRenderQualityLevel(level, reason) {
        var q = Math.max(0, Math.min(2, Number(level)))
        if (root.renderQualityLevel === q) return
        root.renderQualityLevel = q
        uiSettings.renderQualityLevel = q
        console.log("[DOA-ADAPTIVE] quality=" + root._qualityName()
                    + " level=" + q
                    + " reason=" + reason
                    + " source=" + root.logicalSourceName())
    }

    // Smooth-render cache: incoming RX/DF frames are coalesced into one
    // visible frame on a bounded render clock. This prevents QML from trying
    // to repaint every backend packet when the UI/GPU is temporarily busy.
    property var displayFftFreqHz: []
    property var displayFftMagDb: []
    property int displayFrameSequence: 0
    property bool _rxPendingFrame: false
    property bool _dfPendingFrame: false
    property int _perfFrames: 0
    property int _perfCoalesced: 0
    property int _perfLastSeq: 0

    function mw() {
        return (typeof(mainWindows) !== "undefined" && mainWindows !== null) ? mainWindows : null
    }

    readonly property bool rxSourceAvailable: mw() !== null
    // CH1/RX FFT presentation gate. This is intentionally separate from
    // doaClient.spectrumEnabled, which belongs to the RFSoC DF FFT path.
    property bool rxFftEnabled: true

    function logicalSourceName() {
        if (displayChannel === 1) return "RX"
        return "DF" + (displayChannel - 1)
    }

    function spectrumTitle() {
        return "RF FFT Spectrum (CH" + displayChannel + " / " + logicalSourceName() + ")"
    }

    function syncRxConsumer() {
        var m = mw()
        if (!m || m.setDoaRxSpectrumActive === undefined) return
        m.setDoaRxSpectrumActive(root.visible && root.rxDisplaySelected && root.rxFftEnabled)
    }

    function rebuildRxFrequencyAxis() {
        var m = mw()
        if (!m) return

        var mags = m.doaRxFftMagDb()
        var n = mags && mags.length !== undefined ? Number(mags.length) : 0
        var center = Number(m.doaRxCenterHz())
        var rate = Number(m.doaRxSampleRate())
        if (n < 8 || !isFinite(center) || center <= 0 || !isFinite(rate) || rate <= 0) {
            root.rxFftMagDb = []
            root.rxFftFreqHz = []
            root.rxAxisBins = 0
            return
        }

        root.rxFftMagDb = mags

        if (root.rxAxisBins === n
                && Math.abs(root.rxAxisCenterHz - center) < 0.5
                && root.rxAxisSampleRate === rate)
            return

        var startHz = center - rate * 0.5
        var stepHz = rate / Math.max(1, n - 1)
        var axis = new Array(n)
        for (var i = 0; i < n; ++i)
            axis[i] = startHz + stepHz * i

        root.rxFftFreqHz = axis
        root.rxAxisBins = n
        root.rxAxisCenterHz = center
        root.rxAxisSampleRate = rate
    }

    function publishSelectedFrame(reason) {
        if (!root.visible) return

        var seqBefore = root.displayFrameSequence

        if (root.rxDisplaySelected) {
            if (!root.rxSourceAvailable || !root.rxFftEnabled) return
            root.rebuildRxFrequencyAxis()
            if (!root.rxFftMagDb || root.rxFftMagDb.length < 8) return
            root.displayFftFreqHz = root.rxFftFreqHz
            root.displayFftMagDb = root.rxFftMagDb
        } else {
            if (typeof(doaClient) === "undefined" || doaClient === null) return
            if (!doaClient.fftMagDb || doaClient.fftMagDb.length < 8) return
            root.displayFftFreqHz = doaClient.fftFreqHz
            root.displayFftMagDb = doaClient.fftMagDb
        }

        root.displayFrameSequence++
        root._perfFrames++
        if (root.displayFrameSequence > seqBefore + 1)
            root._perfCoalesced += (root.displayFrameSequence - seqBefore - 1)
    }

    Timer {
        id: renderScheduler
        // One DoA display clock for spectrum + waterfall. Keep UI responsive
        // without replaying old FFT frames. Hidden/unselected sources only cache
        // the latest backend frame.
        interval: root._schedulerIntervalMs()
        running: root.visible
        repeat: true
        onTriggered: {
            if (root.rxDisplaySelected) {
                if (!root.rxFftEnabled) return
                if (!root._rxPendingFrame) return
                root._rxPendingFrame = false
            } else {
                if (!root._dfPendingFrame) return
                root._dfPendingFrame = false
            }
            root.publishSelectedFrame("tick")
        }
    }

    Timer {
        id: perfLogTimer
        interval: 5000
        running: root.visible
        repeat: true
        onTriggered: {
            var deltaSeq = root.displayFrameSequence - root._perfLastSeq
            root._perfLastSeq = root.displayFrameSequence
            var fps = deltaSeq / 5.0
            var targetFps = 1000.0 / Math.max(1, renderScheduler.interval)
            var bins = (root.displayFftMagDb ? root.displayFftMagDb.length : 0)

            if (root.adaptiveQualityEnabled && bins >= 8) {
                if (fps < targetFps * 0.62) {
                    root._adaptiveOverloadStreak++
                    root._adaptiveRecoveryStreak = 0
                } else if (fps > targetFps * 0.82) {
                    root._adaptiveRecoveryStreak++
                    root._adaptiveOverloadStreak = 0
                } else {
                    root._adaptiveOverloadStreak = 0
                    root._adaptiveRecoveryStreak = 0
                }

                if (root._adaptiveOverloadStreak >= 2 && root.renderQualityLevel < 2) {
                    root._adaptiveOverloadStreak = 0
                    root._setRenderQualityLevel(root.renderQualityLevel + 1, "fps-below-budget")
                } else if (root._adaptiveRecoveryStreak >= 4 && root.renderQualityLevel > 0) {
                    root._adaptiveRecoveryStreak = 0
                    root._setRenderQualityLevel(root.renderQualityLevel - 1, "fps-recovered")
                }
            }

            console.log("[DOA-VIEWER-PERF] source=" + root.logicalSourceName()
                        + " frames5s=" + deltaSeq
                        + " fps=" + fps.toFixed(1)
                        + " targetFps=" + targetFps.toFixed(1)
                        + " quality=" + root._qualityName()
                        + " adaptive=" + root.adaptiveQualityEnabled
                        + " bins=" + bins
                        + " seq=" + root.displayFrameSequence)
        }
    }

    function selectDisplayChannel(channel) {
        var ch = Math.max(1, Math.min(6, Number(channel)))
        if (root.displayChannel !== ch)
            root.displayChannel = ch

        if (ch >= 2 && typeof(doaClient) !== "undefined" && doaClient !== null) {
            var physicalDfChannel = ch - 2
            console.log("[DOA-DISPLAY-SOURCE] logical=CH" + ch
                        + " source=DF" + (ch - 1)
                        + " physical_adc=" + physicalDfChannel)
            if (doaClient.fftChannel !== physicalDfChannel)
                doaClient.fftChannel = physicalDfChannel
        } else if (ch === 1) {
            console.log("[DOA-DISPLAY-SOURCE] logical=CH1 source=ASTRARX_HOME no_setAdcChannel=1")
        }

        syncRxConsumer()
        if (ch === 1 && root.rxFftEnabled) rebuildRxFrequencyAxis()
    }

    function setRxFftEnabled(enabled) {
        var v = !!enabled
        if (root.rxFftEnabled === v)
            return

        root.rxFftEnabled = v
        uiSettings.rxFftEnabled = v
        syncRxConsumer()

        if (root.rxDisplaySelected) {
            root.displayFftFreqHz = []
            root.displayFftMagDb = []
            root.displayFrameSequence++
            root._rxPendingFrame = v
            if (v)
                root.publishSelectedFrame("rx-fft-enabled")
        }

        console.log("[DOA-RX-FFT]", v ? "ON" : "OFF", "logical=CH1 source=ASTRARX_HOME")
    }

    Connections {
        target: root.mw()
        function onDoaRxFftFrameChanged() {
            if (root.rxDisplaySelected && root.rxFftEnabled)
                root._rxPendingFrame = true
        }
    }

    Connections {
        target: (typeof(doaClient) !== "undefined" && doaClient !== null) ? doaClient : null
        function onFftChannelChanged() {
            // Preserve CH1/RX selection. If a DF channel is currently selected,
            // reflect authoritative backend changes using the logical +1 offset.
            if (root.displayChannel >= 2)
                root.displayChannel = Math.max(2, Math.min(6, doaClient.fftChannel + 2))
        }
        function onFftChanged() {
            if (!root.rxDisplaySelected)
                root._dfPendingFrame = true
        }
    }

    onDisplayChannelChanged: {
        syncRxConsumer()
        root.displayFftFreqHz = []
        root.displayFftMagDb = []
        root.displayFrameSequence++
        root._rxPendingFrame = root.rxDisplaySelected && root.rxFftEnabled
        root._dfPendingFrame = !root.rxDisplaySelected
        root.publishSelectedFrame("source-change")
    }
    onVisibleChanged: {
        syncRxConsumer()
        if (visible) {
            root._rxPendingFrame = root.rxDisplaySelected && root.rxFftEnabled
            root._dfPendingFrame = !root.rxDisplaySelected
            root.publishSelectedFrame("visible")
        }
    }

    // =========================
    // Persist settings
    // =========================
    Settings {
        id: uiSettings
        category: "FftWaterfall"

        property bool yAuto: false
        property int  yMinDbUser: -120   // ✅ เก็บเป็น int ให้ตรงกับ SpinBox
        property int  yMaxDbUser: -60
        property bool rxFftEnabled: true
        property bool adaptiveQualityEnabled: true
        property int  renderQualityLevel: 1
    }

    // =========================
    // Runtime state (ไม่ bind ตรงกับ Settings)
    // =========================
    property bool yAuto: false
    property int  yMinDbUser: -120
    property int  yMaxDbUser: -60

    Component.onCompleted: {
        // ✅ โหลดค่าจาก Settings ครั้งเดียว
        root.yAuto = uiSettings.yAuto
        root.yMinDbUser = uiSettings.yMinDbUser
        root.yMaxDbUser = uiSettings.yMaxDbUser
        root.rxFftEnabled = uiSettings.rxFftEnabled
        root.adaptiveQualityEnabled = uiSettings.adaptiveQualityEnabled
        root.renderQualityLevel = Math.max(0, Math.min(2, Number(uiSettings.renderQualityLevel)))

        // กันค่าพัง
        if (root.yMaxDbUser <= root.yMinDbUser + 1)
            root.yMaxDbUser = root.yMinDbUser + 1

        // New logical CH1 is the shared Home/RX source. No setAdcChannel is
        // emitted here; physical DF selection remains untouched until CH2..CH6.
        root.selectDisplayChannel(1)
        root._rxPendingFrame = root.rxFftEnabled
        root.publishSelectedFrame("completed")
    }

    Component.onDestruction: {
        var m = root.mw()
        if (m && m.setDoaRxSpectrumActive !== undefined)
            m.setDoaRxSpectrumActive(false)
    }

    function saveDbSettings() {
        // กัน user ตั้งผิด
        if (root.yMaxDbUser <= root.yMinDbUser + 1)
            root.yMaxDbUser = root.yMinDbUser + 1

        // ✅ เขียนกลับ Settings
        uiSettings.yAuto = root.yAuto
        uiSettings.yMinDbUser = root.yMinDbUser
        uiSettings.yMaxDbUser = root.yMaxDbUser
    }

    // ถ้าเปลี่ยนจาก code ก็ save
    onYAutoChanged: saveDbSettings()
    onYMinDbUserChanged: saveDbSettings()
    onYMaxDbUserChanged: saveDbSettings()
    onRxFftEnabledChanged: {
        uiSettings.rxFftEnabled = root.rxFftEnabled
        syncRxConsumer()
    }
    onAdaptiveQualityEnabledChanged: uiSettings.adaptiveQualityEnabled = root.adaptiveQualityEnabled
    onRenderQualityLevelChanged: {
        var q = Math.max(0, Math.min(2, Number(root.renderQualityLevel)))
        if (root.renderQualityLevel !== q) {
            root.renderQualityLevel = q
            return
        }
        uiSettings.renderQualityLevel = q
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        anchors.topMargin: 60

        TopBar {
            id: top1
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            fftPlotTarget: fftPlot
            displayChannel: root.displayChannel
            rxSourceAvailable: root.rxSourceAvailable
            rxFftEnabled: root.rxFftEnabled
            onDisplayChannelRequested: root.selectDisplayChannel(channel)
            onRxFftEnabledRequested: root.setRxFftEnabled(enabled)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            // ================= DOA =================
            Rectangle {
                Layout.preferredWidth: 560
                Layout.fillHeight: true
                radius: 12
                color: "#071025"
                border.color: "#20304A"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Text {
                        text: "DOA (MUSIC Polar)"
                        color: "#E5E7EB"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    DoaPolarPlot {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        enabled: doaClient.doaEnabled
                        theta: doaClient.theta
                        spectrum: doaClient.spectrum
                        peakDeg: doaClient.doaDeg
                        conf: doaClient.confidence
                        signalPresent: doaClient.signalPresent
                        sigPower: doaClient.doaSigPower
                        bandPeakDb: doaClient.bandPeakDb
                        gateThDb: doaClient.gateThDb
                        paintFps: root._polarFps()
                    }
                }
            }

            // ================= FFT + WATERFALL =================
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: "#071025"
                border.color: "#20304A"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Text {
                        text: root.spectrumTitle()
                        color: "#E5E7EB"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    // ===== Controls: Y range =====
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        CheckBox {
                            text: "Auto Y"
                            checked: root.yAuto
                            onToggled: root.yAuto = checked   // ✅ จะไป saveDbSettings() เอง
                        }

                        Text { text: "Min dB"; color: "#94A3B8"; verticalAlignment: Text.AlignVCenter }

                        SpinBox {
                            from: -200; to: 0
                            value: root.yMinDbUser
                            enabled: !root.yAuto

                            // ✅ ใช้ onValueModified = user เปลี่ยนจริงเท่านั้น
                            onValueModified: root.yMinDbUser = value
                        }

                        Text { text: "Max dB"; color: "#94A3B8"; verticalAlignment: Text.AlignVCenter }

                        SpinBox {
                            from: -200; to: 0
                            value: root.yMaxDbUser
                            enabled: !root.yAuto
                            onValueModified: root.yMaxDbUser = value
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: root.yAuto
                                  ? ("AUTO (" + fftPlot._mmin.toFixed(1) + " .. " + fftPlot._mmax.toFixed(1) + " dB)")
                                  : ("MANUAL (" + root.yMinDbUser + " .. " + root.yMaxDbUser + " dB)")
                            color: "#AAB7D1"
                            font.pixelSize: 12
                        }

                        Text {
                            text: "Q " + root._qualityName().toUpperCase()
                                  + " S" + root._spectrumFps()
                                  + "/W" + root._waterfallFps()
                            color: root.adaptiveQualityEnabled ? "#86EFAC" : "#FBBF24"
                            font.pixelSize: 12
                        }
                    }

                    FftPlot {
                        id: fftPlot
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: parent.height * 0.55

                        enabled: root.rxDisplaySelected ? (root.rxSourceAvailable && root.rxFftEnabled) : doaClient.spectrumEnabled
                        freqHz: root.displayFftFreqHz
                        magDb: root.displayFftMagDb
                        fftFps: root._spectrumFps()
                        frameSequence: root.displayFrameSequence
                        nativeRenderEnabled: true

                        // DOA target overlay belongs to the DF domain only. CH1/RX
                        // keeps the same renderer but does not pretend its wide RX
                        // axis is a DF-target interaction domain.
                        bandCenterHz: root.rxDisplaySelected ? 0 : (doaClient.fcHz + doaClient.doaOffsetHz)
                        bandBwHz: root.rxDisplaySelected ? 0 : doaClient.doaBwHz

                        yAuto: root.yAuto
                        yMinDb: root.yMinDbUser
                        yMaxDb: root.yMaxDbUser

                        clickOffsetEnabled: !root.rxDisplaySelected
                        baseFcHz: root.rxDisplaySelected ? root.rxAxisCenterHz : doaClient.fcHz
                        centerGuardHz: 1000

                        // ✅ ให้ component คุมเอง เพื่อ auto shift range ตามคลิก
                        offsetMinHz: NaN
                        offsetMaxHz: NaN
                        offsetRangeAuto: true
                        offsetRangeSpanHz: doaClient.doaBwHz

                        showOffsetMarker: !root.rxDisplaySelected

                        onOffsetRequested: {
                            console.log("[FftPlot] newOffsetHz=", newOffsetHz, " actualHz=", clickedHz)
                        }
                        onOffsetRangeChanged: {
                            console.log("[FftPlot] autoRange min=", newMinHz, " max=", newMaxHz)
                        }
                    }
                    // ===== Waterfall =====
                    WaterfallCanvas {
                        id: wf
                        Layout.fillWidth: true
                        Layout.preferredHeight: parent.height * 0.35

                        enabled: root.rxDisplaySelected ? (root.rxSourceAvailable && root.rxFftEnabled) : doaClient.spectrumEnabled
                        waterfallRowDb: root.displayFftMagDb
                        frameSequence: root.displayFrameSequence
                        sourceKey: root.logicalSourceName()

                        autoDb: root.yAuto
                        minDb: root.yMinDbUser
                        maxDb: root.yMaxDbUser

                        padLeft:  fftPlot.padLeft
                        padRight: fftPlot.padRight

                        wfFps: root._waterfallFps()
                        rowHeightPx: 1
                        showDebug: false
                        nativeRenderEnabled: true

                        waterfallColors: [
                            0x000004, 0x02021E, 0x04043A, 0x060656, 0x080872,
                            0x0A0A8E, 0x0C0CAA, 0x0E0EC6,
                            0x0030E0, 0x0060FF, 0x0090FF, 0x00C0FF, 0x00FFFF,
                            0x00FFB0, 0x00FF60, 0x40FF00, 0xA0FF00, 0xFFFF00,
                            0xFFB000, 0xFF8000, 0xFF4000, 0xFF0000,
                            0xFF8080, 0xFFFFFF
                        ]
                    }
                }
            }
        }
    }
}
