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
    property var krakenmapval: null
    property bool keyfreqEdit: false

    /* ===== Local cache from JSON ===== */
    property var netRows: []   // array ของ object: [{id,DHCP,IP_ADDRESS,...}, ...]

    /* ===== Mode: Basic / Advanced ===== */
    property bool advancedMode: true
    property int  nicCount: advancedMode ? 4 : 1
    property int  selectedNic: 0
    property bool _dhcpChanging: false
    property bool _blockServerFieldSignal: false

    onAdvancedModeChanged: {
        nicCount = advancedMode ? 4 : 1
        if (!advancedMode && selectedNic !== 0)
            selectedNic = 0
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
        // ----- อ่านจาก netRows ก่อน -----
        if (netRows && netRows.length > 0) {
            var rec = null
            var wantId = i + 1   // id 1..4 map กับ selectedNic 0..3

            for (var idx = 0; idx < netRows.length; ++idx) {
                if (netRows[idx].id === wantId) {
                    rec = netRows[idx]
                    break
                }
            }
            // ถ้าไม่เจอ ใช้ index ตรง ๆ เผื่อ backend ส่งมาไม่ตรง id
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
                // iscreen / isubnet / igw ยังไม่มีใน JSON ตอนนี้ ใช้ fallback ด้านล่าง
                }
            }
        }

        // ----- fallback: style เดิม (krakenmapval arrays) -----
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
        case "iscreen": return (krakenmapval.iScreenIps && krakenmapval.iScreenIps[i]) || (krakenmapval.iScreenIp || "")
        case "isubnet": return (krakenmapval.serverKrakensubnets && krakenmapval.serverKrakensubnets[i]) || (krakenmapval.serverKrakensubnet || "")
        case "igw":     return (krakenmapval.serverKrakengateways && krakenmapval.serverKrakengateways[i]) || (krakenmapval.serverKrakengateway || "")
        default: return ""
        }
    }

    // เขียนค่าลง netRows + ส่งต่อไป backend เดิม
    function s(i, k, v) {
        // ----- update ใน netRows ก่อน -----
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
                }
            }
        }

        // ----- ส่งต่อไป backend เหมือนเดิม -----
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
        case "useDHCP": ensureArr("useDHCPs");             krakenmapval.useDHCPs[i]             = v; break
        case "ip":      ensureArr("ipAddresses");          krakenmapval.ipAddresses[i]          = v; break
        case "mask":    ensureArr("subnetMasks");          krakenmapval.subnetMasks[i]          = v; break
        case "gw":      ensureArr("gateways");             krakenmapval.gateways[i]             = v; break
        case "dns1":    ensureArr("dns1s");                krakenmapval.dns1s[i]                = v; break
        case "dns2":    ensureArr("dns2s");                krakenmapval.dns2s[i]                = v; break
        case "server":  ensureArr("serverKrakens");        krakenmapval.serverKrakens[i]        = v; break
        case "iscreen": ensureArr("iScreenIps");           krakenmapval.iScreenIps[i]           = v; break
        case "isubnet": ensureArr("serverKrakensubnets");  krakenmapval.serverKrakensubnets[i]  = v; break
        case "igw":     ensureArr("serverKrakengateways"); krakenmapval.serverKrakengateways[i] = v; break
        }
    }

    function applyOne(i, dhcp, ip, mask, gw, d1, d2){
        if(!krakenmapval) return
        if(typeof krakenmapval.updateNetworkfromDisplayIndex==="function")
            krakenmapval.updateNetworkfromDisplayIndex(i, dhcp, ip, mask, gw, d1, d2)
        else if(typeof krakenmapval.updateNetworkfromDisplay==="function"){
            if(krakenmapval.hasOwnProperty("currentNicIndex")) krakenmapval.currentNicIndex=i
            krakenmapval.updateNetworkfromDisplay(dhcp, ip, mask, gw, d1, d2)
        }
    }

    function restartOne(i){
        if(!krakenmapval) return
        if(typeof krakenmapval.restartNetworkIndex==="function")
            krakenmapval.restartNetworkIndex(i)
        else
            console.log("Restart NIC", i, "not implemented")
    }

    function applyAll(){
        if (!advancedMode) {
            var dhcp0 = (dhcpCombo.currentIndex===0) ? "on" : "off"
            applyOne(0, dhcp0, g(0,"ip"), g(0,"mask"), g(0,"gw"), g(0,"dns1"), g(0,"dns2"))
            return
        }
        for (var i=0; i<nicCount; ++i) {
            var dhcp = (i===selectedNic)
                       ? ((dhcpCombo.currentIndex===0) ? "on" : "off")
                       : (g(i,"useDHCP")==="off" ? "off" : "on")
            applyOne(i, dhcp, g(i,"ip"), g(i,"mask"), g(i,"gw"), g(i,"dns1"), g(i,"dns2"))
        }
    }

    function refillFields() {
        const i = selectedNic

        _dhcpChanging = true
        dhcpCombo.currentIndex = (g(i, "useDHCP") === "off") ? 1 : 0
        _dhcpChanging = false

        ipField.text      = ipField.originalValue      = g(i, "ip")
        maskField.text    = maskField.originalValue    = g(i, "mask")
        gwField.text      = gwField.originalValue      = g(i, "gw")
        dns1Field.text    = dns1Field.originalValue    = g(i, "dns1")
        dns2Field.text    = dns2Field.originalValue    = g(i, "dns2")
        // serverField.text  = serverField.originalValue  = g(i, "server")
        // iscreenField.text = iscreenField.originalValue = g(i, "iscreen")
        isubnetField.text = isubnetField.originalValue = g(i, "isubnet")
        igwField.text     = igwField.originalValue     = g(i, "igw")
    }

    onSelectedNicChanged: {
        console.log("[TopNetworkDrawer] selectedNic changed ->", selectedNic)
        refillFields()
    }

    onVisibleChanged: {
        if (visible && krakenmapval) {
            console.log("[TopNetworkDrawer] visible=true, selectedNic =", selectedNic)
            // ขอข้อมูล fresh จาก C++ (ซึ่งจะส่ง JSON rows ทั้งหมดกลับมา)
            // C++ side: getNetworkfromDb(int id) -> emit networkRowUpdated(row)
            // row["all"] = fullJsonString
            krakenmapval.getNetworkfromDb(selectedNic + 1)
            krakenmapval.getRecorderSettings()
        }
    }

    Component.onCompleted: {
        console.log("Component.onCompleted:profilesFromDb")
        mainWindows.updateNetworkToDisplay.connect(function(str) {
            updateNetworkToDisplay(str)
        })
    }

    function cidrToNetmask(prefix) {
        var p = Number(prefix)
        if (!isFinite(p) || p < 0 || p > 32) return ""

        var mask = p === 0 ? 0 : (0xFFFFFFFF << (32 - p)) >>> 0  // >>>0 = unsigned
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

    function updateNetworkToDisplay(str){
        try {
            var networkData = JSON.parse(str)

            // ===== split DNS only =====
            var dns1 = ""
            var dns2 = ""
            if (networkData.dns) {
                var parts = networkData.dns.split(",")
                dns1 = parts.length > 0 ? parts[0].trim() : ""
                dns2 = parts.length > 1 ? parts[1].trim() : ""
            }

            // ===== mode mapping (static -> off, dhcp -> on) =====
            var dhcpFlag = (networkData.mode === "static") ? "off" : "on"

            // ===== split IP/CIDR -> ip + netmask =====
            var ipInfo = splitIpCidr(networkData.ip)
            var ipOnly = ipInfo.ip
            var netmask = ipInfo.mask

            // ===== iface -> index =====
            var index = 0
            if (networkData.iface === "enP8p1s0")      index = 0
            else if (networkData.iface === "enP1p1s0") index = 1
            else if (networkData.iface === "end0")     index = 2
            else if (networkData.iface === "end1")     index = 3

            console.log("iface =", networkData.iface)
            console.log("ipOnly =", ipOnly)
            console.log("netmask =", netmask)
            console.log("gateway =", networkData.gateway)
            console.log("dns1 =", dns1)
            console.log("dns2 =", dns2)
            console.log("dhcpFlag =", dhcpFlag)

            // =========================================================
            // ✅ (NEW) อัปเดตค่าในตัวแปรปัจจุบัน (cache) ด้วย
            //     - update netRows (ถ้ามี) + update krakenmapval arrays
            // =========================================================
            s(index, "useDHCP", dhcpFlag)
            s(index, "ip",      ipOnly)
            s(index, "mask",    netmask)
            s(index, "gw",      networkData.gateway || "")
            s(index, "dns1",    dns1)
            s(index, "dns2",    dns2)

            // ถ้า adapter ที่ถูกอัปเดตเป็นตัวที่กำลังโชว์อยู่ -> refresh fields
            if (index === selectedNic) {
                refillFields()
            }

            // =========================================================
            // ✅ ส่งเข้า C++ แบบแยก ip/mask แล้ว (เหมือนเดิม)
            // =========================================================
            krakenmapval.updateNetworkfromDisplayIndex(
                index,
                dhcpFlag,
                ipOnly,
                netmask,
                networkData.gateway,
                dns1,
                dns2
            )

        } catch (e) {
            console.error("JSON parse error:", e)
        }

        console.log("updateNetworkToDisplay raw =", str)
    }



    Connections {
        target: krakenmapval
        function onNetworkRowUpdated(row) {
            console.log("[TopNetworkDrawer] onNetworkRowUpdated: row.id =", row.id)

            if (row.all) {
                var obj = JSON.parse(row.all)
                netRows = obj.rows || []
                nicCount = netRows.length

                console.log("[TopNetworkDrawer] netRows length =", netRows.length)
                if (row.id !== undefined && row.id > 0 && row.id <= nicCount) {
                    selectedNic = row.id - 1
                }

                refillFields()
            } else {
                console.log("[TopNetworkDrawer] no .all in row (legacy mode)")
                if ((selectedNic + 1) !== row.id)
                    return

                ipField.text   = row.IP_ADDRESS
                maskField.text = row.SUBNETMASK
                gwField.text   = row.GATEWAY
                dns1Field.text = row.PRIMARY_DNS
                dns2Field.text = row.SECONDARY_DNS
                dhcpCombo.currentIndex = (row.DHCP === "off") ? 1 : 0
            }
        }
        function onUpdateServeripDfserver(ip){
            serverField.text = ip
        }
        function onRecorderSettings(alsaDevice, clientIp, frequency, rtspServer, rtspUrl, rtspPort) {
            console.log("Recorder Settings Loaded:", alsaDevice, clientIp, frequency)

            alsaDeviceField.text = alsaDevice
            clientIpField.text = clientIp
            recFreqField.text = String(frequency)
            rtspServerField.text = rtspServer
            rtspUriField.text = rtspUrl
            rtspPortField.text = String(rtspPort)
        }
        function onUpdateGlobalOffsets(offsetValue, compassOffset) {
            offsetField.text = String(offsetValue)
            compassField.text = Number(compassOffset).toFixed(3)
            offsetField.originalValue = offsetField.text
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
        // spacing: 0

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
                        Layout.preferredHeight: 44        // <<< เพิ่มความสูง แต่สีเดิมทั้งหมด

                        model: ["Advanced", "Basic"]

                        Component.onCompleted: {
                            currentIndex = 1   // Basic
                            advancedMode = false
                        }

                        onCurrentIndexChanged: {
                            advancedMode = (currentIndex === 0)
                        }

                        // ==== ข้อความในกล่องหลัก ====
                        contentItem: Text {
                            text: modeCombo.currentText
                            color: colText                    // <<< สีเดิม
                            font.pixelSize: 16
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignLeft
                            leftPadding: 10
                            anchors.fill: parent
                            elide: Text.ElideRight
                        }

                        // ==== dropdown delegate ====
                        delegate: ItemDelegate {
                            id: modeDelegate
                            width: modeCombo.width

                            property alias control: modeDelegate

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

                        // ==== background (สีเดิม) ====
                        background: Rectangle {
                            radius: 8
                            color: colField
                            border.color: colBorder
                            border.width: 1
                        }

                        // ==== indicator (▼ สามเหลี่ยม) ====
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

                                Text {
                                    text: "Display"
                                    color: colText
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: colBorder
                                }
                                Repeater {
                                    model: Math.min(2, nicCount)
                                    delegate: MouseArea {
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        property int nicIndex: index    // 0,1
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
                                                        if (r && r.phyName)
                                                            return r.phyName
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

                                Text {
                                    text: "DF Device"
                                    color: colText
                                    font.pixelSize: 14
                                    font.bold: true
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: colBorder
                                }
                                Repeater {
                                    // adapter 2,3 → ถ้า nicCount น้อยกว่านั้นก็จะลดจำนวนให้เอง
                                    model: Math.max(0, Math.min(2, nicCount - 2))
                                    delegate: MouseArea {
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        property int nicIndex: index + 2   // 2,3
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
                                                        if (r && r.phyName)
                                                            return r.phyName
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

                /* --- Card 1: Adapter / Recorder Settings --- */
                Rectangle {
                    id: adapterCard
                    width: parent.width
                    radius: 14
                    color: colCard
                    border.color: colBorder
                    // 0 = Network, 1 = Recorder
                    property int pageMode: 0
                    implicitHeight: adapterCol.implicitHeight + 32

                    ColumnLayout {
                        id: adapterCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true

                            // ชื่อการ์ด เปลี่ยนตามโหมด
                            Text {
                                text: adapterCard.pageMode === 0
                                      ? "Adapter Settings"
                                      : "Recorder Settings"
                                color: colText
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            // === สวิตช์แท็บแบบสวย ๆ (Network / Recorder) ===
                            Rectangle {
                                id: modeSwitch
                                Layout.preferredWidth: 220
                                Layout.preferredHeight: 34
                                radius: 17
                                color: colField
                                border.color: colBorder
                                border.width: 1

                                // ตัวไฮไลท์เลื่อนซ้าย-ขวา
                                Rectangle {
                                    id: modeHighlight
                                    y: 2
                                    height: parent.height - 4
                                    width: (parent.width - 4) / 2
                                    radius: height / 2
                                    x: adapterCard.pageMode === 0 ? 2 : (parent.width / 2)
                                    color: colAccent

                                    Behavior on x {
                                        NumberAnimation {
                                            duration: 160
                                            easing.type: Easing.InOutQuad
                                        }
                                    }
                                }

                                Row {
                                    anchors.fill: parent
                                    anchors.margins: 0

                                    // --- Network ---
                                    MouseArea {
                                        id: networkTab
                                        width: parent.width / 2
                                        height: parent.height
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor

                                        onClicked: {
                                            if (adapterCard.pageMode !== 0)
                                                adapterCard.pageMode = 0
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "Network"
                                            font.pixelSize: 13
                                            font.bold: adapterCard.pageMode === 0
                                            color: adapterCard.pageMode === 0 ? "white" : colText
                                        }
                                    }

                                    // --- Recorder ---
                                    MouseArea {
                                        id: recorderTab
                                        width: parent.width / 2
                                        height: parent.height
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor

                                        onClicked: {
                                            if (adapterCard.pageMode !== 1)
                                                adapterCard.pageMode = 1
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "Recorder"
                                            font.pixelSize: 13
                                            font.bold: adapterCard.pageMode === 1
                                            color: adapterCard.pageMode === 1 ? "white" : colText
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: colBorder }

                        /* ==================== โหมด Network (ของเดิม) ==================== */
                        ColumnLayout {
                            id: networkSettingsCol
                            Layout.fillWidth: true
                            spacing: 12
                            visible: adapterCard.pageMode === 0

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
                                        if (_dhcpChanging)
                                            return
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

                                /* IP */
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader {
                                        sourceComponent: fieldText
                                        onLoaded: { item.text = "IP Address" }
                                    }
                                    TextField {
                                        id: ipField
                                        property string originalValue: ""
                                        placeholderText: "192.168.1.100"
                                        enabled: dhcpCombo.currentIndex === 1
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34      // <<< กำหนดความสูงให้เตี้ยลง
                                        validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                                        background: fieldBox.createObject(this, { "control": ipField })
                                        color: colText
                                        placeholderTextColor: colSub
                                        leftPadding: 10
                                        rightPadding: 10
                                        topPadding: 4                    // <<< บีบ padding ด้านบน
                                        bottomPadding: 4                 // <<< บีบ padding ด้านล่าง
                                        font.pixelSize: 15
                                        onTextChanged: s(selectedNic, "ip", text)
                                        onCursorVisibleChanged: {
                                            keyfreqEdit = cursorVisible
                                            if (cursorVisible && focus) selectAll()
                                            focus = cursorVisible
                                        }
                                    }
                                }

                                /* Mask */
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader {
                                        sourceComponent: fieldText
                                        onLoaded: { item.text = "Subnet Mask" }
                                    }
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

                                /* Gateway */
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader {
                                        sourceComponent: fieldText
                                        onLoaded: { item.text = "Gateway" }
                                    }
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

                                /* DNS1 */
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader {
                                        sourceComponent: fieldText
                                        onLoaded: { item.text = "Primary DNS" }
                                    }
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

                                /* DNS2 */
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader {
                                        sourceComponent: fieldText
                                        onLoaded: { item.text = "Secondary DNS" }
                                    }
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

                            /* ปุ่ม Network */
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
                                        let dhcp = (dhcpCombo.currentIndex === 0) ? "on" : "off"
                                        console.log("[Apply] ONE nic =", selectedNic,"dhcp=", dhcp,"IP=", ipField.text,"mask=", maskField.text , "gw=", gwField.text , "dns=", dns1Field.text , " dns2=",dns2Field.text)
                                        applyOne(selectedNic, dhcp,
                                                 ipField.text,
                                                 maskField.text,
                                                 gwField.text,
                                                 dns1Field.text,
                                                 dns2Field.text)
                                        mainWindows.setNetworkFormDisplay(selectedNic, dhcp,
                                                                          ipField.text,
                                                                          gwField.text,
                                                                          dns1Field.text+" "+dns2Field.text)
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

                                // Button {
                                //     id: applyAllBtn
                                //     visible: advancedMode
                                //     text: "Apply All (4 adapters)"
                                //     Layout.fillWidth: true
                                //     background: Rectangle {
                                //         radius: 10
                                //         color: applyAllBtn.pressed ? Qt.darker(colPurple, 1.2) : colPurple
                                //     }
                                //     contentItem: Text {
                                //         text: applyAllBtn.text
                                //         color: "white"
                                //         anchors.centerIn: parent
                                //         font.bold: true
                                //     }
                                //     onClicked: {
                                //         for (var i = 0; i < nicCount; ++i) {
                                //             var dhcpVal = (i === selectedNic)
                                //                           ? ((dhcpCombo.currentIndex === 0) ? "on" : "off")
                                //                           : ((g(i, "useDHCP") === "off") ? "off" : "on")

                                //             var ipVal   = (i === selectedNic) ? ipField.text   : g(i, "ip")
                                //             var maskVal = (i === selectedNic) ? maskField.text : g(i, "mask")
                                //             var gwVal   = (i === selectedNic) ? gwField.text   : g(i, "gw")
                                //             var d1Val   = (i === selectedNic) ? dns1Field.text : g(i, "dns1")
                                //             var d2Val   = (i === selectedNic) ? dns2Field.text : g(i, "dns2")

                                //             console.log("[Apply] ALL: nic=", i,
                                //                         "dhcp=", dhcpVal,
                                //                         "IP=", ipVal,
                                //                         "mask=", maskVal,
                                //                         "gw=", gwVal,
                                //                         "dns1=", d1Val,
                                //                         "dns2=", d2Val)
                                //         }
                                //         applyAll()
                                //     }
                                // }
                            }
                        }

                        /* ==================== โหมด Recorder ==================== */
                        ColumnLayout {
                            id: recorderSettingsCol
                            Layout.fillWidth: true
                            spacing: 12
                            visible: adapterCard.pageMode === 1

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                rowSpacing: 10
                                columnSpacing: 20

                                /* ALSA Device */
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader {
                                        sourceComponent: fieldText
                                        onLoaded: { item.text = "ALSA Device" }
                                    }
                                    TextField {
                                        id: alsaDeviceField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        placeholderText: "recin1"
                                        text: (krakenmapval && krakenmapval.recAlsaDevice !== undefined)
                                              ? krakenmapval.recAlsaDevice : ""
                                        background: fieldBox.createObject(this, { "control": alsaDeviceField })
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

                                /* Client IP */
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader {
                                        sourceComponent: fieldText
                                        onLoaded: { item.text = "Client IP" }
                                    }
                                    TextField {
                                        id: clientIpField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        placeholderText: "10.0.25.1"
                                        text: (krakenmapval && krakenmapval.recClientIp !== undefined)
                                              ? krakenmapval.recClientIp : ""
                                        validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                                        background: fieldBox.createObject(this, { "control": clientIpField })
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

                                /* Frequency */
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader {
                                        sourceComponent: fieldText
                                        onLoaded: { item.text = "Frequency" }
                                    }
                                    TextField {
                                        id: recFreqField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        placeholderText: "0"
                                        text: (krakenmapval && krakenmapval.recFrequency !== undefined)
                                              ? String(krakenmapval.recFrequency) : "0"
                                        inputMethodHints: Qt.ImhDigitsOnly
                                        validator: IntValidator { bottom: 0; top: 1000000000 }
                                        background: fieldBox.createObject(this, { "control": recFreqField })
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

                                /* RTSP Server */
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader {
                                        sourceComponent: fieldText
                                        onLoaded: { item.text = "RTSP Server" }
                                    }
                                    TextField {
                                        id: rtspServerField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        placeholderText: "192.168.10.31"
                                        text: (krakenmapval && krakenmapval.recRtspServer !== undefined)
                                              ? krakenmapval.recRtspServer : ""
                                        validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                                        background: fieldBox.createObject(this, { "control": rtspServerField })
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

                                /* RTSP URI */
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader {
                                        sourceComponent: fieldText
                                        onLoaded: { item.text = "RTSP URI" }
                                    }
                                    TextField {
                                        id: rtspUriField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        placeholderText: "igate1"
                                        text: (krakenmapval && krakenmapval.recRtspUri !== undefined)
                                              ? krakenmapval.recRtspUri : ""
                                        background: fieldBox.createObject(this, { "control": rtspUriField })
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

                                /* RTSP Port */
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Loader {
                                        sourceComponent: fieldText
                                        onLoaded: { item.text = "RTSP Port" }
                                    }
                                    TextField {
                                        id: rtspPortField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        placeholderText: "0"
                                        text: (krakenmapval && krakenmapval.recRtspPort !== undefined)
                                              ? String(krakenmapval.recRtspPort) : "0"
                                        inputMethodHints: Qt.ImhDigitsOnly
                                        validator: IntValidator { bottom: 0; top: 65535 }
                                        background: fieldBox.createObject(this, { "control": rtspPortField })
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
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Button {
                                    id: applyRecBtn
                                    text: "Apply Recorder Settings"
                                    Layout.fillWidth: true
                                    background: Rectangle {
                                        radius: 10
                                        color: applyRecBtn.pressed ? Qt.darker(colAccent, 1.2) : colAccent
                                    }
                                    contentItem: Text {
                                        text: applyRecBtn.text
                                        color: "white"
                                        anchors.centerIn: parent
                                        font.bold: true
                                    }
                                    onClicked: {
                                        if (!krakenmapval)
                                            return

                                        var freqVal = parseInt(recFreqField.text)
                                        var portVal = parseInt(rtspPortField.text)
                                        if (isNaN(freqVal)) freqVal = 0
                                        if (isNaN(portVal)) portVal = 0

                                        if (typeof krakenmapval.setRecorderSettings === "function") {
                                            krakenmapval.setRecorderSettings(
                                                        alsaDeviceField.text,
                                                        clientIpField.text,
                                                        freqVal,
                                                        rtspServerField.text,
                                                        rtspUriField.text,
                                                        portVal)
                                        } else {
                                            console.warn("setRecorderSettings() not implemented in krakenmapval")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                /* --- Card 2: Service Endpoints + Global Offsets (merged) --- */
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
                                Loader {
                                    sourceComponent: fieldText
                                    onLoaded: { item.text = "DF Server IP" }
                                }
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
                            // ColumnLayout {
                            //     Layout.row: 0
                            //     Layout.column: 1
                            //     Layout.fillWidth: true
                            //     Loader {
                            //         sourceComponent: fieldText
                            //         onLoaded: { item.text = "iScreen IP" }
                            //     }
                            //     TextField {
                            //         id: iscreenField
                            //         Layout.fillWidth: true
                            //         Layout.preferredHeight: 34
                            //         placeholderText: "192.168.10.50"
                            //         validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                            //         background: fieldBox.createObject(this, { "control": iscreenField })
                            //         color: colText
                            //         placeholderTextColor: colSub
                            //         leftPadding: 10
                            //         rightPadding: 10
                            //         topPadding: 4
                            //         bottomPadding: 4
                            //         font.pixelSize: 15
                            //         onTextChanged: s(selectedNic, "iscreen", text)
                            //         onCursorVisibleChanged: {
                            //             keyfreqEdit = cursorVisible
                            //             if (cursorVisible && focus) selectAll()
                            //             focus = cursorVisible
                            //         }
                            //     }
                            // }
                            // ColumnLayout {
                            //     Layout.row: 0
                            //     Layout.column: 2
                            //     Layout.fillWidth: true
                            //     Loader {
                            //         sourceComponent: fieldText
                            //         onLoaded: { item.text = "Subnet" }
                            //     }
                            //     TextField {
                            //         id: isubnetField
                            //         Layout.fillWidth: true
                            //         Layout.preferredHeight: 34
                            //         placeholderText: "255.255.255.0"
                            //         validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                            //         background: fieldBox.createObject(this, { "control": isubnetField })
                            //         color: colText
                            //         placeholderTextColor: colSub
                            //         leftPadding: 10
                            //         rightPadding: 10
                            //         topPadding: 4
                            //         bottomPadding: 4
                            //         font.pixelSize: 15
                            //         onTextChanged: s(selectedNic, "isubnet", text)
                            //         onCursorVisibleChanged: {
                            //             keyfreqEdit = cursorVisible
                            //             if (cursorVisible && focus) selectAll()
                            //             focus = cursorVisible
                            //         }
                            //     }
                            // }
                            // ColumnLayout {
                            //     Layout.row: 0
                            //     Layout.column: 3
                            //     Layout.fillWidth: true
                            //     Loader {
                            //         sourceComponent: fieldText
                            //         onLoaded: { item.text = "Gateway" }
                            //     }
                            //     TextField {
                            //         id: igwField
                            //         Layout.fillWidth: true
                            //         Layout.preferredHeight: 34
                            //         placeholderText: "192.168.10.1"
                            //         validator: RegExpValidator { regExp: /^(\d{1,3}\.){3}\d{1,3}$/ }
                            //         background: fieldBox.createObject(this, { "control": igwField })
                            //         color: colText
                            //         placeholderTextColor: colSub
                            //         leftPadding: 10
                            //         rightPadding: 10
                            //         topPadding: 4
                            //         bottomPadding: 4
                            //         font.pixelSize: 15
                            //         onTextChanged: s(selectedNic, "igw", text)
                            //         onCursorVisibleChanged: {
                            //             keyfreqEdit = cursorVisible
                            //             if (cursorVisible && focus) selectAll()
                            //             focus = cursorVisible
                            //         }
                            //     }
                            // }

                            /* ================= BUTTON: APPLY ================= */
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
                                    color: applySvcBtn.pressed
                                           ? Qt.darker(colWarn, 1.2)
                                           : colWarn
                                }

                                contentItem: Text {
                                    text: applySvcBtn.text
                                    color: "white"
                                    anchors.centerIn: parent
                                    font.pixelSize: 14
                                    font.bold: true
                                }

                                onClicked: {
                                    console.log("[APPLY] clicked")
                                    console.log("[APPLY] krakenmapval =", krakenmapval)
                                    console.log("[APPLY] typeof connectToDFserver =", (krakenmapval ? typeof krakenmapval.connectToDFserver : "null"))
                                    console.log("[APPLY] server =", serverField.text)

                                    if (!krakenmapval) return

                                    try {
                                        mainWindows.setNetworkFormDisplay(serverField.text)
                                        krakenmapval.connectToDFserver(serverField.text)
                                        console.log("[APPLY] call connectToDFserver() OK")
                                    } catch(e) {
                                        console.log("[APPLY] call FAILED:", e)
                                    }
                                }

                            }

                            // /* ================= BUTTON: RESTART KRAKEN ================= */
                            // Button {
                            //     id: restartKrakenBtn
                            //     Layout.row: 0
                            //     Layout.column: 2
                            //     Layout.preferredWidth: 180
                            //     Layout.preferredHeight: 44
                            //     Layout.alignment: Qt.AlignVCenter
                            //     Layout.topMargin: 16

                            //     text: "Restart Kraken"

                            //     background: Rectangle {
                            //         radius: 8
                            //         color: restartKrakenBtn.pressed
                            //                ? Qt.darker(colInfo, 1.2)
                            //                : colInfo
                            //     }

                            //     contentItem: Text {
                            //         text: restartKrakenBtn.text
                            //         color: "white"
                            //         anchors.centerIn: parent
                            //         font.pixelSize: 14
                            //         font.bold: true
                            //     }

                            //     onClicked: {
                            //         if (krakenmapval &&
                            //             typeof krakenmapval.RestartKraken === "function")
                            //             krakenmapval.RestartKraken("true")
                            //     }
                            // }

                            /* ================= BUTTON: RECONNECT ================= */
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
                                    color: reconnectBtn.pressed
                                           ? Qt.darker(colOk, 1.2)
                                           : colOk
                                }

                                contentItem: Text {
                                    text: reconnectBtn.text
                                    color: "white"
                                    anchors.centerIn: parent
                                    font.pixelSize: 14
                                    font.bold: true
                                }

                                onClicked: {
                                    if (krakenmapval &&
                                        typeof krakenmapval.connectToserverKraken === "function")
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

                            /* ----- Offset Value ----- */
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: false
                                Loader {
                                    sourceComponent: fieldText
                                    onLoaded: { item.text = "Offset Value" }
                                }
                                TextField {
                                    id: offsetField
                                    property string originalValue: ""
                                    text: (krakenmapval && krakenmapval.offset !== undefined)
                                          ? krakenmapval.offset.toString() : ""
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
                                    background: fieldBox.createObject(this, { "control": offsetField })
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

                            /* ----- Set Offset ----- */
                            Button {
                                id: setOffsetBtn
                                text: "Set Offset"
                                Layout.preferredWidth: 160
                                Layout.preferredHeight: 46
                                Layout.topMargin: 16
                                Layout.alignment: Qt.AlignVCenter
                                visible: false

                                background: Rectangle {
                                    radius: 10
                                    color: setOffsetBtn.pressed ? Qt.darker(colPurple, 1.2) : colPurple
                                }
                                contentItem: Text {
                                    text: setOffsetBtn.text
                                    color: "white"
                                    anchors.centerIn: parent
                                    font.bold: true
                                }
                                onClicked: {
                                    let v = parseFloat(offsetField.text)
                                    if (!isNaN(v) && krakenmapval && typeof krakenmapval.setOffset === "function") {
                                        krakenmapval.setOffset(v)
                                        offsetField.originalValue = offsetField.text
                                    }
                                }
                            }

                            /* ----- Compass Offset ----- */
                            ColumnLayout {
                                Layout.fillWidth: true
                                Loader {
                                    sourceComponent: fieldText
                                    onLoaded: { item.text = "Compass Offset" }
                                }
                                TextField {
                                    id: compassField
                                    property string originalValue: ""
                                    text: (krakenmapval && krakenmapval.compassOffset !== undefined)
                                          ? krakenmapval.compassOffset.toFixed(3) : ""
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

                            /* ----- Switch Peak-Hold ----- */
                            // ColumnLayout {
                            //     Layout.alignment: Qt.AlignVCenter
                            //     Loader {
                            //         sourceComponent: fieldText
                            //         onLoaded: {
                            //             item.text = "Enable Peak-Hold"
                            //             item.font.pixelSize = 12
                            //         }
                            //     }
                            //     Switch {
                            //         id: peakSwitch
                            //         Layout.alignment: Qt.AlignHCenter
                            //         checked: krakenmapval ? (krakenmapval.spectrumPeakHold || false) : false
                            //     }
                            // }

                            /* ----- Set Compass Offset ----- */
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

                                    let v = parseFloat(compassField.text)
                                    if (isNaN(v)) return

                                    // ✅ ส่ง compass offset ไป C++ อย่างเดียว
                                    if (typeof krakenmapval.setCompassOffset === "function") {
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
}
