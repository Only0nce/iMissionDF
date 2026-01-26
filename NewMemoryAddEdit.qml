import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

Item {
    id: root
    width: 1205
    height: 400
    visible: true

    /* ===== Dependencies/refs ===== */
    // property var  mainWindows:    null
    // property var  stackView:      null
    // property var  listView:       null
    // property var  configManager:  null

    /* ===== IMPORTANT: SOURCE MODEL =====
       โปรเจคจริงคุณน่าจะมี radioMemList อยู่แล้ว (ListModel หรือ ListView.model)
       - ถ้าเป็น ListModel:     root.radioMemList = yourListModel
       - ถ้าเป็น ListView:      root.radioMemList = yourListView   (โค้ดจะใช้ .model ให้อัตโนมัติ)
    */
    property var radioMemLists: null

    /* ===== ตัวแปร/สถานะ เดิม ===== */
    property var  pendingDSP: ({})
    property real pendingCenterFreq: 0
    property var  pendingUIParams: ({})
    property bool startDSPAfter: false
    property bool selectMode: widgetView

    // /* ====== MOCK (กันหน้าโล่งตอนทดสอบ) ====== */
    // ListModel {
    //     id: mockModel
    //     ListElement { profileId: "1"; name: "PRESET-1"; center_freq: 100000000; low_cut: -6000; high_cut: 6000; offset_freq: 0; mod: "wfm"; dmr_filter: 0; audio_service_id: 0; squelch_level: -80; secondary_mod: "" }
    //     ListElement { profileId: "2"; name: "PRESET-2"; center_freq: 118000000; low_cut: -3000; high_cut: 3000; offset_freq: 0; mod: "am";  dmr_filter: 0; audio_service_id: 0; squelch_level: -75; secondary_mod: "" }
    //     ListElement { profileId: "3"; name: "PRESET-3"; center_freq: 145500000; low_cut: -2500; high_cut: 2500; offset_freq: 0; mod: "nfm"; dmr_filter: 0; audio_service_id: 0; squelch_level: -70; secondary_mod: "" }
    //     ListElement { profileId: "4"; name: "PRESET-4"; center_freq: 7100000;   low_cut: -2400; high_cut: 2400; offset_freq: 0; mod: "lsb"; dmr_filter: 0; audio_service_id: 0; squelch_level: -68; secondary_mod: "" }
    //     ListElement { profileId: "5"; name: "PRESET-5"; center_freq: 433920000; low_cut: -12500; high_cut: 12500; offset_freq: 0; mod: "dmr"; dmr_filter: 0; audio_service_id: 0; squelch_level: -78; secondary_mod: "" }
    // }

    /* ========================= FILTER ENGINE (สำคัญ) ========================= */
    // ✅ model ปลายทางที่ GridView จะใช้จริง
    ListModel { id: filteredMemModel }

    // ✅ เลือก source model: ถ้าไม่ได้ส่ง radioMemList มา -> ใช้ mockModel
    function memSrcModel() {
        // รองรับกรณีส่งเข้ามาเป็น ListView (มี .model) หรือเป็น ListModel ตรงๆ
        var src = (radioMemLists && radioMemLists.model) ? radioMemLists.model : radioMemLists
        return src ? src : null
    }


    // ✅ clone roles ทุกอย่างจาก element เพื่อ append เข้า ListModel ใหม่
    function cloneObj(obj) {
        var o = {}
        for (var k in obj) o[k] = obj[k]
        return o
    }

    // ✅ คืนค่า mod จาก element แบบรองรับหลายชื่อ role
    function extractMod(it) {
        if (!it) return ""
        if (it.mod !== undefined) return String(it.mod)
        if (it.modulation !== undefined) return String(it.modulation)
        if (it.mode !== undefined) return String(it.mode)
        return ""
    }

    // ✅ กรองการ์ด: mode="" => show all
    function applyFilterToGrid(mode) {
        var src = memSrcModel()
        filteredMemModel.clear()

        if (!src || typeof src.count !== "number" || typeof src.get !== "function") {
            console.log("[FilterMem] applyFilterToGrid: src invalid", src)
            return
        }

        var want = (mode !== undefined && mode !== null) ? String(mode).trim() : ""

        for (var i = 0; i < src.count; i++) {
            var it = src.get(i)
            var m  = extractMod(it)

            // ✅ ถ้าต้องการ ignore case ให้ใช้:
            // if (want === "" || m.toLowerCase() === want.toLowerCase())
            if (want === "" || m === want) {
                filteredMemModel.append(cloneObj(it))
            }
        }

        console.log("[FilterMem] mode=", want, " filtered=", filteredMemModel.count, "/", src.count)
    }

    Component.onCompleted: {
        // ✅ เริ่มต้น: โชว์ทั้งหมด
        applyFilterToGrid("")
    }

    Timer {
        id: syncTimer
        interval: 50
        repeat: false
        onTriggered: root.applyFilterToGrid(filterMemoryButton.selectedMode)
    }

    Connections {
        target: root.memSrcModel()
        ignoreUnknownSignals: true

        function onCountChanged() {
            syncTimer.restart()
        }
    }

    /* ===== Helper ===== */
    function mhz(v)  { return (v / 1e6).toFixed(6) }
    function fmtHz(v){
        var a = Math.abs(v)
        if (a >= 1e6) return (v/1e6).toFixed(3) + " MHz"
        if (a >= 1e3) return (v/1e3).toFixed(3) + " kHz"
        return v + " Hz"
    }

    // property bool modifyPreset: homeDisplay.modifyPreset
    function editPreset(id,name) {
        modifyPreset = true
        modifyPresetId = id
        modifyPresetName = name
        // stackView.pop(null)
        // listView.currentIndex = 0
    }

    // ===== โหมดสลับ Scan / Memory (2 ปุ่ม) =====
    Column {
        id: modeButtons
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 5
        spacing: 6
        z: 1000

        property int btnW: 150
        property int btnH: 40

        // =================== SCAN ===================
        ToolButton {
            id: btnScan
            width: modeButtons.btnW
            height: modeButtons.btnH
            hoverEnabled: true

            property bool active: widgetView === true

            scale: down ? 0.98 : 1.0
            Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

            background: Rectangle {
                id: bgModeScan
                radius: 8
                border.color: "#0c4a3e"
                border.width: 1
                color: "#169976"

                Rectangle {
                    anchors.fill: parent
                    radius: bgModeScan.radius
                    color: "#ffffff"
                    opacity: btnScan.down ? 0.18 : (btnScan.hovered ? 0.10 : 0.0)
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgModeScan.radius
                    color: "#ffffff"
                    opacity: btnScan.active ? 0.14 : 0.0
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgModeScan.radius
                    color: "#1F2933"
                    property real activeBase: 0.14
                    property real hoverBoost: 0.06
                    property real pressBoost: 0.10

                    opacity: btnScan.active
                             ? (btnScan.down ? (activeBase + pressBoost)
                                            : (btnScan.hovered ? (activeBase + hoverBoost)
                                                               : activeBase))
                             : 0.0

                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgModeScan.radius
                    color: "transparent"
                    border.color: "#ffffff"
                    border.width: 1
                    opacity: btnScan.hovered ? 0.18 : 0.0
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgModeScan.radius
                    color: "transparent"
                    border.color: "#ffffff"
                    border.width: 1
                    opacity: btnScan.active ? 0.28 : 0.0
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 140 } }
                }
            }

            contentItem: Text {
                text: "Scan"
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: selectMode ? "#FFFFFF" : "#9CA3AF"
                font.pixelSize: 16
                font.bold: true
            }

            onClicked: {
                widgetView = true
            }
        }

        // =================== MEMORY ===================
        ToolButton {
            id: btnMemory
            width: modeButtons.btnW
            height: modeButtons.btnH
            hoverEnabled: true

            property bool active: widgetView === false

            scale: down ? 0.98 : 1.0
            Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

            background: Rectangle {
                id: bgModeMem
                radius: 8
                border.color: "#0c4a3e"
                border.width: 1
                color: "#169976"

                Rectangle {
                    anchors.fill: parent
                    radius: bgModeMem.radius
                    color: "#ffffff"
                    opacity: btnMemory.down ? 0.18 : (btnMemory.hovered ? 0.10 : 0.0)
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgModeMem.radius
                    color: "#ffffff"
                    opacity: btnMemory.active ? 0.14 : 0.0
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgModeMem.radius
                    color: "#1F2933"
                    property real activeBase: 0.12
                    property real hoverBoost: 0.06
                    property real pressBoost: 0.10

                    opacity: btnMemory.active
                             ? (btnMemory.down ? (activeBase + pressBoost)
                                              : (btnMemory.hovered ? (activeBase + hoverBoost)
                                                                  : activeBase))
                             : 0.0

                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgModeMem.radius
                    color: "transparent"
                    border.color: "#ffffff"
                    border.width: 1
                    opacity: btnMemory.hovered ? 0.18 : 0.0
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgModeMem.radius
                    color: "transparent"
                    border.color: "#ffffff"
                    border.width: 1
                    opacity: btnMemory.active ? 0.28 : 0.0
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 140 } }
                }
            }

            contentItem: Text {
                text: "Memory"
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: selectMode ? "#9CA3AF" : "#FFFFFF"
                font.pixelSize: 16
                font.bold: true
            }

            // ✅ คุณเขียนผิดเดิมเป็น widgetView=true ตลอด
            onClicked: {
                widgetView = true
            }
        }
    }

    // ===== กลุ่มปุ่ม Delete / Filter =====
    Column {
        id: deleteButtons
        anchors.top: modeButtons.bottom
        anchors.right: parent.right
        anchors.topMargin: 6
        anchors.rightMargin: 5
        spacing: 6
        z: 999

        property int btnW: 150
        property int btnH: 40

        // ================= Delete Scan =================
        ToolButton {
            id: deleteScanButton
            width: deleteButtons.btnW
            height: deleteButtons.btnH
            hoverEnabled: true

            scale: down ? 0.98 : 1.0
            Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

            background: Rectangle {
                id: bgDeleteScan
                radius: 8
                color: "#F4320B"
                border.color: "#0c4a3e"
                border.width: 1
                opacity: 0.95

                Rectangle {
                    anchors.fill: parent
                    radius: bgDeleteScan.radius
                    color: "#ffffff"
                    opacity: deleteScanButton.down ? 0.22 : (deleteScanButton.hovered ? 0.12 : 0.0)
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgDeleteScan.radius
                    color: "#000000"
                    property real hoverDark: 0.10
                    property real pressDark: 0.16
                    opacity: deleteScanButton.down ? pressDark
                          : (deleteScanButton.hovered ? hoverDark : 0.0)
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgDeleteScan.radius
                    color: "transparent"
                    border.color: "#ffffff"
                    border.width: 1
                    opacity: deleteScanButton.hovered ? 0.18 : 0.0
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                }
            }

            contentItem: Text {
                text: "Delete Scan"
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "#ffffff"
                font.pixelSize: 16
                font.bold: true
            }

            onClicked: {
                console.log("Delete Scan Presets")
                clearPresetActionDialog.messageText = "Delete Scan All"
                clearPresetActionDialog.messageInt = 1
                clearPresetActionDialog.open()
            }
        }

        // ================= Delete Memory =================
        ToolButton {
            id: deleteMemoryButton
            width: deleteButtons.btnW
            height: deleteButtons.btnH
            hoverEnabled: true

            scale: down ? 0.98 : 1.0
            Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

            background: Rectangle {
                id: bgDeleteMem
                radius: 8
                color: "#F4320B"
                border.color: "#0c4a3e"
                border.width: 1
                opacity: 0.95

                Rectangle {
                    anchors.fill: parent
                    radius: bgDeleteMem.radius
                    color: "#ffffff"
                    opacity: deleteMemoryButton.down ? 0.22 : (deleteMemoryButton.hovered ? 0.12 : 0.0)
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgDeleteMem.radius
                    color: "#000000"
                    property real hoverDark: 0.10
                    property real pressDark: 0.16
                    opacity: deleteMemoryButton.down ? pressDark
                          : (deleteMemoryButton.hovered ? hoverDark : 0.0)
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgDeleteMem.radius
                    color: "transparent"
                    border.color: "#ffffff"
                    border.width: 1
                    opacity: deleteMemoryButton.hovered ? 0.18 : 0.0
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                }
            }

            contentItem: Text {
                text: "Delete Memory"
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "#ffffff"
                font.pixelSize: 16
                font.bold: true
            }

            onClicked: {
                console.log("Delete Memory Presets")
                clearPresetActionDialog.messageText = "Delete Memory All"
                clearPresetActionDialog.messageInt = 2
                clearPresetActionDialog.open()
            }
        }

        // ================= Filter Memory (Dialog) =================
        ToolButton {
            id: filterMemoryButton
            width: deleteButtons.btnW
            height: deleteButtons.btnH
            hoverEnabled: true

            property string selectedMode: ""

            // ✅ model เก็บโหมดแบบไม่ซ้ำ
            ListModel { id: uniqueModeModel }

            // ✅ model สำหรับ “จับคู่” ให้เป็น 2 คอลัมน์ (ซ้าย|ขวา)
            ListModel { id: pairModeModel }

            // ✅ rebuild unique modes จาก source (radioMemList หรือ mockModel)
            function rebuildUniqueModes() {
                uniqueModeModel.clear()

                var src = root.memSrcModel()
                if (!src || typeof src.count !== "number" || typeof src.get !== "function") {
                    console.log("[FilterMem] src invalid")
                    return
                }

                var seen = {}
                for (var i = 0; i < src.count; i++) {
                    var it = src.get(i)
                    var m  = root.extractMod(it)
                    if (!m || seen[m]) continue
                    seen[m] = true
                    uniqueModeModel.append({ mod: m })
                }
            }

            // ✅ pair 2 columns: [0|1], [2|3]...
            function rebuildPairModes() {
                pairModeModel.clear()
                var n = uniqueModeModel.count
                for (var i = 0; i < n; i += 2) {
                    var left  = uniqueModeModel.get(i).mod
                    var right = (i + 1 < n) ? uniqueModeModel.get(i + 1).mod : ""
                    pairModeModel.append({ left: left, right: right })
                }
            }

            // ✅ กดแล้วใช้ “กรองการ์ดจริง” ด้วย
            function selectAndApply(m) {
                selectedMode = String(m)
                root.applyFilterToGrid(selectedMode)   // ✅ นี่แหละที่ทำให้ gridViewport เหลือเฉพาะโหมด
            }
            function clearAndApply() {
                selectedMode = ""
                root.applyFilterToGrid("")             // ✅ กลับมา show all
            }

            // ✅ วางตรงนี้เลย
            Component.onCompleted: {
                clearAndApply()   // default = ALL
            }

            scale: down ? 0.98 : 1.0
            Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

            onClicked: {
                rebuildUniqueModes()
                rebuildPairModes()
                modeDialog.open()
            }

            background: Rectangle {
                id: bgFilterMem
                radius: 8
                color: "#12568C"
                border.color: "#0c4a3e"
                border.width: 1
                opacity: 0.95

                Rectangle {
                    anchors.fill: parent
                    radius: bgFilterMem.radius
                    color: "#ffffff"
                    opacity: filterMemoryButton.down ? 0.22 : (filterMemoryButton.hovered ? 0.12 : 0.0)
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgFilterMem.radius
                    color: "#000000"
                    property real hoverDark: 0.10
                    property real pressDark: 0.16
                    opacity: filterMemoryButton.down ? pressDark
                          : (filterMemoryButton.hovered ? hoverDark : 0.0)
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: bgFilterMem.radius
                    color: "transparent"
                    border.color: "#ffffff"
                    border.width: 1
                    opacity: filterMemoryButton.hovered ? 0.18 : 0.0
                    visible: opacity > 0.001
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                }
            }

            contentItem: Text {
                text: filterMemoryButton.selectedMode !== ""
                      ? ("Filter: " + filterMemoryButton.selectedMode.toUpperCase() + " ▼")
                      : "Filter Memory ▼"
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "#ffffff"
                font.pixelSize: 16
                font.bold: true
                elide: Text.ElideRight
            }

            // ================= Dialog กลางจอ =================
            Dialog {
                id: modeDialog
                parent: Overlay.overlay
                modal: true
                focus: true
                dim: true
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                anchors.centerIn: parent

                width: 420
                height: Math.min(520, parent.height * 0.8)

                // ===== THEME =====
                property color dBg:     "#111820"
                property color dBorder: "#2A3A44"
                property color dText:   "#E6EDF3"
                property color dSub:    "#9AA6B2"
                property color dHover:  "#1B2A33"
                property color dAccent: "#00c896"
                property color dPress:  "#223742"

                background: Rectangle {
                    radius: 16
                    color: modeDialog.dBg
                    border.color: modeDialog.dBorder
                    border.width: 1
                }

                header: Rectangle {
                    height: 48
                    color: "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 10

                        Text {
                            text: "Filter Memory Mode"
                            color: modeDialog.dText
                            font.pixelSize: 18
                            font.bold: true
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item { Layout.fillWidth: true }

                        // ToolButton {
                        //     text: "✕"
                        //     Layout.alignment: Qt.AlignVCenter
                        //     onClicked: modeDialog.close()
                        // }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: "#22313A"
                        opacity: 0.9
                    }
                }

                contentItem: Item {
                    anchors.fill: parent

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        // ===== LIST (2 columns fishbone) + Scroll hint + Arrows + No overflow =====
                        Item {
                            id: modeListWrap
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 260
                            clip: true

                            // ----- Arrows (scroll up/down) -----
                            Row {
                                id: arrowRow
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.topMargin: 0
                                anchors.rightMargin: 0
                                spacing: 6
                                z: 50

                                // แสดงเฉพาะตอน “เลื่อนได้จริง”
                                visible: modeListView.contentHeight > modeListView.height + 2

                                // ▲
                                ToolButton {
                                    width: 34
                                    height: 28
                                    text: "▲"
                                    enabled: modeListView.contentY > 0
                                    opacity: enabled ? 0.95 : 0.25
                                    onClicked: {
                                        modeListView.contentY = Math.max(0, modeListView.contentY - 120)
                                    }
                                }

                                // ▼
                                ToolButton {
                                    width: 34
                                    height: 28
                                    text: "▼"
                                    enabled: modeListView.contentY < (modeListView.contentHeight - modeListView.height - 1)
                                    opacity: enabled ? 0.95 : 0.25
                                    onClicked: {
                                        var maxY = Math.max(0, modeListView.contentHeight - modeListView.height)
                                        modeListView.contentY = Math.min(maxY, modeListView.contentY + 120)
                                    }
                                }
                            }

                            // ----- ListView -----
                            ListView {
                                id: modeListView
                                anchors.fill: parent
                                anchors.topMargin: 36        // เว้นให้ลูกศรอยู่ด้านบน
                                clip: true

                                spacing: 10
                                model: pairModeModel

                                boundsBehavior: Flickable.StopAtBounds
                                flickableDirection: Flickable.VerticalFlick

                                // ✅ กัน overflow ใน delegate: ให้ขนาดต่อแถวคงที่ + elide text
                                delegate: RowLayout {
                                    width: modeListView.width
                                    height: 56
                                    spacing: 10

                                    // ===== LEFT =====
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 56
                                        radius: 12
                                        visible: (model.left !== "")
                                        clip: true   // ✅ กันลูก/ข้อความล้นกรอบ

                                        color: (leftMouse.pressed ? modeDialog.dPress
                                              : (leftMouse.containsMouse ? modeDialog.dHover : "transparent"))

                                        border.width: (filterMemoryButton.selectedMode === model.left) ? 1 : 0
                                        border.color: (filterMemoryButton.selectedMode === model.left) ? modeDialog.dAccent : "transparent"

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 12
                                            anchors.rightMargin: 12
                                            spacing: 16

                                            Rectangle {
                                                width: 18; height: 18
                                                radius: 6
                                                Layout.alignment: Qt.AlignVCenter
                                                color: (filterMemoryButton.selectedMode === model.left) ? modeDialog.dAccent : "transparent"
                                                border.color: (filterMemoryButton.selectedMode === model.left) ? modeDialog.dAccent : modeDialog.dBorder
                                                border.width: 1

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "✓"
                                                    visible: (filterMemoryButton.selectedMode === model.left)
                                                    color: "#0B1216"
                                                    font.pixelSize: 16
                                                    font.bold: true
                                                }
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: model.left.toUpperCase()
                                                color: modeDialog.dText
                                                font.pixelSize: 18
                                                font.bold: (filterMemoryButton.selectedMode === model.left)
                                                elide: Text.ElideRight
                                                wrapMode: Text.NoWrap
                                                maximumLineCount: 1
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }

                                        MouseArea {
                                            id: leftMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: {
                                                filterMemoryButton.selectAndApply(model.left)
                                                modeDialog.close()
                                            }
                                        }
                                    }

                                    // ===== RIGHT =====
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 56
                                        radius: 12
                                        visible: (model.right !== "")
                                        clip: true   // ✅ กันล้น

                                        color: (rightMouse.pressed ? modeDialog.dPress
                                              : (rightMouse.containsMouse ? modeDialog.dHover : "transparent"))

                                        border.width: (filterMemoryButton.selectedMode === model.right) ? 1 : 0
                                        border.color: (filterMemoryButton.selectedMode === model.right) ? modeDialog.dAccent : "transparent"

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 12
                                            anchors.rightMargin: 12
                                            spacing: 16

                                            Rectangle {
                                                width: 18; height: 18
                                                radius: 6
                                                Layout.alignment: Qt.AlignVCenter
                                                color: (filterMemoryButton.selectedMode === model.right) ? modeDialog.dAccent : "transparent"
                                                border.color: (filterMemoryButton.selectedMode === model.right) ? modeDialog.dAccent : modeDialog.dBorder
                                                border.width: 1

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "✓"
                                                    visible: (filterMemoryButton.selectedMode === model.right)
                                                    color: "#0B1216"
                                                    font.pixelSize: 16
                                                    font.bold: true
                                                }
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: model.right.toUpperCase()
                                                color: modeDialog.dText
                                                font.pixelSize: 18
                                                font.bold: (filterMemoryButton.selectedMode === model.right)
                                                elide: Text.ElideRight
                                                wrapMode: Text.NoWrap
                                                maximumLineCount: 1
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }

                                        MouseArea {
                                            id: rightMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: {
                                                filterMemoryButton.selectAndApply(model.right)
                                                modeDialog.close()
                                            }
                                        }
                                    }
                                }

                                // ✅ ScrollBar ชัด ๆ (บอกว่าเลื่อนได้)
                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AlwaysOn
                                    active: true
                                    width: 10
                                    contentItem: Rectangle {
                                        radius: 6
                                        color: "#6B7A8C"
                                        opacity: 0.85
                                    }
                                    background: Rectangle {
                                        radius: 6
                                        color: "#22313A"
                                        opacity: 0.45
                                    }
                                }
                            }

                            // ----- Top/Bottom fade hint (บอกว่ามีของซ่อนอยู่) -----
                            Rectangle {
                                // fade บน
                                anchors.top: modeListView.top
                                anchors.left: modeListView.left
                                anchors.right: modeListView.right
                                height: 18
                                z: 40
                                visible: modeListView.contentY > 1
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: modeDialog.dBg }
                                    GradientStop { position: 1.0; color: "transparent" }
                                }
                                opacity: 0.95
                            }

                            Rectangle {
                                // fade ล่าง
                                anchors.bottom: modeListView.bottom
                                anchors.left: modeListView.left
                                anchors.right: modeListView.right
                                height: 18
                                z: 40
                                visible: modeListView.contentY < (modeListView.contentHeight - modeListView.height - 1)
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "transparent" }
                                    GradientStop { position: 1.0; color: modeDialog.dBg }
                                }
                                opacity: 0.95
                            }
                        }


                        // ===== Divider =====
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#22313A"
                            opacity: 0.9
                        }

                        // ===== Selected + count (อยู่ล่าง) =====
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                text: filterMemoryButton.selectedMode !== ""
                                      ? ("Selected: " + filterMemoryButton.selectedMode.toUpperCase())
                                      : "Selected: (ALL)"
                                color: modeDialog.dSub
                                font.pixelSize: 14
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }

                            Text {
                                text: uniqueModeModel.count > 0 ? (uniqueModeModel.count + " modes") : "no modes"
                                color: "#66ffffff"
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        // ===== Buttons row (ล่างสุด ชิดขวา) =====
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Item { Layout.fillWidth: true }

                            Button {
                                text: "Clear"
                                onClicked: {
                                    filterMemoryButton.clearAndApply() // ✅ โชว์ทั้งหมดจริง
                                    modeDialog.close()
                                }
                            }

                            // Button {
                            //     text: "Close"
                            //     onClicked: modeDialog.close()
                            // }
                        }
                    }
                }
            }
        }
    }

    /* ===================== Timer (ของเดิม) ===================== */
    Timer {
        id: dspDelayTimer
        interval: 500
        repeat: false
        onTriggered: {
            mainWindows.sendmessage(JSON.stringify(pendingDSP))

            radioScanner.spectrumGLPlot.centerFreq = pendingCenterFreq
            radioScanner.spectrumGLPlot.low_cut = pendingUIParams.low_cut
            radioScanner.spectrumGLPlot.high_cut = pendingUIParams.high_cut
            radioScanner.spectrumGLPlot.offsetFrequency = pendingUIParams.offset_freq
            radioScanner.spectrumGLPlot.start_mod = pendingUIParams.mod
            scanSqlLevel = (pendingUIParams.squelch_level * 2) + 255
            stackView.pop(null)
            listView.currentIndex = 0

            if (startDSPAfter) {
                let dspcontrolStart = { type: "dspcontrol", action: "start" }
                mainWindows.sendmessage(JSON.stringify(dspcontrolStart))
            }
        }
    }

    /* ===================== GRID VIEWPORT (สำคัญ) ===================== */
    Item {
        id: gridViewport
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter

        property int maxColumns: 5
        property int sidePad: 24
        property int cardW: 320
        property int cardH: 150

        width: Math.min(parent.width - sidePad * 2, cardW * maxColumns)
        height: cardH * 2
        clip: true

        GridView {
            id: grid
            anchors.fill: parent
            clip: true
            interactive: true

            cellWidth:  gridViewport.cardW
            cellHeight: gridViewport.cardH

            // ✅ สำคัญที่สุด: เปลี่ยนจาก radioMemList -> filteredMemModel
            model: filteredMemModel

            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AlwaysOn
                width: 10
                contentItem: Rectangle { color: "#5f728a"; radius: 4 }
            }

            cacheBuffer: cellHeight * 4

            delegate: Item {
                id: delegateRoot

                property int gapX: 20
                property int gapY: 10

                width: grid.cellWidth - gapX
                height: grid.cellHeight - gapY

                property real u: height / 210.0
                function s(v) { return (v === undefined || v === null) ? "" : String(v) }
                function n(v, d) {
                    var x = Number(v)
                    return (v === undefined || v === null || v === "" || isNaN(x)) ? (d === undefined ? 0 : d) : x
                }

                property color cTitle:   "#FFFFFF"
                property color cValue:   "#F2F6FF"
                property color cLabel:   "#BFD0E6"
                property color cOutline: "#081018"

                // ===== model roles (มาจาก filteredMemModel แล้ว) =====
                property string profileId:        model.profileId
                property string presetName:       model.name
                property real   center_freq:      model.center_freq
                property int    low_cut:          model.low_cut
                property int    high_cut:         model.high_cut
                property real   offset_freq:      model.offset_freq
                property string mod:              model.mod
                property int    dmr_filter:       model.dmr_filter
                property int    audio_service_id: model.audio_service_id
                property int    squelch_level:    model.squelch_level
                property string secondary_mod:    model.secondary_mod

                property string modTag:  s(mod).toUpperCase()
                property string subLine: mhz(center_freq + offset_freq)
                property string bwLine:  (Math.abs(high_cut - low_cut) / 1000).toFixed(1) + " kHz"
                property string sqlLine: n(squelch_level, -77.5).toFixed(1) + " dB"

                Rectangle {
                    id: card
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height

                    property bool isHover: false
                    property bool isPress: false

                    radius: 18 * u
                    color: "#0E1520"
                    border.color: "#1C2A3D"
                    border.width: Math.max(1, Math.round(1 * u))

                    scale: isPress ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: "#FFFFFF"
                        opacity: card.isHover ? 0.05 : 0.0
                        Behavior on opacity { NumberAnimation { duration: 120 } }
                        z: 1
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: parent.height * 0.45
                        radius: parent.radius
                        color: "#FFFFFF"
                        opacity: 0.04
                        clip: true
                        z: 2
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        z: 3

                        onEntered: card.isHover = true
                        onExited:  { card.isHover = false; card.isPress = false }
                        onPressed: card.isPress = true
                        onReleased: card.isPress = false

                        onPressAndHold: {
                            presetActionDialog.presetId   = profileId
                            presetActionDialog.presetName = presetName
                            presetActionDialog.open()
                        }

                        onClicked: {
                            var setCenterFreq = center_freq

                            var dspcontrolParams = {
                                type: "dspcontrol",
                                params: {
                                    "low_cut": low_cut,
                                    "high_cut": high_cut,
                                    "offset_freq": offset_freq,
                                    "mod": mod,
                                    "dmr_filter": dmr_filter,
                                    "audio_service_id": audio_service_id,
                                    "squelch_level": squelch_level,
                                    "secondary_mod": secondary_mod
                                }
                            }

                            if (mainWindows && typeof mainWindows.sendmessage === "function") {
                                mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":' + setCenterFreq + ',"key":"memagic"}}')

                                pendingDSP        = dspcontrolParams
                                pendingCenterFreq = setCenterFreq
                                pendingUIParams   = dspcontrolParams.params
                                startDSPAfter     = true
                                dspDelayTimer.restart()
                            } else {
                                console.log('[setfrequency]', setCenterFreq, 'mod=', mod)
                            }
                        }
                    }

                    Item {
                        id: content
                        anchors.fill: parent
                        anchors.margins: 18 * u
                        z: 5

                        Rectangle {
                            anchors.top: parent.top
                            anchors.right: parent.right
                            height: 24 * u
                            radius: height / 2
                            color: "#0C2B25"
                            border.color: "#1DE3B7"
                            border.width: Math.max(1, Math.round(1.2 * u))
                            width: Math.max(62 * u, pillText.implicitWidth + 22 * u)

                            Text {
                                id: pillText
                                anchors.centerIn: parent
                                text: modTag
                                color: "#22F3C7"
                                font.pixelSize: Math.round(16 * u)
                                font.bold: true
                                style: Text.Outline
                                styleColor: "#062019"
                            }
                        }

                        Column {
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.rightMargin: 100 * u
                            spacing: 6 * u

                            Text {
                                text: presetName
                                color: cTitle
                                font.pixelSize: Math.round(28 * u)
                                font.bold: true
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                style: Text.Outline
                                styleColor: cOutline
                            }
                        }

                        Rectangle {
                            id: infoBox
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.topMargin: 35 * u
                            height: 122 * u
                            radius: 12 * u
                            color: "#0E1520"
                            border.color: "#122033"
                            border.width: Math.max(1, Math.round(1 * u))

                            Column {
                                anchors.centerIn: parent
                                spacing: 7 * u

                                Row {
                                    spacing: 14 * u
                                    Text { text: "Frequency:"; color: cLabel; font.pixelSize: Math.round(20 * u); font.bold: true; style: Text.Outline; styleColor: cOutline }
                                    Text { text: subLine;      color: cValue; font.pixelSize: Math.round(21 * u); font.bold: true; style: Text.Outline; styleColor: cOutline }
                                    Text { text: "MHz";      color: cValue; font.pixelSize: Math.round(21 * u); font.bold: true; style: Text.Outline; styleColor: cOutline }
                                }

                                Row {
                                    spacing: 14 * u
                                    Text { text: "Bandwidth:"; color: cLabel; font.pixelSize: Math.round(20 * u); font.bold: true; style: Text.Outline; styleColor: cOutline }
                                    Text { text: bwLine;      color: cValue; font.pixelSize: Math.round(21 * u); font.bold: true; style: Text.Outline; styleColor: cOutline }
                                }

                                Row {
                                    spacing: 14 * u
                                    Text { text: "Squelch:"; color: cLabel; font.pixelSize: Math.round(20 * u); font.bold: true; style: Text.Outline; styleColor: cOutline }
                                    Text { text: sqlLine;   color: cValue; font.pixelSize: Math.round(21 * u); font.bold: true; style: Text.Outline; styleColor: cOutline }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /* ===== Dialog: กดค้างที่การ์ด (ของเดิมคุณ) ===== */
    Dialog {
        id: presetActionDialog
        modal: true
        focus: true
        title: ""
        header: null
        footer: null
        standardButtons: Dialog.NoButton

        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        width: 460
        height: 200

        property string presetId: ""
        property string presetName: ""

        property bool modifyMousehovered: modifyMouse.containsMouse
        property bool deleteMousehovered: deleteMouse.containsMouse

        background: Rectangle {
            radius: 24
            color: "#0B1220"
            border.color: "#223049"
            border.width: 1
        }

        contentItem: Item {
            anchors.fill: parent
            anchors.margins: 20

            ColumnLayout {
                anchors.fill: parent
                spacing: 14

                Text {
                    Layout.fillWidth: true
                    text: presetActionDialog.presetName
                    color: "#F1F5F9"
                    font.pixelSize: 20
                    font.bold: true
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#223049"
                    opacity: 0.9
                }

                Item { Layout.fillWidth: true; height: 2 }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: presetActionDialog.modifyMousehovered ? "#0B2A2A" : "transparent"
                            border.color: presetActionDialog.modifyMousehovered ? "#2DD4BF" : "#334155"
                            border.width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "Modify"
                            color: presetActionDialog.modifyMousehovered ? "#ECFEFF" : "#CFFAF4"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: modifyMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                console.log("[Modify]", presetActionDialog.presetId, presetActionDialog.presetName)
                                editPreset(presetActionDialog.presetId, presetActionDialog.presetName)
                                presetActionDialog.close()
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: presetActionDialog.deleteMousehovered ? "#2A1116" : "transparent"
                            border.color: presetActionDialog.deleteMousehovered ? "#FF7A7A" : "#334155"
                            border.width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "Delete"
                            color: presetActionDialog.deleteMousehovered ? "#FF7A7A" : "#F87171"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: deleteMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (configManager && typeof configManager.deletePreset === "function") {
                                    configManager.deletePreset(presetActionDialog.presetId)
                                    configManager.saveToFile("/var/lib/openwebrx/preset.json")

                                    if (typeof configManager.getPresetsAsList === "function"
                                            && radioMemLists && radioMemLists.clear) {

                                        let presets = configManager.getPresetsAsList()
                                        radioMemLists.clear()
                                        for (let i = 0; i < presets.length; i++)
                                            radioMemLists.append(presets[i])

                                        // ✅ หลังลบ/รีเฟรช ให้คง filter เดิมไว้
                                        root.applyFilterToGrid(filterMemoryButton.selectedMode)
                                    }
                                    mainWindows.deleteCardWebSlot(presetActionDialog.presetId)
                                } else {
                                    console.log("[Delete requested]", presetActionDialog.presetId)
                                }
                                presetActionDialog.close()
                            }
                        }
                    }
                }
            }
        }
    }
}
