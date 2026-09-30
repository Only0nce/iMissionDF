//// PopUPDeletedFileWave.qml  (Qt 5.12 / Controls2)  ✅ FULL FILE
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import "../ui"

Item {
    id: rootPopUPDeletedFileWave
    anchors.fill: parent
    z: 999999
    property bool isDarkTheme: Material.theme === Material.Dark
    Theme { id: hmiTheme; darkMode: rootPopUPDeletedFileWave.isDarkTheme }

    // ===== external inputs =====
//    property var listoFDevice: null
    property string deviceTexte: "1"
//    property var qmlCommand: function(jsonString) { console.log("qmlCommand not set:", jsonString) }

    // ===== state =====
    property bool customMode: false
    property int presetDays: 1

    // ===== helpers =====
    function open()  { popUpbuttonDeletedFiles.open() }
    function close() { popUpbuttonDeletedFiles.close() }
//    function selectedDevice() { return deviceCombo.currentText }
    function selectedDevice() {
        return deviceNumBox ? deviceNumBox.currentText : ""
    }

    function pad2(n) {
        n = Number(n)
        return (n < 10 ? "0" : "") + n
    }

    // ===== build JSON =====
    function buildDeleteWaveJson() {
        var label = ""
        var fromStr = ""
        var toStr = ""

//        console.log("<<<<<---- buildDeleteWaveJson --->>>>>", customMode, presetDays)

        if (!customMode) {
            if (presetDays === 1) label = "24 hr"
            else if (presetDays === 3) label = "3 days"
            else if (presetDays === 5) label = "5 days"
            else if (presetDays === 7) label = "7 days"
            else label = presetDays + " days"
        } else {
            label = "custom"
            if (tumblerDateTime && tumblerDateTime.fromText) fromStr = tumblerDateTime.fromText()
            if (tumblerDateTime && tumblerDateTime.toText)   toStr   = tumblerDateTime.toText()
        }

        var dev = selectedDevice()
//        console.log("buildDeleteWaveJson dev =", dev)

        var obj = {
            menuID: "deletedFileWave",
            device: dev,
            mode: customMode ? "custom" : "preset",
            days: customMode ? 0 : presetDays,
            label: label,
            from: fromStr,
            to: toStr
        }

//        console.log("buildDeleteWaveJson obj ready")
        return obj
    }


    // ===== Popup =====
    Popup {
        id: popUpbuttonDeletedFiles
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        width: Math.min(950, Math.max(240, rootPopUPDeletedFileWave.width - 32))

        // Keep the dialog inside the recorder viewport. Custom date range can
        // exceed short displays, so contentItem below becomes scrollable.
        readonly property int minH: Math.min(420, Math.max(220, rootPopUPDeletedFileWave.height - 32))
        readonly property int maxH: Math.max(220, rootPopUPDeletedFileWave.height - 32)
        height: Math.max(minH, Math.min(maxH, contentCol.implicitHeight + 24))

        // center
        x: Math.max(0, (rootPopUPDeletedFileWave.width  - width)  / 2)
        y: Math.max(0, (rootPopUPDeletedFileWave.height - height) / 2)

        background: Rectangle {
            radius: hmiTheme.radiusLg
            color: hmiTheme.panel
            border.color: hmiTheme.lineStrong
            border.width: 1
        }

        contentItem: Flickable {
            id: deleteFilesFlick
            clip: true
            contentWidth: width
            contentHeight: contentCol.implicitHeight + 24
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: contentCol
                x: 12
                y: 12
                width: Math.max(0, deleteFilesFlick.width - 24)
                spacing: 12

                // ===== title =====
                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter

                    Text {
                        text: "Delete files"
                        color: hmiTheme.text
                        font.pixelSize: 18
                        font.bold: true
                    }
                }

                // ===== Device selector =====
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text { text: "Device"; color: hmiTheme.textSecondary; font.pixelSize: 13 }
                    HmiComboBox {
                        id: deviceNumBox
                        darkMode: rootPopUPDeletedFileWave.isDarkTheme
                        property var sourceModel: listoFDevice
                        property var ids: []
                        model: ids
                        font.pixelSize: 18
                        Layout.preferredHeight: 55
                        implicitWidth: 320
                        Layout.preferredWidth: 320
                        background: Rectangle { radius: hmiTheme.radiusSm; color: hmiTheme.input; border.color: hmiTheme.lineStrong }

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

//                    HmiComboBox {
//                        id: deviceCombo
//                        Layout.preferredWidth: 320
//                        Layout.preferredHeight: 60

//                        property var sourceModel: rootPopUPDeletedFileWave.listoFDevice
//                        property var ids: []
//                        model: ids

//                        font.pixelSize: 18

//                        background: Rectangle {
//                            radius: 6
//                            color: "#0e1116"
//                            border.color: "#2a2f37"
//                        }

//                        function rebuild() {
//                            var out = []

//                            if (sourceModel && sourceModel.count > 0) {
//                                for (var i = 0; i < sourceModel.count; ++i) {
//                                    var it = sourceModel.get(i)
//                                    if (!it || it.idDevice === undefined) continue
//                                    var s = String(it.idDevice)
//                                    if (out.indexOf(s) === -1) out.push(s)
//                                }
//                                out.sort(function(a,b){ return Number(a) - Number(b) })
//                            } else {
//                                for (var k = 1; k <= 24; ++k) out.push(String(k))
//                            }

//                            ids = out

//                            var wanted = String(rootPopUPDeletedFileWave.deviceTexte || (ids[0] || "1"))
//                            var idx = ids.indexOf(wanted)
//                            currentIndex = (idx >= 0) ? idx : 0
//                        }

//                        Component.onCompleted: rebuild()

//                        onActivated: rootPopUPDeletedFileWave.deviceTexte = currentText
//                        onCurrentIndexChanged: {
//                            if (currentIndex >= 0 && currentIndex < ids.length)
//                                rootPopUPDeletedFileWave.deviceTexte = ids[currentIndex]
//                        }
//                    }

                }

                // divider
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: hmiTheme.line
                }

                // ===== Quick delete =====
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text { text: "Quick delete"; color: hmiTheme.textSecondary; font.pixelSize: 13 }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        function setPreset(d) {
                            rootPopUPDeletedFileWave.customMode = false
                            rootPopUPDeletedFileWave.presetDays = d
                        }

                        HmiButton {
                            text: "24 HR"; checkable: true; compact: true
                            darkMode: rootPopUPDeletedFileWave.isDarkTheme
                            checked: !rootPopUPDeletedFileWave.customMode && rootPopUPDeletedFileWave.presetDays === 1
                            tone: checked ? "primary" : "normal"
                            onClicked: parent.setPreset(1)
                        }
                        HmiButton {
                            text: "3 DAYS"; checkable: true; compact: true
                            darkMode: rootPopUPDeletedFileWave.isDarkTheme
                            checked: !rootPopUPDeletedFileWave.customMode && rootPopUPDeletedFileWave.presetDays === 3
                            tone: checked ? "primary" : "normal"
                            onClicked: parent.setPreset(3)
                        }
                        HmiButton {
                            text: "5 DAYS"; checkable: true; compact: true
                            darkMode: rootPopUPDeletedFileWave.isDarkTheme
                            checked: !rootPopUPDeletedFileWave.customMode && rootPopUPDeletedFileWave.presetDays === 5
                            tone: checked ? "primary" : "normal"
                            onClicked: parent.setPreset(5)
                        }
                        HmiButton {
                            text: "7 DAYS"; checkable: true; compact: true
                            darkMode: rootPopUPDeletedFileWave.isDarkTheme
                            checked: !rootPopUPDeletedFileWave.customMode && rootPopUPDeletedFileWave.presetDays === 7
                            tone: checked ? "primary" : "normal"
                            onClicked: parent.setPreset(7)
                        }

                        Item { Layout.fillWidth: true }

                        HmiButton {
                            text: "CUSTOM RANGE..."
                            checkable: true
                            compact: true
                            darkMode: rootPopUPDeletedFileWave.isDarkTheme
                            checked: rootPopUPDeletedFileWave.customMode
                            tone: checked ? "primary" : "normal"
                            onClicked: {
                                rootPopUPDeletedFileWave.customMode = !rootPopUPDeletedFileWave.customMode
                                if (rootPopUPDeletedFileWave.customMode) {
                                    rootPopUPDeletedFileWave.presetDays = -1
                                    if (tumblerDateTime && tumblerDateTime.setTodayAll) tumblerDateTime.setTodayAll()
                                } else {
                                    rootPopUPDeletedFileWave.presetDays = 1
                                }
                            }
                        }
                    }
                }

                // ===== Custom Range (TumblerDateTime) =====
                TumblerDateTime {
                    id: tumblerDateTime
                    Layout.fillWidth: true
                    Layout.preferredHeight: 420
                    visible: rootPopUPDeletedFileWave.customMode
                }

                // divider
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: hmiTheme.line
                }

                // ===== Actions =====
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    HmiButton {
                        text: "CANCEL"
                        compact: true
                        darkMode: rootPopUPDeletedFileWave.isDarkTheme
                        onClicked:{
//                            console.log("<<<<<<<<<<<<Cancel>>>>>>>>>>")
                            popUpbuttonDeletedFiles.close()
                        }
                    }

                    HmiButton {
                        text: "DELETE"
                        compact: true
                        tone: "danger"
                        darkMode: rootPopUPDeletedFileWave.isDarkTheme
                        onClicked: {
//                            console.log("<<<<<<<<<<<<Delete>>>>>>>>>>")
                            try {
                                var payload = rootPopUPDeletedFileWave.buildDeleteWaveJson()
//                                console.log("payload:", payload)

                                var json = JSON.stringify(payload)
//                                console.log("json:", json)

                                // ถ้า qmlCommand เป็น signal ของ window:
                                window.qmlCommand(json)

                                popUpbuttonDeletedFiles.close()
                            } catch (e) {
//                                console.log("Delete ERROR:", e)
                            }
                        }
                    }

                }

            }
        }

        onOpened: {
            // default = preset 24hr
            rootPopUPDeletedFileWave.customMode = false
            rootPopUPDeletedFileWave.presetDays = 1
            deviceNumBox.rebuild()
        }

    }
}
