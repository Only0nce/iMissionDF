//// PopUPDeletedFileWave.qml  (Qt 5.12 / Controls2)  ✅ FULL FILE
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: rootPopUPDeletedFileWave
    anchors.fill: parent
    z: 999999

    // ===== external inputs =====
    property var listoFDevice: null
    property string deviceTexte: "1"
//    property var qmlCommand: function(jsonString) { console.log("qmlCommand not set:", jsonString) }

    // ===== state =====
    property bool customMode: false
    property int presetDays: 1

    // ===== helpers =====
    function open()  { popUpbuttonDeletedFiles.open() }
    function close() { popUpbuttonDeletedFiles.close() }
    function selectedDevice() { return deviceCombo.currentText }

    function pad2(n) {
        n = Number(n)
        return (n < 10 ? "0" : "") + n
    }

    // ===== build JSON =====
    function buildDeleteWaveJson() {
        var label = ""
        var fromStr = ""
        var toStr = ""

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

        return {
            menuID: "deletedFileWave",
            device: selectedDevice(),
            mode: customMode ? "custom" : "preset",
            days: customMode ? 0 : presetDays,
            label: label,
            from: fromStr,
            to: toStr
        }
    }

    // ===== Popup =====
    Popup {
        id: popUpbuttonDeletedFiles
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        width: 950

        // ✅ auto height by content (avoid clip)
        readonly property int minH: 420
        readonly property int maxH: 920
        height: {
            var h = contentCol.implicitHeight + 24
            if (h < minH) h = minH
            if (h > maxH) h = maxH
            return h
        }

        // center
        x: (rootPopUPDeletedFileWave.width  - width)  / 2
        y: (rootPopUPDeletedFileWave.height - height) / 2

        background: Rectangle {
            radius: 14
            color: "#0B1216"
            border.color: "#2A3A44"
            border.width: 1
        }

        contentItem: Item {
            anchors.fill: parent

            ColumnLayout {
                id: contentCol
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                // ===== title =====
                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter

                    Text {
                        text: "Delete files"
                        color: "white"
                        font.pixelSize: 18
                        font.bold: true
                    }
                }

                // ===== Device selector =====
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text { text: "Device"; color: "#C7D2DA"; font.pixelSize: 13 }

                    ComboBox {
                        id: deviceCombo
                        Layout.preferredWidth: 320
                        Layout.preferredHeight: 60

                        property var sourceModel: rootPopUPDeletedFileWave.listoFDevice
                        property var ids: []
                        model: ids

                        font.pixelSize: 18

                        background: Rectangle {
                            radius: 6
                            color: "#0e1116"
                            border.color: "#2a2f37"
                        }

                        function rebuild() {
                            var out = []

                            if (sourceModel && sourceModel.count > 0) {
                                for (var i = 0; i < sourceModel.count; ++i) {
                                    var it = sourceModel.get(i)
                                    if (!it || it.idDevice === undefined) continue
                                    var s = String(it.idDevice)
                                    if (out.indexOf(s) === -1) out.push(s)
                                }
                                out.sort(function(a,b){ return Number(a) - Number(b) })
                            } else {
                                for (var k = 1; k <= 24; ++k) out.push(String(k))
                            }

                            ids = out

                            var wanted = String(rootPopUPDeletedFileWave.deviceTexte || (ids[0] || "1"))
                            var idx = ids.indexOf(wanted)
                            currentIndex = (idx >= 0) ? idx : 0
                        }

                        Component.onCompleted: rebuild()

                        onActivated: rootPopUPDeletedFileWave.deviceTexte = currentText
                        onCurrentIndexChanged: {
                            if (currentIndex >= 0 && currentIndex < ids.length)
                                rootPopUPDeletedFileWave.deviceTexte = ids[currentIndex]
                        }
                    }
                }

                // divider
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#20303A"
                }

                // ===== Quick delete =====
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text { text: "Quick delete"; color: "#C7D2DA"; font.pixelSize: 13 }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        function setPreset(d) {
                            rootPopUPDeletedFileWave.customMode = false
                            rootPopUPDeletedFileWave.presetDays = d
                        }

                        Button {
                            text: "24 HR"; checkable: true
                            checked: !rootPopUPDeletedFileWave.customMode && rootPopUPDeletedFileWave.presetDays === 1
                            onClicked: parent.setPreset(1)
                        }
                        Button {
                            text: "3 DAYS"; checkable: true
                            checked: !rootPopUPDeletedFileWave.customMode && rootPopUPDeletedFileWave.presetDays === 3
                            onClicked: parent.setPreset(3)
                        }
                        Button {
                            text: "5 DAYS"; checkable: true
                            checked: !rootPopUPDeletedFileWave.customMode && rootPopUPDeletedFileWave.presetDays === 5
                            onClicked: parent.setPreset(5)
                        }
                        Button {
                            text: "7 DAYS"; checkable: true
                            checked: !rootPopUPDeletedFileWave.customMode && rootPopUPDeletedFileWave.presetDays === 7
                            onClicked: parent.setPreset(7)
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "CUSTOM RANGE..."
                            checkable: true
                            checked: rootPopUPDeletedFileWave.customMode
                            onClicked: {
                                rootPopUPDeletedFileWave.customMode = !rootPopUPDeletedFileWave.customMode
                                if (rootPopUPDeletedFileWave.customMode) {
                                    rootPopUPDeletedFileWave.presetDays = -1
                                    if (tumblerDateTime && tumblerDateTime.today) tumblerDateTime.today()
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
                    color: "#20303A"
                }

                // ===== Actions =====
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Cancel"
                        background: Rectangle { radius: 8; color: "#9c9798" }
                        onClicked: popUpbuttonDeletedFiles.close()
                    }

                    Button {
                        text: "Delete"
                        background: Rectangle { radius: 8; color: "#ff004c" }
                        onClicked: {
                            var payload = rootPopUPDeletedFileWave.buildDeleteWaveJson()
                            var json = JSON.stringify(payload)
                            console.log("json:",json)
                            qmlCommand(json)
//                            rootPopUPDeletedFileWave.qmlCommand(json)
                            popUpbuttonDeletedFiles.close()
                        }
                    }
                }
            }
        }

        onOpened: {
            // default = preset 24hr
            rootPopUPDeletedFileWave.customMode = false
            rootPopUPDeletedFileWave.presetDays = 1
        }

    }
}
