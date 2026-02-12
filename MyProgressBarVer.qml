import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

Rectangle {
    id: root
    width: 24
    height: 200
    color: "transparent"

    property real minValue: 0
    property real progressValue: (value-minValue)/(maxValue-minValue)
    property real maxValue: 100
    property real progressOffset: 0
    property real value: 0.5
    property color baseColor: Material.color(Material.Blue, Material.Shade500)
    property color backgroundColor: Material.color(Material.LightBlue, Material.Shade100)
    property real radius: 4

    Rectangle {
        id: track
        color: backgroundColor
        anchors.fill: parent
        radius: root.radius
        opacity: 0.3
    }

    Rectangle {
        id: progress
        width: parent.width
        height: parent.height * progressValue
        y: parent.height - height
        color: baseColor
        radius: root.radius
        Behavior on height {
            NumberAnimation {
                duration: 100
                easing.type: Easing.InOutQuad
            }
        }
    }

    // Optional: Text overlay
    Text {
        visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        text: Math.round(progressValue * 100) + "%"
        color: "white"
        font.pixelSize: 14
    }

    // Material Design style override (if in mixed UI)
    Material.theme: Material.Light
}
