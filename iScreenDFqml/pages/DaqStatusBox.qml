// components/DaqStatusBox.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3

Rectangle {
    id: daqBox
    // ---------- API ----------
    property var  krakenmapval: null        // ส่ง object เข้ามาจากไฟล์หลัก
    property var  pageSelector: null        // ส่ง pageSelector เข้ามาเพื่อ trigger fade
    // ควบคุมตำแหน่ง/ระยะขอบจากภายนอก (แทนการอ้าง navBar ในไฟล์นี้)
    property real leftMargin: 70
    property real rightMargin: 1300
    property real topMargin: 20

    // สถานะภายใน (ยังคงเหมือนเดิม)
    property bool daqLocked: false
    property bool active: false
    width: 550

    // ---------- Layout ----------
    visible: true
    radius: 10
    color: "#666666"
    border.color: "#DCDCDC"
    border.width: 1
    z: 1
    clip: true

    // ให้ไฟล์แม่เป็นคนวาง anchors ซ้าย/ขวา/บนเอง
    // ตัวคอมโพเนนต์จะใช้ margins ตาม property ด้านบนเป็นค่าช่วย
    anchors.leftMargin: leftMargin
    anchors.rightMargin: rightMargin
    anchors.topMargin: topMargin

    // ความสูงอิงจากเนื้อหา + 40 ตามเดิม
    height: contentLayout.implicitHeight + 40

    // เฟดความทึบตาม lock/active
    opacity: lockFadeButton.checked || active ? 0.9 : 0.2
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }

    // ===== ปุ่มล็อกการเฟด =====
    Button {
        id: lockFadeButton
        width: 35
        height: 35
        anchors.top: parent.top
        anchors.topMargin: 9
        anchors.right: parent.right
        anchors.rightMargin: 20
        z: 100
        opacity: 0.6
        checkable: true
        checked: false

        onClicked: {
            daqBox.daqLocked = !daqBox.daqLocked
            if (daqBox.daqLocked) {
                daqBox.active = true
                fadeOutTimer.stop()
            } else {
                fadeOutTimer.restart()
            }
        }

        contentItem: Image {
            anchors.centerIn: parent
            source: lockFadeButton.checked ? "qrc:/images/lock.png" : "qrc:/images/unlock.png"
            width: 32
            height: 32
        }
        background: Rectangle { radius: width/2; color: lockFadeButton.checked ? "#2ecc71" : "#C7C8CC" }
    }

    // ตั้งเวลาเฟด
    Timer {
        id: fadeOutTimer
        interval: 5000           // 5 วินาที
        running: false
        repeat: false
        onTriggered: {
            if (!lockFadeButton.checked) daqBox.active = false
        }
    }

    // จับ mouse เพื่อรีสตาร์ทเฟดและทำให้ active
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true
        onClicked:  { daqBox.active = true; fadeOutTimer.restart() }
        onEntered:  { daqBox.active = true; fadeOutTimer.restart() }
        onExited:   { fadeOutTimer.restart() }
    }

    // เนื้อหา
    Item {
        anchors.fill: parent
        anchors.margins: 20

        ColumnLayout {
            id: contentLayout
            anchors.fill: parent
            spacing: 12

            Label {
                text: "DAQ Subsystem Status"
                font.pixelSize: 20
                font.bold: true
                color: "#ffffff"
            }

            GridLayout {
                columns: 2
                Layout.fillWidth: true
                rowSpacing: 10
                columnSpacing: 100

                Label { text: "Update Rate:";                 font.pixelSize: 16; color: "#ffffff" }
                Label { text: (krakenmapval? krakenmapval.updateRate + " ms" : "-");
                        font.pixelSize: 16; color: "#00FFAA" }

                Label { text: "Latency:";                     font.pixelSize: 16; color: "#ffffff" }
                Label { text: (krakenmapval? krakenmapval.latency + " ms" : "-");
                        font.pixelSize: 16; color: "#00FFAA" }

                Label { text: "Frame Index:";                 font.pixelSize: 16; color: "#ffffff" }
                Label { text: (krakenmapval? krakenmapval.frameIndex : "-");
                        font.pixelSize: 16; color: "#00FFAA" }

                Label { text: "Frame Type:";                  font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: krakenmapval? krakenmapval.frameType : "-"
                    font.pixelSize: 16
                    color: {
                        if (!krakenmapval) return "red"
                        if (krakenmapval.frameType === "Data") return "#00FFAA"
                        else if (krakenmapval.frameType === "Calibration") return "orange"
                        else return "red"
                    }
                }

                Label { text: "Frame Sync:";                  font.pixelSize: 16; color: "#ffffff" }
                Label { text: (krakenmapval? krakenmapval.frameSync : "-");
                        font.pixelSize: 16; color: "#00FFAA" }

                Label { text: "Power level:";                 font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: krakenmapval? krakenmapval.powerLevel : "-"
                    font.pixelSize: 16
                    color: (krakenmapval && krakenmapval.powerLevel === "OK") ? "#00FFAA" : "red"
                }

                Label { text: "Connection Status:";           font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: krakenmapval? krakenmapval.connectionStatus : "-"
                    font.pixelSize: 16
                    color: (krakenmapval && krakenmapval.connectionStatus === "Connected") ? "#00FFAA" : "red"
                }

                Label { text: "Sample Delay Sync:";           font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: krakenmapval? krakenmapval.sampleDelaySync : "-"
                    font.pixelSize: 16
                    color: (krakenmapval && krakenmapval.sampleDelaySync === "Ok") ? "#00FFAA" : "red"
                }

                Label { text: "IQ Sync:";                     font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: krakenmapval? krakenmapval.iqSync : "-"
                    font.pixelSize: 16
                    color: (krakenmapval && krakenmapval.sampleDelaySync === "Ok") ? "#00FFAA" : "red"
                }

                Label { text: "Noise Source State:";          font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: krakenmapval? krakenmapval.nss : "-"
                    font.pixelSize: 16
                    color: (krakenmapval && krakenmapval.sampleDelaySync === "Ok") ? "#00FFAA" : "red"
                }

                Label { text: "Center Frequency [MHz]:";      font.pixelSize: 16; color: "#ffffff" }
                Label { text: krakenmapval? krakenmapval.centerFrequency : "-";
                        font.pixelSize: 16; color: "#00FFAA" }

                Label { text: "Sampling Frequency [MHz]:";    font.pixelSize: 16; color: "#ffffff" }
                Label { text: krakenmapval? krakenmapval.samplingFrequency.toFixed(3) : "-";
                        font.pixelSize: 16; color: "#00FFAA" }

                Label { text: "DSP Decimated BW [MHz]:";      font.pixelSize: 16; color: "#ffffff" }
                Label { text: krakenmapval? krakenmapval.dspDecimatedBW.toFixed(3) : "-";
                        font.pixelSize: 16; color: "#00FFAA" }

                Label { text: "VFO Range [MHz]:";             font.pixelSize: 16; color: "#ffffff" }
                Label { text: krakenmapval? krakenmapval.vfoRange : "-";
                        font.pixelSize: 16; color: "#00FFAA" }

                Label { text: "Data Block Length [ms]:";      font.pixelSize: 16; color: "#ffffff" }
                Label { text: krakenmapval? krakenmapval.dataBlockLength : "-";
                        font.pixelSize: 16; color: "#00FFAA" }

                Label { text: "RF Gains [dB]:";               font.pixelSize: 16; color: "#ffffff" }
                Label { text: krakenmapval? krakenmapval.rfGain : "-";
                        font.pixelSize: 16; color: "#00FFAA" }

                Label { text: "VFO-0 Power [dB]:";            font.pixelSize: 16; color: "#ffffff" }
                Label { text: krakenmapval? krakenmapval.vfo.toFixed(3) : "-";
                        font.pixelSize: 16; color: "#00FFAA" }
            }
        }
    }

    // ตอบสนองการเปลี่ยนหน้าเหมือนเดิม
    Connections {
        target: pageSelector
        enabled: !!pageSelector
        function onCurrentTextChanged() {
            if (!pageSelector) return
            if (pageSelector.currentText === "DOA ESTIMATION") {
                daqBox.active = true
                if (!lockFadeButton.checked) fadeOutTimer.restart()
            } else {
                if (!lockFadeButton.checked) {
                    daqBox.active = false
                    fadeOutTimer.restart()
                }
            }
        }
    }
}
