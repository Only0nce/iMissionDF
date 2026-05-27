import QtQuick 2.15

Item {
    id: root

    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080
    clip: true
    z: 999

    property var networkBackend: (typeof networkController === "undefined") ? null : networkController
    property var mainWindowBackend: (typeof mainWindows === "undefined") ? null : mainWindows
    readonly property bool hasBackend: networkBackend !== null || mainWindowBackend !== null
    readonly property bool designMode: !hasBackend || Qt.application.arguments.join(" ").indexOf("qml2puppet") >= 0

    property bool hardwareHas5G: (typeof HardwareHas5G === "undefined") ? true : HardwareHas5G
    property string hardwareVersionName: (typeof HardwareVersionName === "undefined") ? "5G" : HardwareVersionName
    property bool useBackendJson: true
    readonly property bool showCellularControls: hardwareHas5G
    readonly property int networkGridColumns: showCellularControls && root.width > 1200 ? 2 : 1
    readonly property int networkCardHeight: showCellularControls ? 620 : Math.max(620, root.height - 220)

    property alias wifiEnabled: pageView.wifiEnabled
    property alias wifiIface: pageView.wifiIface
    property alias wifiSsid: pageView.wifiSsid
    property alias wifiBssid: pageView.wifiBssid
    property alias wifiProfileName: pageView.wifiProfileName
    property alias wifiPassword: pageView.wifiPassword
    property alias wifiAutoConnect: pageView.wifiAutoConnect
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

    function mockWifiRows() {
        return [
            {
                "ssid": "iMission-5G",
                "bssid": "00:11:22:33:44:55",
                "band": "5 GHz",
                "channel": 44,
                "signal": 82,
                "security": "WPA2",
                "active": true,
                "known": true,
                "profile_name": "iMission-5G"
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
        wifiSsid = safeText(wifiSsid, "iMission-5G")
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
            "operator": "Demo Carrier",
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

    function sendBackendCommand(obj) {
        if (useBackendJson && mainWindowBackend && mainWindowBackend.cppSubmitTextFiled) {
            mainWindowBackend.cppSubmitTextFiled(JSON.stringify(obj))
            return true
        }
        return false
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
            return
        }

        wifiState = networkBackend.wifiState(wifiIface)
        wifiEnabled = wifiState.enabled === undefined ? wifiEnabled : wifiState.enabled
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

        wifiMessage = wifiList.length > 0
                ? ("Found " + wifiList.length + " network(s)")
                : safeText(source.message || data.message || data.error, "No WiFi networks found")
    }

    function scanWifi() {
        wifiMessage = "Scanning..."
        if (sendBackendCommand({"menuID": "scan", "iface": wifiIface}))
            return

        if (!networkBackend) {
            wifiList = mockWifiRows()
            wifiMessage = "Found " + wifiList.length + " network(s)"
            return
        }

        applyWifiScanData(networkBackend.scanWifiPage(wifiIface), {})
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
                "operator": "Demo Carrier",
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
    }

    function toggleWifi() {
        var nextEnabled = !wifiEnabled
        wifiMessage = nextEnabled ? "Turning WiFi on..." : "Turning WiFi off..."
        if (sendBackendCommand({"menuID": "wifi_toggle", "on": nextEnabled}))
            return

        if (!networkBackend) {
            wifiEnabled = nextEnabled
            wifiMessage = wifiEnabled ? "WiFi mock enabled" : "WiFi mock disabled"
            if (!wifiEnabled)
                wifiList = []
            refreshWifiStatus()
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
    }

    function connectWifi() {
        wifiMessage = "Connecting..."
        if (sendBackendCommand({
            "menuID": "join",
            "iface": wifiIface,
            "ssid": wifiSsid,
            "password": wifiPassword,
            "bssid": wifiBssid,
            "autoConnect": wifiAutoConnect
        })) {
            return
        }

        if (!networkBackend) {
            wifiState = {
                "connected": true,
                "ssid": wifiSsid,
                "connection": wifiSsid,
                "signal": 82,
                "device": wifiIface,
                "enabled": wifiEnabled
            }
            wifiMessage = "Connected to " + wifiSsid + " (mock)"
            requestToast(wifiMessage)
            return
        }

        networkBackend.connectWifi(wifiIface, wifiSsid, wifiPassword, wifiAutoConnect, wifiBssid)
    }

    function disconnectWifi() {
        wifiMessage = "Disconnecting..."
        if (sendBackendCommand({"menuID": "disconnect", "device": wifiIface}))
            return

        if (!networkBackend) {
            var nextWifiState = copyObject(wifiState)
            nextWifiState.connected = false
            nextWifiState.ssid = ""
            wifiState = nextWifiState
            wifiMessage = "Disconnected (mock)"
            requestToast(wifiMessage)
            return
        }

        networkBackend.disconnectWifi(wifiIface)
    }

    function connectCellular() {
        cellularMessage = "Connecting..."
        if (sendBackendCommand({
            "menuID": "cellularConnect",
            "apn": cellularApn,
            "iface": cellularIface,
            "autoConnect": cellularAutoConnect
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

        networkBackend.connectCellular(cellularApn, cellularIface, cellularAutoConnect)
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
            return
        }

        if (obj.menuID === "cellularStatus" || obj.menuID === "lte_state") {
            cellularState = obj.status || obj.data || {}
            modemList = obj.modems || modemList
            return
        }

        if (obj.menuID === "wifi_toggle") {
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

        if (obj.menuID === "listModems") {
            modemList = obj.modems || []
            return
        }

        if (obj.menuID === "wifiOperationResult") {
            wifiMessage = safeText(obj.message, "")
            requestToast(wifiMessage)
            return
        }

        if (obj.menuID === "cellularOperationResult") {
            cellularMessage = safeText(obj.message, "")
            requestToast(cellularMessage)
            return
        }
    }

    Component.onCompleted: {
        if (designMode)
            applyMockData()
        else
            refreshAll()
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
            root.wifiMessage = message
            root.refreshWifiStatus()
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
        onWifiToggleRequested: root.toggleWifi()
        onWifiConnectRequested: root.connectWifi()
        onWifiDisconnectRequested: root.disconnectWifi()
        onCellularRefreshRequested: root.refreshCellularStatus()
        onCellularConnectRequested: root.connectCellular()
        onCellularDisconnectRequested: root.disconnectCellular()
    }
}
