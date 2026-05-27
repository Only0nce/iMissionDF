import QtQuick 2.15

Text {
    property color textColor: "#9aa6b2"
    property int labelFontSize: 13

    color: textColor
    font.pixelSize: labelFontSize
    font.bold: true
    elide: Text.ElideRight
}
