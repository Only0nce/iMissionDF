// AddNewDevice.qml (ตัวหัว Device List + ปุ่ม Add)
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12
import QtQuick.Extras 1.4
import QtGraphicalEffects 1.0
import "."

Item {
    id: newRegisterDevice
    width: 1980
    height: 100

    property int iconSize: 35
    property int buttonSize: 50
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

        width: 900
        height: 600
        x: (parent ? (parent.width  - width)  / 2 : 0)
        y: (parent ? (parent.height - height) / 2 : 0)

        background: Rectangle {
            anchors.fill: parent
            radius: 8
            color: "transparent"
        }

        RegisterNewDevice {
             anchors.fill: parent

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
            color: "#F9FAFB"
            font.pixelSize: 26
            font.bold: true
            Layout.alignment: Qt.AlignVCenter
        }

        Item { Layout.fillWidth: true }

        TextField {
            id: searchField
            placeholderText: qsTr("Search name / IP / URI")
            Layout.preferredWidth: 260
            Layout.alignment: Qt.AlignVCenter
            height: buttonSize
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            color: "#E5E7EB"
            background: Rectangle {
                radius: 4
                color: "#111827"
                border.color: "#4B5563"
                border.width: 1
            }

            // ⬇⬇⬇ เพิ่มตรงนี้
            onTextChanged: {
                newRegisterDevice.searchTextChanged(text)
            }
        }


        Button {
            id: clearButton
            text: qsTr("Clear")
            Layout.alignment: Qt.AlignVCenter
            height: buttonSize
            font.pixelSize: 14
            background: Rectangle {
                radius: 4
                color: "transparent"
                border.color: "#D1D5DB"
                border.width: 1
            }
            onClicked: searchField.text = ""
        }

        // ========== ปุ่ม Add Device ==========
        ToolButton {
            id: btnAddDevice

            implicitWidth: buttonSize
            implicitHeight: buttonSize
            Layout.preferredWidth: buttonSize
            Layout.preferredHeight: buttonSize

            background: Rectangle {
                anchors.fill: parent
                radius: 6
                color: "transparent"
                border.color: "#4B5563"
                border.width: 1
            }

            contentItem: Image {
                anchors.centerIn: parent
                width: newRegisterDevice.iconSize
                height: newRegisterDevice.iconSize
                source: iconSrc("addDevice")
                fillMode: Image.PreserveAspectFit
                smooth: true
                sourceSize.width: newRegisterDevice.iconSize
                sourceSize.height: newRegisterDevice.iconSize
            }

            onClicked: {
                console.log("Add Device clicked")
                registerPopup.open()
            }
        }
    }
}
