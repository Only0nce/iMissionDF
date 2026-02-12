// ExpandableGroup.qml (fixed for Qt Design Studio)
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: card
    property string title: ""
    property var    items: []      // [{DeviceName, IPAddress, Port, status, GroupsName, ...}]
    property bool   expanded: true
    property int    hMargin: 8     // ใช้ภายในถ้าต้องการ spacing เพิ่ม

    radius: 12
    color: "#0f141a"
    border.width: 1
    border.color: "#233240"

    // อย่า bind กับ parent ที่นี่ เพื่อหลบ M205
    implicitWidth: 480
    // ให้ container ภายนอกเป็นผู้กำหนด width/x/anchors ของ card

    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Header กลุ่ม
        Rectangle {
            id: header
            Layout.fillWidth: true
            height: 44
            radius: 8
            color: expanded ? "#1a2633" : "#151e27"
            border.width: 1
            border.color: expanded ? "#2a6fb0" : "#223140"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Label { text: expanded ? "▾" : "▸"; color: "#9fb6ca"; font.pixelSize: 16 }
                Text  { text: title; color:"#e9f2f9"; font.pixelSize: 16; font.bold: true; Layout.fillWidth: true }
                Rectangle {
                    radius: 10; color: "#22364a"; height: 24; width: countText.implicitWidth + 12
                    Label { id: countText; anchors.centerIn: parent; text: items ? items.length : 0; color:"#cfe6fb"; font.pixelSize: 12 }
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: expanded = !expanded
            }
        }

        // เนื้อหารายการในกลุ่ม
        Item {
            Layout.fillWidth: true
            height: expanded ? contentCol.implicitHeight : 0
            clip: true
            Behavior on height { NumberAnimation { duration: 150 } }

            Column {
                id: contentCol
                width: parent.width
                spacing: 8

                Repeater {
                    model: items ? items.length : 0
                    delegate: Rectangle {
                        radius: 10
                        color: "#141b22"
                        border.width: 1
                        border.color: "#1f2b38"

                        // อย่าใช้ anchors.left/right ใน Column; กำหนดความกว้างตรง ๆ
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
}
