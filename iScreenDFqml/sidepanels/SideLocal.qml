// SideLocal.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3

Item {
    id: sidelocal
    anchors.fill: parent

    property var groups: []
    property var krakenmapval: null
    property string test: ""

    Connections {
        target: krakenmapval
        function onUpdateParameter(deviceName, serial) {
            nameDecimationField.text   = deviceName
            serialDecimationField.text = serial
        }
    }

    Component.onCompleted: {
        Qt.callLater(function() {
            if (krakenmapval) {
                krakenmapval.getdatabaseToSideSettingDrawer("SideLocal")
            } else {
                console.warn("[SideLocal] krakenmapval/getDeviceList not available")
            }
        })
    }

    // ===== Header =====
    Row {
        id: header
        spacing: 10
        height: 36
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 12

        Label {
            anchors.verticalCenter: parent.verticalCenter
            color: "#eeeeee"
            text: "Local Device Setting"
            font.pixelSize: 18
            font.bold: true
            Layout.alignment: Qt.AlignVCenter
        }
        Item { Layout.fillWidth: true }
    }

    // ===== Main content =====
    SplitView {
        id: split
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 12

        ScrollView {
            id: settingsPane
            SplitView.fillWidth: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            clip: true

            ColumnLayout {
                id: settingsColumn
                Layout.fillWidth: true
                spacing: 18

                /* ===== VFO Configuration ===== */
                Label {
                    text: "Parameter Configuration"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#ffffff"
                }

                GridLayout {
                    id: vfoConfigLayout
                    columns: 2
                    rowSpacing: 16
                    columnSpacing: 30
                    Layout.fillWidth: true

                    property var  vfoConfigJson: ({})
                    property bool vfoFormInitialized: false

                    // --- Device Name ---
                    Label { text: "Device Name:"; font.pixelSize: 16; color: "#ffffff" }
                    TextField {
                        id: nameDecimationField
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 32
                        font.pixelSize: 16
                        color: "#7AE2CF"
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        leftPadding: 10
                        rightPadding: 10
                        topPadding: 4
                        bottomPadding: 4
                        placeholderTextColor: "#666"
                        placeholderText: "Device Name"

                        background: Rectangle {
                            color: "#111A1E"
                            radius: 10
                            border.color: "#1B8F77"
                            border.width: 1
                        }

                        property string previousValue2: ""
                        Keys.onReturnPressed: commit()
                        Keys.onEnterPressed:  commit()

                        function commit() { nameDecimationField.focus = false }

                        onFocusChanged: if (focus) selectAll()

                        onEditingFinished: {
                            if (text !== previousValue2) {
                                previousValue2 = text
                                if (krakenmapval)
                                    krakenmapval.setParameterdevice(text, serialDecimationField.text)
                            }
                        }
                    }

                    // --- Serial Number ---
                    Label { text: "Serial Number:"; font.pixelSize: 16; color: "#ffffff" }
                    TextField {
                        id: serialDecimationField
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 32
                        font.pixelSize: 16
                        color: "#7AE2CF"
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        leftPadding: 10
                        rightPadding: 10
                        topPadding: 4
                        bottomPadding: 4
                        placeholderTextColor: "#666"
                        placeholderText: "Serial Number"

                        background: Rectangle {
                            color: "#111A1E"
                            radius: 10
                            border.color: "#1B8F77"
                            border.width: 1
                        }

                        property string previousValue: ""
                        Keys.onReturnPressed: commit()
                        Keys.onEnterPressed:  commit()

                        function commit() { serialDecimationField.focus = false }

                        onFocusChanged: if (focus) selectAll()

                        onEditingFinished: {
                            if (text !== previousValue) {
                                previousValue = text
                                if (krakenmapval)
                                    krakenmapval.setParameterdevice(nameDecimationField.text, text)
                            }
                        }
                    }

                    // --- IP Local For Remote Group ---
                    Label { text: "IP for Remote:"; font.pixelSize: 16; color: "#ffffff" }
                    TextField {
                        id: ipRemoteField
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 32
                        font.pixelSize: 16
                        color: "#7AE2CF"
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        leftPadding: 10
                        rightPadding: 10
                        topPadding: 4
                        bottomPadding: 4
                        placeholderTextColor: "#666"
                        placeholderText: "e.g. 10.10.0.20"
                        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferLowercase

                        background: Rectangle {
                            color: "#111A1E"
                            radius: 10
                            border.color: "#1B8F77"
                            border.width: 1
                        }

                        property string previousValue: ""

                        function commit() {
                            var v = text.trim()
                            if (v === previousValue) { focus = false; return }
                            previousValue = v
                            focus = false

                            if (krakenmapval && !krakenmapval.blockUiSync) {
                                krakenmapval.setIPLocalForRemoteGroup(v)
                            }
                        }

                        Keys.onReturnPressed: commit()
                        Keys.onEnterPressed:  commit()

                        onFocusChanged: if (focus) { previousValue = text; selectAll() }

                        onEditingFinished: {
                            if (text !== previousValue) commit()
                            else focus = false
                        }

                        Connections {
                            target: krakenmapval
                            function onUpdateIPLocalForRemoteGroupFromServer(ip) {
                                if (ipRemoteField.activeFocus) return
                                var s = (ip === undefined || ip === null) ? "" : String(ip).trim()
                                if (ipRemoteField.text !== s) {
                                    ipRemoteField.text = s
                                    ipRemoteField.previousValue = s
                                }
                            }
                        }
                    }

                    // --- DOA Line Length (meters) ---
                    Label { text: "DOA Line (Km):"; font.pixelSize: 16; color: "#ffffff" }

                    ComboBox {
                        id: doaLineMetersCombo
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 44
                        font.pixelSize: 16
                        textRole: "text"

                        // กัน loop ตอน set จาก server
                        property bool blockLocal: false

                        model: ListModel {
                            ListElement { text: "15 Km"; meters: 15000 }
                            ListElement { text: "20 Km"; meters: 20000 }
                            ListElement { text: "25 Km"; meters: 25000 }
                            ListElement { text: "30 Km"; meters: 30000 }
                            ListElement { text: "40 Km"; meters: 40000 }
                            ListElement { text: "50 Km"; meters: 50000 }
                            ListElement { text: "60 Km"; meters: 60000 }
                            ListElement { text: "80 Km"; meters: 80000 }
                            ListElement { text: "90 Km"; meters: 90000 }
                        }

                        background: Rectangle {
                            color: "#111A1E"
                            radius: 10
                            border.color: "#1B8F77"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: doaLineMetersCombo.displayText
                            color: "#7AE2CF"
                            font.pixelSize: 16
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 10
                            elide: Text.ElideRight
                        }

                        delegate: ItemDelegate {
                            width: doaLineMetersCombo.width
                            contentItem: Text {
                                text: model.text
                                color: "#7AE2CF"
                                font.pixelSize: 16
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                                elide: Text.ElideRight
                            }
                            highlighted: false
                        }

                        function indexByMeters(v) {
                            v = Number(v)
                            if (!isFinite(v)) return -1
                            for (var i = 0; i < model.count; i++) {
                                if (Number(model.get(i).meters) === v)
                                    return i
                            }
                            return -1
                        }

                        onActivated: (index) => {
                            if (blockLocal) return
                            if (!krakenmapval) return
                            if (krakenmapval.blockUiSync) return

                            const m = Number(model.get(index).meters)
                            if (!isFinite(m)) return
                            krakenmapval.sendMaxDoaLineMeters(m)
                        }

                        Component.onCompleted: {
                            if (currentIndex < 0 && model.count > 0) currentIndex = 0
                        }

                        Connections {
                            target: krakenmapval
                            function onUpdateDoaLineMeters(meters) {
                                const m = Number(meters)
                                const idx = doaLineMetersCombo.indexByMeters(m)

                                console.log("[QML] onUpdateDoaLineMeters:", m, "idx=", idx)

                                if (idx >= 0 && doaLineMetersCombo.currentIndex !== idx) {
                                    doaLineMetersCombo.blockLocal = true
                                    doaLineMetersCombo.currentIndex = idx
                                    doaLineMetersCombo.blockLocal = false
                                }
                            }
                        }
                    }
                    // ===================== Delay (s) -> sendDelayMs(ms) =====================
                    Label { text: "Delay (s):"; font.pixelSize: 16; color: "#ffffff" }

                    ComboBox {
                        id: maxDoaDelayCombo
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 44
                        font.pixelSize: 16
                        textRole: "text"

                        property bool blockLocal: false
                        property int  currentMs: 0
                        property bool gotServerValue: false
                        model: ListModel {
                            ListElement { text: "None";    ms: 0 }
                            ListElement { text: "0.5 s";  ms: 500 }
                            ListElement { text: "1 s";    ms: 1000 }
                            ListElement { text: "1.5 s";  ms: 1500 }
                            ListElement { text: "2 s";    ms: 2000 }
                            ListElement { text: "3 s";    ms: 3000 }
                            ListElement { text: "5 s";    ms: 5000 }
                            ListElement { text: "8 s";    ms: 8000 }
                            ListElement { text: "10 s";   ms: 10000 }
                            ListElement { text: "15 s";   ms: 15000 }
                            ListElement { text: "20 s";   ms: 20000 }
                            ListElement { text: "30 s";   ms: 30000 }
                            ListElement { text: "45 s";   ms: 45000 }
                            ListElement { text: "60 s";   ms: 60000 }
                        }

                        background: Rectangle {
                            color: "#111A1E"
                            radius: 10
                            border.color: "#1B8F77"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: {
                                // ใช้ currentIndex เพื่อให้โชว์ "None" ตาม text ใน model เสมอ
                                if (maxDoaDelayCombo.currentIndex >= 0 && maxDoaDelayCombo.currentIndex < maxDoaDelayCombo.model.count)
                                    return maxDoaDelayCombo.model.get(maxDoaDelayCombo.currentIndex).text
                                return ""
                            }
                            color: "#7AE2CF"
                            font.pixelSize: 16
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 10
                            elide: Text.ElideRight
                        }


                        delegate: ItemDelegate {
                            width: maxDoaDelayCombo.width
                            contentItem: Text {
                                text: model.text
                                color: "#7AE2CF"
                                font.pixelSize: 16
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                                elide: Text.ElideRight
                            }
                            highlighted: false
                        }

                        function clampMs(ms) {
                            ms = Number(ms)
                            if (!isFinite(ms) || isNaN(ms)) return 0
                            ms = Math.round(ms)
                            if (ms < 0) ms = 0
                            if (ms > 60000) ms = 60000
                            return ms
                        }

                        function indexByMs(ms) {
                            ms = clampMs(ms)
                            for (var i = 0; i < model.count; i++) {
                                if (Number(model.get(i).ms) === ms)
                                    return i
                            }
                            return -1
                        }

                        function applyFromMs(ms) {
                            ms = clampMs(ms)
                            currentMs = ms
                            var idx = indexByMs(ms)
                            if (idx >= 0 && currentIndex !== idx) {
                                blockLocal = true
                                currentIndex = idx
                                blockLocal = false
                            }
                        }

                        onActivated: (index) => {
                            if (blockLocal) return
                            if (!krakenmapval) return
                            if (krakenmapval.blockUiSync) return

                            var ms = clampMs(model.get(index).ms)

                            // ✅ ให้ UI เปลี่ยนแน่นอน
                            if (currentIndex !== index) {
                                blockLocal = true
                                currentIndex = index
                                blockLocal = false
                            }

                            if (ms === currentMs) return
                            currentMs = ms

                            // ✅ send: setDelayMs(ms)
                            if (typeof krakenmapval.setDelayMs === "function")
                                krakenmapval.setDelayMs(ms)
                            else
                                console.log("[QML] no setDelayMs(ms) in backend")
                        }

                        Component.onCompleted: {
                            if (model.count > 0 && currentIndex < 0) {
                                var defIdx = indexByMs(2000)
                                currentIndex = (defIdx >= 0) ? defIdx : 0
                            }
                        }

                        Connections {
                            target: krakenmapval
                            function onUpdateMaxDoaDelayMsFromServer(ms) {
                                maxDoaDelayCombo.applyFromMs(ms)
                            }
                        }
                    }


                    // ===================== DistanceM (m) -> sendDistance(meters) =====================
                    Label { text: "DistanceM (m):"; font.pixelSize: 16; color: "#ffffff" }

                    ComboBox {
                        id: doaDistanceMCombo
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 44
                        font.pixelSize: 16
                        textRole: "text"

                        property bool blockLocal: false

                        model: ListModel {
                            ListElement { text: "10 m";      meters: 10 }
                            ListElement { text: "30 m";      meters: 30 }
                            ListElement { text: "60 m";      meters: 60 }
                            ListElement { text: "100 m";     meters: 100 }
                            ListElement { text: "150 m";     meters: 150 }
                            ListElement { text: "200 m";     meters: 200 }
                            ListElement { text: "250 m";     meters: 250 }
                            ListElement { text: "500 m";     meters: 500 }
                            ListElement { text: "1,000 m";   meters: 1000 }
                        }

                        background: Rectangle {
                            color: "#111A1E"
                            radius: 10
                            border.color: "#1B8F77"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: doaDistanceMCombo.displayText
                            color: "#7AE2CF"
                            font.pixelSize: 16
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 10
                            elide: Text.ElideRight
                        }

                        delegate: ItemDelegate {
                            width: doaDistanceMCombo.width
                            contentItem: Text {
                                text: model.text
                                color: "#7AE2CF"
                                font.pixelSize: 16
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                                elide: Text.ElideRight
                            }
                            highlighted: false
                        }

                        function indexByMeters(v) {
                            v = Number(v)
                            if (!isFinite(v)) return -1
                            for (var i = 0; i < model.count; i++) {
                                if (Number(model.get(i).meters) === v)
                                    return i
                            }
                            return -1
                        }

                        onActivated: (index) => {
                            if (blockLocal) return
                            if (!krakenmapval) return
                            if (krakenmapval.blockUiSync) return

                            const m = Number(model.get(index).meters)
                            if (!isFinite(m)) return

                            // ✅ CHANGED: sendDistance(meters)
                            if (typeof krakenmapval.setDistance === "function")
                                krakenmapval.setDistance(m)
                            else
                                console.log("[QML] no sendDistance(meters) in backend")
                        }

                        Component.onCompleted: {
                            if (currentIndex < 0 && model.count > 0) currentIndex = 0
                        }

                        Connections {
                            target: krakenmapval
                            function onUpdateDoaLineDistanceMFromServer(meters) {
                                const m = Number(meters)
                                if (!isFinite(m)) return
                                const idx = doaDistanceMCombo.indexByMeters(m)
                                if (idx >= 0 && doaDistanceMCombo.currentIndex !== idx) {
                                    doaDistanceMCombo.blockLocal = true
                                    doaDistanceMCombo.currentIndex = idx
                                    doaDistanceMCombo.blockLocal = false
                                }
                            }
                        }
                    }

                    // --- DOA / FFT enable ---
                    RowLayout {
                        Layout.alignment: Qt.AlignVCenter

                        Label { text: "DOA:"; font.pixelSize: 16; color: "#ffffff" }

                        CheckBox {
                            id: doaCheck
                            checked: false
                            onToggled: {
                                if (!krakenmapval) return
                                if (krakenmapval.blockUiSync) return
                                krakenmapval.sendSetDoaEnable(checked)
                            }
                        }

                        Label { text: "FFT:"; font.pixelSize: 16; color: "#ffffff" }

                        CheckBox {
                            id: fftCheck
                            checked: false
                            onToggled: {
                                if (!krakenmapval) return
                                if (krakenmapval.blockUiSync) return
                                krakenmapval.sendSetSpectrumEnable(checked)
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // --- DOA Algorithm ---
                    Label { text: "DOA Algorithm:"; font.pixelSize: 16; color: "#ffffff" }
                    ComboBox {
                        id: doaAlgoCombo
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 44
                        font.pixelSize: 16
                        textRole: "text"

                        model: ListModel {
                            ListElement { text: "MUSIC (STD)"; value: "music_1d" }
                            ListElement { text: "MUSIC (UCA-RB)"; value: "uca_rb_music" }
                            ListElement { text: "ESPRIT(UCA Peak only)"; value: "uca_esprit" }
                        }

                        background: Rectangle { color: "#111A1E"; radius: 10; border.color: "#1B8F77"; border.width: 1 }

                        contentItem: Text {
                            text: doaAlgoCombo.displayText
                            color: "#7AE2CF"
                            font.pixelSize: 16
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 10
                        }

                        delegate: ItemDelegate {
                            width: doaAlgoCombo.width
                            contentItem: Text {
                                text: model.text
                                color: "#7AE2CF"
                                font.pixelSize: 16
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                            }
                            highlighted: false
                        }

                        function indexByValue(v) {
                            v = (v === undefined || v === null) ? "" : String(v)
                            for (var i = 0; i < model.count; i++) {
                                if (String(model.get(i).value) === v)
                                    return i
                            }
                            return -1
                        }

                        onActivated: (index) => {
                            if (!krakenmapval) return
                            if (krakenmapval.blockUiSync) return
                            const value = model.get(index).value
                            krakenmapval.sendDoaAlgorithm(value)
                        }

                        Component.onCompleted: {
                            if (currentIndex < 0 && model.count > 0) currentIndex = 0
                        }
                    }

                    // ====== SQL Threshold + Tx Interval + Radius + RF AGC + Target (ของเดิมคุณ) ======
                    // *** โค้ดช่วงนี้ 그대로 (ผมไม่ตัด) ***

                    // --- SQL (Squelch / Gate threshold) dB ---
                    Item {
                        id: sqlItem
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44

                        property real sqlGateDb: -130.0
                        property bool hasInit: false
                        property bool hasPending: false
                        property real pendingGateDb: -130.0

                        function clamp(v, lo, hi) {
                            v = Number(v)
                            if (isNaN(v)) v = lo
                            if (v < lo) return lo
                            if (v > hi) return hi
                            return v
                        }
                        function fmt1(v) { return (Math.round(Number(v) * 10) / 10).toString() }

                        function applyValue(v) { sqlGateDb = v }

                        Component.onCompleted: {
                            hasInit = true
                            if (hasPending) {
                                applyValue(pendingGateDb)
                                hasPending = false
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: "#111A1E"
                            border.color: "#1B8F77"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 10

                                Text {
                                    text: "Threshold"
                                    color: "#ffffff"
                                    font.pixelSize: 16
                                    Layout.preferredWidth: 70
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                Slider {
                                    id: sqlSlider
                                    Layout.fillWidth: true
                                    from: -140.0
                                    to: 0.0
                                    stepSize: 1.0
                                    enabled: true
                                    Layout.alignment: Qt.AlignVCenter
                                    value: sqlItem.sqlGateDb

                                    onValueChanged: {
                                        if (pressed) {
                                            sqlItem.sqlGateDb = sqlItem.clamp(value, -140.0, 0.0)
                                        }
                                    }

                                    onPressedChanged: {
                                        if (!pressed) {
                                            var v = sqlItem.clamp(value, -140.0, 0.0)
                                            sqlItem.applyValue(v)
                                            if (krakenmapval) krakenmapval.sendGateThDb(v)
                                        }
                                    }
                                }

                                TextField {
                                    id: sqlDbField
                                    Layout.preferredWidth: 90
                                    Layout.preferredHeight: 32
                                    font.pixelSize: 16
                                    color: "#7AE2CF"
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    validator: DoubleValidator { bottom: -140.0; top: 0.0; decimals: 1 }
                                    enabled: true
                                    verticalAlignment: Text.AlignVCenter
                                    horizontalAlignment: Text.AlignRight
                                    leftPadding: 10
                                    rightPadding: 10
                                    topPadding: 4
                                    bottomPadding: 4

                                    background: Rectangle {
                                        color: "#111A1E"
                                        radius: 10
                                        border.color: sqlDbField.activeFocus ? "#7AE2CF" : "#1B8F77"
                                        border.width: 1
                                    }

                                    property string previousValue: ""

                                    Binding {
                                        target: sqlDbField
                                        property: "text"
                                        value: sqlItem.fmt1(sqlItem.sqlGateDb)
                                        when: !sqlDbField.activeFocus
                                    }

                                    function commit() {
                                        var t = text.trim()
                                        if (t.length === 0) { focus = false; return }

                                        var v = Number(t)
                                        if (isNaN(v)) { focus = false; return }

                                        v = sqlItem.clamp(v, -140.0, 0.0)
                                        sqlItem.applyValue(v)
                                        if (krakenmapval) krakenmapval.sendGateThDb(v)
                                        focus = false
                                    }

                                    Keys.onReturnPressed: commit()
                                    Keys.onEnterPressed:  commit()

                                    onFocusChanged: {
                                        if (focus) {
                                            previousValue = text
                                            selectAll()
                                        }
                                    }

                                    onEditingFinished: {
                                        if (text !== previousValue) commit()
                                        else focus = false
                                    }
                                }

                                Text {
                                    text: "dB"
                                    color: "#7AE2CF"
                                    font.pixelSize: 16
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }
                        }

                        Connections {
                            target: krakenmapval

                            function onUpdateGateThDbFromServer(v) {
                                var vv = sqlItem.clamp(v, -140.0, 0.0)
                                if (!sqlItem.hasInit) {
                                    sqlItem.pendingGateDb = vv
                                    sqlItem.hasPending = true
                                    return
                                }
                                sqlItem.applyValue(vv)
                            }

                            function onUpdateDoaAlgorithmFromServer(algo) {
                                console.log("sqlItem got onUpdateDoaAlgorithmFromServer:", algo)
                                var idx = doaAlgoCombo.indexByValue(algo)
                                if (idx >= 0) doaAlgoCombo.currentIndex = idx
                            }
                        }
                    }

                    // --- Tx Rate (Hz) : slider + manual input ---
                    Item {
                        id: txItem
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44

                        property real txHzVal: 10.8
                        property bool hasInit: false
                        property bool hasPending: false
                        property real pendingTxHz: 10.8

                        function clamp(v, lo, hi) {
                            v = Number(v)
                            if (isNaN(v)) v = lo
                            if (v < lo) return lo
                            if (v > hi) return hi
                            return v
                        }
                        function fmt1(v) { return (Math.round(Number(v) * 10) / 10).toString() }

                        function applyUi(v) {
                            txHzVal = v
                            hzSlider.value = v
                            if (!hzField.activeFocus) {
                                const s = fmt1(v)
                                if (hzField.text !== s) {
                                    Qt.callLater(function() { hzField.text = s })
                                }
                            }
                        }

                        Component.onCompleted: {
                            hasInit = true
                            var initV = txHzVal
                            if (krakenmapval && typeof krakenmapval.txHz !== "undefined") {
                                initV = clamp(krakenmapval.txHz, 0.2, 60.0)
                            }
                            hzField.text = fmt1(initV)

                            if (hasPending) {
                                applyUi(pendingTxHz)
                                hasPending = false
                            } else {
                                applyUi(initV)
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: "#111A1E"
                            border.color: "#1B8F77"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 10

                                Text {
                                    text: "Interval"
                                    color: "#ffffff"
                                    font.pixelSize: 16
                                    Layout.preferredWidth: 50
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                Slider {
                                    id: hzSlider
                                    Layout.fillWidth: true
                                    from: 0.2
                                    to: 60.0
                                    stepSize: 0.1
                                    value: txItem.txHzVal
                                    enabled: true
                                    Layout.alignment: Qt.AlignVCenter

                                    onValueChanged: {
                                        if (!hzField.activeFocus) {
                                            const s = txItem.fmt1(value)
                                            if (hzField.text !== s)
                                                hzField.text = s
                                        }
                                    }

                                    onPressedChanged: {
                                        if (!pressed) {
                                            var v = txItem.clamp(value, 0.2, 60.0)
                                            txItem.applyUi(v)
                                            if (krakenmapval && !krakenmapval.blockUiSync) {
                                                krakenmapval.sendTxHz(v)
                                            }
                                        }
                                    }
                                }

                                TextField {
                                    id: hzField
                                    Layout.preferredWidth: 90
                                    Layout.preferredHeight: 32
                                    font.pixelSize: 16
                                    color: "#7AE2CF"
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    validator: DoubleValidator { bottom: 0.2; top: 60.0; decimals: 1 }
                                    enabled: true
                                    verticalAlignment: Text.AlignVCenter
                                    horizontalAlignment: Text.AlignRight
                                    leftPadding: 10
                                    rightPadding: 10
                                    topPadding: 4
                                    bottomPadding: 4

                                    background: Rectangle {
                                        color: "#111A1E"
                                        radius: 10
                                        border.color: hzField.activeFocus ? "#7AE2CF" : "#1B8F77"
                                        border.width: 1
                                    }

                                    property string previousValue: ""

                                    function commit() {
                                        if (text.trim().length === 0) {
                                            text = txItem.fmt1(txItem.txHzVal)
                                            focus = false
                                            return
                                        }

                                        var v = Number(text)
                                        if (isNaN(v)) {
                                            text = txItem.fmt1(txItem.txHzVal)
                                            focus = false
                                            return
                                        }

                                        v = txItem.clamp(v, 0.2, 60.0)
                                        txItem.applyUi(v)
                                        text = txItem.fmt1(v)

                                        if (krakenmapval && !krakenmapval.blockUiSync) {
                                            krakenmapval.sendTxHz(v)
                                        }
                                        focus = false
                                    }

                                    Keys.onReturnPressed: commit()
                                    Keys.onEnterPressed:  commit()

                                    onFocusChanged: {
                                        if (focus) {
                                            previousValue = text
                                            selectAll()
                                        } else {
                                            text = txItem.fmt1(txItem.txHzVal)
                                        }
                                    }

                                    onEditingFinished: {
                                        if (text !== previousValue) commit()
                                        else focus = false
                                    }
                                }

                                Text {
                                    text: "Hz"
                                    color: "#7AE2CF"
                                    font.pixelSize: 16
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }
                        }

                        Connections {
                            target: krakenmapval
                            function onUpdateTxHzFromServer(v) {
                                var vv = txItem.clamp(v, 0.2, 60.0)

                                if (!txItem.hasInit) {
                                    txItem.pendingTxHz = vv
                                    txItem.hasPending = true
                                    return
                                }

                                if (hzField.activeFocus) {
                                    txItem.txHzVal = vv
                                    hzSlider.value = vv
                                    return
                                }

                                txItem.applyUi(vv)
                            }
                        }
                    }

                    Label {
                        text: "Radius (m):"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }

                    TextField {
                        id: radiusField
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 32
                        font.pixelSize: 16
                        color: "#7AE2CF"
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        leftPadding: 10
                        rightPadding: 10
                        topPadding: 4
                        bottomPadding: 4
                        placeholderText: "e.g. 0.80"
                        placeholderTextColor: "#666"
                        inputMethodHints: Qt.ImhFormattedNumbersOnly

                        background: Rectangle {
                            color: "#111A1E"
                            radius: 10
                            border.color: radiusField.activeFocus ? "#7AE2CF" : "#1B8F77"
                            border.width: 1
                        }

                        property string lastGoodText: "0.81"

                        function clampRadius(v) {
                            v = Number(v)
                            if (isNaN(v)) return NaN
                            if (v <= 0) return NaN
                            return v
                        }

                        function fmt2(v) { return Number(v).toFixed(2) }

                        function commit() {
                            var r = clampRadius(text)

                            if (isNaN(r)) {
                                text = lastGoodText
                                focus = false
                                return
                            }

                            var s = fmt2(r)
                            text = s
                            lastGoodText = s

                            if (krakenmapval && !krakenmapval.blockUiSync) {
                                krakenmapval.sendUcaRadiusM(r)
                            }

                            focus = false
                        }

                        Keys.onReturnPressed: commit()
                        Keys.onEnterPressed:  commit()

                        onFocusChanged: {
                            if (focus) {
                                if (text.trim().length > 0)
                                    lastGoodText = text
                                selectAll()
                            } else {
                                if (lastGoodText.trim().length > 0)
                                    text = lastGoodText
                            }
                        }

                        onEditingFinished: {
                            if (activeFocus)
                                commit()
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignVCenter

                        Label { text: "RF AGC:"; font.pixelSize: 16; color: "#ffffff" }

                        CheckBox {
                            id: agcCheck
                            checked: true
                            onToggled: {
                                if (!krakenmapval) return
                                if (krakenmapval.blockUiSync) return
                                krakenmapval.sendRfAgcEnable(-1, checked)
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Item {
                        id: rfAgcTargetAllItem
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44

                        property var  targetDbArr: [-60, -60, -60, -60, -60]
                        property real targetAllDb: -60
                        property bool blockLocal: false
                        property real lastSentDb: 9999

                        function clampDb(v) {
                            v = Number(v)
                            if (isNaN(v)) v = -60
                            if (v < -90) v = -90
                            if (v > -30) v = -30
                            return v
                        }

                        function recomputeAll() {
                            var sum = 0
                            for (var i = 0; i < 5; i++) sum += clampDb(targetDbArr[i])
                            targetAllDb = clampDb(sum / 5.0)
                        }

                        function setOneFromServer(ch, v) {
                            v = clampDb(v)
                            var arr = targetDbArr.slice()
                            arr[ch] = v
                            targetDbArr = arr
                            recomputeAll()
                        }

                        function sendAllOnce(v, reason) {
                            v = clampDb(v)
                            if (blockLocal) return
                            if (!krakenmapval) return
                            if (krakenmapval.blockUiSync) return

                            var vv = Math.round(v * 10) / 10
                            var ll = Math.round(lastSentDb * 10) / 10
                            if (vv === ll) return

                            lastSentDb = vv
                            krakenmapval.sendRfAgcTargetAllDb(vv)
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: "#111A1E"
                            border.color: "#1B8F77"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 10

                                Text {
                                    text: "Target"
                                    color: "#ffffff"
                                    font.pixelSize: 16
                                    Layout.preferredWidth: 90
                                    Layout.alignment: Qt.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                Slider {
                                    id: targetAllSlider
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 44
                                    from: -90
                                    to: -30
                                    stepSize: 0.5
                                    enabled: true
                                    value: rfAgcTargetAllItem.targetAllDb

                                    onValueChanged: {
                                        if (rfAgcTargetAllItem.blockLocal) return
                                        if (!targetAllField.activeFocus)
                                            targetAllField.text = rfAgcTargetAllItem.clampDb(value).toFixed(1)
                                    }

                                    onPressedChanged: {
                                        if (!pressed) {
                                            var v = rfAgcTargetAllItem.clampDb(value)
                                            rfAgcTargetAllItem.blockLocal = true
                                            targetAllField.text = v.toFixed(1)
                                            rfAgcTargetAllItem.blockLocal = false
                                            rfAgcTargetAllItem.sendAllOnce(v, "slider_release")
                                        }
                                    }
                                }

                                TextField {
                                    id: targetAllField
                                    Layout.preferredWidth: 90
                                    Layout.preferredHeight: 32
                                    font.pixelSize: 16
                                    color: "#7AE2CF"
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    validator: DoubleValidator { bottom: -90; top: -30; decimals: 1 }
                                    enabled: true
                                    verticalAlignment: Text.AlignVCenter
                                    horizontalAlignment: Text.AlignRight
                                    leftPadding: 10
                                    rightPadding: 10
                                    topPadding: 4
                                    bottomPadding: 4
                                    text: rfAgcTargetAllItem.targetAllDb.toFixed(1)

                                    background: Rectangle {
                                        color: "#111A1E"
                                        radius: 10
                                        border.color: targetAllField.activeFocus ? "#7AE2CF" : "#1B8F77"
                                        border.width: 1
                                    }

                                    property string previousValue: ""

                                    function commit() {
                                        var v = rfAgcTargetAllItem.clampDb(text)
                                        text = v.toFixed(1)

                                        rfAgcTargetAllItem.blockLocal = true
                                        targetAllSlider.value = v
                                        rfAgcTargetAllItem.blockLocal = false

                                        rfAgcTargetAllItem.sendAllOnce(v, "field_commit")
                                        focus = false
                                    }

                                    Keys.onReturnPressed: commit()
                                    Keys.onEnterPressed:  commit()

                                    onFocusChanged: if (focus) { previousValue = text; selectAll() }
                                    onEditingFinished: {
                                        if (text !== previousValue) commit()
                                        else focus = false
                                    }
                                }

                                Text {
                                    text: "dB"
                                    color: "#7AE2CF"
                                    font.pixelSize: 16
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }
                        }

                        Connections {
                            target: krakenmapval
                            function onUpdateRfAgcTargetFromServer(ch, targetDb) {
                                if (ch < 0 || ch >= 5) return

                                rfAgcTargetAllItem.blockLocal = true
                                rfAgcTargetAllItem.setOneFromServer(ch, targetDb)

                                targetAllSlider.value = rfAgcTargetAllItem.targetAllDb
                                if (!targetAllField.activeFocus)
                                    targetAllField.text = rfAgcTargetAllItem.targetAllDb.toFixed(1)

                                rfAgcTargetAllItem.blockLocal = false
                            }
                        }
                    }

                    // --- Restore values (ครั้งแรก) ---
                    Connections {
                        target: krakenmapval
                        function onRfsocDoaFftUpdated(doaEnable, fftEnable) {
                            doaCheck.checked = !!doaEnable
                            fftCheck.checked = !!fftEnable
                        }
                        function onUpdateDoaAlgorithmFromServer(algo) {
                            const idx = doaAlgoCombo.indexByValue(algo)
                            console.log("QML got DoaAlgorithm:", algo, "idx=", idx)
                            if (idx >= 0 && doaAlgoCombo.currentIndex !== idx) {
                                doaAlgoCombo.currentIndex = idx
                            }
                        }
                        function onUpdateUcaRadiusFromServer(radiusM) {
                            console.log("QML got UCA radius:", radiusM, "focus=", radiusField.activeFocus)
                            if (radiusField.activeFocus) return
                            var r = radiusField.clampRadius(radiusM)
                            if (isNaN(r)) return
                            var s = radiusField.fmt2(r)
                            radiusField.text = s
                            radiusField.lastGoodText = s
                        }
                        function onUpdateRfAgcEnableFromServer(ch, enable) {
                            if (ch < 0) {
                                if (agcCheck.checked !== !!enable)
                                    agcCheck.checked = !!enable
                                return
                            }
                        }
                        // ✅ NEW: ถ้า server ยิงค่า DOA line meters -> ให้ combo DistanceM ตามด้วย (ถ้าคุณต้องการ sync)
                        function onUpdateDoaLineMeters(meters) {
                            const m = Number(meters)
                            if (!isFinite(m)) return
                            const idx = doaDistanceMCombo.indexByMeters(m)
                            if (idx >= 0 && doaDistanceMCombo.currentIndex !== idx) {
                                doaDistanceMCombo.blockLocal = true
                                doaDistanceMCombo.currentIndex = idx
                                doaDistanceMCombo.blockLocal = false
                            }
                        }
                    }
                }

                /* ===== Compass ===== */
                Label {
                    text: "Compass"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#ffffff"
                }

                GridLayout {
                    columns: 2
                    rowSpacing: 20
                    columnSpacing: 30
                    Layout.fillWidth: true

                    Label { text: "Degree:"; font.pixelSize: 16; color: "#ffffff" }
                    TextField {
                        id: degreeInput
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 32
                        font.pixelSize: 16
                        color: "#1B8F77"
                        placeholderTextColor: "#888"
                        readOnly: true
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                        leftPadding: 10
                        rightPadding: 10
                        topPadding: 4
                        bottomPadding: 4

                        background: Rectangle {
                            color: "#111A1E"
                            radius: 10
                            border.color: "#1B8F77"
                            border.width: 1
                        }

                        Connections {
                            target: krakenmapval
                            function onUpdateDegreelocal(heading) {
                                degreeInput.text = heading.toFixed(1)
                            }
                        }
                    }

                    Label { text: "Status:"; font.pixelSize: 16; color: "#ffffff" }
                    TextArea {
                        id: degreeStatus
                        Layout.preferredWidth: 320
                        Layout.preferredHeight: implicitHeight
                        font.pixelSize: 16
                        color: "#1B8F77"
                        placeholderTextColor: "#888"
                        readOnly: true
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText

                        background: Rectangle {
                            color: "#111A1E"
                            radius: 10
                            border.color: "#1B8F77"
                            border.width: 1
                        }

                        Connections {
                            target: krakenmapval
                            function onUpdateStatusCompass(instruction) {
                                degreeStatus.text = instruction
                            }
                        }
                    }

                    Button {
                        id: calibrationButton
                        text: "Calibration"
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        height: 36
                        font.pixelSize: 16

                        background: Rectangle {
                            radius: 6
                            color: calibrationButton.pressed ? Qt.darker("#169976", 1.4) : "#169976"
                            Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.InOutQuad } }
                        }

                        contentItem: Text {
                            text: calibrationButton.text
                            anchors.centerIn: parent
                            color: "#212121"
                            font.pixelSize: calibrationButton.font.pixelSize
                            font.bold: true
                        }

                        onClicked: {
                            if (krakenmapval)
                                krakenmapval.Calibration("Calibration")
                            degreeStatus.text = ""
                        }
                    }
                }
            }
        }
    }
}
