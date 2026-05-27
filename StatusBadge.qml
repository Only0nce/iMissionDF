import QtQuick 2.15

Rectangle {
    id: root

    property string textValue: "Idle"
    property color badgeColor: "#2f4055"

    radius: height / 2
    color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.18)
    border.color: badgeColor
    border.width: 1
    implicitWidth: label.implicitWidth + 24
    implicitHeight: 30

    Text {
        id: label
        anchors.centerIn: parent
        text: root.textValue
        color: root.badgeColor
        font.pixelSize: 13
        font.bold: true
        elide: Text.ElideRight
    }
}
