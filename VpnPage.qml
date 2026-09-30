import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls.Material 2.12

import "ui"
import App 1.0

Item {
    id: root

    Theme { id: hmiTheme; darkMode: root.Material.theme === Material.Dark }

    property bool adminMode: false
    property var vpnProfiles: []
    property var activeProfiles: []
    property bool vpnEnabled: true
    property string runtimeState: "disconnected"
    property string activeName: ""
    property string activeUuid: ""
    property string activeType: ""
    property string activeDevice: ""
    property string activeIpv4: ""
    property string publicIp: "—"
    property bool publicIpLoading: false
    property string publicIpMessage: ""
    property string selectedUuid: ""
    property string selectedName: ""
    property string selectedType: ""
    property string message: "Press Refresh to load VPN status"
    property bool busy: false
    property bool statusRequestInFlight: false
    property bool enableChangeInFlight: false
    property string pendingUuid: ""
    property string pendingName: ""
    property bool pendingTargetActive: false

    signal requestToast(string text)

    QtObject {
        id: c
        property color panel: hmiTheme.panel
        property color card: hmiTheme.cardAlt
        property color field: hmiTheme.input
        property color border: hmiTheme.lineStrong
        property color borderSoft: hmiTheme.line
        property color text: hmiTheme.text
        property color sub: hmiTheme.textSecondary
        property color muted: hmiTheme.muted
        property color accent: hmiTheme.accent
        property color warning: hmiTheme.warning
        property color danger: hmiTheme.danger
        property color info: hmiTheme.info
        property color accentText: "#061514"
        property color selection: hmiTheme.darkMode ? Qt.lighter(hmiTheme.cardAlt, 1.10) : "#E3F2EF"
        property color dangerFill: hmiTheme.darkMode ? "#7F1D1D" : "#F7DDE0"
    }

    function normalizedType(typeText) { return String(typeText).toLowerCase() === "wireguard" ? "WireGuard" : "VPN" }
    function stateLabel() {
        if (!vpnEnabled && runtimeState === "disconnected") return "DISABLED"
        if (runtimeState === "connecting") return "CONNECTING"
        if (runtimeState === "disconnecting") return "DISCONNECTING"
        if (runtimeState === "connected") return "CONNECTED"
        if (runtimeState === "failed") return "FAILED"
        return "DISCONNECTED"
    }
    function stateColor() {
        if (!vpnEnabled && runtimeState === "disconnected") return c.muted
        if (runtimeState === "connected") return c.accent
        if (runtimeState === "connecting" || runtimeState === "disconnecting") return c.info
        if (runtimeState === "failed") return c.danger
        return c.warning
    }
    function profileByUuid(uuid) {
        var wanted = String(uuid || "")
        for (var i = 0; i < vpnProfiles.length; ++i)
            if (String(vpnProfiles[i].uuid || "") === wanted) return vpnProfiles[i]
        return null
    }
    function selectProfile(profile) {
        if (!profile) return
        selectedUuid = String(profile.uuid || "")
        selectedName = String(profile.name || "Unnamed VPN")
        selectedType = String(profile.type || "vpn")
    }
    function ensureSelection() {
        var current = profileByUuid(selectedUuid)
        if (current) { selectProfile(current); return }
        var active = profileByUuid(activeUuid)
        if (active) { selectProfile(active); return }
        if (vpnProfiles.length > 0) { selectProfile(vpnProfiles[0]); return }
        selectedUuid = ""; selectedName = ""; selectedType = ""
    }
    function selectedProfileIsActive() {
        var p = profileByUuid(selectedUuid)
        return p ? p.active === true : false
    }
    function refreshVpn(showMessage) {
        if (statusRequestInFlight) return
        statusRequestInFlight = true
        if (showMessage === true) message = "Refreshing VPN runtime state..."
        NetworkController.requestVpnStatus()
    }
    function refreshPublicIp() {
        if (publicIpLoading) return
        publicIpLoading = true
        publicIpMessage = "Detecting public IP..."
        NetworkController.requestVpnPublicIp()
    }
    function refreshAll() { refreshVpn(true); refreshPublicIp() }
    function openVpnConfirmation(profile, targetActive) {
        if (!adminMode || busy || enableChangeInFlight || !profile) return
        if (targetActive && !vpnEnabled) { requestToast("Enable VPN before connecting a profile"); return }
        pendingUuid = String(profile.uuid || "")
        pendingName = String(profile.name || "Unnamed VPN")
        pendingTargetActive = targetActive
        if (pendingUuid.length > 0) vpnConfirmDialog.open()
    }
    function executePendingVpnAction() {
        if (pendingUuid.length === 0) return
        busy = true
        runtimeState = pendingTargetActive ? "connecting" : "disconnecting"
        message = pendingTargetActive ? "Connecting " + pendingName + "..." : "Disconnecting " + pendingName + "..."
        NetworkController.setVpnConnectionActive(pendingUuid, pendingTargetActive)
    }
    function connectSelectedProfile() { var p = profileByUuid(selectedUuid); if (p) openVpnConfirmation(p, true) }
    function disconnectActiveProfile() {
        var p = profileByUuid(activeUuid)
        if (!p && activeUuid.length > 0) p = {"uuid": activeUuid, "name": activeName.length > 0 ? activeName : "Active VPN"}
        if (p) openVpnConfirmation(p, false)
    }
    function executeEnableChange(targetEnabled) {
        if (!adminMode || busy || enableChangeInFlight) return
        busy = true; enableChangeInFlight = true
        message = targetEnabled ? "Enabling VPN..." : "Disabling VPN and disconnecting active tunnels..."
        NetworkController.setVpnEnabled(targetEnabled)
    }
    function requestEnableToggle() {
        if (!adminMode || busy || enableChangeInFlight) return
        if (vpnEnabled) disableVpnDialog.open(); else executeEnableChange(true)
    }

    // No startup timer/polling here. Setting.qml calls refreshAll() once on the
    // first VPN tab entry per Network Settings session. Later refresh is manual.

    Connections {
        target: NetworkController
        function onVpnStatusReady(status) {
            statusRequestInFlight = false
            vpnProfiles = status.profiles || []
            activeProfiles = status.activeProfiles || []
            vpnEnabled = status.enabled !== false
            if (status.ok === false) {
                busy = false; enableChangeInFlight = false; runtimeState = "failed"
                message = status.message || "Unable to read VPN state"; ensureSelection(); return
            }
            activeName = status.activeName || ""; activeUuid = status.activeUuid || ""; activeType = status.activeType || ""
            activeDevice = status.activeDevice || ""; activeIpv4 = status.activeIpv4 || ""
            runtimeState = status.active === true ? "connected" : "disconnected"
            busy = false; enableChangeInFlight = false; message = status.message || "VPN state synchronized"; ensureSelection()
        }
        function onVpnOperationFinished(action, ok, text) {
            message = text
            if (!ok) { busy = false; runtimeState = "failed"; requestToast(text) }
        }
        function onVpnEnableFinished(enabled, ok, text) {
            message = text
            if (!ok) { busy = false; enableChangeInFlight = false; requestToast(text); return }
            vpnEnabled = enabled
        }
        function onVpnPublicIpReady(ok, ip, text) {
            publicIpLoading = false; publicIpMessage = text || ""
            publicIp = ok && String(ip).length > 0 ? String(ip) : "Unavailable"
        }
    }

    Rectangle {
        id: summaryCard
        x: 28; y: 24; width: parent.width - 56; height: 246; radius: 16
        color: c.panel; border.color: c.border
        Text { x: 22; y: 15; text: "VPN Control"; color: c.text; font.pixelSize: 24; font.bold: true }
        Text { x: 22; y: 45; text: adminMode ? "ADMIN · runtime control enabled" : "VIEWER · status only"; color: adminMode ? c.accent : c.sub; font.pixelSize: 12; font.bold: true }

        Row {
            id: primaryInfoRow
            x: 22; y: 72; width: parent.width - 44; height: 62; spacing: 12
            Repeater {
                model: [
                    {title:"PUBLIC IP", value: publicIpLoading ? "Detecting..." : publicIp, kind:"public"},
                    {title:"STATUS", value: stateLabel(), kind:"status"},
                    {title:"VPN ENABLE", value: vpnEnabled ? "ENABLED" : "DISABLED", kind:"enable"},
                    {title:"SELECTED PROFILE", value: selectedName.length > 0 ? selectedName : "None selected", kind:"profile"}
                ]
                Rectangle {
                    width: (primaryInfoRow.width - 36) / 4; height: parent.height; radius: 10; color: c.field
                    border.color: modelData.kind === "profile" && selectedUuid.length > 0 ? c.info : modelData.kind === "enable" && vpnEnabled ? c.accent : c.borderSoft
                    Text { x: 12; y: 8; text: modelData.title; color: c.sub; font.pixelSize: 10; font.bold: true }
                    Text { x: 12; y: 27; width: parent.width - 24; text: modelData.value; color: modelData.kind === "status" ? stateColor() : modelData.kind === "enable" ? (vpnEnabled ? c.accent : c.muted) : c.text; font.pixelSize: 16; font.bold: true; elide: Text.ElideRight }
                }
            }
        }

        Row {
            x: 22
            y: 143
            spacing: 32

            Column {
                spacing: 2
                Text { text: "ACTIVE PROFILE"; color: c.sub; font.pixelSize: 10; font.bold: true }
                Text { text: activeName.length > 0 ? activeName : "—"; color: c.text; font.pixelSize: 13; width: 190; elide: Text.ElideRight }
            }

            Column {
                spacing: 2
                Text { text: "TYPE"; color: c.sub; font.pixelSize: 10; font.bold: true }
                Text { text: activeType.length > 0 ? normalizedType(activeType) : "—"; color: c.text; font.pixelSize: 13 }
            }

            Column {
                spacing: 2
                Text { text: "INTERFACE"; color: c.sub; font.pixelSize: 10; font.bold: true }
                Text { text: activeDevice.length > 0 ? activeDevice : "—"; color: c.text; font.pixelSize: 13 }
            }

            Column {
                spacing: 2
                Text { text: "TUNNEL IPv4"; color: c.sub; font.pixelSize: 10; font.bold: true }
                Text { text: activeIpv4.length > 0 ? activeIpv4 : "—"; color: c.text; font.pixelSize: 13 }
            }

            Column {
                spacing: 2
                Text { text: "ACTIVE TUNNELS"; color: c.sub; font.pixelSize: 10; font.bold: true }
                Text { text: String(activeProfiles.length); color: c.text; font.pixelSize: 13 }
            }
        }

        Row {
            id: controlRow
            x: 22
            y: 190
            spacing: 12

            Button {
                id: enableVpnButton
                width: 176
                height: 42
                text: vpnEnabled ? "Disable VPN" : "Enable VPN"
                enabled: adminMode && !busy && !enableChangeInFlight
                scale: pressed ? 0.96 : 1.0
                Behavior on scale { NumberAnimation { duration: 90 } }
                onClicked: requestEnableToggle()
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? (vpnEnabled ? c.text : c.accentText) : c.sub
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                }
                background: Rectangle {
                    radius: 10
                    color: !enableVpnButton.enabled ? c.field : vpnEnabled ? c.card : c.accent
                    border.color: !enableVpnButton.enabled ? c.border : vpnEnabled ? c.warning : c.accent
                }
            }

            Button {
                id: refreshButton
                width: 150
                height: 42
                text: (statusRequestInFlight || publicIpLoading) ? "Refreshing..." : "Refresh"
                enabled: !statusRequestInFlight && !publicIpLoading && !busy
                scale: pressed ? 0.96 : 1.0
                Behavior on scale { NumberAnimation { duration: 90 } }
                onClicked: refreshAll()
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? c.text : c.sub
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                }
                background: Rectangle {
                    radius: 10
                    color: refreshButton.pressed ? c.selection : c.field
                    border.color: c.border
                }
            }

            Button {
                id: connectButton
                width: 150
                height: 42
                text: "Connect"
                enabled: adminMode && vpnEnabled && selectedUuid.length > 0 &&
                         !selectedProfileIsActive() && !busy && !statusRequestInFlight
                scale: pressed ? 0.96 : 1.0
                Behavior on scale { NumberAnimation { duration: 90 } }
                onClicked: connectSelectedProfile()
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? c.accentText : c.sub
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                }
                background: Rectangle {
                    radius: 10
                    color: connectButton.enabled ? c.accent : c.field
                    border.color: connectButton.enabled ? c.accent : c.border
                }
            }

            Button {
                id: disconnectButton
                width: 150
                height: 42
                text: "Disconnect"
                enabled: adminMode && activeUuid.length > 0 && !busy && !statusRequestInFlight
                scale: pressed ? 0.96 : 1.0
                Behavior on scale { NumberAnimation { duration: 90 } }
                onClicked: disconnectActiveProfile()
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#ffffff" : c.sub
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                }
                background: Rectangle {
                    radius: 10
                    color: disconnectButton.enabled ? c.dangerFill : c.field
                    border.color: disconnectButton.enabled ? c.danger : c.border
                }
            }
        }
        Text { anchors.right: parent.right; anchors.rightMargin:22; y:202; width:parent.width-controlRow.x-controlRow.width-58; text:publicIpLoading?"Checking external address...":publicIpMessage; color:c.sub; font.pixelSize:11; horizontalAlignment:Text.AlignRight; elide:Text.ElideRight }
    }

    Text { x:30; y:292; text:"VPN Profiles"; color:c.text; font.pixelSize:22; font.bold:true }
    Text { x:30; y:323; text:"Select a NetworkManager VPN / WireGuard profile, then use the controls above"; color:c.sub; font.pixelSize:14 }

    Flickable {
        x:28; y:356; width:parent.width-56; height:parent.height-y-64; contentWidth:width; contentHeight:Math.max(height, profileColumn.height+8); clip:true
        Column {
            id: profileColumn; width:parent.width; spacing:12
            Repeater {
                model:vpnProfiles
                Rectangle {
                    width:profileColumn.width; height:94; radius:14; color:c.card
                    border.color:String(modelData.uuid||"")===root.selectedUuid?c.info:(modelData.active?c.accent:c.border)
                    border.width:String(modelData.uuid||"")===root.selectedUuid||modelData.active?2:1
                    MouseArea { anchors.fill:parent; onClicked:root.selectProfile(modelData) }
                    Rectangle { x:18; y:25; width:12; height:12; radius:6; color:modelData.active?c.accent:c.muted }
                    Text { x:44; y:13; width:parent.width-300; text:modelData.name||"Unnamed VPN"; color:c.text; font.pixelSize:18; font.bold:true; elide:Text.ElideRight }
                    Text { x:44; y:42; width:parent.width-300; text:normalizedType(modelData.type)+(modelData.device?" · "+modelData.device:"")+(modelData.ipv4?" · "+modelData.ipv4:""); color:c.sub; font.pixelSize:13; elide:Text.ElideRight }
                    Text { x:44; y:67; width:parent.width-300; text:(modelData.active?"CONNECTED":"AVAILABLE")+" · UUID "+String(modelData.uuid||""); color:modelData.active?c.accent:c.sub; font.pixelSize:10; font.bold:modelData.active; elide:Text.ElideMiddle }
                    Button {
                        id: selectButton
                        width: 150
                        height: 42
                        anchors.right: parent.right
                        anchors.rightMargin: 18
                        anchors.verticalCenter: parent.verticalCenter
                        text: String(modelData.uuid || "") === root.selectedUuid ? "Selected" : "Select"
                        enabled: !root.busy && !root.statusRequestInFlight
                        onClicked: root.selectProfile(modelData)
                        background: Rectangle {
                            radius: 10
                            color: c.field
                            border.color: String(modelData.uuid || "") === root.selectedUuid ? c.info : c.border
                        }
                        contentItem: Text {
                            text: parent.text
                            color: String(modelData.uuid || "") === root.selectedUuid ? c.info : c.text
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.bold: true
                        }
                    }
                }
            }
            Rectangle { visible:vpnProfiles.length===0; width:profileColumn.width; height:130; radius:14; color:c.card; border.color:c.border; Text { anchors.centerIn:parent; width:parent.width-60; text:statusRequestInFlight?"Searching for VPN profiles...":"No VPN profiles found. Add a VPN or WireGuard profile to NetworkManager, then press Refresh."; color:c.sub; font.pixelSize:15; horizontalAlignment:Text.AlignHCenter; wrapMode:Text.WordWrap } }
        }
    }

    Text { anchors.left:parent.left; anchors.leftMargin:30; anchors.bottom:parent.bottom; anchors.bottomMargin:18; text:message; color:runtimeState==="failed"?c.danger:c.sub; font.pixelSize:13; elide:Text.ElideRight; width:parent.width-60 }

    Dialog {
        id:vpnConfirmDialog; modal:true; focus:true; width:520; height:270; x:Math.round((root.width-width)/2); y:Math.round((root.height-height)/2); closePolicy:Popup.NoAutoClose
        background:Rectangle{radius:16;color:c.panel;border.color:c.border;border.width:1}
        contentItem:ColumnLayout{anchors.fill:parent;anchors.margins:24;spacing:14
            Text{Layout.fillWidth:true;text:pendingTargetActive?"Connect VPN?":"Disconnect VPN?";color:c.text;font.pixelSize:23;font.bold:true}
            Text{Layout.fillWidth:true;text:pendingTargetActive?"Connect to “"+pendingName+"”? Network routing and the public IP may change while the tunnel is active.":"Disconnect “"+pendingName+"”? Traffic currently using this tunnel may be interrupted.";color:c.sub;font.pixelSize:14;wrapMode:Text.WordWrap}
            Item{Layout.fillHeight:true}
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Item { Layout.fillWidth: true }
                Button {
                    text: "Cancel"
                    Layout.preferredWidth: 130
                    Layout.preferredHeight: 44
                    onClicked: vpnConfirmDialog.close()
                }
                Button {
                    id: confirmVpnButton
                    text: pendingTargetActive ? "Connect" : "Disconnect"
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 44
                    onClicked: {
                        vpnConfirmDialog.close()
                        executePendingVpnAction()
                    }
                }
            }
        }
    }

    Dialog {
        id:disableVpnDialog;modal:true;focus:true;width:540;height:280;x:Math.round((root.width-width)/2);y:Math.round((root.height-height)/2);closePolicy:Popup.NoAutoClose
        background:Rectangle{radius:16;color:c.panel;border.color:c.warning;border.width:1}
        contentItem:ColumnLayout{anchors.fill:parent;anchors.margins:24;spacing:14
            Text{Layout.fillWidth:true;text:"Disable VPN?";color:c.text;font.pixelSize:23;font.bold:true}
            Text{Layout.fillWidth:true;text:activeProfiles.length>0?"VPN control will be disabled and all active VPN/WireGuard tunnels will be disconnected.":"VPN control will be disabled. Existing profiles remain saved in NetworkManager.";color:c.sub;font.pixelSize:14;wrapMode:Text.WordWrap}
            Item{Layout.fillHeight:true}
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Item { Layout.fillWidth: true }
                Button {
                    text: "Cancel"
                    Layout.preferredWidth: 130
                    Layout.preferredHeight: 44
                    onClicked: disableVpnDialog.close()
                }
                Button {
                    text: "Disable VPN"
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 44
                    onClicked: {
                        disableVpnDialog.close()
                        executeEnableChange(false)
                    }
                }
            }
        }
    }
}
