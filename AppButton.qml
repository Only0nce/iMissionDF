import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: root

    property color baseColor: "#2fa6ff"
    property color textColor: "white"

    implicitHeight: 42

    contentItem: Text {
        text: root.text
        color: root.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 14
        font.bold: true
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 8
        color: root.enabled ? root.baseColor : "#354052"
        opacity: root.down ? 0.75 : 1.0
        border.color: Qt.lighter(color, 1.15)
        border.width: 1
    }
}
