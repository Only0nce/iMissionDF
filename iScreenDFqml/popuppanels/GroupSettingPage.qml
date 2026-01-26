// /popuppanels/GroupSettingPage.qml
import QtQuick 2.15
import QtGraphicalEffects 1.12
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../i18n" as I18n

Item {
    id: gropsetting
    anchors.fill: parent
    property var krakenmapval: null

    /* ====== Palette / Metrics ====== */
    property color colBg:        "#0f1115"
    property color colCard:      "#1a1e24"
    property color colCardHi:    "#202633"
    property color colBorder:    "#263041"
    property color colAccent:    "#34d399"     // teal green
    property color colAccentDim: "#2aa57a"
    property color colText:      "#e5e7eb"
    property color colSubtext:   "#a3a9b3"
    property int   rad: 12
    property int   pad: 10
    property int   rowH: 40

    // -------- Devices --------
    ListModel {
        id: deviceModel
        // roles: deviceUniqueId, name, ip, port, status, rssi
    }

    // -------- Groups --------
    ListModel {
        id: groupModel
        // roles:
        // name, count, devices (csv ของ deviceUniqueId), groupID, uniqueIdInGroup, hasSentAdd
    }

    // -------- State --------
    property int selectedGroupIndex: -1
    property int selectedAvailIndex: -1
    property int selectedInGroupIndex: -1
    property var deletedGroups: []

    // -------- Utils --------
    function _parseIds(csv) {
        if (!csv || csv.trim().length === 0) return []
        var arr = csv.split(",")
        var out = []
        for (var i=0;i<arr.length;i++){
            var s = String(arr[i]).trim()
            if (s.length>0 && out.indexOf(s)===-1) out.push(s)
        }
        return out
    }
    function _idsToString(arr) { return arr.join(",") }

    // ตอนนี้ idArray คือ array ของ deviceUniqueId
    function _setGroupDevices(idx, idArray) {
        if (idx < 0 || idx >= groupModel.count) return
        groupModel.setProperty(idx, "devices", _idsToString(idArray))
        groupModel.setProperty(idx, "count", idArray.length)
    }

    // devIdStr คือ deviceUniqueId
    function _deviceInGroup(idx, devIdStr) {
        if (idx < 0 || idx >= groupModel.count) return false
        var ids = _parseIds(groupModel.get(idx).devices)
        return ids.indexOf(String(devIdStr)) !== -1
    }

    function _deviceAt(listModel, i) { return (i>=0 && i<listModel.count) ? listModel.get(i) : null }
    function _isInCurrentGroup(devIdStr) { return _deviceInGroup(selectedGroupIndex, devIdStr) }

    // ====== แจ้ง C++ ตอน Add/Delete Group ======
    // action: "add" หรือ "delete"
    function notifyBackendGroupChange(action, groupIndex) {
        if (!krakenmapval) {
            console.warn("[GroupSettingPage] notifyBackendGroupChange: krakenmapval is null")
            return
        }
        if (groupIndex < 0 || groupIndex >= groupModel.count) {
            console.warn("[GroupSettingPage] notifyBackendGroupChange: index out of range")
            return
        }

        var g   = groupModel.get(groupIndex)
        var gid = g.groupID
        var uid = g.uniqueIdInGroup || ""
        var name = g.name || ""
        var devUidArray = _parseIds(g.devices)   // ⭐ ตอนนี้คือ list ของ deviceUniqueId

        // requirement: เวลา add group ต้องมี device ไปด้วย
        if (action === "add" && devUidArray.length === 0) {
            console.warn("[GroupSettingPage] notifyBackendGroupChange(add): no devices, skip")
            return
        }

        var payload = [{
            action:   action,          // "add" / "delete"
            groupID:  gid,
            groupName: name,
            deviceUniqueIds: devUidArray, // ⭐ ส่งเป็น array ของ deviceUniqueId
            uniqueIdInGroup: uid
        }]

        var json = JSON.stringify({
            title:   "EditGroup",
            payload: payload
        })

        console.log("[GroupSettingPage] notifyBackendGroupChange:", json)
        krakenmapval.groupSetting("EditGroup", 0, json)
    }

    // ====== แจ้ง C++ ตอน Add/Remove Device ใน Group เดิม (groupID > 0) ======
    // devUidStr = deviceUniqueId ของ device ที่เพิ่ม/ลบ
    function notifyBackendDeviceChange(action, groupIndex, devUidStr, currentUids) {
        if (!krakenmapval) {
            console.warn("[GroupSettingPage] notifyBackendDeviceChange: krakenmapval is null")
            return
        }
        if (groupIndex < 0 || groupIndex >= groupModel.count) {
            console.warn("[GroupSettingPage] notifyBackendDeviceChange: groupIndex out of range")
            return
        }

        var g   = groupModel.get(groupIndex)
        var gid = g.groupID
        var uid = g.uniqueIdInGroup || ""

        // ใช้เฉพาะ groupID ที่มีอยู่แล้วใน DB
        if (gid === undefined || gid === null || gid <= 0) {
            console.warn("[GroupSettingPage] notifyBackendDeviceChange: groupID invalid (new group), skip")
            return
        }

        var devUidArray = currentUids ? currentUids.slice(0) : _parseIds(g.devices)

        var payload = [{
            action:   action,         // "add" หรือ "remove"
            groupID:  gid,
            groupName: g.name,
            deviceUniqueId: devUidStr,   // ⭐ device ที่เพิ่ง add/remove
            deviceUniqueIds: devUidArray, // ⭐ list ปัจจุบันหลังเปลี่ยนแล้ว
            uniqueIdInGroup: uid
        }]

        var json = JSON.stringify({
            title:   "EditGroupDevices",
            payload: payload
        })

        console.log("[GroupSettingPage] notifyBackendDeviceChange:", json)
        krakenmapval.groupSetting("EditGroupDevices", 0, json)
    }

    // -------- CRUD --------
    function addGroup(nameText) {
        var name = (nameText||"").trim()
        if (name.length === 0) return
        for (var i=0;i<groupModel.count;i++){
            if (groupModel.get(i).name === name) {
                console.log("[Group] name duplicated:", name)
                return
            }
        }

        // สร้าง group ใหม่ใน model (ยังไม่มี groupID จริง, ยังไม่ส่งไป C++)
        groupModel.append({
            name: name,
            count: 0,
            devices: "",          // CSV ของ deviceUniqueId
            groupID: -1,          // ยังไม่มี ID จาก DB
            uniqueIdInGroup: "",
            hasSentAdd: false     // ใช้กันไม่ให้ส่ง "add group" ซ้ำ
        })
        selectedGroupIndex = groupModel.count-1

        // *** ไม่ส่ง JSON ตอนนี้ เพราะยังไม่มี device
        // จะไปส่งตอนเพิ่ม device ตัวแรกเข้า group นี้แทน
    }

    function deleteGroup(idx) {
        if (idx < 0 || idx >= groupModel.count) return

        var g = groupModel.get(idx)
        var gid = g.groupID
        if (gid !== undefined && deletedGroups.indexOf(gid) === -1)
            deletedGroups.push(gid)

        // ถ้า groupID > 0 แปลว่ามีใน DB แล้ว -> แจ้ง C++ ให้ลบทันที
        if (gid !== undefined && gid !== null && gid > 0) {
            notifyBackendGroupChange("delete", idx)
        }

        groupModel.remove(idx)

        if (groupModel.count === 0) selectedGroupIndex = -1
        else if (idx >= groupModel.count) selectedGroupIndex = groupModel.count - 1
        else selectedGroupIndex = idx
    }

    // dev คือ object จาก deviceModel (มี role deviceUniqueId)
    function addDeviceToSelectedGroup(dev) {
        if (!dev) return

        var devUidStr = String(dev.deviceUniqueId || "")

        var gidx = selectedGroupIndex
        if (gidx < 0) return

        var g = groupModel.get(gidx)
        var ids = _parseIds(g.devices)        // ตอนนี้ ids = list ของ deviceUniqueId
        if (ids.indexOf(devUidStr) === -1) {
            ids.push(devUidStr)
            _setGroupDevices(gidx, ids)

            // *** logic แยกกรณี group ใหม่ / group เดิม ***

            // 1) ถ้า groupID > 0 -> group อยู่ใน DB แล้ว
            //    ส่ง EditGroupDevices (add device) ปกติ
            if (g.groupID !== undefined && g.groupID !== null && g.groupID > 0) {
                notifyBackendDeviceChange("add", gidx, devUidStr, ids)
            }
            else {
                // 2) group ใหม่ (groupID <= 0)
                // requirement: เวลา add group ต้องมี Device ไปด้วย
                // ดังนั้นตอนนี้คือเวลาเหมาะจะยิง "add group" ครั้งแรก
                if (!g.hasSentAdd) {
                    // mark flag ใน model (ไม่ใช้ค่าจาก copy g)
                    groupModel.setProperty(gidx, "hasSentAdd", true)
                    // ยิง add group พร้อม devices ทั้งชุด
                    notifyBackendGroupChange("add", gidx)
                } else {
                    // ถ้าเคยส่ง add group แล้ว แต่ groupID ยังไม่อัปเดต
                    // รอ backend ตอบกลับ (applyRemoteGroups) ให้ groupID จริงก่อน
                    console.log("[GroupSettingPage] addDeviceToSelectedGroup: new group already sent, wait for DB")
                }
            }
        }
    }

    function removeDeviceFromSelectedGroup(dev) {
        if (!dev) return

        var devUidStr = String(dev.deviceUniqueId || "")

        var gidx = selectedGroupIndex
        if (gidx < 0) return
        var g = groupModel.get(gidx)
        var ids = _parseIds(g.devices)
        var k = ids.indexOf(devUidStr)
        if (k !== -1) {
            ids.splice(k,1)
            _setGroupDevices(gidx, ids)

            // remove ใช้ได้เฉพาะ group เดิม (groupID > 0)
            if (g.groupID !== undefined && g.groupID !== null && g.groupID > 0) {
                notifyBackendDeviceChange("remove", gidx, devUidStr, ids)
            } else {
                console.log("[GroupSettingPage] removeDeviceFromSelectedGroup: new group, not in DB yet")
            }
        }
    }

    // -------- รับ JSON จาก C++ -> สร้าง deviceModel + groupModel --------
    function applyRemoteGroups(obj) {
        if (!obj) return;

        groupModel.clear();
        deviceModel.clear();

        // -------- Devices --------
        if (obj.devices && obj.devices.length) {
            for (var i = 0; i < obj.devices.length; i++) {
                var d = obj.devices[i];
                var devUid = String(d.deviceUniqueId || d.deviceUid || "")

                deviceModel.append({
                    deviceUniqueId:  devUid,
                    name: d.name || ("dev_" + devUid),
                    ip:   d.ip   || "",
                    port: d.port || 0,
                    status: "Unknown",
                    rssi: 0
                });
            }
        }

        // -------- Groups --------
        if (obj.groups && obj.groups.length) {
            for (var j = 0; j < obj.groups.length; j++) {

                var g = obj.groups[j];

                // devices -> string array (ตอนนี้ backend ส่งเป็น deviceUniqueId)
                var devs = [];
                if (Array.isArray(g.devices)) {
                    for (var k = 0; k < g.devices.length; k++)
                        devs.push(String(g.devices[k]));
                }

                groupModel.append({
                    name:   g.name || ("Group_" + (j+1)),
                    count:  g.count || devs.length,
                    groupID: g.groupID,
                    devices: devs.join(","),              // CSV ของ deviceUniqueId

                    // รับค่า uniqueIdInGroup เต็ม ๆ
                    uniqueIdInGroup: g.uniqueIdInGroup || "",

                    // ถ้ามาจาก Database ถือว่าส่ง add แล้ว
                    hasSentAdd: true
                });
            }
        }

        selectedAvailIndex   = -1;
        selectedInGroupIndex = -1;

        if (groupModel.count > 0) {
            selectedGroupIndex = 0;
            if (groupListView) groupListView.currentIndex = 0;
        } else {
            selectedGroupIndex = -1;
            if (groupListView) groupListView.currentIndex = -1;
        }
    }

    // -------- เปลี่ยนชื่อ Group --------
    function renameSelectedGroup(newTitle) {
        var idx = selectedGroupIndex
        if (idx < 0 || idx >= groupModel.count) {
            console.warn("[GroupSettingPage] rename: no group selected")
            return
        }
        var newName = (newTitle || "").trim()
        if (newName.length === 0) {
            console.warn("[GroupSettingPage] rename: empty name")
            return
        }

        var g     = groupModel.get(idx)
        var gid   = g.groupID
        var uid   = g.uniqueIdInGroup || ""

        for (var i=0; i<groupModel.count; i++) {
            if (i === idx) continue
            if (groupModel.get(i).name === newName) {
                console.warn("[GroupSettingPage] rename: duplicated name:", newName)
                return
            }
        }

        // ส่งไป backend ถ้ามี groupID
        if (gid !== undefined && gid !== null && gid > 0) {
            var editPayload = [{
                id: gid,
                GroupsName: newName,
                uniqueIdInGroup: uid
            }]
            var jsonEdit = JSON.stringify({ payload: editPayload })
            console.log("edit Name Group:", gid, jsonEdit)
            if (krakenmapval) krakenmapval.groupSetting("editName", 0, jsonEdit)
        } else {
            console.warn("[GroupSettingPage] rename: groupID undefined/invalid, will only update UI")
        }

        groupModel.setProperty(idx, "name", newName)
        newGroupField.text = newName
    }

    Connections {
        target: krakenmapval
        function onSetsigGroupsInGroupSetting(json) {
            try {
                var obj = (typeof json === "string") ? JSON.parse(json) : json
                console.log("[GroupSettingPage] parsed ok. groups=", obj && obj.groups ? obj.groups.length : 0,
                            " devices=", obj && obj.devices ? obj.devices.length : 0)
                applyRemoteGroups(obj)
            } catch (e) {
                console.error("[GroupSettingPage] JSON parse error:", e, " raw:", json)
            }
        }
    }

    // ====== Backdrop ======
    Rectangle {
        color: "#0f1115"
        anchors.fill: parent
    }

    // ====== Main Panel ======
    Rectangle {
        id: panelBg
        color: colCard
        radius: rad
        border.color: colBorder
        anchors.fill: parent
        anchors.margins: 5
        anchors.bottomMargin: 45

        ColumnLayout {
            id: mainCol
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            // ===== Header / Toolbar =====
            Rectangle {
                Layout.fillWidth: true
                height: 52
                color: "#00202633"
                radius: rad
                border.color: "#00263041"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: pad
                    spacing: 8

                    Label {
                        text: "Groups"
                        color: colText
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Rectangle {
                        radius: 10
                        color: colAccentDim
                        opacity: 0.18
                        height: 24
                        width: 66
                        Layout.alignment: Qt.AlignVCenter

                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Label {
                                text: groupModel.count
                                color: colAccent
                                font.pixelSize: 14
                                font.bold: true
                            }
                            Label {
                                text: "groups"
                                color: colSubtext
                                font.pixelSize: 12
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    TextField {
                        id: newGroupField
                        placeholderText: "New group name"
                        Layout.preferredWidth: 240
                        Layout.preferredHeight: 34

                        selectByMouse: true
                        color: colText
                        placeholderTextColor: colSubtext
                        font.pixelSize: 15

                        leftPadding: 10
                        rightPadding: 10
                        topPadding: 4
                        bottomPadding: 4

                        background: Rectangle {
                            radius: rad
                            color: "#141922"
                            border.color: colBorder
                            border.width: 1
                        }
                        onAccepted: addGroup(text)
                    }

                    Button {
                        text: "Add Group"
                        onClicked: addGroup(newGroupField.text)
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: "#25c96e"
                            radius: rad
                            border.color: colAccentDim
                        }
                    }

                    Button {
                        id: deleteBtn
                        text: "Delete Group"
                        enabled: selectedGroupIndex >= 0
                        onClicked: deleteGroup(selectedGroupIndex)
                        contentItem: Text {
                            text: parent.text
                            color: enabled ? "#ffefef" : "#7d818a"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: rad
                            color: enabled ? "#232a35" : "#1b212b"
                            border.color: colBorder
                        }
                    }

                    Button {
                        text: "Rename"
                        enabled: selectedGroupIndex >= 0
                        onClicked: renameSelectedGroup(newGroupField.text)
                        contentItem: Text {
                            text: parent.text
                            color: enabled ? "#ffefef" : "#7d818a"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: rad
                            color: enabled ? "#2b3543" : "#1b212b"
                            border.color: colBorder
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "เปลี่ยนชื่อกลุ่มที่เลือก แล้วส่ง {payload:[{id, GroupsName, uniqueIdInGroup}]}"
                    }
                }
            }

            // ===== Body: Lists + Management =====
            RowLayout {
                id: bodyRow
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                // ----- Left: Group list -----
                Rectangle {
                    Layout.preferredWidth: 280
                    Layout.fillHeight: true
                    radius: rad
                    color: colCardHi
                    border.color: colBorder

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: pad
                        spacing: 8

                        Label {
                            text: "All Groups"
                            color: colSubtext
                            font.pixelSize: 13
                        }

                        ListView {
                            id: groupListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: groupModel
                            currentIndex: selectedGroupIndex
                            onCurrentIndexChanged: {
                                selectedGroupIndex = currentIndex
                                if (currentIndex >= 0 && currentIndex < groupModel.count) {
                                    newGroupField.text = groupModel.get(currentIndex).name
                                } else {
                                    newGroupField.text = ""
                                }
                            }
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { active: true }

                            delegate: ItemDelegate {
                                id: groupRowDelegate
                                width: ListView.view.width
                                height: rowH
                                hoverEnabled: true
                                background: Rectangle {
                                    radius: rad - 6
                                    color: groupRowDelegate.down
                                           ? "#2e3746"
                                           : groupRowDelegate.hovered
                                             ? "#262f3d"
                                             : (index === groupListView.currentIndex ? "#223042" : "transparent")
                                    border.color: index === groupListView.currentIndex ? colAccentDim : "transparent"
                                }

                                contentItem: Row {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 8

                                    Label {
                                        text: name
                                        color: colText
                                        font.pixelSize: 14
                                        elide: Text.ElideRight
                                        horizontalAlignment: Text.AlignLeft
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    Item { width: 6; height: 1 }

                                    Rectangle {
                                        radius: 9
                                        height: 22
                                        width: 44
                                        color: "#1b2a22"
                                        border.color: "#234f3c"
                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 4
                                            Label {
                                                text: count
                                                color: colText
                                                font.pixelSize: 12
                                            }
                                        }
                                    }
                                }

                                onClicked: {
                                    groupListView.currentIndex = index
                                    newGroupField.text = name
                                }
                            }
                        }
                    }
                }

                // ----- Right: Manage members -----
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: rad
                    color: colCardHi
                    border.color: colBorder

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: pad
                        spacing: 8

                        Label {
                            id: manageTitle
                            Layout.fillWidth: true
                            text: selectedGroupIndex>=0
                                  ? "Manage devices in: " + groupModel.get(selectedGroupIndex).name
                                  : "Select a group to manage"
                            color: colText
                            font.pixelSize: 14
                            font.bold: true
                        }

                        RowLayout {
                            id: listsRow
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 12

                            // === Available devices ===
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: rad
                                color: colCard
                                border.color: colBorder

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: pad
                                    anchors.leftMargin: 5
                                    spacing: 6

                                    Label { text: "Available"; color: colSubtext; font.pixelSize: 13 }

                                    ListView {
                                        id: availableListView
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        model: deviceModel
                                        clip: true
                                        currentIndex: selectedAvailIndex
                                        onCurrentIndexChanged: selectedAvailIndex = currentIndex
                                        boundsBehavior: Flickable.StopAtBounds
                                        ScrollBar.vertical: ScrollBar { active: true }

                                        delegate: ItemDelegate {
                                            id: availDelegate

                                            property bool shouldShow: selectedGroupIndex >= 0 ? !_isInCurrentGroup(deviceUniqueId) : true

                                            width: ListView.view.width
                                            height: shouldShow ? rowH : 0
                                            visible: true
                                            hoverEnabled: shouldShow

                                            background: Rectangle {
                                                radius: rad - 6
                                                color: {
                                                    if (!shouldShow) return "transparent"
                                                    if (availDelegate.down) return "#27303d"
                                                    if (availDelegate.hovered) return "#202733"
                                                    return "transparent"
                                                }
                                                border.color: "transparent"
                                            }

                                            contentItem: Loader {
                                                active: shouldShow
                                                sourceComponent: Row {
                                                    anchors.fill: parent
                                                    anchors.margins: 10
                                                    spacing: 8

                                                    Label {
                                                        text: name + "  [" + ip + "]"
                                                        color: colText
                                                        font.pixelSize: 14
                                                        elide: Text.ElideRight
                                                        verticalAlignment: Text.AlignVCenter
                                                        height: parent.height
                                                        Layout.alignment: Qt.AlignVCenter
                                                    }

                                                    Item { Layout.fillWidth: true }
                                                }
                                            }

                                            onClicked: if (shouldShow) availableListView.currentIndex = index
                                        }
                                    }
                                }
                            }

                            // === Move buttons ===
                            ColumnLayout {
                                id: moveButtonsCol
                                spacing: 8
                                Layout.preferredWidth: 140
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                Layout.fillHeight: false
                                Item { Layout.fillHeight: true }

                            Button {
                                id: addBtn
                                text: "Add →"
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                enabled: selectedGroupIndex>=0
                                         && selectedAvailIndex>=0
                                         && availableListView.currentItem
                                         && availableListView.currentItem.visible
                                onClicked: {
                                    var d = _deviceAt(deviceModel, selectedAvailIndex)
                                    if (d) {
                                        addDeviceToSelectedGroup(d)
                                        selectedAvailIndex = -1
                                        availableListView.currentIndex = -1
                                    }
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    font.pixelSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: "#25c96e"
                                    radius: rad
                                    border.color: colAccentDim
                                }
                            }

                            Button {
                                id: removeBtn
                                text: "← Remove"
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                enabled: selectedGroupIndex>=0
                                         && selectedInGroupIndex>=0
                                         && _isInCurrentGroup(deviceModel.get(selectedInGroupIndex).deviceUniqueId)
                                onClicked: {
                                    var d2 = deviceModel.get(selectedInGroupIndex)
                                    if (d2) {
                                        removeDeviceFromSelectedGroup(d2)
                                        selectedInGroupIndex = -1
                                        inGroupListView.currentIndex = -1
                                    }
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: enabled ? "#ffefef" : "#7d818a"
                                    font.pixelSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: rad
                                    color: enabled ? "#232a35" : "#1b212b"
                                    border.color: colBorder
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }

                        // === In Group devices ===
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: rad
                            color: colCard
                            border.color: colBorder

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: pad
                                spacing: 6

                                Label { text: "In Group"; color: colSubtext; font.pixelSize: 13 }

                                ListView {
                                    id: inGroupListView
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    model: deviceModel
                                    clip: true
                                    currentIndex: selectedInGroupIndex
                                    onCurrentIndexChanged: selectedInGroupIndex = currentIndex
                                    boundsBehavior: Flickable.StopAtBounds
                                    ScrollBar.vertical: ScrollBar { active: true }

                                    delegate: ItemDelegate {
                                        id: inGroupDelegate

                                        property bool shouldShow: selectedGroupIndex >= 0 ? _isInCurrentGroup(deviceUniqueId) : false

                                        width: ListView.view.width
                                        height: shouldShow ? rowH : 0
                                        visible: true
                                        hoverEnabled: shouldShow

                                        background: Rectangle {
                                            radius: rad - 6
                                            color: {
                                                if (!shouldShow) return "transparent"
                                                if (inGroupDelegate.down) return "#27303d"
                                                if (inGroupDelegate.hovered) return "#202733"
                                                if (index === inGroupListView.currentIndex) return "#1d2631"
                                                return "transparent"
                                            }
                                            border.color: index === inGroupListView.currentIndex ? colAccentDim : "transparent"
                                        }

                                        contentItem: Loader {
                                            active: shouldShow
                                            sourceComponent: Row {
                                                anchors.fill: parent
                                                anchors.margins: 10
                                                spacing: 8

                                                Label {
                                                    text: name + "  [" + ip + "]"
                                                    color: colText
                                                    font.pixelSize: 14
                                                    elide: Text.ElideRight
                                                    verticalAlignment: Text.AlignVCenter
                                                    height: parent.height
                                                    Layout.alignment: Qt.AlignVCenter
                                                }

                                                Item { Layout.fillWidth: true }
                                            }
                                        }

                                        onClicked: if (shouldShow) inGroupListView.currentIndex = index
                                    }
                                }

                            }
                        }
                    }
                }
            }
        }
    }
    /* ===== Bottom Buttons ===== */
    // CancelButtonPopupSettingDrawer {
    //     id: cancelBtn
    //     anchors.right: parent.right
    //     anchors.bottom: parent.bottom
    //     anchors.rightMargin: 15
    //     anchors.bottomMargin: 0
    //     onClicked: deleteGroup(selectedGroupIndex)
    //     // หรือถ้าไม่อยากลบทิ้งจริง ๆ:
    //     // onClicked: popuppanel.close()
    // }

    // ApplyButtonPopupSettingDrawer {
    //     id: applyBtn
    //     anchors.right: parent.right
    //     anchors.bottom: parent.bottom
    //     anchors.rightMargin: 0
    //     anchors.bottomMargin: 0
    //     onClicked: {
    //         var payload = []
    //         for (var i = 0; i < groupModel.count; i++) {
    //             var g   = groupModel.get(i)
    //             var gid = g.groupID
    //             var uid = g.uniqueIdInGroup || ""

    //             if (deletedGroups.indexOf(gid) !== -1) continue

    //             var ids = _parseIds(g.devices)
    //             payload.push({
    //                 groupID:   gid,
    //                 groupName: g.name,
    //                 devices:   ids,
    //                 uniqueIdInGroup: uid
    //             })
    //         }
    //         var title = (payload.length === 1) ? "EditGroupbyID" : "ALL_Group"
    //         var json  = JSON.stringify({ title: title, groups: payload })
    //         console.log("BULK payload (filtered):", json)

    //         if (krakenmapval) {
    //             krakenmapval.groupSetting(title, 0, json)
    //         }
    //         popuppanel.close()
    //     }
    // }
}
    CancelButtonPopupSettingDrawer {
        id: cancelBtn
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 15
        anchors.bottomMargin: 0
        // onClicked: deleteGroup(selectedGroupIndex)
        // หรือถ้าไม่อยากลบทิ้งจริง ๆ:
        onClicked: popuppanel.close()
    }
}
