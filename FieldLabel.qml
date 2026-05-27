import QtQuick 2.15

Text {
    property color textColor: "#9aa6b2"

    color: textColor
    font.pixelSize: 13
    font.bold: true
    elide: Text.ElideRight
}
