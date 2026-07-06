import QtQuick 2.12

Column {
    id: root
    property string title: ""
    property string subtitle: ""
    property color titleColor: "#e6edf3"
    property color subtitleColor: "#9aa6b2"

    spacing: 4
    implicitWidth: Math.max(titleText.implicitWidth, subtitleText.implicitWidth)
    implicitHeight: titleText.implicitHeight + subtitleText.implicitHeight + spacing

    Text {
        id: titleText
        width: root.width
        text: root.title
        color: root.titleColor
        font.pixelSize: 28
        font.bold: true
        elide: Text.ElideRight
    }

    Text {
        id: subtitleText
        width: root.width
        text: root.subtitle
        color: root.subtitleColor
        font.pixelSize: 14
        wrapMode: Text.WordWrap
    }
}
