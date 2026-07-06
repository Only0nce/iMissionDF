import QtQuick 2.12

Rectangle {
    id: root
    property color accentColor: "#00c9a7"
    property color borderColor: "#2d4056"
    property color fillColor: "#17212e"

    width: 42
    height: 42
    radius: 21
    color: fillColor
    border.color: borderColor
    border.width: 1

    Column {
        anchors.centerIn: parent
        spacing: 5
        Repeater {
            model: 3
            Rectangle {
                width: 16
                height: 3
                radius: 2
                color: root.accentColor
            }
        }
    }
}
