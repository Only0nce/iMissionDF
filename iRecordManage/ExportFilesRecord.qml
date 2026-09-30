// ExportFilesRecord.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import "../ui"

Item {
    id: exportFilesRecordroot
    width: 700
    height: 500

    readonly property bool hmiDarkMode: Material.theme === Material.Dark
    Theme { id: hmiTheme; darkMode: exportFilesRecordroot.hmiDarkMode }

    // ==== รับค่าจาก exportOverlay ====
    property var    files: []          // [{full_path, size, duration_sec}, ...]
    property var mountPoint: label ? pathToSave : label     // "/media/usb_sda"  ← แก้จาก var label
    property string defaultName: ""    // ถ้า user ไม่กรอก
    property var    qmlCommandFn: null // main.qml: window.qmlCommand

    // ให้ข้างนอกสั่งปิดเราได้
    signal requestClose()

    // summary
    property int   fileCount: 0
    property int   totalDurationSec: 0
    property real  totalSizeKBytes: 0
    property bool  exporting: false
    property real  progress: 0.0
    property string statusText: ""
    property string exportFrequencyHz: targetFrequencyHz
    property real   exportFrequencyMHz: targetFrequencyMHz

//    onExportFrequencyHzChanged: {
//        console.log("[RecordFiles] send freq to exportFrequencyHz:", exportFrequencyHz)
//    }
    onExportFrequencyHzChanged: {
        console.log("[ExportFilesRecord] Hz arrived:", exportFrequencyHz)

        // สร้างชื่อไฟล์ = yyyyMMdd_HHmmss_Hz
        var base = Qt.formatDateTime(new Date(), "yyyyMMdd_HHmmss")
        defaultName = base + "_" + exportFrequencyHz

        // 🔴 สำคัญที่สุด: ใส่ลง TextField ตรง ๆ
        nameField.text = defaultName
    }

    onExportFrequencyMHzChanged: {
        console.log("[RecordFiles] send freq to exportFrequencyMHz:", exportFrequencyMHz)
    }

    function resetState() {
        exporting  = false
        progress   = 0
        statusText = ""
    }
    Component.onCompleted: resetState()
    onFilesChanged: {
        resetState()

        var cnt = 0, dur = 0, sz = 0
        if (files && files.length) {
            cnt = files.length
            for (var i = 0; i < files.length; ++i) {
                var f = files[i]; if (!f) continue
                if (typeof f.duration_sec === "number") dur += f.duration_sec
                if (typeof f.size === "number") sz += f.size
            }
        }
        fileCount = cnt
        totalDurationSec = Math.floor(dur)
        totalSizeKBytes   = sz
    }

    function formatDuration(sec) {
        sec = Math.floor(sec)
        var h = Math.floor(sec / 3600)
        var m = Math.floor((sec % 3600) / 60)
        var s = sec % 60
        function pad(n) { return (n < 10 ? "0" + n : "" + n) }
        return pad(h) + ":" + pad(m) + ":" + pad(s)
    }

    function formatSize(bytes) {
        var b = Number(bytes)
        if (isNaN(b) || b <= 0) return "0 KB"
        var units = ["KB","MB","GB","TB"]
        var u = 0
        while (b >= 1024 && u < units.length-1) { b /= 1024; ++u }
        return b.toFixed(1) + " " + units[u]
    }


    function updateProgress(percent, message) {
        progress = Math.max(0, Math.min(100, percent || 0))
        statusText = message || ("Exporting... " + Math.round(progress) + "%")

        if (progress >= 100) {
            exporting = false
            if (!message || !message.length)
                statusText = "Export completed."
        }
    }
//    function updateProgress(percent, message) {
//        console.log("[ExportFilesRecord] updateProgress", percent, message)   // << เพิ่มบรรทัดนี้

//        progress = Math.max(0, Math.min(100, percent || 0))
//        statusText = message || ("Exporting... " + Math.round(progress) + "%")
//        if (progress >= 100) {
//            exporting = false
//            statusText = "Export completed successfully."
//        }
//    }


    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: "Export Selected Files"
            font.pixelSize: 22
            font.bold: true
            color: hmiTheme.text
            Layout.fillWidth: true
        }

        Label {
            // ใช้ mountPoint จริง ๆ ที่ส่งมาจาก RecordFiles.qml
            text: pathToSave && pathToSave.length > 0
                  ? ("Target: " + pathToSave)
                  : "Target: (no device selected)"
            color: hmiTheme.muted
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            radius: hmiTheme.radiusMd
            color: hmiTheme.cardAlt
            border.color: hmiTheme.line
            implicitHeight: colSummary.implicitHeight + 12

            ColumnLayout {
                id: colSummary
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Label {
                    text: "Files: " + fileCount
                    color: hmiTheme.textSecondary
                }

                Label {
                    text: "Total duration: " + formatDuration(totalDurationSec)
                    color: hmiTheme.textSecondary
                }

                Label {
                    text: "Total size: " + formatSize(totalSizeKBytes)
                    color: hmiTheme.textSecondary
                }
            }
        }

        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            Label { text: "Output file name"; color: hmiTheme.textSecondary; font.bold: true }
            HmiTextField {
                id: nameField
                darkMode: exportFilesRecordroot.hmiDarkMode
                Layout.fillWidth: true
                text: defaultName
                placeholderText: "auto: date_time"
                fontPixelSize: 16
                emphasized: true
            }

        }

        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Status"; color: hmiTheme.textSecondary; font.bold: true }
                Item { Layout.fillWidth: true }
                HmiStatusPill {
                    darkMode: exportFilesRecordroot.hmiDarkMode
                    compact: true
                    text: exporting ? "EXPORTING" : (progress >= 100 ? "COMPLETE" : "READY")
                    tone: exporting ? "info" : (progress >= 100 ? "good" : "neutral")
                }
            }
            ProgressBar {
                id: exportProgressBar
                Layout.fillWidth: true
                from: 0; to: 100
                value: progress
                background: Rectangle {
                    implicitHeight: 10
                    radius: 5
                    color: hmiTheme.input
                    border.color: hmiTheme.line
                }
                contentItem: Item {
                    implicitHeight: 10
                    Rectangle {
                        width: exportProgressBar.visualPosition * parent.width
                        height: parent.height
                        radius: 5
                        color: progress >= 100 ? hmiTheme.success : hmiTheme.accent
                    }
                }

                Behavior on value {
                    NumberAnimation { duration: 200 }
                }
            }

            Label {
                text: statusText || (exporting ? "Exporting..." : "Idle")
                color: hmiTheme.muted
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            HmiButton {
                text: qsTr("CANCEL")
                compact: true
                darkMode: exportFilesRecordroot.hmiDarkMode
                enabled: !exporting
                onClicked: {
                    var p = exportFilesRecordroot.parent
                    while (p) {
                        if (p.close && typeof p.close === "function") {
                            p.close()
                            return
                        }
                        if (p.visible !== undefined && p.z !== undefined) {

                        }
                        p = p.parent
                    }
                    exportFilesRecordroot.requestClose()
                    console.log("[exportFilesRecordroot] ->> requestClose:", p)

                }
//                onClicked:{

//                    exportFilesRecordroot.requestClose()
//                }
            }

            HmiButton {
                text: exporting ? qsTr("SAVING...") : qsTr("SAVE")
                compact: true
                tone: "primary"
                darkMode: exportFilesRecordroot.hmiDarkMode
                enabled: !exporting && fileCount > 0 && pathToSave !== ""
                onClicked: {
//                    console.log("[mountPoint]",mountPoint,"label",label,"pathToSave",pathToSave)

                    var name = nameField.text.trim()
                    if (!name.length) {
                        name = defaultName || Qt.formatDateTime(new Date(), "yyyyMMdd_hhmmss")
                    }
                    if (!files || !files.length) {
                        console.warn("[ExportFilesRecord] no files to export")
                        statusText = "No files to export."
                        return
                    }
                    function esc(str) {
                        str = String(str)
                        return str
                                .replace(/\\/g, "\\\\")
                                .replace(/"/g, "\\\"")
                    }

                    // ---------- สร้าง JSON string ด้วยมือ (ไม่ใช้ JSON.stringify) ----------
                    var json = "{"
                             + "\"menuID\":\"exportMergeFilesToUSB\","
                             + "\"mountPoint\":\"" + esc(pathToSave) + "\","   // ❗ ใส่เป็น string
                             + "\"fileName\":\"" + esc(name) + "\","      // ❗ ใส่เป็น string
                             + "\"files\":["

                    var first = true
                    for (var i = 0; i < files.length; ++i) {
                        var f = files[i]
                        if (!f) continue

                        // เลือก path ที่ C++ ใช้ได้จริง
                        var p = f.full_path || f.path || ""
                        if (!p.length) continue

                        if (!first) json += ","
                        first = false

                        json += "\"" + esc(p) + "\""
                    }

                    json += "]}"
                    exporting  = true
                    progress   = 0
                    statusText = "Export started..."
//                    console.log("[ExportFilesRecord] send:", json)
                    qmlCommandFn(json)

                }
            }

            HmiButton {
                text: qsTr("OK")
                compact: true
                tone: "primary"
                darkMode: exportFilesRecordroot.hmiDarkMode
                visible: !exporting && progress >= 100

                onClicked: {
                    // ปิด popup ตรง ๆ (parent ของ item ใน Loader คือ exportPanel/loader item tree)
                    var p = exportFilesRecordroot.parent
                    while (p) {
                        if (p.close && typeof p.close === "function") {
                            p.close()
                            return
                        }
                        if (p.visible !== undefined && p.z !== undefined) {
                            // ถ้าเจอ overlay ที่มี visible (อย่าง exportOverlay)
                            // คุณอาจตั้งชื่อ id เป็น exportOverlay แล้วปิดตรงนั้นได้ด้วย
                        }
                        p = p.parent
                    }

                    // fallback: ยิง signal เดิม
                    exportFilesRecordroot.requestClose()
                }
            }

        }
    }
}


