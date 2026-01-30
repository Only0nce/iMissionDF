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

    // ===== Main content: split left (groups) / right (settings) =====
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
                // width: settingsPane.width
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


                        function commit() {
                            nameDecimationField.focus = false
                        }

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


                        function commit() {
                            serialDecimationField.focus = false
                        }

                        onFocusChanged: if (focus) selectAll()
                        onEditingFinished: {
                            if (text !== previousValue) {
                                previousValue = text
                                if (krakenmapval)
                                    krakenmapval.setParameterdevice(nameDecimationField.text, text)
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

                        // ===== helper: find index by meters =====
                        function indexByMeters(v) {
                            v = Number(v)
                            if (!isFinite(v)) return -1
                            for (var i = 0; i < model.count; i++) {
                                if (Number(model.get(i).meters) === v)
                                    return i
                            }
                            return -1
                        }

                        // ===== send -> C++ when user changes =====
                        onActivated: (index) => {
                            if (blockLocal) return
                            if (!krakenmapval) return
                            if (krakenmapval.blockUiSync) return

                            const m = Number(model.get(index).meters)
                            if (!isFinite(m)) return

                            krakenmapval.sendMaxDoaLineMeters(m)
                        }

                        // ===== initial value (optional) =====
                        Component.onCompleted: {
                            if (currentIndex < 0 && model.count > 0) currentIndex = 0
                        }

                        // ===== receive from C++ =====
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

                    RowLayout {
                        Layout.alignment: Qt.AlignVCenter

                        Label { text: "DOA:"; font.pixelSize: 16; color: "#ffffff" }

                        CheckBox {
                            id: doaCheck
                            checked: false

                            onToggled: {
                                if (!krakenmapval) return
                                if (krakenmapval.blockUiSync) return   // กัน loop ตอน server อัปเดตมา
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

                    Item { Layout.fillWidth: true}
                    // --- Default Squelch ---
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

                        // ===== helper: find index by value =====
                        function indexByValue(v) {
                            v = (v === undefined || v === null) ? "" : String(v)
                            for (var i = 0; i < model.count; i++) {
                                if (String(model.get(i).value) === v)
                                    return i
                            }
                            return -1
                        }

                        // ===== send -> C++ when user changes =====
                        onActivated: (index) => {
                            if (!krakenmapval) return
                            if (krakenmapval.blockUiSync) return

                            const value = model.get(index).value
                            krakenmapval.sendDoaAlgorithm(value)
                        }

                        // ===== initial value (optional) =====
                        Component.onCompleted: {
                            // ถ้ามีค่า default ใน DB แล้ว C++ จะ emit มาเอง
                            // ตรงนี้ไม่จำเป็น แต่ใส่ไว้กันว่าง:
                            if (currentIndex < 0 && model.count > 0) currentIndex = 0
                        }
                    }

                    // --- SQL (Squelch / Gate threshold) dB ---
                    // ✅ รับค่าจาก C++ (emit updateGateThDbFromServer(double))
                    // ✅ ช่อง TextField จะ "ตามค่า" ตอนไม่โฟกัส
                    // ✅ ตอนโฟกัส = พิมพ์เอง ไม่โดนทับ
                    Item {
                        id: sqlItem
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44

                        // ===== value หลัก =====
                        property real sqlGateDb: -130.0

                        // init / pending
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

                        // ✅ จุดเดียวที่ “เปลี่ยนค่า” แล้ว UI จะตามเอง
                        function applyValue(v) {
                            sqlGateDb = v
                            // slider ใช้ binding กับ sqlGateDb อยู่แล้ว ไม่ต้อง set value ซ้ำ
                        }

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

                                    // ✅ bind ค่า slider กับค่าเดียวกัน
                                    value: sqlItem.sqlGateDb

                                    // ระหว่างลาก: อัปเดตค่าหลักทันที (TextField จะตามเองถ้าไม่โฟกัส)
                                    onValueChanged: {
                                        // ถ้าถูก set จาก server ก็ให้ผ่านได้ปกติ
                                        // ถ้ากำลังลาก ก็จะเปลี่ยน sqlGateDb และ UI ตาม
                                        if (pressed) {
                                            sqlItem.sqlGateDb = sqlItem.clamp(value, -140.0, 0.0)
                                        }
                                    }

                                    // ปล่อยเมาส์: ค่อย commit + (ถ้าจะส่งกลับ C++)
                                    onPressedChanged: {
                                        if (!pressed) {
                                            var v = sqlItem.clamp(value, -140.0, 0.0)
                                            sqlItem.applyValue(v)

                                            // ✅ ส่งกลับ C++ ถ้าต้องการ
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

                                    // ✅ KEY: ตอน "ไม่โฟกัส" ให้ text bind ตาม sqlGateDb
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

                                        // ✅ ส่งกลับ C++ ถ้าต้องการ
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

                        // ✅ รับค่าจาก C++ (server push)
                        Connections {
                            target: krakenmapval

                            function onUpdateGateThDbFromServer(v) {
                                var vv = sqlItem.clamp(v, -140.0, 0.0)
                                console.log("sqlItem got updateGateThDbFromServer:", vv,
                                            "focus=", sqlDbField.activeFocus,
                                            "oldText=", sqlDbField.text,
                                            "oldGate=", sqlItem.sqlGateDb)

                                if (!sqlItem.hasInit) {
                                    sqlItem.pendingGateDb = vv
                                    sqlItem.hasPending = true
                                    return
                                }

                                // ถ้ากำลังพิมพ์: อัปเดตค่า/slider ได้ แต่ไม่ไปยุ่ง text (Binding ถูกปิดเพราะ focus)
                                sqlItem.applyValue(vv)
                            }
                            function onUpdateDoaAlgorithmFromServer(algo) {
                                console.log("sqlItem got onUpdateDoaAlgorithmFromServer:", algo)
                                doaAlgoCombo.currentIndex = doaAlgoCombo.indexFromValue(algo)
                            }
                        }
                    }

                    // --- Tx Rate (Hz) : slider + manual input ---
                    // รับจาก C++: emit updateTxHzFromServer(double v)
                    // ส่งกลับ C++: krakenmapval.sendTxHz(v)
                    // กัน loop: krakenmapval.blockUiSync

                    Item {
                        id: txItem
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44

                        // ===== value cache =====
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

                            // อัปเดตช่องเฉพาะตอน "ไม่ได้พิมพ์"
                            if (!hzField.activeFocus) {
                                const s = fmt1(v)
                                if (hzField.text !== s) {
                                    Qt.callLater(function() { hzField.text = s })
                                }
                            }
                        }

                        Component.onCompleted: {
                            hasInit = true

                            // ค่าเริ่มต้น: ดึงจาก C++ ถ้ามี
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

                                    // ระหว่างลาก -> อัปเดตช่องถ้าไม่ได้พิมพ์
                                    onValueChanged: {
                                        if (!hzField.activeFocus) {
                                            const s = txItem.fmt1(value)
                                            if (hzField.text !== s)
                                                hzField.text = s
                                        }
                                    }

                                    // ปล่อยเมาส์แล้วค่อย commit + ส่งกลับ C++
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

                                        // sync UI
                                        txItem.applyUi(v)
                                        text = txItem.fmt1(v)

                                        // ส่งกลับ C++
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
                                            // หลุด focus -> โชว์ค่าล่าสุด
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

                        // ===== Receive from C++ =====
                        Connections {
                            target: krakenmapval

                            // C++: emit updateTxHzFromServer(double v);
                            function onUpdateTxHzFromServer(v) {
                                // กัน loop/กันสัญญาณมาก่อน init
                                var vv = txItem.clamp(v, 0.2, 60.0)

                                if (!txItem.hasInit) {
                                    txItem.pendingTxHz = vv
                                    txItem.hasPending = true
                                    return
                                }

                                // ถ้ากำลังพิมพ์ -> ไม่ทับ text แต่ slider+ค่าภายในอัปเดตได้
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

                        // ✅ เก็บค่าที่ "ถูกต้องล่าสุด" ไว้สำหรับ rollback
                        property string lastGoodText: "0.81"

                        function clampRadius(v) {
                            v = Number(v)
                            if (isNaN(v)) return NaN
                            if (v <= 0) return NaN
                            return v
                        }

                        function fmt2(v) {
                            return Number(v).toFixed(2)
                        }

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

                            // ✅ ส่งกลับ C++
                            if (krakenmapval && !krakenmapval.blockUiSync) {
                                // แนะนำให้เป็น sendUcaRadiusM / sendArrayRadius ให้ชื่อชัดเจน
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

                        Label {
                            text: "RF AGC:"
                            font.pixelSize: 16
                            color: "#ffffff"
                        }

                        CheckBox {
                            id: agcCheck
                            checked: true

                            onToggled: {
                                if (!krakenmapval) return
                                if (krakenmapval.blockUiSync) return

                                // ✅ ส่ง -1 = all channels
                                krakenmapval.sendRfAgcEnable(-1, checked)
                            }
                        }
                    }
                    Item { Layout.fillWidth: true}

                    // ColumnLayout {
                    //     Layout.columnSpan: 2
                    //     Layout.fillWidth: true
                    //     spacing: 10

                        // =========================================================
                        // 1) ScannerATT #7 : slider + manual input
                        // =========================================================
                        // Item {
                        //     Layout.fillWidth: true
                        //     Layout.preferredHeight: 44

                        //     Rectangle {
                        //         anchors.fill: parent
                        //         radius: 10
                        //         color: "#111A1E"
                        //         border.color: "#1B8F77"
                        //         border.width: 1

                        //         RowLayout {
                        //             anchors.fill: parent
                        //             anchors.leftMargin: 8
                        //             anchors.rightMargin: 8
                        //             spacing: 10

                        //             Text {
                        //                 text: "ScannerATT #7"
                        //                 color: "#ffffff"
                        //                 font.pixelSize: 16
                        //                 Layout.preferredWidth: 140
                        //                 Layout.alignment: Qt.AlignVCenter
                        //             }

                        //             Slider {
                        //                 id: scannerAttSlider
                        //                 Layout.fillWidth: true
                        //                 from: -90
                        //                 to: -30
                        //                 stepSize: 0.5
                        //                 value: root.scannerAttDb
                        //                 enabled: true
                        //                 Layout.alignment: Qt.AlignVCenter

                        //                 onPressedChanged: {
                        //                     if (!pressed) {
                        //                         var v = value
                        //                         scannerAttField.text = v.toFixed(1)
                        //                         root.scannerAttDb = v
                        //                         if (root.sendScannerAttDb) root.sendScannerAttDb(v)
                        //                     }
                        //                 }

                        //                 onValueChanged: {
                        //                     if (!scannerAttField.activeFocus)
                        //                         scannerAttField.text = value.toFixed(1)
                        //                 }
                        //             }

                        //             TextField {
                        //                 id: scannerAttField
                        //                 Layout.preferredWidth: 90
                        //                 Layout.preferredHeight: 32
                        //                 font.pixelSize: 16
                        //                 color: "#7AE2CF"
                        //                 inputMethodHints: Qt.ImhFormattedNumbersOnly
                        //                 validator: DoubleValidator { bottom: -90; top: -30; decimals: 1 }
                        //                 text: Number(root.scannerAttDb).toFixed(1)
                        //                 enabled: true
                        //                 verticalAlignment: Text.AlignVCenter
                        //                 horizontalAlignment: Text.AlignRight
                        //                 leftPadding: 10
                        //                 rightPadding: 10
                        //                 topPadding: 4
                        //                 bottomPadding: 4

                        //                 background: Rectangle {
                        //                     color: "#111A1E"
                        //                     radius: 10
                        //                     border.color: scannerAttField.activeFocus ? "#7AE2CF" : "#1B8F77"
                        //                     border.width: 1
                        //                 }

                        //                 property string previousValue: ""

                        //                 function commit() {
                        //                     var v = Number(text)
                        //                     if (isNaN(v)) v = Number(scannerAttSlider.value)
                        //                     if (v < -90) v = -90
                        //                     if (v > -30) v = -30

                        //                     scannerAttSlider.value = v
                        //                     text = v.toFixed(1)

                        //                     root.scannerAttDb = v
                        //                     if (root.sendScannerAttDb) root.sendScannerAttDb(v)

                        //                     scannerAttField.focus = false
                        //                 }

                        //                 Keys.onReturnPressed: commit()
                        //                 Keys.onEnterPressed:  commit()

                        //                 onFocusChanged: if (focus) { previousValue = text; selectAll() }

                        //                 onEditingFinished: {
                        //                     if (text !== previousValue) commit()
                        //                     else scannerAttField.focus = false
                        //                 }
                        //             }

                        //             Text {
                        //                 text: "dB"
                        //                 color: "#7AE2CF"
                        //                 font.pixelSize: 16
                        //                 Layout.alignment: Qt.AlignVCenter
                        //             }
                        //         }
                        //     }
                        // }

                        // =========================================================
                        // 2) Target (DF CH0..CH4) : channel select + slider + input
                        // =========================================================
                    Item {
                        id: rfAgcTargetAllItem
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44

                        property var  targetDbArr: [-60, -60, -60, -60, -60]
                        property real targetAllDb: -60
                        property bool blockLocal: false

                        // ✅ กันส่งซ้ำ
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

                        // ✅ ส่งไป C++ แบบกันซ้ำ
                        function sendAllOnce(v, reason) {
                            v = clampDb(v)

                            // กัน loop ตอน server push
                            if (blockLocal) return
                            if (!krakenmapval) return
                            if (krakenmapval.blockUiSync) return

                            // กันส่งซ้ำ (เท่ากันแบบ 0.1dB)
                            var vv = Math.round(v * 10) / 10
                            var ll = Math.round(lastSentDb * 10) / 10
                            if (vv === ll) {
                                // console.log("[QML][RFAGC] skip duplicate", reason, vv)
                                return
                            }

                            lastSentDb = vv
                            // console.log("[QML][RFAGC] SEND", reason, vv)
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

                                    // ระหว่างลาก: โชว์เลขอย่างเดียว ห้ามส่ง
                                    onValueChanged: {
                                        if (rfAgcTargetAllItem.blockLocal) return
                                        if (!targetAllField.activeFocus)
                                            targetAllField.text = rfAgcTargetAllItem.clampDb(value).toFixed(1)
                                    }

                                    // ✅ ส่ง “ครั้งเดียว” ตอนปล่อยเมาส์
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

                                        // ✅ ส่ง “ครั้งเดียว” จาก field commit
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

                        // ✅ รับค่าจาก C++ ทีละ ch=0..4 แล้วรวมเป็น slider เดียว
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
                    // }
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
                                // แค่ตั้ง currentIndex จะไม่เรียก onActivated เอง
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
                            // server ส่ง -1 มา = all
                            if (ch < 0) {
                                if (agcCheck.checked !== !!enable)
                                    agcCheck.checked = !!enable
                                return
                            }

                            // ถ้าอนาคตอยากให้ per-channel ก็ handle ตรงนี้ได้
                            // เช่น rfAgcChEnabled[ch] = enable;
                        }
                        // onVfoConfigUpdated(...) ถ้าจะใช้ ใส่ตรงนี้
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
                            // updateDegreelocal
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
