import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3

Drawer {
    id: settingsDrawer
    width: 300
    height: parent.height
    edge: Qt.LeftEdge
    modal: true
    interactive: true
    visible: false

    background: Rectangle {
        color: "#202020"
    }

    Item {
        anchors.fill: parent
        anchors.margins: 20

        ColumnLayout {
            anchors.fill: parent
            spacing: 20

            Label {
                text: "Settings"
                font.pixelSize: 20
                font.bold: true
                color: "#ffffff"
            }

            TextField {
                id: serverUsername
                placeholderText: "Username"
                Layout.fillWidth: true
                inputMethodHints: Qt.ImhDigitsOnly
            }

            TextField {
                id: serverIpField
                placeholderText: "Server IP"
                Layout.fillWidth: true
                inputMethodHints: Qt.ImhDigitsOnly
            }

            ComboBox {
                Layout.fillWidth: true
                model: ["Option 1", "Option 2", "Option 3"]
            }

            Button {
                text: "Save"
                Layout.fillWidth: true
                background: Rectangle {
                    color: "#169976"
                    radius: 4
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
