// RemoteSdrRow.qml
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: rowWrap
    width: ListView.view ? ListView.view.width : parent.width
    // ใช้ implicitHeight ของ RemoteSdrItem ถ้ามี
    height: deviceRow.implicitHeight > 0 ? deviceRow.implicitHeight : deviceRow.height

    // ===== Inputs =====
    property var view              // listView
    // มี model.* และ index จาก delegate context ให้ใช้ได้ตรง ๆ

    // ===== Signals =====
    signal rowClicked(string groupName, int index)

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
        border.width: ListView.isCurrentItem ? 2 : 1
        border.color: ListView.isCurrentItem ? "#3fe28a" : "#10331a"
        z: 1
    }

    MouseArea {
        anchors.fill: parent
        onClicked: rowWrap.rowClicked(model.GroupsName, index)
    }
}
