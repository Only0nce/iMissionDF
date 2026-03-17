// RegisterNewDevice.qml
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12

Item {
    id: root
    width: 900
    height: 600

    // ความกว้างช่องกรอกคงที่ทั้งซ้าย/ขวา
    property int fieldWidth: 230

    signal cancelRequested()
    signal createRequested(string deviceName,
                           string sid,
                           string payloadSize,
                           string terminalType,
                           string ipAddress,
                           string uri,
                           string frequency,
                           string group,
                           string visible,
                           string ambient,
                           string chunk)

    Rectangle {
        id: panel
        anchors.fill: parent
        radius: 8
        color: "#020617"
        border.color: "#111827"
        border.width: 1

        // ---------- header ----------
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
                    text: qsTr("Add New Device")
                    color: "#F9FAFB"
                    font.pixelSize: 20
                    font.bold: true
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                ToolButton {
                    id: closeButton
                    Layout.alignment: Qt.AlignVCenter
                    text: "✕"
                    font.pixelSize: 16
                    background: Rectangle {
                        radius: 12
                        color: "transparent"
                    }
                    onClicked: root.cancelRequested()
                }
            }
        }

        // ---------- เนื้อหาฟอร์ม 2 คอลัมน์ ----------
        GridLayout {
            id: formLayout
            columns: 4                           // labelL, fieldL, labelR, fieldR
            columnSpacing: 32
            rowSpacing: 20

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: headerBar.bottom
            anchors.bottom: footer.top
            anchors.margins: 32

            // ===== row 0: Device Name / SID =====
            Label {
                text: qsTr("Device Name:")
                color: "#E5E7EB"
                font.pixelSize: 14
                Layout.row: 0; Layout.column: 0
            }
            // ชุดฝั่งซ้าย
            TextField {            // ✅ DeviceName: ตัวอักษรเป็นหลัก
                id: txtDeviceName
                Layout.row: 0; Layout.column: 1
                Layout.preferredWidth: root.fieldWidth
                Layout.fillWidth: false
                height: 32
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter

                // keyboard แบบตัวอักษร / ปิดเดาคำ
                inputMethodHints: Qt.ImhNoPredictiveText

                background: Rectangle {
                    radius: 4
                    color: "#020617"
                    border.color: "#1F2937"
                    border.width: 1
                }
            }

            Label {
                text: qsTr("SID:")
                color: "#E5E7EB"
                font.pixelSize: 14
                Layout.row: 0; Layout.column: 2
            }
            TextField {            // ✅ SID: ตัวเลขอย่างเดียว
                id: txtSid
                Layout.row: 0; Layout.column: 3
                Layout.preferredWidth: root.fieldWidth
                Layout.fillWidth: false
                height: 32
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter

                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhPreferNumbers | Qt.ImhNoPredictiveText

                background: Rectangle {
                    radius: 4
                    color: "#020617"
                    border.color: "#1F2937"
                    border.width: 1
                }
            }

            // ===== row 1: Payload Size / Terminal Type =====
            Label {
                text: qsTr("Payload Size:")
                color: "#E5E7EB"
                font.pixelSize: 14
                Layout.row: 1; Layout.column: 0
            }

            TextField {            // ✅ Payload: ตัวเลขอย่างเดียว
                id: txtPayload
                Layout.row: 1; Layout.column: 1
                Layout.preferredWidth: root.fieldWidth
                Layout.fillWidth: false
                height: 32
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter

                // ตัวเลขล้วน
                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhPreferNumbers | Qt.ImhNoPredictiveText

                background: Rectangle {
                    radius: 4
                    color: "#020617"
                    border.color: "#1F2937"
                    border.width: 1
                }
            }

            Label {
                text: qsTr("Terminal Type:")
                color: "#E5E7EB"
                font.pixelSize: 14
                Layout.row: 1; Layout.column: 2
            }

            TextField {            // ✅ TerminalType: ตัวเลขอย่างเดียว
                id: txtTerminal
                Layout.row: 1; Layout.column: 3
                Layout.preferredWidth: root.fieldWidth
                Layout.fillWidth: false
                height: 32
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter

                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhPreferNumbers | Qt.ImhNoPredictiveText

                background: Rectangle {
                    radius: 4
                    color: "#020617"
                    border.color: "#1F2937"
                    border.width: 1
                }
            }

            // ===== row 2: IP Address / URI =====
            Label {
                text: qsTr("IP Address:")
                color: "#E5E7EB"
                font.pixelSize: 14
                Layout.row: 2; Layout.column: 0
            }
            TextField {            // ✅ IP: 1–3 ตัวต่อ segment, ไม่ต้องกรอกครบ 3 ตัว
                id: txtIp
                Layout.row: 2; Layout.column: 1
                Layout.preferredWidth: root.fieldWidth
                Layout.fillWidth: false
                height: 32
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter

                // ให้ keyboard เป็นตัวเลขล้วน
                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhNoPredictiveText

                // ไม่ใช้ inputMask แล้ว
                // inputMask: "000.000.000.000;_"

                // อนุญาตรูปแบบ:
                //  "1"
                //  "192.168"
                //  "192.168.10"
                //  "192.168.10.32"
                validator: RegExpValidator {
                    regExp: /^(\d{1,3}(\.\d{1,3}){0,3})?$/
                }

                placeholderText: "192.168.10.32"

                background: Rectangle {
                    radius: 4
                    color: "#020617"
                    border.color: "#1F2937"
                    border.width: 1
                }
            }


            Label {
                text: qsTr("URI:")
                color: "#E5E7EB"
                font.pixelSize: 14
                Layout.row: 2; Layout.column: 2
            }
            TextField {            // ✅ URI: ตัวหนังสือเป็นหลัก
                id: txtUri
                Layout.row: 2; Layout.column: 3
                Layout.preferredWidth: root.fieldWidth
                Layout.fillWidth: false
                height: 32
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter

                // keyboard ตัวอักษร / ปิดเดาคำ
                inputMethodHints: Qt.ImhNoPredictiveText

                background: Rectangle {
                    radius: 4
                    color: "#020617"
                    border.color: "#1F2937"
                    border.width: 1
                }
            }

            // ===== row 3: Frequency / Group =====
            Label {
                text: qsTr("Frequency (MHz):")
                color: "#E5E7EB"
                font.pixelSize: 14
                Layout.row: 3; Layout.column: 0
            }

            TextField {            // ✅ Frequency: ตัวเลข + จุด (เช่น 455.236)
                                   // หน่วย "MHz" แนะนำไปใส่ Label ข้างๆ
                id: txtFreq
                Layout.row: 3; Layout.column: 1
                Layout.preferredWidth: root.fieldWidth
                Layout.fillWidth: false
                height: 32
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter

                // keyboard แบบตัวเลข + จุด
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                  | Qt.ImhPreferNumbers
                                  | Qt.ImhNoPredictiveText

                // อนุญาต 123 หรือ 123.456 (ไม่เกิน 3 หลักจุด3)
                validator: RegExpValidator { regExp: /^([0-9]{1,3}(\.[0-9]{1,3})?)$/ }

                placeholderText: "MHz"   // เอาไว้เตือนว่าค่านี้คือ MHz

                background: Rectangle {
                    radius: 4
                    color: "#020617"
                    border.color: "#1F2937"
                    border.width: 1
                }
            }

            Label {
                text: qsTr("Group:")
                color: "#E5E7EB"
                font.pixelSize: 14
                Layout.row: 3; Layout.column: 2
            }

            TextField {            // ✅ Group: ตัวเลขอย่างเดียว
                id: txtGroup
                Layout.row: 3; Layout.column: 3
                Layout.preferredWidth: root.fieldWidth
                Layout.fillWidth: false
                height: 32
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter

                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhPreferNumbers | Qt.ImhNoPredictiveText

                background: Rectangle {
                    radius: 4
                    color: "#020617"
                    border.color: "#1F2937"
                    border.width: 1
                }
            }
            // ===== row 4: Visible / Ambient =====
            Label {
                text: qsTr("Visible:")
                color: "#E5E7EB"
                font.pixelSize: 14
                Layout.row: 4; Layout.column: 0
            }
            TextField {            // ✅ Visible: ตัวเลขอย่างเดียว
                id: txtVisible
                Layout.row: 4; Layout.column: 1
                Layout.preferredWidth: root.fieldWidth
                Layout.fillWidth: false
                height: 32
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter

                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhPreferNumbers | Qt.ImhNoPredictiveText

                background: Rectangle {
                    radius: 4
                    color: "#020617"
                    border.color: "#1F2937"
                    border.width: 1
                }
            }

            Label {
                text: qsTr("Ambient:")
                color: "#E5E7EB"
                font.pixelSize: 14
                Layout.row: 4; Layout.column: 2
            }
            TextField {            // ✅ Ambient: ตัวเลขอย่างเดียว (ถ้าอยากให้เป็น temp ก็ยังเป็นตัวเลขอยู่ดี)
                id: txtAmbient
                Layout.row: 4; Layout.column: 3
                Layout.preferredWidth: root.fieldWidth
                Layout.fillWidth: false
                height: 32
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter

                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhPreferNumbers | Qt.ImhNoPredictiveText

                background: Rectangle {
                    radius: 4
                    color: "#020617"
                    border.color: "#1F2937"
                    border.width: 1
                }
            }


            // ===== row 5: Chunk (ซ้ายอย่างเดียว) =====
            Label {
                text: qsTr("Chunk:")
                color: "#E5E7EB"
                font.pixelSize: 14
                Layout.row: 5; Layout.column: 0
            }
            TextField {            // ✅ Chunk: ตัวเลขอย่างเดียว
                id: txtChunk
                Layout.row: 5; Layout.column: 1
                Layout.preferredWidth: root.fieldWidth
                Layout.fillWidth: false
                height: 32
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter

                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhPreferNumbers | Qt.ImhNoPredictiveText

                background: Rectangle {
                    radius: 4
                    color: "#020617"
                    border.color: "#1F2937"
                    border.width: 1
                }
            }

        }

        // ---------- footer (ปุ่ม) ----------
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
                    background: Rectangle {
                        radius: 4
                        color: "#4B5563"
                    }
                    contentItem: Label {
                        text: btnCancel.text
                        color: "#E5E7EB"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 14
                    }
                }

                Button {
                    id: btnCreate
                    text: qsTr("Create Device")

                    onClicked: {
                        // ส่งข้อมูลออกไปผ่าน signal แทน
                        root.createRequested(
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
                            txtChunk.text
                        )
                    }

                    background: Rectangle {
                        radius: 4
                        color: "#2563EB"
                    }
                    contentItem: Label {
                        text: btnCreate.text
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 14
                        font.bold: true
                    }
                }

            }
        }
    }
}

/*##^##
Designer {
    D{i:0;formeditorZoom:0.75}
}
##^##*/
