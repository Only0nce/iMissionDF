// GroupCard.qml — show device list always, inline rename on pencil
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: card
    property string title: ""
    property var    items: []          // array ของ devices
    property bool   selected: false    // สถานะเรือง

    // ===== Inline-rename state =====
    property bool   editing: false
    property string _draftTitle: ""

    // GroupCard -> parent
    signal headerClicked(string title, var items)
    signal editClicked(string title, var items)
    signal addClicked(string title)

    radius: 12
    color: "#0f141a"
    border.width: selected ? 2 : 1
    border.color: selected ? "#4aa3ff" : "#233240"
    implicitWidth: 480
    implicitHeight: header.implicitHeight + contentCol.implicitHeight + 20
    layer.enabled: true
    layer.samples: 4
    layer.smooth: true
    Behavior on border.color { ColorAnimation { duration: 150 } }
    Behavior on border.width { NumberAnimation { duration: 150 } }

    // ===== Rename helpers =====
    function startEdit() {
        _draftTitle = title
        editing = true
        Qt.callLater(function() {
            titleEditor.forceActiveFocus()
            titleEditor.selectAll()
            Qt.inputMethod.show()
        })
    }
    function commitEdit() {
        const trimmed = _draftTitle.trim()
        const changed = (trimmed.length > 0 && trimmed !== title)
        if (changed) {
            title = trimmed
            card.editClicked(card.title, card.items)
        }
        editing = false
        Qt.inputMethod.hide()
    }
    function cancelEdit() {
        editing = false
        _draftTitle = title
        Qt.inputMethod.hide()
    }
    Connections {
        target: Qt.inputMethod
        onVisibleChanged: {
            if (!Qt.inputMethod.visible && card.editing) {
                card.cancelEdit()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // ===== Header =====
        Rectangle {
            id: header
            height: 44
            Layout.fillWidth: true
            implicitHeight: 44
            radius: 8
            color: card.selected ? "#203142" : "#1a2633"
            border.width: 1
            border.color: card.selected ? "#4aa3ff" : "#2a6fb0"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                anchors.topMargin: 8
                spacing: 10

                // ===== โซนซ้าย: ชื่อ + จำนวน =====
                Item {
                    id: mainClickable
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    height: parent.height

                    RowLayout {
                        anchors.fill: parent
                        spacing: 8

                        // โหมดแสดงผล
                        Text {
                            id: titleLabel
                            visible: !card.editing
                            text: card.title
                            color: "#e9f2f9"
                            font.pixelSize: 16
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item {
                            visible: card.editing
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            height: 28
                            onVisibleChanged: if (visible) Qt.callLater(() => { titleEditor.forceActiveFocus(); titleEditor.selectAll(); })

                            TextField {
                                id: titleEditor
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.right: parent.right
                                text: card._draftTitle
                                color: "#e9f2f9"
                                onTextChanged: card._draftTitle = text
                                selectByMouse: true
                                focus: true
                                font.pixelSize: 16
                                font.bold: true
                                horizontalAlignment: Text.AlignLeft
                                padding: 6
                                background: Rectangle {
                                    radius: 6
                                    color: "#213040"
                                    border.width: 1
                                    border.color: "#3b6fa3"
                                }
                                Keys.onReturnPressed: card.commitEdit()
                                Keys.onEnterPressed:  card.commitEdit()
                                Keys.onEscapePressed: card.cancelEdit()
                                onActiveFocusChanged: if (!activeFocus && visible) card.cancelEdit()
                            }
                        }

                        Rectangle {
                            radius: 10
                            color: "#22364a"
                            height: 24
                            width: countText.implicitWidth + 12
                            Layout.alignment: Qt.AlignVCenter
                            Label {
                                id: countText
                                anchors.centerIn: parent
                                text: items ? items.length : 0
                                color: "#cfe6fb"
                                font.pixelSize: 12
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.leftMargin: -9
                        anchors.rightMargin: -5
                        anchors.topMargin: -10
                        anchors.bottomMargin: -9
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        enabled: !card.editing
                        onClicked: {
                            card.selected = !card.selected
                            card.headerClicked(card.title, card.items)
                        }
                    }
                }

                // ===== ปุ่มดินสอ (แก้ไข) =====
                Rectangle {
                    id: editBtn
                    width: 28; height: 28; radius: 6
                    color: mouseEdit.containsMouse ? "#305060" : "#22364a"
                    border.width: 1; border.color: "#2a6fb0"
                    Layout.alignment: Qt.AlignVCenter

                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/iScreenDFqml/images/penicon.png"
                        width: 22; height: 22
                        fillMode: Image.PreserveAspectFit
                    }

                    MouseArea {
                        id: mouseEdit
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        preventStealing: true
                        onClicked: {
                            if (!card.editing) card.startEdit()
                            else card.commitEdit()
                        }
                    }
                }

                // ===== ปุ่มบวก (เพิ่ม) =====
                Rectangle {
                    id: addBtn
                    width: 28; height: 28; radius: 6
                    color: mouseAdd.containsMouse ? "#305060" : "#22364a"
                    border.width: 1; border.color: "#2a6fb0"
                    Layout.alignment: Qt.AlignVCenter

                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/iScreenDFqml/images/gearicon.png"  /*"qrc:/images/addicon.png"*/
                        width: 37; height: 37
                        fillMode: Image.PreserveAspectFit
                    }
                    MouseArea {
                        id: mouseAdd
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        preventStealing: true
                        onClicked: {
                            if (card.editing) card.commitEdit()
                            card.addClicked(card.title)
                        }
                    }
                }
            }
        }

        // ===== เนื้อหากลุ่ม: แสดงตลอด (ไม่ซ้อน/ไม่ต้องเลือกก่อน)
        Column {
            id: contentCol
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                id: deviceRepeater
                model: items || []   // ใช้ array ตรง ๆ ลด recreate

                delegate: Rectangle {
                    radius: 10
                    color: "#141b22"
                    border.width: 1
                    border.color: "#1f2b38"
                    width: parent.width
                    height: devRow.implicitHeight > 0 ? devRow.implicitHeight + 12 : 64

                    RemoteSdrItem {
                        id: devRow
                        anchors.fill: parent
                        anchors.margins: 8

                        // ใช้ modelData
                        deviceName:   modelData.DeviceName
                        deviceIp:     modelData.IPAddress
                        devicePort:   modelData.Port
                        deviceStatus: modelData.status
                        deviceRssi:   0
                        rowIndex:     index
                    }
                }
            }
        }
    }
}
