import QtQuick 2.12

Item {
    id: root
    property string label: "Key"
    property string value: "-"
    property int labelWidth: 145
    property color labelColor: "#9aa8b8"
    property color valueColor: "#e9f0f7"

    width: 400
    height: 34

    Text {
        x: 0
        y: 0
        width: root.labelWidth
        text: root.label
        color: root.labelColor
        font.family: "Inter"
        font.pixelSize: 14
        font.bold: true
        elide: Text.ElideRight
    }

    Text {
        x: root.labelWidth
        y: 0
        width: root.width - root.labelWidth
        text: root.value
        color: root.valueColor
        font.family: "Inter"
        font.pixelSize: 16
        elide: Text.ElideRight
    }
}
