import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../ui"

HmiPanel {
    id: root
    clip: true
    Theme { id: theme; darkMode: root.darkMode }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        ColumnLayout {
            spacing: 1
            Layout.preferredWidth: 76
            Text { text: "DOA TONE"; color: theme.text; font.pixelSize: 11; font.bold: true }
            Text { text: "GATE / BAND"; color: theme.muted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 0.5 }
        }

        Text { text: "Offset"; color: theme.textSecondary; font.pixelSize: 11 }

        HmiTextField {
            id: offsetK
            darkMode: root.darkMode
            compact: true
            Layout.preferredWidth: 76
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: DoubleValidator {}
            text: (doaClient.doaOffsetHz / 1000.0).toFixed(1)
            enabled: doaClient.connected
        }
        Text { text: "kHz"; color: theme.muted; font.pixelSize: 10 }

        Text { text: "BW"; color: theme.textSecondary; font.pixelSize: 11 }

        HmiTextField {
            id: bwHz
            darkMode: root.darkMode
            compact: true
            Layout.preferredWidth: 78
            enabled: doaClient.connected
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: DoubleValidator { bottom: 0 }
            text: (doaClient.doaBwHz / 1000.0).toFixed(1)
            onEditingFinished: {
                var khz = parseFloat(text)
                if (!isNaN(khz))
                    doaClient.doaBwHz = Math.round(khz * 1000.0)
            }
        }
        Text { text: "kHz"; color: theme.muted; font.pixelSize: 10 }

        HmiButton {
            darkMode: root.darkMode
            compact: true
            tone: "primary"
            text: "APPLY"
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

        HmiMetricChip {
            darkMode: root.darkMode
            label: "OFFSET"
            value: (doaClient.doaOffsetHz >= 0 ? "+" : "") + (doaClient.doaOffsetHz / 1000.0).toFixed(1) + "k"
            tone: "info"
        }

        Rectangle { width: 1; Layout.fillHeight: true; color: theme.line; opacity: 0.8 }

        Text { text: "Threshold"; color: theme.textSecondary; font.pixelSize: 11 }

        HmiTextField {
            id: thDb
            darkMode: root.darkMode
            compact: true
            Layout.preferredWidth: 76
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: DoubleValidator {}
            text: doaClient.gateThDb.toFixed(1)
            enabled: doaClient.connected
            onEditingFinished: {
                var v = parseFloat(text)
                if (!isNaN(v)) doaClient.gateThDb = v
            }
        }
        Text { text: "dB"; color: theme.muted; font.pixelSize: 10 }

        Item { Layout.fillWidth: true }

        HmiStatusPill {
            darkMode: root.darkMode
            text: "BAND " + doaClient.bandPeakDb.toFixed(1) + " dB"
            tone: doaClient.signalPresent ? "good" : "danger"
            compact: true
        }
    }
}
