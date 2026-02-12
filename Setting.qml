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

    function cidrToNetmask(cidr) {
        const bits = parseInt(cidr);
        const mask = (0xFFFFFFFF << (32 - bits)) >>> 0;
        return [24, 16, 8, 0].map(s => (mask >>> s) & 255).join(".");
    }

    function refreshDhcpInfo() {
        const info = NetworkController.queryDhcpInfo(interfaceName);
        if (info) {
            console.log("NetworkController.queryDhcpInfo",info,info.ip)
            ipAddress = info.ip || "";
            netmask = info.netmask || "";
            gateway = info.gateway || "";
            primaryDns = info.dns || "";
            secondaryDns = info.dns2 || "";
        }
    }
    function getRecorderSetting() {
        const recorderData = ReceiverRecorderConfigManager.loadConfig();
        console.log("recorder config:", recorderData);

        recorderConf.alsa_dev = recorderData["alsa_dev"] || "";
        recorderConf.client_as_ip = recorderData["client_as_ip"] || "";
        recorderConf.client_as_freq = recorderData["client_as_freq"] || "";
        recorderConf.rtsp_server_ip = recorderData["rtsp_server_ip"] || "";
        recorderConf.rtsp_server_uri = recorderData["rtsp_server_uri"] || "";
        recorderConf.rtsp_server_port = recorderData["rtsp_server_port"] || "";
    }
    Component.onCompleted: {

        const config = NetworkController.loadConfig(interfaceName);
        if (config) {
            useDhcp = config.mode === "dhcp";
            let ipRaw = config.ip;
            if (ipRaw && ipRaw.includes("/")) {
                ipAddress = ipRaw.split("/")[0];
                netmask = cidrToNetmask(ipRaw.split("/")[1]);
            } else {
                ipAddress = ipRaw;
                netmask = config.netmask || "";
            }
            gateway = config.gateway;
            primaryDns = config.dns;
            secondaryDns = config.dns2;
        }

        if (useDhcp)
            refreshDhcpInfo();

        width = screenrotation==270 ? 1280 : 1920
        height = screenrotation==270 ? 400 : 1020

        getRecorderSetting();
    }
    // ข้อความ preview ที่แสดงด้านบนแป้นพิมพ์

    RowLayout {
        x: 0
        anchors.top: parent.top
        anchors.topMargin: 5
        Rectangle {
            id: rectangle
            color: "#4d000000"
            radius: 5
            border.color: "#e6000000"
            border.width: 1
            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                anchors.topMargin: 5
                anchors.bottomMargin: 5
                spacing: 5

                Label {
                    id: label1
                    text: qsTr("Network Setting")
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.preferredHeight: 40
                    Layout.preferredWidth: 280
                    spacing: 10
                    Text { text: "DHCP:"; Layout.preferredWidth: 117; color: "white" }
                    CheckBox {
                        id: dhcpCheckbox
                        text: "Use DHCP"
                        Layout.preferredWidth: 150
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        onCheckedChanged: {
                            useDhcp = checked;
                            if (useDhcp) refreshDhcpInfo();
                        }
                        checked: useDhcp
                        Layout.fillHeight: true
                    }
                }

                RowLayout {
                    Layout.preferredHeight: 40
                    Layout.preferredWidth: 280
                    spacing: 10
                    Text { text: "IP Address:"; Layout.preferredWidth: 117; color: "white" }
                    TextField {
                        id: ipField
                        text: ipAddress
                        leftPadding: 10
                        Layout.preferredWidth: 150
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        placeholderText: "IP Address"
                        onTextChanged: { ipAddress = text }
                        enabled: !useDhcp
                        Layout.fillHeight: true
                    }
                }

                RowLayout {
                    Layout.preferredHeight: 40
                    Layout.preferredWidth: 280
                    spacing: 10
                    Text { text: "Netmask:"; Layout.preferredWidth: 117; color: "white" }
                    TextField {
                        id: netmaskField
                        text: netmask
                        leftPadding: 10
                        Layout.preferredWidth: 150
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        placeholderText: "Netmask"
                        onTextChanged: { netmask = text }
                        enabled: !useDhcp
                        Layout.fillHeight: true
                    }
                }

                RowLayout {
                    Layout.preferredHeight: 40
                    Layout.preferredWidth: 280
                    spacing: 10
                    Text { text: "Gateway:"; Layout.preferredWidth: 117; color: "white" }
                    TextField {
                        id: gatewayField
                        text: gateway
                        leftPadding: 10
                        Layout.preferredWidth: 150
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        placeholderText: "Gateway"
                        onTextChanged: { gateway = text }
                        enabled: !useDhcp
                        Layout.fillHeight: true
                    }
                }

                RowLayout {
                    Layout.preferredHeight: 40
                    Layout.preferredWidth: 280
                    spacing: 10
                    Text { text: "Pri. DNS:"; Layout.preferredWidth: 117; color: "white" }
                    TextField {
                        id: dns1Field
                        text: primaryDns
                        leftPadding: 10
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        Layout.preferredWidth: 150
                        placeholderText: "Primary DNS"
                        onTextChanged: { primaryDns = text }
                        enabled: !useDhcp
                        Layout.fillHeight: true
                    }
                }

                RowLayout {
                    Layout.preferredHeight: 40
                    Layout.preferredWidth: 280
                    spacing: 10
                    Text { text: "Sec. DNS:"; Layout.preferredWidth: 117; color: "white" }
                    TextField {
                        id: dns2Field
                        text: secondaryDns
                        leftPadding: 10
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        Layout.preferredWidth: 150
                        placeholderText: "Secondary DNS"
                        onTextChanged: { secondaryDns = text }
                        enabled: !useDhcp
                        Layout.fillHeight: true
                    }
                }

                Button {
                    text: "Apply Network Setting"
                    Layout.preferredHeight: 50
                    onClicked: {
                        const cidr = netmask.split(".").reduce((acc, octet) => acc + ((Array(9).join("0") + parseInt(octet).toString(2)).slice(-8)), "").split("1").length - 1;
                        const ipWithCidr = ipAddress + "/" + cidr;
                        NetworkController.applyNetworkConfig(
                                    interfaceName,
                                    useDhcp ? "dhcp" : "static",
                                    ipWithCidr,
                                    gateway,
                                    primaryDns + (secondaryDns ? "," + secondaryDns : "")
                                    );
                        if (useDhcp) refreshDhcpInfo();
                    }
                    Layout.fillWidth: true
                }


            }
            Layout.preferredWidth: 300
            Layout.preferredHeight: 390
        }

        Rectangle {
            id: recorderConf
            width: parent.width
            height: 200
            color: "#4d000000"
            radius: 5
            border.color: "#e6000000"
            border.width: 1
            anchors.margins: 8
            Layout.preferredWidth: 300
            Layout.preferredHeight: 390

            property string alsa_dev: ""
            property string client_as_ip: ""
            property real client_as_freq: 0
            property string rtsp_server_ip: ""
            property string rtsp_server_uri: ""
            property int rtsp_server_port: 554

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                anchors.topMargin: 5
                anchors.bottomMargin: 5
                spacing: 5

                Label {
                    id: label
                    text: qsTr("Recorder Settings")
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }

                RowLayout {
                    id: row
                    Layout.preferredHeight: 40
                    Layout.preferredWidth: 280
                    spacing: 10
                    Text { text: "ALSA Device:"; Layout.preferredWidth: 117; color: "white" }
                    TextField {
                        text: recorderConf.alsa_dev
                        leftPadding: 10
                        Layout.preferredWidth: 150
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        placeholderText: "recin1"
                        onTextChanged: recorderConf.alsa_dev = text
                    }
                }

                RowLayout {
                    Layout.preferredHeight: 35
                    Layout.preferredWidth: 280
                    spacing: 10
                    Text { text: "Client IP:"; Layout.preferredWidth: 117; color: "white" }
                    TextField {
                        text: recorderConf.client_as_ip
                        leftPadding: 10
                        Layout.preferredWidth: 150
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        placeholderText: "10.0.25.1"
                        onTextChanged: recorderConf.client_as_ip = text
                    }
                }

                RowLayout {
                    Layout.preferredHeight: 35
                    Layout.preferredWidth: 280
                    spacing: 10
                    Text { text: "Frequency:"; Layout.preferredWidth: 117; color: "white" }
                    TextField {
                        text: recorderConf.client_as_freq.toString()
                        leftPadding: 10
                        Layout.preferredWidth: 150
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        placeholderText: "985.500"
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        onTextChanged: recorderConf.client_as_freq = parseFloat(text)
                    }
                }

                RowLayout {
                    Layout.preferredWidth: 280
                    Layout.preferredHeight: 35
                    spacing: 10
                    Text { text: "RTSP Server:"; Layout.preferredWidth: 117; color: "white" }
                    TextField {
                        text: recorderConf.rtsp_server_ip
                        leftPadding: 10
                        Layout.preferredWidth: 150
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        placeholderText: "192.168.10.31"
                        onTextChanged: recorderConf.rtsp_server_ip = text
                    }
                }

                RowLayout {
                    Layout.preferredWidth: 280
                    Layout.preferredHeight: 35
                    spacing: 10
                    Text { text: "RTSP URI:"; Layout.preferredWidth: 117; color: "white" }
                    TextField {
                        text: recorderConf.rtsp_server_uri
                        leftPadding: 10
                        Layout.preferredWidth: 150
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        placeholderText: "igate1"
                        onTextChanged: recorderConf.rtsp_server_uri = text
                    }
                }

                RowLayout {
                    Layout.preferredHeight: 35
                    Layout.preferredWidth: 280
                    spacing: 10
                    Text { text: "RTSP Port:"; Layout.preferredWidth: 117; color: "white" }
                    TextField {
                        text: recorderConf.rtsp_server_port.toString()
                        leftPadding: 10
                        Layout.preferredWidth: 150
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        placeholderText: "554"
                        inputMethodHints: Qt.ImhDigitsOnly
                        onTextChanged: recorderConf.rtsp_server_port = parseInt(text)
                    }
                }

                Button {
                    text: "Apply Recorder Settings"
                    Layout.preferredHeight: 50
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
                    }
                }


            }


        }


    }

}

