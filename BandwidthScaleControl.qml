import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
// import QtGraphicalEffects 1.15

Item {
    id: bandwidthScaleControl
    width: 300
    height: 110

    property bool darkMode: true

    Material.theme: darkMode ? Material.Dark : Material.Light
    Material.accent: darkMode ? "#6EF2E8" : "#008B75"

    readonly property color primaryText: darkMode ? "#F4FBFF" : "#102824"
    readonly property color secondaryText: darkMode ? "#D3E1E7" : "#284640"
    readonly property color fieldBackground: darkMode ? "#B30A141B" : "#FFFFFF"
    readonly property color fieldBorder: darkMode ? "#805E7A86" : "#739E96"
    readonly property color selectionFill: darkMode ? "#6EF2E8" : "#16A98F"
    readonly property color selectedText: darkMode ? "#071018" : "#FFFFFF"

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
                    font.pixelSize: 13
                }
                TextField {
                    id: lowField
                    text: low_cut.toString()
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    Layout.preferredWidth: 95
                    font.pixelSize: 13
                    color: primaryText
                    selectionColor: selectionFill
                    selectedTextColor: selectedText
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
                    font.pixelSize: 12
                }
                TextField {
                    id: highField
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    font.pixelSize: 13
                    validator: IntValidator { bottom: 0; top: 250000 }  // ช่วงค่าที่รองรับ
                    text: high_cut.toString()
                    Layout.preferredWidth: 95
                    color: primaryText
                    selectionColor: selectionFill
                    selectedTextColor: selectedText
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
                font.pixelSize: 13
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
