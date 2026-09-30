import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../ui"

HmiPanel {
    id: root
    clip: true
    implicitWidth: 300
    Theme { id: theme; darkMode: root.darkMode }

    // Logical display channel: CH1=Home/RX, CH2..CH6=legacy DF ADC CH1..CH5.
    property int displayChannel: 1
    property bool rxSourceAvailable: false
    // Single FFT toggle. It controls only the currently selected logical
    // channel; switching channel moves the FFT source instead of keeping a
    // separate per-channel FFT state alive.
    property bool fftEnabled: false
    signal displayChannelRequested(int channel)
    signal fftEnabledRequested(bool enabled)

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 5

        ColumnLayout {
            spacing: 1
            Layout.preferredWidth: 42
            Text { text: "FFT"; color: theme.text; font.pixelSize: 12; font.bold: true }
            Text { text: "SOURCE"; color: theme.muted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 0.8 }
        }

        Switch {
            checked: root.fftEnabled
            enabled: root.displayChannel === 1 ? root.rxSourceAvailable : doaClient.connected
            onToggled: root.fftEnabledRequested(checked)
        }

        Rectangle { width: 1; height: 24; color: theme.line; opacity: 0.8 }

        Text { text: "CH"; color: theme.textSecondary; font.pixelSize: 11; font.bold: true }

        HmiComboBox {
            id: chCombo
            darkMode: root.darkMode
            Layout.preferredWidth: 82
            enabled: root.rxSourceAvailable || doaClient.connected
            model: ["CH1","CH2","CH3","CH4","CH5","CH6"]
            currentIndex: Math.max(0, Math.min(5, root.displayChannel - 1))
            onActivated: root.displayChannelRequested(currentIndex + 1)
        }

        HmiStatusPill {
            property bool sourceOn: root.fftEnabled
                                    && (root.displayChannel === 1 ? root.rxSourceAvailable : doaClient.connected)
            darkMode: root.darkMode
            text: sourceOn ? "LIVE" : "OFF"
            tone: sourceOn ? "good" : "danger"
            compact: true
            horizontalPadding: 9
        }

        Item { Layout.fillWidth: true }
    }
}
