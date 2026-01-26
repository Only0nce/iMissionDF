import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    anchors.fill: parent

    // ===== อินพุต =====
    // ต้องเป็น ListModel ที่มี role: GroupsName, DeviceName, IPAddress, Port, status
    property var model: null

    // ชื่อกลุ่มที่เลือกอยู่ (สองทาง: parent ผูก binding เข้ามาได้)
    property string selectedGroup: ""

    // ส่งสถานะกลับให้ผู้ใช้คอมโพเนนต์
    signal groupSelected(string groupName)
    signal deviceSelected(string groupName, string deviceName, string ip, int port, string status, int index)

    // helper เล็ก ๆ
    function firstIndexOfGroup(groupName) {
        if (!model) return -1
        for (var i = 0; i < model.count; ++i) {
            var it = model.get(i)
            if (it.GroupsName === groupName) return i
        }
        return -1
    }
    function countInGroup(groupName) {
        if (!model) return 0
        var c = 0
        for (var i = 0; i < model.count; ++i)
            if (model.get(i).GroupsName === groupName) ++c
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

            // ===== แบ่งกลุ่มตามชื่อ =====
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
                        root.groupSelected(section)
                    }
                }
            }

            // ===== ใช้ RemoteSdrItem เป็น delegate ของอุปกรณ์ =====
            delegate: Item {
                id: rowWrap
                width: ListView.view ? ListView.view.width : parent.width
                height: deviceRow.implicitHeight > 0 ? deviceRow.implicitHeight : deviceRow.height

                // แผงหลัก
                RemoteSdrItem {
                    id: deviceRow
                    anchors.fill: parent

                    // mapping role -> props ของ RemoteSdrItem
                    deviceName:   model.DeviceName
                    deviceIp:     model.IPAddress
                    devicePort:   model.Port
                    deviceStatus: model.status
                    deviceRssi:   0        // หากไม่มี rssi ในโมเดล
                    rowIndex:     index
                }

                // ไฮไลท์เมื่อถูกเลือก
                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: "transparent"
                    border.width: ListView.isCurrentItem ? 2 : 1
                    border.color: ListView.isCurrentItem ? "#3fe28a" : "#10331a"
                    z: 1
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        listView.currentIndex = index
                        root.selectedGroup = model.GroupsName
                        listView.forceActiveFocus()
                        root.deviceSelected(model.GroupsName, model.DeviceName, model.IPAddress, model.Port, model.status, index)
                    }
                }
            }
        }
    }
}
