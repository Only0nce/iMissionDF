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
    readonly property bool showCellularControls: hardwareHas5G
    readonly property int networkGridColumns: showCellularControls && root.width > 1200 ? 2 : 1
    readonly property int networkCardHeight: showCellularControls ? 620 : Math.max(620, root.height - 220)

    property bool wifiEnabled: true
    property string wifiIface: "wlP9p1s0"
    property string wifiSsid: "iMission-5G"
    property string wifiBssid: "00:11:22:33:44:55"
    property string wifiProfileName: "iMission-5G"
    property string wifiPassword: ""
    property bool wifiAutoConnect: true
    property var wifiList: [
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
    property var wifiState: ({
        "connected": true,
        "ssid": "iMission-5G",
        "connection": "iMission-5G",
        "signal": 82,
        "ip": "192.168.10.24",
        "gateway": "192.168.10.1",
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
        "operator": "Demo Carrier",
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
    signal wifiToggleRequested()
    signal wifiConnectRequested()
    signal wifiDisconnectRequested()
    signal cellularRefreshRequested()
    signal cellularConnectRequested()
    signal cellularDisconnectRequested()

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
    }

    function safeText(value, fallback) {
        if (value === undefined || value === null || value === "")
            return fallback
        return String(value)
    }

    function statusColor(connected) {
        return connected ? ui.accent : ui.danger
    }

    function wifiMetaText(row) {
        var values = [
            row.known ? "Saved" : "",
            safeText(row.band, ""),
            row.channel ? ("CH " + row.channel) : "",
            safeText(row.security, "Open")
        ]
        return values.filter(function(value) { return value !== "" }).join(" | ")
    }

    Rectangle {
        anchors.fill: parent
        color: ui.bg

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
                        text: root.showCellularControls
                              ? "WiFi and 5G settings. Hardware version: " + root.hardwareVersionName
                              : "WiFi settings. Hardware version: " + root.hardwareVersionName
                        color: ui.subText
                        font.pixelSize: 14
                    }
                }

                StatusBadge {
                    visible: root.showCellularControls
                    textValue: "5G Hardware"
                    badgeColor: ui.accent
                }

                AppButton {
                    text: "Refresh All"
                    baseColor: ui.panel2
                    Layout.preferredWidth: 140
                    onClicked: root.refreshAllRequested()
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.networkGridColumns
                columnSpacing: 18
                rowSpacing: 18

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.networkCardHeight
                    radius: 8
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
                                textValue: !root.wifiEnabled ? "Off" : (root.wifiState.connected ? "Connected" : "Disconnected")
                                badgeColor: !root.wifiEnabled ? ui.warning : root.statusColor(root.wifiState.connected)
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 92
                            radius: 8
                            color: ui.panel2
                            border.color: ui.border

                            GridLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                columns: 2
                                rowSpacing: 8
                                columnSpacing: 8

                                FieldLabel {
                                    text: "Interface"
                                    textColor: ui.subText
                                }
                                Text {
                                    text: root.wifiIface
                                    color: ui.text
                                    font.pixelSize: 15
                                }

                                FieldLabel {
                                    text: "Current SSID"
                                    textColor: ui.subText
                                }
                                RowLayout {
                                    Text {
                                        text: root.safeText(root.wifiState.ssid || root.wifiState.connection, "-")
                                        color: ui.text
                                        font.pixelSize: 15
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    SignalBar {
                                        value: Number(root.wifiState.signal || 0)
                                        goodColor: ui.accent
                                        warningColor: ui.warning
                                        dangerColor: ui.danger
                                    }
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 10

                            FieldLabel {
                                text: "WiFi Interface"
                                textColor: ui.subText
                            }
                            DarkField {
                                text: root.wifiIface
                                placeholderText: "wlP9p1s0"
                                textColor: ui.text
                                accentColor: ui.accent
                                borderColor: ui.border
                                fillColor: ui.field
                                Layout.fillWidth: true
                                onTextEdited: root.wifiIface = text
                            }

                            FieldLabel {
                                text: "SSID"
                                textColor: ui.subText
                            }
                            DarkField {
                                text: root.wifiSsid
                                placeholderText: "Select from list or type SSID"
                                textColor: ui.text
                                accentColor: ui.accent
                                borderColor: ui.border
                                fillColor: ui.field
                                Layout.fillWidth: true
                                onTextEdited: root.wifiSsid = text
                            }

                            FieldLabel {
                                text: "Password"
                                textColor: ui.subText
                            }
                            DarkField {
                                text: root.wifiPassword
                                placeholderText: "WiFi password"
                                echoMode: TextInput.Password
                                textColor: ui.text
                                accentColor: ui.accent
                                borderColor: ui.border
                                fillColor: ui.field
                                Layout.fillWidth: true
                                onTextEdited: root.wifiPassword = text
                            }

                            FieldLabel {
                                text: "Auto Connect"
                                textColor: ui.subText
                            }
                            CheckBox {
                                checked: root.wifiAutoConnect
                                text: checked ? "Enabled" : "Disabled"
                                Layout.fillWidth: true
                                onToggled: root.wifiAutoConnect = checked
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            AppButton {
                                text: "Scan"
                                baseColor: ui.panel2
                                Layout.fillWidth: true
                                enabled: root.wifiEnabled
                                onClicked: root.wifiScanRequested()
                            }

                            AppButton {
                                text: root.wifiEnabled ? "WiFi Off" : "WiFi On"
                                baseColor: root.wifiEnabled ? ui.warning : ui.accent
                                Layout.fillWidth: true
                                onClicked: root.wifiToggleRequested()
                            }

                            AppButton {
                                text: "Connect"
                                baseColor: ui.accent
                                Layout.fillWidth: true
                                enabled: root.wifiEnabled && root.wifiSsid.length > 0
                                onClicked: root.wifiConnectRequested()
                            }

                            AppButton {
                                text: "Disconnect"
                                baseColor: ui.danger
                                Layout.fillWidth: true
                                enabled: root.wifiEnabled
                                onClicked: root.wifiDisconnectRequested()
                            }
                        }

                        Text {
                            text: root.wifiMessage
                            color: ui.subText
                            font.pixelSize: 13
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 8
                            color: ui.field
                            border.color: ui.border

                            ListView {
                                id: wifiListView
                                anchors.fill: parent
                                anchors.margins: 8
                                clip: true
                                model: root.wifiList

                                delegate: Rectangle {
                                    id: wifiDelegate

                                    width: wifiListView.width
                                    height: 56
                                    radius: 8
                                    color: wifiMouseArea.containsMouse ? "#1f2c3d" : "transparent"
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
                                                text: root.safeText(modelData.ssid, "Hidden")
                                                color: ui.text
                                                font.pixelSize: 15
                                                font.bold: modelData.active
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            Text {
                                                text: root.wifiMetaText(modelData)
                                                color: ui.subText
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }

                                        SignalBar {
                                            value: Number(modelData.signal || 0)
                                            goodColor: ui.accent
                                            warningColor: ui.warning
                                            dangerColor: ui.danger
                                        }

                                        Text {
                                            text: String(modelData.signal || 0) + "%"
                                            color: ui.subText
                                            font.pixelSize: 12
                                            Layout.preferredWidth: 45
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }

                                    MouseArea {
                                        id: wifiMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {
                                            root.wifiSsid = modelData.ssid || ""
                                            root.wifiBssid = modelData.bssid || ""
                                            root.wifiProfileName = modelData.profile_name || ""
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: root.showCellularControls
                    enabled: root.showCellularControls
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.showCellularControls ? 620 : 0
                    radius: 8
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
                                text: "5G / Cellular"
                                color: ui.text
                                font.pixelSize: 23
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            StatusBadge {
                                textValue: root.cellularState.connected ? "Connected" : "Disconnected"
                                badgeColor: root.statusColor(root.cellularState.connected)
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 150
                            radius: 8
                            color: ui.panel2
                            border.color: ui.border

                            GridLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                columns: 2
                                rowSpacing: 8
                                columnSpacing: 8

                                FieldLabel {
                                    text: "Modem"
                                    textColor: ui.subText
                                }
                                Text {
                                    text: root.safeText(root.cellularState.modemName || root.cellularState.device || root.cellularState.interface, "-")
                                    color: ui.text
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                FieldLabel {
                                    text: "Operator"
                                    textColor: ui.subText
                                }
                                Text {
                                    text: root.safeText(root.cellularState.operator, "-")
                                    color: ui.text
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                FieldLabel {
                                    text: "State"
                                    textColor: ui.subText
                                }
                                Text {
                                    text: root.safeText(root.cellularState.state || root.cellularState.sim_status || root.cellularState.registration_state, "-")
                                    color: ui.text
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                FieldLabel {
                                    text: "Access Tech"
                                    textColor: ui.subText
                                }
                                Text {
                                    text: root.safeText(root.cellularState.accessTech || root.cellularState.access_technology, "-")
                                    color: ui.text
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                FieldLabel {
                                    text: "Signal"
                                    textColor: ui.subText
                                }
                                Text {
                                    text: root.safeText(root.cellularState.signal, "-")
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

                            FieldLabel {
                                text: "Interface"
                                textColor: ui.subText
                            }
                            DarkField {
                                text: root.cellularIface
                                placeholderText: "* or wwan0"
                                enabled: root.showCellularControls
                                textColor: ui.text
                                accentColor: ui.accent
                                borderColor: ui.border
                                fillColor: ui.field
                                Layout.fillWidth: true
                                onTextEdited: root.cellularIface = text
                            }

                            FieldLabel {
                                text: "APN"
                                textColor: ui.subText
                            }
                            DarkField {
                                text: root.cellularApn
                                placeholderText: "internet"
                                enabled: root.showCellularControls
                                textColor: ui.text
                                accentColor: ui.accent
                                borderColor: ui.border
                                fillColor: ui.field
                                Layout.fillWidth: true
                                onTextEdited: root.cellularApn = text
                            }

                            FieldLabel {
                                text: "Auto Connect"
                                textColor: ui.subText
                            }
                            CheckBox {
                                checked: root.cellularAutoConnect
                                enabled: root.showCellularControls
                                text: checked ? "Enabled" : "Disabled"
                                Layout.fillWidth: true
                                onToggled: root.cellularAutoConnect = checked
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            AppButton {
                                text: "Refresh"
                                baseColor: ui.panel2
                                Layout.fillWidth: true
                                enabled: root.showCellularControls
                                onClicked: root.cellularRefreshRequested()
                            }

                            AppButton {
                                text: "Connect"
                                baseColor: ui.accent
                                Layout.fillWidth: true
                                enabled: root.showCellularControls
                                onClicked: root.cellularConnectRequested()
                            }

                            AppButton {
                                text: "Disconnect"
                                baseColor: ui.danger
                                Layout.fillWidth: true
                                enabled: root.showCellularControls
                                onClicked: root.cellularDisconnectRequested()
                            }
                        }

                        Text {
                            text: root.cellularMessage
                            color: ui.subText
                            font.pixelSize: 13
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 8
                            color: ui.field
                            border.color: ui.border

                            ListView {
                                id: modemListView
                                anchors.fill: parent
                                anchors.margins: 8
                                clip: true
                                model: root.modemList

                                delegate: Rectangle {
                                    id: modemDelegate

                                    width: modemListView.width
                                    height: 62
                                    radius: 8
                                    color: "transparent"

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 2

                                        Text {
                                            text: root.safeText(modelData.name, "No modem")
                                            color: modelData.disabled ? ui.warning : ui.text
                                            font.pixelSize: 15
                                            font.bold: true
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: root.safeText(modelData.vendor, root.safeText(modelData.error, ""))
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
