import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    radius: 12
    color: "#071025"
    border.color: "#20304A"
    border.width: 1

    // ✅ ใช้กับ Layout ในหน้า main
    Layout.fillWidth: true
    Layout.preferredHeight: 240

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

    // ================= RF ATT AGC (per-channel) =================
    property bool rfAgcAvailable: false
    property bool rfAgcEnabledGlobal: true
    property var rfAgcChEnabled: [true,true,true,true,true]   // CH0..CH4
    property var rfAgcTargetDb:  [-55,-55,-55,-55,-55]
    property var rfAgcAttDb:     [0,0,0,0,0]
    property var rfAgcErrDb:     [0,0,0,0,0]
    property var rfAgcPeakDb:    [-200,-200,-200,-200,-200]
    property real scannerAttDb: 0.0   // ATT #7 manual (scanner)

    function _clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

    function sendRfAgcEnable(ch, enable) {
        var c = dc()
        if (!c) return

        var en = !!enable

        // ✅ when AGC disabled -> release ATT1..5 back to 0 dB (CH0..CH4)
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

        // ✅ target 1..5 adjust together (CH0..CH4)
        var tg = (root.rfAgcTargetDb && root.rfAgcTargetDb.slice) ? root.rfAgcTargetDb.slice(0) : [-75,-75,-75,-75,-75]
        for (var i = 0; i < 5; ++i) tg[i] = v
        root.rfAgcTargetDb = tg

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

        root.scannerAttDb = adb

        if (c.setScannerAttDb !== undefined && typeof c.setScannerAttDb === "function") {
            try { c.setScannerAttDb(adb, true) }
            catch(e) { try { c.setScannerAttDb(adb) } catch(e2) {} }
            return
        }

        if (c.sendJson !== undefined && typeof c.sendJson === "function") {
            c.sendJson({ menuID: "setScannerAttDb", att_db: adb, needAck: true })
        }
    }

    function syncRfAgcFromClient() {
        var c = dc()
        if (!c) return

        if (c.rfAgcAvailable !== undefined) rfAgcAvailable = !!c.rfAgcAvailable
        if (c.rfAgcEnabled   !== undefined) rfAgcEnabledGlobal = !!c.rfAgcEnabled

        var chs = null
        if (c.rfAgcChannels !== undefined) chs = c.rfAgcChannels
        if (!chs || chs.length === undefined) return

        var en = rfAgcChEnabled.slice(0)
        var tg = rfAgcTargetDb.slice(0)
        var at = rfAgcAttDb.slice(0)
        var pk = rfAgcPeakDb.slice(0)

        // ✅ if global disabled: force att=0 and disable channels
        if (!rfAgcEnabledGlobal) {
            for (var j = 0; j < 5; j++) {
                en[j] = false
                at[j] = 0.0
            }
        }

        for (var k = 0; k < chs.length; k++) {
            var o = chs[k]
            var ch = Number(o.ch)
            if (isNaN(ch) || ch < 0 || ch > 4) continue

            if (o.enabled !== undefined) en[ch] = !!o.enabled
            if (o.target_db !== undefined) tg[ch] = Number(o.target_db)
            if (o.att_db !== undefined) at[ch] = rfAgcEnabledGlobal ? Number(o.att_db) : 0.0

            if (o.band_db !== undefined) pk[ch] = Number(o.band_db)
            else if (o.band_peak_db !== undefined) pk[ch] = Number(o.band_peak_db)
        }

        if (tg.length > 0 && tg[0] !== undefined && !isNaN(Number(tg[0])))
            rfAgcTargetAllDb = Number(tg[0])

        rfAgcChEnabled = en
        rfAgcTargetDb  = tg
        rfAgcAttDb     = at
        rfAgcPeakDb    = pk
    }

    // ========= helper: call method if exists, otherwise set property =========
    function setIfExists(methodName, propName, value, needAck) {
        var c = dc()
        if (!c) return
        if (c[methodName] !== undefined && typeof c[methodName] === "function") {
            try { c[methodName](value, needAck) }
            catch(e) { try { c[methodName](value) } catch(e2) {} }
            return
        }
        if (propName && (c[propName] !== undefined)) c[propName] = value
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
                text: root.dc() ? root.dc().host : "172.0.0.1"
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

            Text { text: "RF (Hz)"; color: "#cccccc" }

            TextField {
                id: freqField
                Layout.preferredWidth: 200
                inputMethodHints: Qt.ImhDigitsOnly
                text: {
                    var c = root.dc()
                    return c ? c.fcHz.toFixed(0) : "130000000"
                }
                placeholderText: "e.g. 130000000"
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
                    var f = Number(freqField.text)
                    if (isNaN(f) || f <= 0) return
                    c.setFrequencyHz(f, updateEnSpin.value, true)
                }
            }

            // Button {
            //     text: "Set FC only"
            //     enabled: root.dc() ? root.dc().connected : false
            //     onClicked: {
            //         var c = root.dc()
            //         if (!c) return
            //         var f = Number(freqField.text)
            //         if (isNaN(f) || f <= 0) return
            //         c.setFcHz(f, true)
            //     }
            // }

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
                    return c ? ("Current: " + c.fcHz.toFixed(0) + " Hz") : "Current: --"
                }
                color: "#6fbf73"
                font.bold: true
            }
        }

        // ===================== Row 3: Tx + Tone =====================
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

                    Text { text: "Interval"; color: "#E5E7EB"; font.pixelSize: 13; Layout.preferredWidth: 50 }

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
                        elide: Text.ElideRight
                    }
                }
            }

            DoaTonePanel {
                Layout.preferredHeight: root.rowH
                Layout.fillWidth: true
            }
        }

        // ===================== Row 4: RF AGC (Responsive, no overflow) =====================
        RowLayout {
            id: rfAgcRow
            Layout.preferredHeight: 60
            Layout.fillWidth: true
            spacing: 2

            // ===== responsive sizing =====
            property real fixedW: 70 + 80 + 60
            property real gapsW: spacing * 12
            property real avail: Math.max(0, width - fixedW - gapsW)

            property real scannerMin: 240
            property real scannerMax: 380
            property real targetMin:  240
            property real targetMax:  380
            property real chMin:      140
            property real chMax:      210

            property real scannerW: Math.min(scannerMax, Math.max(scannerMin, avail * 0.26))
            property real targetW:  Math.min(targetMax,  Math.max(targetMin,  avail * 0.26))
            property real chArea:   Math.max(0, avail - scannerW - targetW)
            property real chW:      Math.min(chMax, Math.max(chMin, chArea / 5.0))

            Text {
                text: "RF AGC"
                color: "#E5E7EB"
                font.pixelSize: 13
                Layout.preferredWidth: 70
                elide: Text.ElideRight
            }

            Text {
                text: root.rfAgcAvailable ? "available" : "n/a"
                color: root.rfAgcAvailable ? "#22c55e" : "#f87171"
                font.pixelSize: 12
                Layout.preferredWidth: 80
                elide: Text.ElideRight
            }

            Switch {
                id: rfAgcGlobalSwitch
                enabled: true   // ✅ เปิดตลอดตามที่สั่ง (เดิมคุณผูก connected)
                checked: root.rfAgcEnabledGlobal
                onToggled: root.sendRfAgcEnable(-1, checked)
            }

            // ================= Scanner ATT #7 =================
            Rectangle {
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: rfAgcRow.scannerW
                Layout.minimumWidth: rfAgcRow.scannerMin
                radius: 8
                color: "#0B1220"
                border.color: "#223049"
                border.width: 1
                clip: true

                readonly property int pad: 8
                readonly property int gap: 8
                readonly property int labelW: 110
                readonly property int fieldW: 66
                readonly property real sliderW: Math.max(60, width - (pad*2) - labelW - fieldW - (gap*2))

                Row {
                    anchors.fill: parent
                    anchors.margins: pad
                    spacing: gap

                    Text {
                        width: labelW
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: "Scanner ATT #7"
                        color: "#E5E7EB"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Slider {
                        id: scannerAttSlider
                        width: sliderW
                        height: parent.height
                        from: 0
                        to: 31.75
                        stepSize: 0.25
                        value: root.scannerAttDb
                        enabled: true   // ✅ เปิดตลอด

                        onMoved: scannerAttField.text = Number(value).toFixed(2)
                        onPressedChanged: { if (!pressed) root.sendScannerAttDb(value) }
                    }

                    TextField {
                        id: scannerAttField
                        width: fieldW
                        height: 30                      // ✅ fix สูงให้ชัวร์
                        anchors.verticalCenter: parent.verticalCenter

                        text: Number(root.scannerAttDb).toFixed(2)
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        color: "#E5E7EB"

                        // ✅ จัดตัวหนังสือให้อยู่กลางกรอบ
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignRight

                        // ✅ คุม padding เอง (ไม่ให้ style มาดัน)
                        leftPadding: 8
                        rightPadding: 8
                        topPadding: 0
                        bottomPadding: 0

                        // ✅ กันกรอบ/เนื้อหาล้น
                        clip: true

                        background: Rectangle {
                            anchors.fill: parent         // ✅ กรอบครอบ TextField จริง
                            radius: 6
                            color: "#111A24"
                            border.color: "#223049"
                            border.width: 1
                        }

                        onEditingFinished: root.sendScannerAttDb(text)
                    }
                }
            }

            // ================= Target (DF CH0..CH4) =================
            Rectangle {
                Layout.preferredHeight: root.rowH
                Layout.preferredWidth: rfAgcRow.targetW
                Layout.minimumWidth: rfAgcRow.targetMin
                radius: 8
                color: "#071025"
                border.color: "#20304A"
                border.width: 1
                clip: true

                readonly property int pad: 6
                readonly property int gap: 8
                readonly property int labelW: 130
                readonly property int valueW: 64
                readonly property real sliderW: Math.max(60, width - (pad*2) - labelW - valueW - (gap*2))

                Row {
                    anchors.fill: parent
                    anchors.margins: pad
                    spacing: gap

                    Text {
                        width: labelW
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: "Target (DF CH0..CH4)"
                        color: "#CBD5E1"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Slider {
                        id: rfTargetAllSlider
                        width: sliderW
                        height: parent.height
                        from: -90
                        to: -30
                        stepSize: 0.5
                        enabled: true   // ✅ เปิดตลอด
                        value: Number(root.rfAgcTargetAllDb)

                        onPressedChanged: {
                            if (!pressed) {
                                root.rfAgcTargetAllDb = value
                                root.sendRfAgcTarget(0, value)
                            }
                        }
                    }

                    Text {
                        width: valueW
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignRight
                        text: Number(root.rfAgcTargetAllDb).toFixed(1) + " dB"
                        color: "#93c5fd"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }

            // ================= CH0..CH4 =================
            Repeater {
                model: 5
                delegate: Rectangle {
                    Layout.preferredHeight: root.rowH
                    Layout.preferredWidth: rfAgcRow.chW
                    Layout.minimumWidth: rfAgcRow.chMin
                    radius: 8
                    color: "#071025"
                    border.color: "#20304A"
                    border.width: 1
                    clip: true

                    readonly property int pad: 8
                    readonly property int gap: 4
                    readonly property int labelW: 30
                    readonly property real textW: Math.max(40, width - (pad*2) - labelW - gap)

                    Row {
                        anchors.fill: parent
                        anchors.margins: pad
                        spacing: gap

                        Text {
                            width: labelW
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            text: "CH" + index
                            color: "#CBD5E1"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        Text {
                            width: textW
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignRight
                            text: {
                                var pk = Number(root.rfAgcPeakDb[index]).toFixed(1)
                                var at = Number(root.rfAgcAttDb[index]).toFixed(1)
                                var tg = Number(root.rfAgcTargetAllDb).toFixed(1)
                                return pk + " dB | A" + at + " | T" + tg
                            }
                            color: "#93c5fd"
                            font.pixelSize: 11
                            elide: Text.ElideRight
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

                Button { text: "Close"; onClicked: fftYPopup.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }
}
