import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.5 as C2
import QtGraphicalEffects 1.15
import "."

Item {
    id: recordFiles
    width: 1980
    height: 1080

    /* ================== State ================== */
    property date startDT: new Date()
    property date endDT:   new Date(startDT)
    property int  intervalMins: 0
    property string deviceTexte: "1"
    readonly property string fmt: "MM/dd/yyyy, HH:mm:ss"
    property string startText: Qt.formatDateTime(startDT, fmt)
    property string endText:   Qt.formatDateTime(endDT,   fmt)
    property bool enableSearch: false
    property alias logView: logDataFIles
    property bool isDarkTheme: true
    property int  iconSize: 28
    property int  squareButton: 44
    property var  selectedFiles: []
    property int  selectedTotalDurationSec: 0
    property real selectedTotalSizeBytes: 0.0
    property string statusSearchingFromMain: window.statusSearching
    property string statusScanFromMain: statusDeviceScan
    property bool pageRecordFileReady: pageReady
    property int currentSegIndex: 0
    signal waveFilesSelected(var filesArray)
    signal wavePlayToggleRequested(bool wantPlay, var filesArray, bool concatMode, int playPosMs)
    property int heightOfPopUP: 500
    property int extraHeightCustom: 120

    PopUPDeletedFileWave {
        id: popupDeleteWave
        listoFDevice: listoFDevice
        deviceTexte: deviceTexte
        customMode: customMode
        presetDays: presetDays
    }

    onStatusSearchingFromMainChanged: {
        console.log("statusSearchingFromMain changed:", statusSearchingFromMain)

        if (!searchStatusPopup.visible)
            return

        // อัปเดตข้อความใน popup ตามสถานะล่าสุด
        popupStatusText.text = statusSearchingFromMain

        if (statusSearchingFromMain === "Done") {
            popupCloseTimer.start()
        }
    }

    onStatusScanFromMainChanged: {
        console.log("[RecordFiles] statusScanFromMainChanged:", statusScanFromMain)

        if (!scanStatusPopup.visible)
            return

        if (statusScanFromMain === "Done") {
            scanDoneDelayTimer.start()
        } else {
            popupScanText.text = statusScanFromMain
        }
    }

    Timer {
        id: scanPopupCloseTimer
        interval: 1500
        repeat: false
        onTriggered: {
            scanStatusPopup.close()
            popupScanText.text = ""
        }
    }

    Timer {
        id: scanDoneDelayTimer
        interval: 1000   // 0.6 วินาที, จะเอา 1000 ก็ได้
        repeat: false
        onTriggered: {
            popupScanText.text = "Done"
            scanPopupCloseTimer.start()
        }
    }
    function iconSrc(name) {
        function pick(lightFile, darkFile) {
            return isDarkTheme
                    ? ("qrc:/iRecordManage/images/" + lightFile)
                    : ("qrc:/iRecordManage/images/" + darkFile)
        }
        if (name === "refresh") {
            return pick("refresh_light.png", "refresh_dark.png")
        }
        if (name === "calendar") {
            return pick("calendarDarkMode.png", "calendarlightMode.png")
        }
        return ""
    }


    function addMinutes(d, mins) { var t = new Date(d); t.setMinutes(t.getMinutes() + mins); return t }
    function applyInterval() {
        endDT  = (intervalMins === 0) ? new Date(startDT) : addMinutes(startDT, intervalMins)
        endText = Qt.formatDateTime(endDT, fmt)
    }

    /* ================== Helpers ================== */

    function buildFullPathFromFilename(filename, baseDir) {

        var re = /^([^_]+)_(\d{8})_.*\.wav$/;   // group1=device, group2=YYYYMMDD
        var m = (filename||"").match(re);
        if (!m) return "";
        var device = m[1];
        var ymd    = m[2];
        return (baseDir + "/" + device + "/" + ymd + "/" + filename).replace(/\/+/g, "/");
    }

    function toFileUrl(absPath) {
        if (!absPath) return "";
        if (absPath.indexOf("file:") === 0) return absPath;
        return "file:" + absPath.replace(/\/+/g, "/");
    }

    function getSelectedFilePaths() {
        var arr = [];
        var baseDir = "/var/ivoicex";

        for (var i = 0; i < listFileRecord.count; ++i) {
            var r = listFileRecord.get(i);
            if (!r || !r.selected) continue;

            var p = "";
            if (r.full_path && r.full_path.length) {
                p = r.full_path;
            } else if ((r.file_path && r.file_path.length) && (r.filename && r.filename.length)) {

                var built = buildFullPathFromFilename(r.filename, r.file_path);
                p = built || ((r.file_path + "/" + r.filename).replace(/\/+/g, "/"));
            } else if (r.filename && r.filename.length) {
                p = buildFullPathFromFilename(r.filename, baseDir);
            }

            if (p && p.length) arr.push(p);
        }
        return arr;
    }

    /* ================== Background ================== */
    Rectangle {
        anchors.fill: parent
        color: "#23404d" //"#1f2428"
        LogDataFIles {
            id: logDataFIles
            anchors.fill: parent
            anchors.rightMargin: 90
            anchors.leftMargin: 28
            anchors.bottomMargin: 274
            anchors.topMargin: 188

        }
        function uncheckAllChecks() {
            logDataFIles.uncheckAll()
        }

        WaveEditor {
            id: editor
            y: 810
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 66
            height: 270
            onPlayToggleRequested: {
                console.log("[RecordFiles] playToggleRequested wantPlay=", wantPlay,
                            "concatMode=", concatMode, "filesArray.length=",
                            filesArray ? filesArray.length : 0)

                recordFiles.wavePlayToggleRequested(
                            wantPlay,
                            filesArray,
                            concatMode,
                            playPosMs)
            }
        }
    }

    Popup {
        id: scanStatusPopup
        modal: false
        focus: false
        width: 260
        height: 50

        background: Rectangle {
            radius: 8
            color: "#1f2633"
            border.color: "#3a4757"
        }

        Text {
            id: popupScanText
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 20
            text: ""
        }
    }


    Popup {
        id: searchStatusPopup
        modal: false
        focus: false
        x: buttonSearch.x + buttonSearch.width + 10
        y: buttonSearch.y - 5
        width: 260
        height: 50

        background: Rectangle {
            radius: 8
            color: "#1f2633"
            border.color: "#3a4757"
        }

        Text {
            id: popupStatusText
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 20
            text: ""
        }
    }

    Timer {
        id: popupCloseTimer
        interval: 1500
        repeat: false
        onTriggered: {
            searchStatusPopup.close()
            popupStatusText.text = ""
            window.statusSearching = ""
        }
    }


    Timer {
        id: searchDelayTimer
        interval: 3000
        repeat: false
        onTriggered: sendSearch()
    }
    // ======= พื้นที่แสดง Waveform ด้านล่าง =======

    ColumnLayout {
        height: 162
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        anchors.rightMargin: 8
        anchors.leftMargin: 24
        anchors.topMargin: -1
        spacing: 2

        RowLayout {
            spacing: 20
            Layout.fillWidth: true

            // -------- Device ----------


            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Label { text: "Device"; color: "#b7c0ca" }

                ComboBox {
                    id: deviceNumBox
                    property var sourceModel: listoFDevice
                    property var ids: []
                    model: ids
                    font.pixelSize: 18
                    Layout.preferredHeight: 55
                    implicitWidth: 320
                    Layout.preferredWidth: 320
                    background: Rectangle { radius: 6; color: "#0e1116"; border.color: "#2a2f37" }

                    function rebuild() {
                        const out = []
                        if (sourceModel && sourceModel.count > 0) {
                            for (var i = 0; i < sourceModel.count; ++i) {
                                const it = sourceModel.get(i)
                                if (!it || it.idDevice === undefined) continue
                                const s = String(it.idDevice)
                                if (out.indexOf(s) === -1) out.push(s)
                            }
                            out.sort(function(a,b){ return Number(a) - Number(b) })
                        } else {
                            for (var k = 1; k <= 24; ++k) out.push(String(k))
                        }
                        ids = out

                        const wanted = String(deviceTexte || (ids[0] || "1"))
                        const idx = ids.indexOf(wanted)
                        currentIndex = (idx >= 0) ? idx : 0
                    }

                    Component.onCompleted: rebuild()
                    onActivated: deviceTexte = currentText
                    onCurrentIndexChanged: if (currentIndex >= 0 && currentIndex < ids.length)
                                               deviceTexte = ids[currentIndex]
                }

                Connections {
                    target: window
                    function onDeviceListUpdated() { deviceNumBox.rebuild() }
                }
            }

            // -------- Start Date/Time ----------

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Label { text: "Start Date/Time"; color: "#b7c0ca" }
                RowLayout {
                    spacing: 8
                    TextField {
                        id: tfStart
                        text: startText
                        readOnly: true
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                        background: Rectangle { radius: 6; color: "#0e1116"; border.color: "#2a2f37" }
                        implicitWidth: 320
                        Layout.preferredWidth: 320
                        onPressed: calendarOverlay.openFor("start")
                    }
                    Item {
                        implicitWidth: 40; implicitHeight: 36
                        Layout.preferredWidth: 40; Layout.preferredHeight: 36

                        Image {
                            anchors.centerIn: parent
                            source: iconSrc("calendar")
                            fillMode: Image.PreserveAspectFit
                            width: 50; height: 50
                            mipmap: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: calendarOverlay.openFor("start")
                        }
                    }

                }
            }

            // -------- Interval ----------

            ColumnLayout {
                spacing: 6
                Label { text: "Interval (minutes)"; color: "#b7c0ca" }
                ComboBox {
                    id: cbInterval
                    model: ["Same Time", "+5 minutes", "+10 minutes", "+15 minutes", "+30 minutes", "+60 minutes", "Custom..."]
                    currentIndex: 0
                    implicitWidth: 220
                    font.pixelSize: 18
                    Layout.preferredHeight: 55
                    background: Rectangle { radius: 6; color: "#0e1116"; border.color: "#2a2f37" }

                    onActivated: function(i){
                        switch (i) {
                        case 0: intervalMins = 0; break;
                        case 1: intervalMins = 5; break;
                        case 2: intervalMins = 10; break;
                        case 3: intervalMins = 15; break;
                        case 4: intervalMins = 30; break;
                        case 5: intervalMins = 60; break;
                        case 6: customIntervalPopup.open(); return;
                        }
                        applyInterval();
                    }
                }
            }

            // -------- End Date/Time ----------

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Label { text: "End Date/Time"; color: "#b7c0ca" }
                TextField {
                    id: tfEnd
                    text: endText
                    readOnly: true
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle { radius: 6; color: "#0e1116"; border.color: "#2a2f37" }
                    implicitWidth: 320
                    Layout.preferredWidth: 320
                }
            }

            // -------- Tools (Combo + Export + Scan) ----------

            ColumnLayout {
                id: toolsColumn
                spacing: 4
                Layout.alignment: Qt.AlignTop | Qt.AlignRight
                Label { text: "Scan Device"; color: "#b7c0ca" }

                RowLayout {
                    spacing: 8

                    ComboBox {
                        id: comboExportTarget
                        Layout.preferredWidth: 320

                        model: exportDeviceList      // <= ใช้ property local
                        textRole: "text"

                        onModelChanged: {
                            if (!model || model.length === undefined || model.length === 0) {
                                currentIndex = -1
                                selectedExportMountPoint = ""
                                selectedExportDevPath   = ""
                                console.log("[comboExportTarget] cleared (empty model)")
                                return
                            }

                            currentIndex = 0
                            var item = model[0]
                            if (item) {
                                selectedExportMountPoint = item.mountPoint || ""
                                selectedExportDevPath   = item.devPath   || ""
                                console.log("[comboExportTarget] auto-select", selectedExportMountPoint)
                            } else {
                                selectedExportMountPoint = ""
                                selectedExportDevPath   = ""
                            }
                        }

                        onActivated: function(i) {
                            if (!model || i < 0 || i >= model.length) return
                            var item = model[i]
                            selectedExportMountPoint = (item && item.mountPoint) ? item.mountPoint : ""
                            selectedExportDevPath   = (item && item.devPath)   ? item.devPath   : ""
                            console.log("[comboExportTarget] selected:", selectedExportMountPoint)
                        }
                    }

                    Button {
                        id: scanButton
                        text: qsTr("Scan")
                        Layout.preferredWidth: 100

                        onClicked: {
                            // ตั้งข้อความไว้ก่อนเลย
                            popupScanText.text = statusDeviceScan
                            window.statusScan = "Scanning..."

                            // ให้ popup ขึ้นตรงข้าง ๆ Export File
                            var p = exportButton.mapToItem(recordFiles, 0, 0)
                            scanStatusPopup.x = p.x + exportButton.width + 10
                            scanStatusPopup.y = p.y + (exportButton.height - scanStatusPopup.height) / 2
                            scanStatusPopup.open()

                            // ค่อยยิงคำสั่งไปหา C++
                            var msg = { menuID: "scanDeivce" }
                            qmlCommand(JSON.stringify(msg))
                        }
                    }



                    Button {
                        id: unmountButton
                        text: qsTr("Unmount")
                        Layout.preferredWidth: 100

                        onClicked: {
                            var msg = {
                                menuID: "unmountDeivce"
                            }
                            qmlCommand(JSON.stringify(msg))
                        }
                    }
                }
                Button {
                    id: exportButton
                    text: qsTr("Export File")
                    Layout.preferredWidth: comboExportTarget.implicitWidth

                    onClicked: {
                        var items = collectSelectedFiles()
                        if (items.length === 0) {
                            console.log("[Export] no file selected")
                            return
                        }

                        var now = new Date()
                        var defName = Qt.formatDateTime(now, "yyyyMMdd_hhmmss")

                        // แค่เตือน แต่ "ไม่ return"
                        if (!window.label || window.label === "") {
                            console.log("[Export] no USB target selected, open popup anyway")
                        } else {
                            console.log("[Export] use mountPoint:", window.label)
                        }
                        pathToSave = window.label
                        console.log("[Export] total size =", selectedTotalSizeBytes,
                                    "total dur_sec =", selectedTotalDurationSec)

                        // ถ้าไม่มี mountPoint ก็ส่ง "" ไปก่อน
                        var mp = window.selectedExportMountPoint || ""
                        exportOverlay.openFor(items, mp, defName)
                    }
                }
            }
        }

        RowLayout {
            // height: 80                   // <-- ลบออก
            Layout.fillHeight: true
            Layout.fillWidth: false          // <-- ให้เต็มความกว้างเหมือนแถวบน
            Layout.alignment: Qt.AlignLeft  // ให้ชิดซ้ายใต้ deviceNumBox
            spacing: 8                      // ช่องไฟระหว่างปุ่ม

            Button {
                id: buttonSearch
                text: "Search"
                background: Rectangle { radius: 6; color: "#1f8d4d" }
                onClicked:{
                    popupSearching()    // <-- เปลี่ยนมาเรียกฟังก์ชันนี้
                    clearSelections()
                }
            }

            Button {
                id: buttonClear
                text: "Clear"
                Layout.fillHeight: false
                background: Rectangle { radius: 6; color: "#727b87" }
                onClicked: {
                    freezeRecordFilesUpdate = false
                    clearSelections()
                    resetFiltersAndReload()
                }
            }
            Button {
                id: buttonDeletedFiles
                x: 432
                y: -71
                text: "Deleted Files"
                background: Rectangle { radius: 6; color: "#ff004c" }

                onClicked: {
                    popupDeleteWave.customMode = false
                    popupDeleteWave.presetDays = 1
                    popupDeleteWave.open()
                }

            }
            ToolButton {
                id: btnRefresh
                width: squareButton; height: squareButton
                Layout.fillHeight: true
                Layout.fillWidth: false

                background: Rectangle {
                    color: "transparent"
                    border.color: "transparent"
                }
                onClicked: {
                    var msg = { menuID: "refreshpage" }
                    qmlCommand(JSON.stringify(msg))

                }
                contentItem: Image {
                    anchors.fill: parent
                    width: iconSize; height: iconSize
                    source: iconSrc("refresh")
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }
            }


        }

        // ----- RowLayout ล่าง (Search / Clear / Select Files) ของคุณวางต่อได้เลย -----
    }

    /* ================== Calendar Overlay ================== */
    Rectangle {
        id: calendarOverlay
        anchors.fill: parent
        color: "#00000088"
        visible: false
        z: 9999
        property string target: "start"

        function openFor(which) { target = which || "start"; visible = true; }
        function close() { visible = false }

        MouseArea { anchors.fill: parent; onClicked: calendarOverlay.close() }

        Rectangle {
            id: panel
            width: 1000; height: 500; radius: 12
            color: "#0e1116"; border.color: "#ffffff"
            anchors.centerIn: parent

            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; preventStealing: true }

            Loader {
                id: picker
                anchors.fill: parent
                source: "CalendarPopup.qml"
                active: calendarOverlay.visible

                onLoaded: {
                    if (!item) return;
                    var dt = (calendarOverlay.target === "start") ? startDT : endDT;
                    if (item.initialDate !== undefined) item.initialDate = dt;

                    if (item.accepted) item.accepted.connect(function(ymdHmsStr){
                        var d = Date.fromLocaleString(Qt.locale(), ymdHmsStr, "yyyy/MM/dd HH:mm:ss");
                        if (!isNaN(d)) {
                            if (calendarOverlay.target === "start") {
                                startDT   = d;
                                startText = Qt.formatDateTime(startDT, fmt);
                                applyInterval();
                            } else {
                                endDT   = d;
                                endText = Qt.formatDateTime(endDT, fmt);
                            }
                        }
                        calendarOverlay.close();
                    });
                    if (item.canceled) item.canceled.connect(function(){ calendarOverlay.close() });
                }

                onStatusChanged: {
                    if (status === Loader.Ready && calendarOverlay.visible && item) {
                        var dt = (calendarOverlay.target === "start") ? startDT : endDT;
                        if (item.initialDate !== undefined) item.initialDate = dt;
                    }
                }
            }
        }
    }

    Popup {
        id: customIntervalPopup
        modal: true; focus: true
        width: 320; height: 160
        x: (parent.width - width)/2
        y: (parent.height - height)/2
        background: Rectangle { radius: 10; color: "#161a20"; border.color: "#2a2f37" }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Label { text: "Custom interval (minutes)"; color: "#b7c0ca" }
            SpinBox { id: sbCustom; from: 1; to: 24*60; value: 5; Layout.preferredWidth: 140 }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Item { Layout.fillWidth: true }
                Button { text: "Cancel"; onClicked: customIntervalPopup.close() }
                Button {
                    text: "OK"
                    onClicked: {
                        intervalMins = sbCustom.value
                        applyInterval()
                        customIntervalPopup.close()
                    }
                }
            }
        }
    }

    // ===================== Export Overlay =====================
    Rectangle {
        id: exportOverlay
        anchors.fill: parent
        color: "#00000088"
        visible: false
        z: 9999

        // ข้อมูลที่จะส่งเข้า popup export
        property var    exportFiles: []
        property string exportMountPoint: ""
        property string exportDefaultName: ""

        function openFor(files, mountPoint, defName) {
            exportFiles       = files || []
            exportMountPoint  = mountPoint || ""
            exportDefaultName = defName || ""
            visible           = true
        }
        function close() { visible = false }

        MouseArea { anchors.fill: parent; onClicked: exportOverlay.close() }

        Rectangle {
            id: exportPanel
            width: 1000
            height: 500
            radius: 12
            color: "#0e1116"
            border.color: "#ffffff"
            anchors.centerIn: parent

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
                preventStealing: true
            }

            Loader {
                id: exportLoader
                anchors.fill: parent
                source: "ExportFilesRecord.qml"
                active: exportOverlay.visible

                onLoaded: {
                    if (!item) return

                    if (item.files        !== undefined) item.files        = exportOverlay.exportFiles
                    if (item.mountPoint   !== undefined) item.mountPoint   = exportOverlay.exportMountPoint
                    if (item.defaultName  !== undefined) item.defaultName  = exportOverlay.exportDefaultName
                    if (item.qmlCommandFn !== undefined) item.qmlCommandFn = window.qmlCommand

                    // 🔹 รีเซ็ต progress ทุกครั้งที่ popup ถูกสร้างใหม่
                    if (item.resetState) {
                        item.resetState()
                    } else {
                        if (item.progress   !== undefined) item.progress   = 0
                        if (item.statusText !== undefined) item.statusText = ""
                        if (item.exporting  !== undefined) item.exporting  = false
                    }

                    if (item.requestClose) {
                        item.requestClose.connect(function() {
                            exportOverlay.close()
                        })
                    }
                }
            }

        }

    }

    /* ================== Search / Reset ================== */
    function popupSearching() {
        recordListFrozen = true
        window.statusSearching = ""
        popupStatusText.text = "Searching files ............."
        var p = btnRefresh.mapToItem(recordFiles, 0, 0)
        searchStatusPopup.x = p.x + btnRefresh.width + 10
        searchStatusPopup.y = p.y + (btnRefresh.height - searchStatusPopup.height) / 2
        searchStatusPopup.open()
        searchDelayTimer.start()
    }

    function sendSearch() {
        var deviceStr = (typeof deviceNumBox !== "undefined" && deviceNumBox)
                ? deviceNumBox.currentText
                : "";

        var s = new Date(startDT);
        var e = new Date(endDT);
        if (s > e) { var tmp = s; s = e; e = tmp }

        var payload = {
            menuID: "searchRecordFiles",
            device: deviceStr,
            startDate: tfStart.text,
            endDate: tfEnd.text,
            startISO: Qt.formatDateTime(s, "yyyy-MM-ddTHH:mm:ss"),
            endISO:   Qt.formatDateTime(e, "yyyy-MM-ddTHH:mm:ss"),
            interval: intervalMins.toString(),
            page: 1,
            pageSize: 25
        };
        if (typeof qmlCommand === "function") qmlCommand(JSON.stringify(payload));
        console.log("Search payload:", JSON.stringify(payload));
        enableSearch = true
    }

    function resetFiltersAndReload() {
        console.log("resetFiltersAndReload")
        recordListFrozen = false
        startDT = new Date();
        startText = Qt.formatDateTime(startDT, fmt);
        cbInterval.currentIndex = 0;
        intervalMins = 0;
        applyInterval();

        window.clearSelections()
        Qt.callLater(function(){ logDataFIles.uncheckAll() })
        if (listFileRecord) listFileRecord.clear()
        editor.clearWaveform()

        var payload = { menuID: "getRecordFiles", page: 1, pageSize: 25 };
        if (typeof qmlCommand === "function") qmlCommand(JSON.stringify(payload));
        enableSearch = false
    }

    function collectSelectedFiles() {
        var items = []
        var seen = {}
        var totalDur = 0
        var totalSize = 0

        for (var i = 0; i < listFileRecord.count; ++i) {
            var r = listFileRecord.get(i)
            if (!r || !r.selected) continue

            var path = ""
            if (r.full_path && r.full_path.length) {
                path = r.full_path
            } else if ((r.file_path && r.file_path.length) && (r.filename && r.filename.length)) {
                var built = buildFullPathFromFilename(r.filename, r.file_path)
                path = built || ((r.file_path + "/" + r.filename).replace(/\/+/g, "/"))
            } else if (r.filename && r.filename.length) {
                path = buildFullPathFromFilename(r.filename, "/var/ivoicex")
            }

            if (!path || !path.length) continue
            if (seen[path]) continue
            seen[path] = true

            var sz = Number(r.size)
            var dur = Number(r.duration_sec)

            if (!isNaN(dur)) totalDur += dur
            if (!isNaN(sz))  totalSize += sz

            items.push({
                           full_path: path,
                           size: isNaN(sz) ? undefined : sz,
                           duration_sec: isNaN(dur) ? undefined : dur
                       })
        }

        selectedFiles = items
        selectedTotalDurationSec = Math.floor(totalDur)
        selectedTotalSizeBytes   = totalSize

        return items
    }

    Connections {
        target: Backend

        function onExportProgress(percent, status) {
            console.log("[RecordFiles] exportProgress", percent, status)
            if (exportLoader.status === Loader.Ready &&
                    exportLoader.item && exportLoader.item.updateProgress) {
                exportLoader.item.updateProgress(percent, status)
            }
        }

        function onExportFinished(ok, outPath, error) {
            console.log("[RecordFiles] exportFinished", ok, outPath, error)
            if (exportLoader.status === Loader.Ready &&
                    exportLoader.item && exportLoader.item.updateProgress) {
                exportLoader.item.updateProgress(
                            ok ? 100 : 0,
                            ok ? ("Saved: " + outPath) : ("Error: " + error)
                            )
            }
        }
    }



    Component.onCompleted: {
        console.log("[RecordFiles] Component.onCompleted -> pageReady = true")
        pageReady = true
        Backend.recordFilesPageActive = true

        if (freezeRecordFilesUpdate) {
            console.log("[RecordFiles] freezeRecordFilesUpdate=true -> restoreSelectionFromTxtAndSyncModel()")
            restoreSelectionFromTxtAndSyncModel()
        }
    }
    onVisibleChanged: {
        console.log("[RecordFiles] visible =", visible)
        Backend.recordFilesPageActive = visible
    }

}

/*##^##
Designer {
    D{i:0;formeditorZoom:0.5}
}
##^##*/
