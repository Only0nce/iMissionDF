// sidepanels/SideLogsFile.qml
// ✅ Select DOA log แล้ว show บน QMLMap.qml ทันที
// ✅ จำ selected logs ผ่าน Settings
// ✅ กลับเข้า QMLMap.qml แล้วเรียก reloadAndSendToMap() เพื่อ load DB + ส่งค่าไป map
// ✅ Delete selected logs ส่ง JSON ไป C++ ผ่าน requestDeleteDoaLogsJson(jsonText)
// ✅ FIX: reload / visibleChanged / completed จะไม่ทำให้ Map ขยับมั่ว
// ✅ ส่ง keepMapView:true ตอน auto reload เพื่อให้ QMLMap.qml ไม่ center / fit / zoom เอง
// ✅ ADD: FIRST / PREV / NEXT / LAST + Search button สำหรับโหลด log ทีละหน้า
// ✅ ADD: เก็บ selectedLogRecords เพื่อ select ข้ามหน้า แล้วยังส่ง selected ก่อนหน้าไป Map ด้วย
// ✅ FIX: Search ยิง Database ทั้งหมด ไม่ filter เฉพาะ page ปัจจุบัน
// ✅ ADD: จำกัดการเลือกสูงสุด 200 รายการ ถ้าเต็มแล้วต้องเอาออกก่อนถึงจะเลือกเพิ่มได้

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import Qt.labs.settings 1.1

