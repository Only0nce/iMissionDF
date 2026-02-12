 // /popuppanels/CancelButtonPopupSettingDrawer.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
// Button {
//     id: cancelButton
//     text: qsTr("Cancel")

//     width: 140
//     height: 44
//     font.bold: true

//     background: Rectangle {
//         radius: 10
//         color: "#333333"
//         border.color: "#555555"
//     }

//     contentItem: Text {
//         text: cancelButton.text
//         anchors.fill: parent
//         anchors.margins: 0
//         font.pixelSize: 16
//         font.bold: true
//         color: "white"
//         horizontalAlignment: Text.AlignHCenter
//         verticalAlignment: Text.AlignVCenter
//     }
// }

Rectangle {
    id: cancelButtonPopupSettingDrawer
    width: 70; height: 40
    radius: height/2
    anchors.right: parent.right
    anchors.rightMargin: 30
    color: "#25303b"
    Layout.alignment: Qt.AlignVCenter

    signal clicked()
    Text { text: "OK"; anchors.centerIn: parent; color: "white"; font.pixelSize: 15; font.bold: true }
    // Image {
    //     id: iconImg
    //     source: "qrc:/images/delete.svg"   // ✅ ใส่รูปของคุณตรงนี้
    //     anchors.centerIn: parent
    //     width: 22
    //     height: 22
    //     fillMode: Image.PreserveAspectFit
    //     smooth: true
    // }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true

        onClicked: {
            cancelButtonPopupSettingDrawer.clicked()
        }

        // Hover effect
        onEntered: cancelButtonPopupSettingDrawer.color = "#324152"
        onExited:  cancelButtonPopupSettingDrawer.color = "#25303b"
    }
}
