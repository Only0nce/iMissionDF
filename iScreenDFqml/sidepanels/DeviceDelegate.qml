import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    width: 600
    height: 64
    radius: 8
    color: (typeof index !== "undefined" && (index % 2)) ? "#14381d" : "#154021"
    border.color: "#10331a"

    property string devName: ""
    property string devIP: ""
    property int    devPort: 0
    property string status: "Offline"
    property int    rssi: 0
    property bool   selected: false

    signal settingsClicked()
    signal selectedChanged(bool on)

    Row {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // ==== Bars ====
        Item {
            width: 26; height: 26
            anchors.verticalCenter: parent.verticalCenter
            property color ok:  "#39d353"
            property color dim: "#2a6a3a"
            Repeater {
                model: 4
                Rectangle {
                    width: 4
                    height: 8 + index * 4
                    radius: 1
                    anchors.bottom: parent.bottom
                    x: index * 6
                    color: (index < root.rssi) ? ok : dim
                }
            }
        }

        // ==== Name + Status ====
        Column {
            spacing: 2
            width: parent ? parent.width - 180 : 400
            anchors.verticalCenter: parent.verticalCenter
            Text {
                text: root.devName.length ? root.devName : "-"
                color: "#e6f7ec"
                font.pixelSize: 14
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                text: root.status
                color: root.status === "Online" ? "#9ae6b4" : "#ffc9c9"
                font.pixelSize: 12
            }
        }

        Item { Layout.fillWidth: true }

        // ==== Gear ====
        Rectangle {
            id: gearBtn
            width: 28; height: 28
            radius: 14
            color: "transparent"
            border.color: "#2a6a3a"
            anchors.verticalCenter: parent.verticalCenter
            Text { anchors.centerIn: parent; text: "\u2699"; color: "#d9f7e4"; font.pixelSize: 16 }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: gearBtn.border.color = "#4bc46d"
                onExited:  gearBtn.border.color = "#2a6a3a"
                onClicked: root.settingsClicked()
            }
        }

        // ==== Select ring ====
        Rectangle {
            id: ring
            width: 22; height: 22
            radius: 11
            color: "transparent"
            border.width: 2
            border.color: "#94e3ab"
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                id: dot
                anchors.centerIn: parent
                width: 10; height: 10
                radius: 5
                visible: root.selected
                color: root.selected ? "#3fbd6a" : "transparent"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    root.selected = !root.selected
                    root.selectedChanged(root.selected)
                }
            }
        }
    }
}
