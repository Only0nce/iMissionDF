// RemoteSdrRow.qml
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: rowWrap

    // ===== Inputs =====
    // ส่ง listView มาเป็น view จาก delegate
    property var view              // e.g., view: listView

    // ขนาด: เต็มความกว้างของ view ถ้ามี ไม่งั้นใช้ของ parent
    width: view ? view.width : (parent ? parent.width : implicitWidth)

    // ใช้ implicitHeight ของ RemoteSdrItem ถ้ามี
    height: deviceRow.implicitHeight > 0 ? deviceRow.implicitHeight : deviceRow.height

    // ===== Signals =====
    signal rowClicked(string groupName, int i)

    // พาเนลหลัก
    RemoteSdrItem {
        id: deviceRow
        anchors.fill: parent

        // mapping roles -> props
        deviceName:   model.DeviceName
        deviceIp:     model.IPAddress
        devicePort:   model.Port
        deviceStatus: model.status
        deviceRssi:   0
        rowIndex:     index
    }

    // ไฮไลต์เมื่อ current
    Rectangle {
        anchors.fill: parent
        radius: 8
        color: "transparent"
        // ใช้ Qt.rgba เพื่อเลี่ยง parser งอแงกับ #AARRGGBB
        border.color: Qt.rgba(0.0, 0.0625, 0.2, 0.1)  // เดิม "#0010331A"
        // เลี่ยง isCurrentItem; เทียบ currentIndex เอง
        border.width: (view && view.currentIndex === index) ? 2 : 1
        z: 1
    }

    MouseArea {
        anchors.fill: parent
        onClicked: rowWrap.rowClicked(model.GroupsName, index)
    }
}
