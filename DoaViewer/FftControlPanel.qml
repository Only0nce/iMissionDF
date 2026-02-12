import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    radius: 10
    color: "#0B1220"
    border.color: "#223049"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        // anchors.margins: 8
        anchors.leftMargin: 8
        anchors.rightMargin: 10
        spacing: 10

        Text { text: "FFT"; color: "#E5E7EB"; font.pixelSize: 13 }

        Switch {
            checked: doaClient.spectrumEnabled
            enabled: doaClient.connected
            onToggled: doaClient.spectrumEnabled = checked
        }

        Rectangle { width: 1; height: 20; color: "#223049"; opacity: 0.7 }

        Text { text: "ADC CH"; color: "#E5E7EB"; font.pixelSize: 12 }

        ComboBox {
            id: chCombo
            Layout.preferredWidth: 110
            enabled: doaClient.connected

            model: ["CH1","CH2","CH3","CH4","CH5"]

            // fftChannel = 0..4
            currentIndex: doaClient.fftChannel

            onActivated: {
                doaClient.fftChannel = currentIndex   // ส่ง 0..4
            }
        }

        Text {
            text: doaClient.spectrumEnabled ? "ON" : "OFF"
            color: doaClient.spectrumEnabled ? "#22c55e" : "#f87171"
            font.pixelSize: 13
        }
    }
}
