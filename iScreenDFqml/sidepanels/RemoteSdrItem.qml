import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3

Rectangle {
    id: remotedevicelist
    property string deviceName: ""
    property string deviceIp: ""
    property int    devicePort: -1
    property string deviceStatus: ""
    property int    deviceRssi: 0
    property int    rowIndex: 0
    property bool   isCurrent: false    // รับจาก ListView.isCurrentItem
    signal clicked()                    // แจ้งคลิกออกไป

    Component.onCompleted: {
        console.log("[RemoteSdrItem] got:", deviceName, deviceStatus, deviceIp, devicePort, deviceRssi)
    }

    width: parent ? parent.width : 300
    height: 64
    color: "#2a2a2a"
    radius: 8

    border.color: isCurrent ? "#48ff9a" : "#2a2a2a"
    border.width: 2

    MouseArea {
        anchors.fill: parent
        onClicked: remotedevicelist.clicked()
    }

    RowLayout {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        anchors.topMargin: 0
        spacing: 12

        // ==== แท่งสัญญาณ ====
        // Item {
        //     id: bars
        //     width: 26; height: 26
        //     Layout.alignment: Qt.AlignVCenter
        //     property color ok:  "#39d353"
        //     property color dim: "#2a6a3a"
        //     Repeater {
        //         model: 4
        //         Rectangle {
        //             width: 4
        //             height: 8 + index * 4
        //             radius: 1
        //             anchors.bottom: parent.bottom
        //             x: index * 6
        //             color: (index < remotedevicelist.deviceRssi) ? bars.ok : bars.dim
        //         }
        //     }
        // }

        // ==== ชื่อ + สถานะ (ปล่อยกินที่ได้เต็มที่) ====
        Column {
            Layout.fillWidth: true           // << สำคัญ
            Layout.alignment: Qt.AlignVCenter
            spacing: 2
            clip: true

            Text {
                text: remotedevicelist.deviceName || "(unnamed)"
                color: "#e6f7ec"
                font.pixelSize: 14
                font.bold: true
                elide: Text.ElideRight
            }
            // Text {
            //     color: (remotedevicelist.deviceStatus === "Online") ? "#9ae6b4" : "#ffc9c9"
            //     text: remotedevicelist.deviceStatus || "(unknown)"
            //     font.pixelSize: 12
            //     elide: Text.ElideRight
            // }
        }

        // ช่องว่างดันปุ่มไปขวา (ยืดได้)
        Item { Layout.fillWidth: true }

        // ==== Settings ====
        // Rectangle {
        //     id: gearBtn
        //     width: 30; height: 30; radius: 14
        //     color: "transparent"; border.color: "#2a6a3a"
        //     Layout.rightMargin: 10
        //     Layout.alignment: Qt.AlignVCenter
        //     // Text { anchors.centerIn: parent; text: "\u2699"; color: "#d9f7e4"; font.pixelSize: 16 }
        //     Image {
        //         id: penIcon
        //         anchors.centerIn: parent
        //         source: "qrc:/iScreenDFqml/images/gearicon.png"    // หรือ "images/add_icon.png"
        //         width: 33
        //         height: 33
        //         fillMode: Image.PreserveAspectFit
        //     }
        //     MouseArea {
        //         anchors.fill: parent
        //         hoverEnabled: true
        //         onEntered: gearBtn.border.color = "#4bc46d"
        //         onExited:  gearBtn.border.color = "#2a6a3a"
        //         onClicked: {
        //             if (krakenmapval) krakenmapval.openPopupSetting("Setting Parameter")
        //             console.log("settings:", root.deviceName, root.deviceIp + ":" + root.devicePort)
        //         }
        //     }
        // }

        // ==== วงแหวนเลือกอุปกรณ์ ====
        // Rectangle {
        //     id: ring
        //     width: 25; height: 25; radius: 11
        //     color: "transparent"; border.width: 2; border.color: "#94e3ab"
        //     Layout.alignment: Qt.AlignVCenter
        //     Rectangle {
        //         id: dot
        //         anchors.centerIn: parent
        //         width: 10; height: 10; radius: 5
        //         color: "transparent"; visible: false
        //     }
        //     MouseArea {
        //         anchors.fill: parent
        //         onClicked: {
        //             dot.visible = !dot.visible
        //             dot.color = dot.visible ? "#3fbd6a" : "transparent"
        //             console.log("select:", root.deviceName)
        //         }
        //     }
        // }
        Rectangle {
            id: ring
            width: 30
            height: 30
            color: "transparent"

            Image {
                id: ringImage
                anchors.fill: parent
                source: "qrc:/iScreenDFqml/images/target_ring.png"   // ไฟล์ target โปร่งใส
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            MouseArea {
                anchors.fill: parent

                onPressed: {
                    ringImage.scale = 1.2          // ขยายเล็กน้อยตอนกด
                    ringImage.opacity = 0.6        // ทำให้จางลงตอนกด
                }
                onReleased: {
                    ringImage.scale = 1.0          // กลับสภาพเดิม
                    ringImage.opacity = 1.0
                    console.log("select:", root.deviceName)
                }
            }

            Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.InOutQuad } }
            Behavior on opacity { NumberAnimation { duration: 100 } }
        }

    }
}
