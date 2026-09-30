import QtQuick 2.15
import QtQuick.Controls 2.15
import "ui"

Item {
    id: root
    property string bname: "name"
    property string bColor: "#000000"
    property real buttonID: 0
    property alias label: label
    property bool darkMode: true
    width: 120
    height: 40
    rotation: 0
    property alias toolButton: toolButton

    Theme { id: hmiTheme; darkMode: root.darkMode }

    ToolButton {
        id: toolButton
        anchors.fill: parent
        hoverEnabled: true

        background: Rectangle {
            radius: 8
            color: bColor
            border.color: hmiTheme.lineStrong
            border.width: 1
        }

        contentItem: Label {
            id: label
            text: bname
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pointSize: 12
            font.bold: true
            color: hmiTheme.darkMode ? "#F5FCFA" : "#17312B"
            elide: Text.ElideRight
        }
    }
}
