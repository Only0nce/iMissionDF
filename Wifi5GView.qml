import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Item {
    id: root
    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080
    clip: true

    property bool hardwareHas5G: true
    property bool hardwareHasWifi: true
    property bool hardwareHasWireless: true
    property string hardwareVersionName: "5G"
    property string selectedNetworkPage: "wifi"
    property bool forceSingleNetworkPage: false
    property bool hideInternalNetworkTabs: false

    property bool wifiEnabled: true
    property string wifiIface: "wlan0"
    property string wifiSsid: ""
    property string wifiBssid: ""
    property string wifiProfileName: ""
    property string wifiPassword: ""
    property string initialWifiPasswordRequestKey: ""
    property bool wifiAutoConnect: true
    property bool wifiSelectedKnown: false
    property bool selectedWifiConnected: false
    property bool selectedWifiHasPassword: false
    property bool wifiPasswordVisible: false
    property bool wifiChangeSavedPassword: false
    property string wifiConnectErrorText: ""
    property bool wifiToggleBusy: false
    property bool wifiConnectBusy: false
    property bool wifiForgetBusy: false
    property bool wifiAdvancedBusy: false
    property string pendingWifiAction: ""
    property bool wifiAdvancedVisible: false
    property var selectedWifiForConfig: null
    property string wifiAdvancedMessage: ""
    property string wifiAdvancedIpv4Mode: "dhcp"
    property string wifiAdvancedIpAddress: ""
    property string wifiAdvancedSubnetMask: ""
    property string wifiAdvancedGateway: ""
    property bool wifiAdvancedDnsAutomatic: true
    property string wifiAdvancedDnsServers: ""
    property string wifiAdvancedPrimaryDns: ""
    property string wifiAdvancedSecondaryDns: ""
    property bool wifiAdvancedDnsSplitSyncing: false
    property string wifiAdvancedCurrentIp: ""
    property string wifiAdvancedCurrentPrefix: ""
    property string wifiAdvancedCurrentGateway: ""
    property string wifiAdvancedConnectionName: ""
    property var wifiList: []
    property var wifiState: ({})
    property string wifiMessage: ""

    property string cellularIface: "rmnet_mhi0.1"
    property string cellularApn: "internet"
    property bool cellularAutoConnect: true
    property bool cellularResetBusy: false
    property var cellularState: ({})
    property var modemList: []
    property string cellularMessage: ""
    property var cellularModuleLogs: []

    readonly property bool showWifiControls: hardwareHasWireless && hardwareHasWifi
    readonly property bool showCellularControls: hardwareHasWireless && hardwareHas5G
    readonly property bool currentWifiConnected: !!(wifiState && wifiState.connected)
    readonly property bool hasSelectedWifi: safeText(wifiSsid, "").length > 0

    signal refreshAllRequested()
    signal wifiScanRequested()
    signal wifiToggleRequested(bool on)
    signal wifiConnectRequested(string iface, string ssid, string password, string bssid, bool autoConnect)
    signal wifiDisconnectRequested(string iface)
    signal wifiForgetRequested(string iface, string ssid, string bssid, string profileName)
    signal wifiSavedPasswordRequested(string iface, string ssid, string bssid, string profileName)
    signal wifiAdvancedOpenRequested(string iface, string ssid, string bssid, string profileName)
    signal wifiAdvancedSaveRequested(var settings)
    signal cellularRefreshRequested()
    signal cellularConnectRequested(string apn, string iface, bool autoConnect)
    signal cellularDisconnectRequested()
    signal cellularResetModemRequested()
    signal cellularListModemsRequested()

    QtObject {
        id: ui
        property color bg: "#07101a"
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
        property color blue: "#2fa6ff"
        property color warning: "#f59e0b"
        property color danger: "#ef4444"
        property color disabled: "#718096"
        property color saved: "#38bdf8"
        property color good: "#22c55e"
        property color fair: "#eab308"
        property color weak: "#f97316"
    }

    Component.onCompleted: {
        console.log("WIFI_TAB_BINDING_CHECK completed", wifiStatusText(), wifiSsidText(), wifiSignalText())
        console.log("MODEM_5G_TAB_BINDING_CHECK completed", cellularBadgeText(), cellularInterfaceText(), cellularSignalText())
        Qt.callLater(function() {
            root.syncWifiDerivedFields()
            root.requestInitialWifiPassword(false)
        })
    }

    onWifiStateChanged: {
        console.log("WIFI_TAB_BINDING_CHECK state", wifiStatusText(), wifiSsidText(), wifiSignalText(), wifiIpText(), wifiGatewayText())
        Qt.callLater(function() {
            root.syncWifiDerivedFields()
            root.requestInitialWifiPassword(false)
        })
    }

    onWifiListChanged: {
        console.log("WIFI_TAB_BINDING_CHECK list count", wifiList ? wifiList.length : 0)
        Qt.callLater(function() {
            root.syncWifiDerivedFields()
            root.requestInitialWifiPassword(false)
        })
    }

    onCellularStateChanged: {
        console.log("MODEM_5G_TAB_BINDING_CHECK state", cellularBadgeText(), cellularInterfaceText(), cellularSignalText(), cellularOperatorText(), cellularIpAddressText())
    }

    function safeText(value, fallback) {
        if (value === undefined || value === null || value === "")
            return fallback
        return String(value)
    }

    function firstValue(obj, keys, fallback) {
        if (!obj)
            return fallback
        for (var i = 0; i < keys.length; ++i) {
            var key = keys[i]
            if (obj[key] !== undefined && obj[key] !== null && obj[key] !== "")
                return obj[key]
        }
        return fallback
    }

    function currentWifiRow() {
        var activeSsid = safeText(firstValue(wifiState, ["ssid", "active_ssid", "connection", "connection_name"], ""), "")
        var selectedSsid = safeText(wifiSsid, "")
        var profile = safeText(wifiProfileName, "")
        if (!wifiList)
            return null
        for (var i = 0; i < wifiList.length; ++i) {
            var row = wifiList[i]
            if (!row)
                continue
            var rowSsid = safeText(row.ssid, "")
            var rowProfile = safeText(firstValue(row, ["profile_name", "connection", "connection_name"], ""), "")
            if (row.active || row.connected)
                return row
            if (activeSsid.length > 0 && (rowSsid === activeSsid || rowProfile === activeSsid))
                return row
            if (selectedSsid.length > 0 && (rowSsid === selectedSsid || rowProfile === profile))
                return row
        }
        return null
    }

    function selectedWifiRow() {
        var selectedSsid = safeText(wifiSsid, "")
        var selectedBssid = safeText(wifiBssid, "")
        if (!wifiList || selectedSsid.length === 0)
            return null
        for (var i = 0; i < wifiList.length; ++i) {
            var row = wifiList[i]
            if (!row)
                continue
            if (safeText(row.ssid, "") !== selectedSsid)
                continue
            if (selectedBssid.length > 0 && safeText(row.bssid, "") !== "" && safeText(row.bssid, "") !== selectedBssid)
                continue
            return row
        }
        return null
    }

    function numericSignalValue(value) {
        if (value === undefined || value === null || value === "")
            return NaN
        var raw = String(value).trim()
        if (raw.length === 0)
            return NaN
        var cleaned = raw.replace(/[^0-9\.\-]/g, "")
        if (cleaned.length === 0 || cleaned === "-" || cleaned === ".")
            return NaN
        return Number(cleaned)
    }

    function wifiSignalLevel(value) {
        if (value === undefined || value === null || value === "")
            return 0

        var n = numericSignalValue(value)

        // RSSI dBm case, usually negative. Examples: -45, -67, "-72 dBm".
        if (!isNaN(n) && n < 0) {
            if (n >= -55) return 5
            if (n >= -67) return 4
            if (n >= -75) return 3
            if (n >= -85) return 2
            return 1
        }

        // Percent case. Examples: 72, "72%".
        if (!isNaN(n)) {
            if (n >= 80) return 5
            if (n >= 60) return 4
            if (n >= 40) return 3
            if (n >= 20) return 2
            return 1
        }

        var s = String(value).toLowerCase()
        if (s.indexOf("excellent") >= 0 || s.indexOf("strong") >= 0) return 5
        if (s.indexOf("good") >= 0) return 4
        if (s.indexOf("fair") >= 0) return 3
        if (s.indexOf("weak") >= 0) return 2
        if (s.indexOf("poor") >= 0) return 1

        return 0
    }

    function signalPercent(value) {
        var level = wifiSignalLevel(value)
        if (level === 0)
            return 0

        var n = numericSignalValue(value)
        if (!isNaN(n) && n < 0) {
            if (n >= -50) return 100
            if (n <= -90) return 5
            return Math.max(5, Math.min(100, Math.round((n + 90) * 100 / 40)))
        }
        if (!isNaN(n))
            return Math.max(0, Math.min(100, Math.round(n)))

        return level * 20
    }

    function normalizedSignal(value) {
        return signalPercent(value)
    }

    function wifiSignalColor(value) {
        var level = wifiSignalLevel(value)
        if (level >= 5) return ui.good
        if (level === 4) return ui.accent
        if (level === 3) return ui.fair
        if (level === 2) return ui.weak
        if (level === 1) return ui.danger
        return ui.disabled
    }

    function wifiSignalQualityText(value) {
        var level = wifiSignalLevel(value)
        if (level >= 5) return "Excellent"
        if (level === 4) return "Good"
        if (level === 3) return "Fair"
        if (level === 2) return "Weak"
        if (level === 1) return "Poor"
        return "Unknown"
    }

    function wifiSignalValue() {
        var row = currentWifiRow()
        var selected = selectedWifiRow()
        return firstValue(wifiState, ["signal", "strength", "rssi", "quality"],
                          row ? firstValue(row, ["signal", "strength", "rssi", "quality"],
                                           selected ? firstValue(selected, ["signal", "strength", "rssi", "quality"], "") : "")
                              : (selected ? firstValue(selected, ["signal", "strength", "rssi", "quality"], "") : ""))
    }

    function wifiSignalRawText(value) {
        var v = value === undefined ? wifiSignalValue() : value
        if (v === undefined || v === null || v === "")
            return "--"
        var n = numericSignalValue(v)
        var raw = String(v)
        if (!isNaN(n) && n < 0)
            return String(n) + " dBm"
        if (raw.indexOf("%") >= 0)
            return String(signalPercent(v)) + "%"
        if (!isNaN(n))
            return String(signalPercent(v)) + "%"
        return raw
    }

    function wifiSignalText(value) {
        var v = value === undefined ? wifiSignalValue() : value
        if (v === undefined || v === null || v === "")
            return "--"
        return wifiSignalQualityText(v) + " · " + wifiSignalRawText(v)
    }

    function wifiRowSignalValue(row) {
        return row ? firstValue(row, ["signal", "strength", "rssi", "quality", "signalPercent"], "") : ""
    }

    function wifiSsidText() {
        var row = currentWifiRow()
        return safeText(firstValue(wifiState, ["ssid", "active_ssid"], row ? row.ssid : wifiSsid), "-")
    }

    function wifiStatusText() {
        if (!wifiEnabled)
            return "Disabled"
        if (currentWifiConnected)
            return "Connected"
        return "Disconnected"
    }

    function wifiStatusColor() {
        if (!wifiEnabled)
            return ui.disabled
        return currentWifiConnected ? ui.accent : ui.danger
    }

    function wifiIpText() {
        return safeText(firstValue(wifiState, ["current_ip", "ip", "ip_address", "ipAddress", "dev_ip4_plain"],
                                  safeText(wifiAdvancedCurrentIp || wifiAdvancedIpAddress, "")), "-")
    }

    function wifiGatewayText() {
        return safeText(firstValue(wifiState, ["current_gateway", "gateway", "ip_gateway", "ipGateway", "default_gateway", "dev_ip4_gateway"],
                                  safeText(wifiAdvancedCurrentGateway || wifiAdvancedGateway, "")), "-")
    }

    function wifiSubnetText() {
        return safeText(firstValue(wifiState, ["current_netmask", "netmask", "subnet_mask", "subnetMask", "dev_ip4_netmask"],
                                  wifiAdvancedSubnetMask), "-")
    }

    function wifiDnsText() {
        return safeText(firstValue(wifiState, ["dns", "dnsServers", "nameservers", "dev_ip4_dns"], wifiAdvancedDnsServers), "-")
    }

    function wifiSecurityText(row) {
        var r = row || currentWifiRow() || selectedWifiRow()
        return safeText(r ? firstValue(r, ["security", "securityType", "secure"], "") : "", "-")
    }

    function wifiMetaText(row) {
        var values = []
        if (isSavedWifi(row)) values.push("Saved")
        var band = safeText(row ? row.band : "", "")
        if (band.length > 0) values.push(band)
        var ch = safeText(row ? row.channel : "", "")
        if (ch.length > 0) values.push("CH " + ch)
        values.push(safeText(row ? row.security : "", "Open"))
        return values.join(" | ")
    }

    function wifiIsOpenNetwork(row) {
        if (!row)
            return false
        var security = safeText(firstValue(row, ["security", "securityType", "secure", "sec"], ""), "").toLowerCase()
        if (security.length === 0)
            return true
        return security.indexOf("open") >= 0 || security.indexOf("none") >= 0 || security.indexOf("--") >= 0 || security.indexOf("no security") >= 0
    }

    function wifiRequiresPassword(row) {
        if (!row || isSavedWifi(row))
            return false
        if (wifiIsOpenNetwork(row))
            return false
        var security = safeText(firstValue(row, ["security", "securityType", "secure", "sec"], ""), "").toLowerCase()
        return security.indexOf("wpa") >= 0 || security.indexOf("wep") >= 0 || security.indexOf("psk") >= 0 || security.indexOf("802.1x") >= 0
    }

    function wifiPasswordFieldVisible() {
        var row = selectedOrCurrentWifiRow()
        if (!hasSelectedWifi || selectedWifiConnected)
            return false
        if (wifiSelectedKnown || (row && isSavedWifi(row)))
            return true
        return wifiRequiresPassword(row)
    }

    function wifiPasswordFieldEnabled() {
        var row = selectedOrCurrentWifiRow()
        if (!wifiPasswordFieldVisible())
            return false
        if (wifiSelectedKnown || (row && isSavedWifi(row)))
            return wifiChangeSavedPassword
        return wifiRequiresPassword(row)
    }

    function wifiPasswordPlaceholder() {
        var row = selectedOrCurrentWifiRow()
        if (wifiSelectedKnown || (row && isSavedWifi(row))) {
            if (wifiChangeSavedPassword)
                return "Enter new password for saved WiFi"
            return "Saved password will be used"
        }
        if (row && wifiIsOpenNetwork(row))
            return "Open network: password not required"
        return "Enter WiFi password"
    }

    function clearWifiPasswordInput() {
        wifiPassword = ""
        wifiPasswordVisible = false
        wifiChangeSavedPassword = false
        wifiConnectErrorText = ""
    }

    function syncWifiDerivedFields() {
        if (!wifiState)
            return
        var iface = safeText(firstValue(wifiState, ["device", "interface", "iface"], ""), "")
        if (iface.length > 0)
            wifiIface = iface
        var ssid = safeText(firstValue(wifiState, ["ssid", "active_ssid"], ""), "")
        if (ssid.length > 0)
            wifiSsid = ssid
        var conn = safeText(firstValue(wifiState, ["connection", "connection_name", "profileName"], ""), "")
        if (conn.length > 0)
            wifiProfileName = conn
        var ip = wifiIpText()
        if (ip !== "-")
            wifiAdvancedCurrentIp = ip
        var gw = wifiGatewayText()
        if (gw !== "-")
            wifiAdvancedCurrentGateway = gw
        var mask = wifiSubnetText()
        if (mask !== "-")
            wifiAdvancedSubnetMask = mask
    }

    function requestInitialWifiPassword(force) {
        var ssid = safeText(firstValue(wifiState, ["ssid", "active_ssid", "connection"], wifiSsid), "")
        if (ssid.length === 0)
            return

        var row = currentWifiRow()
        var bssid = safeText(row ? row.bssid : wifiBssid, wifiBssid)
        var profile = safeText(row ? firstValue(row, ["profile_name", "connection", "connection_name"], wifiProfileName) : wifiProfileName, ssid)
        var known = row ? isSavedWifi(row) : wifiSelectedKnown
        if (!known && profile.length === 0)
            return

        var requestKey = wifiIface + "|" + ssid + "|" + bssid + "|" + profile
        if (!force && initialWifiPasswordRequestKey === requestKey)
            return

        initialWifiPasswordRequestKey = requestKey
        wifiSsid = ssid
        wifiBssid = bssid
        wifiProfileName = profile
        wifiSelectedKnown = known
        selectedWifiConnected = currentWifiConnected || (row ? isConnectedWifi(row) : false)

        wifiSavedPasswordRequested(wifiIface, wifiSsid, wifiBssid, wifiProfileName)
        // Do not open the IP configuration modal automatically here.
        // The modal is user-controlled only: long press WiFi card or explicit Config IP action.
    }

    function cellularValue(keys, fallback) {
        return safeText(firstValue(cellularState, keys, fallback), fallback)
    }

    function cellularInterfaceText() {
        return cellularValue(["interface", "iface", "device", "dev", "networkInterface"], cellularIface)
    }

    function cellularIpAddressText() {
        return cellularValue(["ipAddress", "ip_address", "ipv4", "ip", "current_ip", "dev_ip4_plain"], "No IPv4 assigned")
    }

    function cellularGatewayText() {
        return cellularValue(["gateway", "dev_ip4_gateway", "default_gateway", "current_gateway"], "--")
    }

    function cellularDnsText() {
        return cellularValue(["dns", "dnsServers", "nameservers", "dev_ip4_dns"], "-")
    }

    function cellularOperatorText() {
        return cellularValue(["operator", "carrier", "operatorName", "connection", "plmn", "operator_code"], "-")
    }

    function cellularSignalText() {
        var parts = []
        var sig = cellularValue(["signal", "rssi"], "")
        var rsrp = cellularValue(["rsrp"], "")
        var rsrq = cellularValue(["rsrq"], "")
        var sinr = cellularValue(["sinr"], "")
        if (sig.length > 0) parts.push(sig)
        if (rsrp.length > 0 && rsrp !== sig) parts.push("RSRP " + rsrp)
        if (rsrq.length > 0) parts.push("RSRQ " + rsrq)
        if (sinr.length > 0) parts.push("SINR " + sinr)
        return parts.length > 0 ? parts.join(" / ") : "--"
    }

    function cellularAccessText() {
        return cellularValue(["accessTech", "access_technology", "rat", "networkMode", "network_mode"], "-")
    }

    function cellularSimStatusText() {
        return cellularValue(["simStatus", "sim_status", "sim", "sim_state"], "Unknown")
    }

    function cellularDataStateText() {
        return cellularValue(["dataState", "data_state", "state"], cellularState && cellularState.connected ? "Connected" : "Disconnected")
    }

    function cellularBadgeText() {
        if (cellularState && cellularState.connected)
            return "Connected"
        var simLower = cellularSimStatusText().toLowerCase()
        if (simLower.indexOf("no sim") >= 0 || simLower.indexOf("not found") >= 0)
            return "No SIM"
        if (cellularIpAddressText().toLowerCase().indexOf("no ipv4") >= 0)
            return "No IP"
        return cellularDataStateText()
    }

    function cellularBadgeColor() {
        var b = cellularBadgeText().toLowerCase()
        if (b.indexOf("connected") >= 0)
            return ui.accent
        if (b.indexOf("no ip") >= 0 || b.indexOf("connecting") >= 0)
            return ui.warning
        if (b.indexOf("no sim") >= 0 || b.indexOf("not") >= 0)
            return ui.danger
        return ui.disabled
    }

    function moduleLogDisplayText() {
        if (cellularModuleLogs && cellularModuleLogs.length > 0)
            return cellularModuleLogs.join("  |  ")
        if (cellularState && cellularState.moduleLogs && cellularState.moduleLogs.length > 0)
            return cellularState.moduleLogs.join("  |  ")
        return "-"
    }

    function isSavedWifi(row) {
        if (!row)
            return false
        return !!(row.saved === true || row.known === true || row.configured === true || row.profileExists === true || row.remembered === true || row.previouslyConnected === true || row.autoConnect === true || row.connectedBefore === true || row.isSaved === true || row.hasProfile === true || safeText(row.profile_name || row.connection || row.connection_name, "") !== "")
    }

    function isConnectedWifi(row) {
        if (!row)
            return false
        var activeSsid = safeText(firstValue(wifiState, ["ssid", "active_ssid", "connection"], ""), "")
        return !!(row.active || row.connected || (activeSsid.length > 0 && safeText(row.ssid, "") === activeSsid))
    }

    function isSelectedWifi(row) {
        if (!row)
            return false
        if (safeText(row.ssid, "") !== safeText(wifiSsid, ""))
            return false
        if (safeText(wifiBssid, "").length === 0)
            return true
        return safeText(row.bssid, "") === wifiBssid
    }

    function selectWifiRow(row) {
        if (!row)
            return
        var sameSelection = isSelectedWifi(row)
        wifiSsid = safeText(row.ssid, wifiSsid)
        wifiBssid = safeText(row.bssid, "")
        wifiProfileName = safeText(row.profile_name || row.connection || row.connection_name, wifiProfileName)
        wifiSelectedKnown = isSavedWifi(row)
        selectedWifiConnected = isConnectedWifi(row)
        selectedWifiHasPassword = wifiSelectedKnown
        if (!sameSelection)
            clearWifiPasswordInput()
        if (wifiSelectedKnown) {
            // Ask backend only whether a saved profile/secret exists. Never prefill or display the real password in QML.
            wifiSavedPasswordRequested(wifiIface, wifiSsid, wifiBssid, wifiProfileName)
            // Selection must not open the IP configuration modal automatically.
        }
        console.log("WIFI_TAB_BINDING_CHECK selected", wifiSsid, "signal", safeText(row.signal, ""), "saved", isSavedWifi(row), "active", isConnectedWifi(row))
    }

    function selectedOrCurrentWifiRow() {
        var row = selectedWifiRow()
        if (row)
            return row
        return currentWifiRow()
    }

    function connectSelectedWifi() {
        var row = selectedOrCurrentWifiRow()
        if (row)
            selectWifiRow(row)

        if (!hasSelectedWifi) {
            wifiMessage = "Select a WiFi network first"
            wifiConnectErrorText = wifiMessage
            return
        }

        var saved = wifiSelectedKnown || (row && isSavedWifi(row))
        var open = row ? wifiIsOpenNetwork(row) : false
        var changingSavedPassword = saved && wifiChangeSavedPassword
        var passwordToSend = changingSavedPassword || (!saved && !open) ? safeText(wifiPassword, "") : ""

        if (changingSavedPassword && passwordToSend.length === 0) {
            wifiConnectErrorText = "Enter the new WiFi password before connecting."
            wifiMessage = wifiConnectErrorText
            console.log("[WiFiUI] Connect blocked until changed password is entered:", wifiSsid)
            return
        }

        if (!saved && !open && passwordToSend.length === 0) {
            wifiConnectErrorText = "Please enter WiFi password."
            wifiMessage = "Please enter WiFi password for " + wifiSsid
            console.log("[WiFiUI] Connect blocked until password is entered:", wifiSsid)
            return
        }

        wifiConnectErrorText = ""
        console.log("[WiFiUI] Connect clicked:", wifiSsid, "saved:", saved, "open:", open, "change_password:", changingSavedPassword, "bssid:", wifiBssid)
        // Do not log password. Empty password for saved profiles means backend/NetworkManager should use the stored secret.
        wifiConnectRequested(wifiIface, wifiSsid, passwordToSend, wifiBssid, wifiAutoConnect)
        clearWifiPasswordInput()
    }

    function disconnectSelectedWifi() {
        console.log("[WiFiUI] Disconnect clicked:", wifiIface, wifiSsid)
        wifiDisconnectRequested(wifiIface)
    }

    function forgetSelectedWifi() {
        var row = selectedOrCurrentWifiRow()
        if (row)
            selectWifiRow(row)

        if (!hasSelectedWifi || !wifiSelectedKnown) {
            wifiMessage = "Select a saved WiFi network first"
            return
        }

        console.log("[WiFiUI] Forget clicked:", wifiSsid, "profile:", wifiProfileName)
        wifiForgetRequested(wifiIface, wifiSsid, wifiBssid, wifiProfileName)
        clearWifiPasswordInput()
    }

    function openWifiConfigFromUserAction(row) {
        if (row)
            selectWifiRow(row)
        else
            row = selectedOrCurrentWifiRow()

        selectedWifiForConfig = row

        if (!hasSelectedWifi) {
            wifiMessage = "Select or long-press a WiFi network first"
            return
        }

        console.log("[WiFiUI] long press config:", wifiSsid, "profile:", wifiProfileName)
        wifiAdvancedVisible = true
        wifiAdvancedOpenRequested(wifiIface, wifiSsid, wifiBssid, wifiProfileName)
    }

    function openWifiConfigForSelected() {
        console.log("[WiFiUI] Config IP clicked")
        openWifiConfigFromUserAction(selectedOrCurrentWifiRow())
    }

    function splitDnsServerList(value) {
        var raw = safeText(value, "").trim()
        if (raw.length === 0)
            return []

        var parts = raw.split(/[\s,;]+/)
        var out = []
        for (var i = 0; i < parts.length; ++i) {
            var item = safeText(parts[i], "").trim()
            if (item.length > 0)
                out.push(item)
        }
        return out
    }

    function syncDnsFieldsFromCombined() {
        if (wifiAdvancedDnsSplitSyncing)
            return

        wifiAdvancedDnsSplitSyncing = true
        var list = splitDnsServerList(wifiAdvancedDnsServers)
        wifiAdvancedPrimaryDns = list.length > 0 ? list[0] : ""
        wifiAdvancedSecondaryDns = list.length > 1 ? list[1] : ""
        wifiAdvancedDnsSplitSyncing = false
    }

    function rebuildCombinedDnsServers() {
        if (wifiAdvancedDnsSplitSyncing)
            return

        wifiAdvancedDnsSplitSyncing = true
        var list = []
        var primary = safeText(wifiAdvancedPrimaryDns, "").trim()
        var secondary = safeText(wifiAdvancedSecondaryDns, "").trim()
        if (primary.length > 0)
            list.push(primary)
        if (secondary.length > 0)
            list.push(secondary)
        wifiAdvancedDnsServers = list.join(",")
        wifiAdvancedDnsSplitSyncing = false
    }

    function applyWifiConfigFromPanel() {
        rebuildCombinedDnsServers()
        console.log("[WiFiUI] Apply IP config clicked:", wifiAdvancedIpv4Mode, wifiAdvancedIpAddress, wifiAdvancedSubnetMask, wifiAdvancedGateway, wifiAdvancedDnsServers)
        wifiAdvancedSaveRequested(advancedSettingsPayload())
    }

    function openWifiAdvancedPanel() {
        wifiAdvancedVisible = true
    }

    function closeWifiAdvancedPanel() {
        wifiAdvancedVisible = false
    }

    function advancedSettingsPayload() {
        rebuildCombinedDnsServers()
        return {
            "iface": wifiIface,
            "ssid": wifiSsid,
            "bssid": wifiBssid,
            "profileName": wifiProfileName,
            "ipv4Mode": wifiAdvancedIpv4Mode,
            "ipAddress": wifiAdvancedIpAddress,
            "subnetMask": wifiAdvancedSubnetMask,
            "gateway": wifiAdvancedGateway,
            "dnsAutomatic": wifiAdvancedDnsAutomatic,
            "dnsServers": wifiAdvancedDnsServers
        }
    }

    onWifiAdvancedDnsServersChanged: {
        syncDnsFieldsFromCombined()
    }

    onForceSingleNetworkPageChanged: {
        if (forceSingleNetworkPage && selectedNetworkPage !== "wifi" && selectedNetworkPage !== "cellular")
            selectedNetworkPage = "wifi"
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Item {
            anchors.fill: parent
            anchors.margins: 34

            Row {
                id: internalTabs
                x: parent.width - 420
                y: 0
                spacing: 12
                visible: !root.hideInternalNetworkTabs

                Button {
                    width: 190
                    height: 44
                    text: "WiFi"
                    enabled: root.showWifiControls && !root.forceSingleNetworkPage
                    onClicked: root.selectedNetworkPage = "wifi"
                    contentItem: Text { text: parent.text; color: root.selectedNetworkPage === "wifi" ? "#001412" : ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15 }
                    background: Rectangle { radius: 10; color: root.selectedNetworkPage === "wifi" ? ui.accent : ui.field; border.color: root.selectedNetworkPage === "wifi" ? ui.accent : ui.border }
                }
                Button {
                    width: 190
                    height: 44
                    text: "5G"
                    enabled: root.showCellularControls && !root.forceSingleNetworkPage
                    onClicked: root.selectedNetworkPage = "cellular"
                    contentItem: Text { text: parent.text; color: root.selectedNetworkPage === "cellular" ? "#001412" : ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15 }
                    background: Rectangle { radius: 10; color: root.selectedNetworkPage === "cellular" ? ui.accent : ui.field; border.color: root.selectedNetworkPage === "cellular" ? ui.accent : ui.border }
                }
            }

            Item {
                id: wifiPage
                anchors.fill: parent
                visible: root.selectedNetworkPage === "wifi"

                Rectangle {
                    x: 0
                    y: 0
                    width: 500
                    height: parent.height
                    radius: 18
                    color: ui.panel
                    border.color: ui.border

                    Text { x: 28; y: 28; text: "WiFi"; color: ui.text; font.pixelSize: 28; font.bold: true }
                    Rectangle { x: parent.width - 158; y: 30; width: 130; height: 32; radius: 16; color: currentWifiConnected ? "#0d302e" : "#301515"; border.color: wifiStatusColor()
                        Text { anchors.centerIn: parent; text: wifiStatusText(); color: wifiStatusColor(); font.pixelSize: 13; font.bold: true }
                    }

                    Rectangle {
                        x: 28
                        y: 80
                        width: parent.width - 56
                        height: 135
                        radius: 14
                        color: ui.card
                        border.color: ui.border
                        Text { x: 24; y: 20; text: wifiSsidText(); color: ui.text; font.pixelSize: 22; font.bold: true; width: parent.width - 48; elide: Text.ElideRight }
                        Text { x: 24; y: 55; text: "Interface " + safeText(wifiIface, "wlan0") + " · " + wifiSecurityText(null); color: ui.subText; font.pixelSize: 15; width: parent.width - 48; elide: Text.ElideRight }
                        Rectangle { x: 24; y: 91; width: 150; height: 8; radius: 4; color: "#263449"
                            Rectangle { height: parent.height; radius: parent.radius; color: wifiSignalColor(wifiSignalValue()); width: parent.width * wifiSignalLevel(wifiSignalValue()) / 5 }
                        }
                        Text { x: 24; y: 106; width: parent.width - 170; text: "Signal " + wifiSignalText(); color: wifiSignalColor(wifiSignalValue()); font.pixelSize: 15; font.bold: true; elide: Text.ElideRight }
                        Rectangle { x: parent.width - 116; y: 86; width: 88; height: 28; radius: 14; visible: wifiSelectedKnown || (currentWifiRow() && isSavedWifi(currentWifiRow())); color: "#102b3d"; border.color: ui.saved
                            Text { anchors.centerIn: parent; text: "Saved"; color: ui.saved; font.pixelSize: 13; font.bold: true } }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            pressAndHoldInterval: 700
                            onClicked: {
                                var row = root.currentWifiRow()
                                if (row)
                                    root.selectWifiRow(row)
                            }
                            onPressAndHold: root.openWifiConfigFromUserAction(root.currentWifiRow())
                        }
                    }

                    Grid {
                        x: 28
                        y: 240
                        columns: 2
                        rowSpacing: 24
                        columnSpacing: 34
                        Repeater {
                            model: [
                                { label: "SSID", value: wifiSsidText() },
                                { label: "Status", value: wifiStatusText() },
                                { label: "IPv4", value: wifiIpText() },
                                { label: "Subnet", value: wifiSubnetText() },
                                { label: "Gateway", value: wifiGatewayText() },
                                { label: "DNS", value: wifiDnsText() },
                                { label: "Mode", value: wifiAdvancedIpv4Mode === "manual" ? "Static" : "DHCP" },
                                { label: "MAC", value: safeText(firstValue(wifiState, ["mac", "macAddress", "hwaddr"], ""), "TODO: bind backend") }
                            ]
                            Column {
                                width: 198
                                spacing: 6
                                clip: true

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
                                    height: 20
                                    text: safeText(modelData.value, "-")
                                    color: ui.text
                                    font.pixelSize: 16
                                    fontSizeMode: Text.Fit
                                    minimumPixelSize: 11
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    Text {
                        x: 28
                        y: parent.height - 246
                        width: parent.width - 56
                        text: "WiFi Password"
                        visible: wifiPasswordFieldVisible()
                        color: ui.subText
                        font.pixelSize: 13
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Row {
                        x: 28
                        y: parent.height - 224
                        spacing: 10
                        visible: wifiPasswordFieldVisible() && wifiSelectedKnown
                        Rectangle { width: 90; height: 28; radius: 14; color: "#102b3d"; border.color: ui.saved
                            Text { anchors.centerIn: parent; text: "Saved"; color: ui.saved; font.pixelSize: 13; font.bold: true }
                        }
                        Button { width: 155; height: 28; text: wifiChangeSavedPassword ? "Use saved password" : "Change password"
                            onClicked: {
                                wifiChangeSavedPassword = !wifiChangeSavedPassword
                                wifiPassword = ""
                                wifiConnectErrorText = ""
                            }
                            contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 12; elide: Text.ElideRight }
                            background: Rectangle { radius: 9; color: ui.field; border.color: ui.border }
                        }
                    }

                    TextField {
                        x: 28
                        y: parent.height - 190
                        width: wifiPasswordFieldEnabled() ? parent.width - 122 : parent.width - 56
                        height: 42
                        visible: wifiPasswordFieldVisible()
                        enabled: wifiPasswordFieldEnabled()
                        text: wifiPassword
                        echoMode: wifiPasswordVisible ? TextInput.Normal : TextInput.Password
                        placeholderText: wifiPasswordPlaceholder()
                        color: ui.text
                        font.pixelSize: 15
                        selectByMouse: true
                        onTextChanged: wifiPassword = text
                        background: Rectangle { radius: 10; color: ui.field; border.color: parent.activeFocus ? ui.accent : ui.border }
                    }

                    Button {
                        x: parent.width - 84
                        y: parent.height - 190
                        width: 56
                        height: 42
                        visible: wifiPasswordFieldVisible() && wifiPasswordFieldEnabled()
                        text: wifiPasswordVisible ? "Hide" : "Show"
                        onClicked: wifiPasswordVisible = !wifiPasswordVisible
                        contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 12; elide: Text.ElideRight }
                        background: Rectangle { radius: 10; color: ui.field; border.color: ui.border }
                    }

                    Text {
                        x: 28
                        y: parent.height - 145
                        width: parent.width - 56
                        height: 32
                        visible: wifiConnectErrorText.length > 0
                        text: wifiConnectErrorText
                        color: ui.danger
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    Row { x: 28; y: parent.height - 104; spacing: 10
                        Button { width: 132; height: 42; text: "Config IP"; enabled: root.showWifiControls && hasSelectedWifi; onClicked: root.openWifiConfigForSelected()
                            contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 14; elide: Text.ElideRight }
                            background: Rectangle { radius: 10; color: ui.field; border.color: ui.border } }
                        Button { width: 132; height: 42; text: "Forget"; enabled: root.showWifiControls && hasSelectedWifi && wifiSelectedKnown && !selectedWifiConnected && !wifiForgetBusy; onClicked: root.forgetSelectedWifi()
                            contentItem: Text { text: parent.text; color: enabled ? "white" : ui.muted; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 14; elide: Text.ElideRight }
                            background: Rectangle { radius: 10; color: enabled ? ui.warning : ui.field; border.color: enabled ? ui.warning : ui.border } }
                        Button { width: 132; height: 42; text: currentWifiConnected ? "Disconnect" : "Connect"; enabled: root.showWifiControls && hasSelectedWifi && !wifiConnectBusy; onClicked: currentWifiConnected ? root.disconnectSelectedWifi() : root.connectSelectedWifi()
                            contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 14; elide: Text.ElideRight }
                            background: Rectangle { radius: 10; color: currentWifiConnected ? ui.danger : ui.accent; border.color: currentWifiConnected ? ui.danger : ui.accent } }
                    }

                    Text { x: 28; y: parent.height - 34; width: parent.width - 56; height: 30; text: safeText(wifiMessage, ""); color: ui.subText; font.pixelSize: 13; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
                }

                Rectangle {
                    x: 530
                    y: 0
                    width: parent.width - 530
                    height: parent.height
                    radius: 18
                    color: ui.panel
                    border.color: ui.border

                    Text { x: 30; y: 28; width: 300; text: "Available Networks"; color: ui.text; font.pixelSize: 28; font.bold: true; elide: Text.ElideRight }
                    Text { x: 330; y: 38; width: parent.width - 760; text: wifiList.length + " network(s)"; color: ui.subText; font.pixelSize: 15; elide: Text.ElideRight }
                    Button { x: parent.width - 360; y: 24; width: 150; height: 44; text: "Refresh"; onClicked: { console.log("[WiFiUI] Refresh clicked"); root.wifiScanRequested() }
                        contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15; elide: Text.ElideRight }
                        background: Rectangle { radius: 10; color: ui.field; border.color: ui.border } }
                    Button { x: parent.width - 190; y: 24; width: 160; height: 44; text: wifiEnabled ? "WiFi Off" : "WiFi On"; enabled: root.showWifiControls; onClicked: { console.log("[WiFiUI] WiFi enabled toggled:", !wifiEnabled); root.wifiToggleRequested(!wifiEnabled) }
                        contentItem: Text { text: parent.text; color: "#001412"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15; elide: Text.ElideRight }
                        background: Rectangle { radius: 10; color: ui.accent; border.color: ui.accent } }

                    ListView {
                        x: 30
                        y: 88
                        width: parent.width - 60
                        height: parent.height - 118
                        clip: true
                        spacing: 14
                        model: wifiList
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 122
                            radius: 14
                            color: root.isSelectedWifi(modelData) ? "#16283d" : ui.card
                            border.color: root.isConnectedWifi(modelData) ? ui.accent : (root.isSavedWifi(modelData) ? ui.saved : ui.border)
                            border.width: root.isConnectedWifi(modelData) || root.isSelectedWifi(modelData) ? 2 : 1
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                pressAndHoldInterval: 700
                                onClicked: root.selectWifiRow(modelData)
                                onPressAndHold: root.openWifiConfigFromUserAction(modelData)
                            }

                            Text { x: 24; y: 20; visible: root.isSavedWifi(modelData); text: "★"; color: ui.saved; font.pixelSize: 18; font.bold: true }
                            Text { x: root.isSavedWifi(modelData) ? 50 : 24; y: 18; width: parent.width - 390; text: root.safeText(modelData.ssid, "Hidden network"); color: ui.text; font.pixelSize: 20; font.bold: true; elide: Text.ElideRight }
                            Text { x: root.isSavedWifi(modelData) ? 50 : 24; y: 50; width: parent.width - 390; text: root.wifiMetaText(modelData); color: ui.subText; font.pixelSize: 14; elide: Text.ElideRight }
                            Text { x: root.isSavedWifi(modelData) ? 50 : 24; y: 78; width: parent.width - 390; visible: root.isSavedWifi(modelData); text: "Previously connected"; color: ui.saved; font.pixelSize: 13; elide: Text.ElideRight }

                            Rectangle { x: parent.width - 330; y: 20; width: 92; height: 28; radius: 14; visible: root.isConnectedWifi(modelData); color: "#0d302e"; border.color: ui.accent
                                Text { anchors.centerIn: parent; text: "Active"; color: ui.accent; font.pixelSize: 13; font.bold: true } }
                            Rectangle { x: parent.width - 228; y: 20; width: 88; height: 28; radius: 14; visible: root.isSavedWifi(modelData); color: "#102b3d"; border.color: ui.saved
                                Text { anchors.centerIn: parent; text: "Saved"; color: ui.saved; font.pixelSize: 13; font.bold: true } }

                            Rectangle {
                                x: parent.width - 330
                                y: 69
                                width: 135
                                height: 8
                                radius: 4
                                color: "#263449"
                                Rectangle {
                                    height: parent.height
                                    radius: parent.radius
                                    color: root.wifiSignalColor(root.wifiRowSignalValue(modelData))
                                    width: parent.width * root.wifiSignalLevel(root.wifiRowSignalValue(modelData)) / 5
                                }
                            }
                            Text { x: parent.width - 330; y: 84; width: 135; text: root.wifiSignalText(root.wifiRowSignalValue(modelData)); color: root.wifiSignalColor(root.wifiRowSignalValue(modelData)); font.pixelSize: 13; font.bold: true; elide: Text.ElideRight; horizontalAlignment: Text.AlignLeft }

                            Button { x: parent.width - 150; y: 58; width: 66; height: 38; text: "Forget"; visible: root.isSavedWifi(modelData); enabled: root.showWifiControls && !root.isConnectedWifi(modelData) && !wifiForgetBusy
                                onClicked: {
                                    root.selectWifiRow(modelData)
                                    root.forgetSelectedWifi()
                                }
                                contentItem: Text { text: parent.text; color: enabled ? "white" : ui.muted; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { radius: 9; color: enabled ? ui.warning : ui.field; border.color: enabled ? ui.warning : ui.border }
                            }

                            Button { x: parent.width - 76; y: 58; width: 54; height: 38; text: root.isConnectedWifi(modelData) ? "Off" : "Go"; enabled: root.showWifiControls
                                onClicked: {
                                    root.selectWifiRow(modelData)
                                    if (root.isConnectedWifi(modelData))
                                        root.disconnectSelectedWifi()
                                    else
                                        root.connectSelectedWifi()
                                }
                                contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 13 }
                                background: Rectangle { radius: 9; color: root.isConnectedWifi(modelData) ? ui.danger : ui.accent; border.color: root.isConnectedWifi(modelData) ? ui.danger : ui.accent }
                            }
                        }
                    }
                }

                Rectangle {
                    id: wifiAdvancedOverlay
                    visible: wifiAdvancedVisible
                    z: 50
                    x: Math.max(30, (parent.width - width) / 2)
                    y: Math.max(30, (parent.height - height) / 2)
                    width: Math.min(960, parent.width - 80)
                    height: 370
                    radius: 18
                    color: ui.panel
                    border.color: ui.accent
                    border.width: 1

                    Rectangle { anchors.fill: parent; anchors.margins: 1; radius: 17; color: "transparent"; border.color: ui.borderSoft }

                    Text { x: 30; y: 24; width: parent.width - 240; text: "WiFi IPv4 Configuration · " + wifiSsidText(); color: ui.text; font.pixelSize: 24; font.bold: true; elide: Text.ElideRight }
                    Text { x: 30; y: 58; width: parent.width - 240; text: safeText(wifiSsid, "-") + " · " + safeText(wifiProfileName, "profile pending"); color: ui.subText; font.pixelSize: 14; elide: Text.ElideRight }
                    Button { x: parent.width - 74; y: 22; width: 44; height: 36; text: "X"; onClicked: root.closeWifiAdvancedPanel()
                        contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 14 }
                        background: Rectangle { radius: 9; color: ui.field; border.color: ui.border } }

                    Row { x: 30; y: 92; spacing: 12
                        Button { width: 180; height: 46; text: "Using DHCP"; onClicked: wifiAdvancedIpv4Mode = "dhcp"
                            contentItem: Text { text: parent.text; color: wifiAdvancedIpv4Mode === "dhcp" ? "#001412" : ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 14; elide: Text.ElideRight }
                            background: Rectangle { radius: 10; color: wifiAdvancedIpv4Mode === "dhcp" ? ui.accent : ui.field; border.color: wifiAdvancedIpv4Mode === "dhcp" ? ui.accent : ui.border } }
                        Button { width: 180; height: 46; text: "Static Manual"; onClicked: wifiAdvancedIpv4Mode = "manual"
                            contentItem: Text { text: parent.text; color: wifiAdvancedIpv4Mode === "manual" ? "#001412" : ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 14; elide: Text.ElideRight }
                            background: Rectangle { radius: 10; color: wifiAdvancedIpv4Mode === "manual" ? ui.accent : ui.field; border.color: wifiAdvancedIpv4Mode === "manual" ? ui.accent : ui.border } }
                        Text { width: parent.parent.width - 450; height: 46; text: wifiAdvancedBusy ? "Loading / saving IPv4 settings..." : safeText(wifiAdvancedMessage, "Current: " + wifiIpText() + " via " + wifiGatewayText()); color: ui.subText; font.pixelSize: 14; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    }

                    GridLayout {
                        x: 30
                        y: 156
                        width: wifiAdvancedOverlay.width - 60
                        columns: 2
                        rowSpacing: 16
                        columnSpacing: 24

                        ColumnLayout {
                            Layout.preferredWidth: (wifiAdvancedOverlay.width - 84) / 2
                            spacing: 6
                            Text { text: "IPv4 Address"; color: ui.subText; font.pixelSize: 13; font.bold: true; Layout.fillWidth: true }
                            TextField {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                text: wifiAdvancedIpAddress
                                enabled: wifiAdvancedIpv4Mode === "manual"
                                color: enabled ? ui.text : "#a9b4c2"
                                font.pixelSize: 15
                                selectByMouse: true
                                verticalAlignment: TextInput.AlignVCenter
                                leftPadding: 12
                                rightPadding: 12
                                topPadding: 0
                                bottomPadding: 0
                                onTextChanged: wifiAdvancedIpAddress = text
                                background: Rectangle {
                                    radius: 9
                                    color: parent.enabled ? ui.field : "#263241"
                                    border.color: parent.enabled ? (parent.activeFocus ? ui.accent : ui.border) : "#4a596c"
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.preferredWidth: (wifiAdvancedOverlay.width - 84) / 2
                            spacing: 6
                            Text { text: "Subnet Mask"; color: ui.subText; font.pixelSize: 13; font.bold: true; Layout.fillWidth: true }
                            TextField {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                text: wifiAdvancedSubnetMask
                                enabled: wifiAdvancedIpv4Mode === "manual"
                                color: enabled ? ui.text : "#a9b4c2"
                                font.pixelSize: 15
                                selectByMouse: true
                                verticalAlignment: TextInput.AlignVCenter
                                leftPadding: 12
                                rightPadding: 12
                                topPadding: 0
                                bottomPadding: 0
                                onTextChanged: wifiAdvancedSubnetMask = text
                                background: Rectangle {
                                    radius: 9
                                    color: parent.enabled ? ui.field : "#263241"
                                    border.color: parent.enabled ? (parent.activeFocus ? ui.accent : ui.border) : "#4a596c"
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.preferredWidth: (wifiAdvancedOverlay.width - 84) / 2
                            spacing: 6
                            Text { text: "Gateway"; color: ui.subText; font.pixelSize: 13; font.bold: true; Layout.fillWidth: true }
                            TextField {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                text: wifiAdvancedGateway
                                enabled: wifiAdvancedIpv4Mode === "manual"
                                color: enabled ? ui.text : "#a9b4c2"
                                font.pixelSize: 15
                                selectByMouse: true
                                verticalAlignment: TextInput.AlignVCenter
                                leftPadding: 12
                                rightPadding: 12
                                topPadding: 0
                                bottomPadding: 0
                                onTextChanged: wifiAdvancedGateway = text
                                background: Rectangle {
                                    radius: 9
                                    color: parent.enabled ? ui.field : "#263241"
                                    border.color: parent.enabled ? (parent.activeFocus ? ui.accent : ui.border) : "#4a596c"
                                }
                            }
                        }
                        RowLayout {
                            Layout.preferredWidth: (wifiAdvancedOverlay.width - 84) / 2
                            spacing: 10

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    text: "Primary DNS"
                                    color: ui.subText
                                    font.pixelSize: 13
                                    font.bold: true
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 44
                                    text: wifiAdvancedPrimaryDns
                                    enabled: !wifiAdvancedDnsAutomatic
                                    color: enabled ? ui.text : "#a9b4c2"
                                    font.pixelSize: 15
                                    selectByMouse: true
                                    verticalAlignment: TextInput.AlignVCenter
                                    leftPadding: 12
                                    rightPadding: 12
                                    topPadding: 0
                                    bottomPadding: 0
                                    onTextChanged: {
                                        if (wifiAdvancedDnsSplitSyncing)
                                            return
                                        wifiAdvancedPrimaryDns = text
                                        rebuildCombinedDnsServers()
                                    }
                                    background: Rectangle {
                                        radius: 9
                                        color: parent.enabled ? ui.field : "#263241"
                                        border.color: parent.enabled ? (parent.activeFocus ? ui.accent : ui.border) : "#4a596c"
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    text: "Secondary DNS"
                                    color: ui.subText
                                    font.pixelSize: 13
                                    font.bold: true
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 44
                                    text: wifiAdvancedSecondaryDns
                                    enabled: !wifiAdvancedDnsAutomatic
                                    color: enabled ? ui.text : "#a9b4c2"
                                    font.pixelSize: 15
                                    selectByMouse: true
                                    verticalAlignment: TextInput.AlignVCenter
                                    leftPadding: 12
                                    rightPadding: 12
                                    topPadding: 0
                                    bottomPadding: 0
                                    onTextChanged: {
                                        if (wifiAdvancedDnsSplitSyncing)
                                            return
                                        wifiAdvancedSecondaryDns = text
                                        rebuildCombinedDnsServers()
                                    }
                                    background: Rectangle {
                                        radius: 9
                                        color: parent.enabled ? ui.field : "#263241"
                                        border.color: parent.enabled ? (parent.activeFocus ? ui.accent : ui.border) : "#4a596c"
                                    }
                                }
                            }
                        }
                    }

                    Row { x: parent.width - 430; y: parent.height - 66; spacing: 12
                        Button { width: 150; height: 44; text: wifiAdvancedDnsAutomatic ? "DNS Auto" : "DNS Manual"; onClicked: wifiAdvancedDnsAutomatic = !wifiAdvancedDnsAutomatic
                            contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 14; elide: Text.ElideRight }
                            background: Rectangle { radius: 10; color: ui.field; border.color: ui.border } }
                        Button { width: 120; height: 44; text: "Cancel"; onClicked: root.closeWifiAdvancedPanel()
                            contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 14; elide: Text.ElideRight }
                            background: Rectangle { radius: 10; color: ui.field; border.color: ui.border } }
                        Button { width: 130; height: 44; text: wifiAdvancedBusy ? "Saving" : "Apply"; enabled: !wifiAdvancedBusy; onClicked: root.applyWifiConfigFromPanel()
                            contentItem: Text { text: parent.text; color: "#001412"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 14; elide: Text.ElideRight }
                            background: Rectangle { radius: 10; color: enabled ? ui.accent : ui.disabled; border.color: enabled ? ui.accent : ui.disabled } }
                    }
                }
            }

            Item {
                id: cellularPage
                anchors.fill: parent
                visible: root.selectedNetworkPage === "cellular"

                Rectangle {
                    x: 0
                    y: 0
                    width: 500
                    height: parent.height
                    radius: 18
                    color: ui.panel
                    border.color: ui.border

                    Text { x: 28; y: 28; text: "5G"; color: ui.text; font.pixelSize: 28; font.bold: true }
                    Rectangle { x: parent.width - 158; y: 30; width: 130; height: 32; radius: 16; color: "#1a2434"; border.color: cellularBadgeColor()
                        Text { anchors.centerIn: parent; text: cellularBadgeText(); color: cellularBadgeColor(); font.pixelSize: 13; font.bold: true }
                    }

                    Rectangle { x: 28; y: 80; width: parent.width - 56; height: 135; radius: 14; color: ui.card; border.color: ui.border
                        Text { x: 24; y: 20; width: parent.width - 48; text: cellularIpAddressText(); color: ui.text; font.pixelSize: 22; font.bold: true; elide: Text.ElideRight }
                        Text { x: 24; y: 55; width: parent.width - 48; text: "Interface " + cellularInterfaceText() + " · SIM " + cellularSimStatusText(); color: ui.subText; font.pixelSize: 15; elide: Text.ElideRight }
                        Text { x: 24; y: 91; width: parent.width - 48; text: cellularOperatorText() + " · " + cellularAccessText() + " · " + cellularSignalText(); color: ui.subText; font.pixelSize: 14; elide: Text.ElideRight }
                    }

                    Grid { x: 28; y: 240; columns: 2; rowSpacing: 24; columnSpacing: 34
                        Repeater { model: [
                            { label: "APN", value: cellularApn },
                            { label: "Operator", value: cellularOperatorText() },
                            { label: "Signal", value: cellularSignalText() },
                            { label: "IPv4", value: cellularIpAddressText() },
                            { label: "Gateway", value: cellularGatewayText() },
                            { label: "DNS", value: cellularDnsText() },
                            { label: "Mode", value: cellularAccessText() },
                            { label: "Device", value: cellularValue(["imei", "deviceId", "modemName"], "TODO: bind backend") }
                        ]
                            Column { width: 198; spacing: 6
                                Text { text: modelData.label; color: ui.subText; font.pixelSize: 14; font.bold: true; width: parent.width; elide: Text.ElideRight }
                                Text { text: safeText(modelData.value, "-"); color: ui.text; font.pixelSize: 16; width: parent.width; wrapMode: Text.WrapAnywhere; maximumLineCount: 2; elide: Text.ElideRight }
                            }
                        }
                    }

                    Row { x: 28; y: parent.height - 126; spacing: 12
                        Button { width: 130; height: 48; text: "Connect"; enabled: root.showCellularControls; onClicked: root.cellularConnectRequested(cellularApn, cellularIface, cellularAutoConnect)
                            contentItem: Text { text: parent.text; color: "#001412"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15 }
                            background: Rectangle { radius: 10; color: ui.accent; border.color: ui.accent } }
                        Button { width: 130; height: 48; text: "Disconnect"; enabled: root.showCellularControls; onClicked: root.cellularDisconnectRequested()
                            contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15 }
                            background: Rectangle { radius: 10; color: ui.danger; border.color: ui.danger } }
                        Button { width: 130; height: 48; text: cellularResetBusy ? "Restarting" : "Restart"; enabled: root.showCellularControls && !cellularResetBusy; onClicked: root.cellularResetModemRequested()
                            contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15 }
                            background: Rectangle { radius: 10; color: ui.field; border.color: ui.border } }
                    }
                    Text { x: 28; y: parent.height - 58; width: parent.width - 56; text: safeText(cellularMessage, ""); color: ui.subText; font.pixelSize: 14; wrapMode: Text.WordWrap }
                }

                Rectangle {
                    x: 530
                    y: 0
                    width: parent.width - 530
                    height: parent.height
                    radius: 18
                    color: ui.panel
                    border.color: ui.border

                    Text { x: 30; y: 28; text: "Cellular Status"; color: ui.text; font.pixelSize: 28; font.bold: true }
                    Button { x: parent.width - 390; y: 24; width: 160; height: 44; text: "List Modems"; onClicked: root.cellularListModemsRequested()
                        contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15 }
                        background: Rectangle { radius: 10; color: ui.field; border.color: ui.border } }
                    Button { x: parent.width - 210; y: 24; width: 180; height: 44; text: "Refresh"; onClicked: root.cellularRefreshRequested()
                        contentItem: Text { text: parent.text; color: ui.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 15 }
                        background: Rectangle { radius: 10; color: ui.field; border.color: ui.border } }

                    Row { x: 30; y: 90; spacing: 24
                        Repeater { model: [
                            { label: "Network", value: cellularDataStateText(), sub: cellularValue(["state", "registration_state"], "-") },
                            { label: "IPv4", value: cellularIpAddressText(), sub: cellularGatewayText() },
                            { label: "SIM", value: cellularSimStatusText(), sub: cellularValue(["iccid", "simIccid"], "-") }
                        ]
                            Rectangle {
                                width: 330
                                height: 125
                                radius: 14
                                color: ui.card
                                border.color: ui.border
                                clip: true

                                Rectangle {
                                    x: 18
                                    y: 18
                                    width: 14
                                    height: 14
                                    radius: 7
                                    color: modelData.label === "IPv4" && modelData.value.indexOf("No IPv4") >= 0 ? ui.warning : cellularBadgeColor()
                                }

                                Text {
                                    x: 44
                                    y: 13
                                    width: parent.width - 62
                                    text: modelData.label
                                    color: ui.subText
                                    font.pixelSize: 14
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    x: 18
                                    y: 48
                                    width: 294
                                    height: 30
                                    text: modelData.value
                                    color: ui.text
                                    font.pixelSize: 22
                                    font.bold: true
                                    fontSizeMode: Text.Fit
                                    minimumPixelSize: 13
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                Text {
                                    x: 18
                                    y: 84
                                    width: 294
                                    height: 20
                                    text: modelData.sub
                                    color: ui.muted
                                    font.pixelSize: 14
                                    fontSizeMode: Text.Fit
                                    minimumPixelSize: 10
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    Rectangle {
                        x: 30
                        y: 245
                        width: 510
                        height: 210
                        radius: 16
                        color: ui.card
                        border.color: ui.borderSoft
                        clip: true

                        Text { x: 24; y: 22; text: "Modem Details"; color: ui.text; font.pixelSize: 22; font.bold: true }

                        Grid {
                            x: 24
                            y: 70
                            columns: 2
                            rowSpacing: 16
                            columnSpacing: 70

                            Repeater {
                                model: [
                                    { label: "Device", value: cellularInterfaceText() },
                                    { label: "State", value: cellularValue(["state", "registration_state"], "-") },
                                    { label: "APN", value: cellularApn },
                                    { label: "Access", value: cellularAccessText() },
                                    { label: "IMEI", value: cellularValue(["imei"], "-") },
                                    { label: "SIM ICCID", value: cellularValue(["simIccid", "iccid"], "-") }
                                ]

                                Column {
                                    width: 180
                                    spacing: 4
                                    clip: true

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
                                        height: 18
                                        text: safeText(modelData.value, "-")
                                        color: ui.text
                                        font.pixelSize: 15
                                        fontSizeMode: Text.Fit
                                        minimumPixelSize: 10
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: serviceDeviceCard
                        x: 570
                        y: 245
                        width: parent.width - 600
                        height: 210
                        radius: 16
                        color: ui.card
                        border.color: ui.borderSoft
                        clip: true

                        Text {
                            x: 24
                            y: 22
                            text: "Service / Device"
                            color: ui.text
                            font.pixelSize: 22
                            font.bold: true
                        }

                        Grid {
                            id: serviceDeviceGrid
                            x: 24
                            y: 66
                            width: parent.width - 48
                            columns: 2
                            rowSpacing: 10
                            columnSpacing: 40

                            Repeater {
                                model: [
                                    { label: "quectel-CM", value: cellularValue(["qcmServiceState"], "-") },
                                    { label: "Recover", value: cellularValue(["recoverServiceState"], "-") },
                                    { label: "PCIe", value: cellularState && cellularState.pcieDetected === true ? "Detected" : cellularValue(["pcieDetected"], "-") },
                                    { label: "QMI", value: cellularState && cellularState.qmiDeviceReady === true ? "Ready" : cellularValue(["qmiDeviceReady"], "-") },
                                    { label: "Source", value: cellularValue(["source"], "-") },
                                    { label: "Last Error", value: cellularValue(["lastError"], "-") }
                                ]

                                Column {
                                    width: (serviceDeviceGrid.width - serviceDeviceGrid.columnSpacing) / 2
                                    spacing: 3
                                    clip: true

                                    Text {
                                        width: parent.width
                                        height: 17
                                        text: modelData.label
                                        color: ui.subText
                                        font.pixelSize: 14
                                        font.bold: true
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        width: parent.width
                                        height: 18
                                        text: safeText(modelData.value, "-")
                                        color: ui.text
                                        font.pixelSize: 14
                                        verticalAlignment: Text.AlignVCenter
                                        horizontalAlignment: Text.AlignLeft
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }

                    Rectangle { x: 30; y: 480; width: parent.width - 60; height: parent.height - 510; radius: 16; color: ui.card; border.color: ui.borderSoft
                        Text { x: 24; y: 18; text: "Module Log"; color: ui.text; font.pixelSize: 22; font.bold: true }
                        Text { x: 24; y: 62; width: parent.width - 48; height: parent.height - 84; text: moduleLogDisplayText(); color: ui.subText; font.pixelSize: 14; wrapMode: Text.WordWrap; elide: Text.ElideRight }
                    }
                }
            }
        }
    }
}
