/// TopNetworkDrawer.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtQuick.Window 2.15

Drawer {
    id: topDrawer
    parent: Overlay.overlay
    width:  Overlay.overlay ? Overlay.overlay.width : Screen.width
    height: Math.min(Overlay.overlay ? Overlay.overlay.height : Screen.height, 800)

    edge: Qt.TopEdge
    modal: true
    interactive: true
    dragMargin: 0

    /* ===== Theme ===== */
    property color colBg:      "#0f1115"
    property color colBar:     "#12161c"
    property color colCard:    "#1a1f27"
    property color colHi:      "#202734"
    property color colBorder:  "#2a3444"
    property color colText:    "#e6edf3"
    property color colSub:     "#9aa6b2"
    property color colAccent:  "#00c896"
    property color colInfo:    "#2980b9"
    property color colWarn:    "#f39c12"
    property color colOk:      "#27ae60"
    property color colPurple:  "#8e44ad"
    property color colField:   "#212733"
    property color colFieldHi: "#2b3342"

    /* ===== Externals ===== */
    property var  krakenmapval: null
    property bool keyfreqEdit: false

    /* ===== Local cache from JSON ===== */
    property var netRows: []   // array ของ object: [{id,DHCP,IP_ADDRESS,...}, ...]

    /* ===== Mode: Basic / Advanced =====
       - Basic: ให้โชว์ Network 2 (index=1) เป็นหลัก (ยังคงมี nicCount=2 เพื่อให้ index 1 มีจริง)
       - Advanced: 4 adapters + ต้องใส่รหัสก่อนเข้า
    */
    property bool advancedMode: false
    property int  nicCount: advancedMode ? 4 : 2
    property int  selectedNic: 0

    property bool _dhcpChanging: false
    property bool _blockServerFieldSignal: false

    /* ===== Advanced password ===== */
    property string advancedPassword: "ifz8zean6969**"  // เปลี่ยนได้ตามต้องการ
    property bool _isSwitchingMode: false     // กัน loop ตอน set modeCombo เอง

    onAdvancedModeChanged: {
        nicCount = advancedMode ? 4 : 2

        // Basic: บังคับไป Network 2 (index=1) ถ้ามี
        if (!advancedMode) {
            selectedNic = (nicCount >= 2) ? 1 : 0
        } else {
            if (selectedNic < 0 || selectedNic >= nicCount)
                selectedNic = 0
        }

        refillFields()
    }

    /* ===== Adapter helper ===== */
    function hasFns() {
        return krakenmapval
                && typeof krakenmapval.netGet === "function"
                && typeof krakenmapval.netSet === "function"
    }

    // อ่านค่าจาก netRows (JSON) เป็นหลัก
    function g(i, k) {
        if (netRows && netRows.length > 0) {
            var rec = null
            var wantId = i + 1 // id 1..4

            for (var idx = 0; idx < netRows.length; ++idx) {
                if (netRows[idx].id === wantId) {
                    rec = netRows[idx]
                    break
                }
            }
            if (!rec && i < netRows.length)
                rec = netRows[i]

            if (rec) {
                switch (k) {
                case "useDHCP": return rec.DHCP
                case "ip":      return rec.IP_ADDRESS
                case "mask":    return rec.SUBNETMASK
                case "gw":      return rec.GATEWAY
                case "dns1":    return rec.PRIMARY_DNS
                case "dns2":    return rec.SECONDARY_DNS
                case "server":  return rec.krakenserver
                default: break
                }
            }
        }

        // fallback: krakenmapval arrays
        if (!krakenmapval) return ""
        if (hasFns())
            return krakenmapval.netGet(i, k)

        switch (k) {
        case "useDHCP": return (krakenmapval.useDHCPs && krakenmapval.useDHCPs[i]) || "on"
        case "ip":      return (krakenmapval.ipAddresses && krakenmapval.ipAddresses[i]) || ""
        case "mask":    return (krakenmapval.subnetMasks && krakenmapval.subnetMasks[i]) || ""
        case "gw":      return (krakenmapval.gateways && krakenmapval.gateways[i]) || ""
        case "dns1":    return (krakenmapval.dns1s && krakenmapval.dns1s[i]) || ""
        case "dns2":    return (krakenmapval.dns2s && krakenmapval.dns2s[i]) || ""
        case "server":  return (krakenmapval.serverKrakens && krakenmapval.serverKrakens[i]) || (krakenmapval.serverKraken || "")
        default: return ""
        }
    }

    // เขียนค่าลง netRows + ส่งต่อไป backend เดิม
    function s(i, k, v) {
        if (netRows && netRows.length > 0) {
            var rec = null
            var wantId = i + 1
            for (var idx = 0; idx < netRows.length; ++idx) {
                if (netRows[idx].id === wantId) {
                    rec = netRows[idx]
                    break
                }
            }
            if (!rec && i < netRows.length)
                rec = netRows[i]

            if (rec) {
                switch (k) {
                case "useDHCP": rec.DHCP          = v; break
                case "ip":      rec.IP_ADDRESS    = v; break
                case "mask":    rec.SUBNETMASK    = v; break
                case "gw":      rec.GATEWAY       = v; break
                case "dns1":    rec.PRIMARY_DNS   = v; break
                case "dns2":    rec.SECONDARY_DNS = v; break
                case "server":  rec.krakenserver  = v; break
                default: break
                }
            }
        }

        if (!krakenmapval) return
        if (hasFns()) {
            krakenmapval.netSet(i, k, v)
            return
        }

        function ensureArr(name) {
            if (!krakenmapval[name])
                krakenmapval[name] = new Array(nicCount).fill("")
        }

        switch (k) {
        case "useDHCP": ensureArr("useDHCPs");      krakenmapval.useDHCPs[i]      = v; break
        case "ip":      ensureArr("ipAddresses");   krakenmapval.ipAddresses[i]   = v; break
        case "mask":    ensureArr("subnetMasks");   krakenmapval.subnetMasks[i]   = v; break
        case "gw":      ensureArr("gateways");      krakenmapval.gateways[i]      = v; break
        case "dns1":    ensureArr("dns1s");         krakenmapval.dns1s[i]         = v; break
        case "dns2":    ensureArr("dns2s");         krakenmapval.dns2s[i]         = v; break
        case "server":  ensureArr("serverKrakens"); krakenmapval.serverKrakens[i] = v; break
        default: break
        }
    }

    function applyOne(i, dhcp, ip, mask, gw, d1, d2) {
        if (!krakenmapval) return
        if (typeof krakenmapval.updateNetworkfromDisplayIndex === "function") {
            krakenmapval.updateNetworkfromDisplayIndex(i, dhcp, ip, mask, gw, d1, d2)
        } else if (typeof krakenmapval.updateNetworkfromDisplay === "function") {
            if (krakenmapval.hasOwnProperty("currentNicIndex"))
                krakenmapval.currentNicIndex = i
            krakenmapval.updateNetworkfromDisplay(dhcp, ip, mask, gw, d1, d2)
        }
    }

    function restartOne(i) {
        if (!krakenmapval) return
        if (typeof krakenmapval.restartNetworkIndex === "function")
            krakenmapval.restartNetworkIndex(i)
        else
            console.log("Restart NIC", i, "not implemented")
    }

    function refillFields() {
        var i = selectedNic

        _dhcpChanging = true
        dhcpCombo.currentIndex = (g(i, "useDHCP") === "off") ? 1 : 0
        _dhcpChanging = false

        ipField.text   = ipField.originalValue   = g(i, "ip")
        maskField.text = maskField.originalValue = g(i, "mask")
        gwField.text   = gwField.originalValue   = g(i, "gw")
        dns1Field.text = dns1Field.originalValue = g(i, "dns1")
        dns2Field.text = dns2Field.originalValue = g(i, "dns2")
    }

    onSelectedNicChanged: {
        console.log("[TopNetworkDrawer] selectedNic changed ->", selectedNic)
        refillFields()
    }

    onVisibleChanged: {
        if (visible && krakenmapval) {
            // เปิด drawer แล้ว Basic ให้โชว์ NIC2 ก่อน
            if (!advancedMode) {
                selectedNic = (nicCount >= 2) ? 1 : 0
            }
            krakenmapval.getNetworkfromDb(selectedNic + 1)
        }
    }

    Component.onCompleted: {
        // ถ้าอยากให้ Basic เสมอ
        advancedMode = false
        selectedNic = 1

        if (mainWindows && mainWindows.updateNetworkToDisplay) {
            mainWindows.updateNetworkToDisplay.connect(function(str) {
                updateNetworkToDisplay(str)
            })
        }
    }

    function cidrToNetmask(prefix) {
        var p = Number(prefix)
        if (!isFinite(p) || p < 0 || p > 32) return ""
        var mask = p === 0 ? 0 : (0xFFFFFFFF << (32 - p)) >>> 0
        var a = (mask >>> 24) & 255
        var b = (mask >>> 16) & 255
        var c = (mask >>> 8)  & 255
        var d = mask & 255
        return a + "." + b + "." + c + "." + d
    }

    function splitIpCidr(ipCidr) {
        var ip = ""
        var mask = ""
        if (ipCidr && typeof ipCidr === "string") {
            var parts = ipCidr.split("/")
            ip = (parts[0] || "").trim()
            if (parts.length > 1) {
                var prefix = parts[1].trim()
                mask = cidrToNetmask(prefix)
            }
        }
        return { ip: ip, mask: mask }
    }

    function updateNetworkToDisplay(str) {
        try {
            var networkData = JSON.parse(str)

            var dns1 = ""
            var dns2 = ""
            if (networkData.dns) {
                var parts = networkData.dns.split(",")
                dns1 = parts.length > 0 ? parts[0].trim() : ""
                dns2 = parts.length > 1 ? parts[1].trim() : ""
            }

            var dhcpFlag = (networkData.mode === "static") ? "off" : "on"

            var ipInfo = splitIpCidr(networkData.ip)
            var ipOnly = ipInfo.ip
            var netmask = ipInfo.mask

            var index = 0
            if (networkData.iface === "enP8p1s0")      index = 0
            else if (networkData.iface === "enP1p1s0") index = 1
            else if (networkData.iface === "end0")     index = 2
            else if (networkData.iface === "end1")     index = 3

            s(index, "useDHCP", dhcpFlag)
            s(index, "ip",      ipOnly)
            s(index, "mask",    netmask)
            s(index, "gw",      networkData.gateway || "")
            s(index, "dns1",    dns1)
            s(index, "dns2",    dns2)

            if (index === selectedNic)
                refillFields()

            if (krakenmapval && typeof krakenmapval.updateNetworkfromDisplayIndex === "function") {
                krakenmapval.updateNetworkfromDisplayIndex(
                            index, dhcpFlag, ipOnly, netmask, networkData.gateway, dns1, dns2)
            }
        } catch (e) {
            console.error("JSON parse error:", e)
        }
    }

    /* ===== Password Popup for Advanced ===== */
    Popup {
        id: advPassPopup
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose

        width: 360
        height: 210
        x: Math.max(10, (topDrawer.width - width) / 2)
        y: Math.max(10, (topDrawer.height - height) / 3)

        background: Rectangle {
            radius: 14
            color: colCard
            border.color: colBorder
            border.width: 1
        }

        property string typed: ""

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            Text {
                text: "Advanced Mode"
                color: colText
                font.pixelSize: 18
                font.bold: true
            }

            Text {
                text: "Enter password to continue"
                color: colSub
                font.pixelSize: 13
            }

            TextField {
                id: advPassField
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                echoMode: TextInput.Password
                placeholderText: "Password"
                color: colText
                placeholderTextColor: colSub
                background: fieldBox.createObject(this, { "control": advPassField })
                leftPadding: 10
                rightPadding: 10
                font.pixelSize: 15

                onTextChanged: advPassPopup.typed = text
                Keys.onReturnPressed: okBtn.clicked()
            }

            Text {
                id: advError
                text: ""
                color: "#f87171"
                font.pixelSize: 12
                visible: text.length > 0
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    id: cancelBtn
                    text: "Cancel"
                    Layout.fillWidth: true
                    background: Rectangle {
                        radius: 10
                        color: cancelBtn.pressed ? Qt.darker(colInfo, 1.2) : colInfo
                    }
                    contentItem: Text {
                        text: cancelBtn.text
                        color: "white"
                        anchors.centerIn: parent
                        font.bold: true
                    }
                    onClicked: {
                        advError.text = ""
                        advPassField.text = ""
                        advPassPopup.close()

                        // กลับ Basic
                        _isSwitchingMode = true
                        modeCombo.currentIndex = 1
                        _isSwitchingMode = false

                        advancedMode = false
                    }
                }

                Button {
                    id: okBtn
                    text: "Unlock"
                    Layout.fillWidth: true
                    background: Rectangle {
                        radius: 10
                        color: okBtn.pressed ? Qt.darker(colAccent, 1.2) : colAccent
                    }
                    contentItem: Text {
                        text: okBtn.text
                        color: "white"
                        anchors.centerIn: parent
                        font.bold: true
                    }
                    onClicked: {
                        if (advPassPopup.typed === topDrawer.advancedPassword) {
                            advError.text = ""
                            advPassField.text = ""
                            advPassPopup.close()

                            advancedMode = true

                            _isSwitchingMode = true
                            modeCombo.currentIndex = 0
                            _isSwitchingMode = false
                        } else {
                            advError.text = "Wrong password"
                        }
                    }
                }
            }
        }

        onOpened: {
            advError.text = ""
            advPassPopup.typed = ""
            advPassField.text = ""
            advPassField.forceActiveFocus()
        }
    }

    Connections {
        target: krakenmapval

        function onNetworkRowUpdated(row) {
            if (row.all) {
                var obj = JSON.parse(row.all)
                netRows = obj.rows || []

                // nicCount ตามโหมด
                nicCount = advancedMode ? netRows.length : Math.max(2, Math.min(2, netRows.length))

                // Basic: ล็อคไป NIC2 (index=1) ถ้ามี
                if (!advancedMode) {
                    selectedNic = (netRows.length >= 2) ? 1 : 0
                } else {
                    if (row.id !== undefined && row.id > 0 && row.id <= netRows.length)
                        selectedNic = row.id - 1
                }

                refillFields()
            } else {
                // legacy
                if ((selectedNic + 1) !== row.id) return

                ipField.text   = row.IP_ADDRESS
                maskField.text = row.SUBNETMASK
                gwField.text   = row.GATEWAY
                dns1Field.text = row.PRIMARY_DNS
                dns2Field.text = row.SECONDARY_DNS
                dhcpCombo.currentIndex = (row.DHCP === "off") ? 1 : 0
            }
        }

        function onUpdateServeripDfserver(ip) {
            serverField.text = ip
        }

        function onUpdateGlobalOffsets(offsetValue, compassOffset) {
            // ✅ ทศนิยม 6 ตำแหน่ง
            compassField.text = Number(compassOffset).toFixed(6)
            compassField.originalValue = compassField.text
        }
    }

    background: Rectangle { color: colBg }

    /* ===== Helpers ===== */
    Component {
        id: fieldBox
        Rectangle {
            property var control: null
            radius: 8
            color: control && control.activeFocus ? colFieldHi : colField
            border.color: colBorder
            border.width: 1
        }
    }

    Component {
        id: fieldText
        Text {
            color: colSub
            font.pixelSize: 13
            elide: Text.ElideRight
        }
    }

    /* ========= TOP + CONTENT ========= */
    Column {
        id: topArea
        anchors.fill: parent

        /* ===== TOP BAR ===== */
        Rectangle {
            id: topBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 64
            color: colBar

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16

                /* === Left section: Title + Mode === */
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "Network Configuration"
                        color: colText
                        font.pixelSize: 20
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                    }

                    ComboBox {
                        id: modeCombo
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 44
                        model: ["Advanced", "Basic"]

                        Component.onCompleted: {
                            _isSwitchingMode = true
                            currentIndex = 1
                            _isSwitchingMode = false
                            advancedMode = false
                        }

                        // ✅ Advanced ต้องถามรหัสก่อน
                        onCurrentIndexChanged: {
                            if (_isSwitchingMode) return

                            if (currentIndex === 0) {
                                // ผู้ใช้เลือก Advanced -> เปิด popup แล้ว "ยังไม่เปลี่ยน advancedMode"
                                _isSwitchingMode = true
                                modeCombo.currentIndex = 1
                                _isSwitchingMode = false

                                advPassPopup.open()
                            } else {
                                // Basic
                                advancedMode = false
                            }
                        }

                        contentItem: Text {
                            text: modeCombo.currentText
                            color: colText
                            font.pixelSize: 16
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignLeft
                            leftPadding: 10
                            anchors.fill: parent
                            elide: Text.ElideRight
                        }

                        delegate: ItemDelegate {
                            width: modeCombo.width
                            contentItem: Text {
                                text: modelData
                                font.pixelSize: 14
                                color: control.down ? "#ffffff" : "#000000"
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignLeft
                                leftPadding: 10
                                anchors.fill: parent
                            }
                            background: Rectangle {
                                color: control.highlighted ? "#cccccc" : "#e0e0e0"
                            }
                        }

                        background: Rectangle {
                            radius: 8
                            color: colField
                            border.color: colBorder
                            border.width: 1
                        }

                        indicator: Rectangle {
                            width: 14
                            height: 14
                            color: "transparent"
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter

                            Canvas {
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.beginPath()
                                    ctx.moveTo(0, 0)
                                    ctx.lineTo(width, 0)
                                    ctx.lineTo(width/2, height)
                                    ctx.closePath()
                                    ctx.fillStyle = colText
                                    ctx.fill()
                                }
                            }
                        }
                    }
                }

                /* === Right section: Adapter Pills (Advanced only) === */
                RowLayout {
                    spacing: 12
                    visible: advancedMode

                    /* ===== Group 1: Display (Adapter 0,1) ===== */
                    Rectangle {
                        radius: 12
                        color: colCard
                        border.color: colBorder
                        border.width: 1
                        Layout.preferredHeight: displayCol.implicitHeight + 16
                        Layout.preferredWidth: Math.max(220, displayCol.implicitWidth + 16)

                        ColumnLayout {
                            id: displayCol
                            anchors.margins: 8
                            anchors.fill: parent
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text { text: "Display"; color: colText; font.pixelSize: 14; font.bold: true }
                                Rectangle { Layout.fillWidth: true; height: 1; color: colBorder }

                                Repeater {
                                    model: Math.min(2, nicCount)
                                    delegate: MouseArea {
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        property int nicIndex: index
                                        Layout.preferredWidth: pill.implicitWidth
                                        Layout.preferredHeight: pill.implicitHeight
                                        onClicked: selectedNic = nicIndex

                                        Rectangle {
                                            id: pill
                                            radius: 14
                                            color: selectedNic === nicIndex ? colHi : "transparent"
                                            border.width: 1
                                            border.color: selectedNic === nicIndex ? colAccent : colBorder
                                            implicitHeight: 28
                                            implicitWidth: Math.max(120, label.implicitWidth + 24)
                                            Behavior on color { ColorAnimation { duration: 120 } }

                                            Text {
                                                id: label
                                                anchors.centerIn: parent
                                                text: {
                                                    if (topDrawer.netRows && topDrawer.netRows.length > nicIndex) {
                                                        var r = topDrawer.netRows[nicIndex]
                                                        if (r && r.phyName) return r.phyName
                                                    }
                                                    return "Adapter " + nicIndex
                                                }
                                                color: selectedNic === nicIndex ? colText : colSub
                                                font.pixelSize: 13
                                                font.bold: selectedNic === nicIndex
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    /* ===== Group 2: DF Device (Adapter 2,3) ===== */
                    Rectangle {
                        radius: 12
                        color: colCard
                        border.color: colBorder
                        border.width: 1
                        Layout.preferredHeight: dfCol.implicitHeight + 16
                        Layout.preferredWidth: Math.max(220, dfCol.implicitWidth + 16)

                        ColumnLayout {
                            id: dfCol
                            anchors.margins: 8
                            anchors.fill: parent
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text { text: "DF Device"; color: colText; font.pixelSize: 14; font.bold: true }
                                Rectangle { Layout.fillWidth: true; height: 1; color: colBorder }

                                Repeater {
                                    model: Math.max(0, Math.min(2, nicCount - 2))
                                    delegate: MouseArea {
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        property int nicIndex: index + 2
                                        Layout.preferredWidth: pill1.implicitWidth
                                        Layout.preferredHeight: pill1.implicitHeight
                                        onClicked: selectedNic = nicIndex

                                        Rectangle {
                                            id: pill1
                                            radius: 14
                                            color: selectedNic === nicIndex ? colHi : "transparent"
                                            border.width: 1
                                            border.color: selectedNic === nicIndex ? colAccent : colBorder
                                            implicitHeight: 28
                                            implicitWidth: Math.max(120, label1.implicitWidth + 24)
                                            Behavior on color { ColorAnimation { duration: 120 } }

                                            Text {
                                                id: label1
                                                anchors.centerIn: parent
                                                text: {
                                                    if (topDrawer.netRows && topDrawer.netRows.length > nicIndex) {
                                                        var r = topDrawer.netRows[nicIndex]
                                                        if (r && r.phyName) return r.phyName
                                                    }
                                                    return "Adapter " + nicIndex
                                                }
                                                color: selectedNic === nicIndex ? colText : colSub
                                                font.pixelSize: 13
                                                font.bold: selectedNic === nicIndex
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

        /* ===== CONTENT ===== */
        ScrollView {
            id: scroller
            anchors.top: topBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            contentItem: Column {
                id: contentCol
                spacing: 18
                width: Math.min(1200, scroller.width - 48)
                x: Math.max(0, (scroller.width - width) / 2)

                /* --- Card 1: Adapter Settings (Network only) --- */
                Rectangle {
                    id: adapterCard
                    width: parent.width
                    radius: 14
                    color: colCard
                    border.color: colBorder
                    implicitHeight: adapterCol.implicitHeight + 32

                    ColumnLayout {
                        id: adapterCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: advancedMode ? "Adapter Settings" : "Adapter Settings (Basic: Network 2)"
                                color: colText
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                visible: !advancedMode
                                text: "Using: Adapter 1 (Network 2)"
                                color: colSub
                                font.pixelSize: 13
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: colBorder }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Loader {
                                    sourceComponent: fieldText
                                    Layout.preferredWidth: 100
                                    onLoaded: { item.text = "Mode" }
                                }

                                ComboBox {
                                    id: dhcpCombo
                                    Layout.preferredWidth: 240
                                    Layout.preferredHeight: 44
                                    model: ["Automatic (DHCP)", "Static (Manual)"]

                                    Component.onCompleted: {
                                        _dhcpChanging = true
                                        currentIndex = (g(selectedNic, "useDHCP") === "off") ? 1 : 0
                                        _dhcpChanging = false
                                    }

                                    onCurrentIndexChanged: {
                                        if (_dhcpChanging) return
                                        s(selectedNic, "useDHCP", (currentIndex === 0) ? "on" : "off")
                                    }

                                    background: fieldBox.createObject(this, { "control": dhcpCombo })

                                    contentItem: Text {
                                        text: dhcpCombo.displayText
                                        color: colText
                                        font.pixelSize: 16
                                        anchors.fill: parent
                                        horizontalAlignment: Text.AlignLeft
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                        leftPadding: 10
                                        rightPadding: 26
                                    }

                                    indicator: Rectangle {
                                        width: 14
                                        height: 14
                                        color: "transparent"
                                        anchors.right: parent.right
                                        anchors.rightMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter

                                        Canvas {
                                            anchors.fill: parent
                                            onPaint: {
                                                var ctx = getContext("2d")
                                                ctx.reset()
                                                ctx.beginPath()
                                                ctx.moveTo(0, 0)
                                                ctx.lineTo(width, 0)
                                                ctx.lineTo(width/2, height)
                                                ctx.closePath()
                                                ctx.fillStyle = colText
                                                ctx.fill()
                                            }
                                        }
                                    }
                                }
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                rowSpacing: 10
                                columnSpacing: 20

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader { sourceComponent: fieldText; onLoaded: { item.text = "IP Address" } }
                                    TextField {
                                        id: ipField
                                        property string originalValue: ""
                                        placeholderText: "192.168.1.100"
                                        enabled: dhcpCombo.currentIndex === 1
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                                        background: fieldBox.createObject(this, { "control": ipField })
                                        color: colText
                                        placeholderTextColor: colSub
                                        leftPadding: 10
                                        rightPadding: 10
                                        topPadding: 4
                                        bottomPadding: 4
                                        font.pixelSize: 15
                                        onTextChanged: s(selectedNic, "ip", text)
                                        onCursorVisibleChanged: {
                                            keyfreqEdit = cursorVisible
                                            if (cursorVisible && focus) selectAll()
                                            focus = cursorVisible
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader { sourceComponent: fieldText; onLoaded: { item.text = "Subnet Mask" } }
                                    TextField {
                                        id: maskField
                                        property string originalValue: ""
                                        placeholderText: "255.255.255.0"
                                        enabled: dhcpCombo.currentIndex === 1
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                                        background: fieldBox.createObject(this, { "control": maskField })
                                        color: colText
                                        placeholderTextColor: colSub
                                        leftPadding: 10
                                        rightPadding: 10
                                        topPadding: 4
                                        bottomPadding: 4
                                        font.pixelSize: 15
                                        onTextChanged: s(selectedNic, "mask", text)
                                        onCursorVisibleChanged: {
                                            keyfreqEdit = cursorVisible
                                            if (cursorVisible && focus) selectAll()
                                            focus = cursorVisible
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader { sourceComponent: fieldText; onLoaded: { item.text = "Gateway" } }
                                    TextField {
                                        id: gwField
                                        property string originalValue: ""
                                        placeholderText: "192.168.1.1"
                                        enabled: dhcpCombo.currentIndex === 1
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                                        background: fieldBox.createObject(this, { "control": gwField })
                                        color: colText
                                        placeholderTextColor: colSub
                                        leftPadding: 10
                                        rightPadding: 10
                                        topPadding: 4
                                        bottomPadding: 4
                                        font.pixelSize: 15
                                        onTextChanged: s(selectedNic, "gw", text)
                                        onCursorVisibleChanged: {
                                            keyfreqEdit = cursorVisible
                                            if (cursorVisible && focus) selectAll()
                                            focus = cursorVisible
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader { sourceComponent: fieldText; onLoaded: { item.text = "Primary DNS" } }
                                    TextField {
                                        id: dns1Field
                                        property string originalValue: ""
                                        placeholderText: "8.8.8.8"
                                        enabled: dhcpCombo.currentIndex === 1
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                                        background: fieldBox.createObject(this, { "control": dns1Field })
                                        color: colText
                                        placeholderTextColor: colSub
                                        leftPadding: 10
                                        rightPadding: 10
                                        topPadding: 4
                                        bottomPadding: 4
                                        font.pixelSize: 15
                                        onTextChanged: s(selectedNic, "dns1", text)
                                        onCursorVisibleChanged: {
                                            keyfreqEdit = cursorVisible
                                            if (cursorVisible && focus) selectAll()
                                            focus = cursorVisible
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader { sourceComponent: fieldText; onLoaded: { item.text = "Secondary DNS" } }
                                    TextField {
                                        id: dns2Field
                                        property string originalValue: ""
                                        placeholderText: "1.1.1.1"
                                        enabled: dhcpCombo.currentIndex === 1
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                                        background: fieldBox.createObject(this, { "control": dns2Field })
                                        color: colText
                                        placeholderTextColor: colSub
                                        leftPadding: 10
                                        rightPadding: 10
                                        topPadding: 4
                                        bottomPadding: 4
                                        font.pixelSize: 15
                                        onTextChanged: s(selectedNic, "dns2", text)
                                        onCursorVisibleChanged: {
                                            keyfreqEdit = cursorVisible
                                            if (cursorVisible && focus) selectAll()
                                            focus = cursorVisible
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Button {
                                    id: applyBtn
                                    text: advancedMode ? "Apply (Adapter)" : "Apply"
                                    Layout.fillWidth: true
                                    background: Rectangle {
                                        radius: 10
                                        color: applyBtn.pressed ? Qt.darker(colAccent, 1.2) : colAccent
                                    }
                                    contentItem: Text {
                                        text: applyBtn.text
                                        color: "white"
                                        anchors.centerIn: parent
                                        font.bold: true
                                    }
                                    onClicked: {
                                        if (!krakenmapval) return
                                        var dhcp = (dhcpCombo.currentIndex === 0) ? "on" : "off"
                                        applyOne(selectedNic, dhcp,
                                                 ipField.text,
                                                 maskField.text,
                                                 gwField.text,
                                                 dns1Field.text,
                                                 dns2Field.text)

                                        if (mainWindows && typeof mainWindows.setNetworkFormDisplay === "function") {
                                            mainWindows.setNetworkFormDisplay(selectedNic, dhcp,
                                                                              ipField.text,
                                                                              gwField.text,
                                                                              dns1Field.text + " " + dns2Field.text)
                                        }

                                        ipField.originalValue   = ipField.text
                                        maskField.originalValue = maskField.text
                                        gwField.originalValue   = gwField.text
                                        dns1Field.originalValue = dns1Field.text
                                        dns2Field.originalValue = dns2Field.text
                                    }
                                }

                                Button {
                                    id: restartBtn
                                    text: advancedMode ? "Restart (Adapter)" : "Restart"
                                    Layout.fillWidth: true
                                    background: Rectangle {
                                        radius: 10
                                        color: restartBtn.pressed ? Qt.darker(colInfo, 1.2) : colInfo
                                    }
                                    contentItem: Text {
                                        text: restartBtn.text
                                        color: "white"
                                        anchors.centerIn: parent
                                        font.bold: true
                                    }
                                    onClicked: restartOne(selectedNic)
                                }
                            }
                        }
                    }
                }

                /* --- Card 2: Service Endpoints + Global Offsets --- */
                Rectangle {
                    width: parent.width
                    radius: 14
                    color: colCard
                    border.color: colBorder
                    implicitHeight: svcCol.implicitHeight + 32

                    ColumnLayout {
                        id: svcCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Service Endpoints"
                                color: colText
                                font.pixelSize: 16
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Rectangle { height: 1; Layout.fillWidth: true; color: colBorder }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 7
                            rowSpacing: 10
                            columnSpacing: 20

                            ColumnLayout {
                                Layout.row: 0
                                Layout.column: 0
                                Layout.fillWidth: true

                                Loader { sourceComponent: fieldText; onLoaded: { item.text = "DF Server IP" } }

                                TextField {
                                    id: serverField
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
                                    placeholderText: "192.168.10.200"
                                    validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                                    background: fieldBox.createObject(this, { "control": serverField })
                                    color: colText
                                    placeholderTextColor: colSub
                                    leftPadding: 10
                                    rightPadding: 10
                                    topPadding: 4
                                    bottomPadding: 4
                                    font.pixelSize: 15

                                    onTextChanged: {
                                        if (_blockServerFieldSignal) return
                                        s(selectedNic, "server", text)
                                    }

                                    onCursorVisibleChanged: {
                                        keyfreqEdit = cursorVisible
                                        if (cursorVisible && focus) selectAll()
                                        focus = cursorVisible
                                    }
                                }
                            }

                            Button {
                                id: applySvcBtn
                                Layout.row: 0
                                Layout.column: 1
                                Layout.preferredWidth: 120
                                Layout.preferredHeight: 44
                                Layout.alignment: Qt.AlignVCenter
                                Layout.topMargin: 16
                                text: "Apply"

                                background: Rectangle {
                                    radius: 8
                                    color: applySvcBtn.pressed ? Qt.darker(colWarn, 1.2) : colWarn
                                }
                                contentItem: Text {
                                    text: applySvcBtn.text
                                    color: "white"
                                    anchors.centerIn: parent
                                    font.pixelSize: 14
                                    font.bold: true
                                }

                                onClicked: {
                                    if (!krakenmapval) return
                                    try {
                                        if (mainWindows && typeof mainWindows.setNetworkFormDisplay === "function")
                                            mainWindows.setNetworkFormDisplay(serverField.text)

                                        if (typeof krakenmapval.connectToDFserver === "function")
                                            krakenmapval.connectToDFserver(serverField.text)
                                    } catch(e) {
                                        console.log("[APPLY] call FAILED:", e)
                                    }
                                }
                            }

                            Button {
                                id: reconnectBtn
                                Layout.row: 0
                                Layout.column: 3
                                Layout.preferredWidth: 160
                                Layout.preferredHeight: 44
                                Layout.alignment: Qt.AlignVCenter
                                Layout.topMargin: 16
                                text: "Reconnect"

                                background: Rectangle {
                                    radius: 8
                                    color: reconnectBtn.pressed ? Qt.darker(colOk, 1.2) : colOk
                                }

                                contentItem: Text {
                                    text: reconnectBtn.text
                                    color: "white"
                                    anchors.centerIn: parent
                                    font.pixelSize: 14
                                    font.bold: true
                                }

                                onClicked: {
                                    if (krakenmapval && typeof krakenmapval.connectToserverKraken === "function")
                                        krakenmapval.connectToserverKraken(serverField.text)
                                }
                            }
                        }

                        Rectangle { height: 1; Layout.fillWidth: true; color: colBorder }

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Global Offsets"
                                color: colText
                                font.pixelSize: 16
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 20

                            ColumnLayout {
                                Layout.fillWidth: true
                                Loader { sourceComponent: fieldText; onLoaded: { item.text = "Compass Offset" } }

                                TextField {
                                    id: compassField
                                    property string originalValue: ""
                                    // ✅ ทศนิยม 6 ตำแหน่ง
                                    text: (krakenmapval && krakenmapval.compassOffset !== undefined)
                                          ? Number(krakenmapval.compassOffset).toFixed(6) : ""
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
                                    background: fieldBox.createObject(this, { "control": compassField })
                                    color: colText
                                    placeholderTextColor: colSub
                                    leftPadding: 10
                                    rightPadding: 10
                                    topPadding: 4
                                    bottomPadding: 4
                                    font.pixelSize: 15
                                    onCursorVisibleChanged: {
                                        keyfreqEdit = cursorVisible
                                        if (cursorVisible && focus) selectAll()
                                        focus = cursorVisible
                                    }
                                }
                            }

                            Button {
                                id: setCompassBtn
                                text: "Set Compass Offset"
                                Layout.preferredWidth: 200
                                Layout.preferredHeight: 44
                                Layout.topMargin: 16
                                Layout.alignment: Qt.AlignVCenter

                                background: Rectangle {
                                    radius: 10
                                    color: setCompassBtn.pressed ? Qt.darker(colPurple, 1.2) : colPurple
                                }
                                contentItem: Text {
                                    text: setCompassBtn.text
                                    color: "white"
                                    anchors.centerIn: parent
                                    font.bold: true
                                }

                                onClicked: {
                                    if (!krakenmapval) return
                                    var v = parseFloat(compassField.text)
                                    if (isNaN(v)) return
                                    if (typeof krakenmapval.setCompassOffset === "function")
                                        krakenmapval.setCompassOffset(v)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
