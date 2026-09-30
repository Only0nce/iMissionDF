import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../ui"

HmiPanel {
    id: root
    Theme { id: theme; darkMode: root.darkMode }
    height: 334
    width: parent ? parent.width : 1920
    property int rowH: 44
    property int pad: 10
    property int channelCardWidth: 176
    property int channelCardMinWidth: 156
    property int displayChannel: 1
    property bool rxSourceAvailable: false
    property bool fftEnabled: false
    property int ncoUpdateEnDisplay: root.dc() ? Number(root.dc().ncoUpdateEn) : 31
    signal displayChannelRequested(int channel)
    signal fftEnabledRequested(bool enabled)

    // Channel row uses logical CH1=Home/RX plus DF physical indexes CH0..CH4 shown as CH2..CH6
    property real rfAgcTargetAllDb: -75.0
    // optional target (FftPlot) for controlling Y-axis scale
    property var fftPlotTarget: null

    implicitHeight: 334

    // ---- SAFE ACCESS: doaClient is contextProperty, may be undefined/null early ----
    function dc() {
        return (typeof(doaClient) !== "undefined" && doaClient !== null) ? doaClient : null
    }

    // ========= MHz UI <-> Hz backend helpers =========
    function rfTextToHz(s) {
        var mhz = parseFloat((s || "").toString().trim())
        if (isNaN(mhz) || mhz <= 0) return NaN
        return Math.round(mhz * 1e6)
    }
    function hzToMhzText(hz) {
        var v = Number(hz)
        if (isNaN(v) || v <= 0) return ""
        return (v / 1e6).toFixed(6)
    }

    // ================= RF ATT AGC (per-channel) =================
    property bool rfAgcAvailable: false
    property bool rfAgcEnabledGlobal: true
    property var rfAgcChEnabled: [true,true,true,true,true]   // backend DF CH0..CH4 => UI CH2..CH6
    property var rfAgcTargetDb:  [-55,-55,-55,-55,-55]
    property var rfAgcAttDb:     [0,0,0,0,0]
    property var rfAgcErrDb:     [0,0,0,0,0]
    property var rfAgcPeakDb:    [-200,-200,-200,-200,-200]

    // ✅ NEW: phase per channel (degrees)
    property var rfAgcPhaseDeg:  [0,0,0,0,0]

    property real scannerAttDb: 0.0   // ATT #7 manual (scanner)

    function _clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

    // UI logical channel mapping:
    // CH1 = shared Home/RX FFT source.
    // CH2..CH6 = DF physical ADC channels 0..4 shown in the RF AGC cards below.
    function dfIndexToLogicalChannel(idx) {
        return Number(idx) + 2
    }

    function logicalChannelLabel(logicalChannel) {
        return "CH" + Number(logicalChannel)
    }

    function sendDisplayChannelFromDfIndex(idx) {
        var ch = dfIndexToLogicalChannel(idx)
        root.displayChannelRequested(ch)
    }

    function sendDisplayChannelHomeRx() {
        root.displayChannelRequested(1)
    }

    function sendRfAgcEnable(ch, enable) {
        var c = dc()
        if (!c) return

        var en = !!enable

        // ✅ requirement: when AGC is disabled -> release ATT1..5 back to 0 dB (CH0..CH4)
        // ทำแบบ clone + reassign เพื่อไม่ให้ QML พัง (undefined / bindings not triggered)
        if (!en) {
            var at = (root.rfAgcAttDb && root.rfAgcAttDb.slice) ? root.rfAgcAttDb.slice(0) : [0,0,0,0,0]
            var pk = (root.rfAgcPeakDb && root.rfAgcPeakDb.slice) ? root.rfAgcPeakDb.slice(0) : [-200,-200,-200,-200,-200]
            var er = (root.rfAgcErrDb  && root.rfAgcErrDb.slice)  ? root.rfAgcErrDb.slice(0)  : [0,0,0,0,0]

            for (var i = 0; i < 5; ++i) {
                at[i] = 0
                pk[i] = -200
                er[i] = 0
            }

            root.rfAgcAttDb  = at
            root.rfAgcPeakDb = pk
            root.rfAgcErrDb  = er
        }

        if (c.setRfAgcEnable !== undefined && typeof c.setRfAgcEnable === "function") {
            try { c.setRfAgcEnable(ch, en, true) }
            catch(e) { try { c.setRfAgcEnable(ch, en) } catch(e2) {} }
            return
        }

        if (c.sendJson !== undefined && typeof c.sendJson === "function") {
            c.sendJson({ menuID: "setRfAgcEnable", ch: ch, enable: en, needAck: true })
        }
    }

    function sendRfAgcTarget(ch, targetDb) {
        var c = dc()
        if (!c) return

        var v = Number(targetDb)
        if (isNaN(v)) return
        v = _clamp(v, -120, 0)

        // ✅ requirement: target 1..5 should be adjusted together (CH0..CH4)
        // update UI arrays (clone -> edit -> reassign)
        var tg = (root.rfAgcTargetDb && root.rfAgcTargetDb.slice) ? root.rfAgcTargetDb.slice(0) : [-75,-75,-75,-75,-75]
        for (var i = 0; i < 5; ++i) tg[i] = v
        root.rfAgcTargetDb = tg

        // send to server: setRfAgcChannel for all 0..4
        if (c.setRfAgcChannel !== undefined && typeof c.setRfAgcChannel === "function") {
            for (var k = 0; k < 5; ++k) {
                try { c.setRfAgcChannel(k, v, true) }
                catch(e) { try { c.setRfAgcChannel(k, v) } catch(e2) {} }
            }
            return
        }

        if (c.sendJson !== undefined && typeof c.sendJson === "function") {
            for (var kk = 0; kk < 5; ++kk) {
                c.sendJson({ menuID: "setRfAgcChannel", ch: kk, target_db: v, needAck: true })
            }
        }
    }

    function sendScannerAttDb(attDb) {
        var c = dc()
        if (!c) return

        var adb = Number(attDb)
        if (isNaN(adb)) return

        // clamp to PE43711 typical range 0..31.75 (0.25 step)
        if (adb < 0.0) adb = 0.0
        if (adb > 31.75) adb = 31.75
        adb = Math.round(adb * 4) / 4.0   // ✅ force 0.25 step

        if (root.scannerAttDb !== undefined) root.scannerAttDb = adb

        if (c.setScannerAttDb !== undefined && typeof c.setScannerAttDb === "function") {
            try { c.setScannerAttDb(adb, true) }
            catch(e) { try { c.setScannerAttDb(adb) } catch(e2) {} }
            return
        }

        if (c.sendJson !== undefined && typeof c.sendJson === "function") {
            c.sendJson({ menuID: "setScannerAttDb", att_db: adb, needAck: true })
        }
    }

    function syncScannerAttFromClient() {
        var c = dc()
        if (!c) return
        if (c.scannerAttDb === undefined) return

        var v = Number(c.scannerAttDb)
        if (isNaN(v)) return
        if (v < 0.0) v = 0.0
        if (v > 31.75) v = 31.75
        v = Math.round(v * 4) / 4.0

        root.scannerAttDb = v
    }

    // ✅ helper: convert rad->deg
    function _radToDeg(r) { return r * (180.0 / Math.PI) }

    function syncRfAgcFromClient() {
        var c = dc()
        if (!c) return

        if (c.rfAgcAvailable !== undefined) rfAgcAvailable = !!c.rfAgcAvailable
        if (c.rfAgcEnabled   !== undefined) rfAgcEnabledGlobal = !!c.rfAgcEnabled

        var chs = null
        if (c.rfAgcChannels !== undefined) chs = c.rfAgcChannels
        if (!chs || chs.length === undefined) return

        // ✅ clone arrays
        var en = rfAgcChEnabled.slice(0)
        var tg = rfAgcTargetDb.slice(0)
        var at = rfAgcAttDb.slice(0)
        var pk = rfAgcPeakDb.slice(0)
        var ph = rfAgcPhaseDeg.slice(0)

        // ✅ ถ้า disable AGC: ปล่อย ATT 1-5 เป็น 0 และปิดช่องทั้งหมด
        if (!rfAgcEnabledGlobal) {
            for (var j = 0; j < 5; j++) {
                en[j] = false
                at[j] = 0.0
            }
        }

        // ✅ sync จาก server
        for (var k = 0; k < chs.length; k++) {
            var o = chs[k]
            var ch = Number(o.ch)
            if (isNaN(ch) || ch < 0 || ch > 4) continue

            if (o.enabled !== undefined) en[ch] = !!o.enabled
            if (o.target_db !== undefined) tg[ch] = Number(o.target_db)

            if (o.att_db !== undefined) at[ch] = rfAgcEnabledGlobal ? Number(o.att_db) : 0.0

            if (o.band_db !== undefined) pk[ch] = Number(o.band_db)
            else if (o.band_peak_db !== undefined) pk[ch] = Number(o.band_peak_db)

            // ✅ NEW: phase (accept many key names)
            var gotPhase = false
            if (o.phase_deg !== undefined) { ph[ch] = Number(o.phase_deg); gotPhase = true }
            else if (o.phaseDeg !== undefined) { ph[ch] = Number(o.phaseDeg); gotPhase = true }
            else if (o.phase !== undefined) { ph[ch] = Number(o.phase); gotPhase = true }          // assume degrees
            else if (o.phase_rad !== undefined) { ph[ch] = _radToDeg(Number(o.phase_rad)); gotPhase = true }
            else if (o.phaseRad !== undefined) { ph[ch] = _radToDeg(Number(o.phaseRad)); gotPhase = true }

            // normalize
            if (gotPhase) {
                if (isNaN(ph[ch])) ph[ch] = 0
                // wrap to [-180..180] (optional but nice)
                while (ph[ch] > 180) ph[ch] -= 360
                while (ph[ch] < -180) ph[ch] += 360
            }
        }

        // หลังจากอ่าน tg[] แล้ว ให้เอาค่าช่องแรกมาเป็นค่ากลาง
        if (tg.length > 0 && tg[0] !== undefined && !isNaN(Number(tg[0]))) {
            rfAgcTargetAllDb = Number(tg[0])
        }

        // ✅ re-assign trigger bindings
        rfAgcChEnabled = en
        rfAgcTargetDb  = tg
        rfAgcAttDb     = at
        rfAgcPeakDb    = pk
        rfAgcPhaseDeg  = ph
    }

    // ===== DOA algorithm selection =====
    function algoToIndex(a) {
        a = (a || "").toString().toLowerCase().trim()
        if (a === "music_1d") return 0
        if (a === "uca_rb_music") return 1
        if (a === "uca_esprit") return 2
        if (a.indexOf("esprit") !== -1) return 2
        if (a.indexOf("rb") !== -1) return 1
        if (a.indexOf("music") !== -1) return 0
        return 0
    }

    function indexToAlgo(i) {
        if (i === 2) return "uca_esprit"
        if (i === 1) return "uca_rb_music"
        return "music_1d"
    }

    // ========= helper: call method if exists, otherwise set property =========
    function setIfExists(methodName, propName, value, needAck) {
        var c = dc()
        if (!c) return
        if (c[methodName] !== undefined && typeof c[methodName] === "function") {
            try { c[methodName](value, needAck) }
            catch(e) { try { c[methodName](value) } catch(e2) { } }
            return
        }
        if (propName && (c[propName] !== undefined)) {
            c[propName] = value
        }
    }

    Timer {
        interval: 200
        running: root.dc() ? root.dc().connected : false
        repeat: true
        onTriggered: root.syncRfAgcFromClient()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.pad
        spacing: 8

        // ===================== Row 1 =====================
        HmiHorizontalScroll {
            // Row-1 controls have a 64 px preferred height. Keep the viewport
            // equally tall so the ComboBox / DOA / FFT panels are not clipped.
            Layout.preferredHeight: 64
            Layout.fillWidth: true
            spacing: 10

            HmiButton {
                darkMode: root.darkMode
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: 110
                tone: (root.dc() && root.dc().connected) ? "normal" : "primary"
                text: (root.dc() && root.dc().connected) ? "DISCONNECT" : "CONNECT"
                onClicked: {
                    var c = root.dc()
                    if (!c) return
                    c.connected ? c.disconnectFromServer() : c.connectToServer()
                }
            }

            HmiTextField {
                darkMode: root.darkMode
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: 150
                text: root.dc() ? root.dc().host : ""
                placeholderText: "IP"
                onEditingFinished: {
                    var c = root.dc()
                    if (!c) return
                    c.host = text
                }
            }

            HmiTextField {
                darkMode: root.darkMode
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: 80
                inputMethodHints: Qt.ImhDigitsOnly
                text: root.dc() ? String(root.dc().port) : "5555"
                placeholderText: "Port"
                onEditingFinished: {
                    var c = root.dc()
                    if (!c) return
                    c.port = parseInt(text)
                }
            }

            HmiButton {
                darkMode: root.darkMode
                compact: true
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: 76
                text: "STATE"
                enabled: root.dc() ? root.dc().connected : false
                onClicked: {
                    var c = root.dc()
                    if (!c) return
                    c.requestState()
                }
            }

            Rectangle { width: 1; Layout.fillHeight: true; color: theme.line; opacity: 0.8 }

            // ===== Algorithm selector (3 modes) =====
            HmiPanel {
                darkMode: root.darkMode
                Layout.preferredHeight: root.rowH + 20
                Layout.preferredWidth: 280

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        text: "DOA Algo"
                        color: theme.text
                        font.pixelSize: 12
                        font.bold: true
                        Layout.preferredWidth: 70
                        elide: Text.ElideRight
                    }

                    HmiComboBox {
                        id: algoCombo
                        darkMode: root.darkMode
                        Layout.fillWidth: true
                        enabled: root.dc() ? root.dc().connected : false
                        model: [
                            "MUSIC (STD)",
                            "MUSIC (UCA-RB)",
                            "ESPRIT (UCA Peak only)"
                        ]

                        function algoToIndex(a) {
                            a = (a || "").toString().toLowerCase().trim()
                            if (a === "music_1d") return 0
                            if (a === "uca_rb_music") return 1
                            if (a === "uca_esprit") return 2
                            if (a.indexOf("esprit") !== -1) return 2
                            if (a.indexOf("rb") !== -1) return 1
                            return 0
                        }
                        function indexToAlgo(i) {
                            if (i === 2) return "uca_esprit"
                            if (i === 1) return "uca_rb_music"
                            return "music_1d"
                        }

                        Component.onCompleted: {
                            var c = root.dc()
                            currentIndex = algoToIndex(c ? c.doaAlgo : "music_1d")
                        }

                        onActivated: {
                            var c = root.dc()
                            if (!c) return
                            var a = indexToAlgo(currentIndex)
                            if (c.setDoaAlgorithm !== undefined && typeof c.setDoaAlgorithm === "function")
                                c.setDoaAlgorithm(a, true)
                            else
                                c.doaAlgo = a
                        }

                        Connections {
                            target: root.dc()
                            function onDoaAlgoChanged() {
                                var c = root.dc()
                                if (!c) return
                                algoCombo.currentIndex = algoCombo.algoToIndex(c.doaAlgo)
                            }
                        }
                    }
                }
            }

            DoaControlPanel {
                darkMode: root.darkMode
                Layout.preferredHeight: root.rowH + 20
                Layout.preferredWidth: 196
            }

            FftControlPanel {
                darkMode: root.darkMode
                Layout.preferredHeight: root.rowH + 20
                Layout.preferredWidth: 328
                displayChannel: root.displayChannel
                rxSourceAvailable: root.rxSourceAvailable
                fftEnabled: root.fftEnabled
                onDisplayChannelRequested: root.displayChannelRequested(channel)
                onFftEnabledRequested: root.fftEnabledRequested(enabled)
            }

            HmiButton {
                darkMode: root.darkMode
                compact: true
                Layout.preferredHeight: root.rowH + 20
                Layout.preferredWidth: 76
                text: "FFT Y"
                enabled: (root.fftPlotTarget !== null)
                onClicked: fftYPopup.open()
            }

            Item { Layout.fillWidth: true }

            HmiStatusPill {
                darkMode: root.darkMode
                compact: true
                Layout.preferredWidth: 210
                Layout.maximumWidth: 230
                text: {
                    var c = root.dc()
                    if (!c) return "NO CLIENT"
                    return c.connected ? ("CONNECTED · " + c.doaAlgo) : "DISCONNECTED"
                }
                tone: (root.dc() && root.dc().connected) ? "good" : "danger"
            }
        }

        // ===================== Row 2: Frequency + Array =====================
        HmiHorizontalScroll {
            Layout.preferredHeight: 60
            Layout.fillWidth: true
            spacing: 8

            Text { text: "RF CENTER"; color: theme.textSecondary; font.pixelSize: 11; font.bold: true }

            // ===== RF input: UI MHz, backend Hz =====
            HmiTextField {
                id: freqField
                darkMode: root.darkMode
                emphasized: true
                Layout.preferredWidth: 200

                inputMethodHints: Qt.ImhFormattedNumbersOnly
                validator: DoubleValidator {
                    bottom: 0.001
                    top: 6000.0
                    decimals: 6
                    notation: DoubleValidator.StandardNotation
                }

                text: {
                    var c = root.dc()
                    return c ? root.hzToMhzText(c.fcHz) : "130.000"
                }

                placeholderText: "e.g. 130.000"
                selectByMouse: true

                onEditingFinished: {
                    var c = root.dc()
                    if (!c) return

                    var hz = root.rfTextToHz(text)
                    if (isNaN(hz)) {
                        text = root.hzToMhzText(c.fcHz)
                        return
                    }

                    // set FC in Hz (no dependency on undefined root.sendSetFcHz)
                    if (c.setFcHz !== undefined && typeof c.setFcHz === "function")
                        c.setFcHz(hz, true)
                    else if (c.fcHz !== undefined)
                        c.fcHz = hz

                    text = root.hzToMhzText(hz)
                }
            }

            Text { text: "UPDATE EN"; color: theme.muted; font.pixelSize: 10; font.bold: true }

            // Read-only backend update-enable value. A disabled SpinBox looked like
            // "- 31 +" and visually collided with the surrounding labels in dark mode.
            // Keep the exact backend state but present it as a compact metric instead.
            HmiMetricChip {
                id: updateEnSpin
                darkMode: root.darkMode
                compact: true
                label: "EN"
                value: String(root.ncoUpdateEnDisplay)
                tone: "neutral"
                Layout.preferredWidth: 66

                Connections {
                    target: root.dc()
                    function onNcoUpdateEnChanged() {
                        var c = root.dc()
                        if (!c) return
                        root.ncoUpdateEnDisplay = Number(c.ncoUpdateEn)
                    }
                }
            }

            HmiButton {
                darkMode: root.darkMode
                compact: true
                tone: "primary"
                text: "SET NCO"
                enabled: root.dc() ? root.dc().connected : false
                onClicked: {
                    var c = root.dc()
                    if (!c) return

                    // ✅ freqField is MHz -> convert to Hz
                    var fHz = root.rfTextToHz(freqField.text)
                    if (isNaN(fHz) || fHz <= 0) return

                    // Production baseline intentionally sends the validated update_en=31.
                    c.setFrequencyHz(fHz, 31, true)
                }
            }

            Rectangle { width: 1; Layout.fillHeight: true; color: theme.line; opacity: 0.8 }

            Text { text: "ARRAY RADIUS"; color: theme.textSecondary; font.pixelSize: 11; font.bold: true }

            HmiTextField {
                id: radiusField
                darkMode: root.darkMode
                compact: true
                Layout.preferredWidth: 90
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                text: {
                    var c = root.dc()
                    if (!c) return "0.80"
                    return (c.ucaRadiusM !== undefined) ? Number(c.ucaRadiusM).toFixed(2) : "0.80"
                }
                placeholderText: "0.80"
            }

            Text { text: "ANT N"; color: theme.muted; font.pixelSize: 10; font.bold: true }

            SpinBox {
                id: numAntSpin
                from: 2
                to: 16
                value: {
                    var c = root.dc()
                    if (!c) return 5
                    return (c.numAntennas !== undefined) ? Number(c.numAntennas) : 5
                }
                editable: true
                Layout.preferredWidth: 100
            }

            HmiButton {
                darkMode: root.darkMode
                compact: true
                text: "APPLY ARRAY"
                enabled: root.dc() ? root.dc().connected : false
                onClicked: {
                    var c = root.dc()
                    if (!c) return
                    var r = Number(radiusField.text)
                    if (!isNaN(r) && r > 0) root.setIfExists("setUcaRadiusM", "ucaRadiusM", r, true)
                    var n = parseInt(numAntSpin.value)
                    if (!isNaN(n) && n >= 2) root.setIfExists("setNumAntennas", "numAntennas", n, true)
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: {
                    var c = root.dc()
                    if (!c) return "Current: --"
                    return "Current: " + (c.fcHz/1e6).toFixed(6) + " MHz (" + c.fcHz.toFixed(0) + " Hz)"
                }
                color: theme.success
                font.bold: true
            }
        }

        // ===================== Row 2 (Tx Hz) =====================
        HmiHorizontalScroll {
            Layout.preferredHeight: 60
            Layout.fillWidth: true
            spacing: 10

            HmiPanel {
                darkMode: root.darkMode
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: 360

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 10

                    Text { text: "TX RATE"; color: theme.text; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 60 }

                    Slider {
                        id: txSlider
                        Layout.fillWidth: true
                        from: 0.2
                        to: 60.0
                        stepSize: 0.1
                        value: root.dc() ? root.dc().txHz : 10.0
                        enabled: root.dc() ? root.dc().connected : false
                        onPressedChanged: {
                            var c = root.dc()
                            if (!c) return
                            if (!pressed && c.connected) c.txHz = value
                        }

                        Connections {
                            target: root.dc()
                            function onTxHzChanged() {
                                var c = root.dc()
                                if (!c) return
                                if (!txSlider.pressed) txSlider.value = c.txHz
                            }
                        }
                    }

                    Text {
                        Layout.preferredWidth: 80
                        horizontalAlignment: Text.AlignRight
                        text: (root.dc() ? root.dc().txHz.toFixed(1) : "10.0") + " Hz"
                        color: theme.info
                        font.pixelSize: 12
                    }
                }
            }

            DoaTonePanel {
                darkMode: root.darkMode
                Layout.preferredHeight: root.rowH
                Layout.fillWidth: true
            }
        }

        // ===================== RF AGC cards =====================
        HmiHorizontalScroll {
            Layout.preferredHeight: 104
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: "RF AGC"
                color: theme.text
                font.pixelSize: 12
                font.bold: true
                Layout.preferredWidth: 70
            }

            Text {
                text: root.rfAgcAvailable ? "available" : "n/a"
                color: root.rfAgcAvailable ? theme.success : theme.danger
                font.pixelSize: 12
                Layout.preferredWidth: 80
            }

            Switch {
                id: rfAgcGlobalSwitch
                enabled: root.dc() ? root.dc().connected : false
                checked: root.rfAgcEnabledGlobal
                onToggled: root.sendRfAgcEnable(-1, checked)
            }

            // ----- Scanner Attenuator (ATT #7) -----
            HmiPanel {
                darkMode: root.darkMode
                Layout.preferredWidth: 170
                Layout.minimumWidth: 152
                Layout.preferredHeight: 88

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Text {
                        text: "SCANNER ATT #7"
                        color: theme.text
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 6

                        Slider {
                            id: scannerAttSlider
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            from: 0
                            to: 31.75
                            stepSize: 0.25
                            value: root.scannerAttDb
                            enabled: root.dc() ? root.dc().connected : false

                            onMoved: scannerAttField.text = Number(value).toFixed(2)
                            onPressedChanged: if (!pressed) root.sendScannerAttDb(value)
                        }

                        HmiTextField {
                            id: scannerAttField
                            darkMode: root.darkMode
                            compact: true
                            Layout.preferredWidth: 58
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredHeight: 28

                            text: Number(root.scannerAttDb).toFixed(2)
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                            clip: true

                            onEditingFinished: root.sendScannerAttDb(text)
                        }
                    }
                }
            }

            HmiPanel {
                darkMode: root.darkMode
                Layout.preferredWidth: 180
                Layout.minimumWidth: 160
                Layout.preferredHeight: 88

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 4

                    Text {
                        text: "DF TARGET · CH2..CH6"
                        color: theme.textSecondary
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Slider {
                            id: rfTargetAllSlider
                            Layout.fillWidth: true
                            from: -90
                            to: -30
                            stepSize: 0.5
                            enabled: rfAgcGlobalSwitch.checked && (root.dc() ? root.dc().connected : false)
                            value: Number(root.rfAgcTargetAllDb)

                            onPressedChanged: {
                                if (!pressed) {
                                    root.rfAgcTargetAllDb = value
                                    root.sendRfAgcTarget(0, value)
                                }
                            }
                        }

                        Text {
                            text: Number(root.rfAgcTargetAllDb).toFixed(1) + " dB"
                            color: theme.info
                            font.pixelSize: 11
                            Layout.preferredWidth: 60
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            HmiPanel {
                id: rxChannelCard
                darkMode: root.darkMode
                Layout.preferredHeight: 88
                Layout.preferredWidth: root.channelCardWidth
                Layout.minimumWidth: root.channelCardMinWidth
                clip: true

                readonly property int logicalChannel: 1
                readonly property bool selectedForFft: root.displayChannel === logicalChannel
                readonly property bool hovered: rxCardMouse.containsMouse

                color: selectedForFft
                       ? (root.darkMode ? "#123F3B" : "#E8F8F5")
                       : (hovered ? theme.cardAlt : theme.panel)
                border.width: selectedForFft ? 2 : 1
                border.color: selectedForFft ? theme.accent : (hovered ? theme.lineStrong : theme.line)

                MouseArea {
                    id: rxCardMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.sendDisplayChannelHomeRx()
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: root.logicalChannelLabel(rxChannelCard.logicalChannel)
                            color: rxChannelCard.selectedForFft ? theme.accent : theme.text
                            font.pixelSize: 12
                            font.bold: true
                            Layout.preferredWidth: 30
                        }

                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            text: Number(root.dc() ? root.dc().bandPeakDb : -200).toFixed(1) + " dB"
                            color: rxChannelCard.selectedForFft ? theme.accent : theme.info
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.rxSourceAvailable ? "HOME / RX FFT" : "HOME / RX WAIT"
                        color: theme.textSecondary
                        font.pixelSize: 10
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: {
                            var c = root.dc()
                            if (!c) return "Band --   Th --"
                            return "Band " + Number(c.bandPeakDb).toFixed(1)
                                   + "   Th " + Number(c.gateThDb).toFixed(1)
                        }
                        color: rxChannelCard.selectedForFft ? theme.accent : theme.warn
                        font.pixelSize: 9
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }
            }

            Repeater {
                Layout.fillHeight: true
                // Physical DF ADC cards are 0..4, displayed as logical FFT CH2..CH6.
                model: 5
                delegate: HmiPanel {
                    id: dfChannelCard
                    darkMode: root.darkMode
                    Layout.preferredHeight: 88
                    Layout.preferredWidth: root.channelCardWidth
                    Layout.minimumWidth: root.channelCardMinWidth
                    clip: true

                    readonly property int logicalChannel: root.dfIndexToLogicalChannel(index)
                    readonly property bool selectedForFft: root.displayChannel === logicalChannel
                    readonly property bool hovered: dfCardMouse.containsMouse

                    color: selectedForFft
                           ? (root.darkMode ? "#123F3B" : "#E8F8F5")
                           : (hovered ? theme.cardAlt : theme.panel)
                    border.width: selectedForFft ? 2 : 1
                    border.color: selectedForFft ? theme.accent : (hovered ? theme.lineStrong : theme.line)

                    MouseArea {
                        id: dfCardMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.sendDisplayChannelFromDfIndex(index)
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: root.logicalChannelLabel(dfChannelCard.logicalChannel)   // UI: CH2..CH6, backend index remains 0..4
                                color: dfChannelCard.selectedForFft ? theme.accent : theme.text
                                font.pixelSize: 12
                                font.bold: true
                                Layout.preferredWidth: 32
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignRight
                                text: Number(root.rfAgcPeakDb[index]).toFixed(1) + " dB"
                                color: dfChannelCard.selectedForFft ? theme.accent : theme.info
                                font.pixelSize: 12
                                font.bold: true
                                elide: Text.ElideRight
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "ATT " + Number(root.rfAgcAttDb[index]).toFixed(1)
                                  + "   TARGET " + Number(root.rfAgcTargetAllDb).toFixed(1) + " dB"
                            color: theme.textSecondary
                            font.pixelSize: 9
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: {
                                var c = root.dc()
                                if (!c || !c.phaseDebug) return "φ --   C --   RMS --"

                                var pd = c.phaseDebug
                                var ph = (pd.phase_deg && pd.phase_deg.length > index)
                                            ? Number(pd.phase_deg[index]) : 0
                                var co = (pd.coh && pd.coh.length > index)
                                            ? Number(pd.coh[index]) : 0
                                var rm = (pd.rms_dbfs && pd.rms_dbfs.length > index)
                                            ? Number(pd.rms_dbfs[index]) : 0

                                return "φ " + ph.toFixed(2) + "°   C " + co.toFixed(3)
                                     + "   RMS " + rm.toFixed(1)
                            }
                            color: theme.warning
                            font.pixelSize: 9
                            elide: Text.ElideRight
                        }

                        // Preserve the per-channel enable control contract. It remains
                        // hidden because the current product UX uses the global RF AGC
                        // switch, exactly as before this visual cleanup.
                        Switch {
                            visible: false
                            checked: !!root.rfAgcChEnabled[index]
                            enabled: rfAgcGlobalSwitch.checked && (root.dc() ? root.dc().connected : false)
                            onToggled: root.sendRfAgcEnable(index, checked) // backend: 0..4
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }
    }

    // ===================== FFT Y-Axis Menu (Popup) =====================
    Popup {
        id: fftYPopup
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: Math.max(10, root.width - width - 10)
        y: root.pad
        width: 320
        height: 200

        background: Rectangle {
            radius: theme.radiusLg
            color: theme.panel
            border.color: theme.lineStrong
            border.width: 1
        }

        onOpened: {
            if (!root.fftPlotTarget) return
            autoSwitch.checked = !!root.fftPlotTarget.yAuto
            minField.text = Number(root.fftPlotTarget.yMinDb).toFixed(1)
            maxField.text = Number(root.fftPlotTarget.yMaxDb).toFixed(1)
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Text {
                text: "FFT Y-Axis"
                color: theme.text
                font.pixelSize: 14
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text { text: "Auto"; color: theme.textSecondary; Layout.preferredWidth: 60 }
                Switch {
                    id: autoSwitch
                    checked: true
                    onToggled: {
                        if (!root.fftPlotTarget) return
                        root.fftPlotTarget.yAuto = checked
                    }
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text { text: "Min dB"; color: theme.textSecondary; Layout.preferredWidth: 60 }
                HmiTextField {
                    id: minField
                    darkMode: root.darkMode
                    Layout.fillWidth: true
                    enabled: !autoSwitch.checked
                    placeholderText: "e.g. -95"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text { text: "Max dB"; color: theme.textSecondary; Layout.preferredWidth: 60 }
                HmiTextField {
                    id: maxField
                    darkMode: root.darkMode
                    Layout.fillWidth: true
                    enabled: !autoSwitch.checked
                    placeholderText: "e.g. -10"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                HmiButton {
                    darkMode: root.darkMode
                    tone: "primary"
                    text: "APPLY"
                    enabled: !autoSwitch.checked && (root.fftPlotTarget !== null)
                    onClicked: {
                        if (!root.fftPlotTarget) return
                        var lo = Number(minField.text)
                        var hi = Number(maxField.text)
                        if (isNaN(lo) || isNaN(hi)) return
                        root.fftPlotTarget.yMinDb = lo
                        root.fftPlotTarget.yMaxDb = hi
                        root.fftPlotTarget.yAuto = false
                        fftYPopup.close()
                    }
                }

                HmiButton {
                    darkMode: root.darkMode
                    text: "CLOSE"
                    onClicked: fftYPopup.close()
                }

                Item { Layout.fillWidth: true }
            }
        }
    }
}
