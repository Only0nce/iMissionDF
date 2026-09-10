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
import iScan.Display 1.0
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
    // R20.4-SPECTRUM-A3: AstraRX WebV2-inspired analyzer palette.
    // The native renderer interpolates between stops for a smooth waterfall.
    property var  waterfallColorMap: [
        0x030712, 0x071B4D, 0x0A3B92, 0x0D72D8,
        0x12B6E8, 0x28D6C0, 0x54DC7B, 0xA6E34B,
        0xF0DE43, 0xF6A53A, 0xEF5B36, 0xE82F37, 0xFFF2D0
    ]

    // R20.4-SPECTRUM-CUDA1.8-READABILITY-PASS:
// Waterfall remains the dominant analyzer surface (58%). Persistent bottom HUD
// controls now use separate low-opacity gray cards (not one large pod), while
// the data-driven legend grows vertically and SCAN/MEMORY live directly below
// it as a compact right-side vertical toggle rail.
// R20.4-SPECTRUM-CUDA1.3-FLOATING-WORKSPACE:
    // Spectrum/Waterfall now own the full receiver workspace. Scan/Memory are
    // presented as a non-modal floating workspace above the live analyzer, so
    // the CUDA/FPS60 renderer is never resized, destroyed, or recreated when
    // the operator opens folders/results.
    readonly property real analyzerHeight: height
    readonly property real spectrumRatio: 0.42
    readonly property color analyzerBackground: "#081018"
    readonly property color analyzerPanel: "#0D1721"
    readonly property color analyzerBorder: "#263948"
    readonly property color analyzerAccent: "#35D5BD"
    // CUDA1.11 visual polish: dedicated trace colors so Spectrum styling can
    // evolve without changing selection/HUD accent semantics.
    readonly property color spectrumLiveColor: "#49F3DD"
    readonly property color spectrumMaxHoldColor: "#FFBE4A"
    readonly property color analyzerText: "#EEF6FB"
    readonly property color analyzerMuted: "#91A4B3"
    // CUDA1.8 readability pass: dynamic Waterfall content can become very bright
    // (cyan/green/yellow), so card chrome must provide stable contrast while
    // text/controls remain fully opaque. Hover raises contrast smoothly rather
    // than changing content opacity.
    readonly property color waterfallHudCardColor: Qt.rgba(0.035, 0.055, 0.070, 0.72)
    readonly property color waterfallHudCardHoverColor: Qt.rgba(0.040, 0.070, 0.090, 0.86)
    readonly property color waterfallHudCardBorder: Qt.rgba(0.50, 0.68, 0.74, 0.28)
    readonly property color waterfallHudCardHoverBorder: Qt.rgba(0.43, 0.90, 0.83, 0.68)
    readonly property color hudPrimaryText: "#F4FBFF"
    readonly property color hudSecondaryText: "#D3E1E7"
    readonly property color hudAccentText: "#6EF2E8"
    readonly property real waterfallHudCardRadius: 8

    // Phase 4 runtime budgets. Full-span FFT is preserved; only UI delivery and
    // rendering cadence are bounded to avoid saturating one CPU core.
    property int offsetCommandIntervalMs: 20
    property real pendingOffsetCommand: 0
    property bool offsetCommandPending: false
    property bool offsetRecenterInProgress: false

    property real smeterBuffered: smeterLevel

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

    // R20.4-SPECTRUM-CUDA1.2: receiver-selection geometry is a first-class
    // QML state so the measurement HUD can remain physically attached to the
    // selected listening band without relying on Canvas paint side effects.
    readonly property real selectedReceiverHz: centerFreq + offsetFrequency
    readonly property real selectionLeftFreqHz: selectedReceiverHz + low_cut
    readonly property real selectionRightFreqHz: selectedReceiverHz + high_cut
    readonly property real selectionLeftX: ((selectionLeftFreqHz - viewStartFreq) / Math.max(1.0, visibleSpanHz)) * Math.max(1.0, width)
    readonly property real selectionRightX: ((selectionRightFreqHz - viewStartFreq) / Math.max(1.0, visibleSpanHz)) * Math.max(1.0, width)
    readonly property real selectionCenterX: ((selectedReceiverHz - viewStartFreq) / Math.max(1.0, visibleSpanHz)) * Math.max(1.0, width)
    readonly property bool selectionVisible: Math.max(selectionLeftX, selectionRightX) >= 0
                                             && Math.min(selectionLeftX, selectionRightX) <= width

    property bool selectionMetricsLocked: false
    readonly property real selectionMetricsPanelOpacity: selectionMetricsLocked ? 1.0 : 0.28
    readonly property real selectionMetricsMargin: 8
    readonly property real selectionMetricsEdgeMargin: 8

    // Floating Scan/Memory workspace. The existing folder/result components stay
    // alive for the lifetime of SpectrumGLPlot; opening/closing this panel changes
    // presentation only and therefore preserves scan state and waterfall history.
    property bool floatingWorkspaceOpen: false
    readonly property string floatingWorkspaceTitle: widgetView ? "SCAN WORKSPACE" : "MEMORY WORKSPACE"
    // CUDA1.4: reserve a dedicated HUD dock inside the Waterfall. The floating
    // workspace now stops above this dock instead of competing with bandwidth,
    // S-meter and intensity controls for the same bottom pixels.
    readonly property real waterfallHudDockHeight: 92
    readonly property real waterfallHudSideMargin: 10
    readonly property real waterfallRightToggleHeight: 108
    readonly property real waterfallRightToggleWidth: 96
    // Leave generous left/right Waterfall gutters for the WATERFALL label,
    // Pause/Clear actions and data-driven scale.  At 1920/1648-class layouts
    // this still leaves enough width for four scan-result cards + side actions.
    // CUDA1.9: the original 76% x 39% floating workspace was too tight for
    // the production Scan/Memory layouts (Back rail + always-on scrollbars +
    // right-side action column). Expand it while preserving the Waterfall HUD
    // and right utility rail as visible safe areas.
    readonly property real floatingWorkspaceSideSafe: waterfallRightToggleWidth + waterfallHudSideMargin + 18
    readonly property real floatingWorkspaceTopSafe: 28
    readonly property real floatingWorkspaceBottomMargin: waterfallHudDockHeight + 14
    readonly property real floatingWorkspaceWidth: Math.max(620,
        Math.min(root.width * 0.86, root.width - (floatingWorkspaceSideSafe * 2)))
    readonly property real floatingWorkspaceHeight: Math.max(360,
        Math.min(root.height * 0.56,
                 root.height - floatingWorkspaceBottomMargin - floatingWorkspaceTopSafe))

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
    // CUDA1.1/FPS60: presentation-side invalidations are no longer capped
    // at 30 Hz. Native FFT delivery is independently capped at ~60 Hz.
    // ============================================================
    Timer {
        id: spectrumPaintTimer
        interval: 16
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

    function openFloatingWorkspace(scanMode) {
        // Existing global contract: widgetView=true => Scan, false => Memory.
        widgetView = !!scanMode
        floatingWorkspaceOpen = true
    }

    function toggleFloatingWorkspace(scanMode) {
        const requestedScan = !!scanMode
        if (floatingWorkspaceOpen && widgetView === requestedScan) {
            floatingWorkspaceOpen = false
            return
        }
        openFloatingWorkspace(requestedScan)
    }

    function closeFloatingWorkspace() {
        floatingWorkspaceOpen = false
    }

    function rebuildWaterfallPalette(colors) {
        // Packed RGB values are consumed directly by the native renderer.
        waterfallColorMap = colors || []
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
        maxHoldKept = []
        if (nativeMaxHoldAvailable())
            wsClient.resetMaxHold()
    }

    function peakScanMaxHoldSnapshot() {
        if (nativeMaxHoldAvailable()) {
            const snapshot = wsClient.maxHoldSnapshot()
            return snapshot && snapshot.length ? snapshot : []
        }
        return []
    }

    // R20.4: Max Hold display is delivered directly from WebSocketClient to
    // FftDisplayItem as QVector<float>. Peak Scan still requests an exact
    // full-resolution snapshot only when the operator starts a scan.

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
        floatingWorkspaceOpen = false
        spectrumPaintTimer.stop()
        scanTimer.stop()
        zoomNavTimer.stop()
        zoomTimer.stop()
        spectrumCanvas.clearPeakTimer.stop()

        scanning = false

        // Peak scan depends on FFT/max-hold. Stop it cleanly instead of letting
        // its timers continue with stale data after the FFT gate closes.
        if (peakScan.running)
            peakScan.stopRange()

        // No FFT-sized JavaScript buffers exist in the production path.
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
        invalidateViewport()
    }

    onViewPanRatioChanged: {
        if (runtimeActive)
            invalidateViewport()
    }


    // R20.2 / R16 restore: no executable JavaScript .connect() lifetime bridges.
    Connections {
        target: mainWindows
        ignoreUnknownSignals: true

        function onUpdateCenterFreq() { updateCenterFreq() }
        function onUpdateReceiverFreq(centerHz, offsetHz, receiverHz) {
            applyBackendReceiverState(centerHz, offsetHz, receiverHz)
        }
        function onFrequencyTuneError(message) { handleFrequencyTuneError(message) }
        function onWaterfallColorUpdate(waterfallColors) { waterfallColorUpdate(waterfallColors) }
        function onFindBandsWithProfile(mode) { findBandsWithProfile(mode) }
        function onSmeterValueUpdated(smeter) { smeterValueUpdated(smeter) }
    }

    Component.onCompleted: {
        runtimeInitialized = true
        console.log("[ASTRARX-COMPAT-QML] revision=20260817-bidirectional-span-stability-r10")
        console.log("[R20.4-SPECTRUM-CUDA1.8-READABILITY-PASS] spectrum42=1 waterfall58=1 stableContrastHud=1 highContrastText=1 darkRightRail=1")
        console.log("[R20.4-SPECTRUM-CUDA1.11-SPECTRUM-VISUAL-POLISH] gradientFill=1 liveTrace=#49F3DD maxHold=#FFBE4A")

        if (spectrumGridCanvas && runtimeActive) spectrumGridCanvas.invalidate()

        // Pull the current receiver snapshot after every signal is connected.
        // This closes the startup race where AstraRX config can arrive before
        // SpectrumGLPlot exists.
        applyBackendReceiverState(Number(mainWindows.center_freq()),
                                  Number(mainWindows.start_offset_freq()),
                                  Number(mainWindows.receiver_freq()))

        // Prime the fallback palette before the first server color message.
        rebuildWaterfallPalette(waterfallColorMap)

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

    function invalidateViewport() {
        // AstraRX AB6 semantics: zoom/pan/resize are view transforms only.
        // The native waterfall keeps history on the full acquisition RF axis
        // and reprojects it at paint time, so history must never be cleared here.
        scheduleSpectrumPaint()
        if (waterfallCanvas && runtimeActive)
            waterfallCanvas.requestPaint()
        if (overlayCanvas)
            overlayCanvas.requestPaint()
        if (spectrumGridCanvas)
            spectrumGridCanvas.invalidate()
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
        invalidateViewport()
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

    function autoScaleWaterfallColor() {
        if (typeof wsClient === "undefined" || !wsClient || !wsClient.fftAutoScaleValid)
            return

        // R20.4: median and 99.5-percentile are computed natively from a
        // bounded sample. QML receives only two scalar values, not an FFT array.
        const noiseDb = Number(wsClient.fftNoiseDb)
        const strongDb = Number(wsClient.fftStrongDb)
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

    // ============================================================
    // AstraRX WebV2-inspired local analyzer surface. This rectangle is
    // confined to the existing Spectrum/Waterfall footprint only.
    // ============================================================
    Rectangle {
        id: analyzerBackdrop
        x: 0
        y: 0
        width: root.width
        height: root.analyzerHeight
        color: root.analyzerBackground
        border.color: root.analyzerBorder
        border.width: 1
        z: 0
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
        z: 1

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
                ctx.fillText(db.toFixed(0) + " dBFS ", 4, y - 2)
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

    FftDisplayItem {
        id: spectrumCanvas
        z: 2
        // Geometry, mapping, colors and Max Hold behavior intentionally match
        // the previous Canvas renderer. Only the execution path moves to C++.
        width: root.width
        height: root.analyzerHeight * root.spectrumRatio
        x: 0

        backend: (typeof wsClient !== "undefined") ? wsClient : null
        mode: FftDisplayItem.Spectrum
        renderEnabled: root.runtimeActive
        minDb: root.waterfallMinDb
        maxDb: root.waterfallMaxDb
        fullStartFreq: root.fullStartFreq
        viewStartFreq: root.viewStartFreq
        viewStopFreq: root.viewStopFreq
        sampleRate: Math.max(1, root.sampRate)
        spectrumColor: root.spectrumLiveColor
        maxHoldColor: root.spectrumMaxHoldColor

        property alias clearPeakTimer: clearPeakTimer
        showMaxHold: true

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
    }

    // R20.4-SPECTRUM-CUDA1.2: measurement HUD is physically attached to the
    // selected receiver band. It automatically flips to the opposite side when
    // the preferred side would run out of safe plot space. Only the LOCK button
    // accepts pointer input; the rest of the panel remains transparent to the
    // existing click/drag-to-tune MouseArea below it.
    Item {
        id: selectionMetricsOverlay
        width: 202
        height: 86
        z: 40
        visible: root.runtimeActive && root.selectionVisible

        readonly property real bandLeft: Math.min(root.selectionLeftX, root.selectionRightX)
        readonly property real bandRight: Math.max(root.selectionLeftX, root.selectionRightX)
        readonly property real safeLeft: root.selectionMetricsEdgeMargin
        // Reserve the existing zoom-tool footprint so the attached HUD flips
        // before it can hide beneath those controls on the right edge.
        readonly property real safeRight: Math.max(safeLeft,
                                                   spectrumCanvas.width
                                                   - ((zoom && zoom.visible) ? (zoom.width + 20) : safeLeft))
        readonly property real needWidth: width + root.selectionMetricsMargin
        readonly property real spaceLeft: Math.max(0, bandLeft - safeLeft)
        readonly property real spaceRight: Math.max(0, safeRight - bandRight)
        property bool placeLeft: false
        readonly property real flipHysteresisPx: 16

        function updateAttachedSide() {
            // Stay on the current side until the opposite side has a little
            // extra room. This prevents left/right flicker while dragging near
            // the flip threshold, while still guaranteeing edge avoidance.
            if (placeLeft) {
                if ((spaceLeft < needWidth && spaceRight >= needWidth)
                        || spaceRight >= needWidth + flipHysteresisPx)
                    placeLeft = false
            } else {
                if ((spaceRight < needWidth && spaceLeft >= needWidth)
                        || (spaceRight < needWidth
                            && spaceLeft > spaceRight + flipHysteresisPx))
                    placeLeft = true
            }
        }

        onSpaceLeftChanged: updateAttachedSide()
        onSpaceRightChanged: updateAttachedSide()
        onNeedWidthChanged: updateAttachedSide()
        Component.onCompleted: updateAttachedSide()

        readonly property real requestedX: placeLeft
                                                ? bandLeft - width - root.selectionMetricsMargin
                                                : bandRight + root.selectionMetricsMargin

        x: Math.max(safeLeft, Math.min(requestedX, safeRight - width))
        y: 25

        // Small visual bridge makes the panel read as one object with the
        // receiver-selection band while preserving the original tuning gesture.
        Rectangle {
            id: selectionMetricsBridge
            width: Math.max(2, root.selectionMetricsMargin)
            height: 2
            y: 18
            x: selectionMetricsOverlay.placeLeft
                   ? selectionMetricsOverlay.width
                   : -width
            color: root.analyzerAccent
            opacity: root.selectionMetricsLocked ? 0.95 : 0.45
        }

        Rectangle {
            id: selectionMetricsPanel
            anchors.fill: parent
            radius: 7
            color: root.analyzerPanel
            opacity: root.selectionMetricsPanelOpacity
            border.width: 1
            border.color: root.selectionMetricsLocked ? root.analyzerAccent : root.analyzerBorder

            Behavior on opacity {
                NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
            }
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.topMargin: 7
            spacing: 3

            Row {
                width: parent.width
                spacing: 6
                Text {
                    width: 47
                    text: "PEAK"
                    color: root.analyzerMuted
                    font.pixelSize: 9
                    font.bold: true
                }
                Text {
                    width: 79
                    horizontalAlignment: Text.AlignRight
                    text: spectrumCanvas.measurementsValid
                          ? spectrumCanvas.peakDb.toFixed(1) + " dBFS" : "—"
                    color: root.analyzerText
                    font.pixelSize: 11
                    font.family: "monospace"
                    font.bold: true
                }
                Text {
                    width: 48
                    horizontalAlignment: Text.AlignRight
                    text: spectrumCanvas.measurementsValid
                          ? (spectrumCanvas.peakFrequencyHz / 1e6).toFixed(3) : ""
                    color: root.analyzerMuted
                    font.pixelSize: 8
                    font.family: "monospace"
                }
            }

            Row {
                width: parent.width
                spacing: 6
                Text {
                    width: 47
                    text: "NOISE"
                    color: root.analyzerMuted
                    font.pixelSize: 9
                    font.bold: true
                }
                Text {
                    width: 79
                    horizontalAlignment: Text.AlignRight
                    text: spectrumCanvas.measurementsValid
                          ? spectrumCanvas.noiseFloorDb.toFixed(1) + " dBFS" : "—"
                    color: root.analyzerText
                    font.pixelSize: 11
                    font.family: "monospace"
                    font.bold: true
                }
            }

            Row {
                width: parent.width
                spacing: 6
                Text {
                    width: 47
                    text: "SNR"
                    color: root.analyzerMuted
                    font.pixelSize: 9
                    font.bold: true
                }
                Text {
                    width: 79
                    horizontalAlignment: Text.AlignRight
                    text: spectrumCanvas.measurementsValid
                          ? spectrumCanvas.snrDb.toFixed(1) + " dB" : "—"
                    color: root.analyzerAccent
                    font.pixelSize: 11
                    font.family: "monospace"
                    font.bold: true
                }
            }
        }

        // LOCK changes panel opacity only. It deliberately does not freeze RF
        // tuning or metric values, so the HUD continues to stick to the current
        // receiver-selection bar exactly as requested.
        Rectangle {
            id: selectionMetricsLockButton
            width: 42
            height: 18
            radius: 4
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 5
            color: root.selectionMetricsLocked ? root.analyzerAccent : "#99131F29"
            border.width: 1
            border.color: root.selectionMetricsLocked ? root.analyzerAccent : root.analyzerBorder
            z: 3

            Text {
                anchors.centerIn: parent
                text: root.selectionMetricsLocked ? "LOCKED" : "LOCK"
                color: root.selectionMetricsLocked ? "#081018" : root.analyzerText
                font.pixelSize: 8
                font.bold: true
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: root.selectionMetricsLocked = !root.selectionMetricsLocked
            }
        }
    }

    FftDisplayItem {
        id: waterfallCanvas
        z: 2
        y: spectrumCanvas.height
        x: 0
        width: root.width
        height: Math.max(1, root.analyzerHeight - spectrumCanvas.height)

        backend: (typeof wsClient !== "undefined") ? wsClient : null
        mode: FftDisplayItem.Waterfall
        renderEnabled: root.runtimeActive
        minDb: root.waterfallMinDb
        maxDb: root.waterfallMaxDb
        fullStartFreq: root.fullStartFreq
        viewStartFreq: root.viewStartFreq
        viewStopFreq: root.viewStopFreq
        sampleRate: Math.max(1, root.sampRate)
        palette: root.waterfallColorMap

        Connections {
            target: root
            function onWaterfallColorUpdate(colors) {
                root.rebuildWaterfallPalette(colors)
            }
        }
    }

    // Waterfall title/actions are local to this subsystem. Pause preserves
    // history; Clear is the only view-side action that explicitly discards it.
    Row {
        id: waterfallHeader
        x: 10
        y: waterfallCanvas.y + 8
        spacing: 8
        z: 110

        Text {
            text: "WATERFALL"
            color: root.analyzerText
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 1.0
        }
        Text {
            text: "SMOOTH · PERSISTENT"
            color: root.analyzerAccent
            font.pixelSize: 9
            font.bold: true
        }
    }

    Row {
        id: waterfallActions
        anchors.right: waterfallCanvas.right
        anchors.rightMargin: 76
        y: waterfallCanvas.y + 5
        spacing: 6
        z: 111

        Rectangle {
            width: 62; height: 26; radius: 6
            color: waterfallCanvas.waterfallPaused ? "#243746" : "#CC0D1721"
            border.color: root.analyzerBorder
            Text { anchors.centerIn: parent; text: waterfallCanvas.waterfallPaused ? "Resume" : "Pause"; color: root.analyzerText; font.pixelSize: 10; font.bold: true }
            MouseArea { anchors.fill: parent; onClicked: waterfallCanvas.waterfallPaused = !waterfallCanvas.waterfallPaused }
        }
        Rectangle {
            width: 54; height: 26; radius: 6
            color: "#CC0D1721"; border.color: root.analyzerBorder
            Text { anchors.centerIn: parent; text: "Clear"; color: root.analyzerText; font.pixelSize: 10; font.bold: true }
            MouseArea { anchors.fill: parent; onClicked: waterfallCanvas.clearHistory() }
        }
    }

    // AB8-style data-driven waterfall scale. The gradient uses the exact same
    // palette stops as the C++ renderer; P/N are live visible-spectrum values.
    Item {
        id: waterfallLegend
        width: 66
        // CUDA1.6: extend the color legend through most of the Waterfall while
        // reserving deterministic space for the vertical SCAN/MEMORY toggles
        // and the bottom HUD dock. The lower edge can therefore never overlap
        // S-meter / intensity cards.
        height: Math.max(180, waterfallCanvas.height
                              - root.waterfallHudDockHeight
                              - root.waterfallRightToggleHeight
                              - 58)
        anchors.right: waterfallCanvas.right
        anchors.rightMargin: 6
        anchors.top: waterfallCanvas.top
        anchors.topMargin: 38
        z: 112

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: "#D9081118"
            border.color: "#70577B86"
        }

        Canvas {
            id: waterfallScaleCanvas
            x: 8
            y: 16
            width: 12
            height: Math.max(20, parent.height - 34)
            antialiasing: false
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const gradient = ctx.createLinearGradient(0, 0, 0, height)
                const colors = root.waterfallColorMap || []
                if (!colors.length) return
                for (let i = 0; i < colors.length; ++i) {
                    const packed = Number(colors[colors.length - 1 - i]) >>> 0
                    const hex = "#" + ("000000" + packed.toString(16)).slice(-6)
                    gradient.addColorStop(i / Math.max(1, colors.length - 1), hex)
                }
                ctx.fillStyle = gradient
                ctx.fillRect(0, 0, width, height)
            }
            Connections {
                target: root
                function onWaterfallColorMapChanged() { waterfallScaleCanvas.requestPaint() }
            }
        }

        function levelY(dbValue) {
            const minDb = root.waterfallMinDb
            const maxDb = Math.max(minDb + 0.001, root.waterfallMaxDb)
            const clamped = Math.max(minDb, Math.min(maxDb, Number(dbValue)))
            return waterfallScaleCanvas.y + (1.0 - (clamped - minDb) / (maxDb - minDb)) * waterfallScaleCanvas.height
        }

        Text { x: 25; y: 5; text: root.waterfallMaxDb.toFixed(0); color: root.hudPrimaryText; font.pixelSize: 10; font.bold: true; font.family: "monospace" }
        Text { x: 25; anchors.verticalCenter: parent.verticalCenter; text: ((root.waterfallMinDb + root.waterfallMaxDb) * 0.5).toFixed(0); color: root.hudSecondaryText; font.pixelSize: 10; font.bold: true; font.family: "monospace" }
        Text { x: 25; anchors.bottom: parent.bottom; anchors.bottomMargin: 13; text: root.waterfallMinDb.toFixed(0); color: root.hudPrimaryText; font.pixelSize: 10; font.bold: true; font.family: "monospace" }
        Text { x: 25; anchors.bottom: parent.bottom; anchors.bottomMargin: 2; text: "dBFS"; color: root.hudSecondaryText; font.pixelSize: 9; font.bold: true }

        Rectangle {
            visible: spectrumCanvas.measurementsValid
            x: 2; y: waterfallLegend.levelY(spectrumCanvas.peakDb) - 7
            width: 18; height: 14; radius: 3
            color: "#E6F6A53A"
            Text { anchors.centerIn: parent; text: "P"; color: "#081018"; font.pixelSize: 9; font.bold: true }
        }
        Rectangle {
            visible: spectrumCanvas.measurementsValid
            x: 2; y: waterfallLegend.levelY(spectrumCanvas.noiseFloorDb) - 7
            width: 18; height: 14; radius: 3
            color: "#E635D5BD"
            Text { anchors.centerIn: parent; text: "N"; color: "#081018"; font.pixelSize: 9; font.bold: true }
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

            let offsetFreqAbs = root.selectedReceiverHz;
            let freqLeft = root.selectionLeftFreqHz;
            let freqRight = root.selectionRightFreqHz;

            let x1 = root.selectionLeftX;
            let x2 = root.selectionRightX;
            let xCenter = root.selectionCenterX;

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

    /* ============================================================
       R20.4-SPECTRUM-CUDA1.4: Waterfall HUD dock / layout cleanup
       ------------------------------------------------------------
       All persistent analyzer controls live on one explicit overlay layer
       above the Waterfall renderer.  Left / center / right safe zones keep
       the controls readable and prevent the floating workspace from covering
       them.  No control is re-created when Scan/Memory opens.
       ============================================================ */
    Item {
        id: waterfallHudLayer
        x: 0
        y: waterfallCanvas.y
        width: waterfallCanvas.width
        height: waterfallCanvas.height
        z: 108
        visible: root.runtimeActive

        // LEFT POD: layout container only. Each persistent control owns its
        // own compact translucent card; there is deliberately no large pod fill.
        Rectangle {
            id: waterfallLeftHudPod
            width: Math.min(520, Math.max(380, waterfallHudLayer.width * 0.31))
            height: root.waterfallHudDockHeight - 12
            anchors.left: parent.left
            anchors.leftMargin: root.waterfallHudSideMargin
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            color: "transparent"
            border.width: 0

            RowLayout {
                anchors.fill: parent
                spacing: 8

                Rectangle {
                    id: bandwidthHudCard
                    Layout.fillWidth: true
                    Layout.preferredWidth: 340
                    Layout.preferredHeight: 78
                    Layout.alignment: Qt.AlignVCenter
                    radius: root.waterfallHudCardRadius
                    color: bandwidthCardHover.hovered ? root.waterfallHudCardHoverColor : root.waterfallHudCardColor
                    border.width: 1
                    border.color: bandwidthCardHover.hovered ? root.waterfallHudCardHoverBorder : root.waterfallHudCardBorder
                    scale: bandwidthCardHover.hovered ? 1.012 : 1.0
                    transformOrigin: Item.Center

                    Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on border.color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                    HoverHandler { id: bandwidthCardHover }

                    BandwidthScaleControl {
                        id: bandwidthScaleControl
                        anchors.fill: parent
                        anchors.margins: 3
                        visible: receiverMode.get(scanReceiverModeSelected).mode === "Analog"
                        opacity: 1.0
                    }
                }

                Rectangle {
                    id: maxHoldHudCard
                    Layout.preferredWidth: 158
                    Layout.preferredHeight: 54
                    Layout.alignment: Qt.AlignVCenter
                    radius: root.waterfallHudCardRadius
                    color: maxHoldCardHover.hovered ? root.waterfallHudCardHoverColor : root.waterfallHudCardColor
                    border.width: 1
                    border.color: maxHoldCardHover.hovered ? root.waterfallHudCardHoverBorder : root.waterfallHudCardBorder
                    scale: maxHoldCardHover.hovered ? 1.018 : 1.0
                    transformOrigin: Item.Center

                    Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on border.color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                    HoverHandler { id: maxHoldCardHover }

                    CheckBox {
                        id: maxHoldCheckBox
                        anchors.centerIn: parent
                        text: "Show Max Hold"
                        opacity: 1.0
                        Material.foreground: root.hudPrimaryText
                        Material.accent: root.hudAccentText
                        font.pixelSize: 12
                        font.bold: true
                        onCheckedChanged: spectrumCanvas.showMaxHold = checked
                        checked: spectrumCanvas.showMaxHold
                    }
                }
            }
        }

        // RIGHT POD: transparent layout owner. S-meter and intensity controls
        // each receive an independent translucent gray card for legibility.
        Rectangle {
            id: waterfallRightHudPod
            width: Math.min(650, Math.max(520, waterfallHudLayer.width * 0.39))
            height: root.waterfallHudDockHeight - 12
            anchors.right: parent.right
            anchors.rightMargin: root.waterfallHudSideMargin
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            color: "transparent"
            border.width: 0

            RowLayout {
                anchors.fill: parent
                spacing: 8

                Rectangle {
                    id: smeterHudCard
                    Layout.fillWidth: true
                    Layout.preferredWidth: 300
                    Layout.preferredHeight: 68
                    Layout.alignment: Qt.AlignVCenter
                    radius: root.waterfallHudCardRadius
                    color: smeterCardHover.hovered ? root.waterfallHudCardHoverColor : root.waterfallHudCardColor
                    border.width: 1
                    border.color: smeterCardHover.hovered ? root.waterfallHudCardHoverBorder : root.waterfallHudCardBorder
                    scale: smeterCardHover.hovered ? 1.012 : 1.0
                    transformOrigin: Item.Center

                    Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on border.color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                    HoverHandler { id: smeterCardHover }

                    AnalogSMeter {
                        id: smeterOverlay
                        anchors.fill: parent
                        anchors.margins: 3
                    }
                }

                Rectangle {
                    id: waterfallScaleHudCard
                    Layout.fillWidth: true
                    Layout.preferredWidth: 310
                    Layout.preferredHeight: 78
                    Layout.alignment: Qt.AlignVCenter
                    radius: root.waterfallHudCardRadius
                    color: waterfallScaleCardHover.hovered ? root.waterfallHudCardHoverColor : root.waterfallHudCardColor
                    border.width: 1
                    border.color: waterfallScaleCardHover.hovered ? root.waterfallHudCardHoverBorder : root.waterfallHudCardBorder
                    scale: waterfallScaleCardHover.hovered ? 1.012 : 1.0
                    transformOrigin: Item.Center

                    Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on border.color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                    HoverHandler { id: waterfallScaleCardHover }

                    WaterfallScaleControl {
                        id: waterfallScaleControl
                        anchors.fill: parent
                        anchors.margins: 3
                        opacity: 1.0
                        waterfallMinDb: root.waterfallMinDb
                        waterfallMaxDb: root.waterfallMaxDb
                        onWaterfallMinDbChanged: root.waterfallMinDb = waterfallMinDb
                        onWaterfallMaxDbChanged: root.waterfallMaxDb = waterfallMaxDb
                        onManualScaleEdited: {
                            root.autoScaleEnabled = false
                            root.autoScaleInitialized = false
                        }
                    }
                }
            }
        }
    }

    /* ============================================================
       R20.4-SPECTRUM-CUDA1.9: Expanded floating workspace
       (inherits CUDA1.6 full-screen analyzer/floating workspace)
       ------------------------------------------------------------
       - Spectrum/Waterfall remain full-size and live behind this panel.
       - Existing Scan/Memory folder components are reused unchanged.
       - The panel is never Loader-created/destroyed, preserving QML state.
       - Only geometry/opacity animate; CUDA/FBO/FFT ownership is untouched.
       ============================================================ */

    // CUDA1.6: SCAN/MEMORY are a persistent vertical toggle rail attached
    // directly below the Waterfall color legend. The rail remains visible while
    // the floating workspace is open, so the active mode can be toggled closed
    // or switched without reaching across the display.
    Item {
        id: floatingWorkspaceLauncher
        width: root.waterfallRightToggleWidth
        height: root.waterfallRightToggleHeight
        // Keep the larger touch targets on the same right edge as the color
        // legend so they read as one continuous Waterfall utility rail.
        x: waterfallLegend.x + waterfallLegend.width - width
        y: waterfallLegend.y + waterfallLegend.height + 5
        z: 113
        visible: root.runtimeActive
        opacity: visible ? 0.96 : 0.0

        Behavior on opacity { NumberAnimation { duration: 120 } }

        Column {
            anchors.fill: parent
            spacing: 4

            Rectangle {
                width: parent.width
                height: (parent.height - parent.spacing) / 2
                radius: 8
                color: root.floatingWorkspaceOpen && widgetView
                       ? "#F0189286"
                       : (scanLaunchArea.pressed ? "#F0167F76"
                          : (scanLaunchArea.containsMouse ? "#F0243E48" : "#D612252D"))
                border.width: 1
                border.color: root.floatingWorkspaceOpen && widgetView
                              ? "#D06EF2E8"
                              : (scanLaunchArea.containsMouse ? "#B066B9C3" : "#704B6973")
                scale: scanLaunchArea.pressed ? 0.97 : (scanLaunchArea.containsMouse ? 1.025 : 1.0)
                transformOrigin: Item.Center
                Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                Text {
                    anchors.centerIn: parent
                    text: "SCAN"
                    color: root.hudPrimaryText
                    font.pixelSize: 14
                    font.bold: true
                    font.letterSpacing: 0.6
                }
                MouseArea {
                    id: scanLaunchArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.toggleFloatingWorkspace(true)
                }
            }

            Rectangle {
                width: parent.width
                height: (parent.height - parent.spacing) / 2
                radius: 8
                color: root.floatingWorkspaceOpen && !widgetView
                       ? "#F0189286"
                       : (memoryLaunchArea.pressed ? "#F0167F76"
                          : (memoryLaunchArea.containsMouse ? "#F0243E48" : "#D612252D"))
                border.width: 1
                border.color: root.floatingWorkspaceOpen && !widgetView
                              ? "#D06EF2E8"
                              : (memoryLaunchArea.containsMouse ? "#B066B9C3" : "#704B6973")
                scale: memoryLaunchArea.pressed ? 0.97 : (memoryLaunchArea.containsMouse ? 1.025 : 1.0)
                transformOrigin: Item.Center
                Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                Text {
                    anchors.centerIn: parent
                    text: "MEMORY"
                    color: root.hudPrimaryText
                    font.pixelSize: 13
                    font.bold: true
                    font.letterSpacing: 0.3
                }
                MouseArea {
                    id: memoryLaunchArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.toggleFloatingWorkspace(false)
                }
            }
        }
    }

    Item {
        id: floatingWorkspace
        width: root.floatingWorkspaceWidth
        height: root.floatingWorkspaceHeight
        x: (root.width - width) / 2
        y: root.floatingWorkspaceOpen
           ? root.height - height - root.floatingWorkspaceBottomMargin
           : root.height + 24
        z: 120
        enabled: root.floatingWorkspaceOpen
        opacity: root.floatingWorkspaceOpen ? 1.0 : 0.0

        Behavior on y {
            NumberAnimation { duration: 280; easing.type: Easing.OutCubic }
        }
        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
        }

        // Lightweight shadow without introducing QtGraphicalEffects/GPU passes.
        Rectangle {
            x: 5
            y: 8
            width: parent.width
            height: parent.height
            radius: 15
            color: "#90000000"
        }

        Rectangle {
            id: floatingWorkspacePanel
            anchors.fill: parent
            radius: 14
            color: "#F20B1C26"
            border.width: 1
            border.color: "#80548C98"
            clip: true

            // R20.4-SPECTRUM-CUDA1.10: header text uses content-driven spacing;
            // the LIVE badge follows the actual workspace title width instead of a
            // fixed x-offset, preventing SCAN/MEMORY title overlap.
            Rectangle {
                id: floatingWorkspaceHeader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 44
                color: "#F20A1821"
                border.width: 0

                Text {
                    id: floatingWorkspaceTitleText
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, Math.max(120, parent.width * 0.34))
                    text: root.floatingWorkspaceTitle
                    color: root.analyzerText
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    wrapMode: Text.NoWrap
                }

                Rectangle {
                    height: 22
                    width: 178
                    radius: 6
                    anchors.left: floatingWorkspaceTitleText.right
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#401A8D83"
                    border.width: 1
                    border.color: "#5535D5BD"

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        text: "SPECTRUM + WATERFALL LIVE"
                        color: root.analyzerAccent
                        opacity: 0.90
                        font.pixelSize: 9
                        font.bold: true
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        wrapMode: Text.NoWrap
                    }
                }

                Rectangle {
                    width: 34
                    height: 28
                    radius: 7
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    color: floatingCloseArea.pressed ? "#36505E" : "#B21B2D38"
                    border.width: 1
                    border.color: root.analyzerBorder
                    Text { anchors.centerIn: parent; text: "×"; color: root.analyzerText; font.pixelSize: 20; font.bold: true }
                    MouseArea {
                        id: floatingCloseArea
                        anchors.fill: parent
                        onClicked: root.closeFloatingWorkspace()
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: floatingWorkspaceHeader.bottom
                height: 1
                color: root.analyzerBorder
                opacity: 0.9
            }

            Item {
                id: memorySlot
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: floatingWorkspaceHeader.bottom
                anchors.bottom: parent.bottom
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 9
                anchors.bottomMargin: 10
            }

            // Existing production Scan/Memory implementations are deliberately
            // not duplicated. Their Folder -> Group/Channel -> Detail behavior,
            // Delete, Filter and mode buttons all remain authoritative here.
            NewMemoryAddEdit {
                id: newMemoryAddEdit
                anchors.fill: memorySlot
                visible: root.floatingWorkspaceOpen && !widgetView
                enabled: visible
                z: 2
                radioMemLists: radioMemList
            }

            LogDeviceScanner {
                id: logDeviceScanner
                anchors.fill: memorySlot
                visible: root.floatingWorkspaceOpen && widgetView
                enabled: visible
                z: 3
            }
        }
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

            if (profileCards.count > 0) {
                widgetView = true
                root.floatingWorkspaceOpen = true
            }

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
                    root.floatingWorkspaceOpen = true
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
