import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import App 1.0

Item {
    id: networkManager
    width: 1920
    height: 1080
    clip: true

    // KP-6JUL2026 : build flag controls only the top-bar drawer tab.
    // The complete Network Settings page remains available regardless of this flag.
    readonly property bool topNetworkDrawerEnabled:
        (typeof FeatureTopNetworkDrawer === "undefined")
        ? false
        : FeatureTopNetworkDrawer

    // Runtime proof: this must appear when the real Network page is loaded.
    // Keep it during integration; remove after the device test is accepted.
    Component.onCompleted: {
        console.log("NETWORK_UI_RUNTIME_PROOF_TABS_WIFI_5G_LOADED qrc:/Setting.qml")
        loadLanInterfaces()
    }

    property string selectedTab: "lan"
    onSelectedTabChanged: {
        if (selectedTab === "lan")
            loadLanInterfaces()
    }

    // LAN backend contract: keep existing NetworkController function calls.
    property string interfaceName: ""
    property bool useDhcp: true
    property string ipAddress: ""
    property string netmask: ""
    property string gateway: ""
    property string primaryDns: ""
    property string secondaryDns: ""
    property string statusMessage: ""
    property var lanInterfaces: []
    property var lanByIface: ({})

    // LAN fields are now populated from the real C++ NetworkController snapshot.
    property string lanMacAddress: ""
    property string lanLinkStatus: "Unknown"
    property string lanSpeed: "-"
    property string lanDuplex: "-"

    property bool hardwareHasWireless: (typeof HardwareHasWireless === "undefined") ? true : HardwareHasWireless
    property bool hardwareHasWifi: (typeof HardwareHasWifi === "undefined") ? true : HardwareHasWifi
    property bool hardwareHas5G: (typeof HardwareHas5G === "undefined") ? true : HardwareHas5G

    QtObject {
        id: ui
        property color bg: "#07101a"
        property color topBar: "#101010"
        property color mainPanel: "#101a26"
        property color panel: "#132235"
        property color card: "#122033"
        property color field: "#0d1723"
        property color border: "#2d4056"
        property color borderSoft: "#203044"
        property color text: "#e9f0f7"
        property color subText: "#9aa8b8"
        property color muted: "#667589"
        property color accent: "#00c9a7"
        property color warning: "#f59e0b"
        property color danger: "#ef4444"
        property color disabled: "#718096"
    }

    function safeText(value, fallback) {
        if (value === undefined || value === null || value === "")
            return fallback
        return String(value)
    }

    function backendAvailable() {
        return (typeof NetworkController !== "undefined" && NetworkController)
    }

    function cidrToNetmask(cidr) {
        var bits = parseInt(cidr)
        if (isNaN(bits) || bits < 0 || bits > 32)
            return ""
        var mask = (0xFFFFFFFF << (32 - bits)) >>> 0
        var out = []
        var shifts = [24, 16, 8, 0]
        for (var i = 0; i < shifts.length; ++i)
            out.push((mask >>> shifts[i]) & 255)
        return out.join(".")
    }

    function netmaskToCidr(mask) {
        var parts = String(mask).split(".")
        if (parts.length !== 4)
            return 24
        var binary = ""
        for (var i = 0; i < 4; ++i) {
            var n = parseInt(parts[i])
            if (isNaN(n) || n < 0 || n > 255)
                return 24
            binary += ("00000000" + n.toString(2)).slice(-8)
        }
        var count = 0
        for (var j = 0; j < binary.length; ++j) {
            if (binary.charAt(j) === "1")
                count++
        }
        return count
    }

    function readLanValue(info, keys, fallback) {
        if (!info)
            return fallback
        for (var i = 0; i < keys.length; ++i) {
            var key = keys[i]
            if (info[key] !== undefined && info[key] !== null && String(info[key]).length > 0)
                return String(info[key])
        }
        return fallback
    }

    function cloneLanInfo(info) {
        var out = {}
        if (!info)
            return out
        for (var k in info)
            out[k] = info[k]
        out.iface = readLanValue(out, ["iface", "interface"], "")
        out.interface = out.iface
        out.name = readLanValue(out, ["name", "label", "key"], out.iface)
        return out
    }

    function currentLanInfo() {
        if (lanByIface && lanByIface[interfaceName])
            return lanByIface[interfaceName]
        return null
    }

    function normalizeLanIndex(index) {
        if (lanInterfaces.length > index)
            return lanInterfaces[index].iface
        return lanInterfaces.length > 0 ? lanInterfaces[0].iface : ""
    }

    function lanDisplayName() {
        var info = currentLanInfo()
        return info ? readLanValue(info, ["name", "label", "key"], interfaceName) : interfaceName
    }

    function lanInfoStatusText(info) {
        var st = readLanValue(info, ["status", "linkStatus", "link", "state", "operstate"], "")
        if (st.length > 0 && st !== "Unknown")
            return st
        var ip = readLanValue(info, ["ip", "liveIp", "current_ip", "dev_ip4_plain"], "")
        if (ip.length > 0)
            return "Configured"
        return "Unknown"
    }

    function lanInfoStatusColor(info) {
        var status = lanInfoStatusText(info).toLowerCase()
        if (status.indexOf("connected") >= 0 || status.indexOf("up") >= 0 || status.indexOf("configured") >= 0)
            return ui.accent
        if (status.indexOf("no cable") >= 0 || status.indexOf("down") >= 0 || status.indexOf("disconnect") >= 0)
            return ui.warning
        return ui.disabled
    }

    function lanStatusColor() {
        return lanInfoStatusColor(currentLanInfo())
    }

    function lanStatusText() {
        return lanInfoStatusText(currentLanInfo())
    }

    function replaceLanInfo(iface, incoming) {
        if (!iface || !incoming)
            return

        var map = {}
        for (var key in lanByIface)
            map[key] = cloneLanInfo(lanByIface[key])

        var merged = map[iface] ? cloneLanInfo(map[iface]) : {}
        for (var k in incoming)
            merged[k] = incoming[k]
        merged.iface = iface
        merged.interface = iface
        if (!merged.name || merged.name.length === 0)
            merged.name = iface
        map[iface] = merged

        var list = []
        for (var i = 0; i < lanInterfaces.length; ++i) {
            var row = cloneLanInfo(lanInterfaces[i])
            list.push(row.iface === iface ? merged : row)
        }
        if (list.length === 0)
            list.push(merged)

        lanByIface = map
        lanInterfaces = list
    }

    function applyLanInfo(info) {
        if (!info)
            return

        var iface = readLanValue(info, ["iface", "interface"], interfaceName)
        if (iface.length > 0)
            interfaceName = iface

        useDhcp = readLanValue(info, ["mode", "ipv4_method"], "dhcp").toLowerCase() !== "static"

        var ipRaw = readLanValue(info, ["ip", "liveIp", "current_ip", "dev_ip4_plain", "configuredIp"], "")
        if (ipRaw.length > 0 && ipRaw.indexOf("/") >= 0) {
            var ipParts = ipRaw.split("/")
            ipAddress = ipParts[0]
            netmask = cidrToNetmask(ipParts[1])
        } else {
            ipAddress = ipRaw
            netmask = readLanValue(info, ["netmask", "liveNetmask", "current_netmask", "dev_ip4_netmask"], netmask)
        }

        gateway = readLanValue(info, ["gateway", "liveGateway", "current_gateway", "dev_ip4_gateway", "configuredGateway"], "")
        primaryDns = readLanValue(info, ["dns", "primaryDns"], "")
        secondaryDns = readLanValue(info, ["dns2", "secondaryDns"], "")
        lanMacAddress = readLanValue(info, ["mac", "macAddress", "address"], "")
        lanLinkStatus = readLanValue(info, ["status", "linkStatus", "link", "state", "operstate"], "Unknown")
        lanSpeed = readLanValue(info, ["speed", "linkSpeed"], "-")
        lanDuplex = readLanValue(info, ["duplex"], "-")
    }

    function loadLanInterfaces() {
        if (!backendAvailable()) {
            statusMessage = "NetworkController is not available"
            return
        }

        var result = NetworkController.loadAllLanConfig()
        var list = []
        var map = {}

        if (result && result.lanList && result.lanList.length !== undefined) {
            for (var i = 0; i < result.lanList.length; ++i) {
                var item = cloneLanInfo(result.lanList[i])
                if (item.iface.length === 0)
                    continue
                list.push(item)
                map[item.iface] = item
            }
        } else if (result && result.lan) {
            var keys = Object.keys(result.lan)
            keys.sort()
            for (var j = 0; j < keys.length; ++j) {
                var key = keys[j]
                var row = cloneLanInfo(result.lan[key])
                if (row.iface.length === 0)
                    continue
                row.key = key
                row.name = row.name.length > 0 ? row.name : key.toUpperCase()
                list.push(row)
                map[row.iface] = row
            }
        }

        if (list.length === 0) {
            statusMessage = "No LAN interface found"
            return
        }

        lanInterfaces = list
        lanByIface = map

        if (interfaceName.length === 0 || !lanByIface[interfaceName])
            interfaceName = list[0].iface

        applyLanInfo(lanByIface[interfaceName])
    }

    function refreshDhcpInfo() {
        if (!backendAvailable()) {
            statusMessage = "NetworkController is not available"
            return
        }
        var info = NetworkController.queryDhcpInfo(interfaceName)
        if (info) {
            replaceLanInfo(interfaceName, info)
            applyLanInfo(lanByIface[interfaceName])
        }
    }

    function loadLanSetting() {
        if (!backendAvailable()) {
            statusMessage = "NetworkController is not available"
            return
        }

        if (interfaceName.length === 0) {
            loadLanInterfaces()
            return
        }

        var config = NetworkController.loadConfig(interfaceName)
        if (config) {
            replaceLanInfo(interfaceName, config)
            applyLanInfo(lanByIface[interfaceName])
        }
    }

    function selectLan(iface) {
        interfaceName = iface
        loadLanSetting()
    }

    function applyLanSetting() {
        if (!backendAvailable()) {
            statusMessage = "NetworkController is not available"
            return
        }
        var cidr = netmaskToCidr(netmask)
        var ipWithCidr = ipAddress + "/" + cidr
        var dns = primaryDns + (secondaryDns.length > 0 ? "," + secondaryDns : "")
        statusMessage = "Saving network config..."
        NetworkController.applyNetworkConfig(interfaceName,
                                             useDhcp ? "dhcp" : "static",
                                             ipWithCidr,
                                             gateway,
                                             dns)
    }

    function requestProtectedLanApply() {
        // KP-6JUL2026 : Do not call the mutating backend until password verification succeeds.
        lanApplyPasswordPopup.requestUnlock()
    }

    NetworkPasswordPopup {
        id: lanApplyPasswordPopup
        titleText: "Network Settings"
        messageText: "Enter password to apply LAN configuration"
        accentColor: ui.accent
        backgroundColor: ui.card
        borderColor: ui.border
        textColor: ui.text
        subTextColor: ui.subText
        fieldColor: ui.field
        fieldFocusColor: "#16283d"

        onAuthorized: networkManager.applyLanSetting()
    }

    Connections {
        target: NetworkController
        function onApplyNetworkConfigFinished(iface, ok, message, gatewayValue, dnsValue) {
            statusMessage = message
            if (iface === interfaceName)
                loadLanSetting()
        }
        function onApplyNetworkConfigNmcliFinished(iface, ok, message) {
            statusMessage = message
            if (iface === interfaceName)
                loadLanInterfaces()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: ui.bg
    }

    Rectangle {
        id: topBar
        x: 0
        y: 0
        width: parent.width
        height: 60
        color: ui.topBar

        Rectangle {
            x: 18
            y: 10
            width: 42
            height: 42
            radius: 21
            color: "#17212e"
            border.color: ui.border
            Repeater {
                model: 3
                Rectangle {
                    x: 13
                    y: 13 + index * 8
                    width: 16
                    height: 3
                    radius: 2
                    color: ui.accent
                }
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 25
            width: 54
            height: 6
            radius: 3
            color: ui.accent
            visible: networkManager.topNetworkDrawerEnabled
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 30
            y: 10
            width: 620
            horizontalAlignment: Text.AlignRight
            text: "Latitude 0.000000°N Longitude 0.000000°E Altitude 0.00m"
            color: ui.accent
            font.pixelSize: 13
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 30
            y: 34
            width: 620
            horizontalAlignment: Text.AlignRight
            text: "MGRS: -   UTM: -"
            color: ui.accent
            font.pixelSize: 13
        }
    }

    Text {
        x: 36
        y: 94
        text: "Network Settings"
        color: ui.text
        font.pixelSize: 34
        font.bold: true
    }

    Text {
        x: 36
        y: 136
        text: "Unified LAN, WiFi, and 5G settings. Runtime proof target: qrc:/Setting.qml"
        color: ui.subText
        font.pixelSize: 15
    }

    Text {
        x: 36
        y: 176
        text: selectedTab === "lan" ? "LAN Interface Settings" : selectedTab === "wifi" ? "WiFi Settings" : "5G Modem Settings"
        color: ui.text
        font.pixelSize: 40
        font.bold: true
    }

    Row {
        x: 1240
        y: 176
        spacing: 14

        Repeater {
            model: [
                { key: "lan", label: "LAN", enabled: true },
                { key: "wifi", label: "WiFi", enabled: networkManager.hardwareHasWireless && networkManager.hardwareHasWifi },
                { key: "cellular", label: "5G", enabled: networkManager.hardwareHasWireless && networkManager.hardwareHas5G }
            ]
            Button {
                width: 130
                height: 48
                enabled: modelData.enabled
                text: modelData.label
                onClicked: selectedTab = modelData.key
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? (selectedTab === modelData.key ? "#001412" : ui.text) : ui.disabled
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 16
                    font.bold: true
                }
                background: Rectangle {
                    radius: 12
                    color: !parent.enabled ? "#0d1723" : selectedTab === modelData.key ? ui.accent : "#0d1723"
                    border.color: selectedTab === modelData.key ? ui.accent : ui.border
                    border.width: 1
                }
            }
        }
    }

    Rectangle {
        id: mainPanel
        x: 36
        y: 270
        width: parent.width - 72
        height: parent.height - 330
        radius: 18
        color: ui.mainPanel
        border.color: ui.border
        border.width: 1

        Item {
            id: lanPage
            anchors.fill: parent
            visible: selectedTab === "lan"

            Rectangle {
                x: 34
                y: 45
                width: 420
                height: parent.height - 90
                radius: 18
                color: ui.panel
                border.color: ui.border

                Text { x: 28; y: 28; text: "LAN Interfaces"; color: ui.text; font.pixelSize: 28; font.bold: true }

                Repeater {
                    model: lanInterfaces
                    Rectangle {
                        x: 28
                        y: 88 + index * 116
                        width: 364
                        height: 96
                        radius: 14
                        color: interfaceName === modelData.iface ? "#16283d" : "#0d1723"
                        border.color: interfaceName === modelData.iface ? ui.accent : ui.border
                        border.width: interfaceName === modelData.iface ? 2 : 1

                        MouseArea { anchors.fill: parent; onClicked: selectLan(modelData.iface) }
                        Rectangle { x: 24; y: 24; width: 14; height: 14; radius: 7; color: lanInfoStatusColor(modelData) }
                        Text { x: 52; y: 16; text: safeText(modelData.name, modelData.iface); color: ui.text; font.pixelSize: 22; font.bold: true }
                        Text { x: 52; y: 46; text: modelData.iface + " · " + lanInfoStatusText(modelData); color: ui.subText; font.pixelSize: 14 }
                        Text { x: 52; y: 70; text: safeText(modelData.ip || modelData.liveIp || modelData.current_ip || modelData.dev_ip4_plain, "--"); color: ui.text; font.pixelSize: 16; font.bold: true }
                    }
                }
            }

            Rectangle {
                x: 484
                y: 45
                width: parent.width - 518
                height: parent.height - 90
                radius: 18
                color: ui.panel
                border.color: ui.border

                Text { x: 30; y: 28; text: "Selected Interface: " + lanDisplayName() + " / " + interfaceName; color: ui.text; font.pixelSize: 28; font.bold: true }

                Rectangle {
                    x: parent.width - 170
                    y: 30
                    width: 130
                    height: 32
                    radius: 16
                    color: "#0d302e"
                    border.color: lanStatusColor()
                    Text { anchors.centerIn: parent; text: lanStatusText(); color: lanStatusColor(); font.pixelSize: 13; font.bold: true }
                }

                Row {
                    x: 30
                    y: 100
                    spacing: 24

                    Repeater {
                        model: [
                            { label: "Link", value: safeText(lanSpeed, "-"), sub: safeText(lanDuplex, "Full duplex") },
                            { label: "Mode", value: useDhcp ? "DHCP" : "Static", sub: useDhcp ? "Automatic IPv4" : "Manual IPv4" },
                            { label: "IPv4", value: safeText(ipAddress, "No IP"), sub: safeText(gateway, "No gateway") }
                        ]

                        Rectangle {
                            width: 396
                            height: 125
                            radius: 14
                            color: ui.card
                            border.color: ui.border

                            Rectangle {
                                x: 18
                                y: 18
                                width: 14
                                height: 14
                                radius: 7
                                color: ui.accent
                            }

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.leftMargin: 44
                                anchors.rightMargin: 18
                                anchors.topMargin: 12
                                anchors.bottomMargin: 12
                                spacing: 4

                                Text {
                                    width: parent.width
                                    text: modelData.label
                                    color: ui.subText
                                    font.pixelSize: 14
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: modelData.value
                                    color: ui.text
                                    font.pixelSize: 22
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: modelData.sub
                                    color: ui.muted
                                    font.pixelSize: 14
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    x: 30
                    y: 250
                    width: 590
                    height: 260
                    radius: 16
                    color: ui.card
                    border.color: ui.borderSoft

                    Text {
                        x: 26
                        y: 22
                        text: "IPv4 Configuration"
                        color: ui.text
                        font.pixelSize: 22
                        font.bold: true
                    }

                    Row {
                        x: 26
                        y: 62
                        spacing: 10

                        Button {
                            width: 260
                            height: 44
                            text: "Using DHCP"
                            onClicked: {
                                useDhcp = true
                                refreshDhcpInfo()
                            }

                            contentItem: Text {
                                text: parent.text
                                color: useDhcp ? "#001412" : ui.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 14
                                font.bold: true
                            }

                            background: Rectangle {
                                radius: 10
                                color: useDhcp ? ui.accent : ui.field
                                border.color: useDhcp ? ui.accent : ui.border
                            }
                        }

                        Button {
                            width: 260
                            height: 44
                            text: "Static Manual"
                            onClicked: useDhcp = false

                            contentItem: Text {
                                text: parent.text
                                color: !useDhcp ? "#001412" : ui.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 14
                                font.bold: true
                            }

                            background: Rectangle {
                                radius: 10
                                color: !useDhcp ? ui.accent : ui.field
                                border.color: !useDhcp ? ui.accent : ui.border
                            }
                        }
                    }

                    GridLayout {
                        x: 26
                        y: 120
                        width: parent.width - 52
                        columns: 2
                        columnSpacing: 14
                        rowSpacing: 10

                        // Keep the original 2 x 2 layout. The fourth cell is a compact
                        // dual-DNS editor so Primary and Secondary DNS fit without
                        // increasing the card size or moving the controls below it.
                        Repeater {
                            model: [
                                { label: "IP Address", value: ipAddress, placeholder: "192.168.10.15", key: "ip" },
                                { label: "Subnet Mask", value: netmask, placeholder: "255.255.255.0", key: "mask" },
                                { label: "Gateway", value: gateway, placeholder: "192.168.10.254", key: "gw" }
                            ]

                            ColumnLayout {
                                Layout.preferredWidth: 260
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: ui.subText
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                TextField {
                                    Layout.preferredWidth: 260
                                    Layout.preferredHeight: 42
                                    text: modelData.value
                                    placeholderText: modelData.placeholder
                                    enabled: !useDhcp
                                    color: ui.text
                                    font.pixelSize: 14
                                    verticalAlignment: TextInput.AlignVCenter
                                    leftPadding: 12
                                    rightPadding: 12
                                    topPadding: 0
                                    bottomPadding: 0

                                    onEditingFinished: {
                                        if (modelData.key === "ip") ipAddress = text
                                        else if (modelData.key === "mask") netmask = text
                                        else if (modelData.key === "gw") gateway = text
                                    }

                                    background: Rectangle {
                                        radius: 9
                                        color: ui.field
                                        border.color: parent.activeFocus ? ui.accent : ui.border
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.preferredWidth: 260
                            Layout.preferredHeight: 63
                            spacing: 8

                            ColumnLayout {
                                Layout.preferredWidth: 126
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: "Primary DNS"
                                    color: ui.subText
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                TextField {
                                    Layout.preferredWidth: 126
                                    Layout.preferredHeight: 42
                                    text: primaryDns
                                    placeholderText: "8.8.8.8"
                                    color: ui.text
                                    font.pixelSize: 13
                                    verticalAlignment: TextInput.AlignVCenter
                                    leftPadding: 10
                                    rightPadding: 10
                                    topPadding: 0
                                    bottomPadding: 0

                                    onEditingFinished: primaryDns = text

                                    background: Rectangle {
                                        radius: 9
                                        color: ui.field
                                        border.color: parent.activeFocus ? ui.accent : ui.border
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.preferredWidth: 126
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: "Secondary DNS"
                                    color: ui.subText
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                TextField {
                                    Layout.preferredWidth: 126
                                    Layout.preferredHeight: 42
                                    text: secondaryDns
                                    placeholderText: "8.8.4.4"
                                    color: ui.text
                                    font.pixelSize: 13
                                    verticalAlignment: TextInput.AlignVCenter
                                    leftPadding: 10
                                    rightPadding: 10
                                    topPadding: 0
                                    bottomPadding: 0

                                    onEditingFinished: secondaryDns = text

                                    background: Rectangle {
                                        radius: 9
                                        color: ui.field
                                        border.color: parent.activeFocus ? ui.accent : ui.border
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: deviceInfoCard
                    x: 650
                    y: 250
                    width: parent.width - 700
                    height: 260
                    radius: 16
                    color: ui.card
                    border.color: ui.borderSoft

                    readonly property real infoColumnWidth: Math.max(220, (width - 52 - 48) / 2)

                    Text {
                        x: 26
                        y: 22
                        text: "Device Information"
                        color: ui.text
                        font.pixelSize: 22
                        font.bold: true
                    }

                    GridLayout {
                        x: 26
                        y: 72
                        width: parent.width - 52
                        columns: 2
                        rowSpacing: 18
                        columnSpacing: 48

                        Repeater {
                            model: [
                                { label: "Interface", value: interfaceName },
                                { label: "MAC Address", value: safeText(lanMacAddress, "-") },
                                { label: "DNS", value: primaryDns + (secondaryDns.length > 0 ? ", " + secondaryDns : "") },
                                { label: "Status", value: statusMessage.length > 0 ? statusMessage : lanStatusText() }
                            ]

                            ColumnLayout {
                                Layout.preferredWidth: deviceInfoCard.infoColumnWidth
                                spacing: 4

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: ui.subText
                                    font.pixelSize: 14
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: safeText(modelData.value, "-")
                                    color: ui.text
                                    font.pixelSize: 16
                                    wrapMode: Text.WrapAnywhere
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                Row {
                    x: 30
                    y: parent.height - 92
                    spacing: 20
                    Button { width: 220; height: 48; text: "Apply / Save"; onClicked: requestProtectedLanApply()
                        contentItem: Text { text: parent.text; color: "#001412"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15 }
                        background: Rectangle { radius: 10; color: ui.accent; border.color: ui.accent } }
                    Button { width: 220; height: 48; text: "Refresh"; onClicked: loadLanInterfaces()
                        contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15 }
                        background: Rectangle { radius: 10; color: ui.field; border.color: ui.border } }
                    Button { width: 220; height: 48; text: "DHCP Info"; onClicked: refreshDhcpInfo()
                        contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15 }
                        background: Rectangle { radius: 10; color: ui.field; border.color: ui.border } }
                }
            }
        }

        Loader {
            id: wifiLoader
            anchors.fill: parent
            active: selectedTab === "wifi"
            visible: selectedTab === "wifi"
            source: active ? "qrc:/Wifi5GSetting.qml" : ""
            onLoaded: {
                // KP-6JUL2026 : WiFi backend work is allowed only for this WiFi loader.
                item.pageScope = "wifi"
                item.initialNetworkPage = "wifi"
                item.forceSingleNetworkPage = true
                item.hideInternalNetworkTabs = true
                item.requestToast.connect(function(text) { statusMessage = text })
            }
        }

        Loader {
            id: cellularLoader
            anchors.fill: parent
            active: selectedTab === "cellular"
            visible: selectedTab === "cellular"
            source: active ? "qrc:/Wifi5GSetting.qml" : ""
            onLoaded: {
                // KP-6JUL2026 : 5G loader must never start WiFi scan/status timers.
                item.pageScope = "cellular"
                item.initialNetworkPage = "cellular"
                item.forceSingleNetworkPage = true
                item.hideInternalNetworkTabs = true
                item.requestToast.connect(function(text) { statusMessage = text })
            }
        }


    }
}
