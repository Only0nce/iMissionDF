import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    width: 100
    height: collapsed ? handleHeight : expandedHeight

    // ===================== CONFIG =====================
    property int expandedHeight: 95
    property int handleHeight: 32          // ✅ เพิ่มความสูงตอนซ่อนให้หน่อย
    property bool collapsed: true

    // auto hide
    property int autoHideMs: 10000   // 10s

    // expose button
    property alias toolButtonPower: toolButtonPower

    function restartAutoHide() {
        if (!collapsed) autoHideTimer.restart()
        else autoHideTimer.stop()
    }

    Timer {
        id: autoHideTimer
        interval: autoHideMs
        repeat: false
        onTriggered: collapsed = true
    }

    Behavior on height {
        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 10
        color: "#009688"
        clip: true
    }

    // ===================== HANDLE (ขีด) =====================
    Rectangle {
        id: handle
        visible: collapsed
        enabled: collapsed

        // ✅ ขยาย "ขีด" ให้ใหญ่ขึ้นตอนซ่อน
        width: 54                 // เดิม 36
        height: 8                 // เดิม 4
        radius: height / 2
        color: "#E0F2F1"

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 12      // ขยับลงนิดให้บาลานซ์
        opacity: collapsed ? 1.0 : 0.0

        Behavior on opacity { NumberAnimation { duration: 150 } }

        // ✅ ขยายพื้นที่กดให้ใหญ่ขึ้น (กดง่าย)
        MouseArea {
            // โซนกดใหญ่รอบขีด
            x: -40
            y: -16
            width: handle.width + 160
            height: handle.height + 80

            cursorShape: Qt.PointingHandCursor
            onClicked: {
                collapsed = false
                restartAutoHide()
                console.log("HANDLE clicked, collapsed:", collapsed)
            }
        }
    }

    // ===================== POWER BUTTON =====================
    ToolButton {
        id: toolButtonPower
        anchors.fill: parent
        visible: !collapsed
        enabled: !collapsed
        opacity: collapsed ? 0.0 : 1.0

        Behavior on opacity { NumberAnimation { duration: 180 } }

        contentItem: Item {
            anchors.fill: parent

            Image {
                id: icon
                source: "images/powerButton.png"
                width: 60
                fillMode: Image.PreserveAspectFit
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -6
            }

            Label {
                text: qsTr("Power")
                font.pointSize: 10
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 6
                color: "white"
            }
        }

        // กดปุ่มแล้วซ่อนกลับ (คุณจะเปลี่ยนเป็น collapsed=true ก็ได้)
        onClicked: {
            console.log("Power clicked")
            restartAutoHide()
        }
    }

    onCollapsedChanged: restartAutoHide()
    Component.onCompleted: restartAutoHide()
}
