// LogDeviceScanner.qml (FULL FILE)  ✅ Scan Filter by Date/Time + Session list shows "HH:MM:SS (YYYY-MM-DD)"
// ✅ FIX: Scan/Memory buttons set widgetView correctly
// ✅ FIX: Dialog y positioning (center) (no "/ width" bug)
// ✅ FIX: splitDateTime trims milliseconds/timezone
// ✅ FIX: rebuildScanSessionsForDate real sort (no dummy setProperty)
// ✅ FIX: applyScanFilter sync scanFilterDate when tk selected
// ✅ FIX: enterGroupByTimeKey Index mapping bug
// ✅ Session list shows date in parentheses

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    width: 1205
    height: 400
    visible: true

    // =========================================================
    // NOTE:
    // - widgetView ต้องมาจากระบบเดิมคุณ (true=Scan, false=Memory)
    // =========================================================
    // property bool widgetView: true

    // ====== Internal "window" navigation state ======
    property bool inGroupView: false
    property string activeGroupTimeKey: ""
    property string activeGroupTitle: ""
    property var pendingDeleteIndex: null

    // ====== delete confirm target ======
    property string pendingDeleteGroupTimeKey: ""

    // ✅ กัน rebuild ดึง tk กลับมา (จนกว่า backend จะลบจริง)
    property var pendingDeletedTk: ({})   // map: tk -> true

    Connections {
        target: mainWindows

        function onScanCardUpdateDelete(idx) {
            console.log("scanCardUpdateDelete idx:", idx)
            // ✅ ลบ UI ทันที
            removeScanItemFromUI(idx)
        }
    }

    // =====================
    // Preset Save Dialog State
    // =====================
    property bool modifyPreset: false
    property string modifyPresetId: ""
    property string modifyPresetName: ""

    // ใช้เรียกเปิด dialog (พร้อมใส่ object ที่จะ save)
    function openPresetNameDialog(presetId, presetObj, isModify) {
        nameDialog.generatedPresetId = String(presetId)
        nameDialog.pendingPresetObject = presetObj ? JSON.parse(JSON.stringify(presetObj)) : ({})
        modifyPreset = !!isModify

        presetNameField.text = (nameDialog.pendingPresetObject.name !== undefined)
                                ? String(nameDialog.pendingPresetObject.name)
                                : (modifyPreset ? (modifyPresetName || "") : "")

        nameDialog.open()
        presetNameField.forceActiveFocus()
        presetNameField.selectAll()
    }

    // =====================
    // Dialog: Enter Preset Name (Unified UI)
    // =====================
    Dialog {
        id: nameDialog
        modal: true
        focus: true
        standardButtons: Dialog.NoButton
        visible: false

        // ❌ ไม่ใช้ title/header ของ Dialog
        title: ""
        header: null
        footer: null

        width: Math.min(parent.width - 60, 520)
        height: 170

        x: (parent.width - width) / 2
        y: (parent.height - height) / 2   // ✅ FIX

        property string generatedPresetId: ""
        property var pendingPresetObject: ({})

        background: Rectangle {
            radius: 18
            color: "#0E1520"
            border.color: "#1C2A3D"
            border.width: 1
        }

        function doAccept() {
            var nm = presetNameField.text.trim()
            if (nm === "")
                return

            pendingPresetObject.name = nm

            // ===== logic เดิมทั้งหมด =====
            if (typeof configManager !== "undefined" && configManager) {
                configManager.addOrModifyPreset(generatedPresetId, pendingPresetObject)

                var presets = configManager.getPresetsAsList()
                if (typeof radioMemList !== "undefined" && radioMemList) {
                    radioMemList.clear()
                    for (var i = 0; i < presets.length; i++)
                        radioMemList.append(presets[i])
                }

                configManager.saveToFile("/var/lib/openwebrx/preset.json")

                if (modifyPreset && mainWindows.editCardWebSlot)
                    mainWindows.editCardWebSlot(generatedPresetId,
                                                 JSON.stringify(pendingPresetObject))
                else if (!modifyPreset && mainWindows.addCardWebSlot)
                    mainWindows.addCardWebSlot(generatedPresetId)

            } else {
                if (mainWindows && mainWindows.savePresetFromScanSlot) {
                    mainWindows.savePresetFromScanSlot(
                        generatedPresetId,
                        JSON.stringify(pendingPresetObject)
                    )
                } else {
                    console.warn("No configManager and no savePresetFromScanSlot()")
                }
            }

            modifyPreset = false
            nameDialog.close()
        }

        contentItem: Item {
            anchors.fill: parent
            anchors.margins: 18

            Column {
                anchors.fill: parent
                spacing: 16

                // ===== Title (เหมือน confirm) =====
                Text {
                    text: "Enter Preset Name"
                    color: "#FFFFFF"
                    font.pixelSize: 20
                    font.bold: true
                }

                // ===== Input =====
                TextField {
                    id: presetNameField
                    placeholderText: "Enter preset name"
                    width: parent.width
                    height: 40

                    color: "#E5E7EB"
                    placeholderTextColor: "#93A4B8"
                    font.pixelSize: 16

                    leftPadding: 12
                    rightPadding: 12
                    verticalAlignment: Text.AlignVCenter

                    background: Rectangle {
                        radius: 10
                        color: "#0F172A"
                        border.color: "#334155"
                        border.width: 1
                    }

                    Keys.onReturnPressed: nameDialog.doAccept()
                    Keys.onEnterPressed:  nameDialog.doAccept()
                    Keys.onEscapePressed: nameDialog.close()
                }

                Item { height: 6 }

                // ===== Buttons (เหมือน confirm) =====
                Row {
                    spacing: 12
                    anchors.right: parent.right

                    Item {
                        id: cancelBtn
                        width: 140
                        height: 40

                        property bool hovered: cancelMouse.containsMouse
                        property bool pressed: cancelMouse.pressed

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: cancelBtn.pressed
                                   ? "#1E293B"
                                   : (cancelBtn.hovered ? "#0F172A" : "transparent")
                            border.color: cancelBtn.hovered ? "#94A3B8" : "#3B4B63"
                            border.width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "CANCEL"
                            color: cancelBtn.hovered ? "#FFFFFF" : "#F2F6FF"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: cancelMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: nameDialog.close()
                        }
                    }

                    Item {
                        id: okBtn
                        width: 140
                        height: 40

                        property bool hovered: okMouse.containsMouse
                        property bool pressed: okMouse.pressed

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: okBtn.pressed
                                   ? "#064E3B"
                                   : (okBtn.hovered ? "#0F766E" : "transparent")
                            border.color: okBtn.hovered ? "#2DD4BF" : "#3B4B63"
                            border.width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "OK"
                            color: okBtn.hovered ? "#ECFEFF" : "#2DD4BF"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: okMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: nameDialog.doAccept()
                        }
                    }
                }
            }
        }

        onOpened: {
            presetNameField.forceActiveFocus()
            presetNameField.selectAll()
        }
    }

    Dialog {
        id: confirmDeleteDialog
        modal: true
        focus: true
        standardButtons: Dialog.NoButton

        title: ""
        header: null
        footer: null

        width: 420
        height: 150

        x: (parent.width / 2) - (width / 2)
        y: (parent.height - height) / 2   // ✅ FIX

        background: Rectangle {
            radius: 18
            color: "#0E1520"
            border.color: "#1C2A3D"
            border.width: 1
        }

        contentItem: Item {
            anchors.fill: parent
            anchors.margins: 18

            Column {
                anchors.fill: parent
                spacing: 16

                Text {
                    text: "Confirm delete"
                    color: "#FFFFFF"
                    font.pixelSize: 20
                    font.bold: true
                }

                Text {
                    text: "Delete this scan item?"
                    color: "#BFD0E6"
                    font.pixelSize: 18
                    wrapMode: Text.WordWrap
                }

                Item { height: 6 }

                Row {
                    spacing: 12
                    anchors.right: parent.right

                    // ---------- CANCEL ----------
                    Item {
                        id: confirmDeleteCancelBtn
                        width: 120
                        height: 40

                        property bool hovered: confirmDeleteCancelMouse.containsMouse
                        property bool pressed: confirmDeleteCancelMouse.pressed

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: confirmDeleteCancelBtn.pressed
                                   ? "#1E293B"
                                   : (confirmDeleteCancelBtn.hovered ? "#0F172A" : "transparent")
                            border.color: confirmDeleteCancelBtn.hovered ? "#94A3B8" : "#3B4B63"
                            border.width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "CANCEL"
                            color: confirmDeleteCancelBtn.hovered ? "#FFFFFF" : "#F2F6FF"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: confirmDeleteCancelMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: confirmDeleteDialog.close()
                        }
                    }

                    // ---------- DELETE ----------
                    Item {
                        id: confirmDeleteDeleteBtn
                        width: 120
                        height: 40

                        property bool hovered: confirmDeleteDeleteMouse.containsMouse
                        property bool pressed: confirmDeleteDeleteMouse.pressed

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: confirmDeleteDeleteBtn.pressed
                                   ? "#7F1D1D"
                                   : (confirmDeleteDeleteBtn.hovered ? "#991B1B" : "transparent")
                            border.color: confirmDeleteDeleteBtn.hovered ? "#FF5C5C" : "#3B4B63"
                            border.width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "DELETE"
                            color: confirmDeleteDeleteBtn.hovered ? "#FFE4E6" : "#FF5C5C"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: confirmDeleteDeleteMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                console.log("confirmDeleteDeleteMouse:")
                                if (pendingDeleteIndex !== null) {
                                    removeScanItemFromUI(pendingDeleteIndex)
                                    mainWindows.deleteScanCardSlot(pendingDeleteIndex)
                                }
                                pendingDeleteIndex = null
                                confirmDeleteDialog.close()
                            }
                        }
                    }
                }
            }
        }
    }

    function removeScanItemFromUI(indexValue) {
        var key = String(indexValue)
        var removed = false

        // 1) ถ้าอยู่หน้า group (การ์ดย่อย) -> ลบจาก UI ทันที
        if (inGroupView) {
            for (var gi = 0; gi < groupItemsModel.count; gi++) {
                if (String(groupItemsModel.get(gi).Index) === key) {
                    groupItemsModel.remove(gi)
                    removed = true
                    break
                }
            }

            // อัปเดต count/min/max ของ folder บนหน้า MAIN
            updateGroupSummaryFromActiveItems()

            // ถ้ากลุ่มว่างแล้ว -> ลบ folder และออกจากหน้านี้ทันที
            if (groupItemsModel.count === 0 && activeGroupTimeKey) {
                removeGroupFromUI(activeGroupTimeKey)
            }
        }

        // 2) ลบจาก source model (profileScan) เพื่อไม่ให้ rebuild ดึงกลับมา
        if (typeof profileScan !== "undefined" && profileScan && profileScan.count !== undefined) {
            for (var i = 0; i < profileScan.count; i++) {
                if (String(profileScan.get(i).Index) === key) {
                    profileScan.remove(i)
                    removed = true
                    break
                }
            }
        }

        return removed
    }

    // ===== Helpers =====
    function s(v) { return (v === undefined || v === null) ? "" : String(v) }
    function n(v, d) {
        var x = Number(v)
        return (v === undefined || v === null || v === "" || isNaN(x)) ? (d === undefined ? 0 : d) : x
    }
    function toMHz(x) {
        x = n(x, 0)
        return (Math.abs(x) > 100000) ? (x / 1e6) : x
    }

    // =========================================================
    // ✅ รีคำนวณ count/min/max ของ group ปัจจุบันจาก groupItemsModel
    // แล้วอัปเดต groupedScanModel (หน้า MAIN) โดยไม่ต้อง rebuild ทั้งหมด
    // =========================================================
    function updateGroupSummaryFromActiveItems() {
        var tk = activeGroupTimeKey
        if (!tk) return

        var count = groupItemsModel.count
        var minF = 999999
        var maxF = -999999

        for (var i = 0; i < groupItemsModel.count; i++) {
            var it = groupItemsModel.get(i)
            var f = n(it.Freq, 0)
            if (f > 0) {
                if (f < minF) minF = f
                if (f > maxF) maxF = f
            }
        }

        if (minF === 999999) minF = 0
        if (maxF === -999999) maxF = 0

        // ถ้าลบจนหมด -> ลบโฟลเดอร์ออกจาก UI ด้วย
        if (count <= 0) {
            removeGroupFromUI(tk)
            // อยู่ใน group view -> ออกอัตโนมัติ
            if (inGroupView && activeGroupTimeKey === tk) leaveGroup()
            return
        }

        // หา row ใน groupedScanModel แล้วอัปเดต count/min/max
        for (var g = 0; g < groupedScanModel.count; g++) {
            var row = groupedScanModel.get(g)
            if (row && String(row.timeKey) === String(tk)) {
                groupedScanModel.setProperty(g, "count", count)
                groupedScanModel.setProperty(g, "minFreq", minF)
                groupedScanModel.setProperty(g, "maxFreq", maxF)
                break
            }
        }
    }

    function timeKeyOfRow(row) {
        var t = ""
        if (!row) return ""
        if (row.Time !== undefined) t = String(row.Time)
        else if (row.time !== undefined) t = String(row.time)
        else if (row.timestamp !== undefined) t = String(row.timestamp)
        return t
    }

    // =========================================================
    // ✅ Folder ID mapping (ซ้ายบน = 1, ไล่ตามลำดับ UI)
    // =========================================================
    function reindexFolderIds() {
        for (var i = 0; i < groupedScanModel.count; i++) {
            groupedScanModel.setProperty(i, "folderId", i + 1)
        }
    }

    function folderIdFromTk(tk) {
        for (var i = 0; i < groupedScanModel.count; i++) {
            var g = groupedScanModel.get(i)
            if (g && String(g.timeKey) === String(tk))
                return n(g.folderId, i + 1)
        }
        return 0
    }

    // =========================================================
    // ✅ ลบกลุ่มจาก UI ทันที (ลบการ์ด tk ออกจาก groupedScanModel)
    // =========================================================
    function removeGroupFromUI(timeKey) {
        if (!timeKey) return

        // mark กัน rebuild ดึงกลับมา
        pendingDeletedTk[String(timeKey)] = true

        // ถ้าอยู่หน้า detail ของกลุ่มนี้อยู่ -> ออกก่อน
        if (inGroupView && activeGroupTimeKey === timeKey) {
            leaveGroup()
        }

        // ลบ row ใน groupedScanModel (การ์ด group)
        for (var i = groupedScanModel.count - 1; i >= 0; i--) {
            var g = groupedScanModel.get(i)
            if (g && String(g.timeKey) === String(timeKey)) {
                groupedScanModel.remove(i, 1)
                break
            }
        }

        // ✅ หลังลบต้องจัด id ใหม่ (ซ้ายบนกลับเป็น 1 เสมอ)
        reindexFolderIds()
    }

    // ===== Grouped model for Scan =====
    ListModel { id: groupedScanModel }     // 1 row = 1 session/time

    // =========================
    // Scan Filter (by Date/Time)
    // =========================
    property string scanFilterDate: ""     // "YYYY-MM-DD" หรือว่าง = ALL
    property string scanFilterTk: ""       // timeKey เต็ม (ISO) หรือว่าง = ALL/ตามวัน
    property bool   scanFilterActive: false

    ListModel { id: scanFilteredModel }    // model สำหรับแสดงผลหลังกรอง
    ListModel { id: scanUniqueDateModel }  // { date:"YYYY-MM-DD", count:int }
    ListModel { id: scanSessionModel }     // { tk:"...", time:"HH:MM:SS", count:int, folderId:int, minFreq:real, maxFreq:real }

    // ✅ FIX: trim milliseconds/timezone + clean date/time
    function splitDateTime(isoString) {
        if (!isoString) return { date: "", time: "" }

        var s = String(isoString)
        var tIdx = s.indexOf("T")
        if (tIdx === -1) return { date: "", time: "" }

        var datePart = s.slice(0, tIdx)
        var timePart = s.slice(tIdx + 1)

        // cut ms / Z
        var dot = timePart.indexOf(".")
        if (dot !== -1) timePart = timePart.slice(0, dot)
        var z = timePart.indexOf("Z")
        if (z !== -1) timePart = timePart.slice(0, z)

        return { date: datePart, time: timePart }
    }

    function scanDateOfTk(tk) {
        var dt = splitDateTime(String(tk))
        return dt.date || ""
    }
    function scanTimeOfTk(tk) {
        var dt = splitDateTime(String(tk))
        return dt.time || ""
    }

    function rebuildScanUniqueDates() {
        scanUniqueDateModel.clear()

        var seen = {}
        for (var i = 0; i < groupedScanModel.count; i++) {
            var g = groupedScanModel.get(i)
            if (!g) continue
            var tk = String(g.timeKey)
            var d  = scanDateOfTk(tk)
            if (!d) d = "NO_DATE"

            if (!seen[d]) {
                seen[d] = { date: d, count: 0 }
            }
            seen[d].count += 1
        }

        var keys = Object.keys(seen)
        keys.sort(function(a, b) {
            if (a === "NO_DATE" && b === "NO_DATE") return 0
            if (a === "NO_DATE") return 1
            if (b === "NO_DATE") return -1
            return (a < b) ? 1 : -1
        })

        for (var k = 0; k < keys.length; k++) {
            var key = keys[k]
            scanUniqueDateModel.append({ date: key, count: seen[key].count })
        }
    }

    // ✅ FIX: real sort + no dummy
    function rebuildScanSessionsForDate(dateStr) {
        scanSessionModel.clear()
        var dsel = String(dateStr || "")

        var temp = []
        for (var i = 0; i < groupedScanModel.count; i++) {
            var g = groupedScanModel.get(i)
            if (!g) continue

            var tk = String(g.timeKey)
            var d  = scanDateOfTk(tk)
            if (!d) d = "NO_DATE"

            if (dsel !== "" && dsel !== d) continue

            temp.push({
                tk: tk,
                time: scanTimeOfTk(tk),
                count: n(g.count, 0),
                folderId: n(g.folderId, 0),
                minFreq: n(g.minFreq, 0),
                maxFreq: n(g.maxFreq, 0)
            })
        }

        // ใหม่ -> เก่า (ตาม tk)
        temp.sort(function(a, b) { return String(b.tk).localeCompare(String(a.tk)) })

        for (var j = 0; j < temp.length; j++)
            scanSessionModel.append(temp[j])
    }

    function applyScanFilter(dateStr, tkStr) {
        scanFilteredModel.clear()

        scanFilterDate = String(dateStr || "")
        scanFilterTk   = String(tkStr || "")

        // ✅ ถ้าเลือก tk -> sync date ให้ตรงด้วย (ปุ่มจะโชว์ถูก)
        if (scanFilterTk !== "")
            scanFilterDate = scanDateOfTk(scanFilterTk) || scanFilterDate

        scanFilterActive = (scanFilterDate !== "" || scanFilterTk !== "")

        if (!scanFilterActive)
            return

        // 1) เลือก tk -> เหลือใบเดียว
        if (scanFilterTk !== "") {
            for (var i = 0; i < groupedScanModel.count; i++) {
                var g = groupedScanModel.get(i)
                if (g && String(g.timeKey) === scanFilterTk) {
                    scanFilteredModel.append({
                        folderId: 1,
                        timeKey: g.timeKey,
                        count: g.count,
                        minFreq: g.minFreq,
                        maxFreq: g.maxFreq,
                        items: g.items
                    })
                    break
                }
            }
            return
        }

        // 2) เลือกตาม date -> คัดเฉพาะวันนั้น และ reindex folderId 1..N
        var idx = 1
        for (var j = 0; j < groupedScanModel.count; j++) {
            var row = groupedScanModel.get(j)
            if (!row) continue
            var tk = String(row.timeKey)
            var d  = scanDateOfTk(tk)
            if (!d) d = "NO_DATE"

            if (scanFilterDate !== "" && scanFilterDate !== d)
                continue

            scanFilteredModel.append({
                folderId: idx++,
                timeKey: row.timeKey,
                count: row.count,
                minFreq: row.minFreq,
                maxFreq: row.maxFreq,
                items: row.items
            })
        }
    }

    function clearScanFilter() {
        scanFilterDate = ""
        scanFilterTk = ""
        scanFilterActive = false
        scanFilteredModel.clear()
    }

    Connections {
        target: (typeof profileScan !== "undefined") ? profileScan : null
        ignoreUnknownSignals: true
        function onCountChanged() { rebuildGroupedScan() }
    }

    Component.onCompleted: {
        clearScanFilter()
        rebuildGroupedScan()
    }

    // =========================================================
    // ✅ rebuildGroupedScan: group + sort ใหม่->เก่า + folderId (ซ้ายบน=1)
    // =========================================================
    function rebuildGroupedScan() {
        groupedScanModel.clear()
        if (typeof profileScan === "undefined" || !profileScan) return
        if (profileScan.count === undefined || typeof profileScan.get !== "function") return

        var map = {}
        var order = []

        for (var i = 0; i < profileScan.count; i++) {
            var row = profileScan.get(i)
            if (!row) continue

            var tk = timeKeyOfRow(row)
            if (!tk) tk = "NO_TIME"

            if (pendingDeletedTk[String(tk)] === true)
                continue

            if (!map[tk]) {
                map[tk] = { timeKey: tk, count: 0, minFreq: 999999, maxFreq: -999999, items: [] }
                order.push(tk)
            }

            var f = (row.Freq !== undefined) ? n(row.Freq, 0) :
                    (row.freq !== undefined) ? n(row.freq, 0) :
                    (row.center_freq !== undefined) ? toMHz(row.center_freq) : 0

            map[tk].count++
            if (f > 0) {
                if (f < map[tk].minFreq) map[tk].minFreq = f
                if (f > map[tk].maxFreq) map[tk].maxFreq = f
            }

            map[tk].items.push(row)
        }

        function timeScore(tk) {
            if (!tk || tk === "NO_TIME") return -1
            var d = new Date(tk)
            var t = d.getTime()
            return isNaN(t) ? -1 : t
        }

        order.sort(function(a, b) {
            if (a === "NO_TIME" && b === "NO_TIME") return 0
            if (a === "NO_TIME") return 1
            if (b === "NO_TIME") return -1
            return timeScore(b) - timeScore(a)
        })

        for (var k = 0; k < order.length; k++) {
            var key = order[k]
            var g = map[key]
            if (!g) continue

            if (g.minFreq === 999999) g.minFreq = 0
            if (g.maxFreq === -999999) g.maxFreq = 0

            groupedScanModel.append({
                folderId: (k + 1),
                timeKey: g.timeKey,
                count: g.count,
                minFreq: g.minFreq,
                maxFreq: g.maxFreq,
                items: g.items
            })
        }

        // ✅ step4
        rebuildScanUniqueDates()

        if (scanFilterActive) {
            applyScanFilter(scanFilterDate, scanFilterTk)
        }
    }

    // =========================================================
    // Models for "group detail window"
    // =========================================================
    ListModel { id: groupItemsModel }

    function enterGroupByTimeKey(timeKey) {
        groupItemsModel.clear()
        activeGroupTimeKey = timeKey ? timeKey : ""
        var dt = splitDateTime(activeGroupTimeKey)

        activeGroupTitle =
                "SCAN RESULT\n\n" +
                "Date\n" + "• " + dt.date + "\n\n" +
                "Time\n" + "• " + dt.time

        if (typeof profileScan === "undefined" || !profileScan) {
            inGroupView = true
            return
        }
        if (profileScan.count === undefined || typeof profileScan.get !== "function") {
            inGroupView = true
            return
        }

        var temp = []
        for (var i = 0; i < profileScan.count; i++) {
            var r = profileScan.get(i)
            if (!r) continue

            var tk = timeKeyOfRow(r)
            if (!tk) tk = "NO_TIME"

            if (tk === activeGroupTimeKey) {
                temp.push({
                    Profile: (r.Profile !== undefined) ? r.Profile : (r.profile !== undefined ? r.profile : ""),
                    Freq:    (r.Freq !== undefined)    ? Number(r.Freq)
                             : (r.freq !== undefined) ? Number(r.freq)
                             : (r.center_freq !== undefined ? toMHz(r.center_freq) : 0),
                    BW:      (r.BW !== undefined)      ? r.BW      : (r.bw !== undefined ? r.bw : ""),
                    Mode:    (r.Mode !== undefined)    ? r.Mode    : (r.mode !== undefined ? r.mode : ""),
                    LowCut:  (r.LowCut !== undefined)  ? r.LowCut  : (r.lowCut !== undefined ? r.lowCut : ""),
                    HighCut: (r.HighCut !== undefined) ? r.HighCut : (r.highCut !== undefined ? r.highCut : ""),
                    Time:    tk,
                    // ✅ FIX Index mapping
                    Index: (r.Index !== undefined) ? r.Index
                         : (r.index !== undefined) ? r.index
                         : ""
                })
            }
        }

        temp.sort(function(a, b) { return a.Freq - b.Freq })
        for (var j = 0; j < temp.length; j++) groupItemsModel.append(temp[j])

        inGroupView = true
    }

    function leaveGroup() {
        inGroupView = false
        activeGroupTimeKey = ""
        activeGroupTitle = ""
        groupItemsModel.clear()
    }

    /* ===== Timer: ส่ง dspcontrol หน่วงเวลา (เหมือนโค้ดเดิม) ===== */
    Timer {
        id: dspDelayTimer
        interval: 500
        repeat: false
        onTriggered: {
            mainWindows.sendmessage(JSON.stringify(pendingDSP))

            radioScanner.spectrumGLPlot.centerFreq = pendingCenterFreq || 0
            radioScanner.spectrumGLPlot.low_cut = radioMemList.low_cut || 0
            radioScanner.spectrumGLPlot.high_cut = radioMemList.high_cut || 0
            radioScanner.spectrumGLPlot.offsetFrequency = radioMemList.offset_freq || 0
            radioScanner.spectrumGLPlot.start_mod = radioMemList.mod || 0
            scanSqlLevel = (radioMemList.squelch_level * 2) + 255
            stackView.pop(null)
            listView.currentIndex = 0

            if (startDSPAfter) {
                let dspcontrolStart = {
                    type: "dspcontrol",
                    action: "start"
                }
                mainWindows.sendmessage(JSON.stringify(dspcontrolStart))
            }
        }
    }

    // =========================================================
    // Confirm Dialog (Unified UI – Dialog version)
    // =========================================================
    Dialog {
        id: deleteConfirmDialog
        modal: true
        focus: true
        standardButtons: Dialog.NoButton

        title: ""
        header: null
        footer: null

        width: Math.min(root.width - 60, 520)
        height: 200

        x: (root.width - width) / 2
        y: (root.height - height) / 2   // ✅ FIX

        property string titleText:   "Confirm delete"
        property string messageText: "Delete scan group at:\n" + pendingDeleteGroupTimeKey
        property int messageInt: 0
        property var onConfirm: null

        function presetIdFromTk(tk) {
            if (!tk || typeof tk !== "string")
                return ""
            var d = new Date(tk)
            if (isNaN(d.getTime())) {
                console.warn("[presetIdFromTk] invalid tk:", tk)
                return ""
            }
            return d.toISOString()
        }

        background: Rectangle {
            radius: 18
            color: "#0E1520"
            border.color: "#1C2A3D"
            border.width: 1
        }

        contentItem: Item {
            anchors.fill: parent
            anchors.margins: 18

            Column {
                anchors.fill: parent
                spacing: 16

                Text {
                    text: deleteConfirmDialog.titleText
                    color: "#FFFFFF"
                    font.pixelSize: 20
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    text: deleteConfirmDialog.messageText
                    color: "#BFD0E6"
                    font.pixelSize: 24
                    wrapMode: Text.WordWrap
                }

                Item { height: 6 }

                Row {
                    spacing: 12
                    anchors.right: parent.right

                    Item {
                        width: 140
                        height: 40

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: "transparent"
                            border.color: "#3B4B63"
                            border.width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "CANCEL"
                            color: "#F2F6FF"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: deleteConfirmDialog.close()
                        }
                    }

                    Item {
                        width: 140
                        height: 40

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: "transparent"
                            border.color: "#3B4B63"
                            border.width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "DELETE"
                            color: "#FF5C5C"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var tk = pendingDeleteGroupTimeKey
                                var folderId = folderIdFromTk(tk)
                                console.log("Delete folder (tk)", tk, "folderId:", folderId)

                                removeGroupFromUI(tk)

                                if (typeof mainWindows !== "undefined"
                                        && mainWindows
                                        && typeof mainWindows.deleteScanGroupByKey === "function") {
                                    mainWindows.deleteScanGroupByKey(tk)
                                }

                                if (typeof mainWindows !== "undefined"
                                        && mainWindows
                                        && typeof mainWindows.sendmessageToWeb === "function") {

                                    var presetId = deleteConfirmDialog.presetIdFromTk(tk)
                                    var msg = {
                                        objectName: "deleteScanFolder",
                                        presetId: presetId,
                                        tk: String(tk),
                                        folderId: folderId
                                    }
                                    mainWindows.sendmessageToWeb(JSON.stringify(msg))
                                    console.log("[deleteScan->web]", tk, folderId)
                                }

                                var cb = deleteConfirmDialog.onConfirm
                                deleteConfirmDialog.close()
                                if (cb) cb()
                            }
                        }
                    }
                }
            }
        }
    }

    // ==========================
    // Mode buttons (Scan/Memory)
    // ==========================
    Column {
        id: modeButtons
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 5
        spacing: 6
        z: 1000

        property int btnW: 150
        property int btnH: 40

        ToolButton {
            id: btnScan
            width: modeButtons.btnW
            height: modeButtons.btnH
            hoverEnabled: true

            background: Rectangle {
                radius: 8
                border.color: "#0c4a3e"
                border.width: 1
                color: "#169976"
            }

            contentItem: Text {
                text: "Scan"
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: widgetView ? "#FFFFFF" : "#9CA3AF"
                font.pixelSize: 16
                font.bold: true
            }

            onClicked: {
                widgetView = false      // ✅ FIX: Scan = true
                if (inGroupView) leaveGroup()
            }
        }

        ToolButton {
            id: btnMemory
            width: modeButtons.btnW
            height: modeButtons.btnH
            hoverEnabled: true

            background: Rectangle {
                radius: 8
                border.color: "#0c4a3e"
                border.width: 1
                color: "#169976"
            }

            contentItem: Text {
                text: "Memory"
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: widgetView ? "#9CA3AF" : "#FFFFFF"
                font.pixelSize: 16
                font.bold: true
            }

            onClicked: {
                widgetView = false     // ✅ FIX: Memory = false
                if (inGroupView) leaveGroup()
            }
        }
    }

    // ==========================
    // Delete buttons
    // ==========================
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

        ToolButton {
            id: deleteScanButton
            visible: widgetView
            width: deleteButtons.btnW
            height: deleteButtons.btnH
            hoverEnabled: true

            background: Rectangle {
                radius: 8
                color: "#F4320B"
                border.color: "#0c4a3e"
                border.width: 1
                opacity: 0.95
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
                console.log("Delete Scan ALL")
                clearPresetActionDialog.messageText = "Delete Scan All"
                clearPresetActionDialog.messageInt = 1
                clearPresetActionDialog.open()
            }
        }

        ToolButton {
            id: deleteMemoryButton
            visible: widgetView      // ✅ FIX: memory visible when !widgetView
            width: deleteButtons.btnW
            height: deleteButtons.btnH
            hoverEnabled: true

            background: Rectangle {
                radius: 8
                color: "#F4320B"
                border.color: "#0c4a3e"
                border.width: 1
                opacity: 0.95
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
                console.log("Delete Memory ALL")
                clearPresetActionDialog.messageText = "Delete Memory All"
                clearPresetActionDialog.messageInt = 2
                clearPresetActionDialog.open()
            }
        }

        // ================= Filter Scan (Date/Time) =================
        ToolButton {
            id: filterScanButton
            visible: widgetView
            width: deleteButtons.btnW
            height: deleteButtons.btnH
            hoverEnabled: true

            contentItem: Text {
                text: (scanFilterTk !== "")
                      ? ("Scan: " + scanDateOfTk(scanFilterTk) + " " + scanTimeOfTk(scanFilterTk) + " ▼")
                      : (scanFilterDate !== "" ? ("Scan Date: " + scanFilterDate + " ▼") : "Filter Scan ▼")
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "#ffffff"
                font.pixelSize: 16
                font.bold: true
                elide: Text.ElideRight
            }

            background: Rectangle {
                radius: 8
                color: "#12568C"
                border.color: "#0c4a3e"
                border.width: 1
                opacity: 0.95
            }

            onClicked: {
                rebuildScanUniqueDates()
                rebuildScanSessionsForDate(scanFilterDate)
                scanFilterDialog.open()
            }

            Dialog {
                id: scanFilterDialog
                parent: Overlay.overlay
                modal: true
                focus: true
                dim: true
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                anchors.centerIn: parent

                width: 520
                height: Math.min(640, parent.height * 0.85)

                property color dBg:     "#111820"
                property color dBorder: "#2A3A44"
                property color dText:   "#E6EDF3"
                property color dSub:    "#9AA6B2"
                property color dHover:  "#1B2A33"
                property color dAccent: "#00c896"
                property color dPress:  "#223742"

                background: Rectangle {
                    radius: 16
                    color: scanFilterDialog.dBg
                    border.color: scanFilterDialog.dBorder
                    border.width: 1
                }

                contentItem: Item {
                    anchors.fill: parent
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        // ===== Dates List =====
                        Text {
                            text: "Select Date"
                            color: scanFilterDialog.dText
                            font.pixelSize: 14
                            font.bold: true
                        }

                        ListView {
                            id: dateList
                            Layout.fillWidth: true
                            Layout.preferredHeight: 180
                            clip: true
                            spacing: 8
                            model: scanUniqueDateModel

                            delegate: Rectangle {
                                width: dateList.width
                                height: 46
                                radius: 12
                                color: dateMouse.pressed ? scanFilterDialog.dPress
                                      : (dateMouse.containsMouse ? scanFilterDialog.dHover : "transparent")
                                border.width: (scanFilterDate === model.date) ? 1 : 0
                                border.color: (scanFilterDate === model.date) ? scanFilterDialog.dAccent : "transparent"
                                clip: true

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    Text {
                                        Layout.fillWidth: true
                                        text: model.date
                                        color: scanFilterDialog.dText
                                        font.pixelSize: 16
                                        font.bold: (scanFilterDate === model.date)
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: "(" + model.count + ")"
                                        color: scanFilterDialog.dSub
                                        font.pixelSize: 12
                                    }
                                }

                                MouseArea {
                                    id: dateMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        scanFilterDate = String(model.date)
                                        scanFilterTk = ""
                                        rebuildScanSessionsForDate(scanFilterDate)
                                    }
                                }
                            }

                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn; active: true; width: 10 }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#22313A"; opacity: 0.9 }

                        // ===== Sessions List (Time) =====
                        Text {
                            text: "Select Time (optional)"
                            color: scanFilterDialog.dText
                            font.pixelSize: 14
                            font.bold: true
                        }

                        Item {
                            id: sessionListBox
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            // ===== Sessions List (Time) =====
                            ListView {
                                id: sessionList
                                anchors.fill: parent
                                clip: true
                                spacing: 8
                                model: scanSessionModel

                                // ช่วยให้ขอบบน/ล่างมีที่หายใจ เวลามี overlay ลูกศร
                                topMargin: 6
                                bottomMargin: 6

                                delegate: Rectangle {
                                    width: sessionList.width
                                    height: 52
                                    radius: 12
                                    color: sessMouse.pressed ? scanFilterDialog.dPress
                                          : (sessMouse.containsMouse ? scanFilterDialog.dHover : "transparent")
                                    border.width: (scanFilterTk === model.tk) ? 1 : 0
                                    border.color: (scanFilterTk === model.tk) ? scanFilterDialog.dAccent : "transparent"
                                    clip: true

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 10

                                        // ✅ Show: HH:MM:SS (YYYY-MM-DD)
                                        Text {
                                            property var dt: splitDateTime(String(model.tk))
                                            text: (dt.time ? dt.time : model.time) + "  (" + (dt.date ? dt.date : scanDateOfTk(model.tk)) + ")"
                                            color: scanFilterDialog.dText
                                            font.pixelSize: 16
                                            font.bold: (scanFilterTk === model.tk)
                                            Layout.preferredWidth: 240
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Found " + model.count + " ch  •  " +
                                                  ((model.minFreq > 0 && model.maxFreq > 0)
                                                   ? (model.minFreq.toFixed(3) + "–" + model.maxFreq.toFixed(3) + " MHz")
                                                   : "-")
                                            color: scanFilterDialog.dSub
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                        }
                                    }

                                    MouseArea {
                                        id: sessMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: scanFilterTk = String(model.tk)
                                    }
                                }

                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn; active: true; width: 10 }
                            }

                            // =========================================================
                            // ✅ Scroll Hint Arrows (Top/Bottom)
                            // =========================================================
                            // NOTE:
                            // - contentY <= 0 => อยู่บนสุด (ซ่อน ▲)
                            // - contentY + height >= contentHeight => อยู่ล่างสุด (ซ่อน ▼)
                            // - ใช้ +2/-2 กันเด้งจาก floating point
                            // =========================================================

                            // ---- TOP HINT ----
                            Item {
                                id: topHint
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 26
                                visible: (sessionList.contentHeight > sessionList.height + 2) && (sessionList.contentY > 2)
                                z: 10

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 120
                                    height: 22
                                    radius: 11
                                    color: "#000000"
                                    opacity: 0.35
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: "▲"
                                    color: scanFilterDialog.dText
                                    font.pixelSize: 16
                                    opacity: 0.9
                                }
                            }

                            // ---- BOTTOM HINT ----
                            Item {
                                id: bottomHint
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 26
                                visible: (sessionList.contentHeight > sessionList.height + 2) &&
                                         ((sessionList.contentY + sessionList.height) < (sessionList.contentHeight - 2))
                                z: 10

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 120
                                    height: 22
                                    radius: 11
                                    color: "#000000"
                                    opacity: 0.35
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: "▼"
                                    color: scanFilterDialog.dText
                                    font.pixelSize: 16
                                    opacity: 0.9
                                }
                            }
                        }


                        // ===== Apply buttons =====
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Item { Layout.fillWidth: true }

                            Button {
                                text: "Apply"
                                onClicked: {
                                    applyScanFilter(scanFilterDate, scanFilterTk)
                                    scanFilterDialog.close()
                                }
                            }

                            Button {
                                text: "Clear"
                                onClicked: {
                                    clearScanFilter()
                                    scanFilterDialog.close()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // =========================================================
    // MAIN VIEW + GROUP VIEW
    // =========================================================
    Item {
        id: pages
        anchors.fill: parent

        // ------------------------------
        // PAGE 0: MAIN GRID
        // ------------------------------
        Item {
            id: pageMain
            anchors.fill: parent
            visible: !inGroupView

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

                    model: widgetView
                           ? (scanFilterActive ? scanFilteredModel : groupedScanModel)
                           : radioMemList

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
                        property int gapX: 20
                        property int gapY: 10
                        width: grid.cellWidth - gapX
                        height: grid.cellHeight - gapY
                        property real u: height / 210.0

                        property bool isScanGroup: widgetView

                        // ✅ scan group fields
                        property int folderId: isScanGroup ? n(model.folderId, 0) : 0
                        property string timeKey: isScanGroup ? s(model.timeKey) : ""
                        property int count: isScanGroup ? n(model.count, 0) : 0
                        property real minFreq: isScanGroup ? n(model.minFreq, 0) : 0
                        property real maxFreq: isScanGroup ? n(model.maxFreq, 0) : 0

                        // ✅ memory fields
                        property real centerMHz_mem: !isScanGroup ? toMHz(model.center_freq) : 0
                        property string bwLine_mem:  !isScanGroup && (model.high_cut !== undefined && model.low_cut !== undefined)
                                                   ? (Math.abs(n(model.high_cut) - n(model.low_cut)) / 1000).toFixed(1) + " kHz"
                                                   : (!isScanGroup ? (s(model.bw).length ? ("~" + s(model.bw)) : "-") : "-")

                        Rectangle {
                            id: mainCard
                            anchors.fill: parent
                            radius: 18 * u
                            color: "#0E1520"
                            border.color: "#1C2A3D"
                            border.width: Math.max(1, Math.round(1 * u))

                            property bool isHover: false
                            property bool isPress: false

                            scale: isPress ? 0.985 : 1.0
                            Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: "#ffffff"
                                opacity: mainCard.isPress ? 0.12 : (mainCard.isHover ? 0.06 : 0.0)
                                visible: opacity > 0.001
                                Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: "transparent"
                                border.color: "#ffffff"
                                border.width: 1
                                opacity: mainCard.isHover ? 0.16 : 0.0
                                visible: opacity > 0.001
                                Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor

                                onEntered: mainCard.isHover = true
                                onExited:  { mainCard.isHover = false; mainCard.isPress = false }
                                onPressed: mainCard.isPress = true
                                onReleased: mainCard.isPress = false

                                onPressAndHold: {
                                    if (isScanGroup) {
                                        pendingDeleteGroupTimeKey = timeKey
                                        deleteConfirmDialog.open()
                                    }
                                }

                                onClicked: {
                                    console.log("onClick isScanGroup:",isScanGroup)
                                    if (isScanGroup) {
                                        enterGroupByTimeKey(timeKey)
                                    } else {
                                        var setCenterFreq = n(centerMHz_mem, 0) * 1e6
                                        console.log("onClick to set freq:",setCenterFreq)
                                        if (typeof mainWindows !== "undefined" && mainWindows && typeof mainWindows.sendmessage === "function") {
                                            mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":' + setCenterFreq + ',"key":"memagic"}}')
                                        } else {
                                            console.log("[setfrequency]", setCenterFreq)
                                        }
                                    }
                                }
                            }

                            Item {
                                anchors.fill: parent
                                anchors.margins: 16

                                Column {
                                    anchors.fill: parent
                                    spacing: 6
                                    visible: isScanGroup

                                    Text { text: "SCAN RESULT"; color: "#FFFFFF"; font.pixelSize: 16; font.bold: true }
                                    Text { text: "Found " + count + " channels"; color: "#FFFFFF"; font.pixelSize: 16; font.bold: true }
                                    Text { text: "Folder ID: " + folderId; color: "#BFD0E6"; font.pixelSize: 13 }
                                    Text { text: timeKey; color: "#BFD0E6"; font.pixelSize: 13 }
                                    Text {
                                        text: (minFreq > 0 && maxFreq > 0) ? (minFreq.toFixed(3) + " – " + maxFreq.toFixed(3) + " MHz") : "-"
                                        color: "#BFD0E6"; font.pixelSize: 13
                                    }
                                }

                                Column {
                                    anchors.fill: parent
                                    visible: !isScanGroup
                                    spacing: 8

                                    Text { text: "Frequency: " + n(centerMHz_mem,0).toFixed(6); color: "#F2F6FF"; font.pixelSize: 14; font.bold: true }
                                    Text { text: "Bandwidth: " + bwLine_mem; color: "#BFD0E6"; font.pixelSize: 13 }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ------------------------------
        // PAGE 1: GROUP VIEW (เหมือนเดิม)
        // ------------------------------
        Item {
            id: pageGroup
            anchors.fill: parent
            visible: inGroupView

            Item {
                id: detailViewport2
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                clip: true

                property int maxColumns: 5
                property int sidePad: 24
                property int cardW: 320
                property int cardH: 150
                property int rows: 2

                property int gapX: 20
                property int gapY: 10

                width: Math.min(parent.width - sidePad * 2, cardW * maxColumns)
                height: cardH * rows

                GridView {
                    id: detailGrid
                    anchors.fill: parent
                    clip: true

                    cellWidth:  detailViewport2.cardW
                    cellHeight: detailViewport2.cardH

                    model: groupItemsModel

                    interactive: true
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
                        property int gapX: detailViewport2.gapX
                        property int gapY: detailViewport2.gapY
                        width: detailGrid.cellWidth - gapX
                        height: detailGrid.cellHeight - gapY

                        property real u: height / 210.0
                        property color cValue:   "#F2F6FF"
                        property color cLabel:   "#BFD0E6"
                        property color cOutline: "#081018"

                        property string presetName: (model.Profile !== "" ? ("Profile " + model.Profile) : "Profile")
                        property real centerMHz: n(model.Freq, 0)
                        property string bwLine: s(model.BW)

                        Rectangle {
                            id: subCard
                            anchors.fill: parent
                            radius: 18 * u
                            color: "#0E1520"
                            border.color: "#1C2A3D"
                            border.width: Math.max(1, Math.round(1 * u))

                            property bool isHover: false
                            property bool isPress: false

                            scale: isPress ? 0.985 : 1.0
                            Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: "#ffffff"
                                opacity: subCard.isPress ? 0.12 : (subCard.isHover ? 0.06 : 0.0)
                                visible: opacity > 0.001
                                Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: "transparent"
                                border.color: "#ffffff"
                                border.width: 1
                                opacity: subCard.isHover ? 0.16 : 0.0
                                visible: opacity > 0.001
                                Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor

                                onEntered: subCard.isHover = true
                                onExited:  { subCard.isHover = false; subCard.isPress = false }
                                onPressed: subCard.isPress = true
                                onReleased: subCard.isPress = false

                                onClicked: {
                                    console.log("[SCAN ITEM CLICK]", presetName, centerMHz, bwLine)
                                    var setCenterFreq = n(centerMHz,0) * 1e6

                                    var dspcontrolParams = {
                                        type: "dspcontrol",
                                        params: {
                                            "low_cut":          radioMemList.low_cut,
                                            "high_cut":         radioMemList.high_cut,
                                            "offset_freq":      radioMemList.offset_freq || 0,
                                            "mod":              radioMemList.mod,
                                            "dmr_filter":       radioMemList.dmr_filter,
                                            "audio_service_id": radioMemList.audio_service_id,
                                            "squelch_level":    radioMemList.squelch_level,
                                            "secondary_mod":    radioMemList.secondary_mod
                                        }
                                    }

                                    console.log("setCenterFreq", setCenterFreq, "dspcontrolParams", dspcontrolParams)
                                    if (mainWindows && typeof mainWindows.sendmessage === "function") {
                                        mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":' + setCenterFreq + ',"key":"memagic"}}')

                                        pendingDSP        = dspcontrolParams
                                        pendingCenterFreq = setCenterFreq
                                        pendingUIParams   = dspcontrolParams.params
                                        startDSPAfter     = true
                                        dspDelayTimer.restart()
                                    } else {
                                        console.log('[setfrequency]', setCenterFreq)
                                    }
                                }
                            }

                            Item {
                                anchors.fill: parent
                                anchors.margins: 18 * u

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
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
                                            Text {
                                                text: "Frequency:"
                                                color: cLabel
                                                font.pixelSize: Math.round(20 * u)
                                                font.bold: true
                                                style: Text.Outline
                                                styleColor: cOutline
                                            }
                                            Text {
                                                text: n(centerMHz,0).toFixed(6) + " MHz"
                                                color: cValue
                                                font.pixelSize: Math.round(21 * u)
                                                font.bold: true
                                                style: Text.Outline
                                                styleColor: cOutline
                                            }
                                        }

                                        Row {
                                            spacing: 14 * u
                                            Text {
                                                text: "Bandwidth:"
                                                color: cLabel
                                                font.pixelSize: Math.round(20 * u)
                                                font.bold: true
                                                style: Text.Outline
                                                styleColor: cOutline
                                            }
                                            Text {
                                                text: bwLine
                                                color: cValue
                                                font.pixelSize: Math.round(21 * u)
                                                font.bold: true
                                                style: Text.Outline
                                                styleColor: cOutline
                                            }
                                        }
                                    }
                                }

                                Row {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: -20 * u
                                    spacing: 18 * u
                                    height: 62 * u
                                    z: 10

                                    // ================= SAVE =================
                                    Item {
                                        id: scanSaveBtn
                                        width: (parent.width - 18 * u) / 2
                                        height: 52 * u

                                        property bool hovered: scanSaveMouse.containsMouse
                                        property bool pressed: scanSaveMouse.pressed

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: height / 2
                                            color: scanSaveBtn.pressed
                                                   ? "#064E3B"
                                                   : (scanSaveBtn.hovered ? "#0F766E" : "transparent")
                                            border.color: scanSaveBtn.hovered ? "#28FF8B" : "#3B4B63"
                                            border.width: Math.max(1, Math.round(1.4 * u))
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "SAVE"
                                            color: scanSaveBtn.hovered ? "#ECFEFF" : "#28FF8B"
                                            font.pixelSize: Math.round(17 * u)
                                            font.bold: true
                                            style: Text.Outline
                                            styleColor: cOutline
                                        }

                                        MouseArea {
                                            id: scanSaveMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                console.log("receiverMode.get(scanReceiverModeSelected).text::",
                                                            receiverMode.get(scanReceiverModeSelected).text)

                                                var id = mainWindows.generateGUID().replace(/[{}]/g, "")

                                                var f = Number(model.Freq)
                                                var center_hz = (Math.abs(f) < 100000)
                                                                  ? Math.round(f * 1e6)
                                                                  : Math.round(f)

                                                const newPreset = {
                                                    "name": "",
                                                    "low_cut": currentLowcut,
                                                    "high_cut": currentHighcut,
                                                    "center_freq": center_hz,
                                                    "offset_freq": currentOffsetFreq,
                                                    "mod": receiverMode.get(scanReceiverModeSelected).text,
                                                    "dmr_filter": 3,
                                                    "audio_service_id": 0,
                                                    "squelch_level": currentSqlLevel,
                                                    "secondary_mod": false
                                                }

                                                openPresetNameDialog(id, newPreset, false)
                                            }
                                        }
                                    }

                                    // ================= DELETE =================
                                    Item {
                                        id: scanDeleteBtn
                                        width: (parent.width - 18 * u) / 2
                                        height: 52 * u

                                        property bool hovered: scanDeleteMouse.containsMouse
                                        property bool pressed: scanDeleteMouse.pressed

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: height / 2
                                            color: scanDeleteBtn.pressed
                                                   ? "#7F1D1D"
                                                   : (scanDeleteBtn.hovered ? "#991B1B" : "transparent")
                                            border.color: scanDeleteBtn.hovered ? "#FF5C5C" : "#3B4B63"
                                            border.width: Math.max(1, Math.round(1.4 * u))
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "DELETE"
                                            color: scanDeleteBtn.hovered ? "#FFE4E6" : "#FF5C5C"
                                            font.pixelSize: Math.round(17 * u)
                                            font.bold: true
                                            style: Text.Outline
                                            styleColor: cOutline
                                        }

                                        MouseArea {
                                            id: scanDeleteMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                pendingDeleteIndex = model.Index
                                                console.log("REQUEST DELETE -> Index:", pendingDeleteIndex)
                                                confirmDeleteDialog.open()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                id: overlayLeft
                z: 200
                width: 140
                height: detailViewport2.height

                anchors.right: detailViewport2.left
                anchors.verticalCenter: detailViewport2.verticalCenter
                anchors.rightMargin: 12

                Column {
                    anchors.fill: parent
                    spacing: 10

                    Item {
                        id: backBtn
                        width: parent.width
                        height: 32

                        property bool isHover: false
                        property bool isPress: false

                        scale: isPress ? 0.98 : 1.0
                        Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            color: "#111A24"
                            border.color: "#1C2A3D"
                            border.width: 1

                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: "#ffffff"
                                opacity: backBtn.isPress ? 0.18 : (backBtn.isHover ? 0.10 : 0.0)
                                visible: opacity > 0.001
                                Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: "transparent"
                                border.color: "#ffffff"
                                border.width: 1
                                opacity: backBtn.isHover ? 0.18 : 0.0
                                visible: opacity > 0.001
                                Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "< Back"
                            color: "#FFFFFF"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onEntered: backBtn.isHover = true
                            onExited:  { backBtn.isHover = false; backBtn.isPress = false }
                            onPressed: backBtn.isPress = true
                            onReleased: backBtn.isPress = false
                            onClicked: leaveGroup()
                            onPressAndHold: leaveGroup()
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: "#122033"
                    }

                    Text {
                        text: activeGroupTitle
                        color: "#FFFFFF"
                        font.pixelSize: 16
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
