// EditDeviceForm.qml
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.12
import "../ui"

Item {
    id: root
    width: parent ? parent.width : 1200
    height: parent ? parent.height : 600
    property bool isDarkTheme: Material.theme === Material.Dark

    Theme { id: hmiTheme; darkMode: root.isDarkTheme }

    signal cancelRequested()
    signal deleteRequested()
    signal saveRequested(
        string deviceName,
        string sid,
        string payloadSize,
        string terminalType,
        string ipAddress,
        string uri,
        string frequency,
        string group,
        string visible,
        string ambient,
        string lastAccess,
        string chunk
    )

    function setFromDevice(obj) {
        txtDeviceName.text = obj.name           || "";
        txtSid.text        = obj.sid           !== undefined ? String(obj.sid) : "";
        txtPayload.text    = obj.payload_size  || "";
        txtTerminal.text   = obj.terminal_type || "";
        txtIp.text         = obj.ip            || "";
        txtUri.text        = obj.uri           || "";
        txtFreq.text       = obj.freq          || "";
        txtGroup.text      = obj.group         || "";
        txtVisible.text    = obj.visible       || "";
        txtAmbient.text    = obj.ambient       || "";
        txtLastAccess.text = obj.last_access   || "";
        txtChunk.text      = obj.chunk         || "";
    }

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: hmiTheme.panel
        border.color: hmiTheme.lineStrong
        border.width: 1

        // ================= HEADER =================
        Rectangle {
            id: headerBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 52
            color: hmiTheme.panel

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                Label {
                    text: qsTr("Edit Device")
                    color: hmiTheme.text
                    font.pixelSize: 20
                    font.bold: true
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                ToolButton {
                    text: "✕"
                    onClicked: root.cancelRequested()
                    background: Rectangle { radius: 12; color: "transparent" }
                }
            }
        }

        // ============ FORM แบบกริด 2 ช่อง/แถว =============
        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: headerBar.bottom
            anchors.bottom: footer.top
            anchors.margins: 24
            anchors.topMargin: 16
            spacing: 16

            GridLayout {
                id: formGrid
                columns: 4               // label, field, label, field
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                // --- helper ขนาด label ให้เท่ากัน ---
                property int labelWidth: 140

                // ---------- แถว 1 : Device Name / SID ----------
                Label {
                    text: "Device Name:"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtDeviceName
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }

                Label {
                    text: "SID:"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtSid
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }

                // ---------- แถว 2 : Payload Size / Terminal Type ----------
                Label {
                    text: "Payload Size:"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtPayload
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }

                Label {
                    text: "Terminal Type:"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtTerminal
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }

                // ---------- แถว 3 : IP Address / URI ----------
                Label {
                    text: "IP Address:"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtIp
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }

                Label {
                    text: "URI:"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtUri
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }

                // ---------- แถว 4 : Frequency / Group ----------
                Label {
                    text: "Frequency (MHz):"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtFreq
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }

                Label {
                    text: "Group:"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtGroup
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }

                // ---------- แถว 5 : Visible / Ambient ----------
                Label {
                    text: "Visible:"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtVisible
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }

                Label {
                    text: "Ambient:"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtAmbient
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }

                // ---------- แถว 6 : Last Access / Chunk ----------
                Label {
                    text: "Last Access:"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtLastAccess
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }

                Label {
                    text: "Chunk:"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtChunk
                    Layout.fillWidth: true
                    height: 32
                    color: hmiTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: hmiTheme.panel
                        border.color: hmiTheme.line
                        border.width: 1
                    }
                }
            }
        }

        // ================= FOOTER ปุ่ม =================
        Rectangle {
            id: footer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 64
            color: hmiTheme.panel

            RowLayout {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 16
                spacing: 8

                HmiButton {
                    id: btnCancel
                    darkMode: root.isDarkTheme
                    text: qsTr("Cancel")
                    fontPixelSize: 14
                    onClicked: root.cancelRequested()
                }

                HmiButton {
                    id: btnDelete
                    darkMode: root.isDarkTheme
                    tone: "danger"
                    text: qsTr("Delete")
                    fontPixelSize: 14
                    onClicked: root.deleteRequested()
                }

                HmiButton {
                    id: btnSave
                    darkMode: root.isDarkTheme
                    tone: "primary"
                    text: qsTr("Save Changes")
                    fontPixelSize: 14
                    onClicked: root.saveRequested(
                                  txtDeviceName.text,
                                  txtSid.text,
                                  txtPayload.text,
                                  txtTerminal.text,
                                  txtIp.text,
                                  txtUri.text,
                                  txtFreq.text,
                                  txtGroup.text,
                                  txtVisible.text,
                                  txtAmbient.text,
                                  txtLastAccess.text,
                                  txtChunk.text
                              )
                }
            }
        }
    }
}

/*##^##
Designer {
    D{i:0;formeditorZoom:0.66}
}
##^##*/
