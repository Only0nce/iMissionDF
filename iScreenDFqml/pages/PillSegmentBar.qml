// PillSegmentBar.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.12

Item {
    id:  pillsegmentbar
    property alias model: repeater.model          // [{iconSource:"", iconText:"", tooltip:""}, ...]
    property int currentIndex: 0
    signal triggered(int index)

    implicitHeight: 35
    implicitWidth: Math.max(160, (model && model.length ? model.length : 0) * (implicitHeight + 18))

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: height/2
        color: "#2a2a2a"
        border.color: "#2c2c31"
        border.width: 1
    }

    readonly property real segWidth: {
        const n = repeater.count > 0 ? repeater.count : 1
        return (width - 8) / n
    }

    Rectangle {
        id: highlight
        anchors.verticalCenter: parent.verticalCenter
        radius: height/2
        height: parent.height - 8
        color: "#768983"
        border.color: "#44bb94"

        Binding { target: highlight; property: "width"; value: segWidth }
        Binding { target: highlight; property: "x";     value: 4 + segWidth * pillsegmentbar.currentIndex }

        Behavior on x     { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
    }

    Row {
        id: row
        anchors.fill: parent
        anchors.margins: 4
        spacing: 0

        Repeater {
            id: repeater
            model: []    // กำหนดจากภายนอก

            // ถ้าจำนวนปุ่มเปลี่ยน ให้ clamp currentIndex ให้อยู่ในช่วงเสมอ
            onCountChanged: {
                if (pillsegmentbar.currentIndex >= count) pillsegmentbar.currentIndex = Math.max(0, count - 1)
                if (pillsegmentbar.currentIndex < 0) pillsegmentbar.currentIndex = 0
            }

            delegate: Item {
                id: cell
                width: segWidth
                height: row.height

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: index === repeater.count - 1 ? 0 : 1
                    color: "#2c2c31"
                    opacity: 0.9
                }

                MouseArea {
                    id: ma
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        // อัปเดต index ภายใน + ส่งสัญญาณออกไป
                        if (pillsegmentbar.currentIndex !== index) pillsegmentbar.currentIndex = index
                        pillsegmentbar.triggered(index)
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: height/2
                    color: "#ffffff"
                    opacity: ma.containsMouse ? 0.06 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                }

                Item {
                    id: iconWrap
                    anchors.centerIn: parent
                    width: 22; height: 22

                    Image {
                        id: img
                        anchors.centerIn: parent
                        visible: !!(modelData.iconSource && modelData.iconSource.length)
                        source: modelData.iconSource
                        sourceSize.width: 18
                        sourceSize.height: 18
                        fillMode: Image.PreserveAspectFit
                        mipmap: true
                        antialiasing: true
                        layer.enabled: true
                        layer.smooth: true
                        opacity: pillsegmentbar.currentIndex === index ? 1.0 : 0.75
                    }

                    Text {
                        id: glyph
                        visible: !img.visible
                        anchors.centerIn: parent
                        text: (modelData.iconText || "\u2713")
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        color: pillsegmentbar.currentIndex === index ? "#e9e7f5" : "#b9b6c8"
                        opacity: pillsegmentbar.currentIndex === index ? 1.0 : 0.85
                        antialiasing: true
                    }
                }

                ToolTip.visible: ma.containsMouse && !!modelData.tooltip
                ToolTip.text: modelData.tooltip || ""
                ToolTip.delay: 250
            }
        }
    }

    // === แก้: clamp index ทุกครั้งที่เปลี่ยนจากภายนอกด้วย ===
    onCurrentIndexChanged: {
        if (repeater.count > 0) {
            if (currentIndex < 0) currentIndex = 0
            else if (currentIndex >= repeater.count) currentIndex = repeater.count - 1
        } else {
            currentIndex = 0
        }
        // ไม่ต้องจับ x เอง เพราะผูก Binding ไว้แล้ว
    }
}
