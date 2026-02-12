import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    radius: 12
    color: "#071025"
    border.color: "#20304A"
    border.width: 1
    height: 240
    width: 1920
    property int rowH: 44
    property int pad: 10

    // Target dB ใช้ตัวเดียวสำหรับ CH0..CH4
    property real rfAgcTargetAllDb: -75.0
    // optional target (FftPlot) for controlling Y-axis scale
    property var fftPlotTarget: null

    implicitHeight: (rowH * 3) + (pad * 2) + 8

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
    property var rfAgcChEnabled: [true,true,true,true,true]   // CH0..CH4
    property var rfAgcTargetDb:  [-55,-55,-55,-55,-55]
    property var rfAgcAttDb:     [0,0,0,0,0]
    property var rfAgcErrDb:     [0,0,0,0,0]
    property var rfAgcPeakDb:    [-200,-200,-200,-200,-200]

    // ✅ NEW: phase per channel (degrees)
    property var rfAgcPhaseDeg:  [0,0,0,0,0]

    property real scannerAttDb: 0.0   // ATT #7 manual (scanner)

    function _clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

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
        RowLayout {
            Layout.preferredHeight: 60
            Layout.fillWidth: true
            spacing: 10

            Button {
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: 120
                text: (root.dc() && root.dc().connected) ? "Disconnect" : "Connect"
                onClicked: {
                    var c = root.dc()
                    if (!c) return
                    c.connected ? c.disconnectFromServer() : c.connectToServer()
                }
            }

            TextField {
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: 180
                text: root.dc() ? root.dc().host : ""
                placeholderText: "IP"
                onEditingFinished: {
                    var c = root.dc()
                    if (!c) return
                    c.host = text
                }
            }

            TextField {
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: 90
                inputMethodHints: Qt.ImhDigitsOnly
                text: root.dc() ? String(root.dc().port) : "5555"
                placeholderText: "Port"
                onEditingFinished: {
                    var c = root.dc()
                    if (!c) return
                    c.port = parseInt(text)
                }
            }

            Button {
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: 90
                text: "State"
                enabled: root.dc() ? root.dc().connected : false
                onClicked: {
                    var c = root.dc()
                    if (!c) return
                    c.requestState()
                }
            }

            Rectangle { width: 1; Layout.fillHeight: true; color: "#223049"; opacity: 0.7 }

            // ===== Algorithm selector (3 modes) =====
            Rectangle {
                Layout.preferredHeight: root.rowH + 20
                Layout.preferredWidth: 300
                radius: 10
                color: "#0B1220"
                border.color: "#223049"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        text: "DOA Algo"
                        color: "#E5E7EB"
                        font.pixelSize: 13
                        Layout.preferredWidth: 70
                        elide: Text.ElideRight
                    }

                    ComboBox {
                        id: algoCombo
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
                Layout.preferredHeight: root.rowH + 20
                Layout.preferredWidth: 240
            }

            FftControlPanel {
                Layout.preferredHeight: root.rowH + 20
                Layout.preferredWidth: 320
            }

            Button {
                Layout.preferredHeight: root.rowH + 20
                Layout.preferredWidth: 90
                text: "FFT Y"
                enabled: (root.fftPlotTarget !== null)
                onClicked: fftYPopup.open()
            }

            Item { Layout.fillWidth: true }

            Text {
                Layout.preferredWidth: 320
                horizontalAlignment: Text.AlignRight
                text: {
                    var c = root.dc()
                    if (!c) return ""
                    return c.statusText + " | " + c.doaAlgo
                }
                color: (root.dc() && root.dc().connected) ? "#22c55e" : "#f87171"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        // ===================== Row 2: Frequency + Array =====================
        RowLayout {
            Layout.preferredHeight: 60
            Layout.fillWidth: true
            spacing: 8

            Text { text: "RF (MHz)"; color: "#cccccc" }

            // ===== RF input: UI MHz, backend Hz =====
            TextField {
                id: freqField
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

            Text { text: "update_en"; color: "#cccccc" }

            SpinBox {
                id: updateEnSpin
                from: 0
                to: 63
                value: root.dc() ? root.dc().ncoUpdateEn : 31
                editable: true
                Layout.preferredWidth: 110
                Connections {
                    target: root.dc()
                    function onNcoUpdateEnChanged() {
                        var c = root.dc()
                        if (!c) return
                        updateEnSpin.value = c.ncoUpdateEn
                    }
                }
            }

            Button {
                text: "Set NCO"
                enabled: root.dc() ? root.dc().connected : false
                onClicked: {
                    var c = root.dc()
                    if (!c) return

                    // ✅ freqField is MHz -> convert to Hz
                    var fHz = root.rfTextToHz(freqField.text)
                    if (isNaN(fHz) || fHz <= 0) return

                    c.setFrequencyHz(fHz, updateEnSpin.value, true)
                }
            }

            Rectangle { width: 1; Layout.fillHeight: true; color: "#223049"; opacity: 0.7 }

            Text { text: "Radius (m)"; color: "#cccccc" }

            TextField {
                id: radiusField
                Layout.preferredWidth: 90
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                text: {
                    var c = root.dc()
                    if (!c) return "0.80"
                    return (c.ucaRadiusM !== undefined) ? Number(c.ucaRadiusM).toFixed(2) : "0.80"
                }
                placeholderText: "0.80"
            }

            Text { text: "N"; color: "#cccccc" }

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

            Button {
                text: "Apply Array"
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
                color: "#6fbf73"
                font.bold: true
            }
        }

        // ===================== Row 2 (Tx Hz) =====================
        RowLayout {
            Layout.preferredHeight: 60
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: 360
                radius: 10
                color: "#0B1220"
                border.color: "#223049"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 10

                    Text { text: "Tx Hz"; color: "#E5E7EB"; font.pixelSize: 13; Layout.preferredWidth: 50 }

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
                        color: "#93c5fd"
                        font.pixelSize: 13
                    }
                }
            }

            DoaTonePanel {
                Layout.preferredHeight: root.rowH
                Layout.fillWidth: true
            }
        }

        // ===================== RF AGC cards =====================
        RowLayout {
            Layout.preferredHeight: 90
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: "RF AGC"
                color: "#E5E7EB"
                font.pixelSize: 13
                Layout.preferredWidth: 70
            }

            Text {
                text: root.rfAgcAvailable ? "available" : "n/a"
                color: root.rfAgcAvailable ? "#22c55e" : "#f87171"
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
            Rectangle {
                radius: 8
                color: "#0B1220"
                border.color: "#223049"
                border.width: 1
                Layout.preferredWidth: 210
                Layout.preferredHeight: 75

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Text {
                        text: "Scanner ATT #7"
                        color: "#E5E7EB"
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

                        TextField {
                            id: scannerAttField
                            Layout.preferredWidth: 58
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredHeight: 28

                            text: Number(root.scannerAttDb).toFixed(2)
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            color: "#E5E7EB"
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                            rightPadding: 8
                            topPadding: 0
                            bottomPadding: 0
                            clip: true

                            background: Rectangle {
                                radius: 6
                                color: "#111A24"
                                border.color: "#223049"
                                border.width: 1
                            }

                            onEditingFinished: root.sendScannerAttDb(text)
                        }
                    }
                }
            }

            Rectangle {
                radius: 8
                color: "#071025"
                border.color: "#20304A"
                border.width: 1
                Layout.preferredWidth: 200
                Layout.preferredHeight: 75

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 4

                    Text {
                        text: "Target (DF CH1..CH5)"
                        color: "#CBD5E1"
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
                            color: "#93c5fd"
                            font.pixelSize: 11
                            Layout.preferredWidth: 60
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            Repeater {
                Layout.fillHeight: true
                model: 5
                delegate: Rectangle {
                    Layout.preferredHeight: 75
                    Layout.preferredWidth: 240
                    radius: 8
                    color: "#071025"
                    border.color: "#20304A"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 2

                        Text {
                            text: "CH" + (index + 1)   // UI: CH1..CH5 (backend still uses index 0..4)
                            color: "#CBD5E1"
                            font.pixelSize: 13
                            Layout.preferredWidth: 32
                        }

                        Switch {
                            visible: false
                            checked: !!root.rfAgcChEnabled[index]
                            enabled: rfAgcGlobalSwitch.checked && (root.dc() ? root.dc().connected : false)
                            onToggled: root.sendRfAgcEnable(index, checked) // backend: 0..4
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignRight
                                text: {
                                    var pk = Number(root.rfAgcPeakDb[index]).toFixed(1)
                                    var at = Number(root.rfAgcAttDb[index]).toFixed(1)
                                    var tg = Number(root.rfAgcTargetAllDb).toFixed(1)
                                    return pk + " dB | ATT " + at + " | T " + tg
                                }
                                color: "#93c5fd"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignRight
                                text: {
                                    var c = root.dc()
                                    if (!c || !c.phaseDebug) return ""

                                    var pd = c.phaseDebug
                                    var ph = (pd.phase_deg && pd.phase_deg.length > index)
                                                ? Number(pd.phase_deg[index]) : 0
                                    var co = (pd.coh && pd.coh.length > index)
                                                ? Number(pd.coh[index]) : 0
                                    var rm = (pd.rms_dbfs && pd.rms_dbfs.length > index)
                                                ? Number(pd.rms_dbfs[index]) : 0

                                    return "φ " + ph.toFixed(2) + "°"
                                         + " | coh " + co.toFixed(3)
                                         + " | rms " + rm.toFixed(1) + " dBFS"
                                }
                                color: "#EAB308"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
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
            radius: 12
            color: "#0B1220"
            border.color: "#223049"
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
                color: "#E5E7EB"
                font.pixelSize: 14
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text { text: "Auto"; color: "#94A3B8"; Layout.preferredWidth: 60 }
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

                Text { text: "Min dB"; color: "#94A3B8"; Layout.preferredWidth: 60 }
                TextField {
                    id: minField
                    Layout.fillWidth: true
                    enabled: !autoSwitch.checked
                    placeholderText: "e.g. -95"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text { text: "Max dB"; color: "#94A3B8"; Layout.preferredWidth: 60 }
                TextField {
                    id: maxField
                    Layout.fillWidth: true
                    enabled: !autoSwitch.checked
                    placeholderText: "e.g. -10"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: "Apply"
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

                Button {
                    text: "Close"
                    onClicked: fftYPopup.close()
                }

                Item { Layout.fillWidth: true }
            }
        }
    }
}
