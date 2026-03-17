// EditDeviceForm.qml
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Item {
    id: root
    width: 1200
    height: 600

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
        color: "#020617"
        border.color: "#111827"
        border.width: 1

        // ================= HEADER =================
        Rectangle {
            id: headerBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 52
            color: "#020617"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                Label {
                    text: qsTr("Edit Device")
                    color: "#F9FAFB"
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
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtDeviceName
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
                        border.width: 1
                    }
                }

                Label {
                    text: "SID:"
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtSid
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
                        border.width: 1
                    }
                }

                // ---------- แถว 2 : Payload Size / Terminal Type ----------
                Label {
                    text: "Payload Size:"
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtPayload
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
                        border.width: 1
                    }
                }

                Label {
                    text: "Terminal Type:"
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtTerminal
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
                        border.width: 1
                    }
                }

                // ---------- แถว 3 : IP Address / URI ----------
                Label {
                    text: "IP Address:"
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtIp
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
                        border.width: 1
                    }
                }

                Label {
                    text: "URI:"
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtUri
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
                        border.width: 1
                    }
                }

                // ---------- แถว 4 : Frequency / Group ----------
                Label {
                    text: "Frequency (MHz):"
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtFreq
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
                        border.width: 1
                    }
                }

                Label {
                    text: "Group:"
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtGroup
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
                        border.width: 1
                    }
                }

                // ---------- แถว 5 : Visible / Ambient ----------
                Label {
                    text: "Visible:"
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtVisible
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
                        border.width: 1
                    }
                }

                Label {
                    text: "Ambient:"
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtAmbient
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
                        border.width: 1
                    }
                }

                // ---------- แถว 6 : Last Access / Chunk ----------
                Label {
                    text: "Last Access:"
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtLastAccess
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
                        border.width: 1
                    }
                }

                Label {
                    text: "Chunk:"
                    color: "#E5E7EB"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                    Layout.preferredWidth: formGrid.labelWidth
                }
                TextField {
                    id: txtChunk
                    Layout.fillWidth: true
                    height: 32
                    color: "#E5E7EB"
                    horizontalAlignment: Text.AlignHCenter
                    background: Rectangle {
                        radius: 4
                        color: "#020617"
                        border.color: "#1F2937"
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
            color: "#020617"

            RowLayout {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 16
                spacing: 8

                Button {
                    id: btnCancel
                    text: qsTr("Cancel")
                    onClicked: root.cancelRequested()
                }

                Button {
                    id: btnDelete
                    text: qsTr("Delete")
                    background: Rectangle { radius: 4; color: "#DC2626" }
                    contentItem: Label {
                        text: btnDelete.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 14
                        font.bold: true
                    }
                    onClicked: root.deleteRequested()
                }

                Button {
                    id: btnSave
                    text: qsTr("Save Changes")
                    background: Rectangle { radius: 4; color: "#2563EB" }
                    contentItem: Label {
                        text: btnSave.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 14
                        font.bold: true
                    }
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
