import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
Item {
    id: item1
    property real ctrlLevel: 9
    property alias volumeCtrlLevel: volumeCtrlLevel
    property real currentSqlType: 0
    property string stringVolume: text2.text

    width: 100
    height: 400


    Rectangle {
        id: rectangle1
        width: 45
        color: "#00ffffff"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.rightMargin: 20
        anchors.topMargin: 20
        anchors.bottomMargin: 20

        Rectangle {
            id: rectangle
            color: "#4d116273"
            radius: 5
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.leftMargin: 0
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.bottomMargin: 4

            Volume {
                id: volumeCtrlLevel
                x: 0
                y: 62
                anchors.right: parent.right
                anchors.rightMargin: 0
                anchors.left: parent.left
                anchors.leftMargin: 0
                anchors.top: parent.top
                anchors.topMargin: 96
                anchors.bottom: parent.bottom
                inivalue: ctrlLevel
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 64
                Layout.preferredHeight: height-20
                levelmax: 255
                levelmin: 0
                slider.value:inivalue
                progressBar.value: slider.value
            }

            Text {
                id: text2
                x: -7
                y: 24
                width: 60
                height: 45
                color: "#ffffff"
                text: ((ctrlLevel-255)/2).toFixed(1)+" dB"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                rotation: 270
            }
        }
    }

    Rectangle {
        id: rectangle2
        width: 30
        height: 450
        color: "#00ffffff"
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 71

        Text {
            id: text1
            color: "#ffffff"
            text: "SQL Level"
            anchors.fill: parent
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            styleColor: "#ffffff"
            rotation: 270
        }
    }

}



/*##^##
Designer {
    D{i:0;formeditorZoom:0.66}D{i:1}
}
##^##*/
