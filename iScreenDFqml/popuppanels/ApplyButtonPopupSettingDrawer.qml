 // /popuppanels/ApplyButtonPopupSettingDrawer.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Button {
//     id: applyButton
//     text: qsTr("Apply")

//     width: 140
//     height: 44
//     font.bold: true

//     background: Rectangle {
//         radius: 10
//         color: "#2980b9" // ฟ้า
//         border.color: "#1f6c95"
//     }

//     contentItem: Text {
//         text: applyButton.text
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
    id: applyButtonPopupSettingDrawer
    width: 70; height: 40
    radius: height/2
    anchors.right: parent.right
    anchors.rightMargin: 30
    color: "#25303b"
    Layout.alignment: Qt.AlignVCenter

    signal clicked()

    Text { text: "Save"; anchors.centerIn: parent; color: "white"; font.pixelSize: 15; font.bold: true }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true

        onClicked: {
            applyButtonPopupSettingDrawer.clicked()
        }

        // Hover effect
        onEntered: applyButtonPopupSettingDrawer.color = "#324152"
        onExited:  applyButtonPopupSettingDrawer.color = "#25303b"
    }
}
