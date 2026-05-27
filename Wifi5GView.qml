import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080
    clip: true

    property bool hardwareHas5G: true
    property string hardwareVersionName: hardwareHas5G ? "5G" : "NONE_5G"
    property string selectedNetworkPage: "wifi"

    QtObject {
        id: layoutConfig

        // ============================================================
        // PAGE SIZE / MAIN LAYOUT
        // Adjust these values to tune the whole WiFi/5G page.
        // ============================================================
        readonly property int pageMarginLeft: 18
        readonly property int pageMarginRight: 18
        readonly property int pageTopOffset: 100
        readonly property int pageBottomMargin: 18
        readonly property int mainSpacing: 18
        readonly property int minContentWidth: 1000

        // ============================================================
        // HEADER SIZE
        // Adjust title, subtitle, selector, and top action button size here.
        // ============================================================
        readonly property int headerTitleFontSize: 28
        readonly property int headerSubtitleFontSize: 14
        readonly property int headerItemSpacing: 12
        readonly property int headerTextSpacing: 4
        readonly property int selectorButtonWidth: 88
        readonly property int refreshButtonWidth: 140

        // ============================================================
        // CARD SIZE
        // Shared sizing for the left advanced panel and right status panel.
        // ============================================================
        readonly property int cardRadius: 8
        readonly property int cardPadding: 18
        readonly property int cardSpacing: 14
        readonly property int cardBorderWidth: 1
        readonly property int leftPanelPreferredWidth: 560
        readonly property int rightPanelPreferredWidth: 780
        readonly property int wifiCardHeightWith5G: 850
        readonly property int wifiCardHeightOnly: 860
        readonly property int cellularCardHeight: 850
        readonly property int cardMinHeight: 850

        // ============================================================
        // WIFI SECTION SIZE
        // Adjust WiFi controls, status, and network list/card sizing here.
        // ============================================================
        readonly property int wifiTitleFontSize: 23
        readonly property int wifiStatusBoxHeight: 142
        readonly property int wifiStatusBoxRadius: 8
        readonly property int wifiStatusBoxPadding: 14
        readonly property int wifiStatusGridSpacing: 8
        readonly property int wifiHeaderSpacing: 8
        readonly property int wifiSectionSpacing: 12
        readonly property int wifiFormRowSpacing: 2
        readonly property int wifiFormColumnSpacing: 2
        readonly property int wifiFormLabelWidth: 118
        readonly property int wifiButtonRowSpacing: 10
        readonly property int wifiButtonColumnSpacing: 10
        readonly property int wifiActionButtonHeight: 42
        readonly property int wifiActionButtonMinWidth: 120
        readonly property int wifiButtonHeight: 42
        readonly property int wifiListRowHeight: 62
        readonly property int wifiListMargin: 8
        readonly property int wifiListRadius: 8
        readonly property int wifiStarFontSize: 16

        // ============================================================
        // 5G / CELLULAR SECTION SIZE
        // Adjust 5G controls, status, and modem card sizing here.
        // ============================================================
        readonly property int cellularTitleFontSize: 23
        readonly property int cellularStatusBoxHeight: 190
        readonly property int cellularStatusBoxRadius: 8
        readonly property int cellularStatusBoxPadding: 14
        readonly property int cellularStatusGridSpacing: 8
        readonly property int cellularFormRowSpacing: 10
        readonly property int cellularFormColumnSpacing: 12
        readonly property int cellularButtonHeight: 42
        readonly property int modemListRowHeight: 62
        readonly property int modemListMargin: 8
        readonly property int modemListRadius: 8

        // ============================================================
        // COMMON INPUT / TEXT SIZE
        // Shared values for labels, values, fields, and messages.
        // ============================================================
        readonly property int sectionTitleFontSize: 18
        readonly property int labelFontSize: 13
        readonly property int valueFontSize: 15
        readonly property int smallTextFontSize: 12
        readonly property int messageFontSize: 13
        readonly property int textFieldHeight: 40
        readonly property int textFieldRadius: 8
        readonly property int textFieldPadding: 6
        readonly property int buttonRadius: 8
        readonly property int buttonFontSize: 14
        readonly property int listTextSpacing: 2

        // ============================================================
        // STATUS BADGE / SIGNAL BAR SIZE
        // Shared indicator sizes for WiFi and 5G.
        // ============================================================
        readonly property int badgeHeight: 30
        readonly property int badgeHorizontalPadding: 24
        readonly property int badgeFontSize: 13
        readonly property int signalBarWidth: 150
        readonly property int signalBarHeight: 16
        readonly property int signalBarItemWidth: 22
        readonly property int signalBarSpacing: 4
        readonly property int signalBarItemRadius: 4
        readonly property int signalPercentWidth: 45

        // ============================================================
        // INLINE ADVANCED WIFI SETTINGS PANEL
        // Tune the IPv4/DNS config area that expands inside the WiFi card.
        // ============================================================
        readonly property int wifiAdvancedPanelHeight: 360
        readonly property int wifiAdvancedPanelMinHeight: 260
        readonly property int wifiAdvancedPanelRadius: 8
        readonly property int wifiAdvancedPanelPadding: 6
        readonly property int wifiAdvancedPanelSpacing: 8
        readonly property int wifiAdvancedFieldHeight: 42
        readonly property int wifiAdvancedFooterHeight: 42
        readonly property int passwordEyeButtonWidth: 56
        readonly property int busyIndicatorSize: 18
    }

    readonly property bool showCellularControls: hardwareHas5G
    readonly property int networkGridColumns: root.width > 1200 ? 2 : 1
    readonly property int networkCardHeight: Math.max(layoutConfig.cardMinHeight,
                                                      root.showCellularControls
                                                      ? layoutConfig.wifiCardHeightWith5G
                                                      : layoutConfig.wifiCardHeightOnly)
    readonly property bool currentWifiConnected: !!(wifiState && wifiState.connected)
    readonly property bool selectedWifiSaved: wifiSelectedKnown || safeText(wifiProfileName, "") !== ""
    readonly property bool hasSelectedWifi: safeText(wifiSsid, "").length > 0
    readonly property bool wifiAdvancedAvailable: selectedNetworkPage === "wifi"
                                                  && hasSelectedWifi
                                                  && selectedWifiConnected
    readonly property string selectedWifiSsid: wifiSsid
    readonly property string selectedWifiBssid: wifiBssid
    readonly property string selectedWifiProfileName: wifiProfileName
    readonly property bool selectedWifiKnown: wifiSelectedKnown

    property bool wifiEnabled: true
    property string wifiIface: "wlP9p1s0"
    property string wifiSsid: "Office-WiFi"
    property string wifiBssid: "00:11:22:33:44:55"
    property string wifiProfileName: "Office-WiFi"
    property string wifiPassword: ""
    property bool wifiAutoConnect: true
    property bool wifiSelectedKnown: true
    property bool selectedWifiConnected: true
    property bool selectedWifiHasPassword: false
    property bool wifiPasswordVisible: false
    property bool wifiToggleBusy: false
    property bool wifiConnectBusy: false
    property bool wifiForgetBusy: false
    property bool wifiAdvancedBusy: false
    property string pendingWifiAction: ""
    property bool wifiAdvancedVisible: false
    property string wifiAdvancedMessage: ""
    property string wifiAdvancedIpv4Mode: "dhcp"
    property string wifiAdvancedIpAddress: "192.168.10.24"
    property string wifiAdvancedSubnetMask: "255.255.255.0"
    property string wifiAdvancedGateway: "192.168.10.1"
    property bool wifiAdvancedDnsAutomatic: true
    property string wifiAdvancedDnsServers: "8.8.8.8,1.1.1.1"
    property string wifiAdvancedCurrentIp: "192.168.10.24"
    property string wifiAdvancedCurrentPrefix: "24"
    property string wifiAdvancedCurrentGateway: "192.168.10.1"
    property string wifiAdvancedConnectionName: "Office-WiFi"
    property var wifiList: [
        {
            "ssid": "Office-WiFi",
            "bssid": "00:11:22:33:44:55",
            "band": "5 GHz",
            "channel": 44,
            "signal": 82,
            "security": "WPA2",
            "active": true,
            "known": true,
            "saved": true,
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
            "saved": false,
            "profile_name": ""
        }
    ]
    property var wifiState: ({
        "connected": true,
        "ssid": "Office-WiFi",
        "connection": "Office-WiFi",
        "signal": 82,
        "current_ip": "192.168.10.24",
        "current_gateway": "192.168.10.1",
        "current_netmask": "255.255.255.0",
        "device": "wlP9p1s0",
        "enabled": true
    })
    property string wifiMessage: "Mock data for design preview"

    property string cellularIface: "*"
    property string cellularApn: "internet"
    property bool cellularAutoConnect: true
    property var cellularState: ({
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
    })
    property var modemList: [
        {
            "name": "Quectel 5G",
            "vendor": "Mock modem",
            "disabled": false
        }
    ]
    property string cellularMessage: "Mock data for design preview"

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
    signal cellularListModemsRequested()

    QtObject {
        id: ui

        readonly property color bg: "#0b1118"
        readonly property color panel: "#111a24"
        readonly property color panel2: "#162231"
        readonly property color field: "#0d1520"
        readonly property color border: "#2f4055"
        readonly property color text: "#e6edf3"
        readonly property color subText: "#9aa6b2"
        readonly property color accent: "#00c896"
        readonly property color blue: "#2fa6ff"
        readonly property color danger: "#ef4444"
        readonly property color warning: "#f59e0b"
        readonly property color savedStar: "#facc15"
    }

    onHardwareHas5GChanged: {
        if (!showCellularControls && selectedNetworkPage === "cellular")
            selectedNetworkPage = "wifi"
    }

    onSelectedNetworkPageChanged: {
        if (selectedNetworkPage !== "wifi")
            wifiAdvancedVisible = false
    }

    onSelectedWifiConnectedChanged: {
        if (!selectedWifiConnected)
            wifiAdvancedVisible = false
    }

    onWifiAdvancedAvailableChanged: {
        if (!wifiAdvancedAvailable)
            wifiAdvancedVisible = false
    }

    onWifiEnabledChanged: {
        if (!wifiEnabled)
            wifiAdvancedVisible = false
    }

    Component.onCompleted: {
        if (!showCellularControls && selectedNetworkPage === "cellular")
            selectedNetworkPage = "wifi"
    }

    function safeText(value, fallback) {
        if (value === undefined || value === null || value === "")
            return fallback
        return String(value)
    }

    function statusColor(connected) {
        return connected ? ui.accent : ui.danger
    }

    function isSavedWifi(row) {
        if (!row)
            return false
        return !!(row.known || row.saved || safeText(row.profile_name, "") !== "" || safeText(row.connection, "") !== "")
    }

    function isConnectedWifi(row) {
        if (!row)
            return false
        return !!(row.active || row.connected)
    }

    function isSelectedWifi(row) {
        if (!row)
            return false
        if (safeText(row.ssid, "") !== wifiSsid)
            return false
        if (wifiBssid.length > 0 && safeText(row.bssid, "") !== "" && row.bssid !== wifiBssid)
            return false
        return true
    }

    function selectWifiRow(row) {
        if (!row)
            return
        wifiSsid = row.ssid || ""
        wifiBssid = row.bssid || ""
        wifiProfileName = row.profile_name || row.connection || ""
        wifiSelectedKnown = isSavedWifi(row)
        selectedWifiConnected = isConnectedWifi(row)
        selectedWifiHasPassword = false

        if (wifiSelectedKnown) {
            var savedPassword = safeText(row.savedPassword || row.password, "")
            wifiPassword = savedPassword
            selectedWifiHasPassword = savedPassword.length > 0
            wifiSavedPasswordRequested(wifiIface, wifiSsid, wifiBssid, wifiProfileName)
        } else {
            wifiPassword = ""
            wifiPasswordVisible = false
        }

        if (!selectedWifiConnected)
            wifiAdvancedVisible = false
    }

    function wifiMetaText(row) {
        var values = [
            isSavedWifi(row) ? "Saved" : "",
            safeText(row.band, ""),
            row.channel ? ("CH " + row.channel) : "",
            safeText(row.security, "Open")
        ]
        return values.filter(function(value) { return value !== "" }).join(" | ")
    }

    function wifiCurrentSummary() {
        var ip = safeText(wifiAdvancedCurrentIp, safeText(wifiState.current_ip || wifiState.ip, "-"))
        var prefix = safeText(wifiAdvancedCurrentPrefix, "")
        var gateway = safeText(wifiAdvancedCurrentGateway,
                               safeText(wifiState.current_gateway || wifiState.gateway, "-"))
        var ipText = prefix.length > 0 && ip !== "-" ? (ip + "/" + prefix) : ip
        return "Now: " + ipText + " via " + gateway
    }

    function openWifiAdvancedPanel() {
        wifiAdvancedVisible = true
    }

    function closeWifiAdvancedPanel() {
        wifiAdvancedVisible = false
    }

    function advancedSettingsPayload() {
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

    Rectangle {
        anchors.fill: parent
        color: ui.bg

        ColumnLayout {
            x: layoutConfig.pageMarginLeft
            y: layoutConfig.pageTopOffset
            width: Math.max(root.width - layoutConfig.pageMarginLeft - layoutConfig.pageMarginRight,
                            layoutConfig.minContentWidth)
            spacing: layoutConfig.mainSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: layoutConfig.headerItemSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: layoutConfig.headerTextSpacing

                    Text {
                        text: "Wireless Network"
                        color: ui.text
                        font.pixelSize: layoutConfig.headerTitleFontSize
                        font.bold: true
                    }

                    Text {
                        text: root.showCellularControls
                              ? "WiFi and 5G settings. Hardware version: " + root.hardwareVersionName
                              : "WiFi settings. Hardware version: " + root.hardwareVersionName
                        color: ui.subText
                        font.pixelSize: layoutConfig.headerSubtitleFontSize
                    }
                }

                AppButton {
                    text: "WiFi"
                    baseColor: root.selectedNetworkPage === "wifi" ? ui.accent : ui.panel2
                    buttonHeight: layoutConfig.wifiButtonHeight
                    buttonRadius: layoutConfig.buttonRadius
                    buttonFontSize: layoutConfig.buttonFontSize
                    Layout.preferredWidth: layoutConfig.selectorButtonWidth
                    onClicked: root.selectedNetworkPage = "wifi"
                }

                AppButton {
                    visible: root.showCellularControls
                    text: "5G"
                    baseColor: root.selectedNetworkPage === "cellular" ? ui.accent : ui.panel2
                    buttonHeight: layoutConfig.cellularButtonHeight
                    buttonRadius: layoutConfig.buttonRadius
                    buttonFontSize: layoutConfig.buttonFontSize
                    Layout.preferredWidth: layoutConfig.selectorButtonWidth
                    onClicked: root.selectedNetworkPage = "cellular"
                }

                AppButton {
                    text: "Refresh All"
                    baseColor: ui.panel2
                    buttonHeight: layoutConfig.wifiButtonHeight
                    buttonRadius: layoutConfig.buttonRadius
                    buttonFontSize: layoutConfig.buttonFontSize
                    Layout.preferredWidth: layoutConfig.refreshButtonWidth
                    onClicked: root.refreshAllRequested()
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.networkGridColumns
                columnSpacing: layoutConfig.mainSpacing
                rowSpacing: layoutConfig.mainSpacing

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredWidth: layoutConfig.leftPanelPreferredWidth
                    Layout.preferredHeight: root.networkCardHeight
                    radius: layoutConfig.cardRadius
                    color: ui.panel
                    border.color: ui.border
                    border.width: layoutConfig.cardBorderWidth

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: layoutConfig.cardPadding
                        spacing: layoutConfig.cardSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: layoutConfig.wifiHeaderSpacing

                            StatusBadge {
                                textValue: root.selectedNetworkPage === "wifi" ? "WiFi" : "5G"
                                badgeColor: root.selectedNetworkPage === "wifi" ? ui.blue : ui.accent
                                badgeHeight: layoutConfig.badgeHeight
                                horizontalPadding: layoutConfig.badgeHorizontalPadding
                                badgeFontSize: layoutConfig.badgeFontSize
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: layoutConfig.headerTextSpacing

                                Text {
                                    text: root.selectedNetworkPage === "wifi" ? "WiFi" : "5G / Cellular"
                                    color: ui.text
                                    font.pixelSize: layoutConfig.sectionTitleFontSize
                                    font.bold: true
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: root.selectedNetworkPage === "wifi"
                                          ? "Basic settings and actions"
                                          : "Cellular interface and APN"
                                    color: ui.subText
                                    font.pixelSize: layoutConfig.smallTextFontSize
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }

                            StatusBadge {
                                visible: root.selectedNetworkPage === "wifi"
                                textValue: !root.wifiEnabled
                                           ? "Off"
                                           : (root.selectedWifiConnected ? "Connected" : "Ready")
                                badgeColor: !root.wifiEnabled
                                            ? ui.warning
                                            : root.statusColor(root.selectedWifiConnected)
                                badgeHeight: layoutConfig.badgeHeight
                                horizontalPadding: layoutConfig.badgeHorizontalPadding
                                badgeFontSize: layoutConfig.badgeFontSize
                            }
                        }

                        ColumnLayout {
                            visible: root.selectedNetworkPage === "wifi"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: layoutConfig.wifiSectionSpacing

                            // Text {
                            //     text: "Basic WiFi Settings"
                            //     color: ui.text
                            //     font.pixelSize: layoutConfig.valueFontSize
                            //     font.bold: true
                            //     Layout.fillWidth: true
                            // }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: wifiBasicForm.implicitHeight
                                                        + layoutConfig.wifiAdvancedPanelPadding * 2
                                radius: layoutConfig.wifiStatusBoxRadius
                                color: ui.panel2
                                border.color: ui.border
                                border.width: layoutConfig.cardBorderWidth

                                GridLayout {
                                    id: wifiBasicForm
                                    anchors.fill: parent
                                    anchors.margins: layoutConfig.wifiAdvancedPanelPadding
                                    anchors.topMargin: 6
                                    anchors.bottomMargin: 6
                                    columnSpacing: 2
                                    rowSpacing: 0
                                    columns: 2

                                    FieldLabel {
                                        text: "WiFi Interface"
                                        textColor: ui.subText
                                        labelFontSize: layoutConfig.labelFontSize
                                        Layout.preferredWidth: layoutConfig.wifiFormLabelWidth
                                    }

                                    Text {
                                        text: root.safeText(root.wifiIface, "-")
                                        color: ui.text
                                        font.pixelSize: layoutConfig.valueFontSize
                                        font.bold: true
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: layoutConfig.textFieldHeight
                                    }

                                    FieldLabel {
                                        text: "SSID"
                                        textColor: ui.subText
                                        labelFontSize: layoutConfig.labelFontSize
                                        Layout.preferredWidth: layoutConfig.wifiFormLabelWidth
                                    }

                                    Text {
                                        text: root.safeText(root.wifiSsid, "-")
                                        color: ui.text
                                        font.pixelSize: layoutConfig.valueFontSize
                                        font.bold: true
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: layoutConfig.textFieldHeight
                                    }
                                    FieldLabel {
                                        text: "Password"
                                        textColor: ui.subText
                                        labelFontSize: layoutConfig.labelFontSize
                                        Layout.preferredWidth: layoutConfig.wifiFormLabelWidth
                                    }
                                    DarkField {
                                        text: root.wifiPassword
                                        placeholderText: "WiFi password"
                                        echoMode: root.wifiPasswordVisible ? TextInput.Normal : TextInput.Password
                                        textColor: ui.text
                                        accentColor: ui.accent
                                        borderColor: ui.border
                                        fillColor: ui.field
                                        fieldHeight: layoutConfig.textFieldHeight
                                        fieldRadius: layoutConfig.textFieldRadius
                                        fieldFontSize: layoutConfig.valueFontSize
                                        fieldPadding: layoutConfig.textFieldPadding
                                        actionVisible: true
                                        actionText: root.wifiPasswordVisible ? "Hide" : "Show"
                                        actionButtonWidth: layoutConfig.passwordEyeButtonWidth
                                        actionFontSize: layoutConfig.smallTextFontSize
                                        Layout.fillWidth: true
                                        onTextEdited: {
                                            root.wifiPassword = text
                                            root.selectedWifiHasPassword = text.length > 0
                                        }
                                        onActionClicked: root.wifiPasswordVisible = !root.wifiPasswordVisible
                                    }

                                    FieldLabel {
                                        text: "Auto Connect"
                                        textColor: ui.subText
                                        labelFontSize: layoutConfig.labelFontSize
                                        Layout.preferredWidth: layoutConfig.wifiFormLabelWidth
                                    }
                                    CheckBox {
                                        checked: root.wifiAutoConnect
                                        text: checked ? "Enabled" : "Disabled"
                                        Layout.fillWidth: true
                                        onToggled: root.wifiAutoConnect = checked
                                    }
                                }
                            }

                            // Text {
                            //     text: "Actions"
                            //     color: ui.text
                            //     font.pixelSize: layoutConfig.valueFontSize
                            //     font.bold: true
                            //     Layout.fillWidth: true
                            // }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: layoutConfig.wifiButtonColumnSpacing
                                rowSpacing: layoutConfig.wifiButtonRowSpacing

                                AppButton {
                                    text: "Scan"
                                    baseColor: ui.panel2
                                    buttonHeight: layoutConfig.wifiActionButtonHeight
                                    buttonRadius: layoutConfig.buttonRadius
                                    buttonFontSize: layoutConfig.buttonFontSize
                                    busyIndicatorSize: layoutConfig.busyIndicatorSize
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: layoutConfig.wifiActionButtonMinWidth
                                    enabled: root.wifiEnabled && !root.wifiConnectBusy && !root.wifiToggleBusy && !root.wifiForgetBusy
                                    onClicked: root.wifiScanRequested()
                                }

                                AppButton {
                                    text: root.wifiToggleBusy
                                          ? (root.pendingWifiAction === "wifi_on" ? "Turning WiFi On..." : "Turning WiFi Off...")
                                          : (root.wifiEnabled ? "WiFi Off" : "WiFi On")
                                    baseColor: root.wifiEnabled ? ui.warning : ui.accent
                                    buttonHeight: layoutConfig.wifiActionButtonHeight
                                    buttonRadius: layoutConfig.buttonRadius
                                    buttonFontSize: layoutConfig.buttonFontSize
                                    busy: root.wifiToggleBusy
                                    busyIndicatorSize: layoutConfig.busyIndicatorSize
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: layoutConfig.wifiActionButtonMinWidth
                                    enabled: !root.wifiToggleBusy && !root.wifiConnectBusy && !root.wifiForgetBusy
                                    onClicked: root.wifiToggleRequested(!root.wifiEnabled)
                                }

                                AppButton {
                                    text: !root.hasSelectedWifi
                                          ? "Select Network"
                                          : (root.wifiConnectBusy
                                             ? (root.pendingWifiAction === "disconnect" ? "Disconnecting..." : "Connecting...")
                                             : (root.selectedWifiConnected ? "Disconnect" : "Connect"))
                                    baseColor: root.selectedWifiConnected ? ui.danger : ui.accent
                                    buttonHeight: layoutConfig.wifiActionButtonHeight
                                    buttonRadius: layoutConfig.buttonRadius
                                    buttonFontSize: layoutConfig.buttonFontSize
                                    busy: root.wifiConnectBusy
                                    busyIndicatorSize: layoutConfig.busyIndicatorSize
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: layoutConfig.wifiActionButtonMinWidth
                                    enabled: root.wifiEnabled && root.hasSelectedWifi
                                             && !root.wifiConnectBusy
                                             && !root.wifiToggleBusy
                                             && !root.wifiForgetBusy
                                    onClicked: {
                                        if (root.selectedWifiConnected)
                                            root.wifiDisconnectRequested(root.wifiIface)
                                        else
                                            root.wifiConnectRequested(root.wifiIface,
                                                                      root.wifiSsid,
                                                                      root.wifiPassword,
                                                                      root.wifiBssid,
                                                                      root.wifiAutoConnect)
                                    }
                                }

                                AppButton {
                                    text: root.wifiForgetBusy ? "Forgetting..." : "Forget"
                                    baseColor: root.selectedWifiSaved ? ui.warning : ui.panel2
                                    buttonHeight: layoutConfig.wifiActionButtonHeight
                                    buttonRadius: layoutConfig.buttonRadius
                                    buttonFontSize: layoutConfig.buttonFontSize
                                    busy: root.wifiForgetBusy
                                    busyIndicatorSize: layoutConfig.busyIndicatorSize
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: layoutConfig.wifiActionButtonMinWidth
                                    enabled: root.selectedWifiSaved && root.wifiSsid.length > 0
                                             && !root.wifiForgetBusy
                                             && !root.wifiConnectBusy
                                             && !root.wifiToggleBusy
                                    onClicked: root.wifiForgetRequested(root.wifiIface,
                                                                        root.wifiSsid,
                                                                        root.wifiBssid,
                                                                        root.wifiProfileName)
                                }

                                AppButton {
                                    text: root.wifiAdvancedVisible ? "Hide Advanced Settings" : "Advanced Settings"
                                    baseColor: ui.panel2
                                    buttonHeight: layoutConfig.wifiActionButtonHeight
                                    buttonRadius: layoutConfig.buttonRadius
                                    buttonFontSize: layoutConfig.buttonFontSize
                                    busy: root.wifiAdvancedBusy
                                    busyIndicatorSize: layoutConfig.busyIndicatorSize
                                    Layout.fillWidth: true
                                    Layout.columnSpan: 2
                                    Layout.minimumWidth: layoutConfig.wifiActionButtonMinWidth
                                    enabled: root.wifiAdvancedAvailable && !root.wifiAdvancedBusy
                                    onClicked: {
                                        if (root.wifiAdvancedVisible) {
                                            root.closeWifiAdvancedPanel()
                                        } else {
                                            root.wifiAdvancedOpenRequested(root.wifiIface,
                                                                          root.wifiSsid,
                                                                          root.wifiBssid,
                                                                          root.wifiProfileName)
                                        }
                                    }
                                }
                            }

                            Loader {
                                id: wifiAdvancedConfigPanel
                                active: root.wifiAdvancedVisible
                                visible: root.wifiAdvancedVisible
                                clip: true

                                Layout.fillWidth: true

                                // สำคัญ: ให้ Advanced Panel กินพื้นที่ว่างด้านล่างทั้งหมด
                                Layout.fillHeight: root.wifiAdvancedVisible
                                Layout.minimumHeight: root.wifiAdvancedVisible ? layoutConfig.wifiAdvancedPanelMinHeight : 0
                                Layout.preferredHeight: root.wifiAdvancedVisible ? layoutConfig.wifiAdvancedPanelHeight : 0

                                sourceComponent: wifiAdvancedPanelComponent
                            }

                            Text {
                                text: root.wifiMessage
                                color: ui.subText
                                font.pixelSize: layoutConfig.messageFontSize
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Item {
                                // ตอนเปิด Advanced Panel ห้ามตัวนี้แย่งพื้นที่
                                Layout.fillHeight: !root.wifiAdvancedVisible
                                Layout.preferredHeight: root.wifiAdvancedVisible ? 0 : 1
                            }
                        }

                        ColumnLayout {
                            height: 55
                            visible: root.selectedNetworkPage === "cellular"
                            enabled: root.showCellularControls
                            Layout.fillWidth: true
                            spacing: layoutConfig.cardSpacing

                            Text {
                                height: 55
                                text: "5G / Cellular"
                                color: ui.text
                                font.pixelSize: layoutConfig.cellularTitleFontSize
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: layoutConfig.cellularFormColumnSpacing
                                rowSpacing: layoutConfig.cellularFormRowSpacing

                                FieldLabel {
                                    text: "Interface"
                                    textColor: ui.subText
                                    labelFontSize: layoutConfig.labelFontSize
                                }
                                DarkField {
                                    text: root.cellularIface
                                    placeholderText: "* or wwan0"
                                    enabled: root.showCellularControls
                                    textColor: ui.text
                                    accentColor: ui.accent
                                    borderColor: ui.border
                                    fillColor: ui.field
                                    fieldHeight: layoutConfig.textFieldHeight
                                    fieldRadius: layoutConfig.textFieldRadius
                                    fieldFontSize: layoutConfig.valueFontSize
                                    fieldPadding: layoutConfig.textFieldPadding
                                    Layout.fillWidth: true
                                    onTextEdited: root.cellularIface = text
                                }

                                FieldLabel {
                                    text: "APN"
                                    textColor: ui.subText
                                    labelFontSize: layoutConfig.labelFontSize
                                }
                                DarkField {
                                    text: root.cellularApn
                                    placeholderText: "internet"
                                    enabled: root.showCellularControls
                                    textColor: ui.text
                                    accentColor: ui.accent
                                    borderColor: ui.border
                                    fillColor: ui.field
                                    fieldHeight: layoutConfig.textFieldHeight
                                    fieldRadius: layoutConfig.textFieldRadius
                                    fieldFontSize: layoutConfig.valueFontSize
                                    fieldPadding: layoutConfig.textFieldPadding
                                    Layout.fillWidth: true
                                    onTextEdited: root.cellularApn = text
                                }

                                FieldLabel {
                                    text: "Auto Connect"
                                    textColor: ui.subText
                                    labelFontSize: layoutConfig.labelFontSize
                                }
                                CheckBox {
                                    checked: root.cellularAutoConnect
                                    enabled: root.showCellularControls
                                    text: checked ? "Enabled" : "Disabled"
                                    Layout.fillWidth: true
                                    onToggled: root.cellularAutoConnect = checked
                                }
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: layoutConfig.cellularFormColumnSpacing
                                rowSpacing: layoutConfig.cellularFormRowSpacing

                                AppButton {
                                    text: "Refresh"
                                    baseColor: ui.panel2
                                    buttonHeight: layoutConfig.cellularButtonHeight
                                    buttonRadius: layoutConfig.buttonRadius
                                    buttonFontSize: layoutConfig.buttonFontSize
                                    Layout.fillWidth: true
                                    enabled: root.showCellularControls
                                    onClicked: root.cellularRefreshRequested()
                                }

                                AppButton {
                                    text: "List Modems"
                                    baseColor: ui.panel2
                                    buttonHeight: layoutConfig.cellularButtonHeight
                                    buttonRadius: layoutConfig.buttonRadius
                                    buttonFontSize: layoutConfig.buttonFontSize
                                    Layout.fillWidth: true
                                    enabled: root.showCellularControls
                                    onClicked: root.cellularListModemsRequested()
                                }

                                AppButton {
                                    text: "Connect"
                                    baseColor: ui.accent
                                    buttonHeight: layoutConfig.cellularButtonHeight
                                    buttonRadius: layoutConfig.buttonRadius
                                    buttonFontSize: layoutConfig.buttonFontSize
                                    Layout.fillWidth: true
                                    enabled: root.showCellularControls
                                    onClicked: root.cellularConnectRequested(root.cellularApn,
                                                                             root.cellularIface,
                                                                             root.cellularAutoConnect)
                                }

                                AppButton {
                                    text: "Disconnect"
                                    baseColor: ui.danger
                                    buttonHeight: layoutConfig.cellularButtonHeight
                                    buttonRadius: layoutConfig.buttonRadius
                                    buttonFontSize: layoutConfig.buttonFontSize
                                    Layout.fillWidth: true
                                    enabled: root.showCellularControls
                                    onClicked: root.cellularDisconnectRequested()
                                }
                            }

                            Text {
                                text: root.cellularMessage
                                color: ui.subText
                                font.pixelSize: layoutConfig.messageFontSize
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredWidth: layoutConfig.rightPanelPreferredWidth
                    Layout.preferredHeight: root.networkCardHeight
                    radius: layoutConfig.cardRadius
                    color: ui.panel
                    border.color: ui.border
                    border.width: layoutConfig.cardBorderWidth

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: layoutConfig.cardPadding
                        spacing: layoutConfig.cardSpacing

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: root.selectedNetworkPage === "wifi" ? "WiFi Status / Results" : "5G Status / Results"
                                color: ui.text
                                font.pixelSize: layoutConfig.sectionTitleFontSize
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            StatusBadge {
                                textValue: root.selectedNetworkPage === "wifi"
                                           ? (!root.wifiEnabled ? "Off" : (root.currentWifiConnected ? "Connected" : "Disconnected"))
                                           : (root.cellularState.connected ? "Connected" : "Disconnected")
                                badgeColor: root.selectedNetworkPage === "wifi"
                                            ? (!root.wifiEnabled ? ui.warning : root.statusColor(root.currentWifiConnected))
                                            : root.statusColor(root.cellularState.connected)
                                badgeHeight: layoutConfig.badgeHeight
                                horizontalPadding: layoutConfig.badgeHorizontalPadding
                                badgeFontSize: layoutConfig.badgeFontSize
                            }
                        }

                        ColumnLayout {
                            visible: root.selectedNetworkPage === "wifi"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: layoutConfig.cardSpacing

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: layoutConfig.wifiStatusBoxHeight
                                radius: layoutConfig.wifiStatusBoxRadius
                                color: ui.panel2
                                border.color: ui.border

                                GridLayout {
                                    anchors.fill: parent
                                    anchors.margins: layoutConfig.wifiStatusBoxPadding
                                    columns: 2
                                    rowSpacing: layoutConfig.wifiStatusGridSpacing
                                    columnSpacing: layoutConfig.wifiStatusGridSpacing

                                    FieldLabel { text: "Interface"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                                    Text { text: root.wifiIface; color: ui.text; font.pixelSize: layoutConfig.valueFontSize }

                                    FieldLabel { text: "Current SSID"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                                    Text {
                                        text: root.safeText(root.wifiState.ssid || root.wifiState.connection || root.wifiState.active_ssid, "-")
                                        color: ui.text
                                        font.pixelSize: layoutConfig.valueFontSize
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    FieldLabel { text: "IP / Gateway"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                                    Text {
                                        text: root.safeText(root.wifiState.current_ip || root.wifiState.ip, "-")
                                              + " / "
                                              + root.safeText(root.wifiState.current_gateway || root.wifiState.gateway, "-")
                                        color: ui.text
                                        font.pixelSize: layoutConfig.valueFontSize
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    FieldLabel { text: "Netmask / Signal"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                                    RowLayout {
                                        Text {
                                            text: root.safeText(root.wifiState.current_netmask || root.wifiState.netmask, "-")
                                            color: ui.text
                                            font.pixelSize: layoutConfig.valueFontSize
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        SignalBar {
                                            value: Number(root.wifiState.signal || 0)
                                            goodColor: ui.accent
                                            warningColor: ui.warning
                                            dangerColor: ui.danger
                                            barWidth: layoutConfig.signalBarWidth
                                            barHeight: layoutConfig.signalBarHeight
                                            itemWidth: layoutConfig.signalBarItemWidth
                                            itemSpacing: layoutConfig.signalBarSpacing
                                            itemRadius: layoutConfig.signalBarItemRadius
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: layoutConfig.wifiListRadius
                                color: ui.field
                                border.color: ui.border

                                ListView {
                                    id: wifiListView
                                    anchors.fill: parent
                                    anchors.margins: layoutConfig.wifiListMargin
                                    clip: true
                                    model: root.wifiList

                                    delegate: Rectangle {
                                        id: wifiDelegate

                                        width: wifiListView.width
                                        height: layoutConfig.wifiListRowHeight
                                        radius: layoutConfig.wifiListRadius
                                        color: root.isSelectedWifi(modelData)
                                               ? "#213349"
                                               : (wifiMouseArea.containsMouse ? "#1f2c3d" : "transparent")
                                        border.color: root.isConnectedWifi(modelData)
                                                      ? ui.accent
                                                      : (root.isSelectedWifi(modelData) ? ui.blue : "transparent")
                                        border.width: root.isConnectedWifi(modelData) || root.isSelectedWifi(modelData) ? 1 : 0

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: layoutConfig.textFieldPadding
                                            anchors.rightMargin: layoutConfig.textFieldPadding
                                            spacing: layoutConfig.wifiFormColumnSpacing

                                            Text {
                                                visible: root.isSavedWifi(modelData)
                                                text: "★"
                                                color: ui.savedStar
                                                font.pixelSize: layoutConfig.wifiStarFontSize
                                                font.bold: true
                                                Layout.preferredWidth: layoutConfig.wifiStarFontSize
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: layoutConfig.listTextSpacing

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: layoutConfig.wifiFormColumnSpacing

                                                    Text {
                                                        text: root.safeText(modelData.ssid, "Hidden")
                                                        color: ui.text
                                                        font.pixelSize: layoutConfig.valueFontSize
                                                        font.bold: modelData.active
                                                        elide: Text.ElideRight
                                                        Layout.fillWidth: true
                                                    }

                                                    StatusBadge {
                                                        visible: root.isConnectedWifi(modelData)
                                                        textValue: "Active"
                                                        badgeColor: ui.accent
                                                        badgeHeight: layoutConfig.badgeHeight
                                                        horizontalPadding: layoutConfig.badgeHorizontalPadding
                                                        badgeFontSize: layoutConfig.badgeFontSize
                                                    }
                                                }

                                                Text {
                                                    text: root.wifiMetaText(modelData)
                                                    color: ui.subText
                                                    font.pixelSize: layoutConfig.smallTextFontSize
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                            }

                                            SignalBar {
                                                value: Number(modelData.signal || 0)
                                                goodColor: ui.accent
                                                warningColor: ui.warning
                                                dangerColor: ui.danger
                                                barWidth: layoutConfig.signalBarWidth
                                                barHeight: layoutConfig.signalBarHeight
                                                itemWidth: layoutConfig.signalBarItemWidth
                                                itemSpacing: layoutConfig.signalBarSpacing
                                                itemRadius: layoutConfig.signalBarItemRadius
                                            }

                                            Text {
                                                text: String(modelData.signal || 0) + "%"
                                                color: ui.subText
                                                font.pixelSize: layoutConfig.smallTextFontSize
                                                Layout.preferredWidth: layoutConfig.signalPercentWidth
                                                horizontalAlignment: Text.AlignRight
                                            }
                                        }

                                        MouseArea {
                                            id: wifiMouseArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: root.selectWifiRow(modelData)
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            visible: root.selectedNetworkPage === "cellular"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: layoutConfig.cardSpacing

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: layoutConfig.cellularStatusBoxHeight
                                radius: layoutConfig.cellularStatusBoxRadius
                                color: ui.panel2
                                border.color: ui.border

                                GridLayout {
                                    anchors.fill: parent
                                    anchors.margins: layoutConfig.cellularStatusBoxPadding
                                    columns: 2
                                    rowSpacing: layoutConfig.cellularStatusGridSpacing
                                    columnSpacing: layoutConfig.cellularStatusGridSpacing

                                    FieldLabel { text: "Modem"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                                    Text {
                                        text: root.safeText(root.cellularState.modemName || root.cellularState.device || root.cellularState.interface, "-")
                                        color: ui.text
                                        font.pixelSize: layoutConfig.valueFontSize
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    FieldLabel { text: "Operator"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                                    Text {
                                        text: root.safeText(root.cellularState.operator, "-")
                                        color: ui.text
                                        font.pixelSize: layoutConfig.valueFontSize
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    FieldLabel { text: "State"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                                    Text {
                                        text: root.safeText(root.cellularState.state || root.cellularState.sim_status || root.cellularState.registration_state, "-")
                                        color: ui.text
                                        font.pixelSize: layoutConfig.valueFontSize
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    FieldLabel { text: "Access Tech"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                                    Text {
                                        text: root.safeText(root.cellularState.accessTech || root.cellularState.access_technology, "-")
                                        color: ui.text
                                        font.pixelSize: layoutConfig.valueFontSize
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    FieldLabel { text: "Signal"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                                    Text {
                                        text: root.safeText(root.cellularState.signal, "-")
                                        color: ui.text
                                        font.pixelSize: layoutConfig.valueFontSize
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: layoutConfig.modemListRadius
                                color: ui.field
                                border.color: ui.border

                                ListView {
                                    id: modemListView
                                    anchors.fill: parent
                                    anchors.margins: layoutConfig.modemListMargin
                                    clip: true
                                    model: root.modemList

                                    delegate: Rectangle {
                                        id: modemDelegate

                                        width: modemListView.width
                                        height: layoutConfig.modemListRowHeight
                                        radius: layoutConfig.modemListRadius
                                        color: "transparent"

                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: layoutConfig.textFieldPadding
                                            anchors.rightMargin: layoutConfig.textFieldPadding
                                            spacing: layoutConfig.listTextSpacing

                                            Text {
                                                text: root.safeText(modelData.name, "No modem")
                                                color: modelData.disabled ? ui.warning : ui.text
                                                font.pixelSize: layoutConfig.valueFontSize
                                                font.bold: true
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            Text {
                                                text: root.safeText(modelData.vendor, root.safeText(modelData.error, ""))
                                                color: ui.subText
                                                font.pixelSize: layoutConfig.smallTextFontSize
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

    Component {
        id: wifiAdvancedPanelComponent

        Rectangle {
            id: advancedPanel
            width: parent ? parent.width : 500
            height: parent ? parent.height : layoutConfig.wifiAdvancedPanelHeight
            radius: layoutConfig.wifiAdvancedPanelRadius
            color: ui.panel2
            border.color: ui.border
            border.width: layoutConfig.cardBorderWidth
            clip: true

            Flickable {
                id: advancedPanelFlickable
                anchors.fill: parent
                anchors.margins: layoutConfig.wifiAdvancedPanelPadding
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: advancedPanelContent.implicitHeight
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                ColumnLayout {
                    id: advancedPanelContent
                    width: advancedPanelFlickable.width
                    spacing: layoutConfig.wifiAdvancedPanelSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: layoutConfig.headerItemSpacing

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: layoutConfig.headerTextSpacing

                            Text {
                                text: "Advanced Wi-Fi Settings"
                                color: ui.text
                                font.pixelSize: layoutConfig.sectionTitleFontSize
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            // Text {
                            //     text: root.safeText(root.wifiSsid, "Selected network") + " (connected)"
                            //     color: ui.subText
                            //     font.pixelSize: layoutConfig.smallTextFontSize
                            //     elide: Text.ElideRight
                            //     Layout.fillWidth: true
                            // }
                        }

                        AppButton {
                            text: "X"
                            baseColor: ui.panel
                            buttonHeight: layoutConfig.wifiAdvancedFooterHeight
                            buttonRadius: layoutConfig.buttonRadius
                            buttonFontSize: layoutConfig.buttonFontSize
                            Layout.preferredWidth: layoutConfig.wifiAdvancedFooterHeight
                            enabled: !root.wifiAdvancedBusy
                            onClicked: root.closeWifiAdvancedPanel()
                        }
                    }

                    // Rectangle {
                    //     Layout.fillWidth: true
                    //     Layout.preferredHeight: 64
                    //     radius: layoutConfig.wifiStatusBoxRadius
                    //     color: ui.panel
                    //     border.color: ui.border

                    //     ColumnLayout {
                    //         anchors.fill: parent
                    //         anchors.margins: layoutConfig.textFieldPadding
                    //         spacing: layoutConfig.listTextSpacing

                    //         Text {
                    //             text: "IPv4 Configuration"
                    //             color: ui.text
                    //             font.pixelSize: layoutConfig.valueFontSize
                    //             font.bold: true
                    //             Layout.fillWidth: true
                    //         }

                    //         Text {
                    //             text: root.wifiCurrentSummary()
                    //             color: ui.subText
                    //             font.pixelSize: layoutConfig.smallTextFontSize
                    //             elide: Text.ElideRight
                    //             Layout.fillWidth: true
                    //         }
                    //     }
                    // }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: layoutConfig.wifiFormColumnSpacing

                    AppButton {
                        text: "Using DHCP"
                        baseColor: root.wifiAdvancedIpv4Mode === "dhcp" ? ui.accent : ui.panel2
                        buttonHeight: layoutConfig.wifiAdvancedFooterHeight
                        buttonRadius: layoutConfig.buttonRadius
                        buttonFontSize: layoutConfig.buttonFontSize
                        Layout.fillWidth: true
                        enabled: !root.wifiAdvancedBusy
                        onClicked: root.wifiAdvancedIpv4Mode = "dhcp"
                    }

                    AppButton {
                        text: "Manual"
                        baseColor: root.wifiAdvancedIpv4Mode === "manual" ? ui.accent : ui.panel2
                        buttonHeight: layoutConfig.wifiAdvancedFooterHeight
                        buttonRadius: layoutConfig.buttonRadius
                        buttonFontSize: layoutConfig.buttonFontSize
                        Layout.fillWidth: true
                        enabled: !root.wifiAdvancedBusy
                        onClicked: root.wifiAdvancedIpv4Mode = "manual"
                    }
                }

                Text {
                    text: "IPv4 Address"
                    color: ui.text
                    font.pixelSize: layoutConfig.valueFontSize
                    font.bold: true
                    Layout.fillWidth: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: layoutConfig.wifiFormColumnSpacing
                    rowSpacing: layoutConfig.wifiFormRowSpacing

                    FieldLabel { text: "IP Address"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                    DarkField {
                        text: root.wifiAdvancedIpAddress
                        placeholderText: "192.168.10.120"
                        enabled: root.wifiAdvancedIpv4Mode === "manual" && !root.wifiAdvancedBusy
                        textColor: ui.text
                        accentColor: ui.accent
                        borderColor: ui.border
                        fillColor: ui.field
                        fieldHeight: layoutConfig.wifiAdvancedFieldHeight
                        fieldRadius: layoutConfig.textFieldRadius
                        fieldFontSize: layoutConfig.valueFontSize
                        fieldPadding: layoutConfig.textFieldPadding
                        Layout.fillWidth: true
                        onTextEdited: root.wifiAdvancedIpAddress = text
                    }

                    FieldLabel { text: "Subnet Mask"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                    DarkField {
                        text: root.wifiAdvancedSubnetMask
                        placeholderText: "255.255.255.0"
                        enabled: root.wifiAdvancedIpv4Mode === "manual" && !root.wifiAdvancedBusy
                        textColor: ui.text
                        accentColor: ui.accent
                        borderColor: ui.border
                        fillColor: ui.field
                        fieldHeight: layoutConfig.wifiAdvancedFieldHeight
                        fieldRadius: layoutConfig.textFieldRadius
                        fieldFontSize: layoutConfig.valueFontSize
                        fieldPadding: layoutConfig.textFieldPadding
                        Layout.fillWidth: true
                        onTextEdited: root.wifiAdvancedSubnetMask = text
                    }

                    FieldLabel { text: "Gateway"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                    DarkField {
                        text: root.wifiAdvancedGateway
                        placeholderText: "192.168.10.1"
                        enabled: root.wifiAdvancedIpv4Mode === "manual" && !root.wifiAdvancedBusy
                        textColor: ui.text
                        accentColor: ui.accent
                        borderColor: ui.border
                        fillColor: ui.field
                        fieldHeight: layoutConfig.wifiAdvancedFieldHeight
                        fieldRadius: layoutConfig.textFieldRadius
                        fieldFontSize: layoutConfig.valueFontSize
                        fieldPadding: layoutConfig.textFieldPadding
                        actionVisible: true
                        actionText: "Clear"
                        actionButtonWidth: layoutConfig.passwordEyeButtonWidth
                        actionFontSize: layoutConfig.smallTextFontSize
                        Layout.fillWidth: true
                        onTextEdited: root.wifiAdvancedGateway = text
                        onActionClicked: root.wifiAdvancedGateway = ""
                    }
                }

                Text {
                    text: "DNS"
                    color: ui.text
                    font.pixelSize: layoutConfig.valueFontSize
                    font.bold: true
                    Layout.fillWidth: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: layoutConfig.wifiFormColumnSpacing
                    rowSpacing: layoutConfig.wifiFormRowSpacing

                    FieldLabel { text: "Automatic DNS"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                    CheckBox {
                        checked: root.wifiAdvancedDnsAutomatic
                        text: checked ? "Enabled" : "Disabled"
                        enabled: !root.wifiAdvancedBusy
                        Layout.fillWidth: true
                        onToggled: root.wifiAdvancedDnsAutomatic = checked
                    }

                    FieldLabel { text: "DNS Servers"; textColor: ui.subText; labelFontSize: layoutConfig.labelFontSize }
                    DarkField {
                        text: root.wifiAdvancedDnsServers
                        placeholderText: "8.8.8.8,1.1.1.1"
                        enabled: !root.wifiAdvancedDnsAutomatic && !root.wifiAdvancedBusy
                        textColor: ui.text
                        accentColor: ui.accent
                        borderColor: ui.border
                        fillColor: ui.field
                        fieldHeight: layoutConfig.wifiAdvancedFieldHeight
                        fieldRadius: layoutConfig.textFieldRadius
                        fieldFontSize: layoutConfig.valueFontSize
                        fieldPadding: layoutConfig.textFieldPadding
                        Layout.fillWidth: true
                        onTextEdited: root.wifiAdvancedDnsServers = text
                    }
                }

                Text {
                    text: root.wifiAdvancedMessage
                    color: root.wifiAdvancedBusy ? ui.warning : ui.subText
                    font.pixelSize: layoutConfig.messageFontSize
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Item {
                    Layout.preferredHeight: 2
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: layoutConfig.wifiFormColumnSpacing

                    AppButton {
                        text: "Cancel"
                        baseColor: ui.panel2
                        buttonHeight: layoutConfig.wifiAdvancedFooterHeight
                        buttonRadius: layoutConfig.buttonRadius
                        buttonFontSize: layoutConfig.buttonFontSize
                        Layout.fillWidth: true
                        enabled: !root.wifiAdvancedBusy
                        onClicked: root.closeWifiAdvancedPanel()
                    }

                    AppButton {
                        text: root.wifiForgetBusy ? "Forgetting..." : "Forget Connection"
                        baseColor: ui.warning
                        buttonHeight: layoutConfig.wifiAdvancedFooterHeight
                        buttonRadius: layoutConfig.buttonRadius
                        buttonFontSize: layoutConfig.buttonFontSize
                        busy: root.wifiForgetBusy
                        busyIndicatorSize: layoutConfig.busyIndicatorSize
                        Layout.fillWidth: true
                        enabled: !root.wifiAdvancedBusy && !root.wifiForgetBusy
                        onClicked: root.wifiForgetRequested(root.wifiIface,
                                                            root.wifiSsid,
                                                            root.wifiBssid,
                                                            root.wifiProfileName)
                    }

                    AppButton {
                        text: root.wifiAdvancedBusy ? "Saving..." : "Save"
                        baseColor: ui.accent
                        buttonHeight: layoutConfig.wifiAdvancedFooterHeight
                        buttonRadius: layoutConfig.buttonRadius
                        buttonFontSize: layoutConfig.buttonFontSize
                        busy: root.wifiAdvancedBusy
                        busyIndicatorSize: layoutConfig.busyIndicatorSize
                        Layout.fillWidth: true
                        enabled: !root.wifiAdvancedBusy
                        onClicked: root.wifiAdvancedSaveRequested(root.advancedSettingsPayload())
                    }
                }
            }
        }
    }
}
}
