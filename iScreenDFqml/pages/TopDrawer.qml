// pages/TopDrawer.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "pages"
Drawer {
    id: topDrawer
    edge: Qt.TopEdge
    modal: true
    interactive: true
    dragMargin: 5
    width: parent ? parent.width : 1280
    height: 600

    // ----- Public API -----
    property var kraken: null   // bind Krakenmapval จากภายนอก

    background: Rectangle { color: "#1e1e1e" }

    onVisibleChanged: {
        if (!kraken) return
        if (visible) {
            ipField.originalValue      = kraken.ipAddress
            subnetField.originalValue  = kraken.subnetMask
            gatewayField.originalValue = kraken.gateway
            dns1Field.originalValue    = kraken.dns1
            dns2Field.originalValue    = kraken.dns2

            ipField.text      = ipField.originalValue
            subnetField.text  = subnetField.originalValue
            gatewayField.text = gatewayField.originalValue
            dns1Field.text    = dns1Field.originalValue
            dns2Field.text    = dns2Field.originalValue

            serverKrakenField.originalValue   = kraken.serverKraken
            iScreenIpField.originalValue      = kraken.iScreenIp
            subnetiScreenField.originalValue  = kraken.serverKrakensubnet
            gatewayiScreenField.originalValue = kraken.serverKrakengateway

            serverKrakenField.text   = serverKrakenField.originalValue
            iScreenIpField.text      = iScreenIpField.originalValue
            subnetiScreenField.text  = subnetiScreenField.originalValue
            gatewayiScreenField.text = gatewayiScreenField.originalValue

            offsetInputField.originalValue   = kraken.offset.toString()
            offsetInputField.text            = offsetInputField.originalValue
            compassOffsetField.originalValue = kraken.compassOffset.toFixed(3).toString()
            compassOffsetField.text          = compassOffsetField.originalValue
        } else {
            ipField.text      = ipField.originalValue
            subnetField.text  = subnetField.originalValue
            gatewayField.text = gatewayField.originalValue
            dns1Field.text    = dns1Field.originalValue

            serverKrakenField.text   = serverKrakenField.originalValue
            iScreenIpField.text      = iScreenIpField.originalValue
            subnetiScreenField.text  = subnetiScreenField.originalValue
            gatewayiScreenField.text = gatewayiScreenField.originalValue

            offsetInputField.text    = offsetInputField.originalValue
            compassOffsetField.text  = compassOffsetField.originalValue
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Label {
            text: "Network Configuration"
            font.pixelSize: 22
            font.bold: true
            color: "#ffffff"
            Layout.alignment: Qt.AlignHCenter
        }

        RowLayout {
            spacing: 10
            Label { text: "DHCP"; color: "#cccccc"; font.pixelSize: 16; Layout.preferredWidth: 100 }
            ComboBox {
                id: ipModeCombo
                Layout.preferredWidth: 200
                model: ["Automatic", "Static"]
                Component.onCompleted: {
                    if (!kraken) return
                    ipModeCombo.currentIndex = (kraken.useDHCP === "off") ? 1 : 0
                }
            }
        }

        // IP / Subnet
        RowLayout {
            spacing: 12; Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "IP Address"; color: "#cccccc"; font.pixelSize: 14 }
                TextField {
                    id: ipField
                    placeholderText: "192.168.1.100"
                    property string originalValue: ""
                    text: originalValue
                    onTextChanged: { if (kraken) kraken.ipAddress = text }
                    enabled: ipModeCombo.currentIndex === 1
                    Layout.fillWidth: true
                    background: Rectangle { color: activeFocus ? "#3a7bd5" : "#2c2c2c"; radius: 6
                        Behavior on color { ColorAnimation { duration: 250 } } }
                    color: "white"; font.pixelSize: 16; placeholderTextColor: "#888888"
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "Subnet Mask"; color: "#cccccc"; font.pixelSize: 14 }
                TextField {
                    id: subnetField
                    placeholderText: "255.255.255.0"
                    property string originalValue: ""
                    text: originalValue
                    onTextChanged: { if (kraken) kraken.subnetMask = text }
                    enabled: ipModeCombo.currentIndex === 1
                    Layout.fillWidth: true
                    background: Rectangle { color: activeFocus ? "#3a7bd5" : "#2c2c2c"; radius: 6
                        Behavior on color { ColorAnimation { duration: 250 } } }
                    color: "white"; font.pixelSize: 16; placeholderTextColor: "#888888"
                }
            }
        }

        // Gateway / DNS1
        RowLayout {
            spacing: 12; Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "Gateway"; color: "#cccccc"; font.pixelSize: 14 }
                TextField {
                    id: gatewayField
                    placeholderText: "Gateway"
                    property string originalValue: ""
                    text: originalValue
                    onTextChanged: { if (kraken) kraken.gateway = text }
                    enabled: ipModeCombo.currentIndex === 1
                    Layout.fillWidth: true
                    background: Rectangle { color: activeFocus ? "#3a7bd5" : "#2c2c2c"; radius: 6
                        Behavior on color { ColorAnimation { duration: 250 } } }
                    color: "white"; font.pixelSize: 16; placeholderTextColor: "#888888"
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "Primary DNS"; color: "#cccccc"; font.pixelSize: 14 }
                TextField {
                    id: dns1Field
                    placeholderText: "Primary DNS"
                    property string originalValue: ""
                    text: originalValue
                    onTextChanged: { if (kraken) kraken.dns1 = text }
                    enabled: ipModeCombo.currentIndex === 1
                    Layout.fillWidth: true
                    background: Rectangle { color: activeFocus ? "#3a7bd5" : "#2c2c2c"; radius: 6
                        Behavior on color { ColorAnimation { duration: 250 } } }
                    color: "white"; font.pixelSize: 16; placeholderTextColor: "#888888"
                }
            }
        }

        // DNS2
        RowLayout {
            spacing: 12; Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "Secondary DNS"; color: "#cccccc"; font.pixelSize: 14 }
                TextField {
                    id: dns2Field
                    placeholderText: "Secondary DNS"
                    property string originalValue: ""
                    text: originalValue
                    onTextChanged: { if (kraken) kraken.dns2 = text }
                    enabled: ipModeCombo.currentIndex === 1
                    Layout.fillWidth: true
                    background: Rectangle { color: activeFocus ? "#3a7bd5" : "#2c2c2c"; radius: 6
                        Behavior on color { ColorAnimation { duration: 250 } } }
                    color: "white"; font.pixelSize: 16; placeholderTextColor: "#888888"
                }
            }
        }

        // Apply / Restart network
        RowLayout {
            spacing: 12; Layout.fillWidth: true

            Button {
                id: applyBtn
                text: "Apply Settings"
                Layout.fillWidth: true
                font.pixelSize: 16
                background: Rectangle { radius: 6; color: applyBtn.pressed ? Qt.darker("#00c896", 1.4) : "#00c896"
                    Behavior on color { ColorAnimation { duration: 150 } } }
                contentItem: Text { text: parent.text; anchors.centerIn: parent; color: "white"; font.bold: true }
                onClicked: {
                    if (!kraken) return
                    let dhcpValue = (ipModeCombo.currentIndex === 1) ? "off" : "on"
                    kraken.updateNetworkfromDisplay(dhcpValue, ipField.text, subnetField.text, gatewayField.text, dns1Field.text, dns2Field.text)
                    ipField.originalValue      = ipField.text
                    subnetField.originalValue  = subnetField.text
                    gatewayField.originalValue = gatewayField.text
                    dns1Field.originalValue    = dns1Field.text
                    dns2Field.originalValue    = dns2Field.text
                }
            }

            Button {
                id: restartBtn
                text: "Restart Network"
                Layout.fillWidth: true
                font.pixelSize: 16
                background: Rectangle { radius: 6; color: restartBtn.pressed ? Qt.darker("#007acc", 1.4) : "#007acc"
                    Behavior on color { ColorAnimation { duration: 150 } } }
                contentItem: Text { text: parent.text; anchors.centerIn: parent; color: "white"; font.bold: true }
                onClicked: { console.log("Restart Network clicked") }
            }
        }

        // ==== Server Kraken & iScreen ====
        ColumnLayout {
            Layout.fillWidth: true
            Label { text: "Setup Server Kraken"; color: "#cccccc"; font.pixelSize: 14 }

            RowLayout {
                spacing: 12; Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter

                TextField {
                    id: serverKrakenField
                    property string originalValue: ""
                    placeholderText: "Kraken Server IP"
                    text: kraken ? kraken.serverKraken : ""
                    onTextChanged: { if (kraken) kraken.serverKraken = text }
                    Layout.fillWidth: true; Layout.preferredHeight: 40
                    background: Rectangle { color: activeFocus ? "#3a7bd5" : "#2c2c2c"; radius: 6
                        Behavior on color { ColorAnimation { duration: 250 } } }
                    color: "white"; font.pixelSize: 16; placeholderTextColor: "#888888"
                }

                TextField {
                    id: iScreenIpField
                    property string originalValue: ""
                    placeholderText: "iScreen IP"
                    text: kraken ? kraken.iScreenIp : ""
                    onTextChanged: { if (kraken) kraken.iScreenIp = text }
                    Layout.fillWidth: true; Layout.preferredHeight: 40
                    background: Rectangle { color: activeFocus ? "#3a7bd5" : "#2c2c2c"; radius: 6
                        Behavior on color { ColorAnimation { duration: 250 } } }
                    color: "white"; font.pixelSize: 16; placeholderTextColor: "#888888"
                }

                TextField {
                    id: subnetiScreenField
                    property string originalValue: ""
                    placeholderText: "Subnet"
                    text: kraken ? kraken.serverKrakensubnet : ""
                    onTextChanged: { if (kraken) kraken.serverKrakensubnet = text }
                    Layout.fillWidth: true; Layout.preferredHeight: 40
                    background: Rectangle { color: activeFocus ? "#3a7bd5" : "#2c2c2c"; radius: 6
                        Behavior on color { ColorAnimation { duration: 250 } } }
                    color: "white"; font.pixelSize: 16; placeholderTextColor: "#888888"
                }

                TextField {
                    id: gatewayiScreenField
                    property string originalValue: ""
                    placeholderText: "Gateway"
                    text: kraken ? kraken.serverKrakengateway : ""
                    onTextChanged: { if (kraken) kraken.serverKrakengateway = text }
                    Layout.fillWidth: true; Layout.preferredHeight: 40
                    background: Rectangle { color: activeFocus ? "#3a7bd5" : "#2c2c2c"; radius: 6
                        Behavior on color { ColorAnimation { duration: 250 } } }
                    color: "white"; font.pixelSize: 16; placeholderTextColor: "#888888"
                }

                Button {
                    id: applyKrakenBtn
                    text: "Apply"
                    Layout.preferredWidth: 160; Layout.preferredHeight: 40
                    font.pixelSize: 16
                    background: Rectangle { radius: 6; color: applyKrakenBtn.pressed ? Qt.darker("#f39c12", 1.4) : "#f39c12"
                        Behavior on color { ColorAnimation { duration: 120 } } }
                    contentItem: Text { text: parent.text; anchors.fill: parent; color: "white"; font.pixelSize: parent.font.pixelSize; font.bold: true }
                    onClicked: {
                        console.log("Apply", kraken ? kraken.serverKraken : "", kraken ? kraken.iScreenIp : "",
                                            kraken ? kraken.serverKrakensubnet : "", kraken ? kraken.serverKrakengateway : "")
                        serverKrakenField.originalValue   = serverKrakenField.text
                        iScreenIpField.originalValue      = iScreenIpField.text
                        subnetiScreenField.originalValue  = subnetiScreenField.text
                        gatewayiScreenField.originalValue = gatewayiScreenField.text
                    }
                }

                Button {
                    id: restartKrakenBtn
                    text: "Restart Kraken"
                    Layout.preferredWidth: 170; Layout.preferredHeight: 40
                    font.pixelSize: 16
                    background: Rectangle { radius: 6; color: restartKrakenBtn.pressed ? Qt.darker("#2980b9", 1.4) : "#2980b9"
                        Behavior on color { ColorAnimation { duration: 120 } } }
                    contentItem: Text { text: parent.text; anchors.fill: parent; color: "white"; font.pixelSize: parent.font.pixelSize; font.bold: true }
                    onClicked: { if (kraken) kraken.RestartKraken("true") }
                }

                Button {
                    id: reconnectBtn
                    text: "Reconnect"
                    Layout.preferredWidth: 160; Layout.preferredHeight: 40
                    font.pixelSize: 16
                    background: Rectangle { radius: 6; color: reconnectBtn.pressed ? Qt.darker("#27ae60", 1.4) : "#27ae60"
                        Behavior on color { ColorAnimation { duration: 120 } } }
                    contentItem: Text { text: parent.text; anchors.fill: parent; color: "white"; font.pixelSize: parent.font.pixelSize; font.bold: true }
                    onClicked: { if (kraken) kraken.connectToserverKraken(kraken.serverKraken) }
                }
            }
        }

        // Spacer
        Item { Layout.fillWidth: true; Layout.preferredHeight: 15 }

        // ==== Offset / Compass ====
        RowLayout {
            spacing: 15; Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter

            Text { text: "Offset Value:"; color: "#cccccc"; font.pixelSize: 16; Layout.alignment: Qt.AlignVCenter }

            TextField {
                id: offsetInputField
                property string originalValue: ""
                text: kraken ? kraken.offset.toString() : ""
                Layout.fillWidth: true
                font.pixelSize: 16; color: "white"
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                background: Rectangle { color: activeFocus ? "#3a7bd5" : "#2c2c2c"; radius: 6
                    Behavior on color { ColorAnimation { duration: 250 } } }
                placeholderText: "Enter Offset"; placeholderTextColor: "#888888"
            }

            Button {
                id: setOffsetButton
                text: "Set Offset"
                Layout.preferredWidth: 140; Layout.preferredHeight: 40
                font.pixelSize: 16
                background: Rectangle { radius: 6; color: setOffsetButton.pressed ? Qt.darker("#8e44ad", 1.4) : "#8e44ad"
                    Behavior on color { ColorAnimation { duration: 120 } } }
                contentItem: Text { text: parent.text; anchors.fill: parent; color: "white"; font.pixelSize: parent.font.pixelSize; font.bold: true }
                onClicked: {
                    if (!kraken) return
                    let offsetValue = parseFloat(offsetInputField.text)
                    if (!isNaN(offsetValue)) {
                        console.log("Set Offset to:", offsetValue)
                        kraken.setOffset(offsetValue)
                        offsetInputField.originalValue = offsetInputField.text
                    } else {
                        console.warn("Invalid offset value")
                    }
                }
            }
        }

        RowLayout {
            spacing: 20; Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter; anchors.margins: 8

            ColumnLayout {
                spacing: 4; Layout.alignment: Qt.AlignLeft; Layout.preferredWidth: 130
                Text { text: "Compass Offset :"; color: "#cccccc"; font.pixelSize: 16; horizontalAlignment: Text.AlignLeft }
            }

            TextField {
                id: compassOffsetField
                property string originalValue: ""
                text: kraken ? kraken.compassOffset.toFixed(3).toString() : ""
                Layout.fillWidth: true
                font.pixelSize: 16; color: "white"
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                background: Rectangle { color: activeFocus ? "#3a7bd5" : "#2c2c2c"; radius: 6
                    Behavior on color { ColorAnimation { duration: 250 } } }
                placeholderText: "Enter Offset"; placeholderTextColor: "#888888"
            }

            Switch {
                id: compassOffsetSwitch
                checked: kraken ? kraken.spectrumPeakHold : false
                Layout.alignment: Qt.AlignVCenter
            }

            Button {
                id: compassOffsetButton
                text: "Set Compass Offset"
                Layout.preferredWidth: 200; Layout.preferredHeight: 40
                font.pixelSize: 16
                background: Rectangle { radius: 6; color: compassOffsetButton.pressed ? Qt.darker("#8e44ad", 1.4) : "#8e44ad"
                    Behavior on color { ColorAnimation { duration: 120 } } }
                contentItem: Text { text: parent.text; anchors.fill: parent; color: "white"; font.pixelSize: parent.font.pixelSize; font.bold: true }
                onClicked: {
                    if (!kraken) return
                    let offsetValue = parseFloat(compassOffsetField.text)
                    if (!isNaN(offsetValue)) {
                        console.log("Set Offset to:", offsetValue, "Enabled:", compassOffsetSwitch.checked)
                        kraken.setCompassConfig(offsetValue, compassOffsetSwitch.checked)
                        compassOffsetField.originalValue = compassOffsetField.text
                    } else {
                        console.warn("Invalid offset value")
                    }
                }
            }
        }
    }
}
