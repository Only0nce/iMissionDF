// GroupCard.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: card
    property string title: ""
    property var    items: []
    property bool   selected: false
    signal headerClicked(string title, var items, var nameList, var ipList)

    radius: 12
    color: "#0f141a"
    border.width: selected ? 2.5 : 1
    border.color: selected ? "#4aa3ff" : "#233240"
    implicitWidth: 480
    implicitHeight: header.implicitHeight + contentCol.implicitHeight + 20

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Rectangle {
            id: header
            Layout.fillWidth: true
            implicitHeight: 44
            radius: 8
            color: mouseArea.containsMouse || card.selected ? "#203142" : "#1a2633"
            border.width: 1
            border.color: card.selected ? "#4aa3ff"
                           : mouseArea.containsMouse ? "#3a79b9" : "#2a6fb0"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10
                Text  { text: title; color:"#e9f2f9"; font.pixelSize: 16; font.bold: true; Layout.fillWidth: true }
                Rectangle {
                    radius: 10; color: "#22364a"; height: 24; width: countText.implicitWidth + 12
                    Label { id: countText; anchors.centerIn: parent; text: items ? items.length : 0; color:"#cfe6fb"; font.pixelSize: 12 }
                }
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    card.selected = !card.selected

                    // ✅ สร้างรายการชื่อและ IP
                    let nameList = []
                    let ipList = []
                    if (card.items) {
                        for (let i = 0; i < card.items.length; ++i) {
                            nameList.push(card.items[i].DeviceName)
                            ipList.push(card.items[i].IPAddress)
                        }
                    }

                    // ✅ ส่งออกไปทั้งหมด
                    card.headerClicked(card.title, card.items, nameList, ipList)
                }
            }
        }

        // ===== เนื้อหากลุ่ม =====
        Column {
            id: contentCol
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: items ? items.length : 0
                delegate: Rectangle {
                    radius: 10
                    color: "#141b22"
                    border.width: 1
                    border.color: "#1f2b38"
                    width: parent.width
                    height: devRow.implicitHeight > 0 ? devRow.implicitHeight + 12 : 64

                    RemoteSdrItem {
                        id: devRow
                        anchors.fill: parent
                        anchors.margins: 8
                        deviceName:   items[index].DeviceName
                        deviceIp:     items[index].IPAddress
                        devicePort:   items[index].Port
                        deviceStatus: items[index].status
                        deviceRssi:   0
                        rowIndex:     index
                    }
                }
            }
        }
    }
}
