import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.4
import Qt.labs.settings 1.1
import "../ui"

Rectangle {
    id: root
    // StackView sizes pages itself. Do not anchor the root page to parent;
    // anchors on a StackView-managed item can race transitions and produce
    // QQuickWindow UpdateRequest crashes on embedded Qt 5.15 render loops.
    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080
    readonly property bool hmiDarkMode: Material.theme === Material.Dark
    Theme { id: hmiTheme; darkMode: root.hmiDarkMode }
    color: hmiTheme.page

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
        // R20.4-ASTRARX-SMOOTH100: keep the selected-source publish clock fast.
        // It still publishes only the latest real backend frame; native renderers
        // do the smooth visual interpolation/scrolling between RF snapshots.
        if (root.renderQualityLevel <= 0) return 10
        if (root.renderQualityLevel >= 2) return 16
        return 12
    }

    function _spectrumFps() {
        if (root.renderQualityLevel <= 0) return 100
        if (root.renderQualityLevel >= 2) return 60
        return 90
    }

    function _waterfallFps() {
        if (root.renderQualityLevel <= 0) return 100
        if (root.renderQualityLevel >= 2) return 60
        return 90
    }

    // V27: DoA waterfall readability upgrade.
    // Build a 256-entry LUT in QML and pass it to the native/CUDA waterfall
    // renderer.  This keeps the fast render path intact while fixing the low
    // contrast palettes that made the waterfall hard to read in both themes.
    function _wfClamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

    function _wfLerp(a, b, t) {
        return Math.round(Number(a) + (Number(b) - Number(a)) * Number(t))
    }

    function _wfMix(c0, c1, t) {
        var r0 = (c0 >> 16) & 255
        var g0 = (c0 >> 8) & 255
        var b0 = c0 & 255
        var r1 = (c1 >> 16) & 255
        var g1 = (c1 >> 8) & 255
        var b1 = c1 & 255
        return ((_wfLerp(r0, r1, t) & 255) << 16)
             | ((_wfLerp(g0, g1, t) & 255) << 8)
             |  (_wfLerp(b0, b1, t) & 255)
    }

    function _wfRamp(stops, gamma) {
        var out = []
        if (!stops || stops.length < 2) return [0x001122, 0x00FFFF, 0xFFFFFF]
        var g = Number(gamma)
        if (!isFinite(g) || g <= 0.0) g = 1.0

        for (var i = 0; i < 256; ++i) {
            var x = i / 255.0
            // gamma < 1 lifts weak/medium signals so faint vertical traces do
            // not disappear into the noise floor.
            var t = Math.pow(x, g)
            var lower = stops[0]
            var upper = stops[stops.length - 1]
            for (var k = 0; k < stops.length - 1; ++k) {
                if (t >= stops[k].p && t <= stops[k + 1].p) {
                    lower = stops[k]
                    upper = stops[k + 1]
                    break
                }
            }
            var span = Math.max(0.000001, upper.p - lower.p)
            var local = _wfClamp((t - lower.p) / span, 0.0, 1.0)
            out.push(_wfMix(lower.c, upper.c, local))
        }
        return out
    }

    function waterfallPaletteForTheme(dark) {
        if (dark) {
            // V28: Night waterfall now uses a purple/magenta/amber palette so it
            // is visually different from the blue/cyan FFT spectrum trace.
            // Noise stays dark-plum; peaks climb to amber/white.
            return _wfRamp([
                { p: 0.00, c: 0x09040E },
                { p: 0.10, c: 0x170A25 },
                { p: 0.22, c: 0x30105A },
                { p: 0.36, c: 0x6E1BB8 },
                { p: 0.50, c: 0xB625C8 },
                { p: 0.62, c: 0xFF2FA3 },
                { p: 0.74, c: 0xFF684A },
                { p: 0.84, c: 0xFFB000 },
                { p: 0.93, c: 0xFFE766 },
                { p: 1.00, c: 0xFFFFFF }
            ], 0.78)
        }

        // V28: Day waterfall keeps a light background as requested.  It no
        // longer uses a dark/navy floor and no longer follows the blue spectrum
        // color.  Weak signals appear violet; stronger RF energy moves through
        // magenta/orange/red-brown for high contrast on a light panel.
        return _wfRamp([
            { p: 0.00, c: 0xF8F4EA },
            { p: 0.10, c: 0xEFE5D7 },
            { p: 0.22, c: 0xDACBFF },
            { p: 0.36, c: 0xA978FF },
            { p: 0.50, c: 0xD33AC8 },
            { p: 0.63, c: 0xFF4F8E },
            { p: 0.75, c: 0xFF8A00 },
            { p: 0.86, c: 0xD44700 },
            { p: 0.94, c: 0x7A1D00 },
            { p: 1.00, c: 0x2A0800 }
        ], 0.74)
    }

    function waterfallBackgroundForTheme(dark) {
        return dark ? "#09040E" : "#F8F4EA"
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

    // DOA-FFT-LIFE1: channel/source switch transaction state.  A channel
    // change must be a single ordered transaction: choose expected source,
    // switch backend, clear presentation once, then accept the first valid
    // frame from the selected source.  Do not let property-change handlers
    // perform a second clear in the middle of the transaction.
    property int _channelSwitchEpoch: 0
    property bool _channelSwitchActive: false
    property bool _suppressDisplayChannelHandler: false
    property bool _waitingForFirstFrame: false
    property string _expectedSourceKey: "RX"
    property string _lastClearKey: ""

    property int _perfFrames: 0
    property int _perfCoalesced: 0
    property int _perfLastSeq: 0

    function mw() {
        return (typeof(mainWindows) !== "undefined" && mainWindows !== null) ? mainWindows : null
    }

    readonly property bool rxSourceAvailable: mw() !== null
    // Single FFT gate for the selected logical channel.  This replaces the old
    // split CH1/RX vs DF-channel state: one UI toggle controls whichever
    // channel is currently selected.  When the page is hidden, all FFT sources
    // are suspended regardless of this remembered UI state.
    property bool fftEnabled: false

    function logicalSourceName() {
        return root.sourceKeyForChannel(root.displayChannel)
    }

    function sourceKeyForChannel(channel) {
        var ch = Math.max(1, Math.min(6, Number(channel)))
        if (ch === 1) return "RX"
        return "DF" + (ch - 1)
    }

    function physicalDfChannelForDisplay(channel) {
        var ch = Math.max(1, Math.min(6, Number(channel)))
        return Math.max(0, Math.min(4, ch - 2))
    }

    function _setDisplayChannelGuarded(channel) {
        var ch = Math.max(1, Math.min(6, Number(channel)))
        if (root.displayChannel === ch)
            return
        root._suppressDisplayChannelHandler = true
        root.displayChannel = ch
        root._suppressDisplayChannelHandler = false
    }

    function spectrumTitle() {
        return "RF FFT Spectrum (CH" + displayChannel + " / " + logicalSourceName() + ")"
    }

    function syncRxConsumer() {
        // Backward-compatible helper retained for old signal paths.
        root.syncFftResources("sync-rx-consumer")
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

    function _noteFirstFrameAccepted(reason, bins) {
        if (!root._waitingForFirstFrame)
            return
        root._waitingForFirstFrame = false
        console.log("[DOA-FIRST-FRAME]"
                    + " epoch=" + root._channelSwitchEpoch
                    + " channel=CH" + root.displayChannel
                    + " source=" + root.logicalSourceName()
                    + " expected=" + root._expectedSourceKey
                    + " bins=" + bins
                    + " reason=" + reason
                    + " accepted=1")
    }

    function publishSelectedFrame(reason) {
        if (!root.visible || !root.fftEnabled) return

        var source = root.logicalSourceName()
        if (root._waitingForFirstFrame && source !== root._expectedSourceKey) {
            console.log("[DOA-FIRST-FRAME]"
                        + " epoch=" + root._channelSwitchEpoch
                        + " channel=CH" + root.displayChannel
                        + " source=" + source
                        + " expected=" + root._expectedSourceKey
                        + " reason=" + reason
                        + " accepted=0 stale-source=1")
            return
        }

        var seqBefore = root.displayFrameSequence
        var bins = 0

        if (root.rxDisplaySelected) {
            if (!root.fftEnabled || !root.rxSourceAvailable) return
            root.rebuildRxFrequencyAxis()
            if (!root.rxFftMagDb || root.rxFftMagDb.length < 8) return
            root.displayFftFreqHz = root.rxFftFreqHz
            root.displayFftMagDb = root.rxFftMagDb
            bins = root.rxFftMagDb.length
        } else {
            if (typeof(doaClient) === "undefined" || doaClient === null) return
            if (!doaClient.fftMagDb || doaClient.fftMagDb.length < 8) return
            root.displayFftFreqHz = doaClient.fftFreqHz
            root.displayFftMagDb = doaClient.fftMagDb
            bins = doaClient.fftMagDb.length
        }

        root.displayFrameSequence++
        root._perfFrames++
        if (root.displayFrameSequence > seqBefore + 1)
            root._perfCoalesced += (root.displayFrameSequence - seqBefore - 1)

        root._noteFirstFrameAccepted(reason, bins)
    }

    Timer {
        id: renderScheduler
        // One DoA display clock for spectrum + waterfall. Keep UI responsive
        // without replaying old FFT frames. Hidden/unselected sources only cache
        // the latest backend frame.
        interval: root._schedulerIntervalMs()
        running: root.visible && root.fftEnabled
        repeat: true
        onTriggered: {
            if (!root.fftEnabled) return
            if (root.rxDisplaySelected) {
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
        running: root.visible && root.fftEnabled
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

    Timer {
        id: hiddenSuspendTimer
        interval: 1200
        repeat: false
        onTriggered: {
            // StackView/Layout transitions can pulse visible=false briefly. Do not
            // convert that pulse into user FFT OFF. Only suspend backend consumers
            // after the page has stayed hidden for the debounce window; the
            // remembered root.fftEnabled state is preserved and will re-arm on
            // visible=true.
            if (root.visible || !root.fftEnabled)
                return
            root.stopAllFftResources("hidden-debounce")
            console.log("[DOA-FFT-SYNC]"
                        + " reason=hidden-debounce"
                        + " epoch=" + root._channelSwitchEpoch
                        + " source=" + root.logicalSourceName()
                        + " rxConsumer=0"
                        + " dfSpectrumEnabled=0"
                        + " active=0"
                        + " userFftEnabled=" + root.fftEnabled)
        }
    }

    function clearFftDisplayBuffers(reason, force) {
        // DOA-FFT-LIFE1: presentation-only clear.  This must not disable FFT,
        // disconnect the IQ/TCP source, stop the CUDA worker, or change the
        // selected backend.  Channel switch transactions call this once after
        // the expected source/backend has been selected.
        var why = reason || "unknown"
        var key = root._channelSwitchEpoch + "|" + root.displayChannel + "|" + why
        if (!force && root._lastClearKey === key)
            return
        root._lastClearKey = key

        root.displayFftFreqHz = []
        root.displayFftMagDb = []
        root.displayFrameSequence++
        root._rxPendingFrame = false
        root._dfPendingFrame = false

        if (typeof fftPlot !== "undefined" && fftPlot &&
                typeof fftPlot.clearPlotHistory === "function")
            fftPlot.clearPlotHistory(why)

        if (typeof wf !== "undefined" && wf &&
                typeof wf.clearHistory === "function")
            wf.clearHistory(why)

        console.log("[DOA-FFT-CLEAR]"
                    + " reason=" + why
                    + " epoch=" + root._channelSwitchEpoch
                    + " logical=CH" + root.displayChannel
                    + " source=" + root.logicalSourceName()
                    + " fftEnabled=" + root.fftEnabled
                    + " runtimeActive=" + (root.visible && root.fftEnabled)
                    + " sourceDisconnected=0"
                    + " seq=" + root.displayFrameSequence)
    }

    function selectDisplayChannel(channel, reason) {
        var why = reason || "user"
        var ch = Math.max(1, Math.min(6, Number(channel)))
        var oldCh = root.displayChannel
        var oldSource = root.logicalSourceName()
        var newSource = root.sourceKeyForChannel(ch)
        var changed = (oldCh !== ch)

        if (!changed && why === "init") {
            root._expectedSourceKey = newSource
            root.syncFftResources("init")
            return
        }

        root._channelSwitchEpoch++
        root._channelSwitchActive = true
        root._expectedSourceKey = newSource
        root._waitingForFirstFrame = root.visible && root.fftEnabled
        root._lastClearKey = ""

        console.log("[DOA-CHANNEL-SWITCH]"
                    + " old=CH" + oldCh
                    + " new=CH" + ch
                    + " oldSource=" + oldSource
                    + " newSource=" + newSource
                    + " epoch=" + root._channelSwitchEpoch
                    + " fftEnabled=" + root.fftEnabled
                    + " runtimeActive=" + (root.visible && root.fftEnabled)
                    + " changed=" + changed
                    + " reason=" + why)

        root._setDisplayChannelGuarded(ch)

        if (ch >= 2 && typeof(doaClient) !== "undefined" && doaClient !== null) {
            var physicalDfChannel = root.physicalDfChannelForDisplay(ch)
            console.log("[DOA-DISPLAY-SOURCE] logical=CH" + ch
                        + " source=DF" + (ch - 1)
                        + " physical_adc=" + physicalDfChannel
                        + " epoch=" + root._channelSwitchEpoch)
            if (doaClient.fftChannel !== physicalDfChannel)
                doaClient.fftChannel = physicalDfChannel
        } else if (ch === 1) {
            console.log("[DOA-DISPLAY-SOURCE] logical=CH1 source=ASTRARX_HOME no_setAdcChannel=1"
                        + " epoch=" + root._channelSwitchEpoch)
        }

        root.syncFftResources("channel-switch")
        root.clearFftDisplayBuffers(changed ? "channel-change" : "channel-reselect", true)
        root._perfLastSeq = root.displayFrameSequence
        root._channelSwitchActive = false

        // Do not publish immediately on channel change.  The previous source's
        // last FFT array may still be cached in doaClient/mainWindows.  Waiting
        // for the next source-specific frame prevents one-frame stale leaks.
    }

    function stopAllFftResources(reason) {
        var m = mw()
        if (m && m.setDoaRxSpectrumActive !== undefined)
            m.setDoaRxSpectrumActive(false)

        var c = (typeof(doaClient) !== "undefined" && doaClient !== null) ? doaClient : null
        if (c && c.spectrumEnabled !== undefined && c.spectrumEnabled)
            c.spectrumEnabled = false

        root._rxPendingFrame = false
        root._dfPendingFrame = false
    }

    function syncFftResources(reason) {
        var c = (typeof(doaClient) !== "undefined" && doaClient !== null) ? doaClient : null

        if (!root.fftEnabled) {
            hiddenSuspendTimer.stop()
            root.stopAllFftResources(reason)
            console.log("[DOA-FFT-SYNC]"
                        + " reason=" + reason
                        + " epoch=" + root._channelSwitchEpoch
                        + " source=" + root.logicalSourceName()
                        + " rxConsumer=0"
                        + " dfSpectrumEnabled=0"
                        + " active=0"
                        + " userFftEnabled=0")
            return
        }

        if (!root.visible) {
            if (!hiddenSuspendTimer.running)
                hiddenSuspendTimer.restart()
            console.log("[DOA-FFT-SYNC]"
                        + " reason=" + reason
                        + " epoch=" + root._channelSwitchEpoch
                        + " source=" + root.logicalSourceName()
                        + " rxConsumer=" + (root.rxDisplaySelected ? 1 : 0)
                        + " dfSpectrumEnabled=" + ((!root.rxDisplaySelected && c) ? 1 : 0)
                        + " active=0"
                        + " hiddenDeferred=1"
                        + " userFftEnabled=1")
            return
        }

        hiddenSuspendTimer.stop()

        var m = mw()
        if (m && m.setDoaRxSpectrumActive !== undefined)
            m.setDoaRxSpectrumActive(root.rxDisplaySelected)

        var wantDfFft = !root.rxDisplaySelected
        if (c && c.spectrumEnabled !== undefined) {
            if (c.spectrumEnabled !== wantDfFft)
                c.spectrumEnabled = wantDfFft
        }

        console.log("[DOA-FFT-SYNC]"
                    + " reason=" + reason
                    + " epoch=" + root._channelSwitchEpoch
                    + " source=" + root.logicalSourceName()
                    + " rxConsumer=" + (root.rxDisplaySelected ? 1 : 0)
                    + " dfSpectrumEnabled=" + ((!root.rxDisplaySelected && c) ? 1 : 0)
                    + " active=1"
                    + " userFftEnabled=1")
    }

    function setFftEnabled(enabled) {
        var v = !!enabled
        if (root.fftEnabled === v)
            return

        root.fftEnabled = v
        root.syncFftResources("toggle")

        root.clearFftDisplayBuffers(v ? "fft-enabled" : "fft-disabled", true)
        if (v) {
            if (root.rxDisplaySelected) {
                root._rxPendingFrame = true
                root.publishSelectedFrame("fft-enabled-rx")
            } else {
                // Wait for the first DF FFT frame after the selected channel is
                // active; do not draw stale cached DF data.
                root._dfPendingFrame = false
            }
        }

        console.log("[DOA-FFT]", v ? "ON" : "OFF", "logical=CH" + root.displayChannel, "source=" + root.logicalSourceName())
    }

    Connections {
        target: root.mw()
        function onDoaRxFftFrameChanged() {
            if (root.visible && root.fftEnabled && root.rxDisplaySelected)
                root._rxPendingFrame = true
        }
    }

    Connections {
        target: (typeof(doaClient) !== "undefined" && doaClient !== null) ? doaClient : null
        function onFftChannelChanged() {
            // Preserve CH1/RX selection. If a DF channel is currently selected,
            // reflect authoritative backend changes using the logical +1 offset.
            if (root.displayChannel >= 2) {
                var logical = Math.max(2, Math.min(6, doaClient.fftChannel + 2))
                if (logical !== root.displayChannel && !root._channelSwitchActive)
                    root.selectDisplayChannel(logical, "backend")
            }
        }
        function onFftChanged() {
            if (root.visible && root.fftEnabled && !root.rxDisplaySelected)
                root._dfPendingFrame = true
        }
    }

    onDisplayChannelChanged: {
        if (!root._suppressDisplayChannelHandler) {
            // External/property-driven changes are normalized through the same
            // transaction path so they cannot clear/re-arm resources twice.
            root.selectDisplayChannel(root.displayChannel, "property-change")
        }
    }
    onVisibleChanged: {
        if (visible) {
            hiddenSuspendTimer.stop()
            root.syncFftResources("visible")
            if (root.fftEnabled) {
                if (root.rxDisplaySelected) {
                    root._rxPendingFrame = true
                    root.publishSelectedFrame("visible-rx")
                } else {
                    // Wait for a real DF frame from the selected source. Do not
                    // replay stale cached data on StackView return.
                    root._dfPendingFrame = false
                }
            }
            console.log("[DOA-VISIBLE] visible=1 fftEnabled=" + root.fftEnabled
                        + " source=" + root.logicalSourceName()
                        + " epoch=" + root._channelSwitchEpoch)
            return
        }

        // Presentation pause only. Keep the user's FFT toggle value. A delayed
        // backend suspend may run if the page remains hidden, but visible=true
        // will re-arm it without requiring the user to toggle FFT again.
        root._rxPendingFrame = false
        root._dfPendingFrame = false
        if (root.fftEnabled)
            hiddenSuspendTimer.restart()
        console.log("[DOA-VISIBLE] visible=0 fftEnabled=" + root.fftEnabled
                    + " source=" + root.logicalSourceName()
                    + " epoch=" + root._channelSwitchEpoch
                    + " hiddenSuspendDelayMs=" + hiddenSuspendTimer.interval)
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
        // Start with FFT OFF every time the DoA page is created. The FFT toggle
        // is intentionally session/page-local so CH1 does not stay running from
        // an older persisted setting.
        root.fftEnabled = false
        root.adaptiveQualityEnabled = uiSettings.adaptiveQualityEnabled
        root.renderQualityLevel = Math.max(0, Math.min(2, Number(uiSettings.renderQualityLevel)))
        root._expectedSourceKey = root.logicalSourceName()

        // กันค่าพัง
        if (root.yMaxDbUser <= root.yMinDbUser + 1)
            root.yMaxDbUser = root.yMinDbUser + 1

        // New logical CH1 is the shared Home/RX source. No setAdcChannel is
        // emitted here; physical DF selection remains untouched until CH2..CH6.
        root.selectDisplayChannel(1, "init")
        root._rxPendingFrame = false
        root._dfPendingFrame = false
    }

    Component.onDestruction: {
        hiddenSuspendTimer.stop()
        root.stopAllFftResources("destruction")
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
    onFftEnabledChanged: root.syncFftResources("fft-enabled-property")
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
            Layout.preferredHeight: 334
            darkMode: root.hmiDarkMode
            fftPlotTarget: fftPlot
            displayChannel: root.displayChannel
            rxSourceAvailable: root.rxSourceAvailable
            fftEnabled: root.fftEnabled
            onDisplayChannelRequested: root.selectDisplayChannel(channel)
            onFftEnabledRequested: root.setFftEnabled(enabled)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            // ================= DOA =================
            HmiPanel {
                Layout.preferredWidth: Math.max(470, Math.min(590, parent.width * 0.35))
                Layout.fillHeight: true
                darkMode: root.hmiDarkMode

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    // Two-line header prevents PEAK/CONF/DOA-state chips from
                    // competing with the title when the polar panel narrows.
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ColumnLayout {
                                spacing: 1
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: "DOA · MUSIC POLAR"
                                    color: hmiTheme.text
                                    font.pixelSize: 14
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.logicalSourceName() + " display · coherent DF result"
                                    color: hmiTheme.muted
                                    font.pixelSize: 9
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }

                            HmiStatusPill {
                                darkMode: root.hmiDarkMode
                                compact: true
                                text: doaClient.doaEnabled ? (doaClient.signalPresent ? "SIGNAL" : "GATED") : "DOA OFF"
                                tone: doaClient.doaEnabled ? (doaClient.signalPresent ? "good" : "warn") : "danger"
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            HmiMetricChip {
                                darkMode: root.hmiDarkMode
                                label: "PEAK"
                                value: Number(doaClient.doaDeg).toFixed(1) + "°"
                                tone: doaClient.signalPresent ? "warn" : "neutral"
                            }
                            HmiMetricChip {
                                darkMode: root.hmiDarkMode
                                label: "CONF"
                                value: Number(doaClient.confidence).toFixed(2)
                                tone: doaClient.signalPresent ? "good" : "neutral"
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: "Band " + Number(doaClient.bandPeakDb).toFixed(1)
                                      + " dB  ·  Th " + Number(doaClient.gateThDb).toFixed(1) + " dB"
                                color: doaClient.signalPresent ? hmiTheme.success : hmiTheme.textSecondary
                                font.pixelSize: 9
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.maximumWidth: 190
                            }
                        }
                    }

                    DoaPolarPlot {
                        darkMode: root.hmiDarkMode
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
            HmiPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                darkMode: root.hmiDarkMode

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ColumnLayout {
                            spacing: 1
                            Text {
                                text: root.spectrumTitle()
                                color: hmiTheme.text
                                font.pixelSize: 14
                                font.bold: true
                            }
                            Text {
                                text: root.rxDisplaySelected
                                      ? "Shared AstraRX/Home receiver source"
                                      : "Physical DF ADC CH" + root.physicalDfChannelForDisplay(root.displayChannel)
                                color: hmiTheme.muted
                                font.pixelSize: 9
                                font.bold: true
                            }
                        }

                        Item { Layout.fillWidth: true }

                        HmiMetricChip {
                            darkMode: root.hmiDarkMode
                            label: "SOURCE"
                            value: root.logicalSourceName()
                            tone: root.fftEnabled ? "info" : "neutral"
                        }
                        HmiStatusPill {
                            darkMode: root.hmiDarkMode
                            compact: true
                            text: root.fftEnabled ? "FFT LIVE" : "FFT OFF"
                            tone: root.fftEnabled ? "good" : "danger"
                        }
                    }

                    // ===== Controls: Y range =====
                    // Scroll only when the FFT panel becomes narrow. This keeps
                    // Auto-Y, min/max and quality telemetry from colliding.
                    HmiHorizontalScroll {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        spacing: 12

                        CheckBox {
                            text: "Auto Y"
                            checked: root.yAuto
                            onToggled: root.yAuto = checked   // ✅ จะไป saveDbSettings() เอง
                        }

                        Text { text: "Min dB"; color: hmiTheme.textSecondary; verticalAlignment: Text.AlignVCenter; font.pixelSize: 11; font.bold: true }

                        SpinBox {
                            from: -200; to: 0
                            value: root.yMinDbUser
                            enabled: !root.yAuto

                            // ✅ ใช้ onValueModified = user เปลี่ยนจริงเท่านั้น
                            onValueModified: root.yMinDbUser = value
                        }

                        Text { text: "Max dB"; color: hmiTheme.textSecondary; verticalAlignment: Text.AlignVCenter; font.pixelSize: 11; font.bold: true }

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
                            color: hmiTheme.textSecondary
                            font.pixelSize: 12
                        }

                        Text {
                            text: "Q " + root._qualityName().toUpperCase()
                                  + " S" + root._spectrumFps()
                                  + "/W" + root._waterfallFps()
                            color: root.adaptiveQualityEnabled ? hmiTheme.success : hmiTheme.warning
                            font.pixelSize: 12
                        }
                    }

                    FftPlot {
                        id: fftPlot
                        darkMode: root.hmiDarkMode
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: parent.height * 0.55
                        Layout.minimumHeight: 220

                        enabled: root.fftEnabled && (root.rxDisplaySelected ? root.rxSourceAvailable : doaClient.connected)
                        freqHz: root.displayFftFreqHz
                        magDb: root.displayFftMagDb
                        fftFps: root._spectrumFps()
                        frameSequence: root.displayFrameSequence
                        sourceKey: root.logicalSourceName()
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
                        darkMode: root.hmiDarkMode
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: parent.height * 0.35
                        Layout.minimumHeight: 160

                        enabled: root.fftEnabled && (root.rxDisplaySelected ? root.rxSourceAvailable : doaClient.connected)
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
                        color: root.waterfallBackgroundForTheme(root.hmiDarkMode)

                        // V28: theme-specific 256-entry palette.  Day mode
                        // uses a light warm floor; night mode uses purple/amber.
                        // Both intentionally differ from the blue FFT spectrum.
                        waterfallColors: root.waterfallPaletteForTheme(root.hmiDarkMode)
                    }
                }
            }
        }
    }
}
