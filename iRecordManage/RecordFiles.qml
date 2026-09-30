//RecordFiles.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.5 as C2
import QtGraphicalEffects 1.15
import "."
import "../ui"

Item {
    id: recordFiles
    width: parent ? parent.width : 1980
    height: parent ? parent.height : 1080

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
    property bool isDarkTheme: Material.theme === Material.Dark
    readonly property bool compactLayout: width < 1500
    readonly property bool shortLayout: height < 720
    Theme { id: hmiTheme; darkMode: recordFiles.isDarkTheme }
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
        isDarkTheme: recordFiles.isDarkTheme
//        listoFDevice: listoFDevice
        deviceTexte: deviceTexte
        customMode: customMode
        presetDays: presetDays
    }

    onStatusSearchingFromMainChanged: {
//        console.log("statusSearchingFromMain changed:", statusSearchingFromMain)

        if (!searchStatusPopup.visible)
            return

        // อัปเดตข้อความใน popup ตามสถานะล่าสุด
        popupStatusText.text = statusSearchingFromMain

        if (statusSearchingFromMain === "Done") {
            popupCloseTimer.start()
        }
    }

    onStatusScanFromMainChanged: {
//        console.log("[RecordFiles] statusScanFromMainChanged:", statusScanFromMain)

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
    function handleUnmountDone(ok) {
        if (ok) {
            deviceFound = false
            window.statusScan = ""
            popupScanText.text = ""
            scanStatusPopup.close()
        }
    }

    /* ================== Recorder workspace ================== */
    Rectangle {
        anchors.fill: parent
        color: hmiTheme.page

        LogDataFIles {
            id: logDataFIles
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: editor.top
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: recordFiles.shortLayout ? 196 : (recordFiles.compactLayout ? 220 : 198)
            anchors.bottomMargin: 10
        }

        function uncheckAllChecks() {
            logDataFIles.uncheckAll()
        }

        WaveEditor {
            id: editor
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.bottomMargin: 10
            height: recordFiles.shortLayout ? 190 : (recordFiles.compactLayout ? 250 : 276)
            isDarkTheme: recordFiles.isDarkTheme
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
            radius: hmiTheme.radiusSm
            color: hmiTheme.cardAlt
            border.width: 1
            border.color: hmiTheme.lineStrong
        }

        Text {
            id: popupScanText
            anchors.centerIn: parent
            color: hmiTheme.text
            font.pixelSize: 14
            font.bold: true
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
            radius: hmiTheme.radiusSm
            color: hmiTheme.cardAlt
            border.width: 1
            border.color: hmiTheme.lineStrong
        }

        Text {
            id: popupStatusText
            anchors.centerIn: parent
            color: hmiTheme.text
            font.pixelSize: 14
            font.bold: true
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

    // ================== Search / filter command panel ==================
    HmiPanel {
        id: filterPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 8
        height: recordFiles.shortLayout ? 180 : (recordFiles.compactLayout ? 204 : 180)
        darkMode: recordFiles.isDarkTheme
        z: 2

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Label {
                    text: qsTr("Recording Browser")
                    color: hmiTheme.text
                    font.pixelSize: 18
                    font.bold: true
                }

                HmiStatusPill {
                    darkMode: recordFiles.isDarkTheme
                    compact: true
                    text: enableSearch ? "FILTER ACTIVE" : "LIVE LIST"
                    tone: enableSearch ? "info" : "good"
                }

                HmiStatusPill {
                    darkMode: recordFiles.isDarkTheme
                    compact: true
                    text: deviceFound ? "USB READY" : "USB NOT MOUNTED"
                    tone: deviceFound ? "good" : "neutral"
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: qsTr("Select recordings below to load the Wave Editor")
                    color: hmiTheme.muted
                    font.pixelSize: 11
                    visible: !recordFiles.compactLayout
                }
            }

            RowLayout {
                spacing: 10
                Layout.fillWidth: true

                // -------- Device ----------
                ColumnLayout {
                    Layout.preferredWidth: recordFiles.compactLayout ? 150 : 185
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "Device"; color: hmiTheme.textSecondary; font.pixelSize: 11; font.bold: true }

                    HmiComboBox {
                        id: deviceNumBox
                        darkMode: recordFiles.isDarkTheme
                        property var sourceModel: listoFDevice
                        property var ids: []
                        model: ids
                        font.pixelSize: 15
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        background: Rectangle {
                            radius: hmiTheme.radiusSm
                            color: hmiTheme.input
                            border.width: 1
                            border.color: deviceNumBox.activeFocus ? hmiTheme.accent : hmiTheme.lineStrong
                        }

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
                    Layout.preferredWidth: recordFiles.compactLayout ? 225 : 270
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "Start Date / Time"; color: hmiTheme.textSecondary; font.pixelSize: 11; font.bold: true }
                    RowLayout {
                        spacing: 6
                        HmiTextField {
                            id: tfStart
                            darkMode: recordFiles.isDarkTheme
                            text: startText
                            readOnly: true
                            fontPixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: calendarOverlay.openFor("start")
                            }
                        }
                        HmiButton {
                            text: "CAL"
                            compact: true
                            darkMode: recordFiles.isDarkTheme
                            Layout.preferredWidth: 54
                            Layout.preferredHeight: 42
                            onClicked: calendarOverlay.openFor("start")
                        }
                    }
                }

                // -------- Interval ----------
                ColumnLayout {
                    Layout.preferredWidth: recordFiles.compactLayout ? 155 : 180
                    spacing: 4
                    Label { text: "Interval"; color: hmiTheme.textSecondary; font.pixelSize: 11; font.bold: true }
                    HmiComboBox {
                        id: cbInterval
                        darkMode: recordFiles.isDarkTheme
                        model: ["Same Time", "+5 minutes", "+10 minutes", "+15 minutes", "+30 minutes", "+60 minutes", "Custom..."]
                        currentIndex: 0
                        font.pixelSize: 14
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        background: Rectangle {
                            radius: hmiTheme.radiusSm
                            color: hmiTheme.input
                            border.width: 1
                            border.color: cbInterval.activeFocus ? hmiTheme.accent : hmiTheme.lineStrong
                        }

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
                    Layout.preferredWidth: recordFiles.compactLayout ? 210 : 250
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "End Date / Time"; color: hmiTheme.textSecondary; font.pixelSize: 11; font.bold: true }
                    HmiTextField {
                        id: tfEnd
                        darkMode: recordFiles.isDarkTheme
                        text: endText
                        readOnly: true
                        fontPixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                    }
                }

                // -------- Export target ----------
                ColumnLayout {
                    id: toolsColumn
                    Layout.preferredWidth: recordFiles.compactLayout ? 235 : 300
                    Layout.fillWidth: true
                    spacing: 4
                    Label { text: "Export Device"; color: hmiTheme.textSecondary; font.pixelSize: 11; font.bold: true }

                    RowLayout {
                        spacing: 6
                        Layout.fillWidth: true

                        HmiComboBox {
                            id: comboExportTarget
                            darkMode: recordFiles.isDarkTheme
                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            model: exportDeviceList
                            textRole: "text"
                            font.pixelSize: 14
                            background: Rectangle {
                                radius: hmiTheme.radiusSm
                                color: hmiTheme.input
                                border.width: 1
                                border.color: comboExportTarget.activeFocus ? hmiTheme.accent : hmiTheme.lineStrong
                            }

                            onModelChanged: {
                                if (!model || model.length === undefined || model.length === 0) {
                                    currentIndex = -1
                                    selectedExportMountPoint = ""
                                    selectedExportDevPath   = ""
                                    return
                                }

                                currentIndex = 0
                                var item = model[0]
                                if (item) {
                                    selectedExportMountPoint = item.mountPoint || ""
                                    selectedExportDevPath   = item.devPath   || ""
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
                            }
                        }

                        HmiButton {
                            id: scanUnmountButton
                            text: deviceFound ? qsTr("UNMOUNT") : qsTr("MOUNT")
                            compact: true
                            fontPixelSize: 10
                            darkMode: recordFiles.isDarkTheme
                            tone: deviceFound ? "warning" : "normal"
                            Layout.preferredWidth: 82
                            Layout.preferredHeight: 42

                            onClicked: {
                                if (!deviceFound) {
                                    popupScanText.text = statusDeviceScan
                                    window.statusScan = "Scanning..."

                                    var p = exportButton.mapToItem(recordFiles, 0, 0)
                                    scanStatusPopup.x = p.x + exportButton.width + 10
                                    scanStatusPopup.y = p.y + (exportButton.height - scanStatusPopup.height) / 2
                                    scanStatusPopup.open()

                                    qmlCommand(JSON.stringify({ menuID: "scanDeivce" }))
                                } else {
                                    qmlCommand(JSON.stringify({ menuID: "unmountDeivce" }))
                                    deviceFound = false
                                    window.statusScan = ""
                                    popupScanText.text = ""
                                    scanStatusPopup.close()
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 7

                HmiButton {
                    id: buttonSearch
                    text: qsTr("SEARCH")
                    tone: "primary"
                    compact: true
                    darkMode: recordFiles.isDarkTheme
                    onClicked: {
                        popupSearching()
                        clearSelections()
                    }
                }

                HmiButton {
                    id: buttonClear
                    text: qsTr("CLEAR")
                    compact: true
                    darkMode: recordFiles.isDarkTheme
                    onClicked: {
                        freezeRecordFilesUpdate = false
                        clearSelections()
                        resetFiltersAndReload()
                    }
                }

                HmiButton {
                    id: btnRefresh
                    text: qsTr("REFRESH")
                    compact: true
                    darkMode: recordFiles.isDarkTheme
                    onClicked: {
                        console.log("[QML] btnRefresh clicked")
                        var msg = { menuID: "refreshpage" }
                        var json = JSON.stringify(msg)
                        console.log("[QML] send =", json)
                        qmlCommand(json)
                    }
                }

                Rectangle { width: 1; height: 28; color: hmiTheme.line; Layout.leftMargin: 3; Layout.rightMargin: 3 }

                HmiButton {
                    id: exportButton
                    text: qsTr("EXPORT SELECTED")
                    compact: true
                    tone: "primary"
                    darkMode: recordFiles.isDarkTheme

                    onClicked: {
                        var items = collectSelectedFiles()
                        if (items.length === 0)
                            return

                        var now = new Date()
                        var defName = Qt.formatDateTime(now, "yyyyMMdd_hhmmss")
                        pathToSave = window.label
                        var mp = window.selectedExportMountPoint || ""
                        exportOverlay.openFor(items, mp, defName)
                    }
                }

                Item { Layout.fillWidth: true }

                HmiButton {
                    id: buttonDeletedFiles
                    text: qsTr("DELETED FILES")
                    compact: true
                    tone: "danger"
                    darkMode: recordFiles.isDarkTheme
                    onClicked: {
                        popupDeleteWave.customMode = false
                        popupDeleteWave.presetDays = 1
                        popupDeleteWave.open()
                    }
                }

                HmiButton {
                    id: buttonFormatDisk
                    text: qsTr("FORMAT DISK")
                    compact: true
                    tone: "warning"
                    darkMode: recordFiles.isDarkTheme
                    onClicked: qmlCommand('{"menuID":"formatdisknow"}')
                }
            }
        }
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
            width: Math.min(1000, recordFiles.width - 80); height: Math.min(500, recordFiles.height - 80); radius: hmiTheme.radiusLg
            color: hmiTheme.panel; border.color: hmiTheme.lineStrong
            anchors.centerIn: parent

            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; preventStealing: true }

            Loader {
                id: picker
                anchors.fill: parent
                source: "CalendarPopup.qml"
                active: calendarOverlay.visible
                onLoaded: {
                    if (!item) return;
                    if (item.isDarkTheme !== undefined) item.isDarkTheme = recordFiles.isDarkTheme;
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
        background: Rectangle { radius: hmiTheme.radiusMd; color: hmiTheme.panel; border.color: hmiTheme.lineStrong }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Label { text: "Custom interval (minutes)"; color: hmiTheme.textSecondary; font.bold: true }
            SpinBox { id: sbCustom; from: 1; to: 24*60; value: 5; Layout.preferredWidth: 140 }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Item { Layout.fillWidth: true }
                HmiButton { text: "CANCEL"; compact: true; darkMode: recordFiles.isDarkTheme; onClicked: customIntervalPopup.close() }
                HmiButton {
                    text: "APPLY"
                    compact: true
                    tone: "primary"
                    darkMode: recordFiles.isDarkTheme
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
        property string exportFrequencyHz: targetFrequencyHz
        property real   exportFrequencyMHz: targetFrequencyMHz
        onExportFrequencyHzChanged: {
            console.log("[ExportFilesRecord] exportFrequencyHz CHANGED ->", exportFrequencyHz)
        }
        onExportFrequencyMHzChanged: {
            console.log("[ExportFilesRecord] exportFrequencyMHz CHANGED ->", exportFrequencyMHz)
        }

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
            width: Math.min(1000, recordFiles.width - 80)
            height: Math.min(500, recordFiles.height - 80)
            radius: hmiTheme.radiusLg
            color: hmiTheme.panel
            border.color: hmiTheme.lineStrong
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

                    // ✅ ใส่ Hz เข้าไปให้ popup ตั้งชื่อได้ทันที
                    if (item.exportFrequencyHz !== undefined) item.exportFrequencyHz = recordFiles.exportFrequencyHz
                    console.log("[RecordFiles] send Hz to ExportFilesRecord:", recordFiles.exportFrequencyHz)

                    if (item.resetState) item.resetState()

                    if (item.requestClose) {
                        item.requestClose.connect(function() {
                            exportOverlay.close()
                        })
                    }
                    exportLoader.item.requestClose.connect(function() {
                        exportOverlay.visible = false
                    })
                }

//                onLoaded: {
//                    if (!item) return

//                    if (item.files        !== undefined) item.files        = exportOverlay.exportFiles
//                    if (item.mountPoint   !== undefined) item.mountPoint   = exportOverlay.exportMountPoint
//                    if (item.defaultName  !== undefined) item.defaultName  = exportOverlay.exportDefaultName
//                    if (item.qmlCommandFn !== undefined) item.qmlCommandFn = window.qmlCommand

//                    // 🔹 รีเซ็ต progress ทุกครั้งที่ popup ถูกสร้างใหม่
//                    if (item.resetState) {
//                        item.resetState()
//                    } else {
//                        if (item.progress   !== undefined) item.progress   = 0
//                        if (item.statusText !== undefined) item.statusText = ""
//                        if (item.exporting  !== undefined) item.exporting  = false
//                    }

//                    if (item.requestClose) {
//                        item.requestClose.connect(function() {
//                            exportOverlay.close()
//                        })
//                    }
//                }

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
//        console.log("Search payload:", JSON.stringify(payload));
        enableSearch = true
    }

    function resetFiltersAndReload() {
//        console.log("resetFiltersAndReload")
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
//            console.log("[RecordFiles] exportProgress", percent, status)
            if (exportLoader.status === Loader.Ready &&
                    exportLoader.item && exportLoader.item.updateProgress) {
                exportLoader.item.updateProgress(percent, status)
            }
        }

        function onExportFinished(ok, outPath, error) {
//            console.log("[RecordFiles] exportFinished", ok, outPath, error)
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
//        console.log("[RecordFiles] Component.onCompleted -> pageReady = true")
        pageReady = true
        Backend.recordFilesPageActive = true

        if (freezeRecordFilesUpdate) {
//            console.log("[RecordFiles] freezeRecordFilesUpdate=true -> restoreSelectionFromTxtAndSyncModel()")
            restoreSelectionFromTxtAndSyncModel()
        }
    }
    onVisibleChanged: {
//        console.log("[RecordFiles] visible =", visible)
        Backend.recordFilesPageActive = visible
    }

}

/*##^##
Designer {
    D{i:0;formeditorZoom:0.5}
}
##^##*/
