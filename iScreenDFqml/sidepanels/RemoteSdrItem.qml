import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.12
import "../../ui"

Rectangle {
    id: remotedevicelist
    property string deviceName: ""
    property string deviceIp: ""
    property int    devicePort: -1
    property string deviceStatus: ""
    property int    deviceRssi: 0
    property int    rowIndex: 0
    property bool   isCurrent: false    // รับจาก ListView.isCurrentItem
    property bool   darkMode: true
    property bool   hovered: false
    property var    krakenmapval: null
    Theme { id: hmiTheme; darkMode: remotedevicelist.darkMode }

    readonly property color rowBg:        isCurrent ? hmiTheme.remoteRowActive : hmiTheme.remoteRow
    readonly property color rowHoverBg:   hmiTheme.remoteRowHover
    readonly property color rowBorder:    isCurrent ? hmiTheme.remoteRowBorderActive : hmiTheme.remoteRowBorder
    readonly property color rowText:      hmiTheme.text
    readonly property color rowSubText:   hmiTheme.textSecondary
    readonly property color statusColor:  String(deviceStatus).toLowerCase() === "online" ? hmiTheme.success : hmiTheme.muted
    readonly property string detailText: {
        var ip = String(deviceIp || "").trim()
        var status = String(deviceStatus || "").trim()
        var portText = (devicePort > 0) ? (":" + devicePort) : ""
        var left = ip.length > 0 ? (ip + portText) : ""
        if (left.length > 0 && status.length > 0)
            return left + "  ·  " + status
        return left.length > 0 ? left : status
    }

    signal clicked()                    // แจ้งคลิกออกไป

    Component.onCompleted: {
        console.log("[RemoteSdrItem] got:", deviceName, deviceStatus, deviceIp, devicePort, deviceRssi)
    }

    width: parent ? parent.width : 300
    height: 74
    color: hovered ? rowHoverBg : rowBg
    radius: 8

    border.color: rowBorder
    border.width: 2

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: remotedevicelist.hovered = true
        onExited:  remotedevicelist.hovered = false
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
        Rectangle {
            width: 9
            height: 9
            radius: 5
            color: remotedevicelist.statusColor
            Layout.alignment: Qt.AlignVCenter
            opacity: 1.0
        }

        Column {
            Layout.fillWidth: true           // << สำคัญ
            Layout.alignment: Qt.AlignVCenter
            spacing: 3
            clip: true

            Text {
                text: remotedevicelist.deviceName || "(unnamed)"
                color: rowText
                font.pixelSize: 14
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                visible: remotedevicelist.detailText.length > 0
                text: remotedevicelist.detailText
                color: rowSubText
                font.pixelSize: 11
                elide: Text.ElideRight
                opacity: 0.95
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
        Rectangle {
            id: gearBtn
            width: 30
            height: 30
            radius: 15
            color: remotedevicelist.darkMode ? "#28433E" : "#F4FAF8"
            border.width: 1
            border.color: gearMouse.containsMouse ? hmiTheme.accentHover : hmiTheme.lineStrong
            Layout.rightMargin: 4
            Layout.alignment: Qt.AlignVCenter

            Image {
                id: gearBtnIconSource
                anchors.centerIn: parent
                source: "qrc:/iScreenDFqml/images/gearicon.png"
                width: 18
                height: 18
                fillMode: Image.PreserveAspectFit
                visible: false
            }

            ColorOverlay {
                anchors.fill: gearBtnIconSource
                source: gearBtnIconSource
                color: remotedevicelist.darkMode ? "#ECF6F4" : hmiTheme.navTileText
                cached: true
            }

            MouseArea {
                id: gearMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (typeof krakenmapval !== "undefined" && krakenmapval)
                        krakenmapval.openPopupSetting("Setting Parameter")
                    console.log("settings:", remotedevicelist.deviceName, remotedevicelist.deviceIp + ":" + remotedevicelist.devicePort)
                }
            }
        }

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
                opacity: remotedevicelist.darkMode ? 1.0 : 0.82
            }

            MouseArea {
                anchors.fill: parent

                onPressed: {
                    ringImage.scale = 1.2          // ขยายเล็กน้อยตอนกด
                    ringImage.opacity = 0.6        // ทำให้จางลงตอนกด
                }
                onReleased: {
                    ringImage.scale = 1.0          // กลับสภาพเดิม
                    ringImage.opacity = remotedevicelist.darkMode ? 1.0 : 0.9
                    console.log("select:", remotedevicelist.deviceName)
                }
            }

            Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.InOutQuad } }
            Behavior on opacity { NumberAnimation { duration: 100 } }
        }

    }
}
