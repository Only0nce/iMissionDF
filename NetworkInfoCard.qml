import QtQuick 2.12

Rectangle {
    id: root
    property string title: "Title"
    property string value: "-"
    property string sub: ""
    property color dotColor: "#00c9a7"
    property color cardColor: "#122033"
    property color borderLineColor: "#2d4056"
    property color titleColor: "#9aa8b8"
    property color valueColor: "#e9f0f7"
    property color subColor: "#667589"

    color: cardColor
    border.color: borderLineColor
    border.width: 1
    radius: 10

    Rectangle {
        x: 18
        y: 18
        width: 14
        height: 14
        radius: 7
        color: root.dotColor
    }

    Text {
        x: 44
        y: 13
        text: root.title
        color: root.titleColor
        font.family: "Inter"
        font.pixelSize: 14
        font.bold: true
        elide: Text.ElideRight
    }

    Text {
        x: 18
        y: 50
        width: parent.width - 36
        text: root.value
        color: root.valueColor
        font.family: "Inter"
        font.pixelSize: 22
        font.bold: true
        elide: Text.ElideRight
    }

    Text {
        x: 18
        y: 83
        width: parent.width - 36
        text: root.sub
        color: root.subColor
        font.family: "Inter"
        font.pixelSize: 13
        elide: Text.ElideRight
    }
}
