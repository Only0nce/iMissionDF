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
    Material.accent: "#6EF2E8"

    readonly property color primaryText: "#F4FBFF"
    readonly property color secondaryText: "#D3E1E7"
    readonly property color fieldBackground: "#B30A141B"
    readonly property color fieldBorder: "#805E7A86"

    // property real low_cut: -30000  // default
    // property real high_cut: 30000

    Timer {
        id: bandwidthScaleControlTimer
        repeat: false
        running: true
        interval: 10000
        onTriggered: {
            // CUDA1.6: parent HUD card provides translucency; keep text and
            // controls fully opaque/readable instead of fading the whole item.
            bandwidthScaleControl.opacity = 1.0
            mouseArea.enabled = false
        }
    }

    Behavior on opacity {
        NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
    }

    Rectangle {
        id: rectangle
        color: "transparent"
        radius: 0
        border.width: 0
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
                    color: secondaryText
                    font.pointSize: 11
                }
                TextField {
                    id: lowField
                    text: low_cut.toString()
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    Layout.preferredWidth: 95
                    font.pointSize: 11
                    color: primaryText
                    selectionColor: "#6EF2E8"
                    selectedTextColor: "#071018"
                    font.bold: true
                    background: Rectangle {
                        radius: 4
                        color: fieldBackground
                        border.width: 1
                        border.color: fieldBorder
                    }
                    validator: IntValidator { bottom: -250000; top: 0 }  // ช่วงค่าที่รองรับ
                    onEditingFinished: {
                        low_cut = parseInt(text)
                        sendBandwidthUpdate()
                        focus = false
                    }
                }


                Label {
                    text: "High :"
                    color: secondaryText
                    font.pointSize: 10
                }
                TextField {
                    id: highField
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    font.pointSize: 11
                    validator: IntValidator { bottom: 0; top: 250000 }  // ช่วงค่าที่รองรับ
                    text: high_cut.toString()
                    Layout.preferredWidth: 95
                    color: primaryText
                    selectionColor: "#6EF2E8"
                    selectedTextColor: "#071018"
                    font.bold: true
                    background: Rectangle {
                        radius: 4
                        color: fieldBackground
                        border.width: 1
                        border.color: fieldBorder
                    }
                    onEditingFinished: {
                        high_cut = parseInt(text)
                        sendBandwidthUpdate()
                    }
                }
            }

            Label {
                text: "Analog Demod Bandwidth (Hz)"
                font.bold: true
                font.pointSize: 11
                color: primaryText
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
