// main.qml

import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import QtWebSockets 1.0
import QtGraphicalEffects 1.0

// Virtual Keyboard: ใช้ 2.4 ให้ตรงกับของเดิม / SideSettingsDrawer
import QtQuick.VirtualKeyboard 2.4
import QtQuick.VirtualKeyboard.Styles 2.4
import QtQuick.VirtualKeyboard.Settings 2.4

import QtQuick.Extras 1.4
import QtCharts 2.2
import Qt.labs.settings 1.0
import QtMultimedia 5.12

import "iScreenDFqml/pages"
import "iRecordManage"
import "./"
Window {
    id: window
    visible: true
    width: 400
    height: 1280
    flags: Qt.FramelessWindowHint | Qt.Window
    signal profilesLoaded()
    signal getScreenshot()
    signal qmlCommand(string msg)
    signal sCan(string mode)
    signal signalProfileCards()   // ประกาศ signal
    signal profileWeb(string msg)
    property real screenrotation: 0
    property real frequencyUnitValue: -1
    property bool screenOff: false
    property bool widgetView: false
    property bool trigerScan: true
    property bool updateListProfiles: false
    // //--------------Recoder-----------------------------------
    signal deviceListUpdated()
    property string serverAddress: "127.0.0.1"
    property var socketPort: 0
    property string socketUrl: "ws://"+ serverAddress + ":" + socketPort.toString()
    property bool sockStart: false
    property var objectName: []
    property var socketConnectToCpp:""
//    signal qmlCommand(string msg)
    property var currentDate:""
    property var currentTime:""
    property var currentPage:""
    property var totalPages:""
//    property real screenrotation: 0
    property var rowById: ({})
    property var selectedRows: []
    property var selectedItems: []
    property var selectedMap:   ({})
    property bool recordListFrozen: false
    property bool freezeRecordFilesUpdate: false
    property bool restoringSelection: false
    property real totalsizeKB: 0.0
    property real totalDurationSecSelected: 0.0

    property var exportDeviceList: []
    property string selectedExportMountPoint: ""
    property string selectedExportDevPath: ""
    property var label: ""
    property var pathToSave: ""

    property string statusSearching: ""
    property string statusSearchingDone: ""
    property string  statusScan: ""
    property bool waitingDoneStatus: false
    property string statusDeviceScan: ""
    property real currentVolumeLevel:0
    property real convertCurrentVolumeLevel:0
    property bool pageReady: false

    property var waveEditorFiles: []
    property bool waveEditorConcatMode: false
    property int waveEditorIndex: -1
    property string currentWaveFile: ""
    property string waveEditorFirstFile: ""
    property real playerVolume: 1.0
    //---------------------------------------------------------------------
    // signal getScreenshot()
    //---------------------Recoder function-------------------------------
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
            socketCPP.active = sockStart
            console.log("socket.active",socket.active)
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

            // ขนาดไฟล์ (KB numeric)
            var sizeKB_local = 0.0
            if (it.size_bytes !== undefined && it.size_bytes !== null && it.size_bytes !== "") {
                var bytes = Number(it.size_bytes)
                if (isFinite(bytes) && bytes > 0)
                    sizeKB_local = bytes / 1024.0
            } else if (it.size !== undefined && it.size !== null && it.size !== "") {
                var maybe = Number(it.size)
                if (isFinite(maybe) && maybe > 0)
                    sizeKB_local = maybe
            }

            // duration จาก row (sec)
            var durSec = 0.0
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
                    size:         sizeKB_local,   // numeric (KB)
                    size_bytes:   (it.size_bytes !== undefined && it.size_bytes !== null && it.size_bytes !== "")
                                  ? it.size_bytes
                                  : Math.round(sizeKB_local * 1024.0),
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

            // ===== Recompute totalsizeKB + totalDurationSecSelected =====
            var sumKB = 0.0
            var sumSec = 0.0
            for (var kk = 0; kk < selectedItems.length; ++kk) {
                var si = selectedItems[kk]
                var kb = Number(si.size)
                if (isFinite(kb) && kb > 0) sumKB += kb

                var ds = Number(si.duration_sec)
                if (isFinite(ds) && ds > 0) sumSec += ds
            }

            // ⚠️ totalsizeKB / totalDurationSecSelected ต้องประกาศเป็น property ที่ scope นี้เห็นได้
            totalsizeKB = sumKB
            totalDurationSecSelected = sumSec

            console.log("[TOTAL] totalsizeKB=", totalsizeKB.toFixed(3),
                        "totalDurationSecSelected=", totalDurationSecSelected.toFixed(3))

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
        function toNumberSafe(v, def) {
            var n = Number(v)
            return (isFinite(n) ? n : (def !== undefined ? def : 0))
        }
        function recomputeTotalsFromSelectedItems() {
            var sumKB = 0.0
            var sumSec = 0.0

            for (var i = 0; i < selectedItems.length; ++i) {
                var s = selectedItems[i] || {}

                // sizeKB
                var kb = 0.0
                if (s.size_bytes !== undefined && s.size_bytes !== null && s.size_bytes !== "") {
                    kb = toNumberSafe(s.size_bytes, 0) / 1024.0
                } else if (s.size !== undefined && s.size !== null && s.size !== "") {
                    kb = toNumberSafe(s.size, 0) // assume already KB
                }
                sumKB += kb

                // duration_sec
                var ds = toNumberSafe(s.duration_sec, 0)
                sumSec += ds
            }

            totalsizeKB = sumKB
            totalDurationSecSelected = sumSec

            console.log("[TOTAL] totalsizeKB=", totalsizeKB.toFixed(3),
                        "totalDurationSecSelected=", totalDurationSecSelected.toFixed(3))
        }
        function restoreSelectionFromTxtAndSyncModel() {
            console.log("[RESTORE] start restoreSelectionFromTxtAndSyncModel()")

            if (!freezeRecordFilesUpdate) {
                console.log("[RESTORE] skip เพราะ freezeRecordFilesUpdate=false")
                return
            }

            if (!fileReader || !fileReader.loadWaveSelectionState) {
                console.log("[RESTORE] fileReader not ready")
                return
            }

            var st = fileReader.loadWaveSelectionState()
            if (!st || !st.ok || !st.files || st.files.length === 0) {
                console.log("[RESTORE] no saved selection -> do nothing")
                return
            }

            console.log("[RESTORE] loaded files =", st.files.length, "summary =", JSON.stringify(st.summary))

            // ทำ set สำหรับ lookup เร็ว
            var setMap = {}
            for (var i = 0; i < st.files.length; ++i) {
                setMap[String(st.files[i])] = true
            }

            // 1) sync model.selected ให้ตรงกับ txt
            var hit = 0
            for (var r = 0; r < listFileRecord.count; ++r) {
                var it = listFileRecord.get(r)
                var fp = (it && it.full_path) ? String(it.full_path) : ""
                var want = !!setMap[fp]
                if (!!it.selected !== want) {
                    listFileRecord.setProperty(r, "selected", want)
                }
                if (want) hit++
            }
            console.log("[RESTORE] synced model.selected hit =", hit)

            // 2) วาด waveform จากไฟล์ใน txt (ส่งเป็น string list ได้เลย)
            if (editor && editor.setFiles) {
                editor.setFiles(st.files)
                console.log("[RESTORE] editor.setFiles(files) called")
            }
        }
        function getSelectedIds() {
            var ids = []
            for (var i=0;i<selectedItems.length;i++)
                ids.push(selectedItems[i].idDevice)
            return ids
        }
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

// //--------------------------------------------------------------------------------------------
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

    function qmlSubmitTextFiled(message){
        console.log("qmlSubmitTextFiled",message)
        var JsonObject = JSON.parse(message);
        var objectName = JsonObject.objectName;
        var TrapsAlert = JsonObject.TrapsAlert;
        var menuID = JsonObject.menuID;
        var eventRecord = JsonObject.eventRecord;
        var obj = (typeof message === "string") ? JSON.parse(message) : message;

        if (message !== "")
        {
            var JsonObject;
            try {
                JsonObject = JSON.parse(message);
//                console.log(JsonObject.menuID)
            }
            catch(error)
            {
                console.error("error",message)
            }
            if(JsonObject.objectName === "update"){
                updateListProfiles = true
            }
            else if(message === "update"){

            }
        }
        if (obj && obj.menuID === "DateTime" && typeof obj.formattedDateTime === "string") {
            var onlyDate = obj.formattedDateTime.split(" ")[0];
            var onlyTime = obj.formattedDateTime.split(" ")[1];

            currentDate = onlyDate;
            currentTime = onlyTime;

        } else if(objectName === "recordFilesChunk"){
            console.log("[QML_recordFilesChunk] arrived","freeze=", freezeRecordFilesUpdate ,"restoring=", restoringSelection,"selectedItems=", (selectedItems ? selectedItems.length : 0))
            if (freezeRecordFilesUpdate || restoringSelection) {
                console.log("[recordFilesChunk] SKIP clear selection file (freeze/restoring)")
            } else {
                var hasSelection = (selectedItems && selectedItems.length > 0)

                if (!hasSelection) {
                    console.log("[recordFilesChunk] CLEAR filesNameWave.txt (no selection)")
                    if (typeof fileReader !== "undefined" && fileReader && fileReader.clearWaveSelectionState) {
                        fileReader.clearWaveSelectionState()
                    } else {
                        console.warn("[recordFilesChunk] fileReader.clearWaveSelectionState not ready")
                    }
                } else {
                    console.log("[recordFilesChunk] keep selection file (has selection)")
                }
            }
            console.log("QML_recordFilesChunk:", message)
            handleRecordFilesChunk(message)
            return
//            handleRecordFilesChunk(message)
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

    //====================load flie=============================
        function loadWaveSelectionFromTxt() {
            var path = "/home/orinnx/saveFileName/filesNameWave.txt"
            var data = fileReader.readFile(path)
            if (!data || data.length === 0) {
                console.log("[WaveEditor] restore: txt empty")
                return
            }

            var text = data.toString()
            var lines = text.split(/\r?\n/)
            var files = []

            for (var i = 0; i < lines.length; ++i) {
                var p = lines[i].trim()
                if (p.length > 0)
                    files.push(p)
            }

            console.log("[WaveEditor] restore from txt:", files.length, "file(s)")
            if (files.length > 0)
                setFiles(files)
        }
    }
    //==========================================================
    Component.onCompleted: {
        console.log(window.width,window.height)
        if(window.width < window.height){
            screenrotation = 270
            window.width = 400
            window.height = 1280
        }
        else{
            screenrotation = 0
            window.width = 1920
            window.height = 1020
        }
        console.log("Testing WebSocket connection...");
        socketCPP.active = true;
        console.log("status of socket",socketCPP.active);
        socketConnectToCpp =  '{"objectName":"socketConnect","socketCPP": "'+ socketCPP.active+'"}'
        console.log("socketConnect_onCompleted:",socketConnect);
        timerConnectSocket.start();
        freezeRecordFilesUpdate = false
        restoringSelection = false
        console.log("[BOOT] reset flags ->freezeRecordFilesUpdate:", freezeRecordFilesUpdate, "[BOOT] reset flags ->restoringSelection:", restoringSelection)
        playerlog.play()
    }
    function checkcammeraList (ipaddress)
    {
        var cammeraIndex = 0
        for (cammeraIndex = 0; cammeraIndex < cammeraList.count; cammeraIndex++){
            var ipAddress = cammeraList.get(cammeraIndex).ipAddress
            if (ipAddress === ipaddress) {
                return false
            }
        }
        return true
    }

    ListModel {
        id:notificationList
    }


    ListModel {
        id: sqlMode
        ListElement { index: 0;  name : "NSQ (A)" }
        ListElement { index: 1;  name : "NSQ" }
        ListElement { index: 2;  name : "LSQ (A)" }
        ListElement { index: 3;  name : "LSQ" }
    }


    ListModel {
        id: adMode
        ListElement { index: 0;  name: "Analog"; shotName: "A"}
        ListElement { index: 1;  name: "Digital"; shotName: "D"}
    }
    ListModel {
        id: fmIFBW
        ListElement { index: 0;  low_cut: -15000;  high_cut: 15000;  name: "30kHz" }
        ListElement { index: 1;  low_cut: -7500;   high_cut: 7500;   name: "15kHz" }
        ListElement { index: 2;  low_cut: -3000;   high_cut: 3000;   name: "6kHz" }
        ListElement { index: 3;  low_cut: -2000;   high_cut: 2000;   name: "4kHz" }
    }

    ListModel {
        id: wfmIFBW
        ListElement { index: 0; low_cut: -125000; high_cut: 125000; name: "250kHz" }
        ListElement { index: 1; low_cut: -100000; high_cut: 100000; name: "200kHz" }
        ListElement { index: 2; low_cut: -75000;  high_cut: 75000;  name: "150kHz" }
        ListElement { index: 3; low_cut: -50000;  high_cut: 50000;  name: "100kHz" }
    }

    ListModel {
        id: amIFBW
        ListElement { index: 0; low_cut: -7500;  high_cut: 7500;  name: "15kHz" }
        ListElement { index: 1; low_cut: -4000;  high_cut: 4000;  name: "8kHz" }
        ListElement { index: 2; low_cut: -2750;  high_cut: 2750;  name: "5.5kHz" }
        ListElement { index: 3; low_cut: -1900;  high_cut: 1900;  name: "3.8kHz" }
    }

    ListModel {
        id: sahIFBW
        ListElement { index: 0; low_cut: -2750;  high_cut: 2750;  name: "5.5kHz" }
        ListElement { index: 1; low_cut: -1900;  high_cut: 1900;  name: "3.8kHz" }
    }

    ListModel {
        id: salIFBW
        ListElement { index: 0; low_cut: -2750;  high_cut: 2750;  name: "5.5kHz" }
        ListElement { index: 1; low_cut: -1900;  high_cut: 1900;  name: "3.8kHz" }
    }

    ListModel {
        id: usbIFBW
        ListElement { index: 0;  low_cut : 300;   high_cut : 5800;   name: "5.5kHz"}
        ListElement { index: 1;  low_cut : 300;   high_cut : 3000;   name: "2.7kHz"}
    }
    ListModel {
        id: usbdIFBW
        ListElement { index: 0;  low_cut : 300;   high_cut : 2900;   name: "2.6kHz"}
        ListElement { index: 1;  low_cut : 300;   high_cut : 2700;   name: "2.4kHz"}
        ListElement { index: 2;  low_cut : 300;   high_cut : 1000;   name: "0.7kHz"}
    }
    ListModel {
        id: lsbIFBW
        ListElement { index: 0;  low_cut : -300;   high_cut : -5800;   name: "5.5kHz"}
        ListElement { index: 1;  low_cut : -300;   high_cut : -3000;   name: "2.7kHz"}
    }
    ListModel {
        id: cwIFBW
        ListElement { index: 0;  low_cut : 500;    high_cut : 1000;  name: "500Hz"}
        ListElement { index: 1;  low_cut : 700;    high_cut : 900;   name: "200Hz"}
    }
    ListModel {
        id: dmrIFBW
        ListElement { index: 0;  low_cut : -6250;    high_cut : 6250;  name: "12.5kHz"}
    }
    ListModel {
        id: dstarIFBW
        ListElement { index: 0;  low_cut : -3250;    high_cut : 3250;  name: "6.5kHz"}
    }
    ListModel {
        id: nxdnIFBW
        ListElement { index: 0;  low_cut : -3250;    high_cut : 3250;  name: "6.5kHz"}
    }
    ListModel {
        id: yfsIFBW
        ListElement { index: 0;  low_cut : -6250;    high_cut : 6250;  name: "12.5kHz"}
    }
    ListModel {
        id: profileCards
        // ListElement { index: 0;  freq : 30; unit : "MHz"; bw:"200kHz"; startHz:0; endHz:0; mode:"default"; low_cut : -6250;  high_cut : 6250; }
    }
    ListModel {
        id: profileScan
        // ListElement { index: 0;  freq : 30; unit : "MHz"; bw:"200kHz"; startHz:0; endHz:0; mode:"default"; low_cut : -6250;  high_cut : 6250; }
    }
    ListModel {
        id: groupedScanModel
        // ListElement { index: 0;  freq : 30; unit : "MHz"; bw:"200kHz"; startHz:0; endHz:0; mode:"default"; low_cut : -6250;  high_cut : 6250; }
    }
    ListModel {
        id: foundCards
        // ListElement { index: 0;  freq : 30; unit : "MHz"; bw:"200kHz"; startHz:0; endHz:0; mode:"default"; low_cut : -6250;  high_cut : 6250; }
    }
    ListModel {
        id: receiverMode
        ListElement { index: 0;  name : "FM";       text: "nfm";    mode: "Analog" ; modeID: 0; bw:"fmIFBW"  }
        ListElement { index: 1;  name : "WFM";      text: "wfm";    mode: "Analog" ; modeID: 0; bw:"wfmIFBW" }
        ListElement { index: 2;  name : "AM";       text: "am";     mode: "Analog" ; modeID: 0; bw:"amIFBW" }
        ListElement { index: 3;  name : "LSB";      text: "lsb";    mode: "Analog" ; modeID: 0; bw:"lsbIFBW" }
        ListElement { index: 4;  name : "USB";      text: "usb";    mode: "Analog" ; modeID: 0; bw:"usbIFBW" }
        ListElement { index: 5;  name : "CW";       text: "cw";     mode: "Analog" ; modeID: 0; bw:"cwIFBW" }
        ListElement { index: 6;  name : "SAM";      text: "sam";    mode: "Analog" ; modeID: 0; bw:"amIFBW" }
        ListElement { index: 7;  name : "DATA";     text: "usbd";   mode: "Digital" ; modeID: 1; bw:"usbdIFBW" }
        ListElement { index: 8;  name : "DMR";      text: "dmr";    mode: "Digital" ; modeID: 1; bw:"dmrIFBW" }
        ListElement { index: 9;  name : "D-Star";   text: "dstar";  mode: "Digital" ; modeID: 1; bw:"dstarIFBW" }
        ListElement { index: 10; name : "NXDN";     text: "nxdn";   mode: "Digital" ; modeID: 1; bw:"nxdnIFBW" }
    }
    // Analog Modes
    ListModel {
        id: receiverAnalogMode
        ListElement { index: 0;  name : "FM";       text: "nfm";    mode: "Analog" ; modeID: 0; bw:"fmIFBW"  }
        ListElement { index: 1;  name : "WFM";      text: "wfm";    mode: "Analog" ; modeID: 0; bw:"wfmIFBW" }
        ListElement { index: 2;  name : "AM";       text: "am";     mode: "Analog" ; modeID: 0; bw:"amIFBW" }
        ListElement { index: 3;  name : "LSB";      text: "lsb";    mode: "Analog" ; modeID: 0; bw:"lsbIFBW" }
        ListElement { index: 4;  name : "USB";      text: "usb";    mode: "Analog" ; modeID: 0; bw:"usbIFBW" }
        ListElement { index: 5;  name : "CW";       text: "cw";     mode: "Analog" ; modeID: 0; bw:"cwIFBW" }
        ListElement { index: 6;  name : "SAM";      text: "sam";    mode: "Analog" ; modeID: 0; bw:"amIFBW" }
    }

    // Digital Modes
    ListModel {
        id: receiverDigitalMode
        ListElement { index: 7;  name : "DATA";     text: "usbd";   mode: "Digital" ; modeID: 1; bw:"usbdIFBW" }
        ListElement { index: 8;  name : "DMR";      text: "dmr";    mode: "Digital" ; modeID: 1; bw:"dmrIFBW" }
        ListElement { index: 9;  name : "D-Star";   text: "dstar";  mode: "Digital" ; modeID: 1; bw:"dstarIFBW" }
        ListElement { index: 10; name : "NXDN";     text: "nxdn";   mode: "Digital" ; modeID: 1; bw:"nxdnIFBW" }
    }

    ListModel {
        id: freqUnitList
        ListElement { index: 0;  name : "Hz"  }
        ListElement { index: 1;  name : "kHz" }
        ListElement { index: 2;  name : "MHz" }
        ListElement { index: 3;  name : "GHz" }
    }
    // ListModel {
    //     id: receiverDigitalMode
    //     ListElement { index: 0;  name : "ALIN" }
    //     ListElement { index: 1;  name : "P-25" }
    //     ListElement { index: 2;  name : "dPMR" }
    //     ListElement { index: 3;  name : "D-CR" }
    //     ListElement { index: 4;  name : "DMR" }
    //     ListElement { index: 5;  name : "YAES" }
    //     ListElement { index: 6;  name : "DSTR" }
    // }


    ListModel {
        id: memoriesModel
        ListElement {index:0; name:"Name"; mod:"MOD";  modMode:"A"; freq:100000000; unit:"Hz"; bw:"3khz"}
        ListElement {index:1; name:"Radio 2"; mod:"AM"; modMode:"A"; freq:1133600000; unit:"kHz"; bw:"5.5kHz"}
        ListElement {index:2; name:"Radio 3"; mod:"AM"; modMode:"A"; freq:1133600000; unit:"MHz"; bw:"5.5kHz"}
        ListElement {index:3; name:"Radio 4"; mod:"FM"; modMode:"A"; freq:100000000; unit:"MHz"; bw:"200kHz"}
        ListElement {index:4; name:"Radio 5"; mod:"AM"; modMode:"A"; freq:133600000; unit:"Hz"; bw:"5.5kHz"}
        ListElement {index:5; name:"Radio 6"; mod:"AM"; modMode:"A"; freq:133600000; unit:"MHz"; bw:"200kHz"}
        ListElement {index:6; name:"Radio 7"; mod:"AM"; modMode:"A"; freq:133600000; unit:"MHz"; bw:"200kHz"}
        ListElement {index:7; name:"Radio 8"; mod:"AM"; modMode:"A"; freq:133600000; unit:"MHz"; bw:"200kHz"}
        ListElement {index:8; name:"Radio 9"; mod:"AM"; modMode:"A"; freq:133600000; unit:"MHz"; bw:"200kHz"}
        ListElement {index:9; name:"Radio 10"; mod:"AM"; modMode:"A"; freq:133600000; unit:"MHz"; bw:"200kHz"}
        ListElement {index:10; name:"Radio 11"; mod:"AM"; modMode:"A"; freq:133600000; unit:"MHz"; bw:"200kHz"}
        ListElement {index:11; name:"Radio 12"; mod:"AM"; modMode:"A"; freq:133600000; unit:"MHz"; bw:"200kHz"}
        ListElement {index:12; name:"Radio 13"; mod:"AM"; modMode:"A"; freq:133600000; unit:"MHz"; bw:"200kHz"}
        ListElement {index:13; name:"Radio 14"; mod:"AM"; modMode:"A"; freq:133600000; unit:"MHz"; bw:"200kHz"}
        ListElement {index:14; name:"Radio 15"; mod:"AM"; modMode:"A"; freq:133600000; unit:"MHz"; bw:"200kHz"}
    }

    ListModel
    {
        id: listmodel
        ListElement {index:0; source: "" }
        ListElement {index:1; source: "qrc:/MemoryAddEdit.qml"; name:"MemoryAddEdit" }
        ListElement {index:2; source: "qrc:/RecConfigPage.qml"; name:"OpenWebRXProfiles" }
        ListElement {index:3; source: "qrc:/Setting.qml"; name:"Setting" }
    }
    Page
    {
        id: page
        x: 0
        y: 0
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        width: screenrotation==270 ? 1280 : 1920
        height: screenrotation==270 ? 400 : 1080
        rotation: screenrotation
        anchors.centerIn: parent

        states: [
            State {
                name: "landscape"
                when: page.rotation === 0
                PropertyChanges {
                    target: window
                    contentOrientation: Qt.PrimaryOrientation
                }
            },
            State {
                name: "portrait"
                when: page.rotation === 90
                PropertyChanges {
                    target: page
                    width: window.height
                    height: window.width
                }
                PropertyChanges {
                    target: window
                    contentOrientation: Qt.LandscapeOrientation
                }
            },
            State {
                name: "invertedlandscape"
                when: page.rotation === 180
                PropertyChanges {
                    target: window
                    contentOrientation: Qt.InvertedPortraitOrientation
                }
            },
            State {
                name: "invertedportrait"
                when: page.rotation === 270
                PropertyChanges {
                    target: page
                    width: window.height
                    height: window.width
                }
                PropertyChanges {
                    target: window
                    contentOrientation: Qt.InvertedLandscapeOrientation
                }
            }
        ]
        Rectangle {
            id: rectangleHome
            color: "#509fef"
            anchors.fill: parent
            // HomeDisplay {
            //     id: homeDisplay
            //     anchors.fill: parent
            // }
            MainPage {
                id: mainPage
                anchors.fill: parent
            }
        }
    }

    KeyboardInput {
        id: inputKey
        anchors.fill: parent
        y: rotation==270 ? 400+height : 1080+height
    }


    Rectangle{
        id: rectangleScreen
        visible: screenOff
        z:300
        color: "black"
        anchors.fill: parent
        width: screenOff ? parent.width : 0
        height: screenOff ? parent.height : 0
        MouseArea {
            z:300
            anchors.fill: parent
            onClicked: {
                screenOff = false
                mainWindows.onScreenRequested()
            }
        }
    }
}

