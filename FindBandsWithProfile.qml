import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3

Item {
    id: item1
    width: 600
    height: 400
    property string buttonColor: "#009688"
    property string buttonColorUnselect: "#40666666"

    // ===== ค่า property สำหรับ logic ภายนอก =====
    property real startFreqHz: 88000000      // 88 MHz default
    property real stopFreqHz: 95000000       // 108 MHz default
    property string bandMode: "wide"         // "narrow" หรือ "wide"

    Component.onCompleted: {
        console.log("Component.onCompleted:profilesFromDb")
        mainWindows.updateCardProfile.connect(profileCardsfns)
        mainWindows.profilesFromDb.connect(function(list) {
            profilesFromDb(list)
        })
    }

    function profilesFromDb(list){
        profileScan.clear()
        console.log("Total profiles:", (list).length)

        for (var i = 0; i < (list).length; i++) {
            var p = (list)[i]

            console.log(
                "Profile", p.index,
                "Freq =", p.frequency,
                "BW =", p.bw,
                "Mode =", p.mode,
                "LowCut =", p.low_cut,
                "HighCut =", p.high_cut,
                "Time =", p.time
            )

            profileScan.append({
                index: p.index,                               // optional
                freq:       p.frequency || p.freq,      // รองรับทั้ง frequency / freq
                unit:       p.unit || "MHz",
                bw:         p.bw,
                mode:       p.mode,
                low_cut:    p.low_cut,
                high_cut:   p.high_cut,
                time:       p.time
            })
        }
    }

    function profileCardsfns() {
        var size = foundCards.count;
        if (size <= 0) return;

        // สร้าง array เก็บข้อมูลทั้งหมด
        var profiles = [];

        for (var i = 0; i < size; i++) {
            var item = foundCards.get(i);

            profiles.push({
                index:     item.index,
                frequency: item.freq,
                unit:      item.unit,
                bw:        item.bw,
                startHz:   item.startHz,
                endHz:     item.endHz,
                mode:      item.mode,
                // mode:      mainWindows.start_mod(),

                // ====== NEW: low/high cut จาก UI ปัจจุบัน ======
                low_cut:  item.low_cut,
                high_cut: item.high_cut
            });

            // debug log
            console.log(
                "index:", item.index,
                "freq:", item.freq,
                "unit:", item.unit,
                "bw:", item.bw,
                "startHz:", item.startHz,
                "endHz:", item.endHz,
                "mode:", item.mode,
                "lowCutNow:", item.low_cut,
                "highCutNow:", item.high_cut
            );
        }

        var msg = {
            objectName: "profilesCard",
            profiles: profiles
        };

        profileWeb(JSON.stringify(msg, null, 2))
        close();
    }

    Label {
        id: title
        width: 260
        height: 35
        text: qsTr("RF Spectrum Analyzer")
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 27
        color: "white"
        font.bold: true
        font.pixelSize: 24
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        anchors.horizontalCenterOffset: 0
    }

    // ===== กล่องใส่ค่า Start / Stop Frequency =====
    Rectangle {
        id: freqBox
        width: parent.width - 40
        height: 150
        radius: 10
        color: "#202020"
        border.color: "#444"
        anchors.top: title.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 39
        anchors.horizontalCenterOffset: 0

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 15
            spacing: 10

            RowLayout {
                spacing: 10
                Label {
                    text: "Start Frequency (MHz):"
                    color: "white"
                    Layout.alignment: Qt.AlignVCenter
                }
                TextField {
                    id: startField
                    text: (item1.startFreqHz/1e6).toFixed(6)
                    placeholderText: "Enter start freq in MHz"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    validator: DoubleValidator { bottom: 0; top: 6000; decimals: 6 }
                    Layout.fillWidth: true
                    onEditingFinished: {
                        item1.startFreqHz = text * 1e6
                    }
                }
            }

            RowLayout {
                spacing: 10
                Label {
                    text: "Stop Frequency (MHz):"
                    color: "white"
                    Layout.alignment: Qt.AlignVCenter
                }
                TextField {
                    id: stopField
                    text: (item1.stopFreqHz/1e6).toFixed(6)
                    placeholderText: "Enter stop freq in MHz"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    validator: DoubleValidator { bottom: 0; top: 6000; decimals: 6 }
                    Layout.fillWidth: true
                    onEditingFinished: {
                        item1.stopFreqHz = text * 1e6
                    }
                }
            }
        }
    }

    // ===== ปุ่ม Apply / Stop Scan =====
    ColumnLayout {
        id: columnLayout
        y: 294
        height: 50
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 12

        RowLayout {
            id: rowLayout
            spacing: 10

            // ----- ปุ่ม Apply Range (เริ่มสแกน) -----
            Button {
                id: testButton
                text: "Apply Range"
                font.pixelSize: 18

                property color normalColor: "#337ab7"
                property color hoverColor: "#3f8fd6"
                property color pressedColor: "#23527c"

                hoverEnabled: true
                enabled: trigerScan   // พร้อมสแกนเมื่อ trigerScan = true

                onClicked: {
                    trigerScan = false
                    // console.log("Apply Range")

                    var msg = {
                        "objectName": "Scan",
                        "frequency": {
                            "start": item1.startFreqHz,
                            "stop": item1.stopFreqHz
                        },
                        "modes": ["wide", "narrow"]
                    }
                    sCan(JSON.stringify(msg))
                }

                background: Rectangle {
                    id: bgRect
                    color: testButton.down
                           ? testButton.pressedColor
                           : (testButton.hovered ? testButton.hoverColor : testButton.normalColor)
                    radius: 18
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                Layout.preferredHeight: 50
                Layout.fillWidth: true
            }

            // ----- ปุ่ม Stop Scan (หยุดกลางคัน) -----
            Button {
                id: stopButton
                text: "Stop"
                font.pixelSize: 18

                property color normalColor: "#d9534f"
                property color hoverColor: "#c9302c"
                property color pressedColor: "#ac2925"

                hoverEnabled: true
                enabled: !trigerScan     // เปิดใช้ตอนกำลังสแกนอยู่ (ตรงข้าม Apply)

                onClicked: {
                    // console.log("Stop Scan")

                    var msg = {
                        "objectName": "Scan",
                        "action": "stop"      // ให้ backend เช็ค action นี้แล้วหยุดสแกน
                    }
                    sCan(JSON.stringify(msg))

                    trigerScan = true       // พร้อมให้กด Apply ใหม่รอบต่อไป
                }

                background: Rectangle {
                    color: stopButton.down
                           ? stopButton.pressedColor
                           : (stopButton.hovered ? stopButton.hoverColor : stopButton.normalColor)
                    radius: 18
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                Layout.preferredHeight: 50
                Layout.fillWidth: true
            }
        }
    }
}