Item {
    id: sidelogs
    anchors.fill: parent
    clip: true

    implicitWidth: 430
    implicitHeight: 360

    property var krakenmapval: null

    property bool _applying: false
    property bool _mapSendPending: false
    property bool _restoringSelection: false
    property bool _firstCompleted: false
    property int _lastRequestMs: 0

    // ✅ FIX MAP JUMP
    // true  = ส่ง overlay ไป map แต่ห้าม QMLMap center/fit/zoom
    // false = user เลือกเอง อนุญาตให้ QMLMap focus ได้
    property bool _pendingKeepMapView: true
    property string _lastMapPayloadKey: ""

    property var filteredLogs: []
    property int highlightedFilteredIndex: -1

    // ===== Server-side paging =====
    property int currentPage: 1
    property int pageSize: 50
    property int totalPages: 1
    property int totalRowsFromDb: 0
    property string serverSearchText: ""
    property bool pageLoading: false

    property int totalRows: 0
    property string latestTime: "-"

    // ===== Multi select =====
    property var selectedLogKeys: ({})
    // ✅ เก็บข้อมูลเต็มของรายการที่เคยเลือกไว้ เพื่อส่งข้ามหน้า DB paging ได้
    property var selectedLogRecords: ({})
    property int selectedCount: 0
    // ✅ จำกัดจำนวน DOA logs ที่เลือกได้สูงสุด
    property int maxSelectedLogs: 200
    property string selectLimitWarningText: ""

    // ===== Behavior =====
    property bool autoShowSelectedOnMap: true
    property bool reloadWhenVisible: true

    Settings {
        id: sideLogSettings
        category: "SideLogs"
        property string selectedLogKeysJson: "{}"
        // ✅ จำ record เต็มของรายการที่เลือกไว้ด้วย ไม่ใช่จำแค่ key
        property string selectedLogRecordsJson: "{}"
    }

    Timer {
        id: selectLimitWarningTimer
        interval: 2500
        repeat: false
        onTriggered: sidelogs.selectLimitWarningText = ""
    }

    // ===== Theme =====
    property color panelBg: "#111212"
    property color cardBg: "#111A1E"
    property color cardBg2: "#0D1417"
    property color hoverBg: "#152226"
    property color fieldBg: "#111A1E"
    property color borderGreen: "#1B8F77"
    property color textGreen: "#7AE2CF"
    property color textWhite: "#FFFFFF"
    property color textSoft: "#9fb3c8"
    property color textDim: "#666666"
    property color dangerBg: "#3a2020"
    property color dangerBorder: "#6b3333"
    property color selectedBg: "#163A35"

    function pick(obj, keys, fallback) {
        for (var i = 0; i < keys.length; ++i) {
            var k = keys[i]
            if (obj && obj[k] !== undefined && obj[k] !== null && String(obj[k]) !== "")
                return obj[k]
        }
        return fallback
    }

    function safeString(v, fallback) {
        if (v === undefined || v === null)
            return fallback === undefined ? "" : fallback
        return String(v)
    }

    function nowMs() {
        return Date.now()
    }

    function makeTimestamp(r) {
        var ts = pick(r, [
            "Timestamp", "timestamp", "created_at", "createdAt",
            "endLog", "startLog", "datetime", "dateTime", "log_timestamp"
        ], "")

        if (safeString(ts, "") !== "")
            return safeString(ts, "")

        var d = pick(r, ["Date", "date"], "")
        var t = pick(r, ["Time", "time"], "")

        if (safeString(d, "") !== "" || safeString(t, "") !== "")
            return (safeString(d, "") + " " + safeString(t, "")).trim()

        return "-"
    }

    function normalizeRecord(r, idx) {
        var rowId = pick(r, [
            "id", "ID", "logId", "logID",
            "recordId", "recordID", "dbId", "dbID"
        ], "")

        var msg = pick(r, [
            "message", "Message", "Description", "description",
            "text", "Text", "event", "Event",
            "detail", "Detail", "details", "Details", "extra"
        ], "")

        var device = pick(r, [
            "DeviceName", "deviceName", "namedevice", "NameDevice",
            "name", "Name", "stationName", "StationName", "log_name"
        ], "")

        var frequency = pick(r, [
            "IPAddress", "ipAddress", "ip", "IP",
            "address", "Address", "frequency"
        ], "")

        var logKey = pick(r, [
            "Serial", "serial", "serialNo", "SerialNo",
            "LFLSerialNo", "uniqueId", "uniqueID", "key", "log_key"
        ], "")

        var doa = pick(r, ["doa", "doa_value", "doaValue"], "")
        var confidence = pick(r, ["confidence", "conf"], "")
        var heading = pick(r, ["heading", "hdg"], "")
        var lat = pick(r, ["lat", "latitude"], "")
        var lon = pick(r, ["lon", "longitude"], "")

        var detail = safeString(msg, "")

        if (detail === "") {
            var parts = []

            if (safeString(doa, "") !== "")
                parts.push("DOA=" + safeString(doa, ""))

            if (safeString(confidence, "") !== "")
                parts.push("conf=" + safeString(confidence, ""))

            if (safeString(heading, "") !== "")
                parts.push("hdg=" + safeString(heading, ""))

            if (safeString(lat, "") !== "" && safeString(lon, "") !== "")
                parts.push("lat=" + safeString(lat, "") + " lon=" + safeString(lon, ""))

            detail = parts.join("  ")
        }

        if (detail === "")
            detail = "-"

        return {
            id: safeString(rowId, ""),
            no: idx + 1,
            timestamp: makeTimestamp(r),
            device: safeString(device, "-"),
            frequency: safeString(frequency, "-"),
            logKey: safeString(logKey, "-"),
            message: detail,

            doa: safeString(doa, ""),
            confidence: safeString(confidence, ""),
            heading: safeString(heading, ""),
            lat: safeString(lat, ""),
            lon: safeString(lon, "")
        }
    }

    function recordsFromPayload(obj) {
        if (!obj)
            return []

        if (Array.isArray(obj))
            return obj

        if (obj.records && Array.isArray(obj.records))
            return obj.records

        if (obj.payload && Array.isArray(obj.payload))
            return obj.payload

        if (obj.logs && Array.isArray(obj.logs))
            return obj.logs

        if (obj.data && Array.isArray(obj.data))
            return obj.data

        return [obj]
    }

    function makeLogKey(row) {
        if (!row)
            return ""

        // ✅ ใช้ id จาก database ก่อน เพื่อ delete/restore ให้แม่น
        if (row.id !== undefined && row.id !== null && String(row.id) !== "")
            return "ID|" + String(row.id)

        return String(row.timestamp || "") + "|" +
               String(row.device || "") + "|" +
               String(row.frequency || "") + "|" +
               String(row.logKey || "") + "|" +
               String(row.doa || "") + "|" +
               String(row.heading || "") + "|" +
               String(row.lat || "") + "|" +
               String(row.lon || "")
    }

    function cloneSelectedRecord(row) {
        if (!row)
            return null

        return {
            id: safeString(row.id, ""),
            no: Number(row.no || 0),
            timestamp: safeString(row.timestamp, ""),
            device: safeString(row.device, "-"),
            frequency: safeString(row.frequency, "-"),
            logKey: safeString(row.logKey, "-"),
            message: safeString(row.message, ""),

            doa: safeString(row.doa, ""),
            confidence: safeString(row.confidence, ""),
            heading: safeString(row.heading, ""),
            lat: safeString(row.lat, ""),
            lon: safeString(row.lon, "")
        }
    }

    function cacheSelectedRecord(row) {
        var k = makeLogKey(row)
        if (k === "")
            return

        var rec = cloneSelectedRecord(row)
        if (!rec)
            return

        var m = selectedLogRecords || ({})
        m[k] = rec
        selectedLogRecords = m
    }

    function cacheSelectedRowsFromCurrentModel() {
        var m = selectedLogRecords || ({})

        for (var i = 0; i < logModel.count; ++i) {
            var row = logModel.get(i)
            var k = makeLogKey(row)

            if (k !== "" && selectedLogKeys[k] === true) {
                var rec = cloneSelectedRecord(row)
                if (rec)
                    m[k] = rec
            }
        }

        selectedLogRecords = m
    }

    function isLogSelected(row) {
        var k = makeLogKey(row)
        if (k === "")
            return false
        return selectedLogKeys[k] === true
    }

    function refreshSelectedCount() {
        var c = 0
        for (var k in selectedLogKeys) {
            if (selectedLogKeys.hasOwnProperty(k) && selectedLogKeys[k] === true)
                c++
        }
        selectedCount = c
    }

    function showSelectLimitWarning() {
        selectLimitWarningText = "เลือกได้สูงสุด " + maxSelectedLogs + " รายการ ต้องเอาออกก่อนถึงจะเลือกเพิ่มได้"
        selectLimitWarningTimer.restart()
        console.warn("[SideLogsFile] selected limit reached:", selectedCount, "/", maxSelectedLogs)
    }

    function canSelectMore() {
        refreshSelectedCount()
        return selectedCount < maxSelectedLogs
    }

    function saveSelectedState() {
        try {
            sideLogSettings.selectedLogKeysJson = JSON.stringify(selectedLogKeys || ({}))
            sideLogSettings.selectedLogRecordsJson = JSON.stringify(selectedLogRecords || ({}))
        } catch (e) {
            console.warn("[SideLogsFile] saveSelectedState error:", e)
        }
    }

    function restoreSelectedState() {
        try {
            _restoringSelection = true

            var txt = sideLogSettings.selectedLogKeysJson || "{}"
            var obj = JSON.parse(txt)

            selectedLogKeys = obj || ({})

            var recTxt = sideLogSettings.selectedLogRecordsJson || "{}"
            var recObj = JSON.parse(recTxt)

            selectedLogRecords = recObj || ({})
            refreshSelectedCount()
        } catch (e) {
            console.warn("[SideLogsFile] restoreSelectedState error:", e)
            selectedLogKeys = ({})
            selectedLogRecords = ({})
            selectedCount = 0
        } finally {
            _restoringSelection = false
        }
    }

    function callShowLogsOnMap(jsonText) {
        if (krakenmapval && typeof krakenmapval.requestShowDoaLogOnMapJson === "function") {
            krakenmapval.requestShowDoaLogOnMapJson(jsonText)
            return true
        }

        console.log("[SideLogsFile] requestShowDoaLogOnMapJson not ready, retry")

        Qt.callLater(function() {
            if (krakenmapval && typeof krakenmapval.requestShowDoaLogOnMapJson === "function") {
                krakenmapval.requestShowDoaLogOnMapJson(jsonText)
            } else {
                console.log("[SideLogsFile] requestShowDoaLogOnMapJson still not found")
            }
        })

        return false
    }

    // ✅ keepMapView:
    // true  = reload/visibleChanged/completed/delete/clear: วาด overlay แต่ห้าม map ขยับ
    // false = user click row: อนุญาตให้ QMLMap focus/center ได้
    function requestAutoShowSelectedOnMap(keepMapView) {
        if (!autoShowSelectedOnMap)
            return

        var keep = (keepMapView === true)

        if (_mapSendPending) {
            // ถ้าระหว่าง pending มี user click เข้ามา ให้ user action ชนะ
            if (!keep)
                _pendingKeepMapView = false
            return
        }

        _pendingKeepMapView = keep
        _mapSendPending = true

        Qt.callLater(function() {
            _mapSendPending = false
            sendSelectedLogsToMap(_pendingKeepMapView)
        })
    }

    function clearSelection() {
        selectedLogKeys = ({})
        selectedLogRecords = ({})
        selectedCount = 0
        saveSelectedState()

        // clear overlay แต่ไม่ขยับ map
        requestAutoShowSelectedOnMap(true)
    }

    function setLogSelected(row, checked) {
        var k = makeLogKey(row)
        if (k === "")
            return

        var m = selectedLogKeys || ({})
        var r = selectedLogRecords || ({})
        var alreadySelected = (m[k] === true)

        if (checked) {
            // ✅ ถ้ายังไม่เคยเลือก ต้องเช็ก limit ก่อน
            // ✅ ถ้าเลือกอยู่แล้ว ไม่ต้องนับเพิ่ม และไม่ block
            if (!alreadySelected) {
                refreshSelectedCount()

                if (selectedCount >= maxSelectedLogs) {
                    showSelectLimitWarning()
                    return
                }
            }

            m[k] = true

            // ✅ สำคัญ: cache record เต็มไว้ เพื่อกดเลือกข้ามหน้าแล้วยังส่งตัวเก่าด้วย
            var rec = cloneSelectedRecord(row)
            if (rec)
                r[k] = rec
        } else {
            // ✅ ยกเลิกเลือกได้เสมอ แม้ครบ 200 แล้ว
            if (m[k] !== undefined)
                delete m[k]

            if (r[k] !== undefined)
                delete r[k]

            selectLimitWarningText = ""
        }

        selectedLogKeys = m
        selectedLogRecords = r
        refreshSelectedCount()
        saveSelectedState()

        // ✅ ผู้ใช้กดเลือกเอง ส่ง selected ทุกหน้าที่เคย cache ไว้ไป Map
        requestAutoShowSelectedOnMap(false)
    }

    function toggleLogSelected(row) {
        setLogSelected(row, !isLogSelected(row))
    }

    function selectAllFiltered() {
        var m = selectedLogKeys || ({})
        var r = selectedLogRecords || ({})

        refreshSelectedCount()

        var added = 0
        var skippedByLimit = 0

        for (var i = 0; i < filteredLogs.length; ++i) {
            var row = filteredLogs[i]
            var k = makeLogKey(row)

            if (k === "")
                continue

            // ✅ ถ้าเลือกอยู่แล้ว ไม่ต้องนับซ้ำ
            if (m[k] === true)
                continue

            // ✅ จำกัดไม่เกิน maxSelectedLogs
            if ((selectedCount + added) >= maxSelectedLogs) {
                skippedByLimit++
                continue
            }

            m[k] = true

            var rec = cloneSelectedRecord(row)
            if (rec)
                r[k] = rec

            added++
        }

        selectedLogKeys = m
        selectedLogRecords = r
        refreshSelectedCount()
        saveSelectedState()

        if (skippedByLimit > 0)
            showSelectLimitWarning()

        // เลือกหลายรายการพร้อมกัน ไม่ให้ map กระโดดไกล
        requestAutoShowSelectedOnMap(true)
    }

    function clearFilteredSelection() {
        var m = selectedLogKeys || ({})
        var r = selectedLogRecords || ({})

        for (var i = 0; i < filteredLogs.length; ++i) {
            var k = makeLogKey(filteredLogs[i])

            if (k !== "" && m[k] !== undefined)
                delete m[k]

            if (k !== "" && r[k] !== undefined)
                delete r[k]
        }

        selectedLogKeys = m
        selectedLogRecords = r
        refreshSelectedCount()
        saveSelectedState()

        // clear overlay บางส่วน แต่ไม่ขยับ map
        requestAutoShowSelectedOnMap(true)
    }

    function clearLogs(keepSelection) {
        logModel.clear()
        filteredLogs = []
        totalRows = 0
        latestTime = "-"

        if (!keepSelection)
            clearSelection()
        else
            refreshSelectedCount()

        if (logListView)
            logListView.positionViewAtBeginning()
    }

    function refreshStats() {
        // totalRows = จำนวนทั้งหมดใน database หลัง filter/search
        // logModel.count = จำนวน record ที่โหลดมาเฉพาะหน้าปัจจุบัน
        totalRows = totalRowsFromDb > 0 ? totalRowsFromDb : logModel.count
        latestTime = "-"

        if (logModel.count > 0)
            latestTime = logModel.get(0).timestamp
    }

    function appendNormalizedRecord(n) {
        logModel.append({
            id: n.id,
            no: n.no,
            timestamp: n.timestamp,
            device: n.device,
            frequency: n.frequency,
            logKey: n.logKey,
            message: n.message,

            doa: n.doa,
            confidence: n.confidence,
            heading: n.heading,
            lat: n.lat,
            lon: n.lon
        })
    }

    function applyLogsJson(json) {
        if (_applying)
            return

        _applying = true

        try {
            // ✅ โหลดใหม่ แต่ยังจำ selected เดิมไว้
            clearLogs(true)

            var obj = (typeof json === "string") ? JSON.parse(json) : json
            var records = recordsFromPayload(obj)

            // ✅ รับ metadata จาก C++ / DatabaseDF
            if (obj && obj.objectName === "SideLogsFile") {
                var p  = Number(obj.page || 1)
                var ps = Number(obj.pageSize || pageSize)
                var tr = Number(obj.totalRows || 0)
                var tp = Number(obj.totalPages || 1)

                if (!isFinite(p) || p < 1) p = 1
                if (!isFinite(ps) || ps < 1) ps = pageSize
                if (!isFinite(tr) || tr < 0) tr = 0
                if (!isFinite(tp) || tp < 1) tp = 1

                currentPage = Math.floor(p)
                pageSize = Math.floor(ps)
                totalRowsFromDb = Math.floor(tr)
                totalPages = Math.floor(tp)
                serverSearchText = safeString(obj.searchText, serverSearchText)
            } else {
                currentPage = 1
                totalRowsFromDb = records.length
                totalPages = 1
            }

            for (var i = 0; i < records.length; ++i) {
                var n = normalizeRecord(records[i] || {}, i)
                appendNormalizedRecord(n)
            }

            refreshStats()
            rebuildFilteredLogsFromCurrentPage()

            // ✅ หลัง DB load ใหม่ ต้อง restore selected อีกครั้ง
            restoreSelectedState()
            cacheSelectedRowsFromCurrentModel()
            refreshSelectedCount()
            saveSelectedState()

            // ✅ DB reload แล้วส่ง overlay กลับไป map แต่ห้าม map ขยับ
            requestAutoShowSelectedOnMap(true)
        } catch (e) {
            console.warn("[SideLogsFile] applyLogsJson error:", e, json)
        } finally {
            _applying = false
            pageLoading = false
        }
    }

    function appendLogsJson(json) {
        try {
            var obj = (typeof json === "string") ? JSON.parse(json) : json
            var records = recordsFromPayload(obj)


            for (var i = 0; i < records.length; ++i) {
                var n = normalizeRecord(records[i] || {}, logModel.count)
                appendNormalizedRecord(n)
            }

            refreshStats()
            rebuildFilteredLogsFromCurrentPage()
            restoreSelectedState()
            cacheSelectedRowsFromCurrentModel()
            refreshSelectedCount()
            saveSelectedState()

            // append log ใหม่ ไม่ควรทำให้ map กระโดด
            requestAutoShowSelectedOnMap(true)
        } catch (e) {
            console.warn("[SideLogsFile] appendLogsJson error:", e, json)
        }
    }

    function rebuildFilteredLogsFromCurrentPage() {
        // ✅ สำคัญมาก:
        // เมื่อใช้ DB paging แล้ว Search ต้องทำที่ Database เท่านั้น
        // ห้ามเอา searchField.text มากรอง logModel ซ้ำ เพราะ logModel มีแค่ page ปัจจุบัน
        var out = []

        for (var i = 0; i < logModel.count; ++i) {
            var it = logModel.get(i)

            out.push({
                id: it.id,
                no: it.no,
                timestamp: it.timestamp,
                device: it.device,
                frequency: it.frequency,
                logKey: it.logKey,
                message: it.message,

                doa: it.doa,
                confidence: it.confidence,
                heading: it.heading,
                lat: it.lat,
                lon: it.lon
            })
        }

        filteredLogs = out

        if (highlightedFilteredIndex >= filteredLogs.length)
            highlightedFilteredIndex = -1

        refreshSelectedCount()
    }

    function applyFilter() {
        // ✅ เก็บไว้เพื่อ compatibility เท่านั้น
        // DB paging/search ใช้ rebuildFilteredLogsFromCurrentPage() แทน
        rebuildFilteredLogsFromCurrentPage()
    }

    function filteredCount() {
        if (!filteredLogs)
            return 0
        return filteredLogs.length
    }

    function clampPage(p) {
        p = Math.floor(Number(p))
        if (!isFinite(p) || p < 1)
            p = 1

        var tp = Math.floor(Number(totalPages))
        if (!isFinite(tp) || tp < 1)
            tp = 1

        if (p > tp)
            p = tp

        return p
    }

    function requestPage(p, reason) {
        currentPage = clampPage(p)
        highlightedFilteredIndex = -1
        restoreSelectedState()
        requestLogs()
    }

    function searchLogs() {
        // ✅ Search แบบ server-side: ให้ DatabaseDF COUNT + LIMIT/OFFSET ใหม่
        serverSearchText = searchField.text.trim()
        currentPage = 1
        highlightedFilteredIndex = -1
        restoreSelectedState()
        requestLogs()
    }

    function clearSearchAndJumpFirst() {
        searchField.text = ""
        serverSearchText = ""
        currentPage = 1
        highlightedFilteredIndex = -1
        restoreSelectedState()
        requestLogs()
    }

    function jumpFirstLog() {
        requestPage(1, "FIRST")
    }

    function jumpPrevLog() {
        requestPage(currentPage - 1, "PREV")
    }

    function jumpNextLog() {
        requestPage(currentPage + 1, "NEXT")
    }

    function jumpLastLog() {
        requestPage(totalPages, "LAST")
    }

    function requestLogs() {
        if (krakenmapval) {
            pageLoading = true

            if (typeof krakenmapval.getdatabaseToSideSettingDrawerPage === "function") {
                krakenmapval.getdatabaseToSideSettingDrawerPage(
                            "SideLogsFile",
                            currentPage,
                            pageSize,
                            serverSearchText)

                console.log("[SideLogsFile] request page",
                            "page=", currentPage,
                            "pageSize=", pageSize,
                            "search=", serverSearchText)
            } else {
                // ❌ ถ้าเข้าตรงนี้ แปลว่า C++ ยังไม่มี Q_INVOKABLE getdatabaseToSideSettingDrawerPage()
                // Search จะ fallback เป็นหน้า default เท่านั้น ต้องเพิ่ม function ใน iScreenDF.h/.cpp แล้ว rebuild
                pageLoading = false
                console.warn("[SideLogsFile] getdatabaseToSideSettingDrawerPage not found; fallback will not search whole DB")
                krakenmapval.getdatabaseToSideSettingDrawer("SideLogsFile")
            }
        } else {
            pageLoading = false
            console.warn("[SideLogsFile] krakenmapval not available")
        }
    }

    function requestLogsSafe(reason) {
        var t = nowMs()

        // กันเรียกซ้ำถี่เกิน เช่น entering map + open drawer พร้อมกัน
        if ((t - _lastRequestMs) < 300) {
            console.log("[SideLogsFile] skip duplicate request:", reason)
            return
        }

        _lastRequestMs = t

        console.log("[SideLogsFile] reload:", reason)

        restoreSelectedState()
        requestLogs()
    }

    // ✅ ให้ SideSettingsDrawer.qml เรียกตอนกลับเข้า QMLMap.qml
    function reloadAndSendToMap(reason) {
        console.log("[SideLogsFile] reloadAndSendToMap:", reason)

        restoreSelectedState()
        requestLogsSafe(reason || "reloadAndSendToMap")

        // ✅ สำคัญ:
        // ส่ง overlay ที่เลือกอยู่กลับไป map ได้
        // แต่บอก QMLMap.qml ว่าห้าม center/fit/zoom ตอน reload
        requestAutoShowSelectedOnMap(true)
    }

    function reloadWhenPageShown(reason) {
        if (!reloadWhenVisible)
            return

        if (!visible)
            return

        // ✅ ถ้าต้องการให้กลับเข้า QMLMap แล้ว reload จริง ให้เปิดบรรทัดนี้ได้
        // ตอนนี้ปลอดภัยแล้ว เพราะ reloadAndSendToMap() จะส่ง keepMapView:true
        reloadAndSendToMap(reason || "visibleChanged")
    }

    function buildMapRecord(row) {
        var lat = Number(row.lat)
        var lon = Number(row.lon)
        var doa = Number(row.doa)
        var heading = Number(row.heading)

        if (!isFinite(lat) || !isFinite(lon))
            return null

        if (!isFinite(doa))
            doa = 0

        if (!isFinite(heading))
            heading = 0

        return {
            timestamp: String(row.timestamp || ""),
            device: String(row.device || ""),
            frequency: String(row.frequency || ""),
            logKey: String(row.logKey || ""),
            message: String(row.message || ""),

            doa: doa,
            confidence: String(row.confidence || ""),
            heading: heading,
            lat: lat,
            lon: lon
        }
    }

    function sendSelectedLogsToMap(keepMapView) {
        var records = []
        var sentKeys = {}

        // ✅ 1) ส่งจาก cache ก่อน: รวมตัวที่ select จากหน้าก่อนหน้า
        var cached = selectedLogRecords || ({})

        for (var ck in cached) {
            if (!cached.hasOwnProperty(ck))
                continue

            if (selectedLogKeys[ck] !== true)
                continue

            if (sentKeys[ck] === true)
                continue

            var cr = buildMapRecord(cached[ck])
            if (cr) {
                records.push(cr)
                sentKeys[ck] = true
            }
        }

        // ✅ 2) fallback จากหน้าปัจจุบัน เผื่อ cache ยังไม่มี
        for (var i = 0; i < logModel.count; ++i) {
            var row = logModel.get(i)
            var k = makeLogKey(row)

            if (k === "" || selectedLogKeys[k] !== true)
                continue

            if (sentKeys[k] === true)
                continue

            var r = buildMapRecord(row)
            if (r) {
                records.push(r)
                sentKeys[k] = true
                cacheSelectedRecord(row)
            }
        }

        saveSelectedState()

        // ✅ ถ้า records ว่าง จะส่งไป clear overlay บน QMLMap.qml
        var payload = {
            objectName: "ShowDoaLogsOnMap",
            count: records.length,
            records: records,

            // ✅ ตัวนี้ให้ QMLMap.qml ใช้กัน map ขยับมั่ว
            // true  = redraw overlay only
            // false = user selected, allow focus if QMLMap wants
            keepMapView: (keepMapView === true),
            reason: (keepMapView === true) ? "autoReload" : "userSelect"
        }

        var payloadKey = JSON.stringify({
            count: payload.count,
            records: payload.records,
            keepMapView: payload.keepMapView
        })

        // ✅ กันส่ง payload เดิมซ้ำ ๆ ตอน visibleChanged/completed
        if (_lastMapPayloadKey === payloadKey) {
            console.log("[SideLogsFile] skip same map payload")
            return
        }

        _lastMapPayloadKey = payloadKey

        var jsonText = JSON.stringify(payload)

        console.log("[SideLogsFile] show selected logs on map all pages:", jsonText)
        callShowLogsOnMap(jsonText)
    }

    function buildDeleteRecord(row) {
        if (!row)
            return null

        return {
            id: String(row.id || ""),
            timestamp: String(row.timestamp || ""),
            device: String(row.device || ""),
            frequency: String(row.frequency || ""),
            logKey: String(row.logKey || ""),

            doa: String(row.doa || ""),
            confidence: String(row.confidence || ""),
            heading: String(row.heading || ""),
            lat: String(row.lat || ""),
            lon: String(row.lon || ""),

            key: makeLogKey(row)
        }
    }

    function selectedDeleteRecords() {
        var records = []
        var sentKeys = {}

        // ✅ ลบจาก cache ก่อน รวมรายการที่เลือกอยู่คนละหน้า
        var cached = selectedLogRecords || ({})

        for (var ck in cached) {
            if (!cached.hasOwnProperty(ck))
                continue

            if (selectedLogKeys[ck] !== true)
                continue

            if (sentKeys[ck] === true)
                continue

            var cr = buildDeleteRecord(cached[ck])
            if (cr) {
                records.push(cr)
                sentKeys[ck] = true
            }
        }

        // ✅ fallback current page
        for (var i = 0; i < logModel.count; ++i) {
            var row = logModel.get(i)
            var k = makeLogKey(row)

            if (k === "" || selectedLogKeys[k] !== true)
                continue

            if (sentKeys[k] === true)
                continue

            var r = buildDeleteRecord(row)
            if (r) {
                records.push(r)
                sentKeys[k] = true
            }
        }

        return records
    }

    function removeSelectedRowsFromUi() {
        for (var i = logModel.count - 1; i >= 0; --i) {
            var row = logModel.get(i)
            var k = makeLogKey(row)

            if (k !== "" && selectedLogKeys[k] === true)
                logModel.remove(i)
        }

        selectedLogKeys = ({})
        selectedLogRecords = ({})
        selectedCount = 0
        saveSelectedState()

        refreshStats()
        rebuildFilteredLogsFromCurrentPage()

        // ✅ ลบแล้ว clear map overlay แต่ห้าม map ขยับ
        requestAutoShowSelectedOnMap(true)
    }

    function requestDeleteSelectedLogs() {
        var records = selectedDeleteRecords()

        if (records.length <= 0) {
            console.log("[SideLogsFile] no selected logs to delete")
            return
        }

        var payload = {
            objectName: "DeleteDoaLogs",
            count: records.length,
            records: records
        }

        var jsonText = JSON.stringify(payload)

        console.log("[SideLogsFile] request delete selected logs:", jsonText)

        if (krakenmapval && typeof krakenmapval.requestDeleteDoaLogsJson === "function") {
            krakenmapval.requestDeleteDoaLogsJson(jsonText)
            removeSelectedRowsFromUi()
        } else {
            console.log("[SideLogsFile] requestDeleteDoaLogsJson not found")
        }
    }

    Connections {
        target: krakenmapval
        ignoreUnknownSignals: true

        function onSetSideLogsJson(json) {
            console.log("[SideLogsFile] onSetSideLogsJson:", json)
            sidelogs.applyLogsJson(json)
        }

        function onAppendSideLogJson(json) {
            console.log("[SideLogsFile] onAppendSideLogJson:", json)
            sidelogs.appendLogsJson(json)
        }
    }

    onVisibleChanged: {
        if (!visible)
            return

        if (!_firstCompleted)
            return

        Qt.callLater(function() {
            reloadWhenPageShown("visibleChanged")
        })
    }

    Component.onCompleted: {
        _firstCompleted = true

        Qt.callLater(function() {
            reloadAndSendToMap("completed")
        })
    }

    Rectangle {
        anchors.fill: parent
        color: panelBg
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 6
        // ====== Header ======
        Item {
            id: sideLogsHeader
            Layout.fillWidth: true
            Layout.preferredHeight: 48

            Label {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12

                color: "#eeeeee"
                text: "DOA Logs"
                font.pixelSize: 18
                font.bold: true
            }

            Rectangle {
                id: sendDoaMapButton
                width: 115
                height: 30
                radius: height / 2

                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter

                color: sendDoaMapMouse.containsMouse ? "#324152" : "#25303b"
                border.color: sendDoaMapMouse.containsMouse ? textGreen : borderGreen
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Refresh DOA"
                    color: textGreen
                    font.pixelSize: 12
                    font.bold: true
                }

                MouseArea {
                    id: sendDoaMapMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        Qt.callLater(function() {
                            // บังคับส่งซ้ำได้ แม้ payload เดิมเหมือนเดิม
                            sidelogs._lastMapPayloadKey = ""

                            // true = ส่ง selected DOA logs ไป Map ใหม่ แต่ไม่ให้ map center/zoom
                            sidelogs.sendSelectedLogsToMap(true)
                        })
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                radius: 8
                color: fieldBg
                border.color: searchField.activeFocus ? textGreen : borderGreen
                border.width: 1
                clip: true

                TextInput {
                    id: searchField
                    anchors.fill: parent
                    anchors.leftMargin: 9
                    anchors.rightMargin: 9
                    verticalAlignment: Text.AlignVCenter

                    font.pixelSize: 13
                    color: textGreen
                    selectedTextColor: "#111212"
                    selectionColor: textGreen
                    cursorVisible: activeFocus
                    clip: true
                    selectByMouse: true
                    text: ""

                    onTextChanged: {
                        // ยังไม่ยิง DB ทันทีตอนพิมพ์ เพื่อไม่ให้ query ถี่เกิน
                        // กด Search หรือ Enter เพื่อโหลดหน้า 1 จาก database
                        sidelogs.highlightedFilteredIndex = -1
                    }

                    Keys.onReturnPressed: sidelogs.searchLogs()
                    Keys.onEnterPressed: sidelogs.searchLogs()
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 9
                    anchors.right: parent.right
                    anchors.rightMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Search DOA, freq, key, lat/lon..."
                    color: textDim
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    visible: searchField.text.length === 0 && !searchField.activeFocus
                }
            }

            Button {
                id: searchButton
                Layout.preferredWidth: 74
                Layout.preferredHeight: 32
                text: "Search"

                onClicked: sidelogs.searchLogs()

                background: Rectangle {
                    radius: 8
                    color: searchButton.pressed ? Qt.darker("#169976", 1.4) : "#169976"
                    border.color: "#169976"
                    border.width: 1
                }

                contentItem: Text {
                    text: searchButton.text
                    color: "#212121"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                id: refreshButton
                Layout.preferredWidth: 68
                Layout.preferredHeight: 32
                text: "Load"

                // ✅ Load button เป็น reload จึงไม่ขยับ map
                onClicked: sidelogs.reloadAndSendToMap("Load button")

                background: Rectangle {
                    radius: 8
                    color: refreshButton.pressed ? Qt.darker("#169976", 1.4) : "#169976"
                }

                contentItem: Text {
                    text: refreshButton.text
                    color: "#212121"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            spacing: 6

            Button {
                id: firstButton
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                text: "FIRST"
                enabled: sidelogs.currentPage > 1 && !sidelogs.pageLoading
                opacity: enabled ? 1.0 : 0.35

                onClicked: sidelogs.jumpFirstLog()

                background: Rectangle {
                    radius: 8
                    color: firstButton.pressed ? Qt.darker("#25303b", 1.4) : "#25303b"
                    border.color: borderGreen
                    border.width: 1
                }

                contentItem: Text {
                    text: firstButton.text
                    color: textGreen
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                id: prevButton
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                text: "PREV"
                enabled: sidelogs.currentPage > 1 && !sidelogs.pageLoading
                opacity: enabled ? 1.0 : 0.35

                onClicked: sidelogs.jumpPrevLog()

                background: Rectangle {
                    radius: 8
                    color: prevButton.pressed ? Qt.darker("#25303b", 1.4) : "#25303b"
                    border.color: borderGreen
                    border.width: 1
                }

                contentItem: Text {
                    text: prevButton.text
                    color: textGreen
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                id: nextButton
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                text: "NEXT"
                enabled: sidelogs.currentPage < sidelogs.totalPages && !sidelogs.pageLoading
                opacity: enabled ? 1.0 : 0.35

                onClicked: sidelogs.jumpNextLog()

                background: Rectangle {
                    radius: 8
                    color: nextButton.pressed ? Qt.darker("#25303b", 1.4) : "#25303b"
                    border.color: borderGreen
                    border.width: 1
                }

                contentItem: Text {
                    text: nextButton.text
                    color: textGreen
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                id: lastButton
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                text: "LAST"
                enabled: sidelogs.currentPage < sidelogs.totalPages && !sidelogs.pageLoading
                opacity: enabled ? 1.0 : 0.35

                onClicked: sidelogs.jumpLastLog()

                background: Rectangle {
                    radius: 8
                    color: lastButton.pressed ? Qt.darker("#25303b", 1.4) : "#25303b"
                    border.color: borderGreen
                    border.width: 1
                }

                contentItem: Text {
                    text: lastButton.text
                    color: textGreen
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            spacing: 6

            Rectangle {
                Layout.preferredWidth: 92
                Layout.preferredHeight: 26
                radius: 8
                color: fieldBg
                border.color: borderGreen
                border.width: 1
                clip: true

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    text: "Page " + currentPage + "/" + totalPages
                          + "  |  " + filteredLogs.length + "/" + totalRowsFromDb
                    color: textGreen
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                radius: 8
                color: fieldBg
                border.color: borderGreen
                border.width: 1
                clip: true

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    text: selectLimitWarningText.length > 0
                          ? selectLimitWarningText
                          : (selectedCount > 0
                             ? ("Selected: " + selectedCount + "/" + maxSelectedLogs + "  |  Auto Map ON")
                             : ("Select DOA logs 0/" + maxSelectedLogs))
                    color: selectLimitWarningText.length > 0
                           ? "#FFCF4C"
                           : (selectedCount > 0 ? textGreen : textSoft)
                    font.pixelSize: 11
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignLeft
                    elide: Text.ElideRight
                }
            }

            Button {
                id: clearFilterButton
                Layout.preferredWidth: 56
                Layout.preferredHeight: 26
                text: "Clear"

                onClicked: sidelogs.clearSearchAndJumpFirst()

                background: Rectangle {
                    radius: 8
                    color: clearFilterButton.pressed ? Qt.darker("#169976", 1.4) : "#169976"
                }

                contentItem: Text {
                    text: clearFilterButton.text
                    color: "#212121"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            spacing: 6

            Button {
                id: selectFilteredButton
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                text: "All Filtered"
                enabled: sidelogs.selectedCount < sidelogs.maxSelectedLogs
                opacity: enabled ? 1.0 : 0.35

                onClicked: sidelogs.selectAllFiltered()

                background: Rectangle {
                    radius: 8
                    color: selectFilteredButton.pressed ? Qt.darker("#169976", 1.4) : "#169976"
                }

                contentItem: Text {
                    text: selectFilteredButton.text
                    color: "#212121"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                id: clearFilteredButton
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                text: "Clear Selected"

                onClicked: sidelogs.clearFilteredSelection()

                background: Rectangle {
                    radius: 8
                    color: clearFilteredButton.pressed ? Qt.darker("#25303b", 1.4) : "#25303b"
                    border.color: borderGreen
                    border.width: 1
                }

                contentItem: Text {
                    text: clearFilteredButton.text
                    color: textGreen
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: cardBg
            border.color: borderGreen
            border.width: 1
            clip: true

            ListView {
                id: logListView
                anchors.fill: parent
                anchors.margins: 6
                spacing: 6
                clip: true
                model: sidelogs.filteredLogs
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: Math.max(500, height * 2)

                delegate: Rectangle {
                    id: rowCard
                    width: logListView.width
                    implicitHeight: Math.max(70, rowContent.implicitHeight + 14)
                    radius: 8

                    property var row: modelData
                    property bool selected: sidelogs.isLogSelected(row)
                    property bool highlighted: sidelogs.highlightedFilteredIndex === index

                    color: selected ? selectedBg : (highlighted ? "#20313D" : (mouseArea.containsMouse ? hoverBg : cardBg2))
                    border.color: selected ? textGreen : (highlighted ? "#82CFFF" : (mouseArea.containsMouse ? textGreen : borderGreen))
                    border.width: (selected || highlighted) ? 2 : 1

                    ColumnLayout {
                        id: rowContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Rectangle {
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                radius: 4
                                color: rowCard.selected ? textGreen : "#111A1E"
                                border.color: rowCard.selected ? textGreen : "#4A5A5D"
                                border.width: 1
                                Layout.alignment: Qt.AlignVCenter

                                Text {
                                    anchors.centerIn: parent
                                    text: rowCard.selected ? "✓" : ""
                                    color: "#111212"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            Text {
                                text: row.device
                                color: textWhite
                                font.pixelSize: 13
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: row.timestamp
                                color: textGreen
                                font.pixelSize: 10
                                font.family: "Monospace"
                                elide: Text.ElideRight
                                Layout.preferredWidth: 116
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: row.frequency + (row.logKey !== "-" ? "  |  " + row.logKey : "")
                            color: textGreen
                            font.pixelSize: 11
                            font.family: "Monospace"
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: row.message
                            color: "#dce8f5"
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true

                        onClicked: {
                            sidelogs.highlightedFilteredIndex = index
                            logListView.currentIndex = index

                            // ✅ กดเลือกแล้ว show map ทันที
                            // setLogSelected() จะส่ง keepMapView:false
                            sidelogs.toggleLogSelected(row)
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            Column {
                anchors.centerIn: parent
                visible: sidelogs.filteredLogs.length === 0
                spacing: 6

                Text {
                    text: logModel.count === 0 ? "No log data" : "No matching log"
                    color: textWhite
                    font.pixelSize: 14
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: logModel.count === 0
                          ? "Press Load to refresh records"
                          : "Try changing search text"
                    color: textGreen
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: selectLimitWarningText.length > 0
                      ? selectLimitWarningText
                      : (selectedCount > 0
                         ? ("Selected " + selectedCount + "/" + maxSelectedLogs + "  |  Click row to show/hide on map")
                         : ("Select DOA logs 0/" + maxSelectedLogs))
                color: selectLimitWarningText.length > 0
                       ? "#FFCF4C"
                       : (selectedCount > 0 ? textGreen : textSoft)
                font.pixelSize: 11
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            Button {
                id: deleteSelectedButton
                Layout.preferredWidth: 96
                Layout.preferredHeight: 28
                text: "Delete"
                enabled: sidelogs.selectedCount > 0
                opacity: enabled ? 1.0 : 0.35

                onClicked: sidelogs.requestDeleteSelectedLogs()

                background: Rectangle {
                    radius: 8
                    color: deleteSelectedButton.pressed ? Qt.darker(dangerBg, 1.4) : dangerBg
                    border.color: dangerBorder
                    border.width: 1
                }

                contentItem: Text {
                    text: deleteSelectedButton.text
                    color: "#ffdada"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                id: clearUiButton
                Layout.preferredWidth: 76
                Layout.preferredHeight: 28
                text: "Clear UI"

                onClicked: sidelogs.clearLogs(false)

                background: Rectangle {
                    radius: 8
                    color: clearUiButton.pressed ? Qt.darker(dangerBg, 1.4) : dangerBg
                    border.color: dangerBorder
                    border.width: 1
                }

                contentItem: Text {
                    text: clearUiButton.text
                    color: "#ffdada"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    ListModel {
        id: logModel
    }
}
