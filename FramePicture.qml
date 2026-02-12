import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
Item {
    id: element
    width: 120
    height: 120
    property string fileName: ""
    property alias buttonImage: buttonImage
    ToolButton {
        id: buttonImage
        anchors.rightMargin: 1
        anchors.leftMargin: 1
        anchors.bottomMargin: 1
        anchors.topMargin: 1
        anchors.fill: parent
        contentItem: Image {
            id: image
            fillMode: Image.PreserveAspectCrop
            source: fileName
            anchors.fill: parent
        }
    }
}
