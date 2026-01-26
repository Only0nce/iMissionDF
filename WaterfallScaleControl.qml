import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
Item {
    id: waterfallScaleControl
    width: 300
    height: 75

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    // These will be bound to your waterfall logic
    property real waterfallMinDb: -100
    property real waterfallMaxDb: 0

    Timer {
        id: waterfallScaleControlTimer
        repeat: false
        running: true
        interval: 10000
        onTriggered: {
            waterfallScaleControl.opacity = 0.5
            mouseArea.enabled = true
        }
    }

    Behavior on opacity {
        NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
    }

    Rectangle {
        id: rectangle
        color: "#80000000"
        radius: 5
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
                    font.pointSize: 10
                    color: Material.foreground
                    text: "Min: "+waterfallMinDb.toFixed(1) +" dB"
                }

                Text {
                    horizontalAlignment: Text.AlignRight
                    Layout.rightMargin: 8
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    minimumPixelSize: 10
                    font.pointSize: 10
                    color: Material.foreground
                    text: "Max: "+waterfallMaxDb.toFixed(1) +" dB"
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
                    waterfallScaleControlTimer.restart()
                    waterfallScaleControl.opacity = 1
                }

                second.onValueChanged: {
                    waterfallMaxDb = second.value
                    waterfallScaleControlTimer.restart()
                    waterfallScaleControl.opacity = 1
                }
            }






            Text {
                text: "Scale"
                font.bold: true
                font.pointSize: 10
                horizontalAlignment: Text.AlignHCenter
                Layout.bottomMargin: 4
                Layout.preferredHeight: 12
                Layout.fillHeight: false
                Layout.fillWidth: true
                minimumPixelSize: 10
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                color: Material.foreground
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
