import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import App 1.0

Item {
    id: root

    // Keep this page opaque and full-size.
    // Without this, when opened from QMLMap page the map can be visible behind it.
    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080
    clip: true
    z: 999

    // These values are injected from main.cpp.
    property bool hardwareHas5G: (typeof HardwareHas5G !== "undefined") ? HardwareHas5G : false
    property string hardwareVersionName: (typeof HardwareVersionName !== "undefined") ? HardwareVersionName : "NONE_5G"
    property bool useBackendJson: true

    property string wifiIface: "wlan0"
    property string wifiSsid: ""
    property string wifiPassword: ""
    property bool wifiAutoConnect: true
    property var wifiList: []
    property var wifiState: ({})
    property string wifiMessage: ""

    property string cellularIface: "*"
    property string cellularApn: "internet"
    property bool cellularAutoConnect: true
    property var cellularState: ({})
    property var modemList: []
    property string cellularMessage: ""

    signal requestToast(string text)

    function safeText(v, fallback) {
        if (v === undefined || v === null || v === "")
            return fallback
        return String(v)
    }

    function refreshWifiConfig() {
        var cfg = NetworkController.loadWifiConfig()
        wifiIface = safeText(cfg.interface, "wlan0")
        wifiSsid = safeText(cfg.ssid, "")
        wifiAutoConnect = cfg.autoConnect === undefined ? true : cfg.autoConnect
    }

    function refreshWifiStatus() {
        if (sendBackendCommand({"menuID":"wifiStatus", "iface":wifiIface}))
            return
        wifiState = NetworkController.wifiStatus(wifiIface)
    }

    function scanWifi() {
        wifiMessage = "Scanning..."
        if (sendBackendCommand({"menuID":"wifiScan", "iface":wifiIface}))
            return
        wifiList = NetworkController.scanWifi(wifiIface)
        wifiMessage = wifiList.length > 0 ? ("Found " + wifiList.length + " network(s)") : "No WiFi networks found"
    }

    function refreshCellularConfig() {
        var cfg = NetworkController.loadCellularConfig()
        cellularIface = safeText(cfg.interface, "*")
        cellularApn = safeText(cfg.apn, "internet")
        cellularAutoConnect = cfg.autoConnect === undefined ? true : cfg.autoConnect
    }

    function refreshCellularStatus() {
        if (sendBackendCommand({"menuID":"cellularStatus"}))
            return
        cellularState = NetworkController.cellularStatus()
        modemList = NetworkController.listModems()
    }

    function refreshAll() {
        if (sendBackendCommand({"menuID":"getWifi5GPage"}))
            return
        refreshWifiConfig()
        refreshWifiStatus()
        refreshCellularConfig()
        refreshCellularStatus()
    }


    function sendBackendCommand(obj) {
        if (useBackendJson && typeof mainWindows !== "undefined" && mainWindows) {
            mainWindows.cppSubmitTextFiled(JSON.stringify(obj))
            return true
        }
        return false
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

            if (obj.wifiStatus)
                wifiState = obj.wifiStatus

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

        if (obj.menuID === "wifiScan") {
            wifiList = obj.networks || []
            wifiMessage = wifiList.length > 0 ? ("Found " + wifiList.length + " network(s)") : "No WiFi networks found"
            return
        }

        if (obj.menuID === "wifiStatus") {
            wifiState = obj.status || {}
            return
        }

        if (obj.menuID === "cellularStatus") {
            cellularState = obj.status || {}
            modemList = obj.modems || modemList
            return
        }

        if (obj.menuID === "listModems") {
            modemList = obj.modems || []
            return
        }

        if (obj.menuID === "wifiOperationResult") {
            wifiMessage = safeText(obj.message, "")
            if (requestToast) root.requestToast(wifiMessage)
            return
        }

        if (obj.menuID === "cellularOperationResult") {
            cellularMessage = safeText(obj.message, "")
            if (requestToast) root.requestToast(cellularMessage)
            return
        }
    }

    Component.onCompleted: {
        if (!sendBackendCommand({"menuID":"getWifi5GPage"}))
            refreshAll()
    }


    Connections {
        target: (typeof mainWindows !== "undefined") ? mainWindows : null
        ignoreUnknownSignals: true

        function onCppCommand(jsonMsg) {
            var obj = null
            try {
                obj = JSON.parse(String(jsonMsg))
            } catch (e) {
                return
            }
            applyBackendMessage(obj)
        }
    }

    Connections {
        target: NetworkController

        function onWifiOperationFinished(action, ok, message) {
            wifiMessage = message
            refreshWifiStatus()
            if (requestToast) root.requestToast(message)
        }

        function onCellularOperationFinished(action, ok, message) {
            cellularMessage = message
            refreshCellularStatus()
            if (requestToast) root.requestToast(message)
        }
    }

    QtObject {
        id: ui
        readonly property color bg: "#0b1118"
        readonly property color panel: "#111a24"
        readonly property color panel2: "#162231"
        readonly property color border: "#2f4055"
        readonly property color text: "#e6edf3"
        readonly property color subText: "#9aa6b2"
        readonly property color accent: "#00c896"
        readonly property color blue: "#2fa6ff"
        readonly property color danger: "#ef4444"
        readonly property color warning: "#f59e0b"
    }

    function statusColor(connected) {
        return connected ? ui.accent : ui.danger
    }

    function signalColor(v) {
        if (v >= 70) return ui.accent
        if (v >= 40) return ui.warning
        return ui.danger
    }

    component StatusBadge: Rectangle {
        id: badge
        property string textValue: "Idle"
        property color badgeColor: ui.border

        radius: 999
        color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.18)
        border.color: badgeColor
        border.width: 1
        implicitWidth: badgeText.implicitWidth + 24
        implicitHeight: 30

        Text {
            id: badgeText
            anchors.centerIn: parent
            text: badge.textValue
            color: badge.badgeColor
            font.pixelSize: 13
            font.bold: true
        }
    }

    component AppButton: Button {
        id: b
        property color baseColor: ui.blue
        property color textColor: "white"

        implicitHeight: 42

        contentItem: Text {
            text: b.text
            color: b.textColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 14
            font.bold: true
        }

        background: Rectangle {
            radius: 10
            color: b.enabled ? b.baseColor : "#354052"
            opacity: b.down ? 0.75 : 1.0
            border.color: Qt.lighter(color, 1.15)
            border.width: 1
        }
    }

    component FieldLabel: Text {
        color: ui.subText
        font.pixelSize: 13
        font.bold: true
    }

    component DarkField: TextField {
        id: f
        color: ui.text
        placeholderTextColor: "#697789"
        selectionColor: ui.accent
        selectedTextColor: "#061016"
        font.pixelSize: 15
        leftPadding: 12
        rightPadding: 12
        implicitHeight: 42

        background: Rectangle {
            radius: 10
            color: "#0d1520"
            border.color: f.activeFocus ? ui.accent : ui.border
            border.width: 1
        }
    }

    component SignalBar: Item {
        id: sig
        property int value: 0
        implicitWidth: 150
        implicitHeight: 16

        Row {
            anchors.fill: parent
            spacing: 4

            Repeater {
                model: 5
                Rectangle {
                    width: 22
                    height: parent.height
                    radius: 4
                    color: (sig.value >= ((index + 1) * 20)) ? signalColor(sig.value) : "#273243"
                    border.color: "#334155"
                    border.width: 1
                }
            }
        }
    }

    Rectangle {
        id: solidPageBackground
        anchors.fill: parent
        z: 0
        color: ui.bg
        opacity: 1.0

        ColumnLayout {
            x: 18
            y: 100
            width: Math.max(root.width - 36, 1000)
            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "Wireless Network"
                        color: ui.text
                        font.pixelSize: 28
                        font.bold: true
                    }

                    Text {
                        text: "WiFi and 5G settings. Hardware version: " + root.hardwareVersionName
                        color: ui.subText
                        font.pixelSize: 14
                    }
                }

                StatusBadge {
                    textValue: root.hardwareHas5G ? "5G Hardware" : "No 5G Hardware"
                    badgeColor: root.hardwareHas5G ? ui.accent : ui.warning
                }

                AppButton {
                    text: "Refresh All"
                    baseColor: ui.panel2
                    Layout.preferredWidth: 140
                    onClicked: refreshAll()
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.width > 1200 ? 2 : 1
                columnSpacing: 18
                rowSpacing: 18

                // ========================================================
                // WiFi card
                // ========================================================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 620
                    radius: 18
                    color: ui.panel
                    border.color: ui.border
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 14

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Text {
                                text: "WiFi"
                                color: ui.text
                                font.pixelSize: 23
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            StatusBadge {
                                textValue: wifiState.connected ? "Connected" : "Disconnected"
                                badgeColor: statusColor(wifiState.connected)
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 92
                            radius: 14
                            color: ui.panel2
                            border.color: ui.border

                            GridLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                columns: 2
                                rowSpacing: 8
                                columnSpacing: 8

                                FieldLabel { text: "Interface" }
                                Text { text: wifiIface; color: ui.text; font.pixelSize: 15 }

                                FieldLabel { text: "Current SSID" }
                                RowLayout {
                                    Text {
                                        text: safeText(wifiState.ssid || wifiState.connection, "—")
                                        color: ui.text
                                        font.pixelSize: 15
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                    SignalBar { value: Number(wifiState.signal || 0) }
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 10

                            FieldLabel { text: "WiFi Interface" }
                            DarkField {
                                text: wifiIface
                                placeholderText: "wlan0"
                                onTextChanged: wifiIface = text
                                Layout.fillWidth: true
                            }

                            FieldLabel { text: "SSID" }
                            DarkField {
                                text: wifiSsid
                                placeholderText: "Select from list or type SSID"
                                onTextChanged: wifiSsid = text
                                Layout.fillWidth: true
                            }

                            FieldLabel { text: "Password" }
                            DarkField {
                                text: wifiPassword
                                placeholderText: "WiFi password"
                                echoMode: TextInput.Password
                                onTextChanged: wifiPassword = text
                                Layout.fillWidth: true
                            }

                            FieldLabel { text: "Auto Connect" }
                            CheckBox {
                                checked: wifiAutoConnect
                                text: checked ? "Enabled" : "Disabled"
                                onCheckedChanged: wifiAutoConnect = checked
                                Layout.fillWidth: true
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            AppButton {
                                text: "Scan"
                                baseColor: ui.panel2
                                Layout.fillWidth: true
                                onClicked: scanWifi()
                            }

                            AppButton {
                                text: "Connect"
                                baseColor: ui.accent
                                Layout.fillWidth: true
                                enabled: wifiSsid.length > 0
                                onClicked: {
                                    wifiMessage = "Connecting..."
                                    if (!sendBackendCommand({"menuID":"wifiConnect", "iface":wifiIface, "ssid":wifiSsid, "password":wifiPassword, "autoConnect":wifiAutoConnect}))
                                        NetworkController.connectWifi(wifiIface, wifiSsid, wifiPassword, wifiAutoConnect)
                                }
                            }

                            AppButton {
                                text: "Disconnect"
                                baseColor: ui.danger
                                Layout.fillWidth: true
                                onClicked: {
                                    wifiMessage = "Disconnecting..."
                                    if (!sendBackendCommand({"menuID":"wifiDisconnect", "iface":wifiIface}))
                                        NetworkController.disconnectWifi(wifiIface)
                                }
                            }
                        }

                        Text {
                            text: wifiMessage
                            color: ui.subText
                            font.pixelSize: 13
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 14
                            color: "#0d1520"
                            border.color: ui.border

                            ListView {
                                id: wifiListView
                                anchors.fill: parent
                                anchors.margins: 8
                                clip: true
                                model: wifiList

                                delegate: Rectangle {
                                    width: wifiListView.width
                                    height: 56
                                    radius: 10
                                    color: mouseArea.containsMouse ? "#1f2c3d" : "transparent"
                                    border.color: modelData.active ? ui.accent : "transparent"
                                    border.width: modelData.active ? 1 : 0

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 12

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Text {
                                                text: safeText(modelData.ssid, "Hidden")
                                                color: ui.text
                                                font.pixelSize: 15
                                                font.bold: modelData.active
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            Text {
                                                text: safeText(modelData.security, "Open")
                                                color: ui.subText
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }

                                        SignalBar { value: Number(modelData.signal || 0) }

                                        Text {
                                            text: String(modelData.signal || 0) + "%"
                                            color: ui.subText
                                            font.pixelSize: 12
                                            Layout.preferredWidth: 45
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }

                                    MouseArea {
                                        id: mouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {
                                            wifiSsid = modelData.ssid || ""
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ========================================================
                // 5G card
                // ========================================================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 620
                    radius: 18
                    color: ui.panel
                    border.color: root.hardwareHas5G ? ui.border : ui.warning
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 14

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Text {
                                text: "5G / Cellular"
                                color: ui.text
                                font.pixelSize: 23
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            StatusBadge {
                                textValue: root.hardwareHas5G
                                           ? (cellularState.connected ? "Connected" : "Disconnected")
                                           : "Disabled"
                                badgeColor: root.hardwareHas5G
                                            ? statusColor(cellularState.connected)
                                            : ui.warning
                            }
                        }

                        Rectangle {
                            visible: !root.hardwareHas5G
                            Layout.fillWidth: true
                            Layout.preferredHeight: 96
                            radius: 14
                            color: Qt.rgba(ui.warning.r, ui.warning.g, ui.warning.b, 0.10)
                            border.color: ui.warning

                            Text {
                                anchors.fill: parent
                                anchors.margins: 16
                                wrapMode: Text.WordWrap
                                color: ui.warning
                                font.pixelSize: 15
                                text: "This build is HW_NONE_5G. Cellular controls are shown for layout consistency, but backend commands are disabled."
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 150
                            radius: 14
                            color: ui.panel2
                            border.color: ui.border

                            GridLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                columns: 2
                                rowSpacing: 8
                                columnSpacing: 8

                                FieldLabel { text: "Modem" }
                                Text {
                                    text: safeText(cellularState.modemName, "—")
                                    color: ui.text
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                FieldLabel { text: "Operator" }
                                Text {
                                    text: safeText(cellularState.operator, "—")
                                    color: ui.text
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                FieldLabel { text: "State" }
                                Text {
                                    text: safeText(cellularState.state, "—")
                                    color: ui.text
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                FieldLabel { text: "Access Tech" }
                                Text {
                                    text: safeText(cellularState.accessTech, "—")
                                    color: ui.text
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                FieldLabel { text: "Signal" }
                                Text {
                                    text: safeText(cellularState.signal, "—")
                                    color: ui.text
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 10

                            FieldLabel { text: "Interface" }
                            DarkField {
                                text: cellularIface
                                placeholderText: "* or wwan0"
                                enabled: root.hardwareHas5G
                                onTextChanged: cellularIface = text
                                Layout.fillWidth: true
                            }

                            FieldLabel { text: "APN" }
                            DarkField {
                                text: cellularApn
                                placeholderText: "internet"
                                enabled: root.hardwareHas5G
                                onTextChanged: cellularApn = text
                                Layout.fillWidth: true
                            }

                            FieldLabel { text: "Auto Connect" }
                            CheckBox {
                                checked: cellularAutoConnect
                                enabled: root.hardwareHas5G
                                text: checked ? "Enabled" : "Disabled"
                                onCheckedChanged: cellularAutoConnect = checked
                                Layout.fillWidth: true
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            AppButton {
                                text: "Refresh"
                                baseColor: ui.panel2
                                Layout.fillWidth: true
                                onClicked: refreshCellularStatus()
                            }

                            AppButton {
                                text: "Connect"
                                baseColor: ui.accent
                                Layout.fillWidth: true
                                enabled: root.hardwareHas5G
                                onClicked: {
                                    cellularMessage = "Connecting..."
                                    if (!sendBackendCommand({"menuID":"cellularConnect", "apn":cellularApn, "iface":cellularIface, "autoConnect":cellularAutoConnect}))
                                        NetworkController.connectCellular(cellularApn, cellularIface, cellularAutoConnect)
                                }
                            }

                            AppButton {
                                text: "Disconnect"
                                baseColor: ui.danger
                                Layout.fillWidth: true
                                enabled: root.hardwareHas5G
                                onClicked: {
                                    cellularMessage = "Disconnecting..."
                                    if (!sendBackendCommand({"menuID":"cellularDisconnect", "connectionName":"cellular-5g"}))
                                        NetworkController.disconnectCellular("cellular-5g")
                                }
                            }
                        }

                        Text {
                            text: cellularMessage
                            color: ui.subText
                            font.pixelSize: 13
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 14
                            color: "#0d1520"
                            border.color: ui.border

                            ListView {
                                id: modemListView
                                anchors.fill: parent
                                anchors.margins: 8
                                clip: true
                                model: modemList

                                delegate: Rectangle {
                                    width: modemListView.width
                                    height: 62
                                    radius: 10
                                    color: "transparent"

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 2

                                        Text {
                                            text: safeText(modelData.name, "No modem")
                                            color: modelData.disabled ? ui.warning : ui.text
                                            font.pixelSize: 15
                                            font.bold: true
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: safeText(modelData.vendor, safeText(modelData.error, ""))
                                            color: ui.subText
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
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
}
