import QtQuick 2.15
import QtQuick.Window 2.12
import QtQuick.VirtualKeyboard 2.4
import QtQuick.Controls 2.15
import QtQuick3D.Effects 1.15
import QtQuick.Layouts 1.3
import QtWebSockets 1.0
import QtGraphicalEffects 1.0
import QtQuick.VirtualKeyboard 2.15
import QtQuick.VirtualKeyboard.Styles 2.15
import QtQuick.VirtualKeyboard.Settings 2.15
import QtQuick.Extras 1.4
import QtCharts 2.2
import Qt.labs.settings 1.0
import QtMultimedia 5.12
Window {
    id: window
    width: 1920
    height: 1080
    visible: true
    color: "#23404d"
    property string serverAddress: "127.0.0.1"
    property var socketPort: 0
    property string socketUrl: "ws://"+ serverAddress + ":" + socketPort.toString()
    property bool sockStart: false
    property var objectName: []
    property var socketConnectToCpp:""
    signal qmlCommand(string msg)
    property var currentDate:""
    property var currentTime:""
    property var currentPage:""
    property var totalPages:""
    property real screenrotation: 0
    property var rowById: ({})
    property var selectedRows: []
    property var selectedItems: []
    property var selectedMap:   ({})
    property bool recordListFrozen: false
    property bool freezeRecordFilesUpdate: false
    property var exportDeviceList: []
    property string selectedExportMountPoint: ""
    property string selectedExportDevPath: ""
    property var label: ""
    property string statusSearching: ""
    property string statusSearchingDone: ""
    property string  statusScan: ""
    property bool waitingDoneStatus: false
    property string statusDeviceScan: ""
    property real currentVolumeLevel:0
    property real convertCurrentVolumeLevel:0
    property bool pageReady: false
    signal deviceListUpdated()


    // playlist ที่มาจาก WaveEditor / RecordFiles
    property var waveEditorFiles: []     // array ของ path (string)
    property bool waveEditorConcatMode: false
    property int waveEditorIndex: -1     // index ไฟล์ที่กำลังเล่นอยู่
    property string currentWaveFile: ""
    property string waveEditorFirstFile: ""
    property real playerVolume: 1.0

//    ListModel {
//        id: exportDeviceList
//    }
    ListModel {
        id: listFileRecord
        property string idDevice: ""
        property string device: ""
        property string filename : ""
        property string created_at: ""
        property string continuous_count: ""
        property string file_path: ""
        property string name: ""
        property string size: ""
        property string duration_sec: ""
        property string parsed_date: ""
        property bool selected: false
    }

    ListModel {
        id: listoFDevice
        property string ambient: ""
        property string chunk: ""
        property string file_path : ""
        property string freq: ""
        property string group: ""
        property string idDevice: ""
        property string ip: ""
        property string last_access: ""
        property string name: ""
        property string payload_size: ""
        property string sid: ""
        property string terminal_type: ""
        property string updated_at: ""
        property string uri: ""
        property string visible: ""
        property bool selected: false
    }

    function qmlSubmitTextFiled(message) {
        console.log("qmlSubmitTextFiled:", message)
        var JsonObject = JSON.parse(message);
        var objectName = JsonObject.objectName;
        var TrapsAlert = JsonObject.TrapsAlert;
        var menuID = JsonObject.menuID;
        var eventRecord = JsonObject.eventRecord;
        var obj = (typeof message === "string") ? JSON.parse(message) : message;

        if (obj && obj.menuID === "DateTime" && typeof obj.formattedDateTime === "string") {
            var onlyDate = obj.formattedDateTime.split(" ")[0];
            var onlyTime = obj.formattedDateTime.split(" ")[1];

            currentDate = onlyDate;
            currentTime = onlyTime;

        } else if(objectName === "recordFilesChunk"){
            console.log("QML_recordFilesChunk:", message);
            handleRecordFilesChunk(message)
        }else if(objectName === "searchRecordFilesResult"){
            console.log("QML_searchRecordFilesResult:", message);
            handleRecordFilesChunk(message)
        }else if (objectName === "waveformPeaks") {
            if (typeof editor !== "undefined" && editor.setPeaks) {
                editor.setPeaks(JsonObject.index, JsonObject.peaks, JsonObject.duration)
            } else {
                console.log("WaveEditor not ready for peaks")
            }
        } else if (objectName === "waveformError") {
            console.log("Waveform error:", JsonObject.message)
        }else if (menuID === "deviceList") {
            console.log("deviceList:", message)
            handleDeviceUpdated(message)
        }else if (menuID === "statusScanDevice" || menuID === "scanDeivce") {
            statusDeviceScan = JsonObject.status
            console.log("statusScanDevice:", message, "-> statusDeviceScan =", statusDeviceScan,JsonObject.status)

        }else if (menuID === "scanDeivceResult") {
            console.log("scanDeivceResult:", message)
            handleScanDeviceResult(message)
        }else if (menuID === "unmountDeivceResult") {
            console.log("unmountDeivceResult:", message)
            var o = JSON.parse(message)
            if (o.ok === true) {
                exportDeviceList = []   // เคลียร์ model กลาง
                console.log("[unmountDeivceResult] cleared exportDeviceList")
            } else {
                console.warn("[unmountDeivceResult] unmount failed")
            }
        }else if (menuID === "statusSearchFiles") {
            statusSearching = obj.status || ""
            console.log("statusSearchFiles:", message, statusSearching)
        }else if (menuID === "updateRecordVolume") {
            console.log("updateRecordVolume:", message,"currentVolume:",JsonObject.currentVolume)
            currentVolumeLevel = JsonObject.currentVolume
            convertCurrentVolumeLevel = currentVolumeLevel / 100.0
        }else if (menuID === "recordFilesUpdate") {
            console.log("recordFilesUpdate:", message)
//            handleRecordFilesChunk(message)
            handleRecordFilesUpdate(message)
        }

    }

    Timer {
        id: timerConnectSocket
        interval: 2000
        running: false
        repeat: false
        onTriggered: {
            qmlCommand(socketConnectToCpp)
            console.log("connect_to_cpp:");
            var getRecordFiles = '{"menuID":"getRecordFiles"}'
            console.log("getRecordFiles:",getRecordFiles)
            qmlCommand(getRecordFiles);

            var getRegisterDevicePage = '{"menuID":"getRegisterDevicePage"}'
            qmlCommand(getRegisterDevicePage)
        }
    }

    WebSocket {
        id: socketCPP
        url: "ws://127.0.0.1:"+socketPort+""
        property bool checkuser: true

        onTextMessageReceived: {
            qmlSubmitTextFiled(message)
            if (message != ""){
                console.log("onTextMessageReceived",message);
            }
        }
        onStatusChanged: if (socketCPP.status == WebSocket.Error) {
                             console.log("socketCPP Error: " + socketCPP.errorString)
                             WebSocket.Open
                         }
                         else if (socketCPP.status == WebSocket.Open)
                         {
                             let hasSentTaggingCommands = false;
                             console.log("socketCPP Hello World")
                             if (socketCPP.status == WebSocket.Open && !hasSentTaggingCommands) {
                             }

                         }
                         else if (socketCPP.status == WebSocket.Closed)
                         {
                             console.log("Socket closed")

                         }
        active: false
    }

    KeyboardInput {
        id: inputKey
        anchors.fill: parent
        visible: Qt.inputMethod.visible
        y: rotation==270 ? 400+height : 1080+height
    }


    TapBarRecordFiles {
        id: tabBarRecord
        anchors.fill: parent

        // RecordFiles → TapBarRecordFiles → main
        onWavePlayToggleRequested: {
            console.log("[main] wavePlayToggleRequested wantPlay=", wantPlay,
                        "concatMode=", concatMode,
                        "filesArray.length=", filesArray ? filesArray.length : 0)

            window.waveEditorConcatMode = concatMode
            window.waveEditorFiles      = filesArray || []

            if (wantPlay) {
                if (window.waveEditorFiles.length > 0) {
                    window.waveEditorIndex = 0

                    var p = window.waveEditorFiles[0]
                    var url = (p.indexOf("file://") === 0) ? p : ("file://" + p)

                    console.log("[main] start playlist, index=0 url=", url)

                    playerlog.stop()
                    playerlog.source = url
                    playerlog.play()
                } else {
                    console.warn("[main] wantPlay but filesArray is empty")
                }
            } else {
                console.log("[main] pause request from WaveEditor")
                playerlog.pause()
            }
        }
    }

//    TapBarRecordFiles {
//        id: tapBarRecordFiles
//        anchors.fill: parent
//    }

    function handleRecordFilesUpdate(msg) {
        console.log("handleRecordFilesUpdate:", msg)

        if (freezeRecordFilesUpdate) {
            console.log("handleRecordFilesUpdate: FREEZED (skip update while selecting files)")
            return
        }
        // ถ้าอยู่ในโหมด freeze (กำลังดูผล filter) → ไม่ให้รายการเลื่อน
        if (recordListFrozen) {
            console.log("[handleRecordFilesUpdate] ignored because recordListFrozen = true")
            return
        }

        var obj = (typeof msg === "string") ? JSON.parse(msg) : msg

        // รองรับทั้ง objectName / menuID = "recordFilesUpdate"
        if (!obj ||
            (obj.objectName !== "recordFilesUpdate" && obj.menuID !== "recordFilesUpdate") ||
            !obj.records)
            return

        function findIndexByKey(key) {
            for (var i = 0; i < listFileRecord.count; ++i) {
                var item = listFileRecord.get(i)
                var itemKey = recordKeyFromData(
                                  String(item.device),
                                  String(item.filename),
                                  String(item.created_at),
                                  String(item.full_path)
                              )
                if (itemKey === key)
                    return i
            }
            return -1
        }

        // records ใหม่ (ส่วนใหญ่จะเป็นชุดไฟล์ล่าสุด)
        for (var i = obj.records.length - 1; i >= 0; --i) {
            var r = obj.records[i]

            var created_at  = (r.created_at || "").toString()
            var parsed_date = (created_at.indexOf("T") !== -1)
                              ? created_at.split("T")[0]
                              : ""

            var dev   = String(r.device || r.id || "")
            var fname = String(r.filename || "")
            var fullp = String(r.full_path || r.file_path || "")

            var key = recordKeyFromData(dev, fname, created_at, fullp)

            var wasSelected = !!selectedMap[key]
            var idx = findIndexByKey(key)

            // ==== ขนาดไฟล์ ====
            var sizeBytes = Number(r.size_bytes !== undefined ? r.size_bytes : 0)
            if (!isFinite(sizeBytes) || sizeBytes < 0)
                sizeBytes = 0


            var sizeKB = (sizeBytes / 1024)
            sizeKB = parseFloat(sizeKB.toFixed(2))
            // เวลา (วินาที)
            var durSec = Number(r.duration_sec !== undefined ? r.duration_sec : 0)
            if (!isFinite(durSec) || durSec < 0)
                durSec = 0

            var rowData = {
                idDevice:          String(r.id || ""),
                device:            dev,
                filename:          fname,
                created_at:        created_at,
                continuous_count:  String(r.continuous_count || ""),
                file_path:         String(r.file_path || ""),
                full_path:         fullp,
                name:              String(r.name || ""),
                parsed_date:       String(parsed_date),

                size_bytes:        sizeBytes,              // bytes (numeric)
                sizeKB:            sizeKB,                 // KB (numeric)
                size:              sizeKB,
                size_str:          String(r.size_human || ""),
                duration_sec:      durSec,

                selected:          wasSelected
            }

            if (idx >= 0) {
                // อัปเดตแถวเดิม
                listFileRecord.set(idx, rowData)
            } else {
                // แทรกแถวใหม่ด้านบนสุด (ไฟล์ใหม่สุดขึ้นบน)
                listFileRecord.insert(0, rowData)
            }
        }
    }


//    function handleRecordFilesUpdate(msg) {
//        console.log("handleRecordFilesUpdate:", msg)

//        var obj = (typeof msg === "string") ? JSON.parse(msg) : msg

//        // รองรับทั้ง objectName / menuID = "recordFilesUpdate"
//        if (!obj ||
//            (obj.objectName !== "recordFilesUpdate" && obj.menuID !== "recordFilesUpdate") ||
//            !obj.records)
//            return

//        // มี selection อยู่ไหม?  ถ้ามี -> ไฟล์ใหม่ให้ไปต่อท้าย (append)
//        var hasSelection = (selectedItems && selectedItems.length > 0)

//        function findIndexByKey(key) {
//            for (var i = 0; i < listFileRecord.count; ++i) {
//                var item = listFileRecord.get(i)
//                var itemKey = recordKeyFromData(
//                                  String(item.device),
//                                  String(item.filename),
//                                  String(item.created_at),
//                                  String(item.full_path)
//                              )
//                if (itemKey === key)
//                    return i
//            }
//            return -1
//        }

//        // records ใหม่ (ส่วนใหญ่จะเป็นชุดไฟล์ล่าสุด)
//        for (var i = obj.records.length - 1; i >= 0; --i) {
//            var r = obj.records[i]

//            var created_at  = (r.created_at || "").toString()
//            var parsed_date = (created_at.indexOf("T") !== -1)
//                              ? created_at.split("T")[0]
//                              : ""

//            var dev   = String(r.device || r.id || "")
//            var fname = String(r.filename || "")
//            var fullp = String(r.full_path || r.file_path || "")

//            var key = recordKeyFromData(dev, fname, created_at, fullp)

//            var wasSelected = !!selectedMap[key]
//            var idx = findIndexByKey(key)

//            // ==== ขนาดไฟล์ ====
//            var sizeBytes = Number(r.size_bytes !== undefined ? r.size_bytes : 0)
//            if (!isFinite(sizeBytes) || sizeBytes < 0)
//                sizeBytes = 0

//            var sizeKB = sizeBytes > 0 ? (sizeBytes / 1024.0) : 0
//            sizeKB = parseFloat(sizeKB.toFixed(2))

//            // เวลา (วินาที)
//            var durSec = Number(r.duration_sec !== undefined ? r.duration_sec : 0)
//            if (!isFinite(durSec) || durSec < 0)
//                durSec = 0

//            var rowData = {
//                idDevice:          String(r.id || ""),
//                device:            dev,
//                filename:          fname,
//                created_at:        created_at,
//                continuous_count:  String(r.continuous_count || ""),
//                file_path:         String(r.file_path || ""),
//                full_path:         fullp,
//                name:              String(r.name || ""),
//                parsed_date:       String(parsed_date),

//                // ขนาดไฟล์
//                size_bytes:        sizeBytes,                 // bytes
//                sizeKB:            sizeKB,                    // KB (numeric)
//                size:              sizeKB,                    // role หลัก (numeric KB)
//                size_str:          String(r.size_human || ""),// string สำหรับโชว์

//                // ระยะเวลา
//                duration_sec:      durSec,                    // numeric (sec)

//                selected:          wasSelected
//            }

//            if (idx >= 0) {
//                // อัปเดตแถวเดิม
//                listFileRecord.set(idx, rowData)
//            } else {
//                // ถ้ามี selection อยู่ -> ไฟล์ใหม่ไปต่อท้าย
//                // ถ้าไม่มี selection -> แทรกบนสุดเหมือนเดิม
//                var insertIndex = hasSelection ? listFileRecord.count : 0
//                listFileRecord.insert(insertIndex, rowData)
//            }
//        }
//    }

    function clearSelections() {
        console.log("clearSelections before -> rows:", selectedRows.length,
                    "items:", selectedItems.length)

        // ล้าง array ฝั่ง selection
        selectedRows  = []
        selectedItems = []
        selectedMap   = {}    // ล้าง map ด้วย เวลาอยากเริ่มใหม่จริง ๆ

        // เคลียร์ role "selected" ใน model
        if (listFileRecord && listFileRecord.count > 0) {
            for (var i = 0; i < listFileRecord.count; ++i) {
                if (listFileRecord.get(i).selected)
                    listFileRecord.setProperty(i, "selected", false)
            }
        }

        console.log("[select] cleared; rows=", JSON.stringify(selectedRows),
                    "items=", JSON.stringify(selectedItems),
                    "mapSize=", Object.keys(selectedMap).length)
    }

//    function handleRecordFilesUpdate(msg) {
//        console.log("handleRecordFilesUpdate:", msg)

//        var obj = (typeof msg === "string") ? JSON.parse(msg) : msg

//        // รองรับทั้ง objectName / menuID = "recordFilesUpdate"
//        if (!obj ||
//            (obj.objectName !== "recordFilesUpdate" && obj.menuID !== "recordFilesUpdate") ||
//            !obj.records)
//            return

//        function findIndexByKey(key) {
//            for (var i = 0; i < listFileRecord.count; ++i) {
//                var item = listFileRecord.get(i)
//                var itemKey = recordKeyFromData(
//                                  String(item.device),
//                                  String(item.filename),
//                                  String(item.created_at),
//                                  String(item.full_path)
//                              )
//                if (itemKey === key)
//                    return i
//            }
//            return -1
//        }

//        // records ใหม่ (ส่วนใหญ่จะเป็นชุดไฟล์ล่าสุด)
//        for (var i = obj.records.length - 1; i >= 0; --i) {
//            var r = obj.records[i]

//            var created_at  = (r.created_at || "").toString()
//            var parsed_date = (created_at.indexOf("T") !== -1)
//                              ? created_at.split("T")[0]
//                              : ""

//            var dev   = String(r.device || r.id || "")
//            var fname = String(r.filename || "")
//            var fullp = String(r.full_path || r.file_path || "")

//            var key = recordKeyFromData(dev, fname, created_at, fullp)

//            var wasSelected = !!selectedMap[key]
//            var idx = findIndexByKey(key)

//            // ==== ขนาดไฟล์ ====
//            var sizeBytes = Number(r.size_bytes !== undefined ? r.size_bytes : 0)
//            if (!isFinite(sizeBytes) || sizeBytes < 0)
//                sizeBytes = 0

//            var sizeKB = sizeBytes > 0 ? (sizeBytes / 1024.0) : 0

//            // เวลา (วินาที)
//            var durSec = Number(r.duration_sec !== undefined ? r.duration_sec : 0)
//            if (!isFinite(durSec) || durSec < 0)
//                durSec = 0

//            var rowData = {
//                idDevice:          String(r.id || ""),
//                device:            dev,
//                filename:          fname,
//                created_at:        created_at,
//                continuous_count:  String(r.continuous_count || ""),
//                file_path:         String(r.file_path || ""),
//                full_path:         fullp,
//                name:              String(r.name || ""),
//                parsed_date:       String(parsed_date),

//                size_bytes:        sizeBytes,                 // bytes
//                sizeKB:            sizeKB,                    // KB (numeric)
//                size:              sizeKB,                    // role หลัก (numeric KB)
//                size_str:          String(r.size_human || ""),// string สำหรับโชว์
//                duration_sec:      durSec,                    // numeric (sec)

//                selected:          wasSelected
//            }

//            if (idx >= 0) {
//                // อัปเดตแถวเดิม
//                listFileRecord.set(idx, rowData)
//            } else {
//                // แทรกแถวใหม่ด้านบนสุด (ไฟล์ใหม่สุดขึ้นบน)
//                listFileRecord.insert(0, rowData)
//            }
//        }
//    }


    function handleRecordFilesChunk(msg) {
        console.log("handleRecordFilesChunk:", msg)

        var obj = (typeof msg === "string") ? JSON.parse(msg) : msg
        if (!obj || obj.objectName !== "recordFilesChunk" || !obj.records)
            return

        if (typeof obj.page === "number")
            currentPage = obj.page
        if (typeof obj.totalPages === "number")
            totalPages  = obj.totalPages

        // ล้างแล้วเติมใหม่ (page แรก/เปลี่ยนหน้า)
        listFileRecord.clear()

        for (var i = 0; i < obj.records.length; ++i) {
            var r = obj.records[i]

            var created_at  = (r.created_at || "").toString()
            var parsed_date = (created_at.indexOf("T") !== -1)
                              ? created_at.split("T")[0]
                              : ""

            var dev   = String(r.device || r.id || "")
            var fname = String(r.filename || "")
            var fullp = String(r.full_path || r.file_path || "")

            // key เดียวกับ toggleSelection
            var key = recordKeyFromData(dev, fname, created_at, fullp)
            var wasSelected = !!selectedMap[key]

            // ==== ขนาดไฟล์ ====
            var sizeBytes = Number(r.size_bytes !== undefined ? r.size_bytes : 0)
            if (!isFinite(sizeBytes) || sizeBytes < 0)
                sizeBytes = 0


            var sizeKB = (sizeBytes / 1024)
            sizeKB = parseFloat(sizeKB.toFixed(2))

            // เวลา (วินาที)
            var durSec = Number(r.duration_sec !== undefined ? r.duration_sec : 0)
            if (!isFinite(durSec) || durSec < 0)
                durSec = 0

            listFileRecord.append({
                idDevice:          String(r.id || ""),
                device:            dev,
                filename:          fname,
                created_at:        created_at,
                continuous_count:  String(r.continuous_count || ""),
                file_path:         String(r.file_path || ""),
                full_path:         fullp,
                name:              String(r.name || ""),
                parsed_date:       String(parsed_date),

                // --- size / duration เก็บแบบ numeric + string ---
                size_bytes:        sizeBytes,                 // bytes
                sizeKB:            sizeKB,                    // KB (numeric)
                size:              sizeKB,                    // role หลัก (numeric KB)
                size_str:          String(r.size_human || ""),// ข้อความสวย ๆ เช่น "52.9 KB"
                duration_sec:      durSec,                    // วินาที (numeric)

                selected:          wasSelected
            })
        }
        pageReady = true
    }

    function handleDeviceUpdated(msg) {
        console.log("handleDeviceUpdated:", msg)

        var obj = (typeof msg === "string") ? JSON.parse(msg) : msg
        var updates = []

        if (obj.device) {
            updates = [ obj.device ]
        } else if (obj.devices && obj.devices.length) {
            updates = obj.devices
        } else {
            console.warn("handleDeviceUpdated: no device/devices field")
            return
        }

        rebuildRowIndex()
        for (var i = 0; i < updates.length; ++i) {
            var d = updates[i]
            if (d && d.id !== undefined) {
                upsertDevice(d)
            }
        }
        rebuildRowIndex()
    }

    function rebuildRowIndex() {
        rowById = {}
        for (var i = 0; i < listoFDevice.count; ++i) {
            var it = listoFDevice.get(i)
            if (it && it.idDevice !== undefined)
                rowById[String(it.idDevice)] = i
        }
    }

    function upsertDevice(d) {
        // ใช้ชนิด role ของ "แถวแรก" เป็นมาตรฐานกัน Number/String ชน
        var sample = (listoFDevice.count > 0) ? listoFDevice.get(0) : null
        function asType(val, roleName) {
            var want = sample ? typeof sample[roleName] : "string" // ถ้าโมเดลยังว่าง ให้ถือว่า string
            if (want === "number") {
                var n = Number(val)
                return isNaN(n) ? 0 : n
            } else if (want === "boolean") {
                return !!val
            } else { // string
                return (val === null || val === undefined) ? "" : String(val)
            }
        }

        var idStr = String(d.id)
        var idx = rowById.hasOwnProperty(idStr) ? rowById[idStr] : -1

        // --- สร้างเรคคอร์ดที่จะอัปเดต "ยกเว้น selected" ---
        var rec = {
            idDevice:      asType((d.id !== undefined) ? d.id : "", "idDevice"),
            sid:           asType(d.sid,            "sid"),
            name:          asType(d.name,           "name"),
            payload_size:  asType(d.payload_size,   "payload_size"),
            terminal_type: asType(d.terminal_type,  "terminal_type"),
            ip:            asType(d.ip,             "ip"),
            uri:           asType(d.uri,            "uri"),
            freq:          asType(d.freq,           "freq"),
            ambient:       asType((d.ambient !== undefined ? d.ambient : ""), "ambient"),
            group:         asType(d.group,          "group"),
            visible:       asType(d.visible,        "visible"),
            last_access:   asType((d.last_access !== undefined ? d.last_access : ""), "last_access"),
            file_path:     asType(d.file_path,      "file_path"),
            chunk:         asType(d.chunk,          "chunk"),
            updated_at:    asType(d.updated_at,     "updated_at"),
            selected:      asType((idx >= 0 && listoFDevice.get(idx).selected === true), "selected", "")
        }

        if (idx >= 0) {
            // อัปเดตแถวเดิม: "ข้าม selected" เพื่อไม่ให้ติ๊กเอง/เปลี่ยนสถานะเดิม
            for (var k in rec) if (rec.hasOwnProperty(k)) {
                if (k === "selected") continue
                listoFDevice.setProperty(idx, k, rec[k])
            }
        } else {
            // เพิ่มแถวใหม่: append พร้อม selected=false หนเดียว
            var recForAppend = rec
            recForAppend.selected = false   // ✅ มี selected แต่เราไม่ยุ่งทีหลัง
            listoFDevice.append(recForAppend)
            rowById[idStr] = listoFDevice.count - 1
        }
        deviceListUpdated()
    }

    function handleRegisterDevice(msg) {
        var obj = (typeof msg === "string") ? JSON.parse(msg) : msg
        var arr = obj && obj.devices ? obj.devices : []
        console.log("handleRegisterDevice: got", arr.length, "items")

        rebuildRowIndex()

        var seen = {}

        for (var i = 0; i < arr.length; ++i) {
            var d = arr[i]
            var idStr = String(d.id)
            upsertDevice(d)         // ❗ ไม่แตะ selected ในแถวเก่า
            seen[idStr] = true
        }

        for (var r = listoFDevice.count - 1; r >= 0; --r) {
            var it = listoFDevice.get(r)
            var idStr2 = String(it.idDevice)
            if (!seen.hasOwnProperty(idStr2)) {
                listoFDevice.remove(r)
            }
        }

        rebuildRowIndex()
    }

    function handleScanDeviceResult(message) {
        var obj = JSON.parse(message)
        var arr = obj.devices || []
        var list = []
        for (var i = 0; i < arr.length; ++i) {
            var d = arr[i]
            if (!d) continue
            if (d.mounted !== true)
                continue
//            var label = d.name + "  (" + d.sizeGB.toFixed(1) + " GB)  →  " + d.mountPoint
            label = d.mountPoint
            list.push({
                text: label
            })
//            list.push({
//                text: label,
//                name: d.name,
//                devPath: d.devPath,
//                mountPoint: d.mountPoint,
//                sizeGB: d.sizeGB
//            })
        }
        exportDeviceList = list

        console.log("[handleScanDeviceResult] found", list.length, "devices", label)
    }


    function findChildByProperty(parent, propertyName, propertyValue, compareCb) {
        var obj = null
        if (parent === null)
            return null
        var children = parent.children

        for (var i = 0; i < children.length; i++) {
            obj = children[i]
            if (obj.hasOwnProperty(propertyName)) {
                if (compareCb !== null) {
                    if (compareCb(obj[propertyName], propertyValue))
                        break
                } else if (obj[propertyName] === propertyValue)
                {
                    break
                }
            }
            obj = findChildByProperty(obj, propertyName, propertyValue, compareCb)
            if (obj)
                break
        }
        return obj
    }

    function socketConnect() {
        if (socket.status === WebSocket.Open){
            connectToServer.running = false
            connectToServer.repeat = false
            connectToServer.stop()
        }else{
            if(socketPort == 0){
                sockStart=false
            }else{
                sockStart=true
                if (socketCPP.active === true){
                    if (socketCPP.status === WebSocket.Open) {
                        socketCPP.sendTextMessage("Test message from QML!");
                    }
                }
                console.log("Reconnecting....")
                if (socket.status === WebSocket.Error)
                {
                    console.log("Error: " + socket.errorString)
                }
                else if (socket.status === WebSocket.Open)
                {
                    console.log("Socket opened")
                }
                else if (socket.status === WebSocket.Closed)
                {
                    console.log("Socket closed")
                }
            }
        }
    }

    onSockStartChanged: {
        sockStart.active = !sockStart.active
        console.log("socket.active",socket.active)
    }
    Component.onCompleted: {
        console.log("Testing WebSocket connection...");
        socketCPP.active = true;
        console.log("status of socket",socketCPP.active);
        socketConnectToCpp =  '{"objectName":"socketConnect","socketCPP": "'+ socketCPP.active+'"}'
        console.log("socketConnect_onCompleted:",socketConnect);
        timerConnectSocket.start();
        playerlog.play()
    }


    function selectedRecords() {
        var out = []
        for (var i = 0; i < listFileRecord.count; ++i) {
            var r = listFileRecord.get(i)
            if (r && r.selected === true) out.push(r)
        }
        return out
    }
    function recordToPath(rec) {
        var dir  = String(rec.file_path || "").replace(/\/+$/,"")
        var file = String(rec.filename  || "").replace(/^\/+/,"")
        return (dir && file) ? (dir + "/" + file) : ""
    }
    function playSelectedInWaveEditor() {
        var recs = selectedRecords()
        waveEditor.setFilesFromRecords(recs)
        waveEditor.playAllFiles(0)
    }
//    function clearSelections() {
//        console.log("clearSelections count=", selectedRows, selectedItems)

//        selectedRows  = []
//        selectedItems = []
//        selectedMap   = {}    // ★ ล้าง map ด้วย เวลาอยากเริ่มใหม่จริง ๆ

//        if (listFileRecord && listFileRecord.count > 0) {
//            for (var i = 0; i < listFileRecord.count; ++i) {
//                if (listFileRecord.get(i).selected)
//                    listFileRecord.setProperty(i, "selected", false)
//            }
//        }

//        console.log("[select] cleared; rows=", JSON.stringify(selectedRows),
//                    "items=", JSON.stringify(selectedItems),
//                    "mapSize=", Object.keys(selectedMap).length)
//    }

    function getSelectedCount() { return selectedRows.length }
    function getSelectedList() { return selectedItems.slice() }
    function recordKeyFromData(device, filename, created_at, full_path) {
        if (full_path && full_path.length)
            return String(full_path)

        return String(filename) + "|" + String(created_at)
    }

    function recordKeyFromModelRow(row) {
        var it = listFileRecord.get(row)
        if (!it) return ""
        return recordKeyFromData(
                    it.device || it.idDevice || "",
                    it.filename || "",
                    it.created_at || "",
                    it.full_path || it.file_path || ""
                )
    }


    function toggleSelection(row, checked) {
        console.log("toggleSelection->", "[select] row=", row, "checked=", checked)

        // กัน row ผิดปกติ
        if (row < 0 || row >= listFileRecord.count) {
            console.warn("toggleSelection: invalid row", row, "count=", listFileRecord.count)
            return
        }

        var it = listFileRecord.get(row)
        if (!it) {
            console.warn("toggleSelection: no item at row", row)
            return
        }

        // --- ดึง key หลักจาก row ปัจจุบัน ---
        var idStr  = String(it.idDevice || it.device || "")
        var fname  = String(it.filename || "")
        var ctime  = String(it.created_at || "")
        var fpath  = String(it.full_path || it.file_path || "")
        var dev    = String(it.device || "")
        var name   = String(it.name || "")
        var pdate  = String(it.parsed_date || "")

        // ขนาดไฟล์ (ต้องพยายามดึงค่า numeric ให้ได้)
        var sizeKB = 0
        if (it.size_bytes !== undefined && it.size_bytes !== null && it.size_bytes !== "") {
            // ถ้ามี size_bytes (หน่วย byte) → แปลงเป็น KB
            var bytes = Number(it.size_bytes)
            if (isFinite(bytes) && bytes > 0)
                sizeKB = bytes / 1024.0
        } else if (it.size !== undefined && it.size !== null && it.size !== "") {
            // ถ้า it.size เป็นตัวเลข KB อยู่แล้ว ก็ใช้เลย
            var maybe = Number(it.size)
            if (isFinite(maybe) && maybe > 0)
                sizeKB = maybe
        }

        // duration จาก row
        var durSec = 0
        if (it.duration_sec !== undefined && it.duration_sec !== null && it.duration_sec !== "") {
            var d = Number(it.duration_sec)
            if (isFinite(d) && d > 0)
                durSec = d
        }

        var key = recordKeyFromData(idStr, fname, ctime, fpath)

        if (checked) {
            // sync กับ model ด้วย (เผื่อที่อื่นอ่าน role "selected")
            listFileRecord.setProperty(row, "selected", true)

            // --- selectedRows: เก็บ index ของแถวที่ถูกเลือก ---
            if (selectedRows.indexOf(row) === -1)
                selectedRows.push(row)

            // --- selectedItems: เก็บ object ไฟล์ไว้ใช้กับ WaveEditor ---
            var existIndex = -1
            for (var i = 0; i < selectedItems.length; ++i) {
                if ((selectedItems[i].key || "") === key) {
                    existIndex = i
                    break
                }
            }

            var entry = {
                key:          key,
                row:          row,
                idDevice:     idStr,
                device:       dev,
                filename:     fname,
                created_at:   ctime,
                parsed_date:  pdate,
                file_path:    String(it.file_path || ""),
                full_path:    fpath,

                // สำคัญ: ต้องมี size / duration_sec ให้ WaveEditor ใช้
                size:         sizeKB,      // numeric (KB)
                size_bytes:   it.size_bytes !== undefined ? it.size_bytes : (sizeKB * 1024.0),
                duration_sec: durSec,

                name:         name
            }

            if (existIndex >= 0)
                selectedItems[existIndex] = entry
            else
                selectedItems.push(entry)

            // --- selectedMap: ใช้จำข้าม page / ข้าม refresh ---
            selectedMap[key] = true

        } else {
            // sync กับ model
            listFileRecord.setProperty(row, "selected", false)

            // เอา row ออกจาก selectedRows
            var idx = selectedRows.indexOf(row)
            if (idx !== -1)
                selectedRows.splice(idx, 1)

            // เอาออกจาก selectedItems ตาม key
            for (var j = selectedItems.length - 1; j >= 0; --j) {
                if ((selectedItems[j].key || "") === key)
                    selectedItems.splice(j, 1)
            }

            // ลบ flag ใน map
            if (selectedMap[key])
                delete selectedMap[key]
        }

        console.log("[select] row=", row,
                    "checked=", checked,
                    "selectedRows.count=", selectedRows.length,
                    "selectedItems.count=", selectedItems.length,
                    "selectedMap.size=", Object.keys(selectedMap).length)

        // debug ดูว่าตอนนี้ selectedItems มี size/dur หรือยัง
        for (var k = 0; k < selectedItems.length; ++k) {
            var s = selectedItems[k]
            console.log("   [Sel]", k, s.full_path,
                        "sizeKB=", s.size,
                        "dur_sec=", s.duration_sec)
        }
    }

//    function toggleSelection(row, checked) {
//        console.log("toggleSelection->","[select] row=", row, "checked=", checked)

//         const it = listFileRecord.get(row)
//         if (!it) return
//         const idStr = String(it.idDevice || it.filename || it.file_path || row)
//         if (checked) {
//             if (selectedRows.indexOf(row) === -1) selectedRows.push(row)
//             const key = idStr + "|" + String(it.filename||"") + "|" + String(it.created_at||"")
//             let dup = false
//             for (var i=0;i<selectedItems.length;i++){
//                 if ((selectedItems[i].key||"") === key) { dup = true; break }
//             }
//             if (!dup) selectedItems.push({
//                 key: key,
//                 row: row,
//                 idDevice: idStr,
//                 filename: String(it.filename||"")
//             })
//         } else {
//             const idx = selectedRows.indexOf(row)
//             if (idx !== -1) selectedRows.splice(idx, 1)
//             for (let i=selectedItems.length-1;i>=0;--i){
//                 if (selectedItems[i].row === row) selectedItems.splice(i,1)
//             }
//         }
//         console.log("[select] row=", row, "checked=", checked, "count=", selectedRows.length)
//     }
    function getSelectedIds() {
        var ids = []
        for (var i=0;i<selectedItems.length;i++)
            ids.push(selectedItems[i].idDevice)
        return ids
    }

    // This is available in all editors.


    MediaPlayer {
        id: playerlog
        volume: playerVolume
        autoPlay: false

        onStatusChanged: {
            console.log("[playerlog] status =", status, "position =", position)

            // ไฟล์จบ (EndOfMedia) → ถ้า concatMode = true ให้เล่นไฟล์ถัดไป
            if (status === MediaPlayer.EndOfMedia && window.waveEditorConcatMode) {
                if (!window.waveEditorFiles || window.waveEditorFiles.length === 0)
                    return;

                var nextIndex = window.waveEditorIndex + 1
                if (nextIndex < window.waveEditorFiles.length) {
                    window.waveEditorIndex = nextIndex

                    var p = window.waveEditorFiles[nextIndex]
                    var url = (p.indexOf("file://") === 0) ? p : ("file://" + p)

                    console.log("[playerlog] play next:", nextIndex, url)
                    playerlog.stop()
                    playerlog.source = url
                    playerlog.play()
                } else {
                    console.log("[playerlog] playlist finished")
                    // อยาก reset index ก็ได้
                    // window.waveEditorIndex = -1
                }
            }
        }

        onError: {
            console.log("[playerlog] error =", error, "errorString =", errorString)
        }
    }

}
