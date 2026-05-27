import QtQuick 2.15
import QtQuick.Controls 2.15

TextField {
    id: root

    property color textColor: "#e6edf3"
    property color accentColor: "#00c896"
    property color borderColor: "#2f4055"
    property color fillColor: "#0d1520"

    color: textColor
    placeholderTextColor: "#697789"
    selectionColor: accentColor
    selectedTextColor: "#061016"
    font.pixelSize: 15
    leftPadding: 12
    rightPadding: 12
    implicitHeight: 42

    background: Rectangle {
        radius: 8
        color: root.fillColor
        border.color: root.activeFocus ? root.accentColor : root.borderColor
        border.width: 1
    }
}
