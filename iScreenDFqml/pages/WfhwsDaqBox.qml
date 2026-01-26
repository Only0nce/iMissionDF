// WfhwsDaqBox.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: daqBox

    /* ===== Public API ===== */
    // ใส่อ็อบเจ็กต์ Krakenmapval จากภายนอก (optional)
    property var kraken: null
    // ใส่ pageSelector (object ที่มี property currentText) เพื่อ auto-activate เมื่อเข้า "DOA ESTIMATION"
    property var pageSelector: null
    // ปรับค่าพฤติกรรม/สไตล์ได้จากภายนอก
    property real activeOpacity: 0.9
    property real idleOpacity: 0.2
    property int  fadeInterval: 5000
    property bool autoFade: true

    // อ่านสถานะ lock จากปุ่ม
    readonly property bool locked: lockFadeButton.checked

    // methods สำหรับให้ไฟล์หลักเรียก
    function wake() {
        daqBox.active = true
        if (daqBox.autoFade && !lockFadeButton.checked) fadeOutTimer.restart()
    }
    function hideIfUnlocked() {
        if (!lockFadeButton.checked) {
            daqBox.active = false
            if (daqBox.autoFade) fadeOutTimer.stop()
        }
    }

    /* ===== Internal State / Style ===== */
    visible: true
    height: contentLayout.implicitHeight + 40
    anchors.topMargin: 20
    anchors.leftMargin: 70
    anchors.rightMargin: 1300
    // ผู้ใช้ควรกำหนด anchors.top/left/right จากภายนอก
    color: "#666666"
    radius: 10
    border.color: "#DCDCDC"
    border.width: 1
    z: 1000
    clip: true

    property bool active: false

    opacity: (lockFadeButton.checked || active) ? activeOpacity : idleOpacity
    Behavior on opacity {
        NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
    }

    /* ===== Lock / Fade button ===== */
    Button {
        id: lockFadeButton
        width: 35
        height: 35
        anchors.top: parent.top
        anchors.topMargin: 9
        anchors.right: parent.right
        anchors.rightMargin: 20
        z: 1000
        opacity: 0.6
        checkable: true
        checked: false

        onClicked: {
            if (checked) {
                daqBox.active = true
                fadeOutTimer.stop()
            } else {
                if (daqBox.autoFade) fadeOutTimer.restart()
            }
        }

        contentItem: Image {
            anchors.centerIn: parent
            source: lockFadeButton.checked ? "qrc:/iScreenDFqml/images/lock.png" : "qrc:/iScreenDFqml/images/unlock.png"
            width: 32
            height: 32
        }

        background: Rectangle {
            radius: width / 2
            color: lockFadeButton.checked ? "#2ecc71" : "#C7C8CC"
        }
    }

    /* ===== Fade timer (ภายใน component) ===== */
    Timer {
        id: fadeOutTimer
        interval: Math.max(0, daqBox.fadeInterval)
        running: false
        repeat: false
        onTriggered: {
            if (!lockFadeButton.checked) {
                daqBox.active = false
            }
        }
    }

    /* ===== Interaction ===== */
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true
        onClicked: {
            daqBox.active = true
            if (daqBox.autoFade && !lockFadeButton.checked) fadeOutTimer.restart()
        }
        onEntered: {
            daqBox.active = true
            if (daqBox.autoFade && !lockFadeButton.checked) fadeOutTimer.restart()
        }
        onExited: {
            if (daqBox.autoFade && !lockFadeButton.checked) fadeOutTimer.restart()
        }
    }

    /* ===== Content ===== */
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

                function k() { return daqBox.kraken }

                Label { text: "Update Rate:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: (k() ? k().updateRate : 0) + " ms"
                    font.pixelSize: 16; color: "#00FFAA"
                }

                Label { text: "Latency:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: (k() ? k().latency : 0) + " ms"
                    font.pixelSize: 16; color: "#00FFAA"
                }

                Label { text: "Frame Index:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: k() ? k().frameIndex : 0
                    font.pixelSize: 16; color: "#00FFAA"
                }

                Label { text: "Frame Type:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    readonly property var ft: k() ? k().frameType : ""
                    text: ft
                    font.pixelSize: 16
                    color: ft === "Data" ? "#00FFAA" :
                           (ft === "Calibration" ? "orange" : "red")
                }

                Label { text: "Frame Sync:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: k() ? k().frameSync : ""
                    font.pixelSize: 16; color: "#00FFAA"
                }

                Label { text: "Power level:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    readonly property var pv: k() ? k().powerLevel : ""
                    text: pv
                    font.pixelSize: 16
                    color: pv === "OK" ? "#00FFAA" : "red"
                }

                Label { text: "Connection Status:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    readonly property var cs: k() ? k().connectionStatus : ""
                    text: cs
                    font.pixelSize: 16
                    color: cs === "Connected" ? "#00FFAA" : "red"
                }

                Label { text: "Sample Delay Sync:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    readonly property var sds: k() ? k().sampleDelaySync : ""
                    text: sds
                    font.pixelSize: 16
                    color: sds === "Ok" ? "#00FFAA" : "red"
                }

                Label { text: "IQ Sync:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    readonly property var iqs: k() ? k().iqSync : ""
                    text: iqs
                    font.pixelSize: 16
                    color: iqs === "Ok" ? "#00FFAA" : "red"
                }

                Label { text: "Noise Source State:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    readonly property var nss: k() ? k().nss : ""
                    text: nss
                    font.pixelSize: 16
                    color: nss === "Ok" ? "#00FFAA" : "red"
                }

                Label { text: "Center Frequency [MHz]:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: k() ? k().centerFrequency : 0
                    font.pixelSize: 16; color: "#00FFAA"
                }

                Label { text: "Sampling Frequency [MHz]:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: k() ? Number(k().samplingFrequency).toFixed(3) : "0.000"
                    font.pixelSize: 16; color: "#00FFAA"
                }

                Label { text: "DSP Decimated BW [MHz]:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: k() ? Number(k().dspDecimatedBW).toFixed(3) : "0.000"
                    font.pixelSize: 16; color: "#00FFAA"
                }

                Label { text: "VFO Range [MHz]:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: k() ? k().vfoRange : ""
                    font.pixelSize: 16; color: "#00FFAA"
                }

                Label { text: "Data Block Length [ms]:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: k() ? k().dataBlockLength : 0
                    font.pixelSize: 16; color: "#00FFAA"
                }

                Label { text: "RF Gains [dB]:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: k() ? k().rfGain : ""
                    font.pixelSize: 16; color: "#00FFAA"
                }

                Label { text: "VFO-0 Power [dB]:"; font.pixelSize: 16; color: "#ffffff" }
                Label {
                    text: k() ? Number(k().vfo).toFixed(3) : "0.000"
                    font.pixelSize: 16; color: "#00FFAA"
                }
            }
        }
    }

    /* ===== Auto-activate เมื่อเปลี่ยนหน้า (ผ่าน proxy pageSelector) ===== */
    Connections {
        target: pageSelector
        enabled: pageSelector !== null
        function onCurrentTextChanged() {
            if (!pageSelector) return
            if (pageSelector.currentText === "DOA ESTIMATION") {
                daqBox.active = true
                if (!lockFadeButton.checked && daqBox.autoFade) fadeOutTimer.restart()
            } else {
                if (!lockFadeButton.checked) {
                    daqBox.active = false
                    if (daqBox.autoFade) fadeOutTimer.restart()
                }
            }
        }
    }
}
