// DoaHistoryViewer.qml (FULL FILE)
// ✅ FIX: TX emit no longer duplicates
//    - Coalesce ALL txModel signals into ONE emit per event-loop (Qt.callLater)
//    - Hard de-dupe by modelKey (lat/lon/rms/updatedMs) => same TX point never re-sent
// ✅ FIX: use viewer.krakenmapval (injected) ONLY (no global Krakenmapval)
// ✅ TX PANEL + coord toggle (lat/lon vs MGRS) persisted via Settings
// ✅ NOTE: MapViewer should inject:
//    doaHistoryViewer.txModel = txHistoryModel
//    doaHistoryViewer.krakenmapval = Krakenmapval
//    doaHistoryViewer.rfCache = rfCache
//    doaHistoryViewer.loggingEnabled = true for the active overlay only
import QtQuick 2.15
import Qt.labs.settings 1.1
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

Rectangle {
    id: viewer
    width: 530
    height: 440
    radius: 18
    color: "#0B1216"
    border.width: 1
    border.color: "#2A3A44"
    opacity: active ? 0.86 : 0.62

    property bool active: false
    property bool daqLocked: false

    // ✅ ปิด MouseArea wake-cover ไม่ให้บังการ scroll ของ TX/DOA Monitor
    property bool useWakeCover: false

    // ✅ only ONE instance should log/send (set from MapViewer for the active overlay)
    property bool loggingEnabled: true

    // external models/refs
    property var txModel: null
    property var krakenmapval: null
    property var rfCache: null

    // ✅ TX PANEL click -> MapViewer / QMLMap marker
    signal txLogClicked(string txKey, int rowIndex, real lat, real lon, real rms, real updatedMs)

    // ✅ MARK OFF / CLEAR / click same row again -> clear marker on MapViewer
    signal txLogClearRequested()

    // ✅ selected TX log highlight in TX PANEL
    property string selectedTxLogKey: ""

    // ✅ true = click TX log then mark map, false = click log but do not mark map
    property bool txMarkEnabled: true

    function makeTxLogKey(lat, lon, updatedMs) {
        var la = Number(lat)
        var lo = Number(lon)
        var ms = Number(updatedMs || 0)

        if (!isFinite(la) || !isFinite(lo))
            return ""

        if (!isFinite(ms))
            ms = 0

        return la.toFixed(7) + "|" + lo.toFixed(7) + "|" + Math.floor(ms)
    }

    function markTxLogFromPanel(rowIndex, lat, lon, rms, updatedMs) {
        var la = Number(lat)
        var lo = Number(lon)
        var rr = Number(rms || 0)
        var ms = Number(updatedMs || 0)

        if (!isFinite(la) || !isFinite(lo)) {
            console.log("[TX PANEL] invalid tx log lat/lon", lat, lon)
            return
        }

        if (!isFinite(rr)) rr = 0
        if (!isFinite(ms)) ms = 0

        var k = makeTxLogKey(la, lo, ms)

        // ✅ MARK OFF: do not mark on map, and clear current selected marker
        if (!viewer.txMarkEnabled) {
            selectedTxLogKey = ""
            txLogClearRequested()
            console.log("[TX PANEL] MARK OFF, skip map mark")
            return
        }

        // ✅ Click same TX row again = unmark / clear marker
        if (selectedTxLogKey === k) {
            selectedTxLogKey = ""
            txLogClearRequested()
            console.log("[TX PANEL] unmark selected tx log")
            return
        }

        selectedTxLogKey = k

        console.log("[TX PANEL] clicked tx log",
                    "row=", rowIndex,
                    "lat=", la,
                    "lon=", lo,
                    "rms=", rr,
                    "updatedMs=", ms)

        viewer.triggerFadeIn()
        txLogClicked(k, rowIndex, la, lo, rr, ms)
    }

    property alias logger: logger

    // ✅ DOA log model for UI (append-only)
    ListModel { id: doaLogModel }
    property int maxLogItems: 100

    // ✅ selected DOA logs
    property int selectedDoaLogCount: 0

    // ✅ AUTO SAVE DOA LOG (runtime only: ไม่จำค่าเมื่อปิด/เปิดโปรแกรมใหม่)
    property bool doaAutoSaveEnabled: false

    // ✅ กัน Auto Save ถี่เกิน: รวมเป็น batch แล้วส่งตาม cooldown
    property int doaAutoSaveCooldownMs: 2000
    property bool _doaAutoSaveSending: false
    property real _lastDoaAutoSaveMs: 0
    property var _doaAutoSaveQueue: []
    property var _doaAutoSaveSeenKeys: ({})

    // ✅ save status UI
    property string doaSaveStatusText: "Ready"
    property string doaSaveStatusColor: "#A9C1CC"

    Timer {
        id: doaSaveStatusTimer
        interval: 3500
        repeat: false
        onTriggered: {
            viewer.doaSaveStatusText = "Ready"
            viewer.doaSaveStatusColor = "#A9C1CC"
        }
    }

    Timer {
        id: doaAutoSaveTimer
        interval: viewer.doaAutoSaveCooldownMs
        repeat: false

        onTriggered: {
            viewer.flushAutoSaveQueue()
        }
    }

    function setDoaSaveStatus(text, color) {
        doaSaveStatusText = text
        doaSaveStatusColor = color
        doaSaveStatusTimer.restart()
    }

    function refreshSelectedDoaLogCount() {
        var c = 0
        for (var i = 0; i < doaLogModel.count; ++i) {
            if (doaLogModel.get(i).selected === true)
                c++
        }
        selectedDoaLogCount = c
    }

    function setDoaLogSelected(rowIndex, checked) {
        if (rowIndex < 0 || rowIndex >= doaLogModel.count)
            return

        if (doaLogModel.get(rowIndex).sent === true && checked === true) {
            setDoaSaveStatus("This log already saved", "#FFCF4C")
        }

        doaLogModel.setProperty(rowIndex, "selected", checked)
        refreshSelectedDoaLogCount()
    }

    function setAllDoaLogSelected(checked) {
        for (var i = 0; i < doaLogModel.count; ++i) {
            doaLogModel.setProperty(i, "selected", checked)
        }
        refreshSelectedDoaLogCount()

        if (checked)
            setDoaSaveStatus("Selected all logs", "#00FFAA")
        else
            setDoaSaveStatus("Selection cleared", "#A9C1CC")
    }

    function makeDoaLogUniqueKey(row) {
        return [
            String(row.timestamp || "").trim(),
            String(row.name || "").trim(),
            String(row.frequency || "").trim(),
            String(row.doaValue || "").trim(),
            String(row.key || "").trim(),
            String(row.confidence || "").trim(),
            String(row.heading || "").trim(),
            String(row.lat || "").trim(),
            String(row.lon || "").trim()
        ].join("|")
    }

    function saveSelectedDoaLogsToCpp() {
        if (!viewer.krakenmapval) {
            console.log("[DOA SAVE] krakenmapval is null")
            viewer.setDoaSaveStatus("krakenmapval is null", "#FF6B6B")
            return
        }

        var records = []
        var seenKeys = []
        var markIndexes = []

        var selectedCount = 0
        var duplicateCount = 0
        var alreadySentCount = 0

        for (var i = 0; i < doaLogModel.count; ++i) {
            var row = doaLogModel.get(i)

            if (row.selected !== true)
                continue

            selectedCount++

            if (row.sent === true) {
                alreadySentCount++
                continue
            }

            var uniqueKey = makeDoaLogUniqueKey(row)

            // กันซ้ำเฉพาะรอบที่กด SAVE นี้
            if (seenKeys.indexOf(uniqueKey) >= 0) {
                duplicateCount++
                continue
            }

            seenKeys.push(uniqueKey)

            records.push({
                timestamp: String(row.timestamp || ""),
                name: String(row.name || ""),
                frequency: String(row.frequency || ""),
                doa: String(row.doaValue || ""),
                extra: String(row.extra || ""),

                key: String(row.key || ""),
                confidence: String(row.confidence || ""),
                heading: String(row.heading || ""),
                lat: String(row.lat || ""),
                lon: String(row.lon || "")
            })

            markIndexes.push(i)
        }

        if (records.length <= 0) {
            console.log("[DOA SAVE] no new unique logs to send",
                        "selected =", selectedCount,
                        "duplicate =", duplicateCount,
                        "alreadySent =", alreadySentCount)

            if (selectedCount <= 0) {
                viewer.setDoaSaveStatus("Please select log first", "#FFCF4C")
            } else if (alreadySentCount > 0) {
                viewer.setDoaSaveStatus("Already saved", "#FFCF4C")
            } else {
                viewer.setDoaSaveStatus("No new log to save", "#FFCF4C")
            }

            return
        }

        var payload = {
            objectName: "DoaLogSaveSelectedRows",
            count: records.length,
            selectedCount: selectedCount,
            duplicateCount: duplicateCount,
            alreadySentCount: alreadySentCount,
            records: records
        }

        var jsonText = JSON.stringify(payload)

        console.log("[DOA SAVE] send unique selected logs:", jsonText)

        if (typeof viewer.krakenmapval.requestSaveDoaLogSelectedJson === "function") {
            viewer.krakenmapval.requestSaveDoaLogSelectedJson(jsonText)

            for (var m = 0; m < markIndexes.length; ++m) {
                doaLogModel.setProperty(markIndexes[m], "sent", true)
                doaLogModel.setProperty(markIndexes[m], "selected", false)
                doaLogModel.setProperty(markIndexes[m], "saveStatus", "SAVED")
            }

            viewer.refreshSelectedDoaLogCount()

            var statusMsg = "Saved " + records.length + " log(s)"
            if (duplicateCount > 0 || alreadySentCount > 0) {
                statusMsg += " | skip dup:" + duplicateCount + " saved:" + alreadySentCount
            }
            viewer.setDoaSaveStatus(statusMsg, "#00FFAA")
        } else {
            console.log("[DOA SAVE] requestSaveDoaLogSelectedJson not found")
            viewer.setDoaSaveStatus("Save function not found", "#FF6B6B")
        }
    }

    function queueAutoSaveDoaLog(row) {
        if (!viewer.doaAutoSaveEnabled)
            return

        if (!row)
            return

        if (row.sent === true)
            return

        var uniqueKey = viewer.makeDoaLogUniqueKey(row)

        if (!uniqueKey.length)
            return

        // ✅ กันซ้ำใน queue / กันส่ง log เดิมซ้ำระหว่างเปิดใช้งาน AUTO
        if (viewer._doaAutoSaveSeenKeys[uniqueKey] === true)
            return

        var seen = viewer._doaAutoSaveSeenKeys || ({})
        seen[uniqueKey] = true
        viewer._doaAutoSaveSeenKeys = seen

        var q = viewer._doaAutoSaveQueue || []

        q.push({
            uniqueKey: uniqueKey,

            timestamp: String(row.timestamp || ""),
            name: String(row.name || ""),
            frequency: String(row.frequency || ""),
            doa: String(row.doaValue || ""),
            extra: String(row.extra || ""),

            key: String(row.key || ""),
            confidence: String(row.confidence || ""),
            heading: String(row.heading || ""),
            lat: String(row.lat || ""),
            lon: String(row.lon || "")
        })

        viewer._doaAutoSaveQueue = q

        // ✅ เริ่ม timer แค่ครั้งแรก ไม่ restart ทุก log
        // เพื่อให้มี log ต่อเนื่องก็ยัง save เป็น batch ได้
        if (!doaAutoSaveTimer.running) {
            doaAutoSaveTimer.interval = viewer.doaAutoSaveCooldownMs
            doaAutoSaveTimer.start()
        }

        viewer.setDoaSaveStatus("Auto queued " + q.length + " log(s)", "#FFCF4C")
    }

    function flushAutoSaveQueue() {
        if (!viewer.doaAutoSaveEnabled)
            return

        if (!viewer.krakenmapval) {
            console.log("[DOA AUTO SAVE] krakenmapval is null")
            viewer.setDoaSaveStatus("Auto save: krakenmapval null", "#FF6B6B")
            return
        }

        if (viewer._doaAutoSaveSending)
            return

        var q = viewer._doaAutoSaveQueue || []

        if (q.length <= 0)
            return

        var now = Date.now()
        var diff = now - Number(viewer._lastDoaAutoSaveMs || 0)

        if (diff < viewer.doaAutoSaveCooldownMs) {
            doaAutoSaveTimer.interval = Math.max(200, viewer.doaAutoSaveCooldownMs - diff)
            doaAutoSaveTimer.restart()
            return
        }

        // ✅ จำกัด batch ต่อครั้ง กัน payload ใหญ่/ถี่เกิน
        var maxBatch = 20
        var records = q.splice(0, maxBatch)
        viewer._doaAutoSaveQueue = q

        var payload = {
            objectName: "DoaLogSaveSelectedRows",
            count: records.length,
            selectedCount: records.length,
            duplicateCount: 0,
            alreadySentCount: 0,
            autoSave: true,
            records: records
        }

        var jsonText = JSON.stringify(payload)

        console.log("[DOA AUTO SAVE] send batch:", records.length, jsonText)

        if (typeof viewer.krakenmapval.requestSaveDoaLogSelectedJson === "function") {
            viewer._doaAutoSaveSending = true
            viewer.krakenmapval.requestSaveDoaLogSelectedJson(jsonText)
            viewer._lastDoaAutoSaveMs = Date.now()
            viewer._doaAutoSaveSending = false

            // ✅ mark sent ใน model ตาม uniqueKey ที่ส่งไปแล้ว
            for (var i = 0; i < doaLogModel.count; ++i) {
                var row = doaLogModel.get(i)
                var k = viewer.makeDoaLogUniqueKey(row)

                for (var j = 0; j < records.length; ++j) {
                    if (records[j].uniqueKey === k) {
                        doaLogModel.setProperty(i, "sent", true)
                        doaLogModel.setProperty(i, "selected", false)
                        doaLogModel.setProperty(i, "saveStatus", "AUTO")
                        break
                    }
                }
            }

            viewer.refreshSelectedDoaLogCount()
            viewer.setDoaSaveStatus("Auto saved " + records.length + " log(s)", "#00FFAA")

            // ✅ ถ้ายังมีคิวเหลือ ส่งรอบถัดไปตาม cooldown
            if (viewer._doaAutoSaveQueue.length > 0) {
                doaAutoSaveTimer.interval = viewer.doaAutoSaveCooldownMs
                doaAutoSaveTimer.restart()
            }
        } else {
            console.log("[DOA AUTO SAVE] requestSaveDoaLogSelectedJson not found")
            viewer.setDoaSaveStatus("Auto save function not found", "#FF6B6B")
        }
    }

    function clearAutoSaveQueue() {
        viewer._doaAutoSaveQueue = []
        viewer._doaAutoSaveSeenKeys = ({})
        viewer._doaAutoSaveSending = false
        viewer._lastDoaAutoSaveMs = 0
        doaAutoSaveTimer.stop()
    }

    // ============================================================
    // TX Coordinate display mode (persist)
    // ============================================================
    // 0 = lat/lon, 1 = MGRS
    property int txCoordMode: 1

    Settings {
        id: viewSettings
        category: "DoaHistoryViewer"
        property int txCoordMode: 1
        onTxCoordModeChanged: viewer.txCoordMode = txCoordMode
    }

    // ============================================================
    // ✅ TX emit coalesce + hard de-dupe
    // ============================================================
    property bool   _txEmitPending: false
    property string _lastTxModelKey: ""
    property real   _lastTxEmitAtMs: 0
    property string _lastTxDebug: ""

    function scheduleEmitLatestTx() {
        if (viewer._txEmitPending) return
        viewer._txEmitPending = true

        Qt.callLater(function() {
            viewer._txEmitPending = false
            viewer.emitLatestTxToKrakenmapval()
        })
    }

    onTxModelChanged: {
        viewer._lastTxModelKey = ""
        viewer._lastTxEmitAtMs = 0
        viewer.scheduleEmitLatestTx()
    }

    // ============================================================
    // MGRS (WGS84) helper
    // ============================================================
    function _mgrsPad(n, width) {
        var s = String(Math.floor(Math.abs(n)))
        while (s.length < width) s = "0" + s
        return s
    }

    function _mgrsLatitudeBandLetter(latDeg) {
        if (!isFinite(latDeg)) return "Z"
        if (latDeg <= -80) return "C"
        if (latDeg >=  84) return "X"

        var bands = "CDEFGHJKLMNPQRSTUVWX"
        var idx = Math.floor((latDeg + 80) / 8)
        if (idx < 0) idx = 0
        if (idx > 19) idx = 19
        return bands.charAt(idx)
    }

    function _mgrsFixZone(latDeg, lonDeg, zone) {
        if (latDeg >= 56 && latDeg < 64 && lonDeg >= 3 && lonDeg < 12) return 32

        if (latDeg >= 72 && latDeg < 84) {
            if      (lonDeg >= 0  && lonDeg < 9 )  return 31
            else if (lonDeg >= 9  && lonDeg < 21)  return 33
            else if (lonDeg >= 21 && lonDeg < 33)  return 35
            else if (lonDeg >= 33 && lonDeg < 42)  return 37
        }

        return zone
    }

    function _latLonToUtm(latDeg, lonDeg) {
        var a = 6378137.0
        var f = 1.0 / 298.257223563
        var e2 = f * (2 - f)
        var ep2 = e2 / (1 - e2)
        var k0 = 0.9996

        var lat = latDeg * Math.PI / 180.0
        var lon = lonDeg * Math.PI / 180.0

        var zone = Math.floor((lonDeg + 180.0) / 6.0) + 1
        zone = _mgrsFixZone(latDeg, lonDeg, zone)

        var lon0Deg = (zone - 1) * 6 - 180 + 3
        var lon0 = lon0Deg * Math.PI / 180.0

        var sinLat = Math.sin(lat)
        var cosLat = Math.cos(lat)
        var tanLat = Math.tan(lat)

        var N = a / Math.sqrt(1 - e2 * sinLat * sinLat)
        var T = tanLat * tanLat
        var C = ep2 * cosLat * cosLat
        var A = cosLat * (lon - lon0)

        var e4 = e2 * e2
        var e6 = e4 * e2

        var M = a * ((1 - e2/4 - 3*e4/64 - 5*e6/256) * lat
                     - (3*e2/8 + 3*e4/32 + 45*e6/1024) * Math.sin(2*lat)
                     + (15*e4/256 + 45*e6/1024) * Math.sin(4*lat)
                     - (35*e6/3072) * Math.sin(6*lat))

        var easting = k0 * N * (A + (1 - T + C) * Math.pow(A,3)/6
                                + (5 - 18*T + T*T + 72*C - 58*ep2) * Math.pow(A,5)/120) + 500000.0

        var northing = k0 * (M + N * tanLat * (A*A/2
                                + (5 - T + 9*C + 4*C*C) * Math.pow(A,4)/24
                                + (61 - 58*T + T*T + 600*C - 330*ep2) * Math.pow(A,6)/720))

        if (latDeg < 0)
            northing += 10000000.0

        return {
            zone: zone,
            easting: easting,
            northing: northing,
            latDeg: latDeg,
            lonDeg: lonDeg
        }
    }

    function _mgrs100kSetForZone(zone) {
        var set = zone % 3
        if (set === 0) set = 3
        return set
    }

    function _mgrs100kColumnLetter(zone, easting) {
        var set = _mgrs100kSetForZone(zone)
        var colSets = { 1: "ABCDEFGH", 2: "JKLMNPQR", 3: "STUVWXYZ" }
        var cols = colSets[set]
        var col = Math.floor(easting / 100000.0)

        if (col < 0) col = 0
        if (col > 8) col = 8

        var idx = (col - 1) % 8
        if (idx < 0) idx = 0

        return cols.charAt(idx)
    }

    function _mgrs100kRowLetter(zone, northing) {
        var rowSets = {
            1: "ABCDEFGHJKLMNPQRSTUV",
            2: "FGHJKLMNPQRSTUVABCDE"
        }

        var set = (zone % 2 === 0) ? 2 : 1
        var rows = rowSets[set]
        var row = Math.floor(northing / 100000.0)

        if (row < 0) row = 0

        var idx = row % 20
        return rows.charAt(idx)
    }

    // digits: 1..5 (1=10km,2=1km,3=100m,4=10m,5=1m)
    function latLonToMgrs(latDeg, lonDeg, digits) {
        var lat = Number(latDeg)
        var lon = Number(lonDeg)
        if (!isFinite(lat) || !isFinite(lon)) return "-"

        var d = (digits === undefined) ? 5 : Math.floor(Number(digits))
        if (d < 1) d = 1
        if (d > 5) d = 5

        var utm = _latLonToUtm(lat, lon)
        var zone = utm.zone
        var band = _mgrsLatitudeBandLetter(lat)

        var e = utm.easting
        var n = utm.northing

        var colL = _mgrs100kColumnLetter(zone, e)
        var rowL = _mgrs100kRowLetter(zone, n)

        var eIn = Math.floor(e % 100000.0)
        var nIn = Math.floor(n % 100000.0)

        if (eIn < 0) eIn += 100000
        if (nIn < 0) nIn += 100000

        var div = Math.pow(10, 5 - d)
        var eRed = Math.floor(eIn / div)
        var nRed = Math.floor(nIn / div)

        var eStr = _mgrsPad(eRed, d)
        var nStr = _mgrsPad(nRed, d)

        return String(zone) + band + " " + colL + rowL + " " + eStr + " " + nStr
    }

    // ============================================================
    // TX -> C++ emit
    // ============================================================
    function emitLatestTxToKrakenmapval() {
        if (!txModel) return

        if (!viewer.krakenmapval) {
            console.log("[TX] krakenmapval is null (not injected)")
            return
        }

        if (!viewer.visible || !viewer.loggingEnabled)
            return

        if (txModel.count !== undefined && txModel.count <= 0)
            return

        var m = txModel.get ? txModel.get(0) : null
        if (!m) return

        var lat = Number(m.lat)
        var lon = Number(m.lon)
        var rms = Number(m.rms || 0)
        var updatedMs = Number(m.updatedMs || 0)

        if (!isFinite(lat) || !isFinite(lon))
            return

        var modelKey = lat.toFixed(6) + "," + lon.toFixed(6)
                     + "|rms=" + Math.round(rms)
                     + "|ms=" + Math.floor(updatedMs)

        if (modelKey === viewer._lastTxModelKey)
            return

        viewer._lastTxModelKey = modelKey

        var fHz = 0
        if (logger && typeof logger._getRfFreqHzNow === "function")
            fHz = Number(logger._getRfFreqHzNow())

        var dStr = String(logger ? logger.lastDate : "")
        var tStr = String(logger ? logger.lastTime : "")

        function _pad2(n) {
            n = Math.floor(Number(n))
            return (n < 10 ? ("0" + n) : ("" + n))
        }

        function _fmtDate(ms) {
            var d = new Date(Number(ms))
            var mon = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"][d.getMonth()]
            return _pad2(d.getDate()) + " " + mon + " " + d.getFullYear()
        }

        function _fmtTime(ms) {
            var d = new Date(Number(ms))
            return _pad2(d.getHours()) + ":" + _pad2(d.getMinutes()) + ":" + _pad2(d.getSeconds())
        }

        var baseMs = (isFinite(updatedMs) && updatedMs > 0) ? updatedMs : Date.now()

        if (!dStr.length) dStr = _fmtDate(baseMs)
        if (!tStr.length) tStr = _fmtTime(baseMs)

        var mgrs = latLonToMgrs(lat, lon, 5)

        if (typeof viewer.krakenmapval.onTxSnapshotUpdated === "function") {
            viewer.krakenmapval.onTxSnapshotUpdated(
                lat, lon, rms, fHz, dStr, tStr, updatedMs, mgrs
            )
        } else {
            console.log("[TX] krakenmapval.onTxSnapshotUpdated not found")
        }
    }

    Connections {
        target: txModel
        ignoreUnknownSignals: true

        function onRowsInserted(parent, first, last) {
            viewer.scheduleEmitLatestTx()
        }

        function onModelReset() {
            viewer.scheduleEmitLatestTx()
        }

        function onCountChanged() {
            viewer.scheduleEmitLatestTx()
        }
    }

    // ============================================================
    // Visual effects
    // ============================================================
    layer.enabled: true
    layer.effect: DropShadow {
        color: "#00E5FF33"
        radius: 22
        samples: 48
        verticalOffset: 0
        horizontalOffset: 0
    }

    Rectangle {
        anchors.fill: parent
        radius: viewer.radius
        color: "transparent"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#14212A" }
            GradientStop { position: 1.0; color: "#070B0E" }
        }
        opacity: 0.85
    }

    // ============================================================
    // Logger (DOA)
    // ============================================================
    QtObject {
        id: logger
        signal historyUpdated()

        property string inst: "DH-" + Date.now() + "-" + Math.floor(Math.random()*100000)

        property var doaHistory: []
        property var lastVfoConfig: null
        property string lastTime: ""
        property string lastDate: ""
        property string lastUptime: ""

        property real lastRfFreqHz: 0
        property real lastRfBwHz: 0

        property int  maxStableMs: 3200
        property real maxStableDeltaDeg: 1.0
        property int  maxMinIntervalMs: 300

        property string candKey: ""
        property real   candDoa: -9999
        property real   candConf: -1
        property real   candSinceMs: 0
        property real   candFreqHz: 0
        property real   candBwHz: 0

        property real   lastDeltaDegUsed: -1
        property string lastLoggedKey: ""
        property real   lastLoggedDoa: -9999
        property real   lastLoggedMs: 0

        function saveRfParams(freqHz, bwHz) {
            var f = Number(freqHz)
            var b = Number(bwHz)

            if (viewer.rfCache) {
                if (typeof viewer.rfCache.save === "function") {
                    viewer.rfCache.save(f, b)
                } else {
                    if (isFinite(f) && f > 0) viewer.rfCache.lastRfFreqHz = f
                    if (isFinite(b) && b > 0) viewer.rfCache.lastRfBwHz = b
                }
            }

            if (viewer.rfCache) {
                var sf = Number(viewer.rfCache.lastRfFreqHz)
                var sb = Number(viewer.rfCache.lastRfBwHz)

                if (isFinite(sf) && sf > 0) lastRfFreqHz = sf
                if (isFinite(sb) && sb > 0) lastRfBwHz = sb
            } else {
                if (isFinite(f) && f > 0) lastRfFreqHz = f
                if (isFinite(b) && b > 0) lastRfBwHz = b
            }
        }

        function _getRfFreqHzNow() {
            if (viewer.rfCache &&
                isFinite(viewer.rfCache.lastRfFreqHz) &&
                viewer.rfCache.lastRfFreqHz > 0)
                return Number(viewer.rfCache.lastRfFreqHz)

            if (isFinite(lastRfFreqHz) && lastRfFreqHz > 0)
                return Number(lastRfFreqHz)

            return 0
        }

        function _getRfBwHzNow() {
            if (viewer.rfCache &&
                isFinite(viewer.rfCache.lastRfBwHz) &&
                viewer.rfCache.lastRfBwHz > 0)
                return Number(viewer.rfCache.lastRfBwHz)

            if (isFinite(lastRfBwHz) && lastRfBwHz > 0)
                return Number(lastRfBwHz)

            return 0
        }

        function _now() {
            return Date.now()
        }

        function _degChanged(a, b) {
            if (!isFinite(a) || !isFinite(b))
                return true

            return Math.abs(Number(a) - Number(b)) >= maxStableDeltaDeg
        }

        function _fmtDoa(v) {
            var x = Number(v)
            if (!isFinite(x)) return "-"
            return x.toFixed(3)
        }

        function _fmtFreqMHzFromHz(hz) {
            var v = Number(hz)
            if (!isFinite(v) || v <= 0) return "-"
            return (v / 1000000.0).toFixed(3)
        }

        function _fmtBwKHzFromHz(hz) {
            var v = Number(hz)
            if (!isFinite(v) || v <= 0) return "-"
            return (v / 1000.0).toFixed(0)
        }

        function pad2(v) {
            v = Math.floor(v)
            return (v < 10 ? "0" + v : "" + v)
        }

        function fmtSysDateTime(ms) {
            var d = new Date(Number(ms))
            var y  = d.getFullYear()
            var mo = pad2(d.getMonth() + 1)
            var da = pad2(d.getDate())
            var hh = pad2(d.getHours())
            var mm = pad2(d.getMinutes())
            var ss = pad2(d.getSeconds())

            return y + "-" + mo + "-" + da + " " + hh + ":" + mm + ":" + ss
        }

        function _appendLog(timestamp, name, frequency, doaValue, extra, rawVfoIndex, meta) {
            var m = meta || {}

            doaHistory.push({
                timestamp: timestamp,
                name: name,
                frequency: frequency,
                doa: doaValue + (extra.length ? (" " + extra) : ""),
                rawVfoIndex: rawVfoIndex,

                key: String(m.key || ""),
                confidence: String(m.confidence || ""),
                heading: String(m.heading || ""),
                lat: String(m.lat || ""),
                lon: String(m.lon || "")
            })

            if (doaHistory.length > viewer.maxLogItems)
                doaHistory.shift()

            doaLogModel.insert(0, {
                selected: false,
                sent: false,
                saveStatus: "NEW",

                timestamp: timestamp,
                name: name,
                frequency: frequency,
                doaValue: doaValue,
                extra: extra,

                key: String(m.key || ""),
                confidence: String(m.confidence || ""),
                heading: String(m.heading || ""),
                lat: String(m.lat || ""),
                lon: String(m.lon || "")
            })

            // ✅ AUTO SAVE: log ใหม่เข้ามาแล้วเข้าคิว save
            // ส่งจริงเป็น batch ตาม doaAutoSaveCooldownMs เพื่อไม่ให้ถี่เกิน
            if (viewer.doaAutoSaveEnabled) {
                Qt.callLater(function() {
                    if (doaLogModel.count > 0)
                        viewer.queueAutoSaveDoaLog(doaLogModel.get(0))
                })
            }

            if (doaLogModel.count > viewer.maxLogItems)
                doaLogModel.remove(doaLogModel.count - 1)

            viewer.refreshSelectedDoaLogCount()
            historyUpdated()
        }

        function feedMaxDoaCandidate(obj) {
            if (!obj) return
            if (!viewer.visible || !viewer.loggingEnabled) return

            var nowMs = _now()

            var key  = String(obj.key || "")
            var doa  = Number(obj.doa)
            var conf = Number(obj.confidence)

            if (!isFinite(doa))  doa  = 0
            if (!isFinite(conf)) conf = 0

            if ((nowMs - lastLoggedMs) < maxMinIntervalMs)
                return

            var sameKey = (key === lastLoggedKey)
            var sameDoa = !(_degChanged(doa, lastLoggedDoa))

            if (sameKey && sameDoa)
                return

            var fNow = _getRfFreqHzNow()
            var bNow = _getRfBwHzNow()

            lastLoggedKey = key
            lastLoggedDoa = doa
            lastLoggedMs  = nowMs

            var hasRemoteTime = (String(lastDate || "").length > 0) &&
                                (String(lastTime || "").length > 0)

            var tsRemote = hasRemoteTime ? ("[" + lastDate + " " + lastTime + "]")
                                         : ("[" + fmtSysDateTime(nowMs) + "]")

            var doaStr = _fmtDoa(doa)

            var headingStr = ""
            if (obj.heading !== undefined && isFinite(Number(obj.heading)))
                headingStr = Number(obj.heading).toFixed(1)

            var latStr = ""
            if (obj.lat !== undefined && isFinite(Number(obj.lat)))
                latStr = Number(obj.lat).toFixed(6)

            var lonStr = ""
            if (obj.lon !== undefined && isFinite(Number(obj.lon)))
                lonStr = Number(obj.lon).toFixed(6)

            var extra = ""

            if (key.length)
                extra += "key=" + key

            extra += (extra.length ? " " : "") + "conf=" + Number(conf).toFixed(2)

            if (headingStr.length)
                extra += " hdg=" + headingStr

            if (latStr.length && lonStr.length)
                extra += " lat=" + latStr + " lon=" + lonStr

            var freqStr = _fmtFreqMHzFromHz(fNow)
            var bwStr   = _fmtBwKHzFromHz(bNow)

            var freqDisplay = (freqStr === "-")
                    ? "-"
                    : (freqStr + " MHz" + (bwStr !== "-" ? (" / " + bwStr + " kHz") : ""))

            _appendLog(
                tsRemote,
                "[MAX DOA]",
                freqDisplay,
                doaStr,
                extra,
                -999,
                {
                    key: key,
                    confidence: Number(conf).toFixed(2),
                    heading: headingStr,
                    lat: latStr,
                    lon: lonStr
                }
            )
        }

        function saveDoa(vfoIndex, doaValue) {
            if (!viewer.visible || !viewer.loggingEnabled)
                return

            let timeStr = "[" + lastDate + " " + lastTime + "]"
            let nameStr = ""
            let doaStr = "-"

            if (vfoIndex === null) {
                nameStr = "[Center Frequency]"
            } else if (vfoIndex >= 0) {
                nameStr = "[VFO-" + vfoIndex + "]"
            }

            if (!isNaN(doaValue))
                doaStr = Number(doaValue).toFixed(3)

            _appendLog(timeStr, nameStr, "-", doaStr, "", vfoIndex, {})
        }

        function saveTime(currentTime, currentDate, uptime) {
            lastTime = currentTime
            lastDate = currentDate
            lastUptime = uptime
        }

        function saveVfoConfig(config) {
            lastVfoConfig = config
        }
    }

    // ============================================================
    // Fade behavior (UI only)
    // ============================================================
    Behavior on opacity {
        NumberAnimation {
            duration: 220
            easing.type: Easing.InOutQuad
        }
    }

    Timer {
        id: fadeTimer
        interval: 2200
        repeat: false
        onTriggered: {
            if (!daqLocked)
                viewer.active = false
        }
    }

    function triggerFadeIn() {
        active = true

        if (!daqLocked)
            fadeTimer.restart()
    }

    Component.onCompleted: {
        active = false
        viewer.txCoordMode = viewSettings.txCoordMode
        viewer._lastTxModelKey = ""
        viewer.scheduleEmitLatestTx()
    }

    Connections {
        target: logger

        function onHistoryUpdated() {
            viewer.triggerFadeIn()
        }
    }

    function pad2(v) {
        v = Math.floor(v)
        return (v < 10 ? "0" + v : "" + v)
    }

    function tsDateTime(ms) {
        if (!ms) return "-"

        var d = new Date(Number(ms))
        var y = d.getFullYear()
        var mo = pad2(d.getMonth() + 1)
        var da = pad2(d.getDate())
        var hh = pad2(d.getHours())
        var mm = pad2(d.getMinutes())
        var ss = pad2(d.getSeconds())

        return y + "-" + mo + "-" + da + " " + hh + ":" + mm + ":" + ss
    }

    // ===================== TOP BAR =====================
    Row {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 10

        Column {
            spacing: 2

            Text {
                text: "DoA / TX Monitor"
                color: "white"
                font.pixelSize: 14
                font.bold: true
            }

            Text {
                text: (txModel && txModel.count !== undefined)
                      ? ("TX points: " + txModel.count)
                      : "TX points: -"
                color: "#A9C1CC"
                font.pixelSize: 10
            }
        }

        Item {
            width: Math.max(1, topBar.width - 230)
            height: 1
        }

        Button {
            id: lockFadeButton
            width: 40
            height: 40
            checkable: true
            checked: false

            onClicked: {
                viewer.daqLocked = lockFadeButton.checked

                if (viewer.daqLocked) {
                    viewer.active = true
                    fadeTimer.stop()
                } else {
                    fadeTimer.restart()
                }
            }

            background: Rectangle {
                radius: 12
                color: lockFadeButton.checked ? "#1F6F4A" : "#0E1B22"
                border.width: 1
                border.color: lockFadeButton.checked ? "#2ECC71" : "#314f61"
            }

            contentItem: Image {
                anchors.centerIn: parent
                source: lockFadeButton.checked
                        ? "qrc:/iScreenDFqml/images/lock.png"
                        : "qrc:/iScreenDFqml/images/unlock.png"
                width: 30
                height: 30
                fillMode: Image.PreserveAspectFit
            }
        }
    }

    // ===================== TABS =====================
    Row {
        id: tabs
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 10
        spacing: 10

        property int tab: 0 // 0=TX, 1=DOA LOG

        onTabChanged: {
            if (tab === 0)
                viewer.scheduleEmitLatestTx()
        }

        Rectangle {
            id: tabBg
            width: 200
            height: 44
            radius: 18
            color: "#0E1B22"
            border.width: 1
            border.color: "#22313A"

            Row {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 6

                Rectangle {
                    id: tabTx
                    width: 90
                    height: parent.height
                    radius: 16
                    color: tabs.tab === 0 ? "#FFB300" : "transparent"
                    border.width: tabs.tab === 0 ? 0 : 1
                    border.color: "#22313A"

                    property bool pressed: false
                    scale: pressed ? 0.98 : 1.0

                    Behavior on scale {
                        NumberAnimation { duration: 70 }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "TX"
                        color: tabs.tab === 0 ? "#0B1216" : "#A9C1CC"
                        font.bold: true
                        font.pixelSize: 13
                    }

                    TapHandler {
                        onPressedChanged: tabTx.pressed = pressed
                        onTapped: tabs.tab = 0
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onPressed: tabTx.pressed = true
                        onReleased: tabTx.pressed = false
                        onCanceled: tabTx.pressed = false
                        onClicked: tabs.tab = 0
                    }
                }

                Rectangle {
                    id: tabDoa
                    width: 90
                    height: parent.height
                    radius: 16
                    color: tabs.tab === 1 ? "#00FFAA" : "transparent"
                    border.width: tabs.tab === 1 ? 0 : 1
                    border.color: "#22313A"

                    property bool pressed: false
                    scale: pressed ? 0.98 : 1.0

                    Behavior on scale {
                        NumberAnimation { duration: 70 }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "DOA"
                        color: tabs.tab === 1 ? "#0B1216" : "#A9C1CC"
                        font.bold: true
                        font.pixelSize: 13
                    }

                    TapHandler {
                        onPressedChanged: tabDoa.pressed = pressed
                        onTapped: tabs.tab = 1
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onPressed: tabDoa.pressed = true
                        onReleased: tabDoa.pressed = false
                        onCanceled: tabDoa.pressed = false
                        onClicked: tabs.tab = 1
                    }
                }
            }
        }

        Rectangle {
            id: coordToggle
            width: 236
            height: 44
            radius: 18
            color: "#0E1B22"
            border.width: 1
            border.color: "#22313A"
            visible: tabs.tab === 0

            Row {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 6

                Rectangle {
                    id: btnLatLon
                    width: 110
                    height: parent.height
                    radius: 16
                    color: viewer.txCoordMode === 0 ? "#FFB300" : "transparent"
                    border.width: viewer.txCoordMode === 0 ? 0 : 1
                    border.color: "#22313A"

                    property bool pressed: false
                    scale: pressed ? 0.98 : 1.0

                    Behavior on scale {
                        NumberAnimation { duration: 70 }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "lat/lon"
                        color: viewer.txCoordMode === 0 ? "#0B1216" : "#A9C1CC"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    TapHandler {
                        onPressedChanged: btnLatLon.pressed = pressed
                        onTapped: {
                            viewer.txCoordMode = 0
                            viewSettings.txCoordMode = 0
                            viewer._lastTxModelKey = ""
                            viewer.scheduleEmitLatestTx()
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onPressed: btnLatLon.pressed = true
                        onReleased: btnLatLon.pressed = false
                        onCanceled: btnLatLon.pressed = false
                        onClicked: {
                            viewer.txCoordMode = 0
                            viewSettings.txCoordMode = 0
                            viewer._lastTxModelKey = ""
                            viewer.scheduleEmitLatestTx()
                        }
                    }
                }

                Rectangle {
                    id: btnMgrs
                    width: 110
                    height: parent.height
                    radius: 16
                    color: viewer.txCoordMode === 1 ? "#00FFAA" : "transparent"
                    border.width: viewer.txCoordMode === 1 ? 0 : 1
                    border.color: "#22313A"

                    property bool pressed: false
                    scale: pressed ? 0.98 : 1.0

                    Behavior on scale {
                        NumberAnimation { duration: 70 }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "MGRS"
                        color: viewer.txCoordMode === 1 ? "#0B1216" : "#A9C1CC"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    TapHandler {
                        onPressedChanged: btnMgrs.pressed = pressed
                        onTapped: {
                            viewer.txCoordMode = 1
                            viewSettings.txCoordMode = 1
                            viewer._lastTxModelKey = ""
                            viewer.scheduleEmitLatestTx()
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onPressed: btnMgrs.pressed = true
                        onReleased: btnMgrs.pressed = false
                        onCanceled: btnMgrs.pressed = false
                        onClicked: {
                            viewer.txCoordMode = 1
                            viewSettings.txCoordMode = 1
                            viewer._lastTxModelKey = ""
                            viewer.scheduleEmitLatestTx()
                        }
                    }
                }
            }
        }
    }

    // ===================== CONTENT =====================
    Item {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: tabs.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 14

        // ---------- TX PANEL ----------
        Item {
            anchors.fill: parent
            visible: tabs.tab === 0

            Rectangle {
                anchors.fill: parent
                radius: 14
                color: "#0E1B22"
                border.width: 1
                border.color: "#22313A"
                opacity: 0.95
            }

            // ✅ TX MARK ON/OFF + CLEAR control
            Row {
                id: txMarkControlRow
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 10
                anchors.rightMargin: 10
                spacing: 6
                z: 10

                Rectangle {
                    width: 78
                    height: 28
                    radius: 10
                    color: viewer.txMarkEnabled ? "#00FFAA" : "#211416"
                    border.width: 1
                    border.color: viewer.txMarkEnabled ? "#00FFAA" : "#5A2B2B"

                    Text {
                        anchors.centerIn: parent
                        text: viewer.txMarkEnabled ? "MARK ON" : "MARK OFF"
                        color: viewer.txMarkEnabled ? "#0B1216" : "#FFB4B4"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            viewer.txMarkEnabled = !viewer.txMarkEnabled

                            // If mark is disabled, remove current map marker immediately
                            if (!viewer.txMarkEnabled) {
                                viewer.selectedTxLogKey = ""
                                viewer.txLogClearRequested()
                            }
                        }
                    }
                }

                Rectangle {
                    width: 54
                    height: 28
                    radius: 10
                    color: clearTxMarkMouse.pressed ? "#172A34" : "#0E1B22"
                    border.width: 1
                    border.color: "#314f61"
                    opacity: viewer.selectedTxLogKey.length > 0 ? 1.0 : 0.35

                    Text {
                        anchors.centerIn: parent
                        text: "CLEAR"
                        color: "#A9C1CC"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    MouseArea {
                        id: clearTxMarkMouse
                        anchors.fill: parent
                        enabled: viewer.selectedTxLogKey.length > 0
                        onClicked: {
                            viewer.selectedTxLogKey = ""
                            viewer.txLogClearRequested()
                        }
                    }
                }
            }

            Item {
                anchors.fill: parent
                visible: !txModel || (txModel.count !== undefined && txModel.count === 0)

                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        text: "No TX history yet"
                        color: "white"
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        text: "Waiting for 2+ DOA intersection..."
                        color: "#A9C1CC"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            ListView {
                id: txList
                anchors.fill: parent
                anchors.margins: 10
                anchors.topMargin: 46   // ✅ leave room for MARK ON/OFF + CLEAR buttons
                clip: true
                spacing: 8
                model: txModel
                visible: txModel && (txModel.count === undefined || txModel.count > 0)

                // ✅ ทำให้ scroll/flick ง่ายขึ้น ทั้ง mouse/touch/touchpad
                interactive: true
                pressDelay: 60
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.DragAndOvershootBounds
                flickDeceleration: 2600
                maximumFlickVelocity: 8000
                cacheBuffer: Math.max(height * 2, 900)

                onMovementStarted: viewer.triggerFadeIn()
                onFlickStarted: viewer.triggerFadeIn()

                // ✅ รองรับ mouse wheel / touchpad ให้แน่นขึ้น
                WheelHandler {
                    id: txWheelHandler
                    target: txList
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

                    onWheel: function(event) {
                        var maxY = Math.max(0, txList.contentHeight - txList.height)
                        var nextY = txList.contentY - event.angleDelta.y
                        txList.contentY = Math.max(0, Math.min(maxY, nextY))
                        viewer.triggerFadeIn()
                        event.accepted = true
                    }
                }

                delegate: Rectangle {
                    id: txRow

                    // ✅ เว้นพื้นที่ scrollbar ไม่ให้ทับ text
                    width: txList.width - 18
                    height: 70
                    radius: 14

                    property string txKey: viewer.makeTxLogKey(model.lat, model.lon, model.updatedMs)
                    property bool isSelectedTxLog: viewer.selectedTxLogKey === txKey

                    color: isSelectedTxLog
                           ? "#102A27"
                           : (index === 0 ? "#14212A" : "#0B1216")

                    border.width: isSelectedTxLog ? 2 : 1
                    border.color: isSelectedTxLog
                                  ? "#00FFAA"
                                  : (index === 0 ? "#FFB300" : "#22313A")
                    opacity: 0.97

                    Rectangle {
                        x: 10
                        y: 16
                        width: 38
                        height: 38
                        radius: 14
                        color: txRow.isSelectedTxLog ? "#00FFAA" : (index === 0 ? "#FFB300" : "#0E1B22")
                        border.width: 1
                        border.color: txRow.isSelectedTxLog ? "#00FFAA" : (index === 0 ? "#FFB300" : "#22313A")

                        Text {
                            anchors.centerIn: parent
                            text: "#" + (index + 1)
                            color: (txRow.isSelectedTxLog || index === 0) ? "#0B1216" : "#A9C1CC"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 58
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        Text {
                            text: viewer.txCoordMode === 1
                                  ? ("MGRS " + viewer.latLonToMgrs(Number(model.lat), Number(model.lon), 5))
                                  : ("lat " + Number(model.lat).toFixed(6) + "   lon " + Number(model.lon).toFixed(6))
                            color: "white"
                            font.pixelSize: 12
                            font.bold: true
                            font.family: "Monospace"
                            elide: Text.ElideRight
                            width: txList.width - 58 - 52
                        }

                        Row {
                            spacing: 10

                            Text {
                                text: "rms " + Math.round(Number(model.rms || 0)) + " m"
                                color: "#FFB300"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            Text {
                                text: "time " + viewer.tsDateTime(model.updatedMs)
                                color: "#A9C1CC"
                                font.pixelSize: 10
                                font.family: "Monospace"
                            }
                        }
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: 10
                        width: 6
                        height: parent.height - 18
                        radius: 3
                        color: txRow.isSelectedTxLog ? "#00FFAA" : (index === 0 ? "#FFB300" : "#22313A")
                        opacity: 0.9
                    }

                    // ✅ ลาก = scroll, แตะเฉย ๆ = mark TX
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        preventStealing: false
                        propagateComposedEvents: true

                        onPressed: {
                            viewer.triggerFadeIn()
                        }

                        onClicked: {
                            viewer.markTxLogFromPanel(
                                index,
                                Number(model.lat),
                                Number(model.lon),
                                Number(model.rms || 0),
                                Number(model.updatedMs || 0)
                            )
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    id: txScrollBar
                    active: true
                    policy: ScrollBar.AlwaysOn
                    interactive: true
                    width: 18

                    contentItem: Rectangle {
                        implicitWidth: 14
                        radius: 7
                        color: txScrollBar.pressed ? "#00FFAA" : "#7AE2CF"
                        opacity: 0.95
                    }

                    background: Rectangle {
                        implicitWidth: 18
                        radius: 9
                        color: "#101820"
                        border.width: 1
                        border.color: "#22313A"
                        opacity: 0.85
                    }
                }
            }
        }

        // ---------- DOA LOG PANEL ----------
        Item {
            id: doaPanel
            anchors.fill: parent
            visible: tabs.tab === 1

            Rectangle {
                anchors.fill: parent
                radius: 14
                color: "#0E1B22"
                border.width: 1
                border.color: "#22313A"
                opacity: 0.95
            }

            property bool stickToTop: true
            property bool userInteracting: false

            Timer {
                id: doaIdleTimer
                interval: 380
                repeat: false
                onTriggered: doaPanel.userInteracting = false
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Rectangle {
                    id: doaSaveBar
                    Layout.fillWidth: true
                    height: 42
                    radius: 12
                    color: "#0B1216"
                    border.width: 1
                    border.color: "#22313A"
                    clip: true

                    // ===== Left title =====
                    Text {
                        id: doaTitleText
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter

                        text: "DOA LOG"
                        color: "white"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Text {
                        id: selectedText
                        anchors.left: doaTitleText.right
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter

                        text: "Selected: " + viewer.selectedDoaLogCount + (viewer.doaAutoSaveEnabled ? "  AUTO" : "")
                        color: "#00FFAA"
                        font.pixelSize: 11
                        font.bold: true
                    }

                    // ===== Right fixed buttons =====
                    Row {
                        id: rightButtonRow
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        Rectangle {
                            width: 52
                            height: 26
                            radius: 10
                            color: selectAllMouse.pressed ? "#172A34" : "#0E1B22"
                            border.width: 1
                            border.color: "#314f61"

                            Text {
                                anchors.centerIn: parent
                                text: "ALL"
                                color: "#A9C1CC"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            MouseArea {
                                id: selectAllMouse
                                anchors.fill: parent
                                onClicked: viewer.setAllDoaLogSelected(true)
                            }
                        }

                        Rectangle {
                            width: 58
                            height: 26
                            radius: 10
                            color: clearSelMouse.pressed ? "#321A1A" : "#211416"
                            border.width: 1
                            border.color: "#5A2B2B"

                            Text {
                                anchors.centerIn: parent
                                text: "CLEAR"
                                color: "#FFB4B4"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            MouseArea {
                                id: clearSelMouse
                                anchors.fill: parent
                                onClicked: viewer.setAllDoaLogSelected(false)
                            }
                        }

                        Rectangle {
                            width: 62
                            height: 26
                            radius: 10
                            color: viewer.doaAutoSaveEnabled ? "#00FFAA" : "#0E1B22"
                            border.width: 1
                            border.color: viewer.doaAutoSaveEnabled ? "#00FFAA" : "#314f61"

                            Text {
                                anchors.centerIn: parent
                                text: viewer.doaAutoSaveEnabled ? "AUTO ON" : "AUTO"
                                color: viewer.doaAutoSaveEnabled ? "#0B1216" : "#A9C1CC"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    viewer.doaAutoSaveEnabled = !viewer.doaAutoSaveEnabled

                                    if (viewer.doaAutoSaveEnabled) {
                                        viewer.setDoaSaveStatus("Auto save ON", "#00FFAA")

                                        // ✅ เปิด AUTO แล้วมี log เดิมค้างอยู่ ให้ queue เฉพาะตัวล่าสุดทันที 1 ตัว
                                        // ไม่ queue ทั้ง list เพื่อกัน save ถี่/เยอะเกินตอนกดเปิด
                                        if (doaLogModel.count > 0)
                                            viewer.queueAutoSaveDoaLog(doaLogModel.get(0))
                                    } else {
                                        viewer.clearAutoSaveQueue()
                                        viewer.setDoaSaveStatus("Auto save OFF", "#A9C1CC")
                                    }
                                }
                            }
                        }

                        Rectangle {
                            width: 58
                            height: 26
                            radius: 10
                            color: saveSelectedMouse.pressed ? "#00C986" : "#00FFAA"
                            opacity: viewer.selectedDoaLogCount > 0 ? 1.0 : 0.35

                            Text {
                                anchors.centerIn: parent
                                text: "SAVE"
                                color: "#0B1216"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            MouseArea {
                                id: saveSelectedMouse
                                anchors.fill: parent
                                enabled: viewer.selectedDoaLogCount > 0
                                onClicked: viewer.saveSelectedDoaLogsToCpp()
                            }
                        }
                    }

                    // ===== Status fixed between left text and buttons =====
                    Item {
                        id: statusClipBox
                        anchors.left: selectedText.right
                        anchors.leftMargin: 8
                        anchors.right: rightButtonRow.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        height: 20
                        clip: true

                        Text {
                            anchors.fill: parent
                            text: viewer.doaSaveStatusText
                            color: viewer.doaSaveStatusColor
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                ListView {
                    id: doaList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: doaLogModel
                    spacing: 8

                    // ✅ ทำให้ DOA LOG scroll/flick ง่ายเหมือน TX Monitor
                    interactive: true
                    pressDelay: 60
                    flickableDirection: Flickable.VerticalFlick
                    boundsBehavior: Flickable.DragAndOvershootBounds
                    flickDeceleration: 2600
                    maximumFlickVelocity: 8000
                    cacheBuffer: Math.max(height * 2, 900)

                    onMovementStarted: {
                        viewer.triggerFadeIn()
                        doaPanel.userInteracting = true
                        doaIdleTimer.restart()
                    }

                    onFlickStarted: {
                        viewer.triggerFadeIn()
                        doaPanel.userInteracting = true
                        doaIdleTimer.restart()
                    }

                    // ✅ รองรับ mouse wheel / touchpad ให้แน่นขึ้น
                    WheelHandler {
                        id: doaWheelHandler
                        target: doaList
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

                        onWheel: function(event) {
                            var maxY = Math.max(0, doaList.contentHeight - doaList.height)
                            var nextY = doaList.contentY - event.angleDelta.y
                            doaList.contentY = Math.max(0, Math.min(maxY, nextY))
                            viewer.triggerFadeIn()
                            doaPanel.userInteracting = true
                            doaIdleTimer.restart()
                            doaPanel.stickToTop = doaList.contentY < 6
                            event.accepted = true
                        }
                    }

                    onContentYChanged: {
                        doaPanel.userInteracting = true
                        doaIdleTimer.restart()
                        var topGap = contentY
                        doaPanel.stickToTop = topGap < 6
                    }

                    onCountChanged: {
                        if (!doaPanel.userInteracting && doaPanel.stickToTop) {
                            Qt.callLater(function() {
                                doaList.positionViewAtBeginning()
                            })
                        }
                    }

                    delegate: Rectangle {
                        id: doaRow
                        width: doaList.width - 18   // ✅ เว้นพื้นที่ scrollbar
                        implicitHeight: Math.max(58, rowLayout.implicitHeight + 20)
                        radius: 12
                        color: model.selected
                               ? "#102A27"
                               : (model.sent === true ? "#0D1E1A" : (index === 0 ? "#14212A" : "#0B1216"))
                        border.width: 1
                        border.color: model.selected
                                      ? "#00FFAA"
                                      : (model.sent === true ? "#00FFAA66" : (index === 0 ? "#00FFAA" : "#22313A"))
                        opacity: 0.98

                        RowLayout {
                            id: rowLayout
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            CheckBox {
                                id: rowCheck
                                checked: model.selected === true
                                Layout.preferredWidth: 28
                                Layout.alignment: Qt.AlignTop

                                onToggled: {
                                    viewer.setDoaLogSelected(index, checked)
                                }

                                indicator: Rectangle {
                                    implicitWidth: 18
                                    implicitHeight: 18
                                    radius: 5
                                    color: rowCheck.checked ? "#00FFAA" : "#0E1B22"
                                    border.width: 1
                                    border.color: rowCheck.checked ? "#00FFAA" : "#314f61"

                                    Text {
                                        anchors.centerIn: parent
                                        text: rowCheck.checked ? "✓" : ""
                                        color: "#0B1216"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                }

                                contentItem: Item { }
                            }

                            Column {
                                id: rowTextCol
                                Layout.fillWidth: true
                                spacing: 6

                                Row {
                                    spacing: 8

                                    Text {
                                        text: model.timestamp
                                        color: "#A9C1CC"
                                        font.pixelSize: 10
                                        font.family: "Monospace"
                                    }

                                    Text {
                                        text: model.name + " [" + model.doaValue + "]"
                                        color: "white"
                                        font.pixelSize: 12
                                        font.bold: true
                                        font.family: "Monospace"
                                    }

                                    Text {
                                        text: "[" + model.frequency + "]"
                                        color: "#00FFAA"
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                    }

                                    Text {
                                        text: model.sent === true ? "SAVED" : "NEW"
                                        color: model.sent === true ? "#00FFAA" : "#FFCF4C"
                                        font.pixelSize: 10
                                        font.bold: true
                                        font.family: "Monospace"
                                    }
                                }

                                Text {
                                    visible: String(model.extra).length > 0
                                    text: String(model.extra)
                                    color: "#6F8C98"
                                    font.pixelSize: 10
                                    font.family: "Monospace"
                                    wrapMode: Text.Wrap
                                    width: rowTextCol.width
                                }
                            }
                        }

                        // ✅ ลาก = scroll, แตะเฉย ๆ = toggle select
                        // เว้นช่อง checkbox ซ้ายไว้ให้กด checkbox ได้ปกติ
                        MouseArea {
                            anchors.fill: parent
                            anchors.leftMargin: 40
                            acceptedButtons: Qt.LeftButton
                            preventStealing: false
                            propagateComposedEvents: true

                            onPressed: {
                                viewer.triggerFadeIn()
                            }

                            onClicked: {
                                viewer.setDoaLogSelected(index, !(model.selected === true))
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: doaScrollBar
                        active: true
                        policy: ScrollBar.AlwaysOn
                        interactive: true
                        width: 18

                        contentItem: Rectangle {
                            implicitWidth: 14
                            radius: 7
                            color: doaScrollBar.pressed ? "#00FFAA" : "#7AE2CF"
                            opacity: 0.95
                        }

                        background: Rectangle {
                            implicitWidth: 18
                            radius: 9
                            color: "#101820"
                            border.width: 1
                            border.color: "#22313A"
                            opacity: 0.85
                        }
                    }
                }
            }

            Rectangle {
                visible: !doaPanel.stickToTop && doaLogModel.count > 0
                width: 120
                height: 30
                radius: 12
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 14
                anchors.topMargin: 62
                color: "#14212A"
                border.width: 1
                border.color: "#00FFAA55"

                Text {
                    anchors.centerIn: parent
                    text: "Follow new"
                    color: "#00FFAA"
                    font.pixelSize: 11
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        doaPanel.stickToTop = true
                        doaPanel.userInteracting = false
                        Qt.callLater(function() {
                            doaList.positionViewAtBeginning()
                        })
                    }
                }
            }

            Item {
                anchors.fill: parent
                visible: doaLogModel.count === 0

                Column {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 24
                    spacing: 6

                    Text {
                        text: "No DOA logs yet"
                        color: "white"
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        text: "Waiting for stable max DOA..."
                        color: "#A9C1CC"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }

    // tap to show when faded
    MouseArea {
        anchors.fill: parent
        z: 10000

        // ✅ ปิดไว้ ไม่ให้บัง scroll ของ TX/DOA Monitor
        visible: !viewer.active && viewer.useWakeCover
        enabled: visible

        onPressed: viewer.triggerFadeIn()
        propagateComposedEvents: true
    }
}

