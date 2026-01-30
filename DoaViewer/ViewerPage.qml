import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.settings 1.1

Rectangle {
    id: root
    width: 1920
    height: 1080
    color: "#000000"

    // =========================
    // Persist settings
    // =========================
    Settings {
        id: uiSettings
        category: "FftWaterfall"

        property bool yAuto: false
        property int  yMinDbUser: -120   // ✅ เก็บเป็น int ให้ตรงกับ SpinBox
        property int  yMaxDbUser: -60
    }

    // =========================
    // Runtime state (ไม่ bind ตรงกับ Settings)
    // =========================
    property bool yAuto: false
    property int  yMinDbUser: -120
    property int  yMaxDbUser: -60

    Component.onCompleted: {
        // ✅ โหลดค่าจาก Settings ครั้งเดียว
        root.yAuto = uiSettings.yAuto
        root.yMinDbUser = uiSettings.yMinDbUser
        root.yMaxDbUser = uiSettings.yMaxDbUser

        // กันค่าพัง
        if (root.yMaxDbUser <= root.yMinDbUser + 1)
            root.yMaxDbUser = root.yMinDbUser + 1
    }

    function saveDbSettings() {
        // กัน user ตั้งผิด
        if (root.yMaxDbUser <= root.yMinDbUser + 1)
            root.yMaxDbUser = root.yMinDbUser + 1

        // ✅ เขียนกลับ Settings
        uiSettings.yAuto = root.yAuto
        uiSettings.yMinDbUser = root.yMinDbUser
        uiSettings.yMaxDbUser = root.yMaxDbUser
    }

    // ถ้าเปลี่ยนจาก code ก็ save
    onYAutoChanged: saveDbSettings()
    onYMinDbUserChanged: saveDbSettings()
    onYMaxDbUserChanged: saveDbSettings()

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        anchors.topMargin: 60

        TopBar {
            id: top1
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            fftPlotTarget: fftPlot
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            // ================= DOA =================
            Rectangle {
                Layout.preferredWidth: 560
                Layout.fillHeight: true
                radius: 12
                color: "#071025"
                border.color: "#20304A"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Text {
                        text: "DOA (MUSIC Polar)"
                        color: "#E5E7EB"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    DoaPolarPlot {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        enabled: doaClient.doaEnabled
                        theta: doaClient.theta
                        spectrum: doaClient.spectrum
                        peakDeg: doaClient.doaDeg
                        conf: doaClient.confidence
                        signalPresent: doaClient.signalPresent
                        sigPower: doaClient.doaSigPower
                        bandPeakDb: doaClient.bandPeakDb
                        gateThDb: doaClient.gateThDb
                    }
                }
            }

            // ================= FFT + WATERFALL =================
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: "#071025"
                border.color: "#20304A"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Text {
                        text: "RF FFT Spectrum (CH" + doaClient.fftChannel + ")"
                        color: "#E5E7EB"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    // ===== Controls: Y range =====
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        CheckBox {
                            text: "Auto Y"
                            checked: root.yAuto
                            onToggled: root.yAuto = checked   // ✅ จะไป saveDbSettings() เอง
                        }

                        Text { text: "Min dB"; color: "#94A3B8"; verticalAlignment: Text.AlignVCenter }

                        SpinBox {
                            from: -200; to: 0
                            value: root.yMinDbUser
                            enabled: !root.yAuto

                            // ✅ ใช้ onValueModified = user เปลี่ยนจริงเท่านั้น
                            onValueModified: root.yMinDbUser = value
                        }

                        Text { text: "Max dB"; color: "#94A3B8"; verticalAlignment: Text.AlignVCenter }

                        SpinBox {
                            from: -200; to: 0
                            value: root.yMaxDbUser
                            enabled: !root.yAuto
                            onValueModified: root.yMaxDbUser = value
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: root.yAuto
                                  ? ("AUTO (" + fftPlot._mmin.toFixed(1) + " .. " + fftPlot._mmax.toFixed(1) + " dB)")
                                  : ("MANUAL (" + root.yMinDbUser + " .. " + root.yMaxDbUser + " dB)")
                            color: "#AAB7D1"
                            font.pixelSize: 12
                        }
                    }

                    FftPlot {
                        id: fftPlot
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: parent.height * 0.55

                        enabled: doaClient.spectrumEnabled
                        freqHz: doaClient.fftFreqHz
                        magDb: doaClient.fftMagDb

                        bandCenterHz: doaClient.fcHz + doaClient.doaOffsetHz
                        bandBwHz: doaClient.doaBwHz

                        yAuto: root.yAuto
                        yMinDb: root.yMinDbUser
                        yMaxDb: root.yMaxDbUser

                        clickOffsetEnabled: true
                        baseFcHz: doaClient.fcHz
                        centerGuardHz: 1000

                        // ✅ ให้ component คุมเอง เพื่อ auto shift range ตามคลิก
                        offsetMinHz: NaN
                        offsetMaxHz: NaN
                        offsetRangeAuto: true
                        offsetRangeSpanHz: doaClient.doaBwHz

                        showOffsetMarker: true

                        onOffsetRequested: {
                            console.log("[FftPlot] newOffsetHz=", newOffsetHz, " actualHz=", clickedHz)
                        }
                        onOffsetRangeChanged: {
                            console.log("[FftPlot] autoRange min=", newMinHz, " max=", newMaxHz)
                        }
                    }
                    // ===== Waterfall =====
                    WaterfallCanvas {
                        id: wf
                        Layout.fillWidth: true
                        Layout.preferredHeight: parent.height * 0.35

                        enabled: doaClient.spectrumEnabled
                        waterfallRowDb: doaClient.fftMagDb

                        autoDb: root.yAuto
                        minDb: root.yMinDbUser
                        maxDb: root.yMaxDbUser

                        padLeft:  fftPlot.padLeft
                        padRight: fftPlot.padRight

                        wfFps: 25
                        rowHeightPx: 1
                        showDebug: true

                        waterfallColors: [
                            0x000004, 0x02021E, 0x04043A, 0x060656, 0x080872,
                            0x0A0A8E, 0x0C0CAA, 0x0E0EC6,
                            0x0030E0, 0x0060FF, 0x0090FF, 0x00C0FF, 0x00FFFF,
                            0x00FFB0, 0x00FF60, 0x40FF00, 0xA0FF00, 0xFFFF00,
                            0xFFB000, 0xFF8000, 0xFF4000, 0xFF0000,
                            0xFF8080, 0xFFFFFF
                        ]
                    }
                }
            }
        }
    }
}
