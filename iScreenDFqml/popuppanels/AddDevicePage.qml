// /popuppanels/AddDevicePage.qml
import QtQuick 2.15
import QtGraphicalEffects 1.12
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../i18n" as I18n

Item {
    id: adddevice
    anchors.fill: parent

    property var krakenmapval: null
    property var existingIPs: []
    property var existingNames: []
    property bool   editing: false
    property string editOrigName: ""
    property string editOrigIp:   ""
    property int    editId:       -1
    property string editDeviceUid: ""

    property var existingIds: []
    property var existingDeviceUids: []

    /* ====== Palette / Metrics ====== */
    property color colBg:        "#0f1115"
    property color colCard:      "#1a1e24"
    property color colCardHi:    "#202633"
    property color colBorder:    "#263041"
    property color colAccent:    "#34d399"
    property color colAccentDim: "#2aa57a"
    property color colText:      "#e5e7eb"
    property color colSubtext:   "#a3a9b3"
    property int   rad: 12
    property int   pad: 10
    property int   rowH: 40

    /* ====== Public signals ====== */
    signal addManyRequested(var devices)   // [{name, ip}, ...]

    property var existingDevices: []        // [{id, name, ip, uid}]
    property var _filteredDevices: []       // [{id, name, ip, uid}]

    /* ====== Helpers ====== */
    function isValidIp(ip) {
        // IPv4 basic
        var re = /^(25[0-5]|2[0-4][0-9]|[01]?[0-9]?[0-9])(\.(25[0-5]|2[0-4][0-9]|[01]?[0-9]?[0-9])){3}$/
        return re.test(String(ip||"").trim())
    }
    function isDupIp(ip) {
        if (!existingIPs || existingIPs.length === 0) return false
        return existingIPs.indexOf(ip) !== -1
    }
    function isDupName(name) {
        if (!existingNames || existingNames.length === 0) return false
        return existingNames.indexOf(name) !== -1
    }

    function ipToU32(ip) {
        // return uint32, invalid -> -1
        var s = String(ip||"").trim()
        if (!isValidIp(s)) return -1
        var p = s.split(".")
        var a = parseInt(p[0],10), b = parseInt(p[1],10), c = parseInt(p[2],10), d = parseInt(p[3],10)
        return ((a<<24)>>>0) + ((b<<16)>>>0) + ((c<<8)>>>0) + (d>>>0)
    }
    function u32ToIp(u) {
        u = (u>>>0)
        var a = (u>>>24) & 255
        var b = (u>>>16) & 255
        var c = (u>>>8)  & 255
        var d = (u>>>0)  & 255
        return a+"."+b+"."+c+"."+d
    }

    function _rebuildExistingDevices() {
        var out = []
        var n = Math.max(existingIPs.length||0,
                         existingNames.length||0,
                         existingIds.length||0,
                         existingDeviceUids.length||0)
        for (var i=0;i<n;i++) {
            out.push({
                id:   (existingIds[i]  !== undefined ? existingIds[i]  : i),
                name: existingNames[i] || "",
                ip:   existingIPs[i]   || "",
                uid:  existingDeviceUids[i] !== undefined ? existingDeviceUids[i] : ""
            })
        }
        existingDevices = out
    }

    function _updateFiltered() {
        var nKey = nameField.text.trim().toLowerCase()
        var iKey = ipField.text.trim()
        _filteredDevices = existingDevices.filter(function(d){
            var okName = nKey.length ? d.name.toLowerCase().indexOf(nKey) !== -1 : true
            var okIp   = iKey.length ? d.ip.indexOf(iKey) !== -1 : true
            return okName && okIp
        })
    }

    function applyDeviceList(json) {
        try {
            var obj = (typeof json === "string") ? JSON.parse(json) : json
            if (!obj || obj.objectName !== "DeviceList" || !obj.records) {
                console.warn("[AddDevicePage] invalid payload:", json)
                return
            }
            var ips = [], names = [], ids = [], uids = []
            for (var i = 0; i < obj.records.length; ++i) {
                var r = obj.records[i]
                var rid = (r.id !== undefined) ? r.id :
                          (r.ID !== undefined) ? r.ID :
                          (r.deviceId !== undefined) ? r.deviceId :
                          (r.DeviceID !== undefined) ? r.DeviceID : i
                ids.push(Number(rid))
                ips.push(String(r.ip || r.IPAddress || ""))
                names.push(String(r.name || r.Name || ""))

                var duid = r.deviceUniqueId || r.deviceUID || r.uid || ""
                uids.push(String(duid))
            }
            existingIPs         = ips
            existingNames       = names
            existingIds         = ids
            existingDeviceUids  = uids

            _rebuildExistingDevices()
            _updateFiltered()
        } catch (e) {
            console.warn("[AddDevicePage] applyDeviceList error:", e, json)
        }
    }

    ////////////////////////////////////////////////////////////////////////
    function setAllSelected(flag, onlyValid) {
        for (var i = 0; i < scanModel.count; i++) {
            var it = scanModel.get(i)
            if (!onlyValid || (!isDupIp(it.ip) && !isDupName(it.name))) {
                if (it.__selected !== flag) {
                    scanModel.setProperty(i, "__selected", flag)
                }
            }
        }
    }
    function selectAllValid() { setAllSelected(true, true) }
    function clearSelection() { setAllSelected(false, false) }

    Connections {
        target: krakenmapval
        function onSetremoteDeviceListJson(json) { applyDeviceList(json) }
    }

    /* ====== Backdrop ====== */
    Rectangle { color: colBg; anchors.fill: parent }

    /* ====== Main Panel ====== */
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
            spacing: 10

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Label {
                    color: "#ffffff"
                    text: "Add Device"
                    font.pixelSize: 18
                    font.bold: true
                    Layout.alignment: Qt.AlignLeft
                }

                ButtonGroup { id: modeGroup }
                Item { Layout.fillWidth: true }

                RowLayout {
                    width: 180
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    spacing: 6

                    CheckBox {
                        id: manualModeBtn
                        checked: true
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        ButtonGroup.group: modeGroup
                        text: "Manual"
                        spacing: 6
                        leftPadding: ind.implicitWidth + 8
                        rightPadding: 0

                        indicator: Rectangle {
                            id: ind
                            implicitWidth: 16
                            implicitHeight: 16
                            radius: 4
                            color: manualModeBtn.checked ? colAccent : "transparent"
                            border.color: manualModeBtn.checked ? colAccent : colBorder
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        contentItem: Text {
                            color: colText
                            text: manualModeBtn.text
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        onToggled: if (checked) stack.currentIndex = 0
                    }

                    CheckBox {
                        id: scanModeBtn
                        text: "Scan"
                        ButtonGroup.group: modeGroup
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        spacing: 6
                        leftPadding: ind1.implicitWidth + 8
                        rightPadding: 0

                        indicator: Rectangle {
                            id: ind1
                            implicitWidth: 16
                            implicitHeight: 16
                            radius: 4
                            color: scanModeBtn.checked ? colAccent : "transparent"
                            border.color: scanModeBtn.checked ? colAccent : colBorder
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        contentItem: Text {
                            text: scanModeBtn.text
                            color: colText
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        onToggled: if (checked) stack.currentIndex = 1
                    }
                }
            }

            // Content
            StackLayout {
                id: stack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: 0

                /* ====== TAB 1: Manual ====== */
                Item {
                    id: manualPage
                    anchors.fill: parent

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        // -------- Name block --------
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text:"Device Name"
                                    color: colSubtext
                                    font.pixelSize: 14
                                }

                                Label {
                                    id: nameDupWarn
                                    visible: false
                                    text: "Name already exists"
                                    color: "#eab308"
                                    font.pixelSize: 14
                                }

                                Item { Layout.fillWidth: true }
                            }
                            TextField {
                                id: nameField
                                Layout.fillWidth: true
                                Layout.preferredHeight: 34

                                placeholderText: "e.g. beacon_one"
                                color: colText
                                selectionColor: colAccent
                                font.pixelSize: 15

                                leftPadding: 10
                                rightPadding: 10
                                topPadding: 4
                                bottomPadding: 4

                                background: Rectangle {
                                    radius: 8
                                    color: "#141922"
                                    border.color: nameDupWarn.visible ? "#eab308" : colBorder
                                    border.width: 1
                                }

                                onTextChanged: {
                                    nameDupWarn.visible =
                                            isDupName(text) &&
                                            (!editing || text.trim() !== editOrigName)
                                    _updateFiltered()
                                }
                            }
                        }

                        // -------- IP + Serial in one row --------
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                // ===== Left: IP Address =====
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Label {
                                            text: "IP Address"
                                            color: colSubtext
                                            font.pixelSize: 14
                                        }
                                        Label {
                                            id: ipError
                                            visible: false
                                            color: "#ef4444"
                                            text: "[Invalid IP]"
                                            font.pixelSize: 14
                                        }
                                        Label {
                                            id: ipDupWarn
                                            visible: false
                                            color: "#ef4444"
                                            text: "[Duplicate IP]"
                                            font.pixelSize: 14
                                        }
                                        Item { Layout.fillWidth: true }
                                    }

                                    TextField {
                                        id: ipField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34

                                        placeholderText: "192.168.x.x"
                                        color: colText
                                        selectionColor: colAccent
                                        font.pixelSize: 15

                                        leftPadding: 10
                                        rightPadding: 10
                                        topPadding: 4
                                        bottomPadding: 4
                                        inputMethodHints: Qt.ImhPreferNumbers
                                        background: Rectangle {
                                            radius: 8
                                            color: "#141922"
                                            border.color: (ipError.visible || ipDupWarn.visible) ? "#ef4444" : colBorder
                                        }
                                        onTextChanged: {
                                            ipError.visible = (text.length>0 && !isValidIp(text))
                                            ipDupWarn.visible = isDupIp(text) && (!editing || text.trim() !== editOrigIp)
                                        }
                                    }
                                }

                                Item { width: 16 }

                                // ===== Right: Serial Number =====
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Label {
                                            text: "Serial Number"
                                            color: colSubtext
                                            font.pixelSize: 14
                                        }

                                        Item { Layout.fillWidth: true }
                                    }

                                    TextField {
                                        id: serialField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        placeholderText: "auto / custom"
                                        color: colText
                                        selectionColor: colAccent
                                        font.pixelSize: 15

                                        leftPadding: 10
                                        rightPadding: 10
                                        topPadding: 4
                                        bottomPadding: 4

                                        background: Rectangle {
                                            radius: 8
                                            color: "#141922"
                                            border.color: colBorder
                                        }
                                    }
                                }
                            }
                        }

                        // -------- Existing list block --------
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Rectangle {
                                id: existingBox
                                Layout.fillWidth: true
                                visible: _filteredDevices.length > 0
                                radius: 8
                                color: "#0f141b"
                                border.color: colBorder
                                border.width: 1

                                property int rowH: 32
                                property int headerH: 20
                                property int gap: 8
                                Layout.preferredHeight: headerH + 6 + (rowH * 5) + (gap * 4) + 12

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 6

                                    Label {
                                        text: "Existing devices"
                                        color: colSubtext
                                        font.pixelSize: 14
                                        Layout.preferredHeight: headerH
                                    }
                                    GridView {
                                        id: deviceGrid
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true
                                        model: _filteredDevices.length
                                        boundsBehavior: Flickable.StopAtBounds
                                        property real sidePad: 6
                                        cellHeight: 60
                                        cellWidth: (width - sidePad * 2 - 12) / 2
                                        anchors.margins: sidePad
                                        ScrollBar.vertical: ScrollBar { active: true }

                                        delegate: Rectangle {
                                            width: deviceGrid.cellWidth - 4
                                            height: deviceGrid.cellHeight - 4
                                            radius: 6
                                            color: hovered ? "#18202a" : "transparent"
                                            border.color: "#1f2b38"
                                            anchors.margins: 2

                                            property bool hovered: false
                                            HoverHandler {
                                                acceptedDevices: PointerDevice.Mouse
                                                onActiveChanged: hovered = active
                                            }

                                            Row {
                                                anchors.fill: parent
                                                anchors.margins: 6
                                                spacing: 10
                                                anchors.verticalCenter: parent.verticalCenter

                                                Column {
                                                    width: Math.max(160, parent.width * 0.55)
                                                    spacing: 3
                                                    anchors.verticalCenter: parent.verticalCenter

                                                    Label {
                                                        text: _filteredDevices[index].name
                                                        color: colText
                                                        font.pixelSize: 14
                                                        elide: Text.ElideRight
                                                    }

                                                    Row {
                                                        spacing: 6
                                                        visible: _filteredDevices[index].ip ||
                                                                 (_filteredDevices[index].uid && _filteredDevices[index].uid.length > 0)

                                                        Label {
                                                            text: _filteredDevices[index].ip
                                                            color: colSubtext
                                                            font.pixelSize: 12
                                                            elide: Text.ElideRight
                                                        }

                                                        Label {
                                                            text: (_filteredDevices[index].uid && _filteredDevices[index].uid.length > 0)
                                                                  ? "· SN: " + _filteredDevices[index].uid
                                                                  : ""
                                                            color: colSubtext
                                                            font.pixelSize: 12
                                                            elide: Text.ElideRight
                                                        }
                                                    }
                                                }

                                                Item { Layout.fillWidth: true }

                                                Button {
                                                    text: "Edit"
                                                    width: 64; height: 30
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    background: Rectangle { radius: 6; color: "#3b82f6" }
                                                    contentItem: Text {
                                                        text: parent.text
                                                        color: "white"
                                                        font.pixelSize: 12
                                                        font.bold: true
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    onClicked: {
                                                        const d = _filteredDevices[index]
                                                        nameField.text   = d.name
                                                        ipField.text     = d.ip
                                                        serialField.text = d.uid

                                                        editing        = true
                                                        editOrigName   = d.name
                                                        editOrigIp     = d.ip
                                                        editId         = d.id
                                                        editDeviceUid  = d.uid

                                                        nameDupWarn.visible = false
                                                        ipDupWarn.visible   = false
                                                        ipError.visible     = (ipField.text.length > 0 && !isValidIp(ipField.text))
                                                    }
                                                }

                                                Button {
                                                    text: "Delete"
                                                    width: 70; height: 30
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    background: Rectangle { radius: 6; color: "#ef4444" }
                                                    contentItem: Text {
                                                        text: parent.text
                                                        color: "white"
                                                        font.pixelSize: 12
                                                        font.bold: true
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    onClicked: {
                                                        const d = _filteredDevices[index]
                                                        var jsonDelete = JSON.stringify({
                                                            payload: [{
                                                                id: d.id,
                                                                Name: d.name,
                                                                ip: d.ip,
                                                                deviceUniqueId: d.uid
                                                            }]
                                                        })
                                                        if (krakenmapval)
                                                            krakenmapval.groupSetting("DeleteDevice", 0, jsonDelete)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Item { height: 6; Layout.fillWidth: true }

                        // -------- Add button row --------
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Item { Layout.fillWidth: true }

                            Button {
                                id: addManualBtn
                                text: "Add"
                                enabled: {
                                    var name = nameField.text.trim()
                                    var ip   = ipField.text.trim()
                                    if (!name.length || !isValidIp(ip)) return false

                                    if (!editing) {
                                        return !isDupName(name) && !isDupIp(ip)
                                    } else {
                                        if (name !== editOrigName && isDupName(name)) return false
                                        if (ip   !== editOrigIp   && isDupIp(ip))     return false
                                        return true
                                    }
                                }
                                padding: 10
                                background: Rectangle {
                                    radius: 10
                                    color: addManualBtn.enabled ? colAccent : "#2a3342"
                                }
                                contentItem: Text {
                                    text: addManualBtn.text
                                    color: addManualBtn.enabled ? "#0b1118" : "#6b7280"
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    var newName = nameField.text.trim()
                                    var newIp   = ipField.text.trim()
                                    var newUid  = serialField.text.trim()
                                    var wasEditing = editing

                                    if (editing) {
                                        if (!newUid.length)
                                            newUid = editDeviceUid

                                        var updatePayload = [{
                                            id:   editId,
                                            Name: newName,
                                            ip:   newIp,
                                            oldDeviceUniqueId: editDeviceUid,
                                            deviceUniqueId:    newUid
                                        }]
                                        var jsonUpdate = JSON.stringify({ payload: updatePayload })

                                        if (krakenmapval)
                                            krakenmapval.groupSetting("UpdateDevice", 0, jsonUpdate)
                                    } else {
                                        var payloadObj = { Name: newName, ip: newIp }
                                        if (newUid.length > 0)
                                            payloadObj.deviceUniqueId = newUid

                                        var payload = [ payloadObj ]
                                        var json = JSON.stringify({ payload })
                                        if (krakenmapval)
                                            krakenmapval.groupSetting("AddDevice", 0, json)
                                    }

                                    nameField.text   = ""
                                    ipField.text     = ""
                                    serialField.text = ""
                                    editing        = false
                                    editOrigName   = ""
                                    editOrigIp     = ""
                                    editId         = -1
                                    editDeviceUid  = ""

                                    toast.show(wasEditing ? "Updated." : "Added (manual).")
                                }
                            }
                        }
                    }
                }

                /* ====== TAB 2: Scan ====== */
                Item {
                    id: scanPage
                    anchors.fill: parent

                    ListModel { id: scanModel }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        // ===== Scan control row =====
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                text: "Network Scan"
                                color: colSubtext
                                font.pixelSize: 12
                            }

                            // ✅ Start IP full
                            TextField {
                                id: startIpField
                                Layout.preferredWidth: 220
                                Layout.preferredHeight: 36   // เพิ่มจาก 32 -> 36 (ปลอดภัยกว่า)
                                implicitHeight: 36

                                text: ""
                                placeholderText: "Start IP (e.g. 192.168.10.1)"
                                color: colText
                                font.pixelSize: 13

                                // ✅ ทำให้ข้อความอยู่กลางแนวตั้ง
                                verticalAlignment: Text.AlignVCenter

                                leftPadding: 10
                                rightPadding: 10
                                topPadding: 6
                                bottomPadding: 6

                                background: Rectangle {
                                    radius: 8
                                    color: "#141922"
                                    border.color: colBorder
                                    border.width: 1
                                }

                                onTextChanged: {
                                    startIpError.visible = (text.trim().length > 0 && !isValidIp(text))
                                }
                            }

                            Label { text: "to"; color: colSubtext; font.pixelSize: 12 }

                            // ✅ End IP full
                            TextField {
                                id: endIpField
                                Layout.preferredWidth: 220
                                Layout.preferredHeight: 36
                                implicitHeight: 36

                                text: ""
                                placeholderText: "End IP (e.g. 192.168.10.254)"
                                color: colText
                                font.pixelSize: 13

                                verticalAlignment: Text.AlignVCenter

                                leftPadding: 10
                                rightPadding: 10
                                topPadding: 6
                                bottomPadding: 6

                                background: Rectangle {
                                    radius: 8
                                    color: "#141922"
                                    border.color: colBorder
                                    border.width: 1
                                }

                                onTextChanged: {
                                    endIpError.visible = (text.trim().length > 0 && !isValidIp(text))
                                }
                            }

                            // inline errors
                            Label {
                                id: startIpError
                                visible: false
                                text: "Start IP invalid"
                                color: "#ef4444"
                                font.pixelSize: 11
                            }
                            Label {
                                id: endIpError
                                visible: false
                                text: "End IP invalid"
                                color: "#ef4444"
                                font.pixelSize: 11
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                id: btnScan
                                text: scanning ? "Scanning..." : "Scan"
                                property bool scanning: false
                                padding: 10
                                enabled: !scanning
                                background: Rectangle {
                                    radius: 10
                                    color: btnScan.scanning ? colAccentDim : colAccent
                                }
                                contentItem: Text {
                                    text: btnScan.text
                                    color: "#0b1118"
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    if (scanning) return

                                    var sIp = startIpField.text.trim()
                                    var eIp = endIpField.text.trim()

                                    if (!isValidIp(sIp) || !isValidIp(eIp)) {
                                        toast.show("Please enter valid Start/End IP.")
                                        return
                                    }

                                    var sU = ipToU32(sIp)
                                    var eU = ipToU32(eIp)
                                    if (sU < 0 || eU < 0) {
                                        toast.show("Invalid IP range.")
                                        return
                                    }

                                    // ✅ swap if reversed
                                    if (eU < sU) { var t=sU; sU=eU; eU=t }

                                    // sync back to fields (nice)
                                    startIpField.text = u32ToIp(sU)
                                    endIpField.text   = u32ToIp(eU)

                                    scanning = true
                                    scanModel.clear()
                                    toast.show("Scanning " + startIpField.text + " to " + endIpField.text + " ...")

                                    // ✅ call C++ (ต้องมี Q_INVOKABLE/slot)
                                    if (krakenmapval)
                                        krakenmapval.scanDevicesRange(startIpField.text, endIpField.text)
                                }
                            }
                        }

                        Frame {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            background: Rectangle {
                                radius: 10
                                color: "#141922"
                                border.color: colBorder
                            }

                            ListView {
                                id: scanView
                                anchors.fill: parent
                                model: scanModel
                                clip: true
                                spacing: 6

                                delegate: Rectangle {
                                    width: scanView.width
                                    height: rowH
                                    radius: 8
                                    color: hovered ? colCardHi : "transparent"
                                    border.color: colBorder

                                    property bool hovered: false

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onEntered: parent.hovered = true
                                        onExited: parent.hovered = false
                                        onClicked: scanModel.setProperty(index, "__selected", !(model.__selected === true))
                                    }

                                    Row {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.margins: 10
                                        spacing: 10

                                        Rectangle {
                                            width: 18; height: 18; radius: 4
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: (model.__selected === true) ? colAccent : "transparent"
                                            border.color: (model.__selected === true) ? colAccent : colBorder
                                        }

                                        Column {
                                            spacing: 2
                                            Label { text: name; color: colText; font.pixelSize: 14 }
                                            Row {
                                                spacing: 8
                                                Label { text: ip; color: colSubtext; font.pixelSize: 12 }
                                                Label {
                                                    visible: serial !== undefined && serial.length > 0
                                                    text: (serial !== undefined && serial.length > 0) ? ("SN: " + serial) : ""
                                                    color: colSubtext
                                                    font.pixelSize: 12
                                                }
                                            }
                                        }

                                        Item { width: 10 }

                                        Label {
                                            visible: isDupIp(ip) || isDupName(name)
                                            text: isDupIp(ip) ? "Duplicate IP" : "Duplicate Name"
                                            color: "#ef4444"
                                            font.pixelSize: 11
                                        }

                                        Item { Layout.fillWidth: true }

                                        Label {
                                            text: ping !== undefined ? (ping + " ms") : ""
                                            color: colSubtext
                                            font.pixelSize: 12
                                            horizontalAlignment: Text.AlignRight
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                id: btnSelectAll
                                text: "Select All"
                                padding: 10
                                onClicked: selectAllValid()
                                background: Rectangle { radius: 10; color: colAccent }
                                contentItem: Text {
                                    text: btnSelectAll.text
                                    color: "#0b1118"; font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "เลือกทั้งหมด (ข้ามรายการที่ชื่อ/IP ซ้ำ)"
                            }

                            Button {
                                id: btnClear
                                text: "Clear"
                                padding: 10
                                onClicked: clearSelection()
                                background: Rectangle { radius: 10; color: "#2a3342" }
                                contentItem: Text {
                                    text: btnClear.text
                                    color: "#cbd5e1"; font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "ล้างการเลือกทั้งหมด"
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                id: addSelectedBtn
                                text: "Add Selected"
                                padding: 10
                                enabled: {
                                    for (var i=0;i<scanModel.count;i++){
                                        var it = scanModel.get(i)
                                        if (it.__selected && !isDupIp(it.ip) && !isDupName(it.name)) return true
                                    }
                                    return false
                                }
                                background: Rectangle {
                                    radius: 10
                                    color: addSelectedBtn.enabled ? colAccent : "#2a3342"
                                }
                                contentItem: Text {
                                    text: addSelectedBtn.text
                                    color: addSelectedBtn.enabled ? "#0b1118" : "#6b7280"
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    var added = 0
                                    for (var i = 0; i < scanModel.count; i++) {
                                        var it = scanModel.get(i)
                                        if (it.__selected && !isDupIp(it.ip) && !isDupName(it.name)) {

                                            var payloadObj = { Name: it.name, ip: it.ip }
                                            if (it.serial !== undefined && String(it.serial).trim().length > 0) {
                                                payloadObj.deviceUniqueId = String(it.serial).trim()
                                            }

                                            var payload = [ payloadObj ]
                                            var json = JSON.stringify({ payload })

                                            if (krakenmapval)
                                                krakenmapval.groupSetting("AddDevice", 0, json)

                                            added++
                                            scanModel.setProperty(i, "__selected", false)
                                        }
                                    }
                                    if (added > 0)
                                        toast.show("Added %1 device(s).".arg(added))
                                }
                            }

                            Label {
                                text: "Tip: Click on an item to select or deselect."
                                color: colSubtext; font.pixelSize: 12
                            }
                        }
                    }

                    Connections {
                        target: krakenmapval
                        function onDeviceFound(name, serial, ip, ping) {
                            scanModel.append({
                                name:    name,
                                serial:  serial,
                                ip:      ip,
                                ping:    ping,
                                __selected: false
                            })
                        }
                        function onScanFinished() {
                            btnScan.scanning = false
                            toast.show("Scan complete.")
                        }
                    }
                }
            }
        }
    }

    ApplyButtonPopupSettingDrawer {
        id: applyBtn
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 0
        anchors.bottomMargin: 0
        onClicked: {
            if (typeof popuppanel !== "undefined" && popuppanel)
                popuppanel.close()
        }
    }

    /* ===== Toast ===== */
    Rectangle {
        id: toast
        property bool showing: false
        function show(msg) { textItem.text = msg; showing = true; toastTimer.restart() }
        radius: 10
        color: "#111827"
        border.color: "#1f2937"
        opacity: showing ? 0.96 : 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 56
        width: Math.max(160, textItem.implicitWidth + 24)
        height: 36
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 180 } }

        Text {
            id: textItem
            anchors.centerIn: parent
            color: "#e5e7eb"
            font.pixelSize: 12
            text: ""
        }
        Timer {
            id: toastTimer
            interval: 1800; repeat: false
            onTriggered: toast.showing = false
        }
    }
}
