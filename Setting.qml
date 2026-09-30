import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls.Material 2.12
import App 1.0

import "ui"

Item {
    id: networkManager

    Theme {
        id: hmiTheme
        darkMode: networkManager.Material.theme === Material.Dark
    }
    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080
    clip: true
    readonly property bool compactLayout: width < 1500
    readonly property bool veryCompactLayout: width < 1100

    // KP-6JUL2026 : build flag controls only the top-bar drawer tab.
    // The complete Network Settings page remains available regardless of this flag.
    // UX-NAV1: remove the legacy top Network Settings drawer handle.
    // The full Network Settings page remains available from the normal menu.
    readonly property bool topNetworkDrawerEnabled: false

    // NET-AUTH2.2: transient access level supplied by MainPage when this page is pushed.
    // Direct/fallback loads default to Viewer, the least-privileged role.
    property string networkAccessRole: "viewer"
    readonly property bool networkAdminMode: String(networkAccessRole).toLowerCase() === "admin"

    function canEditLanByIndex(index) {
        // Viewer may edit the two local Jetson LAN ports; remote RFSoC LAN3/4 are Admin-only.
        return networkAdminMode || index === 0 || index === 1
    }

    function canEditCurrentLan() {
        return canEditLanByIndex(lanIndexForInterface(interfaceName))
    }

    function networkAccessLabel() {
        return networkAdminMode ? "ADMIN ACCESS" : "VIEWER ACCESS"
    }

    function setNetworkAccessRole(role) {
        var normalized = String(role).toLowerCase() === "admin" ? "admin" : "viewer"
        if (normalized === String(networkAccessRole).toLowerCase())
            return

        closeVirtualKeyboard()
        lanModeDirty = false
        networkAccessRole = normalized

        if (vpnLoader.item)
            vpnLoader.item.adminMode = networkAdminMode

        // Discard protected Admin drafts when downgrading while viewing LAN3/4.
        if (!networkAdminMode && !canEditCurrentLan())
            loadLanSetting()

        statusMessage = networkAdminMode
                ? "Admin access enabled: LAN1-LAN4, Endpoints, WiFi, 5G and VPN control"
                : "Viewer access enabled: LAN1-LAN2, Endpoints, WiFi and 5G editable; LAN3-LAN4 and VPN control read-only"
    }

    function requestNetworkAccessToggle() {
        if (networkAdminMode) {
            setNetworkAccessRole("viewer")
            return
        }
        networkModePasswordPopup.requestUnlock()
    }

    function requestLanApplyConfirmation() {
        closeVirtualKeyboard()
        if (!canEditCurrentLan()) {
            statusMessage = "Viewer access: " + lanDisplayName() + " is read-only. Use Admin access to modify LAN3-LAN4."
            return
        }
        lanApplyConfirmPopup.open()
    }

    // NET-AUTH3: direct/fallback page loads also start least-privileged.
    // Viewer -> Admin remains available through the in-page password flow.
    // Runtime proof: this must appear when the real Network page is loaded.
    Component.onCompleted: {
        networkAccessRole = "viewer"
        console.log("[NET-AUTH3] Network Settings entered in Viewer mode")
        console.log("NETWORK_UI_RUNTIME_PROOF_TABS_LAN_ENDPOINTS_WIFI_5G_VPN_LOADED qrc:/Setting.qml")
        loadLanInterfaces()
    }

    property string selectedTab: "lan"
    // VPN auto-refreshes only on the first VPN entry of this Network Settings page session.
    property bool vpnInitialRefreshDone: false
    onSelectedTabChanged: {
        if (selectedTab === "lan")
            loadLanInterfaces()
    }

    // LAN Phase A: the redesigned UI uses Mainwindows as the compatibility
    // coordinator for mutations, while NetworkController remains the async
    // read/status backend. Krakenmapval stays owned by C++ and is not mutated
    // directly from this page.
    property string interfaceName: ""
    property bool useDhcp: true
    // Preserve the operator's unsaved DHCP/Static choice against async readback.
    property bool lanModeDirty: false
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

    // R-LAN4A: LAN3/LAN4 are remote RFSoC ports. This is the state of the
    // shared TCP control channel used to configure end0/end1; it is not remote
    // physical-carrier telemetry.
    property bool externalLanControlConnected: false
    property string externalLanControlHost: ""
    property int externalLanControlPort: 0

    // UX-KB1.1: Enter the compact LAN editing layout only while the
    // virtual keyboard is actually visible. keyboardRectangle can retain
    // its last non-zero size after the keyboard is hidden on Qt 5, so it
    // must not be used as a visibility latch.
    readonly property bool lanKeyboardMode: selectedTab === "lan" && Qt.inputMethod.visible

    function closeVirtualKeyboard() {
        networkManager.forceActiveFocus()
        Qt.inputMethod.hide()
    }

    property bool hardwareHasWireless: (typeof HardwareHasWireless === "undefined") ? true : HardwareHasWireless
    property bool hardwareHasWifi: (typeof HardwareHasWifi === "undefined") ? true : HardwareHasWifi
    property bool hardwareHas5G: (typeof HardwareHas5G === "undefined") ? true : HardwareHas5G

    // Phase 4: Network Settings now consumes the same HMI design tokens as
    // Radio/DoA.  Keep this compatibility object because the existing LAN
    // workflow references ui.* throughout; only presentation ownership moves.
    QtObject {
        id: ui
        property color bg: hmiTheme.page
        property color topBar: hmiTheme.topBar
        property color mainPanel: hmiTheme.shell
        property color panel: hmiTheme.panel
        property color card: hmiTheme.cardAlt
        property color field: hmiTheme.input
        property color border: hmiTheme.lineStrong
        property color borderSoft: hmiTheme.line
        property color text: hmiTheme.text
        property color subText: hmiTheme.textSecondary
        property color muted: hmiTheme.muted
        property color accent: hmiTheme.accent
        property color warning: hmiTheme.warning
        property color danger: hmiTheme.danger
        property color info: hmiTheme.info
        property color disabled: hmiTheme.disabled
        property color accentText: "#061514"
        property color selection: hmiTheme.darkMode ? Qt.lighter(hmiTheme.cardAlt, 1.10) : "#E3F2EF"
        property color disabledField: hmiTheme.darkMode ? "#303740" : "#E9EFED"
        property color disabledBorder: hmiTheme.darkMode ? "#46505C" : hmiTheme.line
        property color warningFill: hmiTheme.darkMode ? "#3A2A0B" : "#FFF3D6"
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
        var parts = String(mask).trim().split(".")
        if (parts.length !== 4)
            return -1

        var binary = ""
        for (var i = 0; i < 4; ++i) {
            if (!/^\d+$/.test(parts[i]))
                return -1
            var n = Number(parts[i])
            if (!isFinite(n) || n < 0 || n > 255 || Math.floor(n) !== n)
                return -1
            binary += ("00000000" + n.toString(2)).slice(-8)
        }

        // A valid IPv4 netmask must contain one contiguous run of 1 bits
        // followed only by 0 bits. Do not silently turn 255.0.255.0 into /16.
        if (!/^1*0*$/.test(binary))
            return -1

        var firstZero = binary.indexOf("0")
        return firstZero < 0 ? 32 : firstZero
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

    // R-LAN4A.2: role and execution scope are independent concepts.
    // LAN1/LAN3 are device-facing peers; LAN2/LAN4 are network-facing peers.
    // The local/remote distinction only selects the executor/status source.
    function lanPortRole(info) {
        var role = readLanValue(info, ["portRole"], "").toLowerCase()
        if (role === "device" || role === "network")
            return role

        var iface = readLanValue(info, ["iface", "interface"], interfaceName)
        var index = lanIndexForInterface(iface)
        if (index === 0 || index === 2)
            return "device"
        if (index === 1 || index === 3)
            return "network"
        return "unknown"
    }

    function lanPortRoleLabel(info) {
        var explicitLabel = readLanValue(info, ["portRoleLabel"], "")
        if (explicitLabel.length > 0)
            return explicitLabel
        var role = lanPortRole(info)
        if (role === "device")
            return "External Device"
        if (role === "network")
            return "Network"
        return "Unknown"
    }

    function lanPortRoleShortLabel(info) {
        var role = lanPortRole(info)
        if (role === "device")
            return "Device"
        if (role === "network")
            return "Network"
        return "LAN"
    }

    function lanExecutionScope(info) {
        var scope = readLanValue(info, ["executionScope", "controlScope"], "").toLowerCase()
        if (scope === "local" || scope === "remote")
            return scope

        var iface = readLanValue(info, ["iface", "interface"], interfaceName)
        return (iface === "end0" || iface === "end1") ? "remote" : "local"
    }

    function lanExecutionScopeLabel(info) {
        var explicitLabel = readLanValue(info, ["executionScopeLabel"], "")
        if (explicitLabel.length > 0)
            return explicitLabel
        return lanExecutionScope(info) === "remote" ? "Remote RFSoC" : "Local Device"
    }

    function lanRoleScopeText(info) {
        return lanPortRoleLabel(info) + " · " + lanExecutionScopeLabel(info)
    }

    function isRemoteLanInfo(info) {
        if (!info)
            return false
        return lanExecutionScope(info) === "remote"
    }

    // Compatibility helper name retained for the existing page logic.
    function isExternalLanInfo(info) {
        return isRemoteLanInfo(info)
    }

    function isExternalLanCurrent() {
        return isRemoteLanInfo(currentLanInfo()) || interfaceName === "end0" || interfaceName === "end1"
    }

    function externalLanControlText() {
        return externalLanControlConnected ? "Connected" : "Disconnected"
    }

    // R-LAN4A.1: the LAN page represents the remote RFSoC interface being
    // configured (end0/end1), not the management/control socket used to reach
    // the RFSoC. Keep the TCP host/port internally for connectivity telemetry,
    // but present the configured interface IPv4 as the user-facing address.
    function externalLanConfiguredIpText() {
        var configured = String(ipAddress).trim()
        return configured.length > 0 ? configured : "No configured IP"
    }

    function externalLanTargetText() {
        var iface = String(interfaceName).trim()
        var ip = externalLanConfiguredIpText()
        return iface.length > 0 ? (iface + " · " + ip) : ip
    }

    function refreshExternalLanStatus() {
        if (typeof mainWindows === "undefined" || !mainWindows ||
                typeof mainWindows.externalLanStatus !== "function")
            return

        // LAN3/end0 and LAN4/end1 share the same RFSoC TCP control channel.
        // Query it even while LAN1/LAN2 is selected so the external rows are
        // already truthful before the user opens them.
        var index = isExternalLanCurrent() ? lanIndexForInterface(interfaceName) : 2
        var state = mainWindows.externalLanStatus(index)
        if (!state)
            return
        externalLanControlConnected = !!state.connected
        externalLanControlHost = state.host !== undefined && state.host !== null ? String(state.host) : ""
        externalLanControlPort = state.port !== undefined && state.port !== null ? Number(state.port) : 0
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
        if (isExternalLanInfo(info))
            return externalLanControlConnected ? "RFSoC Control Connected" : "RFSoC Control Disconnected"

        var st = readLanValue(info, ["status", "linkStatus", "link", "state", "operstate"], "")
        if (st.length > 0 && st !== "Unknown")
            return st
        var ip = readLanValue(info, ["ip", "liveIp", "current_ip", "dev_ip4_plain"], "")
        if (ip.length > 0)
            return "Configured"
        return "Unknown"
    }

    function lanInfoCompactStatusText(info) {
        if (isExternalLanInfo(info))
            return externalLanControlConnected ? "Control Connected" : "Control Disconnected"
        return lanInfoStatusText(info)
    }

    function lanInfoStatusColor(info) {
        var status = lanInfoStatusText(info).toLowerCase()
        // Check negative states first: "disconnected" contains the substring
        // "connected", so the old ordering could paint LAN3/LAN4 disconnected
        // state with the connected/accent color.
        if (status.indexOf("no cable") >= 0 || status.indexOf("down") >= 0 || status.indexOf("disconnect") >= 0)
            return ui.warning
        if (status.indexOf("connected") >= 0 || status.indexOf("up") >= 0 || status.indexOf("configured") >= 0)
            return ui.accent
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

        if (!lanModeDirty) {
            var modeText = readLanValue(info, ["mode", "ipv4_method"], "dhcp").toLowerCase()
            var staticMode = modeText === "static" || modeText === "manual" ||
                             modeText === "off" || modeText === "0" ||
                             modeText === "false" || modeText === "disabled"
            useDhcp = !staticMode
        }

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

        if (isExternalLanInfo(info))
            refreshExternalLanStatus()
    }

    function loadLanInterfaces() {
        if (!backendAvailable()) {
            statusMessage = "NetworkController is not available"
            return
        }

        // R20.2: nmcli/device probing runs off the Qt GUI/audio event loop.
        NetworkController.requestLoadAllLanConfig()
    }

    function applyLanInterfacesResult(result) {
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
        refreshExternalLanStatus()
    }

    function refreshDhcpInfo() {
        if (!backendAvailable()) {
            statusMessage = "NetworkController is not available"
            return
        }
        NetworkController.requestDhcpInfo(interfaceName)
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

        NetworkController.requestLoadConfig(interfaceName)
    }

    function selectLan(iface) {
        var targetIndex = lanIndexForInterface(iface)
        if (!canEditLanByIndex(targetIndex) && lanKeyboardMode)
            closeVirtualKeyboard()
        lanModeDirty = false
        interfaceName = iface
        loadLanSetting()
        refreshExternalLanStatus()
    }

    function lanIndexForInterface(iface) {
        if (iface === "enP8p1s0") return 0
        if (iface === "enP1p1s0") return 1
        if (iface === "end0") return 2
        if (iface === "end1") return 3
        return -1
    }

    function applyLanSetting() {
        if (!backendAvailable()) {
            statusMessage = "NetworkController is not available"
            return
        }

        var index = lanIndexForInterface(interfaceName)
        if (index < 0) {
            statusMessage = "Unsupported LAN interface: " + interfaceName
            return
        }

        if (!canEditLanByIndex(index)) {
            statusMessage = "Viewer access: " + lanDisplayName() + " is read-only. Use Admin access to modify LAN3-LAN4."
            return
        }

        var mode = useDhcp ? "dhcp" : "static"
        var cidr = netmaskToCidr(netmask)
        var ipWithCidr = ""

        if (!useDhcp) {
            if (cidr < 0) {
                statusMessage = "Invalid subnet mask"
                return
            }
            ipWithCidr = String(ipAddress).trim() + "/" + cidr
        } else if (String(ipAddress).trim().length > 0 && cidr >= 0) {
            // Preserve the currently displayed DHCP lease for compatibility,
            // but DHCP does not depend on it for local NetworkManager apply.
            ipWithCidr = String(ipAddress).trim() + "/" + cidr
        }

        // Production path: one compatibility coordinator restores the legacy
        // Network2/RFSoC + LAN2 recorder side effects and invokes
        // NetworkController exactly once for JSON/system mutation.
        if (typeof mainWindows !== "undefined" && mainWindows) {
            if (typeof mainWindows.validateLanSettings === "function") {
                var validation = mainWindows.validateLanSettings(index,
                                                                 mode,
                                                                 ipWithCidr,
                                                                 netmask,
                                                                 gateway,
                                                                 primaryDns,
                                                                 secondaryDns)
                if (!validation || !validation.ok) {
                    statusMessage = validation && validation.message
                            ? validation.message
                            : "Invalid LAN configuration"
                    return
                }
            }

            if (typeof mainWindows.applyLanSettings === "function") {
                if (index === 2 || index === 3) {
                    // Remote LAN Apply is now persistence-first.  The desired
                    // Network2/JSON state must be saved even when the RFSoC
                    // control TCP link is down; reconnect/dispatch is only a
                    // best-effort follow-up and must never roll the form back.
                    if (typeof mainWindows.externalLanStatus === "function") {
                        var tcpState = mainWindows.externalLanStatus(index)
                        externalLanControlConnected = !!(tcpState && tcpState.connected)
                        externalLanControlHost = tcpState && tcpState.host ? String(tcpState.host) : ""
                        externalLanControlPort = tcpState && tcpState.port ? Number(tcpState.port) : 0
                    }

                    statusMessage = "Saving " + (index === 2 ? "LAN3 / end0" : "LAN4 / end1") +
                            " IP configuration; RFSoC reconnect will continue if needed..."
                } else {
                    statusMessage = "Saving and applying LAN configuration..."
                }

                var accepted = mainWindows.applyLanSettings(index,
                                                            mode,
                                                            ipWithCidr,
                                                            netmask,
                                                            gateway,
                                                            primaryDns,
                                                            secondaryDns)
                if (!accepted && statusMessage.indexOf("disconnected") < 0)
                    statusMessage = "LAN configuration was rejected"
                return
            }
        }

        // Design/compatibility fallback when Mainwindows is not exposed. Local
        // LAN1/LAN2 may still use NetworkController directly. Remote end0/end1
        // must never fall back to a local/network-file-only Apply because the
        // RFSoC TCP command would be missing.
        if (index === 2 || index === 3) {
            statusMessage = "RFSoC control backend is unavailable"
            return
        }

        var dns = primaryDns + (secondaryDns.length > 0 ? "," + secondaryDns : "")
        NetworkController.applyNetworkConfig(interfaceName,
                                             mode,
                                             ipWithCidr,
                                             gateway,
                                             dns)
    }

    Connections {
        target: (typeof mainWindows !== "undefined") ? mainWindows : null
        ignoreUnknownSignals: true

        function onExternalLanControlStatusChanged(connected, host, port) {
            externalLanControlConnected = connected
            externalLanControlHost = host ? String(host) : ""
            externalLanControlPort = Number(port)
        }


        function onRemoteLanIpConfigDispatch(iface, ip, state, detail) {
            if (String(iface) !== String(interfaceName))
                return

            var portName = iface === "end0" ? "LAN3" : (iface === "end1" ? "LAN4" : iface)
            if (state === "DISPATCHED") {
                statusMessage = portName + ": IP saved; RFSoC command dispatched for " + ip
            } else if (state === "QUEUED") {
                statusMessage = portName + ": IP saved; RFSoC command queued until control reconnects"
            } else if (state === "QUEUED_NO_TARGET") {
                statusMessage = portName + ": IP saved; RFSoC control target is not configured yet"
            } else if (state === "CONTROL_DISCONNECTED") {
                statusMessage = portName + ": IP saved; RFSoC control server disconnected"
            } else if (state === "DB_UNAVAILABLE") {
                statusMessage = portName + ": database backend unavailable - IP was not saved"
            } else {
                statusMessage = portName + ": RFSoC IP command failed after save - " + detail
            }
        }
    }

    Connections {
        target: NetworkController
        function onApplyNetworkConfigFinished(iface, ok, message, gatewayValue, dnsValue) {
            statusMessage = message
            if (iface === interfaceName) {
                if (ok)
                    lanModeDirty = false
                loadLanSetting()
            }
        }
        function onApplyNetworkConfigNmcliFinished(iface, ok, message) {
            statusMessage = message
            if (iface === interfaceName)
                loadLanInterfaces()
        }
        function onLanConfigReady(result) {
            applyLanInterfacesResult(result)
        }
        function onLanInterfaceConfigReady(iface, result) {
            if (iface !== interfaceName)
                return
            if (result) {
                replaceLanInfo(iface, result)
                applyLanInfo(lanByIface[iface])
            }
        }
        function onDhcpInfoReady(iface, info) {
            if (iface !== interfaceName)
                return
            if (info) {
                replaceLanInfo(iface, info)
                applyLanInfo(lanByIface[iface])
            }
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
            color: ui.card
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

    NetworkPasswordPopup {
        id: networkModePasswordPopup
        darkMode: hmiTheme.darkMode
        titleText: "Switch to Admin"
        messageText: "Enter the administrator password to enable full LAN and VPN access"
        unlockButtonText: "Switch to Admin"
        onAuthorized: networkManager.setNetworkAccessRole("admin")
    }

    Popup {
        id: lanApplyConfirmPopup
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        width: Math.min(560, networkManager.width - 48)
        height: Math.min(360, networkManager.height - 80)
        x: Math.max(24, (networkManager.width - width) / 2)
        y: Math.max(24, (networkManager.height - height) / 2 - 20)
        enter: Transition { NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 140; easing.type: Easing.OutCubic } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 100; easing.type: Easing.InCubic } }
        background: Rectangle { radius: 18; color: ui.panel; border.color: ui.accent; border.width: 1 }
        contentItem: Item {
            Text { x: 28; y: 24; width: parent.width - 56; text: "Confirm Apply / Save"; color: ui.text; font.pixelSize: 24; font.bold: true }
            Text { x: 28; y: 64; width: parent.width - 56; text: "Review the network change before it is sent to the device."; color: ui.subText; font.pixelSize: 14; wrapMode: Text.WordWrap }
            Rectangle {
                x: 28; y: 104; width: parent.width - 56; height: 148; radius: 12; color: ui.field; border.color: ui.border
                Text { x: 18; y: 16; width: parent.width - 36; text: lanDisplayName() + " / " + interfaceName; color: ui.text; font.pixelSize: 18; font.bold: true; elide: Text.ElideRight }
                Text { x: 18; y: 52; width: parent.width - 36; text: "Mode: " + (useDhcp ? "Using DHCP" : "Static Manual"); color: useDhcp ? ui.accent : ui.info; font.pixelSize: 14; font.bold: true }
                Text { x: 18; y: 82; width: parent.width - 36; text: useDhcp ? "The device will request IPv4 settings automatically." : ("IPv4: " + safeText(ipAddress, "--") + "    Mask: " + safeText(netmask, "--") + "\nGateway: " + safeText(gateway, "--")); color: ui.subText; font.pixelSize: 13; wrapMode: Text.WordWrap }
            }
            Text { x: 28; y: 266; width: parent.width - 56; text: "This can temporarily interrupt network connectivity."; color: ui.warning; font.pixelSize: 13; font.bold: true }
            Row {
                x: 28; y: 302; spacing: 16
                HmiButton {
                    id: cancelLanApplyButton
                    width: 238
                    height: 44
                    darkMode: hmiTheme.darkMode
                    fontPixelSize: 14
                    text: "Cancel"
                    onClicked: lanApplyConfirmPopup.close()
                }

                HmiButton {
                    id: confirmLanApplyButton
                    width: 250
                    height: 44
                    darkMode: hmiTheme.darkMode
                    fontPixelSize: 14
                    tone: "primary"
                    text: "Confirm Apply"
                    onClicked: {
                        lanApplyConfirmPopup.close()
                        networkManager.applyLanSetting()
                    }
                }
            }
        }
    }

    HmiPanel {
        id: networkHeaderPanel
        x: 24
        y: 78
        width: parent.width - 48
        height: 154
        darkMode: hmiTheme.darkMode
    }

    Text {
        x: 48
        y: 94
        text: "Network Settings"
        color: ui.text
        font.pixelSize: networkManager.compactLayout ? 28 : 34
        font.bold: true
    }

    Text {
        x: 48
        y: 136
        width: Math.max(260, parent.width - 420)
        text: "Unified LAN, Service Endpoints, WiFi, 5G and VPN settings."
        color: ui.subText
        font.pixelSize: networkManager.compactLayout ? 13 : 15
        elide: Text.ElideRight
    }

    Text {
        x: 48
        y: 176
        width: Math.max(240, networkTabRow.x - x - 24)
        text: selectedTab === "lan" ? "LAN Interface Settings"
              : selectedTab === "endpoints" ? "Service Endpoints"
              : selectedTab === "wifi" ? "WiFi Settings"
              : selectedTab === "cellular" ? "5G Modem Settings"
              : "VPN Settings"
        color: ui.text
        font.pixelSize: networkManager.compactLayout ? 30 : 40
        elide: Text.ElideRight
        font.bold: true
    }

    Button {
        id: networkAccessSwitchButton
        anchors.right: parent.right
        anchors.rightMargin: 48
        y: 96
        width: networkManager.compactLayout ? 190 : 220
        height: 48
        text: networkManager.networkAccessLabel()
        scale: pressed ? 0.96 : 1.0
        Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        onClicked: networkManager.requestNetworkAccessToggle()
        contentItem: Row {
            anchors.centerIn: parent
            spacing: 10
            Text { text: networkAccessSwitchButton.text; color: networkManager.networkAdminMode ? ui.accent : ui.info; font.pixelSize: 13; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
            Text { text: networkManager.networkAdminMode ? "VIEWER" : "ADMIN"; color: ui.subText; font.pixelSize: 11; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "↔"; color: networkManager.networkAdminMode ? ui.accent : ui.info; font.pixelSize: 14; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
        }
        background: Rectangle {
            radius: 24
            color: networkAccessSwitchButton.pressed ? ui.selection : ui.card
            border.color: networkManager.networkAdminMode ? ui.accent : ui.info
            border.width: 1
        }
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 520
        anchors.right: networkAccessSwitchButton.left
        anchors.rightMargin: 16
        y: 112
        visible: !networkManager.compactLayout
        text: networkManager.networkAdminMode
              ? "Full network access · Endpoints editable · tap to switch Viewer"
              : "LAN1 + LAN2 + Endpoints + WiFi + 5G editable · tap for Admin"
        color: ui.subText
        font.pixelSize: 13
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
    }

    Row {
        id: networkTabRow
        anchors.right: parent.right
        anchors.rightMargin: 48
        y: 176
        spacing: networkManager.veryCompactLayout ? 8 : 16

        Repeater {
            model: [
                { key: "lan", label: "LAN", enabled: true },
                { key: "endpoints", label: "Endpoints", enabled: true },
                { key: "wifi", label: "WiFi", enabled: networkManager.hardwareHasWireless && networkManager.hardwareHasWifi },
                { key: "cellular", label: "5G", enabled: networkManager.hardwareHasWireless && networkManager.hardwareHas5G },
                { key: "vpn", label: "VPN", enabled: true }
            ]
            HmiButton {
                width: networkManager.veryCompactLayout ? 104 : 130
                height: 48
                darkMode: hmiTheme.darkMode
                enabled: modelData.enabled
                text: modelData.label
                tone: selectedTab === modelData.key ? "primary" : "normal"
                onClicked: selectedTab = modelData.key
            }
        }
    }

    Rectangle {
        id: mainPanel
        x: 36
        // 24 px visual gutter below the 48 px tab row (176 + 48 = 224).
        y: 248
        width: parent.width - 72
        height: parent.height - y - 60
        radius: 18
        color: ui.mainPanel
        border.color: ui.border
        border.width: 1

        Item {
            id: lanPage
            anchors.fill: parent
            visible: selectedTab === "lan"

            Rectangle {
                id: lanListPanel
                x: networkManager.compactLayout ? 18 : 34
                y: 20
                width: networkManager.compactLayout ? 250 : 330
                height: parent.height - 40
                visible: true
                enabled: true
                radius: 18
                color: ui.panel
                border.color: ui.border

                Text {
                    x: 18
                    y: 18
                    text: "LAN Interfaces"
                    color: ui.text
                    font.pixelSize: 22
                    font.bold: true
                }

                Repeater {
                    model: lanInterfaces
                    Rectangle {
                        id: lanInterfaceCard
                        x: 18
                        y: 62 + index * 90
                        width: parent.width - 36
                        height: 76
                        radius: 14
                        scale: lanInterfaceMouse.pressed ? 0.985 : 1.0
                        Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }
                        color: interfaceName === modelData.iface ? ui.selection : ui.field
                        border.color: interfaceName === modelData.iface ? ui.accent : ui.border
                        border.width: interfaceName === modelData.iface ? 2 : 1

                        MouseArea { id: lanInterfaceMouse; anchors.fill: parent; onClicked: selectLan(modelData.iface) }
                        Rectangle {
                            x: 16
                            y: 18
                            width: 14; height: 14; radius: 7
                            color: lanInfoStatusColor(modelData)
                        }
                        Text {
                            x: 42
                            y: 9
                            text: safeText(modelData.name, modelData.iface)
                            color: ui.text
                            font.pixelSize: 18
                            font.bold: true
                        }
                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            y: 11
                            text: "VIEW ONLY"
                            visible: !networkManager.canEditLanByIndex(networkManager.lanIndexForInterface(modelData.iface))
                            color: ui.warning
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Text {
                            x: 42
                            y: 34
                            width: parent.width - 54
                            text: modelData.iface + " · " + lanPortRoleShortLabel(modelData) + " · " + lanInfoCompactStatusText(modelData)
                            color: ui.subText
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        Text {
                            x: 42
                            y: 54
                            text: safeText(modelData.ip || modelData.liveIp || modelData.current_ip || modelData.dev_ip4_plain, "--")
                            color: ui.text
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }
                }
            }

            Rectangle {
                id: lanDetailPanel
                x: lanListPanel.x + lanListPanel.width + 18
                y: 20
                width: parent.width - x - (networkManager.compactLayout ? 18 : 34)
                height: parent.height - 40
                radius: 18
                color: ui.panel
                border.color: ui.border

                Text {
                    x: 30; y: 28
                    width: Math.max(160, lanStatusBadge.x - x - 18)
                    text: "Selected Interface: " + lanDisplayName() + " / " + interfaceName
                    color: ui.text
                    font.pixelSize: networkManager.compactLayout ? 20 : 28
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    x: 30; y: 66
                    text: lanRoleScopeText(currentLanInfo())
                    color: ui.subText
                    font.pixelSize: 14
                    font.bold: true
                }

                Rectangle {
                    id: lanStatusBadge
                    property int horizontalPadding: 16
                    property int minBadgeWidth: 130
                    property int maxBadgeWidth: 280

                    y: 30
                    width: Math.max(minBadgeWidth,
                                    Math.min(maxBadgeWidth, lanStatusLabel.implicitWidth + horizontalPadding * 2))
                    height: 32
                    x: parent.width - width - 40

                    radius: 16
                    color: ui.card
                    border.color: lanStatusColor()

                    Text {
                        id: lanStatusLabel
                        anchors.fill: parent
                        anchors.leftMargin: lanStatusBadge.horizontalPadding
                        anchors.rightMargin: lanStatusBadge.horizontalPadding
                        text: lanStatusText()
                        color: lanStatusColor()
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                Row {
                    x: 30
                    y: 100
                    spacing: 24
                    visible: false

                    Repeater {
                        model: [
                            { label: isExternalLanCurrent() ? "RFSoC Control" : "Link",
                              value: isExternalLanCurrent() ? externalLanControlText() : safeText(lanSpeed, "-"),
                              sub: isExternalLanCurrent() ? externalLanTargetText() : safeText(lanDuplex, "Full duplex") },
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
                    id: ipv4ConfigCard
                    x: 30
                    y: 95
                    width: networkManager.compactLayout
                           ? Math.max(430, Math.min(520, parent.width * 0.62))
                           : 590
                    height: 260
                    radius: 16
                    color: ui.card
                    border.color: ui.borderSoft
                    enabled: networkManager.canEditCurrentLan()
                    opacity: enabled ? 1.0 : 0.58

                    Text {
                        x: 26
                        y: 22
                        text: "IPv4 Configuration"
                        color: ui.text
                        font.pixelSize: 22
                        font.bold: true
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.rightMargin: 22
                        y: 18
                        width: 104
                        height: 28
                        radius: 14
                        visible: !networkManager.canEditCurrentLan()
                        color: ui.warningFill
                        border.color: ui.warning
                        Text { anchors.centerIn: parent; text: "READ ONLY"; color: ui.warning; font.pixelSize: 11; font.bold: true }
                    }

                    Row {
                        id: lanModeRow
                        x: 26
                        y: 62
                        width: parent.width - 52
                        spacing: 10

                        HmiButton {
                            id: lanDhcpModeButton
                            width: (lanModeRow.width - lanModeRow.spacing) / 2
                            height: 44
                            darkMode: hmiTheme.darkMode
                            fontPixelSize: 14
                            text: "Using DHCP"
                            tone: useDhcp ? "primary" : "normal"
                            onClicked: {
                                useDhcp = true
                                lanModeDirty = true
                                statusMessage = "DHCP selected. Press Apply / Save to apply this mode."
                            }
                        }

                        HmiButton {
                            id: lanStaticModeButton
                            width: (lanModeRow.width - lanModeRow.spacing) / 2
                            height: 44
                            darkMode: hmiTheme.darkMode
                            fontPixelSize: 14
                            text: "Static Manual"
                            tone: !useDhcp ? "primary" : "normal"
                            onClicked: {
                                useDhcp = false
                                lanModeDirty = true
                                statusMessage = "Static Manual selected. Press Apply / Save to apply this mode."
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
                                Layout.preferredWidth: Math.max(150, (ipv4ConfigCard.width - 66) / 2)
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: useDhcp ? ui.muted : ui.subText
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                TextField {
                                    Layout.preferredWidth: Math.max(150, (ipv4ConfigCard.width - 66) / 2)
                                    Layout.preferredHeight: 42
                                    text: modelData.value
                                    placeholderText: modelData.placeholder
                                    enabled: !useDhcp
                                    color: useDhcp ? ui.disabled : ui.text
                                    placeholderTextColor: useDhcp ? ui.muted : ui.disabled
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
                                        color: useDhcp ? ui.disabledField : ui.field
                                        border.color: useDhcp ? ui.disabledBorder : (parent.activeFocus ? ui.accent : ui.border)
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.preferredWidth: Math.max(150, (ipv4ConfigCard.width - 66) / 2)
                            Layout.preferredHeight: 63
                            spacing: 8

                            ColumnLayout {
                                Layout.preferredWidth: Math.max(70, (parent.width - parent.spacing) / 2)
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: "Primary DNS"
                                    color: useDhcp ? ui.muted : ui.subText
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                TextField {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 42
                                    text: primaryDns
                                    placeholderText: "8.8.8.8"
                                    enabled: !useDhcp
                                    color: useDhcp ? ui.disabled : ui.text
                                    placeholderTextColor: useDhcp ? ui.muted : ui.disabled
                                    font.pixelSize: 13
                                    verticalAlignment: TextInput.AlignVCenter
                                    leftPadding: 10
                                    rightPadding: 10
                                    topPadding: 0
                                    bottomPadding: 0

                                    onEditingFinished: primaryDns = text

                                    background: Rectangle {
                                        radius: 9
                                        color: useDhcp ? ui.disabledField : ui.field
                                        border.color: useDhcp ? ui.disabledBorder : (parent.activeFocus ? ui.accent : ui.border)
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.preferredWidth: Math.max(70, (parent.width - parent.spacing) / 2)
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: "Secondary DNS"
                                    color: useDhcp ? ui.muted : ui.subText
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                TextField {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 42
                                    text: secondaryDns
                                    placeholderText: "8.8.4.4"
                                    enabled: !useDhcp
                                    color: useDhcp ? ui.disabled : ui.text
                                    placeholderTextColor: useDhcp ? ui.muted : ui.disabled
                                    font.pixelSize: 13
                                    verticalAlignment: TextInput.AlignVCenter
                                    leftPadding: 10
                                    rightPadding: 10
                                    topPadding: 0
                                    bottomPadding: 0

                                    onEditingFinished: secondaryDns = text

                                    background: Rectangle {
                                        radius: 9
                                        color: useDhcp ? ui.disabledField : ui.field
                                        border.color: useDhcp ? ui.disabledBorder : (parent.activeFocus ? ui.accent : ui.border)
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: deviceInfoCard
                    x: ipv4ConfigCard.x + ipv4ConfigCard.width + 18
                    y: 250
                    width: Math.max(150, parent.width - x - 20)
                    height: 260
                    radius: 16
                    color: ui.card
                    border.color: ui.borderSoft

                    visible: false

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
                                { label: isExternalLanCurrent() ? "Port Role" : "MAC Address",
                                  value: isExternalLanCurrent() ? lanPortRoleLabel(currentLanInfo()) : safeText(lanMacAddress, "-") },
                                { label: isExternalLanCurrent() ? "Configured IPv4" : "DNS",
                                  value: isExternalLanCurrent() ? externalLanConfiguredIpText()
                                                                : primaryDns + (secondaryDns.length > 0 ? ", " + secondaryDns : "") },
                                { label: "Status",
                                  value: isExternalLanCurrent() ? (lanExecutionScopeLabel(currentLanInfo()) + " · Control " + externalLanControlText())
                                                                : (statusMessage.length > 0 ? statusMessage : lanStatusText()) }
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

                Rectangle {
                    id: keyboardEditSummary
                    x: ipv4ConfigCard.x + ipv4ConfigCard.width + 18
                    y: 95
                    width: Math.max(150, parent.width - x - 20)
                    height: 105
                    radius: 16
                    color: ui.card
                    border.color: ui.accent
                    border.width: 1
                    visible: true

                    Text {
                        x: 24
                        y: 18
                        width: Math.max(80, parent.width - (networkManager.lanKeyboardMode ? 180 : 48))
                        text: "Editing " + lanDisplayName() + " / " + interfaceName
                        color: ui.text
                        font.pixelSize: 21
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        x: 24
                        y: 52
                        width: Math.max(80, parent.width - (networkManager.lanKeyboardMode ? 180 : 48))
                        text: "Configured IPv4: " + safeText(ipAddress, "-") +
                              "  ·  " + (useDhcp ? "DHCP" : "Static Manual") +
                              (networkManager.canEditCurrentLan() ? "" : "  ·  VIEW ONLY")
                        color: ui.subText
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }

                    HmiButton {
                        anchors.right: parent.right
                        anchors.rightMargin: 18
                        anchors.verticalCenter: parent.verticalCenter
                        width: 130
                        height: 46
                        darkMode: hmiTheme.darkMode
                        fontPixelSize: 14
                        tone: "primary"
                        text: "Done"
                        visible: networkManager.lanKeyboardMode
                        enabled: visible
                        onClicked: closeVirtualKeyboard()
                    }
                }

                Grid {
                    id: lanActionRow
                    x: ipv4ConfigCard.x + ipv4ConfigCard.width + 18
                    y: networkManager.compactLayout ? 212 : 220
                    width: Math.max(150, parent.width - x - 20)
                    columns: networkManager.compactLayout ? 1 : 3
                    spacing: networkManager.compactLayout ? 8 : 20
                    HmiButton {
                        id: lanApplyButton
                        width: networkManager.compactLayout ? lanActionRow.width : 220
                        height: networkManager.compactLayout ? 42 : 48
                        darkMode: hmiTheme.darkMode
                        fontPixelSize: 15
                        tone: "primary"
                        text: networkManager.canEditCurrentLan() ? "Apply / Save" : "View Only"
                        enabled: networkManager.canEditCurrentLan()
                        onClicked: networkManager.requestLanApplyConfirmation()
                    }
                    HmiButton {
                        id: lanRefreshButton
                        width: networkManager.compactLayout ? lanActionRow.width : 220
                        height: networkManager.compactLayout ? 42 : 48
                        darkMode: hmiTheme.darkMode
                        fontPixelSize: 15
                        text: "Refresh"
                        onClicked: { lanModeDirty = false; loadLanInterfaces() }
                    }
                    HmiButton {
                        id: lanStatusButton
                        width: networkManager.compactLayout ? lanActionRow.width : 220
                        height: networkManager.compactLayout ? 42 : 48
                        darkMode: hmiTheme.darkMode
                        fontPixelSize: 15
                        text: isExternalLanCurrent() ? "RFSoC Status" : "DHCP Info"
                        onClicked: isExternalLanCurrent() ? refreshExternalLanStatus() : refreshDhcpInfo()
                    }
                }
            }
        }

        Loader {
            id: endpointsLoader
            anchors.fill: parent
            active: selectedTab === "endpoints"
            visible: selectedTab === "endpoints"
            source: active ? "qrc:/ServiceEndpointsPage.qml" : ""
        }

        Connections {
            target: endpointsLoader.item
            ignoreUnknownSignals: true
            function onRequestToast(text) { statusMessage = text }
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
            }
        }

        Connections {
            target: wifiLoader.item
            ignoreUnknownSignals: true
            function onRequestToast(text) { statusMessage = text }
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
            }
        }

        Connections {
            target: cellularLoader.item
            ignoreUnknownSignals: true
            function onRequestToast(text) { statusMessage = text }
        }

        Loader {
            id: vpnLoader
            anchors.fill: parent
            active: selectedTab === "vpn"
            visible: selectedTab === "vpn"
            source: active ? "qrc:/VpnPage.qml" : ""
            onLoaded: {
                item.adminMode = networkManager.networkAdminMode
                if (!networkManager.vpnInitialRefreshDone) {
                    networkManager.vpnInitialRefreshDone = true
                    item.refreshAll()
                }
            }
        }

        Connections {
            target: vpnLoader.item
            ignoreUnknownSignals: true
            function onRequestToast(text) { statusMessage = text }
        }

    }
}
