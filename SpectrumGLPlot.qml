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

    // Page lifecycle gate. When false, C++ drops FFT frame types 1/3 before
    // decode/boxing while audio frame types 2/4 continue uninterrupted.
    property bool runtimeActive: false
    property bool runtimeInitialized: false

    // Virtual zoom width only. Render surfaces stay fixed at root.width so
    // zoom never allocates 2x..20x Canvas/FBO backing stores.
    property real plotWidth: root.width
    readonly property real zoomFactor: Math.max(1.0, plotWidth / Math.max(1.0, root.width))
    property real viewPanRatio: 0.0
    readonly property real visibleSpanHz: Math.max(1.0, sampRate / zoomFactor)
    readonly property real fullStartFreq: centerFreq - sampRate / 2.0
    readonly property real viewStartFreq: fullStartFreq
                                              + Math.max(0.0, sampRate - visibleSpanHz)
                                                * Math.max(0.0, Math.min(1.0, viewPanRatio))
    readonly property real viewStopFreq: viewStartFreq + visibleSpanHz

    property real dataCount : 0
    property real smeterLevel: -100
    property var  spectrumData: []
    // Keep only the newest waterfall FFT row. The painted Canvas already owns
    // the visual history, so retaining one JS array per historical row wastes RAM.
    property var  latestWaterfallLine: []
    property var  waterfallColorMap: [
        0x000008, 0x000040, 0x0000A0, 0x0030FF,
        0x00A0FF, 0x00E0FF, 0x00FF80, 0x80FF00,
        0xFFFF00, 0xFF8000, 0xFF2000, 0xFF0000, 0xFFFFFF
    ]
    // Prebuilt CSS colors avoid allocating "rgb(r,g,b)" strings per FFT bin/frame.
    property var  waterfallCssPalette: []

    // Phase 4 runtime budgets. Full-span FFT is preserved; only UI delivery and
    // rendering cadence are bounded to avoid saturating one CPU core.
    property int waterfallTargetFps: 20
    property int waterfallMaxColumns: 1280
    property int offsetCommandIntervalMs: 20
    property real pendingOffsetCommand: 0
    property bool offsetCommandPending: false
    property bool offsetRecenterInProgress: false

    // Max Hold is accumulated in C++ on every FFT frame and published to QML
    // at a lower display rate. Peak Scan requests an exact native snapshot.
    property var maxHoldDisplayData: []

    property real smeterBuffered: smeterLevel

    // Single FFT delivery from C++. The same full-span frame feeds Spectrum and
    // Waterfall, cutting duplicate Qt->QML signal traffic per FFT frame.
    signal fftFrameUpdated(var frame)

    // Legacy local signals retained for source compatibility with older helpers.
    signal spectrumUpdated(var spectrum)
    signal waterfallUpdated(var line)
    signal waterfallColorUpdate(var colors)

    property var  maxHoldKept: []
    property var  peakVal: []
    property bool rfScannerInterlock: false

    property real centerFreq: mainWindows.center_freq()    // Hz
    property int  sampRate:   mainWindows.samp_rate()      // Hz

    property real waterfallMinDb: -130
    property real waterfallMaxDb: -80
    property real waterfallMax: -80
    property real waterfallMin: -130
    property alias autoScaleTimer: autoScaleTimer

    // DSP receiver offset. Seed from the latest server snapshot instead
    // of assuming zero, otherwise QML initially displays RF center only.
    property real offsetFrequency: Number(mainWindows.start_offset_freq())      // Hz
    property bool backendFrequencySync: false
    // Out-of-span tuning is a request/confirmation transaction. QML never
    // declares a new RF center before AstraRX confirms the hardware tune.
    property bool centerTunePending: false
    property real pendingCenterHz: 0
    property string pendingCenterReason: ""
    property int  bandwidth: high_cut - low_cut
    property int  low_cut:  bwModel.get(scanBwSelected).low_cut
    property int  high_cut: bwModel.get(scanBwSelected).high_cut
    property int  offsetSnapStep: 100     // Hz

    property bool autoScaleEnabled: true
    property bool autoScaleInitialized: false
    property real autoScaleAlpha: 0.18

    property int  sampRateMin: 50000
    property int  sampRateMax: 24.576e6
    property real zoomStep: root.width
    property string start_mod: ""
    property real xPos: 0
    property real setCenterFreq: centerFreq

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
        onTriggered: {
            if (root.runtimeActive)
                spectrumCanvas.requestPaint()
        }
    }

    function scheduleSpectrumPaint() {
        if (!root.runtimeActive)
            return
        if (!spectrumPaintTimer.running)
            spectrumPaintTimer.start()
    }

    // Latest-frame-wins Waterfall cadence. 25 FPS is visually continuous while
    // cutting full-surface scroll/composite frequency versus the previous 30 FPS.
    Timer {
        id: waterfallPaintTimer
        interval: Math.max(16, Math.round(1000 / Math.max(1, root.waterfallTargetFps)))
        repeat: false
        onTriggered: {
            if (root.runtimeActive && latestWaterfallLine && latestWaterfallLine.length)
                waterfallCanvas.requestPaint()
        }
    }

    function scheduleWaterfallPaint() {
        if (!root.runtimeActive)
            return
        if (!waterfallPaintTimer.running)
            waterfallPaintTimer.start()
    }

    function rebuildWaterfallCssPalette(colors) {
        waterfallColorMap = colors || []

        var css = []
        const count = waterfallColorMap.length
        for (var i = 0; i < count; ++i) {
            const rgb = Number(waterfallColorMap[i]) || 0
            const r = (rgb >> 16) & 0xFF
            const g = (rgb >> 8) & 0xFF
            const b = rgb & 0xFF
            css.push("rgb(" + r + "," + g + "," + b + ")")
        }

        if (css.length === 0)
            css.push("rgb(0,0,0)")

        waterfallCssPalette = css
    }

    function nativeMaxHoldAvailable() {
        return typeof wsClient !== "undefined"
                && wsClient
                && wsClient.setMaxHoldEnabled
                && wsClient.resetMaxHold
                && wsClient.maxHoldSnapshot
    }

    function syncNativeMaxHoldState() {
        if (!nativeMaxHoldAvailable())
            return

        const needed = root.runtimeActive
                && (spectrumCanvas.showMaxHold || peakScan.running)
        wsClient.setMaxHoldEnabled(needed)
    }

    function resetNativeMaxHold() {
        maxHoldDisplayData = []
        maxHoldKept = []
        if (nativeMaxHoldAvailable())
            wsClient.resetMaxHold()
    }

    function peakScanMaxHoldSnapshot() {
        if (nativeMaxHoldAvailable()) {
            const snapshot = wsClient.maxHoldSnapshot()
            return snapshot && snapshot.length ? snapshot : []
        }
        return maxHoldDisplayData && maxHoldDisplayData.length
                ? maxHoldDisplayData.slice() : []
    }

    Connections {
        target: (typeof wsClient !== "undefined") ? wsClient : null
        ignoreUnknownSignals: true

        function onMaxHoldUpdated(data) {
            if (!root.runtimeActive)
                return
            root.maxHoldDisplayData = data || []
            if (spectrumCanvas.showMaxHold)
                root.scheduleSpectrumPaint()
        }
    }

    Connections {
        target: root

        function onFftFrameUpdated(frame) {
            if (!root.runtimeActive || !frame || frame.length < 2)
                return

            // One frame object is reused by both render paths. Spectrum and
            // Waterfall remain full-span; no bandwidth crop is introduced.
            spectrumData = frame
            latestWaterfallLine = frame
            root.scheduleSpectrumPaint()
            root.scheduleWaterfallPaint()
        }
    }

    function syncFftRuntime() {
        if (!runtimeInitialized)
            return

        if (typeof wsClient !== "undefined" && wsClient && wsClient.setFftUiActive)
            wsClient.setFftUiActive(runtimeActive)

        if (runtimeActive) {
            syncNativeMaxHoldState()
            if (spectrumGridCanvas)
                spectrumGridCanvas.invalidate()
            return
        }

        if (nativeMaxHoldAvailable())
            wsClient.setMaxHoldEnabled(false)

        // Stop UI-owned realtime work when this page is not active.
        spectrumPaintTimer.stop()
        waterfallPaintTimer.stop()
        scanTimer.stop()
        zoomNavTimer.stop()
        zoomTimer.stop()
        spectrumCanvas.clearPeakTimer.stop()

        scanning = false

        // Peak scan depends on FFT/max-hold. Stop it cleanly instead of letting
        // its timers continue with stale data after the FFT gate closes.
        if (peakScan.running)
            peakScan.stopRange()

        // Release large QML/JS-side FFT buffers. The next active frame repopulates them.
        spectrumData = []
        latestWaterfallLine = []
        maxHoldDisplayData = []
        maxHoldKept = []
        spectrumCanvas.clearPeaks()
    }

    onRuntimeActiveChanged: syncFftRuntime()

    // ===== Timer ที่ใช้แทน for loop =====
    Timer {
        id: scanTimer
        // 20 ms keeps scan progression responsive while avoiding 200 QML
        // timer/property-notification cycles per second. DSP commands remain
        // latest-value coalesced by offsetCommandTimer.
        interval: 20
        repeat: true
        running: false
        onTriggered: {
            offsetFrequency = currentOffset
            currentOffset += offsetStep
            if (currentOffset > offsetStop) stopScan()
        }
    }

    // Coalesce rapid offset changes (scan timer and touch drag) into one DSP
    // command every ~20 ms. The scan progression remains unchanged at 5 ms;
    // only redundant JSON/WebSocket/backend traffic is bounded.
    Timer {
        id: offsetCommandTimer
        interval: root.offsetCommandIntervalMs
        repeat: false
        onTriggered: root.flushOffsetCommand()
    }

    Timer {
        id: centerTuneTimeout
        interval: 3000
        repeat: false
        onTriggered: {
            if (!root.centerTunePending)
                return
            console.warn("[QML-FREQ-RECENTER-TIMEOUT]",
                         "requested=", root.pendingCenterHz,
                         "confirmedCenter=", mainWindows.center_freq(),
                         "confirmedReceiver=", mainWindows.receiver_freq())
            root.centerTunePending = false
            root.pendingCenterHz = 0
            root.pendingCenterReason = ""
            root.applyBackendReceiverState(Number(mainWindows.center_freq()),
                                           Number(mainWindows.start_offset_freq()),
                                           Number(mainWindows.receiver_freq()))
        }
    }

    function sendOffsetCommandNow(value) {
        mainWindows.sendmessage('{"type":"dspcontrol","client_source":"SpectrumGLPlot.offset","params":{"offset_freq":' + Math.round(value) + '}}')
        mainWindows.updateCurrentOffsetFreq(Math.round(value), centerFreq)
    }

    function queueOffsetCommand(value) {
        pendingOffsetCommand = value
        offsetCommandPending = true
        if (!offsetCommandTimer.running)
            offsetCommandTimer.start()
    }

    function flushOffsetCommand() {
        if (!offsetCommandPending)
            return
        const value = pendingOffsetCommand
        offsetCommandPending = false
        sendOffsetCommandNow(value)
    }

    function startScan() {
        if (!runtimeActive || scanning) return
        currentOffset = offsetStart
        scanning = true
        scanTimer.start()
    }

    function stopScan() {
        scanning = false
        scanTimer.stop()
        if (offsetCommandPending)
            flushOffsetCommand()
    }

    onBandwidthChanged: {
        console.log("Bandwidth:", bandwidth, low_cut, high_cut)
        if (runtimeActive)
            overlayCanvas.requestPaint()
    }
    onLow_cutChanged: { if (runtimeActive) overlayCanvas.requestPaint() }
    onHigh_cutChanged: { if (runtimeActive) overlayCanvas.requestPaint() }

    onStart_modChanged: {
        let idx = getReceiverIndex(start_mod);
        if (idx !== -1) scanReceiverModeSelected = idx;
        currentModIndex = scanReceiverModeSelected
    }

    function usableOffsetLimitHz() {
        const halfSpan = Math.max(0.0, Number(sampRate) / 2.0)
        // Match AstraRX frequency_in_active_span(): reserve half of the active
        // demod bandwidth at each IQ edge. For a 7.680 MHz source this is
        // nominally +/-3.840 MHz minus the demod guard.
        const demodBandwidth = Math.abs((Number(high_cut) || 0) - (Number(low_cut) || 0))
        const guard = Math.max(0.0, demodBandwidth / 2.0)
        return Math.max(0.0, halfSpan - guard)
    }

    function requestSourceCenter(targetHz, reason) {
        const target = Math.round(Number(targetHz))
        if (!isFinite(target) || target <= 0)
            return

        // Coalesce the exact same outstanding request. A different target may
        // replace it; AstraRX remains authoritative and confirms via config.
        if (centerTunePending && Math.abs(pendingCenterHz - target) <= 0.5)
            return

        offsetCommandTimer.stop()
        offsetCommandPending = false
        centerTunePending = true
        pendingCenterHz = target
        pendingCenterReason = reason || "out_of_span"

        // Show the operator's requested receiver immediately, but do NOT mutate
        // centerFreq/offsetFrequency. Those remain the last confirmed AstraRX state.
        freqScan = target
        updateFrequency()

        const msg = '{"type":"setfrequency","client_source":"SpectrumGLPlot.'
                  + pendingCenterReason
                  + '","params":{"frequency":' + target + ',"key":"memagic"}}'
        console.log("[QML-FREQ-RECENTER-REQUEST]",
                    "confirmedCenter=", centerFreq,
                    "requestedCenter=", target,
                    "span=", sampRate,
                    "reason=", pendingCenterReason)
        mainWindows.sendmessage(msg)
        centerTuneTimeout.restart()
    }

    function handleFrequencyTuneError(message) {
        if (!centerTunePending)
            return

        console.warn("[QML-FREQ-RECENTER-ERROR]",
                     "requested=", pendingCenterHz,
                     "error=", message)
        centerTuneTimeout.stop()
        centerTunePending = false
        pendingCenterHz = 0
        pendingCenterReason = ""
        applyBackendReceiverState(Number(mainWindows.center_freq()),
                                  Number(mainWindows.start_offset_freq()),
                                  Number(mainWindows.receiver_freq()))
    }

    onOffsetFrequencyChanged: {
        if (runtimeActive)
            overlayCanvas.requestPaint()

        if (offsetRecenterInProgress)
            return

        // Server confirmation/state replay: update visuals only. Never echo a
        // DSP command back to AstraRX from a backend-owned assignment.
        if (backendFrequencySync) {
            freqScan = centerFreq + offsetFrequency
            updateFrequency()
            mainWindows.updateCurrentOffsetFreq(Math.round(offsetFrequency), centerFreq)
            if (runtimeActive)
                root.scheduleSpectrumPaint()
            console.log("[QML-FREQ-BACKEND]",
                        "center=", centerFreq,
                        "offset=", offsetFrequency,
                        "receiver=", freqScan)
            return
        }

        const receiverHz = centerFreq + offsetFrequency
        const limitHz = usableOffsetLimitHz()

        if (Math.abs(offsetFrequency) > limitHz + 0.5) {
            // Do not optimistically change centerFreq. Restore the confirmed DSP
            // offset locally and ask AstraRX to move the Active RF Source center.
            const targetHz = receiverHz
            offsetRecenterInProgress = true
            backendFrequencySync = true
            offsetFrequency = Number(mainWindows.start_offset_freq()) || 0
            backendFrequencySync = false
            offsetRecenterInProgress = false
            requestSourceCenter(targetHz, "out_of_span")
            return
        }

        // Normal in-span tuning: RF center stays fixed; only the per-client DSP
        // offset moves. Update the readout immediately before network feedback.
        centerTuneTimeout.stop()
        centerTunePending = false
        pendingCenterHz = 0
        pendingCenterReason = ""
        freqScan = receiverHz
        updateFrequency()
        mainWindows.updateCurrentOffsetFreq(Math.round(offsetFrequency), centerFreq)
        queueOffsetCommand(offsetFrequency)

        console.log("[QML-FREQ-LOCAL]",
                    "center=", centerFreq,
                    "offset=", offsetFrequency,
                    "limit=", limitHz,
                    "receiver=", freqScan)
    }

    onCenterFreqChanged: {
        if (runtimeActive) {
            spectrumCanvas.clearPeakTimer.start()
            overlayCanvas.requestPaint()
        }
        if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
    }

    onSetCenterFreqChanged: {
        if (setCenterFreq !== centerFreq) {
            mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":' + setCenterFreq + ',"key":"memagic"}}')
        }
        if (runtimeActive)
            spectrumCanvas.clearPeakTimer.start()
        if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
    }

    onSampRateChanged: {
        if (runtimeActive) {
            scheduleSpectrumPaint()
            waterfallCanvas.requestPaint()
            overlayCanvas.requestPaint()
        }
        if (spectrumGridCanvas) spectrumGridCanvas.invalidate()
    }

    onWaterfallMinDbChanged: { if (spectrumGridCanvas) spectrumGridCanvas.invalidate() }
    onWaterfallMaxDbChanged: { if (spectrumGridCanvas) spectrumGridCanvas.invalidate() }

    onWidthChanged: {
        // Keep the virtual zoom factor stable across window-size changes without
        // ever resizing render surfaces beyond the visible page width.
        if (plotWidth < width)
            plotWidth = width
        Qt.callLater(syncZoomNavFromRatio)
        invalidateViewport(true)
    }

    onViewPanRatioChanged: {
        if (runtimeActive)
            invalidateViewport(true)
    }

    Component.onCompleted: {
        runtimeInitialized = true
        console.log("[ASTRARX-COMPAT-QML] revision=20260817-bidirectional-span-stability-r10")

        mainWindows.updateCenterFreq.connect(updateCenterFreq)
        mainWindows.updateReceiverFreq.connect(applyBackendReceiverState)
        mainWindows.frequencyTuneError.connect(handleFrequencyTuneError)

        mainWindows.fftFrameUpdated.connect(fftFrameUpdated)
        mainWindows.waterfallColorUpdate.connect(waterfallColorUpdate)

        mainWindows.findBandsWithProfile.connect(findBandsWithProfile)

        mainWindows.smeterValueUpdated.connect(function(smeter) {
            smeterValueUpdated(smeter)
        })

        if (spectrumGridCanvas && runtimeActive) spectrumGridCanvas.invalidate()

        // Pull the current receiver snapshot after every signal is connected.
        // This closes the startup race where AstraRX config can arrive before
        // SpectrumGLPlot exists.
        applyBackendReceiverState(Number(mainWindows.center_freq()),
                                  Number(mainWindows.start_offset_freq()),
                                  Number(mainWindows.receiver_freq()))

        // Prime the fallback palette before the first server color message.
        rebuildWaterfallCssPalette(waterfallColorMap)

        // Enable the C++ FFT path only after all QML signal handlers are connected.
        syncFftRuntime()
    }

    Component.onDestruction: {
        offsetCommandTimer.stop()
        centerTuneTimeout.stop()
        offsetCommandPending = false
        if (nativeMaxHoldAvailable())
            wsClient.setMaxHoldEnabled(false)
        if (typeof wsClient !== "undefined" && wsClient && wsClient.setFftUiActive)
            wsClient.setFftUiActive(false)
    }

    function setOffset(freqOffset) {
        offsetFrequency = freqOffset;
    }

    function setManualOffset(freq) {
        const targetHz = Number(freq)
        if (!isFinite(targetHz) || targetHz <= 0)
            return

        const freqOffset = targetHz - centerFreq
        if (Math.abs(freqOffset) > usableOffsetLimitHz() + 0.5) {
            requestSourceCenter(targetHz, "manual_out_of_span")
            return
        }

        offsetFrequency = freqOffset
        overlayCanvas.requestPaint()
        scheduleSpectrumPaint()
    }

    function clamp01(value) {
        return Math.max(0.0, Math.min(1.0, Number(value) || 0.0))
    }

    function syncZoomNavFromRatio() {
        if (!zoomNav || !zoomNav.rectangle)
            return

        const travel = Math.max(0.0, zoomNav.width - zoomNav.rectangle.width)
        zoomNav.rectangle.x = travel > 0.0 ? clamp01(viewPanRatio) * travel : 0.0
    }

    function updateViewPanFromNav() {
        if (!zoomNav || !zoomNav.rectangle)
            return

        const travel = Math.max(0.0, zoomNav.width - zoomNav.rectangle.width)
        const nextRatio = travel > 0.0 ? zoomNav.rectangle.x / travel : 0.0
        const clamped = clamp01(nextRatio)
        if (Math.abs(clamped - viewPanRatio) > 0.000001)
            viewPanRatio = clamped
    }

    function invalidateViewport(clearWaterfallHistory) {
        scheduleSpectrumPaint()
        if (overlayCanvas)
            overlayCanvas.requestPaint()
        if (spectrumGridCanvas)
            spectrumGridCanvas.invalidate()

        if (clearWaterfallHistory && waterfallCanvas) {
            waterfallCanvas.clearBeforeNextPaint = true
            if (runtimeActive)
                waterfallCanvas.requestPaint()
        }
    }

    function zoomIn() {
        plotWidth += zoomStep
        if (plotWidth > root.width * 20)
            plotWidth = root.width * 20
        applyZoom()
    }

    function zoomOut() {
        plotWidth -= zoomStep
        if (plotWidth < root.width)
            plotWidth = root.width
        applyZoom()
    }

    function applyZoom() {
        viewPanRatio = clamp01(viewPanRatio)
        Qt.callLater(syncZoomNavFromRatio)
        zoomNav.rectangle.opacity = 1
        invalidateViewport(true)
    }

    function smeterValueUpdated(smeter) { smeterLevel = smeter }

    function updateWaterfallLevels(minDb, maxDb) {
        // Server ranges are useful as a manual fallback, but automatic contrast
        // deliberately owns the range while enabled.
        if (autoScaleEnabled)
            return

        waterfallMinDb = Number(minDb)
        waterfallMaxDb = Number(maxDb)
        waterfallScaleControl.waterfallMinDb = waterfallMinDb
        waterfallScaleControl.waterfallMaxDb = waterfallMaxDb
        if (spectrumGridCanvas)
            spectrumGridCanvas.invalidate()
    }

    function updateCenterFreq() {
        // Center/source metadata only. Receiver readout ownership lives in
        // applyBackendReceiverState(), so an RF-center refresh can never force
        // freqScan back to center.
        sampRate = Number(mainWindows.samp_rate())
        centerFreq = Number(mainWindows.center_freq())
        sampRateMax = sampRate
        start_mod = mainWindows.start_mod()

        if (runtimeActive) {
            overlayCanvas.requestPaint()
            scheduleSpectrumPaint()
        }

        if (spectrumGridCanvas)
            spectrumGridCanvas.invalidate()

        console.log("[QML-CENTER-SYNC]",
                    "center=", centerFreq,
                    "offset=", offsetFrequency,
                    "display=", freqScan)
    }

    function applyBackendReceiverState(centerHz, offsetHz, receiverHz) {
        var c = Number(centerHz)
        var o = Number(offsetHz)
        var r = Number(receiverHz)

        if (!isFinite(c))
            c = Number(mainWindows.center_freq()) || 0
        if (!isFinite(o))
            o = Number(mainWindows.start_offset_freq()) || 0
        if (!isFinite(r) || r <= 0)
            r = c + o

        const previousCenter = Number(centerFreq)
        const wasPending = centerTunePending

        backendFrequencySync = true
        centerFreq = c
        sampRate = Number(mainWindows.samp_rate())
        sampRateMax = sampRate
        start_mod = mainWindows.start_mod()

        // Defensive mirror of the AstraRX policy. R10 server normally sends an
        // already-valid receiver snapshot. If an older/partial backend snapshot
        // arrives with a receiver outside the new source span, QML follows the
        // confirmed AstraRX RF center instead of drawing an impossible cursor.
        const backendLimitHz = usableOffsetLimitHz()
        if (Math.abs(r - c) > backendLimitHz + 0.5) {
            console.warn("[QML-FREQ-ASTRARX-PUSH-CLAMP]",
                         "center=", c,
                         "receiver=", r,
                         "limit=", backendLimitHz)
            o = 0
            r = c
        } else {
            o = r - c
        }
        offsetFrequency = o
        backendFrequencySync = false

        if (!wasPending && isFinite(previousCenter) && Math.abs(c - previousCenter) > 0.5) {
            console.log("[QML-FREQ-ASTRARX-PUSH]",
                        "centerBefore=", previousCenter,
                        "centerAfter=", c,
                        "offset=", o,
                        "receiver=", r,
                        "limit=", backendLimitHz)
        }

        // A center retune is complete only when AstraRX publishes its new
        // authoritative center. Any backend receiver snapshot still wins the UI.
        if (centerTunePending && Math.abs(c - pendingCenterHz) <= 0.5) {
            console.log("[QML-FREQ-RECENTER-CONFIRMED]",
                        "center=", c,
                        "receiver=", r,
                        "reason=", pendingCenterReason)
            centerTuneTimeout.stop()
            centerTunePending = false
            pendingCenterHz = 0
            pendingCenterReason = ""
        }

        // Absolute receiver frequency from AstraRX wins when available.
        freqScan = r
        updateFrequency()
        mainWindows.updateCurrentOffsetFreq(Math.round(o), c)

        if (runtimeActive) {
            overlayCanvas.requestPaint()
            scheduleSpectrumPaint()
        }
        if (spectrumGridCanvas)
            spectrumGridCanvas.invalidate()

        console.log("[QML-FREQ-SYNC]",
                    "center=", c,
                    "offset=", o,
                    "receiver=", r)
    }

    function percentile(sortedValues, fraction) {
        if (!sortedValues || sortedValues.length === 0)
            return NaN
        const idx = Math.max(0, Math.min(sortedValues.length - 1,
                                        Math.floor(fraction * (sortedValues.length - 1))))
        return Number(sortedValues[idx])
    }

    function autoScaleWaterfallColor() {
        var latestLine = latestWaterfallLine
        if (!latestLine || latestLine.length < 8)
            return

        // Sample a bounded number of bins to keep the sort cheap on Jetson.
        const stride = Math.max(1, Math.floor(latestLine.length / 512))
        var values = []
        for (var i = 0; i < latestLine.length; i += stride) {
            const v = Number(latestLine[i])
            if (isFinite(v))
                values.push(v)
        }
        if (values.length < 8)
            return

        values.sort(function(a, b) { return a - b })

        const noiseDb = percentile(values, 0.50)
        const strongDb = percentile(values, 0.995)
        if (!isFinite(noiseDb) || !isFinite(strongDb))
            return

        var targetMin = noiseDb - 5.0
        var targetMax = Math.max(noiseDb + 32.0, strongDb + 3.0)

        var span = targetMax - targetMin
        if (span < 35.0)
            targetMax = targetMin + 35.0
        else if (span > 65.0)
            targetMax = targetMin + 65.0

        targetMin = Math.max(-160.0, Math.min(0.0, targetMin))
        targetMax = Math.max(targetMin + 1.0, Math.min(10.0, targetMax))

        if (!autoScaleInitialized) {
            waterfallMinDb = targetMin
            waterfallMaxDb = targetMax
            autoScaleInitialized = true
        } else {
            waterfallMinDb += (targetMin - waterfallMinDb) * autoScaleAlpha
            waterfallMaxDb += (targetMax - waterfallMaxDb) * autoScaleAlpha
        }

        waterfallMin = waterfallMinDb
        waterfallMax = waterfallMaxDb
        waterfallScaleControl.waterfallMinDb = waterfallMinDb
        waterfallScaleControl.waterfallMaxDb = waterfallMaxDb

        if (spectrumGridCanvas)
            spectrumGridCanvas.invalidate()
    }

    Timer {
        id: autoScaleTimer
        interval: 500
        repeat: true
        running: root.runtimeActive && autoScaleEnabled
        onTriggered: {
            if (root.runtimeActive)
                autoScaleWaterfallColor()
        }
    }

    /* ============================================================
       ✅ Fix4: Cached grid canvas (X axis on TOP, works x86+Jetson)
       - DO NOT set visible:false (x86 often won't render => drawImage blank)
       ============================================================ */
    Canvas {
        id: spectrumGridCanvas
        x: spectrumCanvas.x
        y: spectrumCanvas.y
        width: spectrumCanvas.width
        height: spectrumCanvas.height

        // Real static layer: render grid only when frequency/range/size changes.
        // Spectrum no longer composites this full canvas every 33 ms.
        visible: true
        opacity: 1.0
        z: 0

        renderTarget: Canvas.FramebufferObject
        renderStrategy: Canvas.Immediate
        antialiasing: false
        smooth: false

        property bool ready: false

        function invalidate() {
            ready = false
            if (root.runtimeActive)
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

            // Draw only the logical viewport. Canvas width remains fixed,
            // while zoom/pan changes the visible frequency range.
            let startFreq = root.viewStartFreq
            let stopFreq  = root.viewStopFreq
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
            let pixelsPerHz = w / Math.max(1, root.visibleSpanHz)
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
        }
    }

    Canvas {
        id: spectrumCanvas
        z: 1
        // Fixed-size render surface: logical zoom changes bin mapping only.
        width: root.width
        height: parent.height / 4
        renderTarget: Canvas.FramebufferObject
        renderStrategy: Canvas.Cooperative
        antialiasing: false
        smooth: false
        x: 0

        property alias clearPeakTimer: clearPeakTimer
        property bool showMaxHold: true

        onShowMaxHoldChanged: root.syncNativeMaxHoldState()

        function clearPeaks() {
            root.resetNativeMaxHold()
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

            const minDb = root.waterfallMinDb;
            const maxDb = root.waterfallMaxDb;
            const rangeDb = Math.max(1e-6, (maxDb - minDb));

            const xAxisH = 18
            const plotH = h - xAxisH

            function yOf(v) { return plotH - ((v - minDb) / rangeDb) * plotH; }

            // Map the full-span FFT into the current logical viewport.
            const fullBins = spectrumData.length
            const startRatio = Math.max(0.0, Math.min(1.0,
                (root.viewStartFreq - root.fullStartFreq) / Math.max(1.0, root.sampRate)))
            const stopRatio = Math.max(startRatio, Math.min(1.0,
                (root.viewStopFreq - root.fullStartFreq) / Math.max(1.0, root.sampRate)))
            const startBin = Math.max(0, Math.min(fullBins - 1,
                Math.floor(startRatio * (fullBins - 1))))
            const endBin = Math.max(startBin + 1, Math.min(fullBins - 1,
                Math.ceil(stopRatio * (fullBins - 1))))
            const visibleBins = Math.max(2, endBin - startBin + 1)

            // Spectrum line
            ctx.beginPath();
            ctx.strokeStyle = "#00FF00";
            ctx.lineWidth = 1;
            ctx.moveTo(0, yOf(spectrumData[startBin]));

            // Draw no more samples than the fixed display width needs.
            let step = Math.max(1, Math.floor(visibleBins / Math.max(1, w)))
            for (var i = startBin + step; i <= endBin; i += step) {
                let x = (i - startBin) / Math.max(1, endBin - startBin) * w;
                let y = yOf(spectrumData[i]);
                ctx.lineTo(x, y);
            }
            if ((endBin - startBin) % step !== 0)
                ctx.lineTo(w, yOf(spectrumData[endBin]))
            ctx.stroke();

            // Max Hold accumulation runs natively in WebSocketClient on every
            // FFT frame. QML only paints the throttled display snapshot, removing
            // the full FFT-sized JavaScript comparison loop from Canvas::onPaint.
            const maxHold = root.maxHoldDisplayData
            if (showMaxHold && maxHold && maxHold.length === spectrumData.length) {
                ctx.beginPath();
                ctx.strokeStyle = theme.maxHoldLine;
                ctx.lineWidth = 1;
                ctx.moveTo(0, yOf(maxHold[startBin]));

                let step2 = Math.max(1, Math.floor(visibleBins / Math.max(1, w)))
                for (var k = startBin + step2; k <= endBin; k += step2) {
                    let x2 = (k - startBin) / Math.max(1, endBin - startBin) * w;
                    let y2 = yOf(maxHold[k]);
                    ctx.lineTo(x2, y2);
                }
                if ((endBin - startBin) % step2 !== 0)
                    ctx.lineTo(w, yOf(maxHold[endBin]))
                ctx.stroke();
            }
        }

    }

    Canvas {
        id: waterfallCanvas
        z: 1
        y: spectrumCanvas.height
        x: 0
        width: root.width
        height: parent.height / 4
        property bool clearBeforeNextPaint: false

        onPaint: {
            const minDb = waterfallMinDb;
            const maxDb = waterfallMaxDb;
            const rangeDb = maxDb - minDb;
            var ctx = getContext("2d");

            if (clearBeforeNextPaint) {
                ctx.clearRect(0, 0, width, height)
                clearBeforeNextPaint = false
            } else {
                // Scroll down by 1px only for a new FFT paint.
                ctx.drawImage(waterfallCanvas, 0, 0, width, height - 1,
                              0, 1, width, height - 1);
            }

            var line = latestWaterfallLine;
            if (!line || typeof line.length === "undefined") return;

            const canvasWidth = Math.max(1, Math.floor(width));
            const bins = line.length;
            if (bins < 1)
                return;

            // Select only bins inside the logical viewport. The underlying FFT
            // remains full-span; this is display mapping only, not partial decode.
            const startRatio = Math.max(0.0, Math.min(1.0,
                (root.viewStartFreq - root.fullStartFreq) / Math.max(1.0, root.sampRate)))
            const stopRatio = Math.max(startRatio, Math.min(1.0,
                (root.viewStopFreq - root.fullStartFreq) / Math.max(1.0, root.sampRate)))
            const startBin = Math.max(0, Math.min(bins - 1,
                Math.floor(startRatio * (bins - 1))))
            const endBin = Math.max(startBin, Math.min(bins - 1,
                Math.ceil(stopRatio * (bins - 1))))
            const visibleBins = Math.max(1, endBin - startBin + 1)

            // Fixed-size renderer: no matter whether zoom is 1x or 20x, at most
            // one aggregation pass per screen column is performed.
            const columns = Math.max(1, Math.min(
                canvasWidth, visibleBins, root.waterfallMaxColumns));
            const palette = waterfallCssPalette.length > 0
                          ? waterfallCssPalette
                          : ["rgb(0,0,0)"];
            const paletteLast = palette.length - 1;
            const safeRangeDb = Math.max(1e-9, rangeDb);

            var runColor = -1;
            var runStartX = 0;

            function flushRun(endX) {
                if (runColor < 0 || endX <= runStartX)
                    return;
                ctx.fillStyle = palette[runColor];
                ctx.fillRect(runStartX, 0, endX - runStartX, 1);
            }

            for (var column = 0; column < columns; ++column) {
                const srcStart = startBin + Math.floor(column * visibleBins / columns);
                const srcEnd = Math.min(endBin + 1, Math.max(srcStart + 1,
                    startBin + Math.floor((column + 1) * visibleBins / columns)));

                var peakDb = Number(line[srcStart]);
                if (!isFinite(peakDb))
                    peakDb = minDb;

                for (var src = srcStart + 1; src < srcEnd; ++src) {
                    const sampleDb = Number(line[src]);
                    if (isFinite(sampleDb) && sampleDb > peakDb)
                        peakDb = sampleDb;
                }

                const clampedDb = Math.max(minDb, Math.min(maxDb, peakDb));
                const norm = (clampedDb - minDb) / safeRangeDb;
                const colorIndex = Math.max(0, Math.min(
                    paletteLast, Math.floor(norm * paletteLast)));

                const x0 = Math.floor(column * canvasWidth / columns);
                const x1 = Math.max(x0 + 1,
                                    Math.floor((column + 1) * canvasWidth / columns));

                if (runColor < 0) {
                    runColor = colorIndex;
                    runStartX = x0;
                } else if (colorIndex !== runColor) {
                    flushRun(x0);
                    runColor = colorIndex;
                    runStartX = x0;
                }

                if (column === columns - 1)
                    flushRun(Math.min(canvasWidth, x1));
            }
        }

        Connections {
            target: root
            function onWaterfallColorUpdate(colors) {
                root.rebuildWaterfallCssPalette(colors)
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

            let startFreq = root.viewStartFreq;
            let freqRange = root.visibleSpanHz;
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
                let startFreq = root.viewStartFreq;
                let freqRange = root.visibleSpanHz;

                let offsetFreqAbs = centerFreq + offsetFrequency;
                let targetX = ((offsetFreqAbs - startFreq) / freqRange) * canvasWidth;

                if (Math.abs(x - targetX) < 10) {
                    dragging = true;
                    dragX = x;
                } else {
                    let clickedFreq = startFreq + ((x - leftMargin) / (canvasWidth - leftMargin)) * freqRange;
                    let newOffset = clickedFreq - centerFreq;

                    newOffset = Math.round(newOffset / offsetSnapStep) * offsetSnapStep;
                    offsetFrequency = Math.max(-root.usableOffsetLimitHz(), Math.min(root.usableOffsetLimitHz(), newOffset));

                    overlayCanvas.requestPaint();
                    root.scheduleSpectrumPaint()
                }
            }

            onReleased: dragging = false

            onPositionChanged: {
                if (!dragging) return;

                let x = mouse.x;
                let deltaX = x - dragX;
                dragX = x;

                let deltaFreq = (deltaX / overlayCanvas.width) * root.visibleSpanHz;
                let newOffset = offsetFrequency + deltaFreq;

                newOffset = Math.round(newOffset / offsetSnapStep) * offsetSnapStep;
                offsetFrequency = Math.max(-root.usableOffsetLimitHz(), Math.min(root.usableOffsetLimitHz(), newOffset));

                overlayCanvas.requestPaint();
                root.scheduleSpectrumPaint()
            }
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
        visible: root.zoomFactor > 1.0001
        property alias rectangle: rectangle
        Rectangle {
            id: rectangle
            width: Math.max(8, root.width / root.zoomFactor)
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

                onReleased: root.updateViewPanFromNav()
                onClicked:
                    zoomNavTimer.restart()

            }
            onXChanged: {
                zoomNavTimer.restart()
                rectangle.opacity = 1
                // onViewPanRatioChanged performs the single coalesced viewport invalidation.
                root.updateViewPanFromNav()
            }

            onWidthChanged: {
                const maxX = Math.max(0, zoomNav.width - width)
                if (x > maxX)
                    x = maxX
                root.updateViewPanFromNav()
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
            viewPanRatio = 0.0
            zoomNav.rectangle.x = 0
            applyZoom()
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
            onManualScaleEdited: {
                root.autoScaleEnabled = false
                root.autoScaleInitialized = false
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
        onRunningChanged: root.syncNativeMaxHoldState()
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
            if (!root.runtimeActive) {
                console.log("[PeakScan] ignored while spectrum page is inactive")
                return
            }
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
                const data = root.peakScanMaxHoldSnapshot()
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
