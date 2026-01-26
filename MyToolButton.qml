import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    property string bname: "name"
    property string bColor: "#000000"
    property real buttonID: 0
    property alias label: label
    width: 120
    height: 40
    rotation: 0
    property alias toolButton: toolButton
    ToolButton {
        id: toolButton
        anchors.fill: parent
        Rectangle {
            color: bColor
            radius: 5
            border.color: "#ffffff"
            border.width: 0
            anchors.fill: parent
            Label {
                id: label
                width: 40
                text: bname
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pointSize: 12
                anchors.topMargin: 0
                font.bold: false
                anchors.bottomMargin: 0
            }
        }
    }
}

/*##^##
Designer {
    D{i:0;formeditorColor:"#000000"}D{i:3}D{i:2}D{i:1}
}
##^##*/
