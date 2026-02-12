import QtQuick 2.15
import QtQuick.Controls 2.15

Drawer {
    id: alertsDrawer
    edge: Qt.RightEdge
    width: 420
    height: parent ? parent.height : 800
    modal: true
    interactive: true

    background: Rectangle { color: "#171717" }

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label { text: "Alerts"; color: "white"; font.pixelSize: 20; font.bold: true }

        ListView {
            anchors.left: parent.left
            anchors.right: parent.right
            height: 300
            model: 10
            delegate: Rectangle {
                height: 42; width: parent.width; color: "transparent"
                Text { anchors.verticalCenter: parent.verticalCenter; text: "Alert #" + (index+1); color: "#cfcfcf" }
            }
        }
    }
}
