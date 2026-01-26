import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
Item {
    id: element
    property alias accepted: accepted
    property alias abort: abort
    property string title: "Error"

    ColumnLayout {
        x: 44
        y: 0
        width: 514
        height: 232
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenterOffset: 0
        anchors.verticalCenter: parent.verticalCenter

        BusyIndicator {
            id: busyIndicator
            visible: title=="Restarting"
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
        }

        Text {
            id: element1
            color: "#ffffff"
            text: title
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            font.pixelSize: 16
            Layout.preferredHeight: 100
            Layout.fillHeight: false
            Layout.fillWidth: true
            Layout.preferredWidth: 640
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }

        RowLayout {
            Layout.preferredHeight: 80
            Layout.preferredWidth: 530

            Button{
                id: accepted
                text: "Ok"
                visible: busyIndicator.visible == false
                Layout.preferredHeight: 60
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.preferredWidth: 200
                font.pointSize: 24

            }

            Button {
                id: abort
                text: "CANCEL"
                Layout.fillWidth: true
                visible: false
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                Layout.preferredHeight: 68
                Layout.preferredWidth: 200
                font.pointSize: 24
            }
        }
    }
//    Rectangle {
//        id: rectangle
//        color: "#000000"
//        anchors.fill: parent
//    }
}















/*##^##
Designer {
    D{i:0;autoSize:true;height:300;width:640}D{i:2}D{i:4}D{i:5}D{i:3}D{i:1}
}
##^##*/

