import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    width: 1920
    height: 1080
    color: "#000000"    // ✅ พื้นหลังสีดำสนิท

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        // anchors.margins: 10
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

        // Content: DOA (เล็ก) + FFT (ใหญ่)
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

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

                    FftPlot {
                        id: fftPlot
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        enabled: doaClient.spectrumEnabled
                        freqHz: doaClient.fftFreqHz
                        magDb: doaClient.fftMagDb
                        bandCenterHz: doaClient.fcHz + doaClient.doaOffsetHz
                        bandBwHz: doaClient.doaBwHz
                    }
                }
            }
        }
    }
}
