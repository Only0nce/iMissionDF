import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import App 1.0
import App2 1.0

Item {
    id: networkManager
    width: 1920
    height: 1080

    property string interfaceName: "bond0"
    property bool useDhcp: true
    property string ipAddress: ""
    property string netmask: ""
    property string gateway: ""
    property string primaryDns: ""
    property string secondaryDns: ""
    property string statusMessage: ""
    property bool hardwareHasWireless: (typeof HardwareHasWireless === "undefined") ? false : HardwareHasWireless

    function normalizeSettingTabIndex() {
        if (!hardwareHasWireless && settingTabs.currentIndex === 1)
            settingTabs.currentIndex = 0
    }

    function cidrToNetmask(cidr) {
        const bits = parseInt(cidr)
        if (isNaN(bits) || bits < 0 || bits > 32)
            return ""
        const mask = (0xFFFFFFFF << (32 - bits)) >>> 0
        return [24, 16, 8, 0].map(s => (mask >>> s) & 255).join(".")
    }

    function netmaskToCidr(mask) {
        const parts = String(mask).split(".")
        if (parts.length !== 4)
            return 24

        let binary = ""
        for (let i = 0; i < 4; ++i) {
            const n = parseInt(parts[i])
            if (isNaN(n) || n < 0 || n > 255)
                return 24
            binary += ("00000000" + n.toString(2)).slice(-8)
        }

        return binary.split("1").length - 1
    }

    function refreshDhcpInfo() {
        const info = NetworkController.queryDhcpInfo(interfaceName)
        if (info) {
            ipAddress = info.ip || ""
            netmask = info.netmask || ""
            gateway = info.gateway || ""
            primaryDns = info.dns || ""
            secondaryDns = info.dns2 || ""
        }
    }

    function loadLanSetting() {
        const config = NetworkController.loadConfig(interfaceName)
        if (config) {
            useDhcp = config.mode === "dhcp"

            let ipRaw = config.ip || ""
            if (ipRaw && ipRaw.indexOf("/") >= 0) {
                ipAddress = ipRaw.split("/")[0]
                netmask = cidrToNetmask(ipRaw.split("/")[1])
            } else {
                ipAddress = ipRaw
                netmask = config.netmask || ""
            }

            gateway = config.gateway || ""
            primaryDns = config.dns || ""
            secondaryDns = config.dns2 || ""
        }

        if (useDhcp)
            refreshDhcpInfo()
    }

    function getRecorderSetting() {
        const recorderData = ReceiverRecorderConfigManager.loadConfig()
        recorderConf.alsa_dev = recorderData["alsa_dev"] || ""
        recorderConf.client_as_ip = recorderData["client_as_ip"] || ""
        recorderConf.client_as_freq = recorderData["client_as_freq"] || ""
        recorderConf.rtsp_server_ip = recorderData["rtsp_server_ip"] || ""
        recorderConf.rtsp_server_uri = recorderData["rtsp_server_uri"] || ""
        recorderConf.rtsp_server_port = recorderData["rtsp_server_port"] || ""
    }

    Component.onCompleted: {
        loadLanSetting()
        getRecorderSetting()
        normalizeSettingTabIndex()
    }

    onHardwareHasWirelessChanged: {
        normalizeSettingTabIndex()
    }

    Connections {
        target: NetworkController

        function onApplyNetworkConfigFinished(iface, ok, message, gateway, dns) {
            statusMessage = message
        }

        function onApplyNetworkConfigNmcliFinished(iface, ok, message) {
            statusMessage = message
            if (useDhcp)
                refreshDhcpInfo()
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
    }

    component PageTitle: ColumnLayout {
        property string title: ""
        property string subtitle: ""

        spacing: 4

        Text {
            text: parent.title
            color: ui.text
            font.pixelSize: 28
            font.bold: true
            Layout.fillWidth: true
        }

        Text {
            text: parent.subtitle
            color: ui.subText
            font.pixelSize: 14
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
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

    component FieldLabel: Text {
        color: ui.subText
        font.pixelSize: 13
        font.bold: true
    }

    component AppButton: Button {
        id: b
        property color baseColor: ui.blue
        property color textColor: "white"

        implicitHeight: 44

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

    Rectangle {
        anchors.fill: parent
        color: ui.bg

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                TabBar {
                    id: settingTabs
                    Layout.fillWidth: true
                    currentIndex: 0
                    onCurrentIndexChanged: networkManager.normalizeSettingTabIndex()

                    TabButton { text: "LAN" }
                    TabButton {
                        text: "WiFi / 5G"
                        visible: networkManager.hardwareHasWireless
                        enabled: networkManager.hardwareHasWireless
                    }
                    TabButton { text: "Recorder" }
                }
            }

            StackLayout {
                id: stack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: settingTabs.currentIndex

                // ============================================================
                // LAN PAGE
                // ============================================================
                Item {
                    Rectangle {
                        anchors.fill: parent
                        radius: 18
                        color: ui.panel
                        border.color: ui.border
                        border.width: 1

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 18
                            clip: true

                            ColumnLayout {
                                width: Math.max(parent.width - 36, 500)
                                spacing: 18

                                PageTitle {
                                    title: "Network Setting"
                                    subtitle: "LAN configuration for NetworkManager. Existing apply logic is kept through NetworkController.applyNetworkConfig()."
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 420
                                    radius: 16
                                    color: ui.panel2
                                    border.color: ui.border

                                    GridLayout {
                                        anchors.fill: parent
                                        anchors.margins: 18
                                        columns: 2
                                        rowSpacing: 14
                                        columnSpacing: 16

                                        FieldLabel { text: "Interface" }
                                        DarkField {
                                            text: interfaceName
                                            placeholderText: "bond0"
                                            Layout.fillWidth: true
                                            onTextChanged: interfaceName = text
                                        }

                                        FieldLabel { text: "DHCP" }
                                        CheckBox {
                                            id: dhcpCheckbox
                                            text: checked ? "Use DHCP" : "Static IP"
                                            checked: useDhcp
                                            onCheckedChanged: {
                                                useDhcp = checked
                                                if (useDhcp)
                                                    refreshDhcpInfo()
                                            }
                                            Layout.fillWidth: true
                                        }

                                        FieldLabel { text: "IP Address" }
                                        DarkField {
                                            text: ipAddress
                                            placeholderText: "192.168.10.10"
                                            enabled: !useDhcp
                                            Layout.fillWidth: true
                                            onTextChanged: ipAddress = text
                                        }

                                        FieldLabel { text: "Netmask" }
                                        DarkField {
                                            text: netmask
                                            placeholderText: "255.255.255.0"
                                            enabled: !useDhcp
                                            Layout.fillWidth: true
                                            onTextChanged: netmask = text
                                        }

                                        FieldLabel { text: "Gateway" }
                                        DarkField {
                                            text: gateway
                                            placeholderText: "192.168.10.1"
                                            enabled: !useDhcp
                                            Layout.fillWidth: true
                                            onTextChanged: gateway = text
                                        }

                                        FieldLabel { text: "Primary DNS" }
                                        DarkField {
                                            text: primaryDns
                                            placeholderText: "8.8.8.8"
                                            enabled: !useDhcp
                                            Layout.fillWidth: true
                                            onTextChanged: primaryDns = text
                                        }

                                        FieldLabel { text: "Secondary DNS" }
                                        DarkField {
                                            text: secondaryDns
                                            placeholderText: "8.8.4.4"
                                            enabled: !useDhcp
                                            Layout.fillWidth: true
                                            onTextChanged: secondaryDns = text
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    AppButton {
                                        text: "Reload"
                                        baseColor: ui.panel2
                                        Layout.fillWidth: true
                                        onClicked: loadLanSetting()
                                    }

                                    AppButton {
                                        text: "Apply Network Setting"
                                        baseColor: ui.accent
                                        Layout.fillWidth: true
                                        onClicked: {
                                            const cidr = netmaskToCidr(netmask)
                                            const ipWithCidr = ipAddress + "/" + cidr
                                            statusMessage = "Saving network config..."
                                            NetworkController.applyNetworkConfig(
                                                        interfaceName,
                                                        useDhcp ? "dhcp" : "static",
                                                        ipWithCidr,
                                                        gateway,
                                                        primaryDns + (secondaryDns ? "," + secondaryDns : "")
                                                        )
                                        }
                                    }
                                }

                                Text {
                                    text: statusMessage
                                    color: ui.subText
                                    font.pixelSize: 14
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }

                // ============================================================
                // WIFI / 5G PAGE
                // ============================================================
                Wifi5GSetting {
                    visible: networkManager.hardwareHasWireless
                    enabled: networkManager.hardwareHasWireless
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onRequestToast: function(text) {
                        statusMessage = text
                    }
                }

                // ============================================================
                // RECORDER PAGE
                // ============================================================
                Item {
                    Rectangle {
                        id: recorderConf
                        anchors.fill: parent
                        radius: 18
                        color: ui.panel
                        border.color: ui.border
                        border.width: 1

                        property string alsa_dev: ""
                        property string client_as_ip: ""
                        property real client_as_freq: 0
                        property string rtsp_server_ip: ""
                        property string rtsp_server_uri: ""
                        property int rtsp_server_port: 554

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 18
                            clip: true

                            ColumnLayout {
                                width: Math.max(parent.width - 36, 500)
                                spacing: 18

                                PageTitle {
                                    title: "Recorder Settings"
                                    subtitle: "Audio and RTSP recorder settings. Existing ReceiverRecorderConfigManager API is kept."
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 360
                                    radius: 16
                                    color: ui.panel2
                                    border.color: ui.border

                                    GridLayout {
                                        anchors.fill: parent
                                        anchors.margins: 18
                                        columns: 2
                                        rowSpacing: 14
                                        columnSpacing: 16

                                        FieldLabel { text: "ALSA Device" }
                                        DarkField {
                                            text: recorderConf.alsa_dev
                                            placeholderText: "recin1"
                                            Layout.fillWidth: true
                                            onTextChanged: recorderConf.alsa_dev = text
                                        }

                                        FieldLabel { text: "Client IP" }
                                        DarkField {
                                            text: recorderConf.client_as_ip
                                            placeholderText: "10.0.25.1"
                                            Layout.fillWidth: true
                                            onTextChanged: recorderConf.client_as_ip = text
                                        }

                                        FieldLabel { text: "Frequency" }
                                        DarkField {
                                            text: recorderConf.client_as_freq.toString()
                                            placeholderText: "985.500"
                                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                                            Layout.fillWidth: true
                                            onTextChanged: recorderConf.client_as_freq = parseFloat(text)
                                        }

                                        FieldLabel { text: "RTSP Server" }
                                        DarkField {
                                            text: recorderConf.rtsp_server_ip
                                            placeholderText: "192.168.10.31"
                                            Layout.fillWidth: true
                                            onTextChanged: recorderConf.rtsp_server_ip = text
                                        }

                                        FieldLabel { text: "RTSP URI" }
                                        DarkField {
                                            text: recorderConf.rtsp_server_uri
                                            placeholderText: "igate1"
                                            Layout.fillWidth: true
                                            onTextChanged: recorderConf.rtsp_server_uri = text
                                        }

                                        FieldLabel { text: "RTSP Port" }
                                        DarkField {
                                            text: recorderConf.rtsp_server_port.toString()
                                            placeholderText: "554"
                                            inputMethodHints: Qt.ImhDigitsOnly
                                            Layout.fillWidth: true
                                            onTextChanged: recorderConf.rtsp_server_port = parseInt(text)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    AppButton {
                                        text: "Reload"
                                        baseColor: ui.panel2
                                        Layout.fillWidth: true
                                        onClicked: getRecorderSetting()
                                    }

                                    AppButton {
                                        text: "Apply Recorder Settings"
                                        baseColor: ui.accent
                                        Layout.fillWidth: true
                                        onClicked: {
                                            ReceiverRecorderConfigManager.updateRecorderConfig(
                                                        recorderConf.alsa_dev,
                                                        recorderConf.client_as_ip,
                                                        recorderConf.client_as_freq,
                                                        recorderConf.rtsp_server_ip,
                                                        recorderConf.rtsp_server_uri,
                                                        recorderConf.rtsp_server_port
                                                        )
                                            statusMessage = "Recorder settings saved"
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
