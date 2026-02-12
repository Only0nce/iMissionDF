import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
// import QtGraphicalEffects 1.15

Item {
    id: bandwidthScaleControl
    width: 300
    height: 110

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    // property real low_cut: -30000  // default
    // property real high_cut: 30000

    Timer {
        id: bandwidthScaleControlTimer
        repeat: false
        running: true
        interval: 10000
        onTriggered: {
            bandwidthScaleControl.opacity = 0.5
            mouseArea.enabled = true
        }
    }

    Behavior on opacity {
        NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
    }

    Rectangle {
        id: rectangle
        color: "#A0000000"
        radius: 5
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 5
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

            RowLayout {
                Layout.topMargin: 6
                Layout.fillWidth: true
                spacing: 12
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                Label {
                    text: "Low :"
                    color: Material.foreground
                    font.pointSize: 10
                }
                TextField {
                    id: lowField
                    text: low_cut.toString()
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    Layout.preferredWidth: 95
                    validator: IntValidator { bottom: -250000; top: 0 }  // ช่วงค่าที่รองรับ
                    onEditingFinished: {
                        low_cut = parseInt(text)
                        sendBandwidthUpdate()
                        focus = false
                    }
                }


                Label {
                    text: "High :"
                    color: Material.foreground
                    font.pointSize: 10
                }
                TextField {
                    id: highField
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    font.pointSize: 10
                    validator: IntValidator { bottom: 0; top: 250000 }  // ช่วงค่าที่รองรับ
                    text: high_cut.toString()
                    Layout.preferredWidth: 95
                    onEditingFinished: {
                        high_cut = parseInt(text)
                        sendBandwidthUpdate()
                    }
                }
            }

            Label {
                text: "Analog Demod Bandwidth (Hz)"
                font.bold: true
                font.pointSize: 10
                color: Material.foreground
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }
        }

        MouseArea {
            id: mouseArea
            enabled: false
            anchors.fill: parent
            onClicked: {
                bandwidthScaleControl.opacity = 1
                bandwidthScaleControlTimer.restart()
                mouseArea.enabled = false
            }
        }
    }

    function sendBandwidthUpdate() {
        const msg = {
            type: "dspcontrol",
            params: {
                low_cut: low_cut,
                high_cut: high_cut
            }
        }
        mainWindows.sendmessage(JSON.stringify(msg))
        bandwidthScaleControlTimer.restart()
        highField.focus = false
        lowField.focus = false
    }
}
