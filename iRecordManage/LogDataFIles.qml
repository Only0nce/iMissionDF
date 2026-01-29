import QtQuick 2.0
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtWebSockets 1.0
import QtQuick.Extras 1.4
import QtQuick.Controls 1.4 as C1
import QtQuick.VirtualKeyboard 2.15
import QtQuick.VirtualKeyboard.Styles 2.15
import QtQuick.VirtualKeyboard.Settings 2.15
import QtGraphicalEffects 1.0
import QtQuick.Controls.Styles 1.4

Item {
    id: logdataFileRecorder
    width: 1800
    height: 900

    signal pageRequested(int page)

    property int pageWindowSize: 10

    Rectangle {
        id: rectangleRecord
        anchors.fill: parent
        color: "#ffffff"

        Flickable {
            id: flickable
            anchors.fill: parent
            clip: true

            Rectangle {
                id: viewsLogEvent
                anchors.fill: parent
                anchors.bottomMargin: 50
                color: "#e7e6e6"
                border.color: "#ffffff"

                C1.TableView {
                    id: recordFileSDatebase
                    anchors.fill: parent
                    clip: true
                    model: listFileRecord

                    // ===== ปรับสัดส่วนที่นี่ =====
                    property int rowH: 56
                    property int headerH: 44
                    property int cellPt: 16
                    property int headerPt: 16
                    property int padL: 12

                    // สีสลับแถว + ความสูง
                    rowDelegate: Rectangle {
                        height: recordFileSDatebase.rowH
                        color: styleData.alternate ? "#f7f7f7" : "#ffffff"
                    }

                    // หัวตาราง
                    headerDelegate: Rectangle {
                        height: recordFileSDatebase.headerH
                        color: "#f0f2f5"
                        border.color: "#dcdcdc"
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: recordFileSDatebase.padL
                            anchors.rightMargin: 8
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: styleData.value
                            font.pointSize: recordFileSDatebase.headerPt
                            color: "#333333"
                        }
                    }

                    // เนื้อหาเซลล์
                    itemDelegate: Item {
                        anchors.fill: parent
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: recordFileSDatebase.padL
                            anchors.rightMargin: 8
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: styleData.value
                            font.pointSize: recordFileSDatebase.cellPt
                            color: "#111111"
                        }
                    }

                    // ===== คอลัมน์ =====
                    C1.TableViewColumn {
                        role: "selected"
                        title: ""
                        width: 56

                        delegate: Item {
                            anchors.fill: parent

                            // กล่องติ๊ก (สีเขียว/ขาว)
                            Rectangle {
                                id: box
                                anchors.centerIn: parent
                                width: 45
                                height: 45
                                radius: 10
                                border.width: 1
                                border.color: styleData.value ? "#10b981" : "#9aa3af"
                                color:        styleData.value ? "#34d399" : "#ffffff"

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: parent.width  - 6
                                    height: parent.height - 6
                                    radius: 3
                                    color: styleData.value ? "#34d399" : "transparent"
                                    opacity: styleData.value ? 0.7 : 0.0
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    // 1) toggle ค่าที่ model + logic เดิม
                                    var cur  = !!styleData.value
                                    var next = !cur

                                    listFileRecord.setProperty(styleData.row, "selected", next)
                                    window.toggleSelection(styleData.row, next)

                                    var it = listFileRecord.get(styleData.row)
                                    console.log("[SelectBox]", next ? "✔" : "✖",
                                                "row:", styleData.row,
                                                "idDevice:", String(it.idDevice || it.filename || it.file_path || styleData.row),
                                                "filename:", String(it.filename || ""))

                                    // 2) ดึงไฟล์ทั้งหมดที่ถูกเลือกอยู่ตอนนี้ (reuse function เดิม)
                                    var items = []
                                    if (collectSelectedFiles) {
                                        items = collectSelectedFiles()
                                    }

//                                    console.log("[AutoSelectFiles] items.length =", items.length)

                                    if (items.length > 0) {
                                        editor.setFiles(items)   // ✅ คำนวณ summary เสร็จใน editor แล้ว

                                        // ✅ ทำ list path เป็น string ล้วน
                                        var paths = []
                                        for (var i = 0; i < selectedItems.length; ++i) {
                                            var it = selectedItems[i]
                                            if (it && it.full_path)
                                                paths.push(it.full_path)
                                        }

                                        console.log("[SAVE] paths =", paths)
                                        var safeKB = isFinite(totalsizeKB) ? totalsizeKB : 0.0
                                        // ✅ SAVE ทั้ง list + summary
                                        if (fileReader && fileReader.saveWaveSelectionState) {
                                            fileReader.saveWaveSelectionState(
                                                paths,
                                                editor.totalFiles,
                                                editor.sampleCount,
                                                editor.durationMs,          // totalMs
                                                editor.totalDurationSec,
                                                editor.samples.length,
                                                safeKB                // ✅ เพิ่มตัวนี้
                                            )
                                        }

                                        freezeRecordFilesUpdate = true
                                    } else {
                                        editor.setFiles([])
                                        freezeRecordFilesUpdate = false

                                        // clear file
                                        if (fileReader && fileReader.saveWaveSelectionState) {
                                            fileReader.saveWaveSelectionState([], 0, 0, 0, 0.0, 0)
                                        }
                                    }

                                }
                            }

//=======================================================================================
//                            MouseArea {
//                                anchors.fill: parent
//                                onClicked: {
//                                    // 1) toggle ค่าที่ model + logic เดิม
//                                    var cur  = !!styleData.value
//                                    var next = !cur

//                                    listFileRecord.setProperty(styleData.row, "selected", next)
//                                    window.toggleSelection(styleData.row, next)

//                                    var it = listFileRecord.get(styleData.row)
//                                    console.log("[SelectBox]", next ? "✔" : "✖",
//                                                "row:", styleData.row,
//                                                "idDevice:", String(it.idDevice || it.filename || it.file_path || styleData.row),
//                                                "filename:", String(it.filename || ""))

//                                    // 2) ดึงไฟล์ทั้งหมดที่ถูกเลือกอยู่ตอนนี้ (reuse function เดิม)
//                                    var items = []
//                                    if (collectSelectedFiles) {
//                                        items = collectSelectedFiles()
//                                    }

//                                    console.log("[AutoSelectFiles] items.length =", items.length)

//                                    if (items.length > 0) {
//                                        // ส่งให้ WaveEditor ทันที
//                                        editor.setFiles(items)
//                                        console.log("[AutoSelectFiles] editor.setFiles(...) called")
//                                        // ถ้าอยาก freeze ไม่ให้ recordFiles update ขณะกำลังเลือก
//                                        freezeRecordFilesUpdate = true
//                                    } else {
//                                        // ไม่มีไฟล์ถูกเลือกแล้ว → เคลียร์ใน WaveEditor ก็ได้
//                                        editor.setFiles([])
//                                        console.log("[AutoSelectFiles] no items -> clear editor")
//                                        freezeRecordFilesUpdate = false
//                                    }
//                                }
//                            }

//=======================================================================================
                        }
                    }
                    C1.TableViewColumn { role: "device";       title: "device";      width: 100 }
                    C1.TableViewColumn { role: "filename";     title: "File Name";   width: 720 }
                    C1.TableViewColumn { role: "size";         title: "Size(KB)";    width: 120 }
                    C1.TableViewColumn { role: "duration_sec"; title: "Duration(s)"; width: 150 }
                    C1.TableViewColumn { role: "created_at";   title: "Created at";  width: 320 }
                    C1.TableViewColumn { role: "name";         title: "Name";        width: 200 }
                    C1.TableViewColumn { role: "parsed_date";  title: "Parsed Date"; width: 200 }
                }
            }

            // ScrollBar (Controls 2) ใช้งานกับ Flickable ได้
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
        }

        // ===== เพจจิ้ง (แสดงทีละ 10 หน้า) =====
        Row {
            id: pagination
            spacing: 8
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10

            // First / Prev
            Button {
                text: "\u00AB First"
                enabled: currentPage > 1
                onClicked: sendChangePage(1)
            }
            Button {
                text: "\u2039 Prev"
                enabled: currentPage > 1
                onClicked: sendChangePage(currentPage - 1)
            }

            // ปุ่มเลขหน้า — สร้างจาก visiblePagesList()
            Repeater {
                model: visiblePagesList()
                delegate: Button {
                    property int pageNo: modelData
                    text: pageNo
                    checkable: true
                    checked: currentPage === pageNo
                    onClicked: if (currentPage !== pageNo) sendChangePage(pageNo)
                }
            }

            // Next / Last
            Button {
                text: "Next \u203A"
                enabled: currentPage < totalPages
                onClicked: sendChangePage(currentPage + 1)
            }
            Button {
                text: "Last \u00BB"
                enabled: currentPage < totalPages
                onClicked: sendChangePage(totalPages)
            }
        }
    }

    // ===== Helper =====
    function selectedCount() {
        var c = 0
        for (var i = 0; i < listFileRecord.count; ++i)
            if (listFileRecord.get(i).selected) c++
        return c
    }

    function selectAllRows(checked) {
        for (var i = 0; i < listFileRecord.count; ++i)
            listFileRecord.setProperty(i, "selected", checked)
    }

    // ส่งไป C++ ขอเพจใหม่ (ใช้ค่าจาก main.qml)
    // ส่งไป C++ ขอเพจใหม่ (ใช้ค่าจาก UI เดิมที่มีอยู่แล้ว)
    function sendChangePage(p) {
        if (enableSearch === false) {
            // โหมดเลื่อนหน้าปกติ (ไม่ฟิลเตอร์)
            var payload = {
                menuID: "ChangeNextPageOfRecord",
                page: p,
                pageSize: 25,
                interrupSearch: false
            }
            qmlCommand(JSON.stringify(payload))
            pageRequested(p)

        } else {
            // โหมดเลื่อนหน้าพร้อมคงเงื่อนไขค้นหา (ใช้ตัวแปรเดิมทั้งหมด)
            var deviceStr = (typeof deviceNumBox !== "undefined" && deviceNumBox)
                            ? (deviceNumBox.currentText || "")
                            : (typeof deviceTexte !== "undefined" ? (deviceTexte || "") : "")

            var s = new Date(startDT)
            var e = new Date(endDT)
            if (s > e) { var tmp = s; s = e; e = tmp }

            var payload = {
                menuID: "ChangeNextPageOfRecord",
                page: p,
                pageSize: 25,
                interrupSearch: true,
                // คีย์เดิมทั้งหมด (อย่าเปลี่ยนชื่อ)
                device: deviceStr,
                startDate: tfStart.text,
                endDate: tfEnd.text,
                startISO: Qt.formatDateTime(s, "yyyy-MM-ddTHH:mm:ss"),
                endISO:   Qt.formatDateTime(e, "yyyy-MM-ddTHH:mm:ss"),
                interval: intervalMins.toString()
            }
            qmlCommand(JSON.stringify(payload))
            pageRequested(p)
        }
    }


    function visiblePagesList() {
        var cp  = currentPage || 1
        var tp  = totalPages  || 1
        var win = pageWindowSize || 10

        var start = Math.floor((cp - 1) / win) * win + 1
        var end   = Math.min(tp, start + win - 1)

        var a = []
        for (var p = start; p <= end; ++p) a.push(p)

        if (a.length === 0) a.push(cp)
        return a
    }
    function buildFullPathFromFilename(filename, baseDir) {

        const re = /^([^_]+)_(\d{8})_.+\.wav$/;
        const m = filename.match(re);
        if (!m) return "";                         // ถ้ารูปแบบไม่ตรงก็คืนว่าง
        const device = m[1];
        const ymd    = m[2];                        // 20251006
        return baseDir + "/" + device + "/" + ymd + "/" + filename;
    }
    function clearAllChecks() {
        if (!listFileRecord || listFileRecord.count <= 0) return;
        for (var i = 0; i < listFileRecord.count; ++i) {
            listFileRecord.setProperty(i, "selected", false);
        }
    }

    function uncheckAll() {
//        console.log("uncheckAll()")
        var m = recordFileSDatebase.model
        if (!m || m.count <= 0) {
//            console.log("uncheckAll: model empty")
            return
        }

//        console.log("uncheckAll: model.count =", m.count)
        for (var i = 0; i < m.count; ++i) {
            if (m.get(i).selected)
//                console.log("  -> clear row", i, "was selected")
            m.setProperty(i, "selected", false)
        }

        if (recordFileSDatebase.forceLayout) {
            recordFileSDatebase.forceLayout()
        } else {
            var keep = m
            recordFileSDatebase.model = null
            Qt.callLater(function(){ recordFileSDatebase.model = keep })
        }
    }


    function resetChecksUI() {
        var m = recordFileSDatebase.model
        if (!m || m.count <= 0) return

        // set role เป็น false ทั้งหมด (กันพลาด)
        for (var i = 0; i < m.count; ++i)
            m.setProperty(i, "selected", false)

        if (recordFileSDatebase.forceLayout) {
            recordFileSDatebase.forceLayout()
        } else {
            // fallback: ถอด/ใส่ model เพื่อรีไซเคิล delegate
            var keep = m
            recordFileSDatebase.model = null
            Qt.callLater(function(){ recordFileSDatebase.model = keep })
        }
    }
    function isRecordSelected(idDevice, filename, created_at) {
        var key = makeKey(idDevice, filename, created_at)
        for (var i = 0; i < selectedItems.length; ++i) {
            if ((selectedItems[i].key || "") === key)
                return true
        }
        return false
    }

    function applyRecordFilesChunk(obj) {
        var recs = obj.records || []
//        console.log("[LogDataFiles] applyRecordFilesChunk, recs =", recs.length)

        listFileRecord.clear()

        for (var i = 0; i < recs.length; ++i) {
            var r = recs[i]

            var idStr  = String(r.idDevice || r.device || "")
            var fname  = String(r.filename || "")
            var ctime  = String(r.created_at || "")

            // ★★★ เช็คว่าก้อนนี้เคยถูกเลือกไว้ไหม ★★★
            var wasSelected = isRecordSelected(idStr, fname, ctime)

            listFileRecord.append({
                idDevice:         idStr,
                device:           String(r.device           || ""),
                filename:         fname,
                created_at:       ctime,
                continuous_count: String(r.continuous_count || ""),
                file_path:        String(r.file_path        || ""),
                full_path:        String(r.full_path        || ""),
                name:             String(r.name             || ""),
                size:             String(r.size             || ""),
                duration_sec:     String(r.duration_sec     || ""),
                parsed_date:      String(r.parsed_date      || ""),
                selected:         wasSelected       // <<=== สำคัญสุด
            })
        }

//        console.log("[LogDataFiles] after apply, listFileRecord.count =", listFileRecord.count)
    }

}
