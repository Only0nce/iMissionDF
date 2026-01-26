// AvailableDeviceDelegate.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ItemDelegate {
    id: root
    // ===== ปรับแต่งสไตล์จากภายนอกได้ =====
    property int  rowH: 40
    property int  rad: 12
    property color colText: "#e0e0e0"
    property color colAccent: "#8bdac2"

    // ===== สำหรับ filter ว่าอุปกรณ์นี้อยู่ในกลุ่มปัจจุบันไหม =====
    // ใส่ selectedGroupIndex และฟังก์ชันตรวจสอบจากภายนอกเข้ามา
    property int  selectedGroupIndex: -1
    property var  isInCurrentGroup: null  // ฟังก์ชันแบบ (idStr) => bool

    // หมายเหตุ: ตัวแปร role จาก model เช่น idStr, name, status
    // จะมองเห็นได้ตรง ๆ ภายใน delegate (ไม่ต้องประกาศซ้ำ)

    width: ListView.view ? ListView.view.width : implicitWidth
    height: rowH
    visible: selectedGroupIndex >= 0
             ? !(isInCurrentGroup ? isInCurrentGroup(idStr) : false)
             : true
    implicitHeight: visible ? rowH : 0

    hoverEnabled: true

    background: Rectangle {
        radius: root.rad - 6
        color: root.down
               ? "#27303d"
               : root.hovered ? "#202733" : "transparent"
        border.color: "transparent"
    }

    contentItem: Row {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Label {
            // ใช้ role จาก model ตรง ๆ
            text: name + "  [" + idStr + "]"
            color: root.colText
            font.pixelSize: 14
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Item { Layout.fillWidth: true }

        Label {
            text: status
            color: status === "Online" ? root.colAccent : "#ff6b6b"
            font.pixelSize: 12
        }
    }
}
