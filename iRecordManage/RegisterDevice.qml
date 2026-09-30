import QtQuick 2.0
import QtQuick.Controls 2.15 as C2
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtWebSockets 1.0
import QtQuick.Extras 1.4
import QtQuick.Controls 1.4 as C1
import QtQuick.VirtualKeyboard 2.15
import QtQuick.VirtualKeyboard.Styles 2.15
import QtQuick.VirtualKeyboard.Settings 2.15
import QtGraphicalEffects 1.0
import QtQuick.Controls.Styles 1.4
import "../ui"

Item {
    id: registerDevice
    width: parent ? parent.width : 1980
    height: parent ? parent.height : 1080
    property int editRow: -1
    property int pendingDeleteRow: -1
    property string filterText: ""
    property bool isDarkTheme: Material.theme === Material.Dark

    Theme { id: hmiTheme; darkMode: registerDevice.isDarkTheme }
    // ====== Popup ยืนยันการลบ ======
    C2.Popup {
        id: confirmDeletePopup
        modal: true
        focus: true
        closePolicy: C2.Popup.CloseOnEscape | C2.Popup.CloseOnPressOutside
        width: 360
        height: 180
        x: (parent.width  - width)  / 2
        y: (parent.height - height) / 2

        Rectangle {
            anchors.fill: parent
            radius: hmiTheme.radiusMd
            color: hmiTheme.panel
            border.color: hmiTheme.lineStrong
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16

                Text {
                    id: confirmDeleteText
                    Layout.fillWidth: true
                    text: qsTr("Do you want to delete this device?")
                    color: hmiTheme.text
                    font.pixelSize: 16
                    wrapMode: Text.WordWrap
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 8

                    // ปุ่ม No (ไม่ต้องแต่ง background ก็ได้)
                    HmiButton {
                        darkMode: registerDevice.isDarkTheme
                        text: qsTr("Cancel")
                        onClicked: {
                            pendingDeleteRow = -1
                            confirmDeletePopup.close()
                        }
                    }

                    // ปุ่ม Yes ใช้ Controls2 เต็ม ๆ
                    HmiButton {
                        id: btnYesDelete
                        darkMode: registerDevice.isDarkTheme
                        tone: "danger"
                        text: qsTr("Delete")

                        onClicked: {
                            if (pendingDeleteRow >= 0 &&
                                pendingDeleteRow < listoFDevice.count) {

                                var r = listoFDevice.get(pendingDeleteRow)
                                var payload = {
                                    menuID: "deleteDevice",
                                    id:     r.idDevice || "",
                                    sid:    r.sid      || ""
                                }
                                sendOut(payload)
                            }
                            pendingDeleteRow = -1
                            confirmDeletePopup.close()
                        }
                    }
                }
            }
        }
    }


    // ===================== POPUP แก้ไข DEVICE =====================
    C2.Popup {
        id: editPopup
        modal: true
        focus: true
        closePolicy: C2.Popup.CloseOnEscape | C2.Popup.CloseOnPressOutside
        width: Math.min(1200, parent.width - 40)
        height: Math.min(600, parent.height - 80)
        x: (parent.width  - width)  / 2
        y: Math.max(40, (parent.height - height) / 2 - 60)

        EditDeviceLists {
            id: editForm
            anchors.fill: parent
            isDarkTheme: registerDevice.isDarkTheme

            onCancelRequested: editPopup.close()

            onDeleteRequested: {
                if (registerDevice.editRow >= 0 &&
                    registerDevice.editRow < listoFDevice.count) {
                    registerDevice.deleteDevice(registerDevice.editRow)
                }
                editPopup.close()
            }

            onSaveRequested: function(deviceName,
                                      sid,
                                      payloadSize,
                                      terminalType,
                                      ipAddress,
                                      uri,
                                      frequency,
                                      group,
                                      visible,
                                      ambient,
                                      lastAccess,
                                      chunk) {
                if (registerDevice.editRow < 0 ||
                    registerDevice.editRow >= listoFDevice.count) {
                    editPopup.close()
                    return
                }

                var r = listoFDevice.get(registerDevice.editRow)
                var payload = {
                    menuID:        "updateDevice",
                    id:            r.idDevice       || "",
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
                    last_access:   lastAccess,
                    chunk:         chunk,
                    file_path:     r.file_path || "",
                    updated_at:    r.updated_at || ""
                }

                registerDevice.sendOut(payload)
                editPopup.close()
            }
        }
    }

    // ===================== UTILITIES =====================
    function i(v) {
        return (v === undefined || v === null) ? "" : String(v);
    }
    function toInt(v, def) {
        var n = parseInt(v, 10);
        return isNaN(n) ? (def === undefined ? 0 : def) : n;
    }
    function isEditable(item) {
        return item && (item.hasOwnProperty("cursorPosition")
                        || item.hasOwnProperty("inputMethodComposing")
                        || item.hasOwnProperty("echoMode"));
    }
    function isDescendantOf(child, ancestor) {
        var n = child;
        while (n) {
            if (n === ancestor) return true;
            n = n.parent;
        }
        return false;
    }

    // ใช้ rectangleDeviceList แทน registerDviceList เดิม
    property bool tableWantsInset:
        Qt.inputMethod.visible
        && isEditable(activeFocusItem)
        && isDescendantOf(activeFocusItem, rectangleDeviceList)

    function blurAll() {
        focusCatcher.forceActiveFocus();
        if (Qt.inputMethod.visible) Qt.inputMethod.hide();
    }

    function setInt(tf) {
        tf.inputMask = "";
        tf.inputMethodHints = Qt.ImhDigitsOnly | Qt.ImhPreferNumbers | Qt.ImhNoPredictiveText
    }

    function setDouble(tf) {
        tf.inputMask = ""
        tf.inputMethodHints = Qt.ImhFormattedNumbersOnly | Qt.ImhPreferNumbers | Qt.ImhNoPredictiveText
    }

    function setIP(tf) {
        tf.inputMask = "000.000.000.000;_"
        tf.inputMethodHints = Qt.ImhDigitsOnly | Qt.ImhNoPredictiveText
    }

    function setDate(tf) {
        tf.inputMask = "99/99/9999;_"
        tf.inputMethodHints = Qt.ImhDigitsOnly | Qt.ImhNoPredictiveText
    }

    function setText(tf) {
        tf.inputMethodHints = Qt.ImhNoPredictiveText
    }

    function openEditPopup(rowIndex) {
        if (rowIndex < 0 || rowIndex >= listoFDevice.count)
            return;

        editRow = rowIndex;

        var r = listoFDevice.get(rowIndex);

        editForm.setFromDevice(r);   // <-- ใช้ฟังก์ชันในฟอร์ม

        editPopup.open();
    }
    function setFromDevice(obj) {
        txtDeviceName.text = obj.name || "";
        txtSid.text = obj.sid !== undefined ? String(obj.sid) : "";
        txtPayload.text = obj.payload_size || "";
        txtTerminal.text = obj.terminal_type || "";
        txtIp.text = obj.ip || "";
        txtUri.text = obj.uri || "";
        txtFreq.text = obj.freq || "";
        txtGroup.text = obj.group || "";
        txtVisible.text = obj.visible || "";
        txtAmbient.text = obj.ambient || "";
        txtLastAccess.text = obj.last_access || "";
        txtChunk.text = obj.chunk || "";
    }

    function deleteDevice(rowIndex) {
        if (rowIndex < 0 || rowIndex >= listoFDevice.count)
            return;

        pendingDeleteRow = rowIndex;

        // แสดงชื่อ device ในข้อความด้วยก็ได้
        var r = listoFDevice.get(rowIndex);
        var name = r && r.name ? r.name : "";
        confirmDeleteText.text = name !== ""
                ? qsTr("Do you want to delete \"%1\" ?").arg(name)
                : qsTr("Do you want to delete this device?");

        confirmDeletePopup.open();
    }

    function sendOut(msgObj) {
        var s = JSON.stringify(msgObj);
//        console.log("sendOut:", s);
        if (typeof qmlCommand === "function") {
            qmlCommand(s);
        } else if (typeof window !== "undefined" && typeof window.qmlCommand === "function") {
            window.qmlCommand(s);
        } else {
            console.warn("No qmlCommand hook; payload:", s);
        }
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

    // ===================== fieldCol (ยังใช้ได้ ถ้าจะเปิดฟอร์มด้านบนในอนาคต) =====================
    Rectangle {
        id: rectangle1
        anchors.fill: parent
        color: hmiTheme.page

        Component {
            id: fieldCol
            ColumnLayout {
                property alias labelText: lbl.text
                property alias textField: tf
                Layout.fillWidth: true
                spacing: 8

                C2.Label {
                    id: lbl
                    color: hmiTheme.text
                    font.pixelSize: 20
                    text: "Label"
                }
                C2.TextField {
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

        // ========================= LIST การ์ดด้านล่าง ===========================
        Rectangle {
            id: rectangleDeviceList
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 85
            anchors.bottomMargin: 24 + (registerDevice.tableWantsInset ? Qt.inputMethod.keyboardRectangle.height : 0)
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            color: "transparent"

            Behavior on anchors.bottomMargin {
                NumberAnimation { duration: 160; easing.type: Easing.InOutQuad }
            }

            Flickable {
                id: cardFlick
                anchors.fill: parent
                clip: true

                contentWidth: width
                contentHeight: Math.max(height, container.height + 32)

                Item {
                    id: container
                    width: cardFlick.width
                    height: cardFlow.childrenRect.height
                    anchors.margins: 8

                    Flow {
                        id: cardFlow
                        width: parent.width
                        spacing: 24
                        property int columns: width >= 1500 ? 4 : (width >= 1050 ? 3 : (width >= 650 ? 2 : 1))

                        Repeater {
                            model: listoFDevice

                            delegate: Rectangle {
                                id: card
                                width: (cardFlow.width - cardFlow.spacing * (cardFlow.columns - 1)) / cardFlow.columns
                                radius: hmiTheme.radiusLg
                                color: hmiTheme.cardAlt
                                border.color: hmiTheme.line
                                border.width: 1
                                implicitHeight: contentColumn.implicitHeight + buttonsRow.height + 32
                                // ---------- FILTER ตาม search ----------
                                property bool matchesFilter: {
                                    var t = registerDevice.filterText
                                    if (!t || t.trim() === "")
                                        return true

                                    t = t.toLowerCase()

                                    function contains(v) {
                                        return String(v).toLowerCase().indexOf(t) !== -1
                                    }

                                    // เช็ค name, ip, uri, sid
                                    return contains(name) ||
                                           contains(ip)   ||
                                           contains(uri)  ||
                                           contains(sid)
                                }

                                visible: matchesFilter

                                Column {
                                    id: contentColumn
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 16
                                    spacing: 8

                                    Text {
                                        text: name
                                        color: hmiTheme.text
                                        font.pixelSize: 20
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        anchors.horizontalCenter: parent.horizontalCenter
                                    }

                                    Rectangle {
                                        width: parent.width
                                        height: 1
                                        color: hmiTheme.accent
                                    }

                                    Row {
                                        spacing: 8
                                        Text { text: "Group:"; color: hmiTheme.textSecondary; width: 110 }
                                        Text { text: String(group); color: hmiTheme.text }
                                    }

                                    Row {
                                        spacing: 8
                                        Text { text: "IP:"; color: hmiTheme.textSecondary; width: 110 }
                                        Text { text: String(ip); color: hmiTheme.info }
                                    }

                                    Row {
                                        spacing: 8
                                        Text { text: "SID:"; color: hmiTheme.textSecondary; width: 110 }
                                        Text { text: String(sid); color: hmiTheme.text }
                                    }

                                    Row {
                                        spacing: 8
                                        Text { text: "URI:"; color: hmiTheme.textSecondary; width: 110 }
                                        Text { text: String(uri); color: hmiTheme.success }
                                    }

                                    Row {
                                        spacing: 8
                                        Text { text: "Frequency:"; color: hmiTheme.textSecondary; width: 110 }
                                        Text { text: String(freq) + " MHz"; color: hmiTheme.text }
                                    }

                                    Row {
                                        spacing: 8
                                        Text { text: "Updated At:"; color: hmiTheme.textSecondary; width: 110 }
                                        Text { text: String(updated_at); color: hmiTheme.text }
                                    }
                                }

                                Row {
                                    id: buttonsRow
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 16
                                    spacing: 12
                                    height: 40

                                    Rectangle {
                                        radius: 6
                                        color: editMouse.containsMouse ? hmiTheme.accentHover : hmiTheme.accent
                                        width: (parent.width - 12) / 2
                                        height: parent.height
                                        Text {
                                            anchors.centerIn: parent
                                            text: "Edit"
                                            color: "white"
                                            font.pixelSize: 14
                                            font.bold: true
                                        }
                                        MouseArea { id: editMouse; anchors.fill: parent; hoverEnabled: true; onClicked: registerDevice.openEditPopup(index) }
                                    }

                                    Rectangle {
                                        radius: 6
                                        color: deleteMouse.containsMouse ? Qt.lighter(hmiTheme.danger, 1.08) : hmiTheme.danger
                                        width: (parent.width - 12) / 2
                                        height: parent.height
                                        Text {
                                            anchors.centerIn: parent
                                            text: "Delete"
                                            color: "white"
                                            font.pixelSize: 14
                                            font.bold: true
                                        }
                                        MouseArea { id: deleteMouse; anchors.fill: parent; hoverEnabled: true; onClicked: registerDevice.deleteDevice(index) }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        AddNewDevice {
            id: addNewDevice
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            height: 82
            isDarkTheme: registerDevice.isDarkTheme

            // รับข้อความค้นหาจาก TextField ใน AddNewDevice.qml
            onSearchTextChanged: {
                registerDevice.filterText = text
                // console.log("filterText =", text)
            }
        }

    }

    Rectangle {
        y: inputKey.inputPanel.x + 230
        x: 263
        width: inputKey.inputPanel.width
        height: 33
        color: "#000000"
        radius: 0
        visible: Qt.inputMethod.visible && screenrotation == 270
        Text {
            id: previewText
            anchors.fill: parent
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignBottom
            text: Qt.inputMethod.visible &&
                  activeFocusItem !== null &&
                  typeof activeFocusItem.text === "string"
                  ? activeFocusItem.text : ""
            font.pointSize: 12
        }
    }
}
