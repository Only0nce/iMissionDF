import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
Item {
    id: waterfallScaleControl
    width: 300
    height: 75

    Material.theme: Material.Dark
    Material.accent: "#6EF2E8"

    readonly property color primaryText: "#F4FBFF"
    readonly property color secondaryText: "#D3E1E7"

    // These will be bound to your waterfall logic
    property real waterfallMinDb: -130
    property real waterfallMaxDb: -80
    signal manualScaleEdited()

    Timer {
        id: waterfallScaleControlTimer
        repeat: false
        running: true
        interval: 10000
        onTriggered: {
            // CUDA1.6: parent HUD card provides translucency; keep text and
            // controls fully opaque/readable instead of fading the whole item.
            waterfallScaleControl.opacity = 1.0
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
        border.color: "transparent"
        border.width: 0
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 3



            RowLayout {
                Layout.topMargin: 4
                Layout.fillWidth: true
                Layout.preferredHeight: 12
                spacing: 50
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                Text {
                    Layout.leftMargin: 8
                    Layout.fillWidth: true
                    minimumPixelSize: 10
                    font.pointSize: 11
                    font.bold: true
                    color: primaryText
                    text: "Min: "+waterfallMinDb.toFixed(1) +" dBFS"
                }

                Text {
                    horizontalAlignment: Text.AlignRight
                    Layout.rightMargin: 8
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    minimumPixelSize: 10
                    font.pointSize: 11
                    font.bold: true
                    color: primaryText
                    text: "Max: "+waterfallMaxDb.toFixed(1) +" dBFS"
                }
            }

            RangeSlider {
                id: bwRangeSlider
                Layout.fillHeight: true
                Layout.preferredHeight: 20
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                Layout.preferredWidth: 280
                from: -150
                to: 10
                first.value: waterfallMinDb
                second.value: waterfallMaxDb
                stepSize: 1

                first.onValueChanged: {
                    waterfallMinDb = first.value
                    if (first.pressed) manualScaleEdited()
                    waterfallScaleControlTimer.restart()
                    waterfallScaleControl.opacity = 1
                }

                second.onValueChanged: {
                    waterfallMaxDb = second.value
                    if (second.pressed) manualScaleEdited()
                    waterfallScaleControlTimer.restart()
                    waterfallScaleControl.opacity = 1
                }
            }






            Text {
                text: "Intensity Scale · dBFS"
                font.bold: true
                font.pointSize: 11
                horizontalAlignment: Text.AlignHCenter
                Layout.bottomMargin: 4
                Layout.preferredHeight: 12
                Layout.fillHeight: false
                Layout.fillWidth: true
                minimumPixelSize: 10
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                color: primaryText
            }
        }

        MouseArea {
            id: mouseArea
            enabled: false
            anchors.fill: parent
            onClicked: {
                waterfallScaleControlTimer.restart()
                waterfallScaleControl.opacity = 1
                mouseArea.enabled = false
            }

        }
    }
}
