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

        Text { text: "DOA Tone"; color: "#E5E7EB"; font.pixelSize: 13; Layout.preferredWidth: 78 }

        Text { text: "Offset"; color: "#93c5fd"; font.pixelSize: 12 }

        TextField {
            id: offsetK
            Layout.preferredWidth: 80
            Layout.preferredHeight: 40
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: DoubleValidator {}
            text: (doaClient.doaOffsetHz / 1000.0).toFixed(1)
            enabled: doaClient.connected
        }
        Text { text: "kHz"; color: "#93c5fd"; font.pixelSize: 12 }

        Text { text: "BW"; color: "#93c5fd"; font.pixelSize: 12 }

        TextField {
            id: bwHz
            Layout.preferredWidth: 90
            Layout.preferredHeight: 40
            enabled: doaClient.connected
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: DoubleValidator { bottom: 0 }

            // แสดงจาก Hz -> kHz
            text: (doaClient.doaBwHz / 1000.0).toFixed(1)

            // ตอน user กดจบการแก้ไข: kHz -> Hz
            onEditingFinished: {
                var khz = parseFloat(text)
                if (!isNaN(khz)) {
                    var hz = Math.round(khz * 1000.0)
                    doaClient.doaBwHz = hz
                }
            }
        }

        Text { text: "kHz"; color: "#93c5fd"; font.pixelSize: 12 }

        Button {
            Layout.preferredHeight: 32
            Layout.preferredWidth: 84
            text: "Apply"
            enabled: doaClient.connected
            onClicked: {
                var offKhz = parseFloat(offsetK.text)
                var bwKhz  = parseFloat(bwHz.text)

                if (isNaN(offKhz)) offKhz = 0.0
                if (isNaN(bwKhz))  bwKhz  = 2.0

                var offHz = Math.round(offKhz * 1000.0)
                var bwHzValue = Math.round(bwKhz * 1000.0)

                doaClient.doaOffsetHz = offHz
                doaClient.doaBwHz = bwHzValue
                doaClient.applyDoaTone()
            }
        }

        Text {
            text: (doaClient.doaOffsetHz >= 0 ? "+" : "") + (doaClient.doaOffsetHz / 1000.0).toFixed(1) + "k"
            color: "#93c5fd"
            font.pixelSize: 12
            Layout.preferredWidth: 60
        }

        Rectangle { width: 1; Layout.fillHeight: true; color: "#223049"; opacity: 0.7 }

        Text { text: "Threshold"; color: "#93c5fd"; font.pixelSize: 12 }

        TextField {
            id: thDb
            Layout.preferredWidth: 80
            Layout.preferredHeight: 40
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: DoubleValidator {}
            text: doaClient.gateThDb.toFixed(1)
            enabled: doaClient.connected
            onEditingFinished: {
                var v = parseFloat(text)
                if (!isNaN(v))
                    doaClient.gateThDb = v
            }
        }
        Text { text: "dB"; color: "#93c5fd"; font.pixelSize: 12 }

        // ให้ข้อความ Band ชิดขวาแบบพอดี
        Item { Layout.fillWidth: true }

        Text {
            text: "Band " + doaClient.bandPeakDb.toFixed(1) + " dB"
            color: doaClient.signalPresent ? "#22c55e" : "#f87171"
            font.pixelSize: 12
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: 140
        }
    }
}
