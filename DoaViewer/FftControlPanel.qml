import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    radius: 10
    // Logical display channel: CH1=Home/RX, CH2..CH6=legacy DF ADC CH1..CH5.
    property int displayChannel: 1
    property bool rxSourceAvailable: false
    // CH1 uses the shared AstraRX/Home FFT source. Keep an explicit UI gate so
    // it can be switched OFF/ON just like the RFSoC DF FFT channels.
    property bool rxFftEnabled: true
    signal displayChannelRequested(int channel)
    signal rxFftEnabledRequested(bool enabled)
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
            checked: root.displayChannel === 1
                     ? (root.rxSourceAvailable && root.rxFftEnabled)
                     : doaClient.spectrumEnabled
            enabled: root.displayChannel === 1 ? root.rxSourceAvailable : doaClient.connected
            onToggled: {
                if (root.displayChannel === 1)
                    root.rxFftEnabledRequested(checked)
                else
                    doaClient.spectrumEnabled = checked
            }
        }

        Rectangle { width: 1; height: 20; color: "#223049"; opacity: 0.7 }

        Text { text: "ADC CH"; color: "#E5E7EB"; font.pixelSize: 12 }

        ComboBox {
            id: chCombo
            Layout.preferredWidth: 110
            enabled: root.rxSourceAvailable || doaClient.connected

            model: ["CH1","CH2","CH3","CH4","CH5","CH6"]

            // UI CH1 is RX/Home. UI CH2..CH6 map to physical DF 0..4.
            currentIndex: Math.max(0, Math.min(5, root.displayChannel - 1))

            onActivated: {
                root.displayChannelRequested(currentIndex + 1)
            }
        }

        Text {
            property bool sourceOn: root.displayChannel === 1
                                    ? (root.rxSourceAvailable && root.rxFftEnabled)
                                    : doaClient.spectrumEnabled
            text: sourceOn ? "ON" : "OFF"
            color: sourceOn ? "#22c55e" : "#f87171"
            font.pixelSize: 13
        }
    }
}
