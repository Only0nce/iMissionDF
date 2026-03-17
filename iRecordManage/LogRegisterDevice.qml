// LogRegisterDevice.qml
import QtQuick 2.0
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtWebSockets 1.0
import QtQuick.Extras 1.4
import QtQuick.Controls 1.4 as C1
import QtQuick.VirtualKeyboard 2.15
import QtGraphicalEffects 1.0
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.15

Item {
    id: logregisterDevice
    width: 1880
    height: 1000

    // ---------- utils เดิม ----------
    function i(v) {
        return (v === undefined || v === null) ? "" : String(v);
    }
    function toInt(v, def) {
        var n = parseInt(v, 10);
        return isNaN(n) ? (def === undefined ? 0 : def) : n;
    }
    function blurAll() {
        focusCatcher.forceActiveFocus();
        if (Qt.inputMethod.visible) Qt.inputMethod.hide();
    }
    Component.onCompleted: Qt.callLater(blurAll)
    onVisibleChanged: {
        if (visible) Qt.callLater(blurAll);
        else if (Qt.inputMethod.visible) Qt.inputMethod.hide();
    }
    FocusScope {
        id: focusCatcher
        anchors.fill: parent
        focus: true
    }

    // ---------- 💡 เพิ่มตัวแปรความสูงคีย์บอร์ด (คูณ DPI) ----------
    readonly property int keyboardH: Qt.inputMethod.visible
                                     ? Math.round(Qt.inputMethod.keyboardRectangle.height / (Screen.devicePixelRatio || 1))
                                     : 0

    Rectangle {
        id: rectangle1
        anchors.fill: parent
        color: "#1f2428"

        // --------- ฟอร์ม component (ของเดิม) ----------
        Component {
            id: fieldCol
            ColumnLayout {
                property alias labelText: lbl.text
                property alias textField: tf
                Layout.fillWidth: true
                spacing: 8

                Label {
                    id: lbl
                    color: "#ffffff"
                    font.pixelSize: 20
                    text: "Label"
                }
                TextField {
                    id: tf
                    focus: false
                    placeholderText: qsTr("Enter Value")
                    activeFocusOnPress: true
                    horizontalAlignment: Text.AlignHCenter
                    Layout.preferredWidth: 240
                    Layout.minimumWidth: 180
                    Layout.maximumWidth: 280
                }
            }
        }

        // =========== ตารางด้านล่าง ===========
        Rectangle {
            id: rectangleDeviceList
            color: "#ffffff"
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 290

            // 💡 ยกตารางขึ้นตามความสูงคีย์บอร์ด + margin 24
            anchors.bottomMargin: 24 + keyboardH
            Behavior on anchors.bottomMargin { NumberAnimation { duration: 160; easing.type: Easing.InOutQuad } }

            x: 42
            width: 1774
            clip: true

            Flickable {
                id: hFlick
                anchors.fill: parent
                clip: true
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds

                readonly property int totalW:
                      colSelected.width
                    + colActions.width
                    + colId.width
                    + colSid.width
                    + colPayload.width
                    + colTerm.width
                    + colName.width
                    + colIp.width
                    + colUri.width
                    + colFreq.width
                    + colAmb.width
                    + colGroup.width
                    + colVisible.width
                    + colAccess.width
                    + colPath.width
                    + colChunk.width
                    + colUpdated.width

                contentWidth: totalW
                contentHeight: height

                Item {
                    id: iTemRegisterDviceList
                    width: hFlick.totalW
                    height: hFlick.height

                    C1.TableView {
                        id: registerDviceList
                        anchors.fill: parent
                        clip: true
                        model: listoFDevice     // ← ใช้ตรง ๆ ตามที่สั่ง
                        focus: false

                        // ---------- helpers ----------
                        function inRange(row) { return row >= 0 && row < (model ? model.count : 0) }
                        function safeSetRole(row, roleName, value) {
                            if (inRange(row)) listoFDevice.setProperty(row, roleName, value)
                        }
                        function roleValue(row, roleName) {
                            if (!inRange(row)) return ""
                            var r = model.get(row)
                            var v = (r && r[roleName] !== undefined) ? r[roleName] : ""
                            return (v === null || v === undefined) ? "" : String(v)
                        }
                        function bringRowIntoView(row) {
                            if (inRange(row)) positionViewAtRow(row, C1.TableView.Center)
                        }

                        property int  editingRow: -1
                        property var  editCache: ({})
                        property bool showActionColumn: false

                        onModelChanged: { if (!inRange(editingRow)) editingRow = -1 }

                        function beginEdit(row) {
                            if (editingRow !== row) {
                                var r = listoFDevice.get(row) || {}
                                editCache[row] = {}
                                for (var k in r) editCache[row][k] = r[k]
                            }
                            editingRow = row
                            showActionColumn = true
                            listoFDevice.setProperty(row, "selected", true)
                            bringRowIntoView(row)      // 💡 เลื่อนเข้ากลางทันทีตอนเริ่มแก้
                        }
                        function cancelEdit(row) {
                            if (editCache[row]) {
                                var orig = editCache[row]
                                for (var k in orig) if (k !== "selected") listoFDevice.setProperty(row, k, orig[k])
                                delete editCache[row]
                            }
                            if (inRange(row)) listoFDevice.setProperty(row, "selected", false)
                            editingRow = -1
                            showActionColumn = false
                            if (registerDviceList.forceLayout) registerDviceList.forceLayout()
                        }
                        function applyEdit(row) {
                            if (!inRange(row)) return
                            var r = listoFDevice.get(row) || {}
                            var payload = {
                                menuID: "updateDevice",
                                ambient: r.ambient||"", chunk: r.chunk||"", file_path: r.file_path||"",
                                freq: r.freq||"", group: r.group||"", idDevice: r.id||"", ip: r.ip||"",
                                last_access: r.last_access||"", name: r.name||"", payload_size: r.payload_size||"",
                                sid: r.sid||"", terminal_type: r.terminal_type||"", updated_at: r.updated_at||"",
                                uri: r.uri||"", visible: r.visible||""
                            }
                            // ส่งออก (คุณมี hook sendMessage อยู่แล้ว)
                            if (sendMessage) sendMessage(JSON.stringify(payload))

                            if (editCache[row]) delete editCache[row]
                            listoFDevice.setProperty(row, "selected", false)
                            editingRow = -1
                            showActionColumn = false
                            if (registerDviceList.forceLayout) registerDviceList.forceLayout()
                        }
                        function removeRow(row) {
                            if (editCache[row]) delete editCache[row]
                            if (inRange(row)) listoFDevice.remove(row)
                            if (editingRow === row) editingRow = -1
                        }
                        function onEditorFocused(row) {
                            Qt.inputMethod.show()
                            bringRowIntoView(row)      // 💡 ทุกครั้งที่ TextField โฟกัส ให้เลื่อนเข้ากลาง
                        }

                        // ---------- style ----------
                        property int rowH: 56
                        property int headerH: 44
                        property int cellPt: 16
                        property int headerPt: 16
                        property int padL: 12

                        rowDelegate: Rectangle {
                            height: registerDviceList.rowH
                            color: styleData.alternate ? "#f7f7f7" : "#ffffff"
                        }
                        headerDelegate: Rectangle {
                            height: registerDviceList.headerH
                            color: "#f0f2f5"
                            border.color: "#dcdcdc"
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: registerDviceList.padL
                                anchors.rightMargin: 8
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                text: styleData.value
                                font.pointSize: registerDviceList.headerPt
                                color: "#333333"
                            }
                        }
                        itemDelegate: Item {
                            anchors.fill: parent
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: registerDviceList.padL
                                anchors.rightMargin: 8
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                text: styleData.value
                                font.pointSize: registerDviceList.cellPt
                                color: "#111111"
                            }
                        }

                        // ---------- Columns ----------
                        C1.TableViewColumn { id: colSelected; role: "selected"; title: "Sel"; width: 56;
                            delegate: Item {
                                anchors.fill: parent
                                C1.CheckBox {
                                    id: boxCheck
                                    anchors.centerIn: parent
                                    scale: 1.5
                                    onClicked: {
                                        var newVal = !Boolean(styleData.value)
                                        listoFDevice.setProperty(styleData.row, "selected", newVal)
                                        if (newVal) registerDviceList.beginEdit(styleData.row)
                                        else        registerDviceList.cancelEdit(styleData.row)
                                    }
                                }
                                Binding { target: boxCheck; property: "checked"; value: !!styleData.value }
                            }
                        }

                        C1.TableViewColumn { id: colActions; title: "Action"; width: 240;
                            visible: registerDviceList.showActionColumn
                            delegate: Item {
                                anchors.fill: parent
                                visible: registerDviceList.editingRow === styleData.row
                                Row {
                                    anchors.centerIn: parent
                                    spacing: 8
                                    C1.Button { text: "Apply";  width: 60; height: 30;
                                        onClicked: registerDviceList.applyEdit(styleData.row) }
                                    C1.Button { text: "Cancel"; width: 70; height: 30;
                                        onClicked: registerDviceList.cancelEdit(styleData.row) }
                                    C1.Button { text: "Remove"; width: 75; height: 30;
                                        onClicked: registerDviceList.removeRow(styleData.row) }
                                }
                            }
                        }

                        C1.TableViewColumn { id: colId;  role: "idDevice"; title: "ID";  width: 80 }
                        C1.TableViewColumn { id: colSid; role: "sid";      title: "SID"; width: 80 }

                        // Payload size
                        C1.TableViewColumn {
                            id: colPayload; role: "payload_size"; title: "Payload size"; width: 180
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                           verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                                           text: styleData.value; font.pointSize: registerDviceList.cellPt; color: "#111111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                        text: registerDviceList.roleValue(styleData.row, "payload_size")
                                        validator: IntValidator { bottom: 0 }
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "payload_size", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // Terminal type
                        C1.TableViewColumn {
                            id: colTerm; role: "terminal_type"; title: "Terminal type"; width: 180
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                           verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; font.pointSize: registerDviceList.cellPt; color: "#111111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                        text: registerDviceList.roleValue(styleData.row, "terminal_type")
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "terminal_type", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // Name
                        C1.TableViewColumn {
                            id: colName; role: "name"; title: "Name"; width: 280
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: 12; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; color: "#111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: 12
                                        text: registerDviceList.roleValue(styleData.row, "name")
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "name", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // IP
                        C1.TableViewColumn {
                            id: colIp; role: "ip"; title: "IP"; width: 200
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: 12; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; color: "#111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: 12
                                        text: registerDviceList.roleValue(styleData.row, "ip")
                                        validator: RegExpValidator { regExp: /^[0-9]{1,3}(\.[0-9]{1,3}){3}$/ }
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "ip", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // URI
                        C1.TableViewColumn {
                            id: colUri; role: "uri"; title: "URI"; width: 200
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                           verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; font.pointSize: registerDviceList.cellPt; color: "#111111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                        text: registerDviceList.roleValue(styleData.row, "uri")
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "uri", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // Frequency
                        C1.TableViewColumn {
                            id: colFreq; role: "freq"; title: "Frequency"; width: 150
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                           verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; font.pointSize: registerDviceList.cellPt; color: "#111111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                        text: registerDviceList.roleValue(styleData.row, "freq")
                                        validator: DoubleValidator { bottom: 0 }
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "freq", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // Ambient
                        C1.TableViewColumn {
                            id: colAmb; role: "ambient"; title: "Ambient"; width: 150
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                           verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; font.pointSize: registerDviceList.cellPt; color: "#111111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                        text: registerDviceList.roleValue(styleData.row, "ambient")
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "ambient", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // Group
                        C1.TableViewColumn {
                            id: colGroup; role: "group"; title: "Group"; width: 100
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                           verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; font.pointSize: registerDviceList.cellPt; color: "#111111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                        text: registerDviceList.roleValue(styleData.row, "group")
                                        validator: IntValidator { bottom: 0 }
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "group", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // Visible
                        C1.TableViewColumn {
                            id: colVisible; role: "visible"; title: "Visible"; width: 100
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                           verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; font.pointSize: registerDviceList.cellPt; color: "#111111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                        text: registerDviceList.roleValue(styleData.row, "visible")
                                        validator: IntValidator { bottom: 0; top: 1 }
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "visible", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // Last Access
                        C1.TableViewColumn {
                            id: colAccess; role: "last_access"; title: "Last Access"; width: 220
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                           verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; font.pointSize: registerDviceList.cellPt; color: "#111111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                        text: registerDviceList.roleValue(styleData.row, "last_access")
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "last_access", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // Storage path
                        C1.TableViewColumn {
                            id: colPath; role: "file_path"; title: "Storage"; width: 180
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                           verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; font.pointSize: registerDviceList.cellPt; color: "#111111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                        text: registerDviceList.roleValue(styleData.row, "file_path")
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "file_path", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // Chunk
                        C1.TableViewColumn {
                            id: colChunk; role: "chunk"; title: "Chunk"; width: 100
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                           verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; font.pointSize: registerDviceList.cellPt; color: "#111111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                        text: registerDviceList.roleValue(styleData.row, "chunk")
                                        validator: IntValidator { bottom: 0 }
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "chunk", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }

                        // Updated at
                        C1.TableViewColumn {
                            id: colUpdated; role: "updated_at"; title: "Updated at"; width: 600
                            delegate: Item {
                                anchors.fill: parent
                                Loader { anchors.fill: parent; sourceComponent: (registerDviceList.editingRow === styleData.row) ? editField : viewLabel }
                                Component { id: viewLabel
                                    Text { anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                           verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; text: styleData.value; font.pointSize: registerDviceList.cellPt; color: "#111111" }
                                }
                                Component { id: editField
                                    C1.TextField {
                                        anchors.fill: parent; anchors.leftMargin: registerDviceList.padL; anchors.rightMargin: 8
                                        text: registerDviceList.roleValue(styleData.row, "updated_at")
                                        onTextChanged: if (focus) registerDviceList.safeSetRole(styleData.row, "updated_at", text)
                                        onFocusChanged: if (focus) registerDviceList.onEditorFocused(styleData.row)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
