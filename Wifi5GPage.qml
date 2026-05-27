import QtQuick 2.15

Item {
    id: root

    // Runtime wrapper. The UI was moved to Wifi5GView.qml; backend calls stay here.
    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080
    clip: true
    z: 999

    property var networkBackend: resolveNetworkBackend()
    property var mainWindowBackend: (typeof mainWindows === "undefined") ? null : mainWindows
    readonly property bool hasBackend: networkBackend !== null || mainWindowBackend !== null
    readonly property bool designMode: !hasBackend || Qt.application.arguments.join(" ").indexOf("qml2puppet") >= 0

    property bool hardwareHas5G: (typeof HardwareHas5G === "undefined") ? true : HardwareHas5G
    property string hardwareVersionName: (typeof HardwareVersionName === "undefined") ? "5G" : HardwareVersionName
    property bool useBackendJson: true
    readonly property bool showCellularControls: hardwareHas5G
    readonly property int networkGridColumns: showCellularControls && root.width > 1200 ? 2 : 1
    readonly property int networkCardHeight: showCellularControls ? 620 : Math.max(620, root.height - 220)
    property alias selectedNetworkPage: pageView.selectedNetworkPage

    property alias wifiEnabled: pageView.wifiEnabled
    property alias wifiIface: pageView.wifiIface
    property alias wifiSsid: pageView.wifiSsid
    property alias wifiBssid: pageView.wifiBssid
    property alias wifiProfileName: pageView.wifiProfileName
    property alias wifiPassword: pageView.wifiPassword
    property alias wifiAutoConnect: pageView.wifiAutoConnect
    property alias wifiSelectedKnown: pageView.wifiSelectedKnown
    property alias selectedWifiConnected: pageView.selectedWifiConnected
    property alias selectedWifiHasPassword: pageView.selectedWifiHasPassword
    property alias wifiPasswordVisible: pageView.wifiPasswordVisible
    property alias wifiToggleBusy: pageView.wifiToggleBusy
    property alias wifiConnectBusy: pageView.wifiConnectBusy
    property alias wifiForgetBusy: pageView.wifiForgetBusy
    property alias wifiAdvancedBusy: pageView.wifiAdvancedBusy
    property alias pendingWifiAction: pageView.pendingWifiAction
    property alias wifiAdvancedVisible: pageView.wifiAdvancedVisible
    property alias wifiAdvancedMessage: pageView.wifiAdvancedMessage
    property alias wifiAdvancedIpv4Mode: pageView.wifiAdvancedIpv4Mode
    property alias wifiAdvancedIpAddress: pageView.wifiAdvancedIpAddress
    property alias wifiAdvancedSubnetMask: pageView.wifiAdvancedSubnetMask
    property alias wifiAdvancedGateway: pageView.wifiAdvancedGateway
    property alias wifiAdvancedDnsAutomatic: pageView.wifiAdvancedDnsAutomatic
    property alias wifiAdvancedDnsServers: pageView.wifiAdvancedDnsServers
    property alias wifiAdvancedCurrentIp: pageView.wifiAdvancedCurrentIp
    property alias wifiAdvancedCurrentPrefix: pageView.wifiAdvancedCurrentPrefix
    property alias wifiAdvancedCurrentGateway: pageView.wifiAdvancedCurrentGateway
    property alias wifiAdvancedConnectionName: pageView.wifiAdvancedConnectionName
    property alias wifiList: pageView.wifiList
    property alias wifiState: pageView.wifiState
    property alias wifiMessage: pageView.wifiMessage

    property alias cellularIface: pageView.cellularIface
    property alias cellularApn: pageView.cellularApn
    property alias cellularAutoConnect: pageView.cellularAutoConnect
    property alias cellularState: pageView.cellularState
    property alias modemList: pageView.modemList
    property alias cellularMessage: pageView.cellularMessage

    signal requestToast(string text)

    function resolveNetworkBackend() {
        if (typeof networkController !== "undefined" && networkController)
            return networkController

        // Keep this as a guarded fallback only. We avoid importing App 1.0 here
        // because C++ singleton modules are not available to QML Puppet in Design mode.
        if (typeof NetworkController !== "undefined" && NetworkController)
            return NetworkController

        return null
    }

    function safeText(value, fallback) {
        if (value === undefined || value === null || value === "")
            return fallback
        return String(value)
    }

    function copyObject(source) {
        var target = {}
        if (!source)
            return target

        for (var key in source)
            target[key] = source[key]
        return target
    }

    function startWifiConnectBusy(action) {
        wifiConnectBusy = true
        pendingWifiAction = action
        wifiConnectTimeoutTimer.restart()
    }

    function clearWifiConnectBusy() {
        wifiConnectBusy = false
        wifiConnectTimeoutTimer.stop()
        if (!wifiToggleBusy && !wifiForgetBusy)
            pendingWifiAction = ""
    }

    function startWifiToggleBusy(action) {
        wifiToggleBusy = true
        pendingWifiAction = action
        wifiToggleTimeoutTimer.restart()
    }

    function clearWifiToggleBusy() {
        wifiToggleBusy = false
        wifiToggleTimeoutTimer.stop()
        if (!wifiConnectBusy && !wifiForgetBusy)
            pendingWifiAction = ""
    }

    function startWifiForgetBusy() {
        wifiForgetBusy = true
        pendingWifiAction = "forget"
        wifiForgetTimeoutTimer.restart()
    }

    function clearWifiForgetBusy() {
        wifiForgetBusy = false
        wifiForgetTimeoutTimer.stop()
        if (!wifiConnectBusy && !wifiToggleBusy)
            pendingWifiAction = ""
    }

    function prefixToMask(prefixValue) {
        var prefix = Number(prefixValue)
        if (isNaN(prefix) || prefix < 0 || prefix > 32)
            return ""

        var mask = []
        for (var i = 0; i < 4; ++i) {
            var bits = Math.min(8, Math.max(0, prefix - i * 8))
            mask.push(bits === 0 ? 0 : (256 - Math.pow(2, 8 - bits)))
        }
        return mask.join(".")
    }

    function applyWifiAdvancedInfo(info) {
        var data = info || {}
        wifiAdvancedConnectionName = safeText(data.connection_name || data.profileName, wifiProfileName)
        if (wifiAdvancedConnectionName.length > 0)
            wifiProfileName = wifiAdvancedConnectionName

        var method = safeText(data.ipv4_method || data.method, "auto").toLowerCase()
        wifiAdvancedIpv4Mode = method === "manual" ? "manual" : "dhcp"

        var addresses = safeText(data.ipv4_addresses, "")
        var ip = safeText(data.ip || data.ipAddress || data.dev_ip4_plain, "")
        var prefix = safeText(data.prefix || data.dev_ip4_prefix, "")
        if (addresses.length > 0) {
            var addressParts = addresses.split(",")[0].split("/")
            ip = safeText(addressParts[0], ip)
            prefix = safeText(addressParts[1], prefix)
        }

        wifiAdvancedIpAddress = ip
        wifiAdvancedCurrentIp = safeText(data.dev_ip4_plain, ip)
        wifiAdvancedCurrentPrefix = safeText(data.dev_ip4_prefix, prefix)
        wifiAdvancedSubnetMask = safeText(data.mask || data.subnetMask || data.dev_ip4_netmask,
                                          prefixToMask(prefix))
        wifiAdvancedGateway = safeText(data.ipv4_gateway || data.gateway, "")
        wifiAdvancedCurrentGateway = safeText(data.dev_ip4_gateway,
                                              safeText(data.ipv4_gateway || data.gateway, ""))
        wifiAdvancedDnsAutomatic = data.dns_auto === undefined ? true : data.dns_auto
        wifiAdvancedDnsServers = safeText(data.dns || data.dnsServers, "")
        wifiAdvancedMessage = safeText(data.message, "")
    }

    function mockWifiRows() {
        return [
            {
                "ssid": "Office-WiFi",
                "bssid": "00:11:22:33:44:55",
                "band": "5 GHz",
                "channel": 44,
                "signal": 82,
                "security": "WPA2",
                "active": true,
                "known": true,
                "profile_name": "Office-WiFi"
            },
            {
                "ssid": "Field-Router",
                "bssid": "66:77:88:99:AA:BB",
                "band": "2.4 GHz",
                "channel": 6,
                "signal": 58,
                "security": "WPA2",
                "active": false,
                "known": false,
                "profile_name": ""
            }
        ]
    }

    function applyMockData() {
        // Qt Creator Design mode runs in QML Puppet without C++ context properties.
        // Keep all mock data here so Wifi5GView remains backend-free and previewable.
        hardwareHas5G = true
        hardwareVersionName = "5G"
        wifiEnabled = true
        wifiIface = safeText(wifiIface, "wlP9p1s0")
        wifiSsid = safeText(wifiSsid, "Office-WiFi")
        wifiBssid = safeText(wifiBssid, "00:11:22:33:44:55")
        wifiProfileName = safeText(wifiProfileName, "Office-WiFi")
        wifiSelectedKnown = true
        selectedWifiConnected = true
        selectedWifiHasPassword = false
        wifiPasswordVisible = false
        wifiAdvancedConnectionName = "Office-WiFi"
        wifiAdvancedIpv4Mode = "dhcp"
        wifiAdvancedIpAddress = "192.168.10.24"
        wifiAdvancedSubnetMask = "255.255.255.0"
        wifiAdvancedGateway = "192.168.10.1"
        wifiAdvancedDnsAutomatic = true
        wifiAdvancedDnsServers = "8.8.8.8,1.1.1.1"
        wifiAdvancedCurrentIp = "192.168.10.24"
        wifiAdvancedCurrentPrefix = "24"
        wifiAdvancedCurrentGateway = "192.168.10.1"
        wifiState = {
            "connected": true,
            "ssid": wifiSsid,
            "connection": wifiSsid,
            "signal": 82,
            "ip": "192.168.10.24",
            "gateway": "192.168.10.1",
            "device": wifiIface,
            "enabled": true
        }
        wifiList = mockWifiRows()
        wifiMessage = "Mock data for design preview"

        cellularIface = safeText(cellularIface, "*")
        cellularApn = safeText(cellularApn, "internet")
        cellularAutoConnect = true
        cellularState = {
            "connected": true,
            "modemName": "Quectel 5G",
            "device": "rmnet_mhi0.1",
            "operator": "AIS",
            "state": "registered",
            "sim_status": "ready",
            "registration_state": "home",
            "accessTech": "nr5g",
            "access_technology": "nr5g",
            "signal": "22/31"
        }
        modemList = [
            {
                "name": "Quectel 5G",
                "vendor": "Mock modem",
                "disabled": false
            }
        ]
        cellularMessage = "Mock data for design preview"
    }

    function ensureValidSelectedPage() {
        if (!root.showCellularControls && selectedNetworkPage === "cellular")
            selectedNetworkPage = "wifi"
    }

    function sendBackendCommand(obj) {
        if (useBackendJson && mainWindowBackend && mainWindowBackend.cppSubmitTextFiled) {
            mainWindowBackend.cppSubmitTextFiled(JSON.stringify(obj))
            return true
        }
        return false
    }

    function requestSavedWifiPassword(iface, ssid, bssid, profileName) {
        var targetSsid = safeText(ssid, wifiSsid)
        var targetProfileName = safeText(profileName, wifiProfileName)
        var targetBssid = safeText(bssid, wifiBssid)

        if (targetSsid.length === 0 || targetProfileName.length === 0) {
            wifiPassword = ""
            selectedWifiHasPassword = false
            return
        }

        if (sendBackendCommand({
            "menuID": "wifi_password",
            "iface": safeText(iface, wifiIface),
            "ssid": targetSsid,
            "bssid": targetBssid,
            "profileName": targetProfileName
        })) {
            return
        }

        if (!networkBackend) {
            wifiPassword = ""
            selectedWifiHasPassword = false
            return
        }

        var result = networkBackend.wifiSavedPassword(targetProfileName, targetSsid, targetBssid)
        var password = safeText(result.password, "")
        wifiPassword = result.ok && password.length > 0 ? password : ""
        selectedWifiHasPassword = wifiPassword.length > 0
    }

    function refreshWifiConfig() {
        if (!networkBackend) {
            wifiIface = safeText(wifiIface, "wlP9p1s0")
            wifiSsid = safeText(wifiSsid, "")
            wifiAutoConnect = true
            return
        }

        var cfg = networkBackend.loadWifiConfig()
        wifiIface = safeText(cfg.interface, "wlP9p1s0")
        wifiSsid = safeText(cfg.ssid, "")
        wifiAutoConnect = cfg.autoConnect === undefined ? true : cfg.autoConnect
    }

    function refreshWifiStatus() {
        if (sendBackendCommand({"menuID": "wifi_state", "iface": wifiIface}))
            return

        if (!networkBackend) {
            wifiState = {
                "connected": wifiEnabled,
                "ssid": wifiSsid,
                "connection": wifiSsid,
                "signal": wifiEnabled ? 82 : 0,
                "device": wifiIface,
                "enabled": wifiEnabled
            }
            updateSelectedWifiKnownFromList()
            return
        }

        wifiState = networkBackend.wifiState(wifiIface)
        wifiEnabled = wifiState.enabled === undefined ? wifiEnabled : wifiState.enabled
        updateSelectedWifiKnownFromList()
    }

    function applyWifiScanData(scanData, envelope) {
        var data = scanData || {}
        var source = envelope || {}
        wifiEnabled = data.enabled === undefined ? wifiEnabled : data.enabled
        wifiIface = safeText(data.device || data.interface || source.device || source.iface, wifiIface)
        wifiList = data.rows || source.rows || source.networks || []

        var nextWifiState = copyObject(wifiState)
        if (data.active_ssid !== undefined)
            nextWifiState.ssid = data.active_ssid
        if (data.active_ssid !== undefined)
            nextWifiState.connected = String(data.active_ssid || "").length > 0
        if (data.current_ip !== undefined)
            nextWifiState.ip = data.current_ip
        if (data.current_gateway !== undefined)
            nextWifiState.gateway = data.current_gateway
        if (data.current_netmask !== undefined)
            nextWifiState.netmask = data.current_netmask
        if (data.device !== undefined)
            nextWifiState.device = data.device
        nextWifiState.enabled = wifiEnabled
        wifiState = nextWifiState
        updateSelectedWifiKnownFromList()

        wifiMessage = wifiList.length > 0
                ? ("Found " + wifiList.length + " network(s)")
                : safeText(source.message || data.message || data.error, "No WiFi networks found")
    }

    function updateSelectedWifiKnownFromList() {
        var matched = false
        var selectedSsid = safeText(wifiSsid, "")
        var activeSsid = safeText(wifiState.ssid || wifiState.connection || wifiState.active_ssid, "")
        for (var i = 0; i < wifiList.length; ++i) {
            var row = wifiList[i]
            if (!row || row.ssid !== selectedSsid)
                continue
            if (wifiBssid.length > 0 && row.bssid && row.bssid !== wifiBssid)
                continue

            matched = true
            wifiSelectedKnown = pageView.isSavedWifi(row)
            selectedWifiConnected = pageView.isConnectedWifi(row)
                                    || (!!(wifiState && wifiState.connected)
                                        && selectedSsid.length > 0
                                        && selectedSsid === activeSsid)
            wifiProfileName = safeText(row.profile_name || row.connection, wifiProfileName)
            break
        }

        if (!matched) {
            selectedWifiConnected = !!(wifiState && wifiState.connected)
                                    && selectedSsid.length > 0
                                    && selectedSsid === activeSsid
            if (wifiProfileName.length === 0)
                wifiSelectedKnown = false
        }
    }

    function scanWifi() {
        wifiMessage = "Scanning..."
        if (sendBackendCommand({"menuID": "scan", "iface": wifiIface}))
            return

        if (!networkBackend) {
            wifiList = mockWifiRows()
            updateSelectedWifiKnownFromList()
            wifiMessage = "Found " + wifiList.length + " network(s)"
            return
        }

        applyWifiScanData(networkBackend.scanWifiPage(wifiIface), {})
    }

    function refreshWifiNow() {
        if (!root.visible && !designMode)
            return

        // The automatic page timer uses the same scan path as the manual Scan button.
        // Keep this local to the page lifecycle so NetworkManager scans do not run globally.
        scanWifi()
    }

    function startWifiAutoRescan() {
        if (designMode || !root.visible)
            return

        // This timer runs only while the WiFi/5G page is visible.
        // Do not move it to app startup; background scanning wastes WiFi/CPU resources.
        if (!wifiAutoRescanTimer.running)
            wifiAutoRescanTimer.start()
    }

    function stopWifiAutoRescan() {
        if (wifiAutoRescanTimer.running)
            wifiAutoRescanTimer.stop()
    }

    function refreshCellularConfig() {
        if (!root.showCellularControls) {
            cellularIface = ""
            cellularApn = ""
            cellularAutoConnect = false
            return
        }

        if (!networkBackend) {
            cellularIface = safeText(cellularIface, "*")
            cellularApn = safeText(cellularApn, "internet")
            cellularAutoConnect = true
            return
        }

        var cfg = networkBackend.loadCellularConfig()
        cellularIface = safeText(cfg.interface, "*")
        cellularApn = safeText(cfg.apn, "internet")
        cellularAutoConnect = cfg.autoConnect === undefined ? true : cfg.autoConnect
    }

    function refreshCellularStatus() {
        if (!root.showCellularControls) {
            cellularState = {}
            modemList = []
            cellularMessage = ""
            return
        }

        if (sendBackendCommand({"menuID": "lte_state"}))
            return

        if (!networkBackend) {
            cellularState = {
                "connected": true,
                "modemName": "Quectel 5G",
                "device": "rmnet_mhi0.1",
                "operator": "AIS",
                "state": "registered",
                "sim_status": "ready",
                "registration_state": "home",
                "accessTech": "nr5g",
                "access_technology": "nr5g",
                "signal": "22/31"
            }
            modemList = [
                {
                    "name": "Quectel 5G",
                    "vendor": "Mock modem",
                    "disabled": false
                }
            ]
            return
        }

        cellularState = networkBackend.cellularStatus()
        modemList = networkBackend.listModems()
    }

    function refreshAll() {
        if (sendBackendCommand({"menuID": "getWifi5GPage"}))
            return

        if (!networkBackend) {
            applyMockData()
            return
        }

        refreshWifiConfig()
        refreshWifiStatus()
        if (root.showCellularControls) {
            refreshCellularConfig()
            refreshCellularStatus()
        }
        ensureValidSelectedPage()
    }

    function toggleWifi(on) {
        var nextEnabled = on === undefined ? !wifiEnabled : on
        startWifiToggleBusy(nextEnabled ? "wifi_on" : "wifi_off")
        wifiMessage = nextEnabled ? "Turning WiFi on..." : "Turning WiFi off..."
        if (sendBackendCommand({"menuID": "wifi_toggle", "on": nextEnabled}))
            return

        if (!networkBackend) {
            wifiEnabled = nextEnabled
            wifiMessage = wifiEnabled ? "WiFi mock enabled" : "WiFi mock disabled"
            if (!wifiEnabled)
                wifiList = []
            refreshWifiStatus()
            clearWifiToggleBusy()
            return
        }

        var result = networkBackend.wifiToggle(nextEnabled)
        wifiEnabled = result.enabled === undefined ? nextEnabled : result.enabled
        wifiMessage = safeText(result.message, "")
        refreshWifiStatus()
        if (wifiEnabled)
            scanWifi()
        else
            wifiList = []
        clearWifiToggleBusy()
    }

    function connectWifi(iface, ssid, password, bssid, autoConnect) {
        var targetIface = safeText(iface, wifiIface)
        var targetSsid = safeText(ssid, wifiSsid)
        var targetPassword = password === undefined ? wifiPassword : password
        var targetBssid = safeText(bssid, wifiBssid)
        var targetAutoConnect = autoConnect === undefined ? wifiAutoConnect : autoConnect

        if (targetSsid.length === 0) {
            wifiMessage = "Select a WiFi network first"
            return
        }

        wifiIface = targetIface
        wifiSsid = targetSsid
        wifiPassword = targetPassword
        wifiBssid = targetBssid
        wifiAutoConnect = targetAutoConnect
        startWifiConnectBusy("connect")
        wifiMessage = "Connecting..."
        if (sendBackendCommand({
            "menuID": "join",
            "iface": targetIface,
            "ssid": targetSsid,
            "password": targetPassword,
            "bssid": targetBssid,
            "autoConnect": targetAutoConnect
        })) {
            return
        }

        if (!networkBackend) {
            wifiState = {
                "connected": true,
                "ssid": targetSsid,
                "connection": targetSsid,
                "signal": 82,
                "device": targetIface,
                "enabled": wifiEnabled
            }
            selectedWifiConnected = true
            wifiMessage = "Connected to " + targetSsid + " (mock)"
            requestToast(wifiMessage)
            clearWifiConnectBusy()
            return
        }

        networkBackend.connectWifi(targetIface, targetSsid, targetPassword, targetAutoConnect, targetBssid)
    }

    function disconnectWifi(iface) {
        var targetIface = safeText(iface, wifiIface)
        wifiIface = targetIface
        startWifiConnectBusy("disconnect")
        wifiMessage = "Disconnecting..."
        if (sendBackendCommand({"menuID": "disconnect", "device": targetIface}))
            return

        if (!networkBackend) {
            var nextWifiState = copyObject(wifiState)
            nextWifiState.connected = false
            nextWifiState.ssid = ""
            wifiState = nextWifiState
            selectedWifiConnected = false
            wifiMessage = "Disconnected (mock)"
            requestToast(wifiMessage)
            clearWifiConnectBusy()
            return
        }

        networkBackend.disconnectWifi(targetIface)
    }

    function forgetWifi(iface, ssid, bssid, profileName) {
        var targetIface = safeText(iface, wifiIface)
        var targetSsid = safeText(ssid, wifiSsid)
        var targetBssid = safeText(bssid, wifiBssid)
        var targetProfileName = safeText(profileName, wifiProfileName)

        if (targetSsid.length === 0)
            return

        wifiIface = targetIface
        wifiSsid = targetSsid
        wifiBssid = targetBssid
        wifiProfileName = targetProfileName
        startWifiForgetBusy()
        wifiMessage = "Forgetting saved WiFi profile..."

        if (sendBackendCommand({
            "menuID": "forget",
            "iface": targetIface,
            "ssid": targetSsid,
            "bssid": targetBssid,
            "profileName": targetProfileName
        })) {
            return
        }

        if (!networkBackend) {
            wifiSelectedKnown = false
            wifiProfileName = ""
            wifiPassword = ""
            selectedWifiHasPassword = false
            wifiList = mockWifiRows().map(function(row) {
                if (row.ssid === targetSsid) {
                    row.known = false
                    row.saved = false
                    row.profile_name = ""
                }
                return row
            })
            wifiMessage = "Removed saved WiFi profile (mock)"
            requestToast(wifiMessage)
            pageView.closeWifiAdvancedPanel()
            clearWifiForgetBusy()
            return
        }

        var result = networkBackend.forgetWifiProfile(targetProfileName, targetSsid, targetBssid)
        wifiMessage = safeText(result.message, "")
        if (result.ok) {
            wifiSelectedKnown = false
            wifiProfileName = ""
            wifiPassword = ""
            selectedWifiHasPassword = false
            pageView.closeWifiAdvancedPanel()
        }
        requestToast(wifiMessage)
        refreshWifiNow()
        clearWifiForgetBusy()
    }

    function openWifiAdvanced(iface, ssid, bssid, profileName) {
        var targetIface = safeText(iface, wifiIface)
        var targetSsid = safeText(ssid, wifiSsid)
        var targetBssid = safeText(bssid, wifiBssid)
        var targetProfileName = safeText(profileName, wifiProfileName)

        if (targetSsid.length === 0 || !selectedWifiConnected) {
            wifiMessage = "Advanced settings are available only for the connected network."
            requestToast(wifiMessage)
            return
        }

        wifiIface = targetIface
        wifiSsid = targetSsid
        wifiBssid = targetBssid
        wifiProfileName = targetProfileName
        wifiAdvancedBusy = true
        wifiAdvancedMessage = "Loading advanced WiFi settings..."
        pageView.openWifiAdvancedPanel()

        if (sendBackendCommand({
            "menuID": "advinfo",
            "iface": targetIface,
            "ssid": targetSsid,
            "bssid": targetBssid,
            "profileName": targetProfileName
        })) {
            return
        }

        if (!networkBackend) {
            applyWifiAdvancedInfo({
                "ok": true,
                "connection_name": targetProfileName,
                "ipv4_method": "auto",
                "dev_ip4_plain": "192.168.10.24",
                "dev_ip4_prefix": "24",
                "dev_ip4_netmask": "255.255.255.0",
                "dev_ip4_gateway": "192.168.10.1",
                "dns_auto": true,
                "dns": "8.8.8.8,1.1.1.1"
            })
            wifiAdvancedBusy = false
            return
        }

        var info = networkBackend.wifiAdvancedInfoForProfile(targetProfileName, targetSsid, targetIface)
        applyWifiAdvancedInfo(info)
        wifiAdvancedBusy = false
        if (!info.ok) {
            wifiAdvancedMessage = safeText(info.message, "Unable to load WiFi advanced settings")
            requestToast(wifiAdvancedMessage)
        }
    }

    function saveWifiAdvanced(settings) {
        var payload = settings || {}
        wifiAdvancedBusy = true
        wifiAdvancedMessage = "Saving advanced WiFi settings..."

        if (sendBackendCommand({
            "menuID": "wifi_advanced_save",
            "iface": safeText(payload.iface, wifiIface),
            "ssid": safeText(payload.ssid, wifiSsid),
            "bssid": safeText(payload.bssid, wifiBssid),
            "profileName": safeText(payload.profileName, wifiProfileName),
            "ipv4Mode": safeText(payload.ipv4Mode, wifiAdvancedIpv4Mode),
            "ipAddress": safeText(payload.ipAddress, wifiAdvancedIpAddress),
            "subnetMask": safeText(payload.subnetMask, wifiAdvancedSubnetMask),
            "gateway": safeText(payload.gateway, wifiAdvancedGateway),
            "dnsAutomatic": payload.dnsAutomatic === undefined ? wifiAdvancedDnsAutomatic : payload.dnsAutomatic,
            "dnsServers": safeText(payload.dnsServers, wifiAdvancedDnsServers)
        })) {
            return
        }

        if (!networkBackend) {
            wifiAdvancedMessage = "WiFi advanced settings saved (mock)"
            wifiAdvancedBusy = false
            pageView.closeWifiAdvancedPanel()
            requestToast(wifiAdvancedMessage)
            return
        }

        var result = networkBackend.applyWifiIpv4ForProfile(
                    safeText(payload.profileName, wifiProfileName),
                    safeText(payload.ssid, wifiSsid),
                    safeText(payload.iface, wifiIface),
                    safeText(payload.ipv4Mode, wifiAdvancedIpv4Mode) === "manual" ? "manual" : "auto",
                    safeText(payload.ipAddress, wifiAdvancedIpAddress),
                    safeText(payload.subnetMask, wifiAdvancedSubnetMask),
                    safeText(payload.gateway, wifiAdvancedGateway),
                    payload.dnsAutomatic === undefined ? wifiAdvancedDnsAutomatic : payload.dnsAutomatic,
                    safeText(payload.dnsServers, wifiAdvancedDnsServers))
        wifiAdvancedBusy = false
        wifiAdvancedMessage = safeText(result.message, "")
        if (wifiAdvancedMessage.length > 0)
            requestToast(wifiAdvancedMessage)
        if (result.ok) {
            pageView.closeWifiAdvancedPanel()
            refreshWifiStatus()
        }
    }

    function connectCellular(apn, iface, autoConnect) {
        var targetApn = safeText(apn, cellularApn)
        var targetIface = safeText(iface, cellularIface)
        var targetAutoConnect = autoConnect === undefined ? cellularAutoConnect : autoConnect

        cellularApn = targetApn
        cellularIface = targetIface
        cellularAutoConnect = targetAutoConnect
        cellularMessage = "Connecting..."
        if (sendBackendCommand({
            "menuID": "cellularConnect",
            "apn": targetApn,
            "iface": targetIface,
            "autoConnect": targetAutoConnect
        })) {
            return
        }

        if (!networkBackend) {
            var nextCellularState = copyObject(cellularState)
            nextCellularState.connected = true
            cellularState = nextCellularState
            cellularMessage = "Cellular connected (mock)"
            requestToast(cellularMessage)
            return
        }

        networkBackend.connectCellular(targetApn, targetIface, targetAutoConnect)
    }

    function listModems() {
        if (!root.showCellularControls) {
            modemList = []
            return
        }

        if (sendBackendCommand({"menuID": "listModems"}))
            return

        if (!networkBackend) {
            modemList = [
                {
                    "name": "Quectel 5G",
                    "vendor": "Mock modem",
                    "disabled": false
                }
            ]
            return
        }

        modemList = networkBackend.listModems()
    }

    function disconnectCellular() {
        cellularMessage = "Disconnecting..."
        if (sendBackendCommand({"menuID": "cellularDisconnect", "connectionName": "cellular-5g"}))
            return

        if (!networkBackend) {
            var nextCellularState = copyObject(cellularState)
            nextCellularState.connected = false
            cellularState = nextCellularState
            cellularMessage = "Cellular disconnected (mock)"
            requestToast(cellularMessage)
            return
        }

        networkBackend.disconnectCellular("cellular-5g")
    }

    function applyBackendMessage(obj) {
        if (!obj || typeof obj !== "object")
            return

        if (obj.menuID === "wifi5g") {
            if (obj.hardwareHas5G !== undefined)
                hardwareHas5G = obj.hardwareHas5G
            if (obj.hardwareVersion !== undefined)
                hardwareVersionName = obj.hardwareVersion
            ensureValidSelectedPage()

            if (obj.wifiConfig) {
                wifiIface = safeText(obj.wifiConfig.interface, wifiIface)
                wifiSsid = safeText(obj.wifiConfig.ssid, wifiSsid)
                wifiAutoConnect = obj.wifiConfig.autoConnect === undefined ? wifiAutoConnect : obj.wifiConfig.autoConnect
            }

            if (obj.wifiStatus) {
                wifiState = obj.wifiStatus
                wifiEnabled = obj.wifiStatus.enabled === undefined ? wifiEnabled : obj.wifiStatus.enabled
            }

            if (obj.cellularConfig) {
                cellularIface = safeText(obj.cellularConfig.interface, cellularIface)
                cellularApn = safeText(obj.cellularConfig.apn, cellularApn)
                cellularAutoConnect = obj.cellularConfig.autoConnect === undefined ? cellularAutoConnect : obj.cellularConfig.autoConnect
            }

            if (obj.cellularStatus)
                cellularState = obj.cellularStatus

            if (obj.modems)
                modemList = obj.modems

            updateSelectedWifiKnownFromList()
            return
        }

        if (obj.menuID === "wifiScan" || obj.menuID === "scan") {
            applyWifiScanData(obj.data || obj, obj)
            return
        }

        if (obj.menuID === "wifiStatus" || obj.menuID === "wifi_state") {
            wifiState = obj.status || obj.data || {}
            wifiEnabled = wifiState.enabled === undefined ? wifiEnabled : wifiState.enabled
            wifiIface = safeText(wifiState.device || wifiState.interface || obj.device || obj.iface, wifiIface)
            updateSelectedWifiKnownFromList()
            return
        }

        if (obj.menuID === "cellularStatus" || obj.menuID === "lte_state") {
            cellularState = obj.status || obj.data || {}
            modemList = obj.modems || modemList
            return
        }

        if (obj.menuID === "wifi_toggle") {
            clearWifiToggleBusy()
            var toggleData = obj.data || obj
            wifiEnabled = toggleData.enabled === undefined ? wifiEnabled : toggleData.enabled
            wifiMessage = safeText(obj.message || toggleData.message, "")
            refreshWifiStatus()
            if (wifiEnabled)
                scanWifi()
            else
                wifiList = []
            return
        }

        if (obj.menuID === "forget") {
            clearWifiForgetBusy()
            wifiMessage = safeText(obj.message || (obj.data ? obj.data.message : ""), "")
            if (obj.ok) {
                wifiSelectedKnown = false
                wifiProfileName = ""
                wifiPassword = ""
                selectedWifiHasPassword = false
                pageView.closeWifiAdvancedPanel()
            }
            if (wifiMessage.length > 0)
                requestToast(wifiMessage)
            refreshWifiNow()
            return
        }

        if (obj.menuID === "wifi_password") {
            var passwordData = obj.data || obj
            var passwordSsid = safeText(passwordData.ssid || obj.ssid, "")
            var passwordProfile = safeText(passwordData.connection_name || obj.profileName, "")
            if ((passwordSsid.length === 0 || passwordSsid === wifiSsid)
                    && (passwordProfile.length === 0 || passwordProfile === wifiProfileName)) {
                wifiPassword = passwordData.ok && safeText(passwordData.password, "").length > 0
                        ? safeText(passwordData.password, "")
                        : ""
                selectedWifiHasPassword = wifiPassword.length > 0
            }
            return
        }

        if (obj.menuID === "advinfo") {
            var infoData = obj.info || obj.data || obj
            applyWifiAdvancedInfo(infoData)
            wifiAdvancedBusy = false
            if (!obj.ok && wifiAdvancedMessage.length > 0)
                requestToast(wifiAdvancedMessage)
            return
        }

        if (obj.menuID === "apply_ipv4" || obj.menuID === "wifi_advanced_save") {
            wifiAdvancedBusy = false
            wifiAdvancedMessage = safeText(obj.message || (obj.data ? obj.data.message : ""), "")
            if (wifiAdvancedMessage.length > 0)
                requestToast(wifiAdvancedMessage)
            if (obj.ok) {
                pageView.closeWifiAdvancedPanel()
                refreshWifiStatus()
            }
            return
        }

        if (obj.menuID === "listModems") {
            modemList = obj.modems || []
            return
        }

        if (obj.menuID === "wifiOperationResult") {
            if (obj.action === "connect" || obj.action === "disconnect")
                clearWifiConnectBusy()
            wifiMessage = safeText(obj.message, "")
            requestToast(wifiMessage)
            refreshWifiStatus()
            refreshWifiNow()
            return
        }

        if (obj.menuID === "cellularOperationResult") {
            cellularMessage = safeText(obj.message, "")
            requestToast(cellularMessage)
            return
        }
    }

    onVisibleChanged: {
        if (visible) {
            if (!designMode) {
                refreshAll()
                refreshWifiNow()
            }
            startWifiAutoRescan()
        } else {
            stopWifiAutoRescan()
        }
    }

    Component.onCompleted: {
        if (designMode) {
            applyMockData()
        } else {
            refreshAll()
            refreshWifiNow()
        }
        ensureValidSelectedPage()
        startWifiAutoRescan()
    }

    Component.onDestruction: {
        stopWifiAutoRescan()
        wifiConnectTimeoutTimer.stop()
        wifiToggleTimeoutTimer.stop()
        wifiForgetTimeoutTimer.stop()
    }

    Timer {
        id: wifiAutoRescanTimer
        interval: 10000
        repeat: true
        running: false

        onTriggered: {
            if (root.visible)
                root.refreshWifiNow()
            else
                root.stopWifiAutoRescan()
        }
    }

    Timer {
        id: wifiConnectTimeoutTimer
        interval: 15000
        repeat: false

        onTriggered: {
            if (!root.wifiConnectBusy)
                return
            root.clearWifiConnectBusy()
            root.wifiMessage = "WiFi connect/disconnect timed out"
            root.requestToast(root.wifiMessage)
            root.refreshWifiStatus()
        }
    }

    Timer {
        id: wifiToggleTimeoutTimer
        interval: 10000
        repeat: false

        onTriggered: {
            if (!root.wifiToggleBusy)
                return
            root.clearWifiToggleBusy()
            root.wifiMessage = "WiFi on/off operation timed out"
            root.requestToast(root.wifiMessage)
            root.refreshWifiStatus()
        }
    }

    Timer {
        id: wifiForgetTimeoutTimer
        interval: 10000
        repeat: false

        onTriggered: {
            if (!root.wifiForgetBusy)
                return
            root.clearWifiForgetBusy()
            root.wifiMessage = "WiFi forget operation timed out"
            root.requestToast(root.wifiMessage)
            root.refreshWifiNow()
        }
    }

    Connections {
        target: root.mainWindowBackend
        ignoreUnknownSignals: true

        function onCppCommand(jsonMsg) {
            var obj = null
            try {
                obj = JSON.parse(String(jsonMsg))
            } catch (error) {
                return
            }
            root.applyBackendMessage(obj)
        }
    }

    Connections {
        target: root.networkBackend
        ignoreUnknownSignals: true

        function onWifiOperationFinished(action, ok, message) {
            if (action === "connect" || action === "disconnect")
                root.clearWifiConnectBusy()
            root.wifiMessage = message
            root.refreshWifiStatus()
            root.refreshWifiNow()
            root.requestToast(message)
        }

        function onCellularOperationFinished(action, ok, message) {
            root.cellularMessage = message
            root.refreshCellularStatus()
            root.requestToast(message)
        }
    }

    Wifi5GView {
        id: pageView

        anchors.fill: parent
        hardwareHas5G: root.hardwareHas5G
        hardwareVersionName: root.hardwareVersionName

        onRefreshAllRequested: root.refreshAll()
        onWifiScanRequested: root.scanWifi()
        onWifiToggleRequested: function(on) { root.toggleWifi(on) }
        onWifiConnectRequested: function(iface, ssid, password, bssid, autoConnect) {
            root.connectWifi(iface, ssid, password, bssid, autoConnect)
        }
        onWifiDisconnectRequested: function(iface) { root.disconnectWifi(iface) }
        onWifiForgetRequested: function(iface, ssid, bssid, profileName) {
            root.forgetWifi(iface, ssid, bssid, profileName)
        }
        onWifiSavedPasswordRequested: function(iface, ssid, bssid, profileName) {
            root.requestSavedWifiPassword(iface, ssid, bssid, profileName)
        }
        onWifiAdvancedOpenRequested: function(iface, ssid, bssid, profileName) {
            root.openWifiAdvanced(iface, ssid, bssid, profileName)
        }
        onWifiAdvancedSaveRequested: function(settings) {
            root.saveWifiAdvanced(settings)
        }
        onCellularRefreshRequested: root.refreshCellularStatus()
        onCellularConnectRequested: function(apn, iface, autoConnect) {
            root.connectCellular(apn, iface, autoConnect)
        }
        onCellularDisconnectRequested: root.disconnectCellular()
        onCellularListModemsRequested: root.listModems()
    }
}
