// sidepanels/SideGroup.qml — show all device lists per group (no nesting) + clear on new JSON
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../i18n" as I18n

Item {
    id: group
    anchors.fill: parent

    // groups: [{ groupId, name, uniqueIdInGroup, items: [...] }]
    property var groups: []
    property var krakenmapval: null
    property string test: ""
    property bool _applying: false

    // ====== เคลียร์ UI/โมเดลก่อนรับข้อมูลใหม่ ======
    function resetUIBeforeApply() {
        // เคลียร์ selection ของทุก GroupCard ที่สร้างอยู่
        for (let i = 0; i < groupListView.count; ++i) {
            const it = groupListView.itemAtIndex(i)
            if (it) it.selected = false
        }
        groupListView.currentIndex = -1
        // ย้ายตำแหน่งเลื่อนไปบนสุด (ป้องกันสะดุดจาก list ยาว)
        if (groupListView.contentY !== 0)
            groupListView.positionViewAtBeginning()

        // เคลียร์ model ทั้งสองตัว
        devicegroupModel.clear()
        groups = []

        // บังคับให้ ListView ว่างจริง ๆ รอบนึง
        groupListView.forceLayout()
    }

    // ====== แปลง JSON -> devicegroupModel -> rebuildGroups ======
    function applyRemoteGroups(json) {
        if (_applying) return
        _applying = true
        try {
            // ล้างทุกอย่างก่อนใส่ข้อมูลใหม่
            resetUIBeforeApply()

            var obj = (typeof json === "string") ? JSON.parse(json) : json
            if (!obj || obj.objectName !== "RemoteGroups" || !obj.records) {
                console.warn("[SideGroup] invalid payload:", json)
                return
            }

            for (var i = 0; i < obj.records.length; ++i) {
                var r = obj.records[i] || {}
                devicegroupModel.append({
                    GroupsName:      String(r.GroupsName || ""),
                    GroupsID:        Number(r.GroupsID || 0),
                    uniqueIdInGroup: String(r.uniqueIdInGroup || ""),   // ⭐ เก็บเข้ามาด้วย
                    DeviceName:      String(r.DeviceName || ""),
                    IPAddress:       String(r.IPAddress || ""),
                    Port:            Number(r.Port || 0),
                    status:          String(r.status || "Offline")
                })
            }
            rebuildGroups()
        } catch(e) {
            console.warn("[SideGroup] applyRemoteGroups error:", e, json)
        } finally {
            _applying = false
        }
    }

    // ====== รวม records ใน devicegroupModel ให้เป็นกลุ่ม ======
    function rebuildGroups() {
        const map = {}
        for (let i = 0; i < devicegroupModel.count; ++i) {
            const it   = devicegroupModel.get(i)
            const id   = Number(it.GroupsID)
            const name = it.GroupsName
            const uid  = it.uniqueIdInGroup   // ⭐

            if (!map[id]) {
                map[id] = {
                    name: name,
                    uniqueIdInGroup: uid,  // ⭐ ผูก uid กับ group
                    items: []
                }
            }

            map[id].items.push({
                GroupsID:        id,
                uniqueIdInGroup: uid,       // ⭐ เผื่ออยากใช้ใน item-level ด้วย
                DeviceName:      it.DeviceName,
                IPAddress:       it.IPAddress,
                Port:            it.Port,
                status:          it.status
            })
        }

        const out = []
        for (const id in map) if (map.hasOwnProperty(id)) {
            out.push({
                groupId:        Number(id),
                name:           map[id].name,
                uniqueIdInGroup: map[id].uniqueIdInGroup,   // ⭐ ติดไปที่ group object
                items:          map[id].items
            })
        }
        out.sort((a,b) => a.groupId - b.groupId)

        // ลดการ rebuild delegate โดยเช็คความเหมือนก่อน
        const same = JSON.stringify(out) === JSON.stringify(groups)
        if (!same) groups = out
    }

    Connections {
        target: krakenmapval
        function onSetremoteGroupsJson(json) {
            console.log("[SideGroup] got (clear & apply):", json)
            applyRemoteGroups(json)
        }

        // ⭐ รับค่าจาก C++ เพื่อ select group โดยอัตโนมัติจาก uniqueIdInGroup
        // C++: emit setSelectedGroupByUniqueId(groupUniqueId);
        function onSetSelectedGroupByUniqueId(uid) {
            console.log("[SideGroup] onSetSelectedGroupByUniqueId:", uid)
            groupListView.selectGroupByUniqueId(uid)
        }
        // function onSetSelectedGroupByUniqueId(uid) {
        //     console.log("[SideGroup] onSetSelectedGroupByUniqueId:", uid)

        //     if (!uid || uid === "") {
        //         console.log("[SideGroup] Clear all selections")

        //         // ล้างการเลือกทั้งหมด
        //         groupListView.currentIndex = -1

        //         if (groupListView.clearSelection)
        //             groupListView.clearSelection()

        //         return
        //     }

        //     // ถ้า uid มีค่า → เลือก group
        //     groupListView.selectGroupByUniqueId(uid)
        // }

        // ถ้าอยากเลือกจาก groupId แทน ก็เพิ่มได้แบบนี้:
        // function onSetSelectedGroupById(gid) {
        //     console.log("[SideGroup] onSetSelectedGroupById:", gid)
        //     groupListView.selectGroupById(gid)
        // }
    }

    // ====== โหลดข้อมูลครั้งแรก ======
    Component.onCompleted: {
        Qt.callLater(function() {
            if (krakenmapval) {
                krakenmapval.getdatabaseToSideSettingDrawer("SideGroup")
                console.log("[SideGroup] SideGroup initial fetch")
            }
        })
    }

    // ====== Header ======
    Row {
        id: header
        spacing: 10
        height: 36
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 12

        Label {
            anchors.verticalCenter: parent.verticalCenter
            color: "#eeeeee"
            text: "Remote Groups"
            font.pixelSize: 18
            font.bold: true
            Layout.alignment: Qt.AlignVCenter
        }
        Item { Layout.fillWidth: true }

        Rectangle {
            id: addButton
            width: 50; height: 35; radius: height / 2
            anchors.right: parent.right
            anchors.rightMargin: 30
            color: "#25303b"
            Layout.alignment: Qt.AlignVCenter

            Image {
                anchors.centerIn: parent
                source: "qrc:/iScreenDFqml/images/gearicon.png"
                width: 37; height: 37
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (krakenmapval)
                        krakenmapval.openPopupSetting("Group Management")
                }
                onEntered: addButton.color = "#324152"
                onExited:  addButton.color = "#25303b"
            }
        }
    }

    // ====== รายการกลุ่ม ======
    ListView {
        id: groupListView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 18
        anchors.bottomMargin: 12
        spacing: 10
        clip: true
        model: groups

        cacheBuffer: Math.max(height, 800)

        function selectGroupByUniqueId(uid) {
            if (!uid || uid.length === 0) return
            for (let i = 0; i < count; ++i) {
                const it = itemAtIndex(i)
                if (!it) continue
                if (it.uniqueIdInGroup === uid) {
                    // เซ็ต selected เฉพาะตัวที่ตรง uid
                    for (let j = 0; j < count; ++j) {
                        const it2 = itemAtIndex(j)
                        if (it2) it2.selected = (j === i)
                    }
                    currentIndex = i
                    positionViewAtIndex(i, ListView.Center)
                    console.log("[SideGroup] auto select group index", i, "uid", uid)
                    return
                }
            }
            console.log("[SideGroup] auto select uid not found:", uid)
        }

        // ถ้าจะรองรับเลือกจาก groupId ด้วยก็เพิ่มฟังก์ชันนี้ได้
        function selectGroupById(gid) {
            for (let i = 0; i < count; ++i) {
                const it = itemAtIndex(i)
                if (!it) continue
                if (it.groupId === gid) {
                    for (let j = 0; j < count; ++j) {
                        const it2 = itemAtIndex(j)
                        if (it2) it2.selected = (j === i)
                    }
                    currentIndex = i
                    positionViewAtIndex(i, ListView.Center)
                    console.log("[SideGroup] auto select groupId", gid, "index", i)
                    return
                }
            }
            console.log("[SideGroup] auto select groupId not found:", gid)
        }

        delegate: GroupCard {
            width: groupListView.width
            property int groupId: modelData.groupId
            property string uniqueIdInGroup: modelData.uniqueIdInGroup   // ⭐ ใช้ใน Card ได้
            title: modelData.name
            items: modelData.items

            onHeaderClicked: function(title, items) {
                // clear selection อันอื่น
                for (let i = 0; i < groupListView.count; ++i) {
                    const it = groupListView.itemAtIndex(i)
                    if (it && it !== this) it.selected = false
                }
                this.selected = true
                groupListView.currentIndex = index

                console.log("[SideGroup] Select Group :", groupId , title,
                            "uid:", uniqueIdInGroup)

                const payload = {
                    payload: [{
                        id: groupId,
                        GroupsName: title,
                        uniqueIdInGroup: uniqueIdInGroup   // ⭐ ส่งต่อไป C++ ถ้าต้องการ
                    }]
                }
                if (krakenmapval) {
                    krakenmapval.groupSetting("SelectGroup",
                                             groupId,
                                             JSON.stringify(payload))
                }
            }

            onEditClicked: function(newTitle) {
                var editPayload = [{
                    id: groupId,
                    GroupsName: newTitle,
                    uniqueIdInGroup: uniqueIdInGroup   // ⭐ เผื่อใช้ฝั่ง C++
                }]
                var jsonEdit = JSON.stringify({ payload: editPayload })
                console.log("[SideGroup] edit Name Group:", groupId , jsonEdit)
                if (krakenmapval) krakenmapval.groupSetting("editName", 0, jsonEdit)
            }

            onAddClicked: function(title) {
                var settingbyGroupPayload = [{
                    id: groupId,
                    GroupsName: title,
                    uniqueIdInGroup: uniqueIdInGroup
                }]
                var settingbyGroupJson = JSON.stringify({ payload: settingbyGroupPayload })
                console.log("[SideGroup] setting:", groupId , settingbyGroupJson)
                if (krakenmapval) krakenmapval.groupSetting("settingbyGroup", 0, settingbyGroupJson)
            }
        }
    }

    // ====== Model แบน ======
    ListModel { id: devicegroupModel }
    ListModel { id: deviceModel } // (optional)
}
