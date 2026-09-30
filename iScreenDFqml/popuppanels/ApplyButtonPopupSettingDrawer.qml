 // /popuppanels/ApplyButtonPopupSettingDrawer.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../ui"

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
    property bool darkMode: true
    Theme { id: buttonTheme; darkMode: applyButtonPopupSettingDrawer.darkMode }
    readonly property color normalBg: buttonTheme.accent
    readonly property color hoverBg: buttonTheme.accentHover
    readonly property color textColor: "#FFFFFF"
    width: 78; height: 40
    radius: height/2
    anchors.right: parent.right
    anchors.rightMargin: 30
    color: normalBg
    border.width: 1
    border.color: buttonTheme.lineStrong
    Layout.alignment: Qt.AlignVCenter

    signal clicked()

    Text { text: "Save"; anchors.centerIn: parent; color: parent.textColor; font.pixelSize: 15; font.bold: true }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true

        onClicked: {
            applyButtonPopupSettingDrawer.clicked()
        }

        // Hover effect
        onEntered: applyButtonPopupSettingDrawer.color = applyButtonPopupSettingDrawer.hoverBg
        onExited:  applyButtonPopupSettingDrawer.color = applyButtonPopupSettingDrawer.normalBg
    }
}
