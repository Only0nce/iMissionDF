// AddNewDevice.qml (ตัวหัว Device List + ปุ่ม Add)
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.12
import QtQuick.Extras 1.4
import QtGraphicalEffects 1.0
import "."
import "../ui"

Item {
    id: newRegisterDevice
    width: parent ? parent.width : 1980
    height: 100

    property int iconSize: 35
    property int buttonSize: 50
    property bool isDarkTheme: Material.theme === Material.Dark

    Theme { id: hmiTheme; darkMode: newRegisterDevice.isDarkTheme }
    signal searchTextChanged(string text)
    function iconSrc(name) {
        if (name === "addDevice")
            return "qrc:/iRecordManage/images/addDevice.png"
        return ""
    }

    // ===================== POPUP =====================
    Popup {
        id: registerPopup
        modal: true
        focus: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        width: Math.min(900, parent.width - 40)
        height: Math.min(600, parent.height - 60)
        x: (parent ? (parent.width  - width)  / 2 : 0)
        y: (parent ? (parent.height - height) / 2 : 0)

        background: Rectangle {
            anchors.fill: parent
            radius: hmiTheme.radiusLg
            color: hmiTheme.panel
            border.width: 1
            border.color: hmiTheme.lineStrong
        }

        RegisterNewDevice {
             anchors.fill: parent
             isDarkTheme: newRegisterDevice.isDarkTheme

             onCancelRequested: registerPopup.close()

             onCreateRequested: {
                 // สร้าง payload สำหรับส่งไป C++ / WebSocket
                 var payload = {
                     menuID:        "RegisterDevice",
                     name:          deviceName,
                     sid:           sid,
                     payload_size:  payloadSize,
                     terminal_type: terminalType,
                     ip:            ipAddress,
                     uri:           uri,
                     freq:          frequency,
                     group:         group,
                     visible:       visible,
                     ambient:       ambient,
                     last_access:   "",
                     chunk:         chunk
                 }

                 var json = JSON.stringify(payload)
                 console.log("Create device payload:", json)
                 if (typeof qmlCommand === "function") {
                     qmlCommand(json)
                 } else if (typeof window !== "undefined"
                            && typeof window.qmlCommand === "function") {
                     window.qmlCommand(json)
                 } else {
                     console.warn("No qmlCommand() found, payload:", json)
                 }

                 registerPopup.close()
             }
         }
    }

    // ===================== HEADER ROW =====================
    RowLayout {
        id: headerRow
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: qsTr("Device List")
            color: hmiTheme.text
            font.pixelSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignVCenter
        }

        Item { Layout.fillWidth: true }

        HmiTextField {
            id: searchField
            darkMode: newRegisterDevice.isDarkTheme
            placeholderText: qsTr("Search name / IP / URI")
            Layout.preferredWidth: 300
            Layout.preferredHeight: 42
            Layout.alignment: Qt.AlignVCenter
            fontPixelSize: 14
            horizontalAlignment: Text.AlignHCenter

            onTextChanged: {
                newRegisterDevice.searchTextChanged(text)
            }
        }


        HmiButton {
            id: clearButton
            darkMode: newRegisterDevice.isDarkTheme
            text: qsTr("CLEAR")
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: 42
            fontPixelSize: 13
            onClicked: searchField.text = ""
        }

        HmiButton {
            id: btnAddDevice
            darkMode: newRegisterDevice.isDarkTheme
            tone: "primary"
            text: qsTr("+ ADD DEVICE")
            Layout.preferredHeight: 42
            Layout.preferredWidth: 132
            fontPixelSize: 13

            onClicked: {
                console.log("Add Device clicked")
                registerPopup.open()
            }
        }
    }
}
