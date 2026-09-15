import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Popup {
    id: root

    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose

    width: 640
    height: 360
    x: parent ? Math.max(10, (parent.width - width) / 2) : 10
    y: parent ? Math.max(10, (parent.height - height) / 3) : 10

    property color backgroundColor: "#101a26"
    property color cardColor: "#132235"
    property color borderColor: "#2d4056"
    property color textColor: "#e9f0f7"
    property color subTextColor: "#9aa8b8"
    property color accentColor: "#00c9a7"
    property color viewerColor: "#2f80ed"

    signal viewerSelected()
    signal adminSelected()
    signal cancelled()

    function requestSelection() {
        open()
    }

    background: Rectangle {
        radius: 18
        color: root.backgroundColor
        border.color: root.borderColor
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Text {
            text: "Network Access"
            color: root.textColor
            font.pixelSize: 26
            font.bold: true
        }

        Text {
            text: "Choose the access level for this Network Settings session"
            color: root.subTextColor
            font.pixelSize: 14
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            Button {
                id: viewerButton
                Layout.fillWidth: true
                Layout.fillHeight: true
                scale: pressed ? 0.975 : 1.0
                Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }

                onClicked: {
                    root.close()
                    root.viewerSelected()
                }

                background: Rectangle {
                    radius: 16
                    color: viewerButton.pressed ? Qt.darker(root.cardColor, 1.12) : root.cardColor
                    border.color: root.viewerColor
                    border.width: 2
                }

                contentItem: Column {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Text {
                        text: "VIEWER"
                        color: root.viewerColor
                        font.pixelSize: 22
                        font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: "Limited network access"
                        color: root.textColor
                        font.pixelSize: 17
                        font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: "Can modify LAN1, LAN2, WiFi and 5G. LAN3 and LAN4 remain visible but read-only."
                        color: root.subTextColor
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                    Item { width: 1; height: 4 }
                    Text {
                        text: "Continue without administrator password"
                        color: root.viewerColor
                        font.pixelSize: 13
                        font.bold: true
                    }
                }
            }

            Button {
                id: adminButton
                Layout.fillWidth: true
                Layout.fillHeight: true
                scale: pressed ? 0.975 : 1.0
                Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }

                onClicked: {
                    root.close()
                    root.adminSelected()
                }

                background: Rectangle {
                    radius: 16
                    color: adminButton.pressed ? Qt.darker(root.cardColor, 1.12) : root.cardColor
                    border.color: root.accentColor
                    border.width: 2
                }

                contentItem: Column {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Text {
                        text: "ADMIN"
                        color: root.accentColor
                        font.pixelSize: 22
                        font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: "Full network access"
                        color: root.textColor
                        font.pixelSize: 17
                        font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: "Can modify LAN1, LAN2, LAN3, LAN4, WiFi and 5G."
                        color: root.subTextColor
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                    Item { width: 1; height: 4 }
                    Text {
                        text: "Administrator password required"
                        color: root.accentColor
                        font.pixelSize: 13
                        font.bold: true
                    }
                }
            }
        }

        Button {
            id: cancelButton
            text: "Cancel"
            Layout.alignment: Qt.AlignRight
            Layout.preferredWidth: 130
            Layout.preferredHeight: 42
            scale: pressed ? 0.96 : 1.0
            Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }

            background: Rectangle {
                radius: 10
                color: cancelButton.pressed ? "#26384b" : "#1a2a3b"
                border.color: root.borderColor
            }

            contentItem: Text {
                text: cancelButton.text
                color: root.textColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.bold: true
            }

            onClicked: {
                root.close()
                root.cancelled()
            }
        }
    }
}
