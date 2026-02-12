// RemoteSdrGroupView.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    anchors.fill: parent

    // ====== อินพุต ======
    // ส่งโมเดลเข้ามาจากภายนอก (ListModel ที่มี role: GroupsName, DeviceName, IPAddress, Port, status)
    property var model: null

    // ชื่อกลุ่มที่ถูกโฟกัสอยู่
    property string selectedGroup: ""

    // ====== helper ======
    function firstIndexOfGroup(groupName) {
        if (!root.model) return -1
        for (var i = 0; i < root.model.count; ++i) {
            var it = root.model.get(i)
            if (it.GroupsName === groupName) return i
        }
        return -1
    }
    function countInGroup(groupName) {
        if (!root.model) return 0
        var c = 0
        for (var i = 0; i < root.model.count; ++i)
            if (root.model.get(i).GroupsName === groupName) ++c
        return c
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: 10
        color: "#00111212"
        border.color: "#00111212"

        ListView {
            id: listView
            anchors.fill: parent
            anchors.margins: 10
            clip: true
            spacing: 6
            model: root.model
            focus: true
            currentIndex: -1

            // ==== จัดกลุ่มตามชื่อกลุ่ม ====
            section.property: "GroupsName"
            section.criteria: ViewSection.FullString
            section.delegate: Rectangle {
                width: ListView.view ? ListView.view.width : parent.width
                height: 34
                radius: 8
                color: (root.selectedGroup === section) ? "#20633a" : "#132a1e"
                border.color: (root.selectedGroup === section) ? "#48ff9a" : "#214a36"

                Row {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    Text {
                        text: section
                        color: "#e6f7ec"
                        font.pixelSize: 14
                        font.bold: true
                    }
                    Text {
                        text: "(" + root.countInGroup(section) + ")"
                        color: "#9bd9b3"
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.selectedGroup = section
                        const idx = root.firstIndexOfGroup(section)
                        if (idx >= 0) {
                            listView.currentIndex = idx
                            listView.positionViewAtIndex(idx, ListView.Beginning)
                            listView.forceActiveFocus()
                        }
                        console.log("[Group] focus:", section, "first index =", idx)
                    }
                }
            }

            // ==== ใช้ RemoteSdrItem เป็น "ตัวอุปกรณ์" ใน delegate ====
            delegate: Item {
                id: rowWrap
                width: ListView.view ? ListView.view.width : parent.width
                height: deviceRow.implicitHeight > 0 ? deviceRow.implicitHeight : deviceRow.height

                // อ่านค่า role จาก model แล้วส่งเข้า RemoteSdrItem
                RemoteSdrItem {
                    id: deviceRow
                    anchors.fill: parent

                    // แปลงชื่อ role → prop ของ RemoteSdrItem (ที่คุณมีอยู่เดิม)
                    deviceName:   model.DeviceName
                    deviceIp:     model.IPAddress
                    devicePort:   model.Port
                    deviceStatus: model.status
                    deviceRssi:   0            // ถ้าไม่มี role rssi ในโมเดลนี้ ให้ 0 ไปก่อน (หรือเพิ่มในโมเดล)
                    rowIndex:     index
                }

                // ไฮไลท์เมื่อเป็นแถวที่ถูกเลือก (ไม่ต้องแก้ RemoteSdrItem)
                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: "transparent"
                    border.width: ListView.isCurrentItem ? 2 : 1
                    border.color: ListView.isCurrentItem ? "#3fe28a" : "#10331a"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        listView.currentIndex = index
                        // sync group ตามอุปกรณ์ที่คลิก
                        root.selectedGroup = model.GroupsName
                        listView.forceActiveFocus()
                        console.log("[Device] select:", model.DeviceName, model.IPAddress + ":" + model.Port,
                                    "group:", model.GroupsName, "index:", index)
                    }
                }
            }
        }
    }
}
